#include "server.h"
#include "tokens.h"
#include "common.h"
#include "debugCodes.h"
#include "zmqContext.h"

#include <pxr/base/tf/stringUtils.h>

PXR_NAMESPACE_OPEN_SCOPE

const char* const kInAppCommunicationSockAddr = "inproc://RprIpcServer";

//------------------------------------------------------------------------------
// Construction
//------------------------------------------------------------------------------

RprIpcServer::RprIpcServer(Listener* listener)
    : m_listener(listener) {
    auto& zmqContext = GetZmqContext();

    try {
        m_controlSocket = zmq::socket_t(zmqContext, zmq::socket_type::rep);
        m_controlSocket.bind("tcp://127.0.0.1:*");

        m_appSocket = zmq::socket_t(zmqContext, zmq::socket_type::pull);
        m_appSocket.bind(kInAppCommunicationSockAddr);
    } catch (zmq::error_t& e) {
        throw std::runtime_error(TfStringPrintf("Failed to setup RprIpcServer sockets: %d", e.num()));
    }

    // It's an open question how to run or connect to viewer:
    // whether we expect viewer process to exists,
    // or we will have some daemon that will control viewer process
    // or RprIpcServer must create the process
    //
    // For now, we expect the user to start viewer process manually
    {
        auto controlSocketAddr = GetSocketAddress(m_controlSocket);
        printf("Waiting for connection on socket %s", controlSocketAddr.c_str());
    }

    m_networkThread = std::thread([this]() { RunNetworkWorker(); });
}

RprIpcServer::~RprIpcServer() {
    zmq::socket_t shutdownSocket(GetZmqContext(), zmq::socket_type::push);
    shutdownSocket.connect(kInAppCommunicationSockAddr);
    shutdownSocket.send(GetZmqMessage(RprIpcTokens->shutdown));
    m_networkThread.join();
}

//------------------------------------------------------------------------------
// Layers management
//------------------------------------------------------------------------------

RprIpcServer::Layer* RprIpcServer::AddLayer(SdfPath const& layerPath) {
    std::lock_guard<std::mutex> lock(m_layersMutex);
    if (m_layers.count(layerPath)) {
        TF_CODING_ERROR("Duplicate layer with layerPath - %s", layerPath.GetText());
        return nullptr;
    }
    struct Dummy : public Layer {};
    auto st = m_layers.emplace(layerPath, std::make_unique<Dummy>());
    return st.first->second.get();
}

void RprIpcServer::OnLayerEdit(SdfPath const& layerPath, Layer* layer) {
    layer->OnEdit();
    GetSender(&layer->m_cachedSender);
    if (layer->m_cachedSender) {
        layer->m_cachedSender->SendLayer(layerPath, layer->GetEncodedStage());
    }
}

void RprIpcServer::RemoveLayer(SdfPath const& layerPath) {
    std::lock_guard<std::mutex> lock(m_layersMutex);
    auto it = m_layers.find(layerPath);
    if (it == m_layers.end()) {
        TF_CODING_ERROR("Removing inexisting layer with layerPath - %s", layerPath.GetText());
        return;
    }

    auto& sender = it->second->m_cachedSender;
    GetSender(&sender);
    if (sender) {
        sender->RemoveLayer(layerPath);
    }

    m_layers.erase(it);
}

RprIpcServer::Layer::Layer() : m_stage(UsdStage::CreateInMemory()) {
    OnEdit();
}

std::string const& RprIpcServer::Layer::GetEncodedStage() {
    if (m_stage && m_isEncodedStageDirty) {
        m_isEncodedStageDirty = false;

        if (!m_stage->ExportToString(&m_encodedStage)) {
            TF_RUNTIME_ERROR("Failed to export stage");
        }
    }

    return m_encodedStage;
}

void RprIpcServer::Layer::OnEdit() {
    m_timestamp = std::chrono::steady_clock::now().time_since_epoch().count() / 1000;
    m_isEncodedStageDirty = true;
}

