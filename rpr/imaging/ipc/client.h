#ifndef RPR_IMAGING_IPC_CLIENT_H
#define RPR_IMAGING_IPC_CLIENT_H

#include <zmq.hpp>
#include <pxr/usd/usd/stage.h>
#include <pxr/base/tf/declarePtrs.h>
#include <rpr/imaging/ipc/api.h>

#include <functional>
#include <thread>
#include <memory>
#include <string>
#include <map>
#include <set>

PXR_NAMESPACE_OPEN_SCOPE

class RprIpcClient;
TF_DECLARE_WEAK_AND_REF_PTRS(RprIpcClient);

/// \class RprIpcClient
///
/// All methods are NOT thread-safe unless otherwise indicated
class RprIpcClient : public TfRefBase, public TfWeakBase {
public:
    // ---------------------------------------------------------------------
    /// \name Construction
    /// @{
    // ---------------------------------------------------------------------

    RPR_IPC_API
    static RprIpcClientRefPtr Create(std::string const& serverAddress,
                                     std::function<void()> onStageUpdateCallback);

    RPR_IPC_API
    ~RprIpcClient();

    /// @}

    // ---------------------------------------------------------------------
    /// \name Server control
    /// @{
    // ---------------------------------------------------------------------

    RPR_IPC_API
    bool SendCommand(std::string const& command, std::string const& payload);

    /// @}

    // ---------------------------------------------------------------------
    /// \name Stage
    /// @{
    // ---------------------------------------------------------------------

    RPR_IPC_API
    UsdStagePtr GetStage();

    /// @}

private:
    RprIpcClient(std::string const& serverAddress,
                 std::function<void()> onStageUpdateCallback);

    void RunNetworkWorker();

    using MessageComposer = std::function<void(zmq::socket_t& socket)>;
    std::string TryRequest(MessageComposer messageComposer,
                           long timeoutMs = -1, int numRetries = 3);

    void SetupControlSocket();

    void ProcessDataSocket();

private:
    class LayerController;
    std::unique_ptr<LayerController> m_layerController;
    std::thread m_networkThread;

    std::string m_serverAddress;

    zmq::socket_t m_controlSocket;
    zmq::socket_t m_dataSocket;
    zmq::socket_t m_appSocket;

    std::function<void()> m_onStageUpdate;

private:
    class LayerController {
    public:
        LayerController();
        ~LayerController();

        void AddLayer(std::string const& layerPath,
                      char* encodedLayer,
                      size_t encodedLayerSize);
        void RemoveLayer(std::string const& layerPath);

        bool Update();

        UsdStagePtr GetStage() { return m_rootStage; }

    private:
        std::string GetLayerSavePath(const char* layerPath);
        std::string GetLayerFilePath(const char* layerPath);

    private:
        UsdStageRefPtr m_rootStage;
        std::set<std::string> m_layers;

        enum class LayerUpdateType {
            Added,
            Removed,
            Edited
        };
        std::map<std::string, LayerUpdateType> m_updates;
    };
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // RPR_IMAGING_IPC_CLIENT_H
