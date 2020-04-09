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
    (getStage)
    (ping)
    (pong)
    (ok)
    (fail)
);

namespace {

template <typename T>
zmq::message_t GetZmqMessage(T const& command) {
    return {command.data(), command.size()};
}

std::string GetSocketAddress(zmq::socket_t const& socket) {
    char addrBuffer[1024];
    size_t buffLen = sizeof(addrBuffer);
    socket.getsockopt(ZMQ_LAST_ENDPOINT, addrBuffer, &buffLen);
    return std::string(addrBuffer, buffLen - 1);
}

zmq::message_t TryRequest(zmq::socket_t& socket, std::function<void (zmq::socket_t&)> const& messageComposer) {
    static const int numRetries = 3;
    static const auto requestTimeout = std::chrono::milliseconds(2500);

    for (int i = 0; i < numRetries; ++i) {
        messageComposer(socket);

        zmq::pollitem_t pollItem = {static_cast<void*>(socket), 0, ZMQ_POLLIN, 0};
        zmq::poll(&pollItem, 1, requestTimeout);

        if (pollItem.revents & ZMQ_POLLIN) {
            zmq::message_t reply;
            socket.recv(reply);
            return reply;
        } else {
            printf("Plugin: no response from viewer, retrying...\n");
            auto addr = GetSocketAddress(socket);
            socket.setsockopt(ZMQ_LINGER, 0);
            socket.close();

            if (i + 1 != numRetries) {
                socket = zmq::socket_t(GetZmqContext(), zmq::socket_type::req);
                socket.connect(addr);
            } else {
                printf("Plugin: viewer is offline\n");
            }
        }
    }

    return {};
}

} // namespace anonymous

IPRPort::IPRPort(DataSource* dataSource)
    : m_dataSource(dataSource) {
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
            m_notifySocket = zmq::socket_t(zmqContext, zmq::socket_type::req);
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

void IPRPort::Update() {
    zmq::pollitem_t controlPollItem = {static_cast<void*>(m_controlSocket), 0, ZMQ_POLLIN, 0};
    zmq::poll(&controlPollItem, 1, 0);
    if (controlPollItem.revents & ZMQ_POLLIN) {
        ProcessRequest();
    }

    if (m_stageDirty) {
        SendStage();
    }
}

void IPRPort::ProcessRequest() {
    zmq::message_t message;
    printf("Plugin: receiving control command\n");
    m_controlSocket.recv(message);

    std::string command(message.data<char>(), message.size());
    printf("Plugin: received %s command\n", command.c_str());

    if (_tokens->getStage == command) {
        m_controlSocket.send(GetZmqMessage(_tokens->ok));
        SendStage();
    } else if (_tokens->ping == command) {
        m_controlSocket.send(GetZmqMessage(_tokens->pong));
        printf("Plugin: sent pong reply\n", command.c_str());
    } else {
        printf("Plugin: unknown command received: %s\n");
        m_controlSocket.send(GetZmqMessage(_tokens->fail));
    }
}

bool IPRPort::SendStage() {
    auto encodedStage = m_dataSource->GetEncodedStage();
    auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count() / 1000000;
    auto timestampString = std::to_string(timestamp);
    auto reply = TryRequest(m_notifySocket,
        [&encodedStage, &timestampString](zmq::socket_t& socket) {
        socket.send(GetZmqMessage(_tokens->getStage), zmq::send_flags::sndmore);
        socket.send(GetZmqMessage(timestampString), zmq::send_flags::sndmore);
        socket.send(zmq::message_t(encodedStage.c_str(), encodedStage.size()));
    });
    m_stageDirty = false;
    if (reply.empty()) printf("Plugin: connection lost: timeout. Crash incoming");
    return !reply.empty();
}

void IPRPort::StageChanged() {
    m_stageDirty = true;
}

PXR_NAMESPACE_CLOSE_SCOPE
