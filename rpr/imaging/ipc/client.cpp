#include "client.h"
#include "common.h"
#include "tokens.h"
#include "debugCodes.h"
#include "zmqContext.h"

#include <pxr/usd/sdf/changeBlock.h>
#include <pxr/base/arch/fileSystem.h>
#include <pxr/base/tf/diagnostic.h>
#include <pxr/base/tf/stringUtils.h>
#include <pxr/base/tf/pathUtils.h>
#include <pxr/base/tf/fileUtils.h>

#ifdef WIN32
#include <Windows.h>
#include <fileapi.h>
#endif // WIN32
#include <sys/stat.h>

#include <fstream>

PXR_NAMESPACE_OPEN_SCOPE

const char* const kInAppCommunicationSockAddr = "inproc://RprIpcClient";

//------------------------------------------------------------------------------
// Construction
//------------------------------------------------------------------------------

RprIpcClientRefPtr RprIpcClient::Create(
    std::string const& serverAddress,
    std::function<void()> onStageUpdateCallback) {
    return TfCreateRefPtr(new RprIpcClient(serverAddress, onStageUpdateCallback));
}

RprIpcClient::RprIpcClient(
    std::string const& serverAddress,
    std::function<void()> onStageUpdateCallback)
    : m_layerController(std::make_unique<LayerController>())
    , m_serverAddress(serverAddress)
    , m_dataSocket(GetZmqContext(), zmq::socket_type::pull)
    , m_onStageUpdate(std::move(onStageUpdateCallback)) {
    m_dataSocket.bind("tcp://0.0.0.0:*");

    auto dataSocketPort = GetSocketPort(m_dataSocket);
    auto connectReply = TryRequest([&dataSocketPort](zmq::socket_t& socket) {
        socket.send(GetZmqMessage(RprIpcTokens->connect), zmq::send_flags::sndmore);
        socket.send(GetZmqMessage(dataSocketPort));

        TF_DEBUG(RPR_IPC_DEBUG_MESSAGES).Msg("RprIpcClient: sending connect command: port=%s\n", dataSocketPort.c_str());
    }, 1000);

    if (connectReply != "ok") {
        TF_RUNTIME_ERROR("Failed to connect to %s", m_serverAddress.c_str());
    }

    m_appSocket = zmq::socket_t(GetZmqContext(), zmq::socket_type::pair);
    m_appSocket.bind(kInAppCommunicationSockAddr);

    m_networkThread = std::thread([this]() { RunNetworkWorker(); });
}

RprIpcClient::~RprIpcClient() {
    if (m_appSocket) {
        m_appSocket.send(GetZmqMessage(RprIpcTokens->shutdown));
        m_networkThread.join();
    }
}

//------------------------------------------------------------------------------
// Server control
//------------------------------------------------------------------------------

bool RprIpcClient::SendCommand(
    std::string const& command,
    std::string const& payload) {
    if (!m_appSocket) return false;

    try {
        m_appSocket.send(GetZmqMessage(command), !payload.empty() ? zmq::send_flags::sndmore : zmq::send_flags::none);
        if (!payload.empty()) {
            m_appSocket.send(GetZmqMessage(payload));
        }

        return true;
    } catch (zmq::error_t& e) {
        TF_RUNTIME_ERROR("Failed to send command=%s with payloadSize=%zu: errorCode=%d", command.c_str(), payload.size(), e.num());
        return false;
    }
}

//------------------------------------------------------------------------------
// Stage
//------------------------------------------------------------------------------

UsdStagePtr RprIpcClient::GetStage() {
    return m_layerController->GetStage();
}

//------------------------------------------------------------------------------
// Private
//------------------------------------------------------------------------------

