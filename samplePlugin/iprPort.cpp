#include "iprPort.h"

#include <pxr/base/tf/stringUtils.h>
#include <pxr/base/tf/staticTokens.h>
#include <pxr/base/tf/instantiateSingleton.h>

PXR_NAMESPACE_OPEN_SCOPE

TF_INSTANTIATE_SINGLETON(zmq::context_t);

zmq::context_t& GetZmqContext() {
    return TfSingleton<zmq::context_t>::GetInstance();
}

TF_DEFINE_PRIVATE_TOKENS(_tokens,
    (initNotifySocket)
    (ping)
    (pong)
    (layer)
    (layerEdit)
    (disconnect)
    (ok)
    (fail)
);

namespace {

template <typename U>
struct is_buffer_like {
private:
    template<typename T>
    static constexpr auto check(T*) -> typename
        std::is_pointer<decltype(std::declval<T>().data())>::type;

    template<typename>
    static constexpr std::false_type check(...);

    typedef decltype(check<U>(0)) type;

public:
    static constexpr bool value = type::value;
};
template< class T >
constexpr bool is_buffer_like_v = is_buffer_like<T>::value;

template <typename T>
std::enable_if_t<is_buffer_like_v<T>, zmq::message_t>
GetZmqMessage(T const& command) {
    return {command.data(), command.size()};
}

template <typename T>
std::enable_if_t<std::is_integral_v<T>, zmq::message_t>
GetZmqMessage(T integer) {
    return GetZmqMessage(std::to_string(integer));
}

std::string GetSocketAddress(zmq::socket_t const& socket) {
    char addrBuffer[1024];
    size_t buffLen = sizeof(addrBuffer);
    socket.getsockopt(ZMQ_LAST_ENDPOINT, addrBuffer, &buffLen);
    return std::string(addrBuffer, buffLen - 1);
}

bool SocketReadyForSend(zmq::socket_t& socket) {
    zmq::pollitem_t pollItem{static_cast<void*>(socket), 0, ZMQ_POLLOUT, 0};
    return zmq::poll(&pollItem, 1, 0) == 1;
}

} // namespace anonymous

IPRPort::IPRPort(CommandListener* commandListener)
    : m_commandListener(commandListener) {
    auto& zmqContext = GetZmqContext();

    try {
        m_controlSocket = zmq::socket_t(zmqContext, zmq::socket_type::rep);
        m_controlSocket.bind("tcp://127.0.0.1:*");
    } catch (zmq::error_t& e) {
        throw std::runtime_error(TfStringPrintf("Failed to setup IPRPort control sockets: %d", e.num()));
    }

    // run viewer process
    m_viewerProcess = bp::child(
        bp::search_path("python"), "D:\\dev\\usd-ipc\\viewer\\viewer.py",
        "--control", GetSocketAddress(m_controlSocket));
    if (!m_viewerProcess.running()) {
        throw std::runtime_error("Failed to run viewer process");
    }

    zmq::message_t message;
    m_controlSocket.recv(message);

    std::string command(message.data<char>(), message.size());
    if (_tokens->initNotifySocket == command &&
        message.more()) {
        m_controlSocket.recv(message);
        std::string addr(message.data<char>(), message.size());

        try {
            m_notifySocket = zmq::socket_t(zmqContext, zmq::socket_type::push);
            m_notifySocket.connect(addr);
            printf("Plugin: connected notifySocket to %s\n", addr.c_str());
            m_controlSocket.send(GetZmqMessage(_tokens->ok));
        } catch (zmq::error_t& e) {
            throw std::runtime_error(TfStringPrintf("Invalid notify socket address: %s", addr.c_str()));
        }
    } else {
        throw std::runtime_error("Unexpected hello message");
    }
}

IPRPort::~IPRPort() {
    if (m_viewerProcess.running()) {
        std::error_code ec;
        m_viewerProcess.terminate(ec);
    }
}

void IPRPort::SendLayer(std::string const& layerPath, uint64_t timestamp, std::string layer) {
    auto it = m_enqueuedLayers.find(layerPath);
    if (it != m_enqueuedLayers.end()) {
        if (it->second.timestamp < timestamp) {
            // drop outdated layer
            m_enqueuedLayers.erase(it);
        } else {
            // We will never try to send layer from the past
            assert(it->second.timestamp == timestamp);

            // We can try sending the same layer when the enqueued layer did
            // not have time yet to be sent and IPR requested layer again
            return;
        }
    }

    if (SocketReadyForSend(m_notifySocket)) {
        SendLayerImpl(layerPath, timestamp, layer);
    } else {
        m_enqueuedLayers.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(layerPath),
            std::forward_as_tuple(timestamp, std::move(layer)));
    }
}

void IPRPort::NotifyLayerEdit(std::string const& layerPath, uint64_t timestamp) {
    auto it = m_enqueuedLayerEdits.find(layerPath);
    if (it != m_enqueuedLayerEdits.end()) {
        if (it->second < timestamp) {
            m_enqueuedLayerEdits.erase(it);
        } else {
            assert(it->second == timestamp);
            return;
        }
    }

    if (SocketReadyForSend(m_notifySocket)) {
        SendLayerEditImpl(layerPath, timestamp);
    } else {
        m_enqueuedLayerEdits.emplace(layerPath, timestamp);
    }
}

