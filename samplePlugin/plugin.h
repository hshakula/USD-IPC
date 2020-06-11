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

    struct Layer;
    void UpdateMeshLayer(Layer* node, SdfPath const& layerPath, Mesh const& mesh);
    void UpdateMaterialLayer(Layer* node, SdfPath const& layerPath, Material const& material);

private:
    DCC& m_dcc;

    std::unique_ptr<IPRPort> m_iprPort;

private:
    /// Enqueued IPR requests
    bool m_fullStageResync = true;
    std::set<SdfPath> m_requestedLayers;

private:
    const SdfFileFormatConstPtr kUsdcFileFormat;

private:
    struct Layer {
        /// Time of the last edit, time is since epoch in microseconds
        uint64_t timestamp;

        /// UsdStage of the current layer
        UsdStageRefPtr stage;

        Layer(SdfPath const& layerPath);
    };

    struct Scope {
        SdfPath path;
        std::map<SdfPath, Layer> layers;

        SdfPath GetLayerPath(SdfPath const& relativeLayerPath);
        SdfPath GetLayerPath(std::string primId);
    };

    enum ScopeType {
        kMaterialScope = 0,
        kGeometryScope,
        kScopeCount
    };
    Scope m_scopes[kScopeCount];

    Layer* FindLayer(SdfPath const& layerPath);
    void SendLayer(SdfPath const& layerPath, Layer const& layer);
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PLUGIN_H