void RprIpcClient::RunNetworkWorker() {
    auto threadSocket = zmq::socket_t(GetZmqContext(), zmq::socket_type::pair);
    threadSocket.connect(kInAppCommunicationSockAddr);

    std::vector<zmq::pollitem_t> pollItems = {
        {m_dataSocket, 0, ZMQ_POLLIN, 0},
        {threadSocket, 0, ZMQ_POLLIN, 0},
    };

    while (true) {
        try {
            zmq::poll(pollItems);

            if (pollItems[0].revents & ZMQ_POLLIN) {
                ProcessDataSocket();
            }

            if (pollItems[1].revents & ZMQ_POLLIN) {
                zmq::message_t msg;
                threadSocket.recv(msg);

                auto command = std::to_string(msg);

                if (RprIpcTokens->shutdown == command) {
                    break;
                } else {
                    ProxyMessages(msg, threadSocket, m_controlSocket);

                    m_controlSocket.recv(msg);
                    auto response = std::to_string(msg);

                    TF_DEBUG(RPR_IPC_DEBUG_MESSAGES).Msg("RprIpcClient: server responded on the \"%s\" command: %s\n", command.c_str(), response.c_str());
                }
            }
        } catch (zmq::error_t& e) {
            TF_RUNTIME_ERROR("Network error: %d", e.num());
        }
    }
}

std::string RprIpcClient::TryRequest(
    MessageComposer messageComposer,
    long timeout, int numRetries) {
    if (!m_controlSocket) {
        SetupControlSocket();
    }

    for (int i = 0; i < numRetries; ++i) {
        messageComposer(m_controlSocket);

        zmq::pollitem_t pollItem = {m_controlSocket, 0, ZMQ_POLLIN, 0};
        if (zmq::poll(&pollItem, 1, timeout) == 1 &&
            (pollItem.revents & ZMQ_POLLIN) != 0) {

            zmq::message_t msg;
            m_controlSocket.recv(msg);
            return std::to_string(msg);
        } else {
            TF_WARN("No response from the server, retrying...");

            SetupControlSocket();
        }
    }

    return {};
}

void RprIpcClient::SetupControlSocket() {
    m_controlSocket = zmq::socket_t(GetZmqContext(), zmq::socket_type::req);
    m_controlSocket.set(zmq::sockopt::linger, 0);
    m_controlSocket.connect(m_serverAddress);
}

void RprIpcClient::ProcessDataSocket() {
    zmq::message_t msg;
    while (true) {
        if (!m_dataSocket.recv(msg, zmq::recv_flags::dontwait)) {
            break;
        }

        auto command = std::to_string(msg);

        try {
            if (TfStringStartsWith(command, RprIpcTokens->layer)) {
                RecvMore(m_dataSocket, msg);
                auto layerPath = std::to_string(msg);

                TF_DEBUG(RPR_IPC_DEBUG_MESSAGES).Msg("RprIpcClient: received \"%s\" command: %s\n", command.c_str(), layerPath.c_str());

                if (command.size() == RprIpcTokens->layer.size()) {
                    RecvMore(m_dataSocket, msg);
                    m_layerController->AddLayer(layerPath, msg.data<char>(), msg.size());
                } else /*if (RprIpcTokens->layerRemove == command)*/ {
                    m_layerController->RemoveLayer(layerPath);
                }
            } else {
                TF_RUNTIME_ERROR("Unknown command");
            }

            if (msg.more()) {
                TF_RUNTIME_ERROR("Unexpected number of messages in \"%s\" command", command.c_str());
            }
        } catch (std::exception& e) {
            TF_RUNTIME_ERROR("Failed to process \"%s\" command: %s", command.c_str(), e.what());
        }

        DropRemainingMessages(m_dataSocket, msg);
    }

    if (m_layerController->Update() && m_onStageUpdate) {
        m_onStageUpdate();
    }
}

//------------------------------------------------------------------------------
// LayerController
//------------------------------------------------------------------------------

RprIpcClient::LayerController::LayerController()
    : m_rootStage(UsdStage::CreateNew(GetLayerSavePath("/root"))) {

}

RprIpcClient::LayerController::~LayerController() {

}

