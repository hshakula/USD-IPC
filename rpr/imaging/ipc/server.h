#ifndef RPR_IMAGING_IPC_SERVER_H
#define RPR_IMAGING_IPC_SERVER_H

#include <zmq.hpp>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/stage.h>
#include <rpr/imaging/ipc/api.h>

#include <thread>
#include <mutex>

PXR_NAMESPACE_OPEN_SCOPE

/// \class RprIpcServer
///
/// All methods are thread-safe unless otherwise indicated
class RprIpcServer {
public:
    // ---------------------------------------------------------------------
    /// \name Construction
    /// @{
    // ---------------------------------------------------------------------

    /// Each callback is called from network thread, ideally, they should not be heavyweight
    class Listener {
    public:
        /// The callback to receive commands sent by @see RprIpcClient#SendCommand
        virtual bool ProcessCommand(std::string const& command,
                                    uint8_t* payload, size_t pyaloadSize) = 0;
    };

    RPR_IPC_API
    RprIpcServer(Listener* Listener);

    RPR_IPC_API
    ~RprIpcServer();

    /// @}

    // ---------------------------------------------------------------------
    /// \name Layers management
    /// @{
    // ---------------------------------------------------------------------

    class Layer;

    RPR_IPC_API
    Layer* AddLayer(SdfPath const& layerPath);

    RPR_IPC_API
    void OnLayerEdit(SdfPath const& layerPath, Layer* layer);

    RPR_IPC_API
    void RemoveLayer(SdfPath const& layerPath);

private:
    class Sender;

public:
    class Layer {
    public:
        UsdStagePtr GetStage() { return m_stage; };

    private:
        Layer();

        void OnEdit();
        std::string const& GetEncodedStage();

    private:
        UsdStageRefPtr m_stage;
        uint64_t m_timestamp;

        std::string m_encodedStage;
        bool m_isEncodedStageDirty;

        std::shared_ptr<Sender> m_cachedSender;

        friend RprIpcServer;
    };

    /// @}

private:
    void RunNetworkWorker();

    void ProcessControlSocket();
    void ProcessAppSocket();

    void SendAllLayers();

private:
    Listener* m_listener;
    std::thread m_networkThread;

    zmq::socket_t m_controlSocket;
    zmq::socket_t m_appSocket;
    zmq::socket_t m_dataSocket;

    std::mutex m_layersMutex;
    std::map<SdfPath, std::unique_ptr<Layer>> m_layers;

private:
    /// \class Sender
    ///
    /// Sender designed to allow easy data transmission from many worker threads to the client.
    /// Sender should be used on the same thread it was created on. It can be created only with \c GetSender.
    class Sender {
    public:
        void SendLayer(SdfPath const& layerPath, std::string layer);
        void RemoveLayer(SdfPath const& layerPath);

    private:
        friend RprIpcServer;
        Sender(std::thread::id owningThread, zmq::socket_t&& socket);
        Sender(std::thread::id owningThread, zmq::socket_t* socket);

    private:
        std::thread::id m_owningThreadId;

        zmq::socket_t m_retainedSocket;
        zmq::socket_t* m_pushSocket;
    };

    /// The returned sender can be cached to avoid socket creation and connection.
    /// But the sender should be used on the same thread it was created on.
    /// If this function receives a cached sender it will validate thread conformity
    /// and in case of non-compliance, it will create a new sender.
    void GetSender(std::shared_ptr<Sender>* sender);

private:
    std::unordered_map<std::thread::id, std::weak_ptr<Sender>> m_senders;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // RPR_IMAGING_IPC_SERVER_H
