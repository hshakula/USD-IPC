#ifndef PLUGIN_H
#define PLUGIN_H

#include "dcc.h"
#include "iprPort.h"

#include <pxr/usd/usd/stage.h>
#include <memory>

PXR_NAMESPACE_OPEN_SCOPE

class Plugin : public IPRPort::CommandListener {
public:
    Plugin(DCC& dcc);

    void Update();

    // IPRPort::CommandListener implementation
    std::string ProcessCommand(std::string const& command) override;

private:
    void UpdateStage();

    struct LayerNode;
    void UpdatePrimitiveLayer(DCC::Change const& change, SdfPath const& layerPath);

    void UpdateMeshLayer(LayerNode* node, SdfPath const& layerPath, Mesh const& mesh);
    void UpdateMaterialLayer(LayerNode* node, SdfPath const& layerPath, Material const& material);

private:
    DCC& m_dcc;

    std::unique_ptr<IPRPort> m_iprPort;

private:
    /// Enqueued IPR requests
    bool m_fullStageResync = true;
    std::set<std::string> m_requestedLayers;

private:
    const SdfFileFormatConstPtr kUsdcFileFormat;
    const SdfPath kMaterialsScopePath;
    const SdfPath kMeshesScopePath;

    SdfPath GetMaterialPath(std::string const& id) const;
    SdfPath GetMeshPath(std::string const& id) const;

private:
    struct LayerNode {
        /// Time of the last edit, time is since epoch in microseconds
        uint64_t timestamp;

        /// UsdStage of the current layer
        UsdStageRefPtr stage;

        LayerNode* parent;
        std::map<std::string, std::unique_ptr<LayerNode>> children;

        void UpdateTimestamp();
    };
    std::unique_ptr<LayerNode> m_rootLayer;
    LayerNode* m_materialsNode;
    LayerNode* m_meshesNode;

    std::unique_ptr<LayerNode> CreateLayerNode(LayerNode* parent = nullptr, SdfPath const& layerPath = SdfPath());
    LayerNode* FindLayer(std::string const& layerPath);
    void SendLayer(std::string const& layerPath, LayerNode* layer);
    std::string GetLayerAssetPath(SdfPath const& layerPath, const char* prefix = ".");
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PLUGIN_H