void RprIpcClient::LayerController::AddLayer(
    std::string const& layerPath,
    char* encodedLayer,
    size_t encodedLayerSize) {
    auto it = m_layers.find(layerPath);
    if (it == m_layers.end()) {
        m_updates[layerPath] = LayerUpdateType::Added;
        m_layers.insert(layerPath);
        TF_DEBUG(RPR_IPC_DEBUG_MESSAGES).Msg("RprIpcClient: new layer: %s\n", layerPath.c_str());
    } else {
        m_updates[layerPath] = LayerUpdateType::Edited;
        TF_DEBUG(RPR_IPC_DEBUG_MESSAGES).Msg("RprIpcClient: layer edited: %s\n", layerPath.c_str());
    }

    auto layerSavePath = GetLayerSavePath(layerPath.c_str());
    if (!TfMakeDirs(TfGetPathName(layerSavePath), -1, true)) {
        TF_RUNTIME_ERROR("Cannot create directory for \"%s\" layer", layerSavePath.c_str());
        return;
    }

    std::ofstream layerFile(layerSavePath);
    if (layerFile.fail()) {
        TF_RUNTIME_ERROR("Cannot create \"%s\" layer file", layerSavePath.c_str());
        return;
    }

    layerFile.write(encodedLayer, encodedLayerSize);

    if (layerFile.fail()) {
        TF_RUNTIME_ERROR("Failed to write \"%s\" layer file", layerSavePath.c_str());
    }
}

void RprIpcClient::LayerController::RemoveLayer(
    std::string const& layerPath) {
    auto it = m_layers.find(layerPath);
    if (it == m_layers.end()) {
        TF_RUNTIME_ERROR("Failed to remove \"%s\" layer: does not exist", layerPath.c_str());
    } else {
        TF_DEBUG(RPR_IPC_DEBUG_MESSAGES).Msg("RprIpcClient: removing layer: %s\n", layerPath.c_str());
        m_updates[layerPath] = LayerUpdateType::Removed;

        auto layerSavePath = GetLayerSavePath(layerPath.c_str());
        ArchUnlinkFile(layerSavePath.c_str());
    }
}

bool RprIpcClient::LayerController::Update() {
    if (m_updates.empty()) return false;

    SdfChangeBlock changeBlock;
    auto sublayerPaths = m_rootStage->GetRootLayer()->GetSubLayerPaths();

    for (auto const& entry : m_updates) {
        auto& layerPath = entry.first;
        auto layerFilePath = GetLayerFilePath(layerPath.c_str());

        size_t layerIdx = sublayerPaths.Find(layerFilePath);

        if (entry.second == LayerUpdateType::Added) {
            if (layerIdx != size_t(-1)) {
                TF_CODING_ERROR("Invalid data from the server: \"%s\" already exists", layerPath.c_str());
            } else {
                TF_DEBUG(RPR_IPC_DEBUG_MESSAGES).Msg("RprIpcClient: adding layer: %s\n", layerPath.c_str());
                sublayerPaths.insert(sublayerPaths.begin(), layerFilePath);
            }
        } else if (entry.second == LayerUpdateType::Removed) {
            if (layerIdx == size_t(-1)) {
                TF_CODING_ERROR("Invalid data from the server: \"%s\" does not exist", layerPath.c_str());
            } else {
                TF_DEBUG(RPR_IPC_DEBUG_MESSAGES).Msg("RprIpcClient: removing layer: %s\n", layerPath.c_str());
                sublayerPaths.Erase(layerIdx);
            }
        }
    }
    m_updates.clear();

    m_rootStage->Save();

    return true;
}

std::string RprIpcClient::LayerController::GetLayerSavePath(const char* layerPath) {
    return TfNormPath(TfStringPrintf("%s%s.usda", ArchGetTmpDir(), layerPath));
}

std::string RprIpcClient::LayerController::GetLayerFilePath(const char* layerPath) {
    return TfStringPrintf(".%s.usda", layerPath);
}

PXR_NAMESPACE_CLOSE_SCOPE