//------------------------------------------------------------------------------
// Sender
//------------------------------------------------------------------------------

void RprIpcServer::GetSender(std::shared_ptr<Sender>* senderPtr) {
    // the same zmq::socket_t can be safely reused from the same thread
    // so we cache Sender per thread to minimize the number of created sockets

    auto thisThreadId = std::this_thread::get_id();

    // Check if cached sender can be reused
    if (*senderPtr) {
        auto cachedSender = senderPtr->get();
        if (cachedSender->m_owningThreadId == thisThreadId) {
            return;
        }
    }

    // Check if we need sender for the network thread where we can send directly through dataSocket
    if (m_networkThread.get_id() == thisThreadId) {
        if (m_dataSocket) {
            senderPtr->reset(new Sender(thisThreadId, &m_dataSocket));
        } else {
            // There is no active connection
            senderPtr->reset();
        }
        return;
    }

    static std::mutex s_sendersMutex;

    // Check if we have valid cached sender for this thread
    {
        std::lock_guard<std::mutex> lock(s_sendersMutex);
        auto it = m_senders.find(thisThreadId);
        if (it != m_senders.end()) {
            if (auto sender = it->second.lock()) {
                *senderPtr = sender;
                return;
            } else {
                m_senders.erase(it);
            }
        }
    }

    // Create new sender for this thread
    zmq::socket_t pushSocket;
    try {
        pushSocket = zmq::socket_t(GetZmqContext(), zmq::socket_type::push);
        pushSocket.connect(kInAppCommunicationSockAddr);
    } catch (zmq::error_t& e) {
        TF_RUNTIME_ERROR("Failed to create sender socket: %d", e.num());
        senderPtr->reset();
        return;
    }

    auto newSender = std::shared_ptr<Sender>(new Sender(thisThreadId, std::move(pushSocket)));

    {
        std::lock_guard<std::mutex> lock(s_sendersMutex);
        m_senders.emplace(thisThreadId, newSender);
    }

    std::swap(*senderPtr, newSender);
}

RprIpcServer::Sender::Sender(std::thread::id owningThread, zmq::socket_t&& socket)
    : m_owningThreadId(owningThread)
    , m_retainedSocket(std::move(socket))
    , m_pushSocket(&m_retainedSocket) {

}

RprIpcServer::Sender::Sender(std::thread::id owningThread, zmq::socket_t* socket)
    : m_owningThreadId(owningThread)
    , m_pushSocket(socket) {

}

void RprIpcServer::Sender::SendLayer(SdfPath const& layerPath, std::string layer) {
    if (!m_pushSocket) return;

    try {
        m_pushSocket->send(GetZmqMessage(RprIpcTokens->layer), zmq::send_flags::sndmore);
        m_pushSocket->send(GetZmqMessage(layerPath.GetString()), zmq::send_flags::sndmore);
        m_pushSocket->send(zmq::message_t(layer.c_str(), layer.size()));
    } catch (zmq::error_t& e) {
        TF_RUNTIME_ERROR("Error on layer send: %d", e.num());
    }
}

void RprIpcServer::Sender::RemoveLayer(SdfPath const& layerPath) {
    if (!m_pushSocket) return;

    try {
        m_pushSocket->send(GetZmqMessage(RprIpcTokens->layerRemove), zmq::send_flags::sndmore);
        m_pushSocket->send(GetZmqMessage(layerPath.GetString()));
    } catch (zmq::error_t& e) {
        TF_RUNTIME_ERROR("Error on layer remove: %d", e.num());
    }
}

//------------------------------------------------------------------------------
// Private
//------------------------------------------------------------------------------