void IPRPort::NotifyLayerRemove(std::string const& layerPath) {
    m_enqueuedLayerEdits.erase(layerPath);
    m_enqueuedLayers.erase(layerPath);
    NotifyLayerEdit(layerPath, 0);
}

void IPRPort::Update() {
    std::vector<zmq::pollitem_t> pollItems = {
        {static_cast<void*>(m_controlSocket), 0, ZMQ_POLLIN, 0}
    };

    if (!m_enqueuedLayerEdits.empty() || !m_enqueuedLayers.empty()) {
        pollItems.push_back({static_cast<void*>(m_notifySocket), 0, ZMQ_POLLOUT, 0});
    }

    while (ConnectionIsOk() && zmq::poll(pollItems, 0)) {
        if (pollItems[0].revents & ZMQ_POLLIN) {
            ProcessRequest();
        }

        if (pollItems.size() > 1) {
            if (pollItems[1].revents & ZMQ_POLLOUT) {
                if (!m_enqueuedLayers.empty()) {
                    SendEnqueuedLayer();
                } else if (!m_enqueuedLayerEdits.empty()) {
                    SendEnqueuedLayerEdit();
                } else {
                    assert(false);
                }

                if (m_enqueuedLayerEdits.empty() && m_enqueuedLayers.empty()) {
                    pollItems.pop_back();
                }
            }
        }
    }
}

void IPRPort::ProcessRequest() {
    zmq::message_t message;
    printf("Plugin: receiving control command\n");
    m_controlSocket.recv(message);

    std::string command(message.data<char>(), message.size());
    printf("Plugin: received %s command\n", command.c_str());

    // Process any IPRPort specific commands first
    if (_tokens->disconnect == command) {
        // close connection, etc
        m_controlSocket.close();
    } else if (_tokens->ping == command) {
        m_controlSocket.send(GetZmqMessage(_tokens->pong));
        printf("Plugin: sent pong reply\n", command.c_str());
    } else {
        // notify command listener about any other commands
        auto response = m_commandListener->ProcessCommand(command);
        m_controlSocket.send(GetZmqMessage(response));
    }
}

void IPRPort::SendEnqueuedLayerEdit() {
    if (m_enqueuedLayerEdits.empty()) {
        TF_CODING_ERROR("Attempt to send layer edit when no pending layer edits exist");
        return;
    }

    auto it = m_enqueuedLayerEdits.begin();
    if (SendLayerEditImpl(it->first, it->second)) {
        m_enqueuedLayerEdits.erase(it);
    }
}

void IPRPort::SendEnqueuedLayer() {
    if (m_enqueuedLayers.empty()) {
        TF_CODING_ERROR("Attempt to send layer when no pending layers exist");
        return;
    }

    auto it = m_enqueuedLayers.begin();

    auto layerEditIt = m_enqueuedLayerEdits.find(it->first);
    if (layerEditIt != m_enqueuedLayerEdits.end()) {
        auto layerEditTimestamp = layerEditIt->second;
        if (layerEditTimestamp > it->second.timestamp) {
            // When layerEdit's timestamp is more recent than layer's timestamp, layer is outdated
            // We can somehow get the current one and send it right here or
            // we can let IPR decide if he needs new layer
            m_enqueuedLayers.erase(it);
            return;
        } else {
            // Drop notification as outdated
            m_enqueuedLayerEdits.erase(layerEditIt);
        }
    }

    if (SendLayerImpl(it->first, it->second.timestamp, it->second.encodedString)) {
        m_enqueuedLayers.erase(it);
    }
}

bool IPRPort::SendLayerEditImpl(std::string const& layerPath, uint64_t timestamp) {
    try {
        m_notifySocket.send(GetZmqMessage(_tokens->layerEdit), zmq::send_flags::sndmore);
        m_notifySocket.send(GetZmqMessage(layerPath), zmq::send_flags::sndmore);
        m_notifySocket.send(GetZmqMessage(timestamp));
        return true;
    } catch (zmq::error_t& e) {
        TF_RUNTIME_ERROR("Error on send: %d", e.num());
        return false;
    }
}

bool IPRPort::SendLayerImpl(std::string const& layerPath, uint64_t timestamp, std::string const& layer) {
    try {
        m_notifySocket.send(GetZmqMessage(_tokens->layer), zmq::send_flags::sndmore);
        m_notifySocket.send(GetZmqMessage(layerPath), zmq::send_flags::sndmore);
        m_notifySocket.send(GetZmqMessage(timestamp), zmq::send_flags::sndmore);
        m_notifySocket.send(zmq::message_t(layer.c_str(), layer.size()));
        return true;
    } catch (zmq::error_t& e) {
        TF_RUNTIME_ERROR("Error on send: %d", e.num());
        return false;
    }
}

bool IPRPort::ConnectionIsOk() {
    // Add some checks: last activity, ping, etc
    // In general, explicitly track viewer presence
    return true;
}

PXR_NAMESPACE_CLOSE_SCOPE