void RprIpcServer::RunNetworkWorker() {
    std::vector<zmq::pollitem_t> pollItems = {
        {m_controlSocket, 0, ZMQ_POLLIN, 0},
        {m_appSocket, 0, ZMQ_POLLIN, 0},
    };

    while (m_controlSocket) {
        try {
            zmq::poll(pollItems);

            if (pollItems[0].revents & ZMQ_POLLIN) {
                ProcessControlSocket();
            }

            if (pollItems[1].revents & ZMQ_POLLIN) {
                ProcessAppSocket();
            }
        } catch (zmq::error_t& e) {
            TF_RUNTIME_ERROR("Network error: %d", e.num());
        }
    }
}

void RprIpcServer::ProcessControlSocket() {
    zmq::message_t msg;
    m_controlSocket.recv(msg);

    auto command = std::to_string(msg);
    TF_DEBUG(RPR_IPC_DEBUG_MESSAGES).Msg("RprIpcServer: received \"%s\" command\n", command.c_str());

    // Process any RprIpcServer specific commands first
    if (RprIpcTokens->connect == command) {
        if (msg.more()) {
            m_controlSocket.recv(msg);

            auto port = std::to_string(msg);
            auto dataSocketAddr = TfStringPrintf("tcp://%s:%s", msg.gets("Peer-Address"), port.c_str());
            try {
                m_dataSocket = zmq::socket_t(GetZmqContext(), zmq::socket_type::push);
                m_dataSocket.connect(dataSocketAddr);

                m_controlSocket.send(GetZmqMessage(RprIpcTokens->ok));
                TF_DEBUG(RPR_IPC_DEBUG_MESSAGES).Msg("RprIpcServer: connected dataSocket to %s\n", dataSocketAddr.c_str());

                SendAllLayers();
                return;
            } catch (zmq::error_t& e) {
                TF_RUNTIME_ERROR("Failed to setup notify socket with address %s. Zmq error code: %d", dataSocketAddr.c_str(), e.num());
            }
        }

        m_controlSocket.send(GetZmqMessage(RprIpcTokens->fail));
    } else if (RprIpcTokens->disconnect == command) {
        m_dataSocket.close();
        m_controlSocket.send(GetZmqMessage(RprIpcTokens->ok));
    } else if (RprIpcTokens->shutdown == command) {
        m_controlSocket.close();
    } else if (RprIpcTokens->ping == command) {
        m_controlSocket.send(GetZmqMessage(RprIpcTokens->ping));
    } else {
        // notify command listener about any other commands

        uint8_t* payload = nullptr;
        size_t payloadSize = 0;

        if (msg.more()) {
            m_controlSocket.recv(msg);
            payload = msg.data<uint8_t>();
            payloadSize = msg.size();
        }

        auto response = m_listener->ProcessCommand(command, payload, payloadSize);
        TF_DEBUG(RPR_IPC_DEBUG_MESSAGES).Msg("RprIpcServer: response for \"%s\" command: %d\n", command.c_str(), response);
        m_controlSocket.send(GetZmqMessage(response ? RprIpcTokens->ok : RprIpcTokens->fail));
    }
}

void RprIpcServer::ProcessAppSocket() {
    // Proxy all messages from the application to the client

    zmq::message_t msg;
    m_appSocket.recv(msg);

    {
        // Sniff the packet for `shutdown` command
        if (RprIpcTokens->shutdown.size() == msg.size() &&
            std::strncmp(RprIpcTokens->shutdown.GetText(), msg.data<char>(), msg.size()) == 0) {
            m_controlSocket.close();
            return;
        }
    }

    ProxyMessages(msg, m_appSocket, m_dataSocket);
}

void RprIpcServer::SendAllLayers() {
    std::lock_guard<std::mutex> lock(m_layersMutex);
    if (!m_layers.empty()) {
        TF_DEBUG(RPR_IPC_DEBUG_MESSAGES).Msg("RprIpcServer: sending %zu layers\n", m_layers.size());

        std::shared_ptr<Sender> sender;
        GetSender(&sender);

        if (sender) {
            for (auto& entry : m_layers) {
                sender->SendLayer(entry.first, entry.second->GetEncodedStage());
            }
        } else {
            TF_RUNTIME_ERROR("Failed to get sender to send all layers");
        }
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
