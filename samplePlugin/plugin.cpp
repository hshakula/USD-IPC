#include "plugin.h"
#include "iprPort.h"

#include <pxr/base/vt/value.h>
#include <pxr/base/tf/stringUtils.h>
#include <pxr/base/arch/fileSystem.h>

#include <pxr/usd/sdf/path.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/sdf/fileFormat.h>
#include <pxr/usd/usdGeom/scope.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PRIVATE_TOKENS(_tokens,
    (usdc)
    (UsdPreviewSurface)
    (diffuseColor)
    (surface)
    (getStage)
    (getLayer)
    (materials)
    (meshes)
);

Plugin::Plugin(DCC& dcc)
    : kUsdcFileFormat(SdfFileFormat::FindById(_tokens->usdc))
    , kMaterialsScopePath("/materials")
    , kMeshesScopePath("/meshes")
    , m_dcc(dcc)
    , m_iprPort(new IPRPort(this)) {

}

std::string Plugin::ProcessCommand(std::string const& command) {
    if (_tokens->getStage == command) {
        m_fullStageResync = true;
    } else if (TfStringStartsWith(command, _tokens->getLayer)) {
        auto tokens = TfStringTokenize(command);
        if (tokens.size() != 2) {
            return "invalid args";
        }

        if (!m_fullStageResync) {
            printf("Plugin: new requested layer: \"%s\"\n", tokens[1].c_str());
            m_requestedLayers.insert(tokens[1]);
        }
    } else {
        return "unknown";
    }

    return "ok";
}

void Plugin::Update() {
    UpdateStage();

    if (m_fullStageResync) {
        std::function<void(LayerNode*, std::string const&, std::string const&)> traverseNode =
            [&](LayerNode* node, std::string const& nodeId, std::string const& parentPath) {
            auto nodePath = parentPath + nodeId;
            if (!node->children.empty()) nodePath += '/';

            SendLayer(nodePath, node);

            for (auto& c : node->children) {
                traverseNode(c.second.get(), c.first, nodePath);
            }
        };
        traverseNode(m_rootLayer.get(), "", "");
        m_fullStageResync = false;
    } else if (!m_requestedLayers.empty()) {
        for (auto& layerPath : m_requestedLayers) {
            if (auto layer = FindLayer(layerPath)) {
                SendLayer(layerPath, layer);
            }
        }
    }
    m_requestedLayers.clear();

    m_iprPort->Update();
}

void Plugin::UpdateStage() {
    std::vector<DCC::Change> changes;
    m_dcc.GetChanges(&changes);

    if (!m_rootLayer) {
        m_rootLayer = CreateLayerNode(nullptr, SdfPath("/root"));

        auto materialsNode = CreateLayerNode(m_rootLayer.get());
        m_materialsNode = materialsNode.get();
        m_rootLayer->children["materials"] = std::move(materialsNode);

        auto meshesNode = CreateLayerNode(m_rootLayer.get());
        m_meshesNode = meshesNode.get();
        m_rootLayer->children["meshes"] = std::move(meshesNode);
    }

    auto sublayerPaths = m_rootLayer->stage->GetSessionLayer()->GetSubLayerPaths();

    bool updateRootLayer = false;
    for (auto& change : changes) {
        SdfPath layerPath;
        if (change.primType == DCC::PrimitiveType::Mesh) {
            layerPath = GetMeshPath(change.primId);
        } else if (change.primType == DCC::PrimitiveType::Material) {
            layerPath = GetMaterialPath(change.primId);
        } else {
            TF_CODING_ERROR("Unknown primitive type");
            continue;
        }

        UpdatePrimitiveLayer(change, layerPath);

        if (change.type == DCC::Change::Type::Add) {
            auto sublayerPath = GetLayerAssetPath(layerPath);
            auto layerPathIdx = sublayerPaths.Find(sublayerPath);
            if (layerPathIdx != size_t(-1)) {
                TF_CODING_ERROR("Invalid info from DCC: \"%s\" already exists", layerPath.GetText());
            } else {
                sublayerPaths.insert(sublayerPaths.begin(), sublayerPath);
                updateRootLayer = true;
            }
        } else if (change.type == DCC::Change::Type::Remove) {
            auto sublayerPath = GetLayerAssetPath(layerPath);
            auto layerPathIdx = sublayerPaths.Find(sublayerPath);
            if (layerPathIdx == size_t(-1)) {
                TF_CODING_ERROR("Invalid info from DCC: \"%s\" did not exist", layerPath.GetText());
            } else {
                sublayerPaths.Erase(layerPathIdx);
                updateRootLayer = true;
            }
        }
    }

    if (updateRootLayer) {
        m_rootLayer->UpdateTimestamp();
        m_iprPort->NotifyLayerEdit("/", m_rootLayer->timestamp);
    }
}

void Plugin::UpdatePrimitiveLayer(
    DCC::Change const& change,
    SdfPath const& layerPath) {
    LayerNode* parentLayer;
    if (change.primType == DCC::PrimitiveType::Mesh) {
        parentLayer = m_meshesNode;
    } else if (change.primType == DCC::PrimitiveType::Material) {
        parentLayer = m_materialsNode;
    } else {
        TF_CODING_ERROR("Unknown primitive type");
        return;
    }

    if (change.type == DCC::Change::Type::Remove) {
        auto it = parentLayer->children.find(change.primId);
        if (it == parentLayer->children.end()) {
            TF_RUNTIME_ERROR("Invalid info from DCC: \"%s\" does not exist", layerPath.GetText());
            return;
        }
        parentLayer->children.erase(it);

        m_iprPort->NotifyLayerRemove(layerPath.GetString());
    } else {
        LayerNode* layer;
        if (change.type == DCC::Change::Type::Add) {
            // Sanity check: if primitive already exists
            auto it = parentLayer->children.find(change.primId);
            if (it != parentLayer->children.end()) {
                TF_RUNTIME_ERROR("Invalid info from DCC: \"%s\" already exists", layerPath.GetText());
                parentLayer->children.erase(it);
            }

            auto status = parentLayer->children.emplace(change.primId, CreateLayerNode(parentLayer, layerPath));
            layer = status.first->second.get();
        } else {
            // Sanity check: if primitive does not exists
            auto it = parentLayer->children.find(change.primId);
            if (it == parentLayer->children.end()) {
                TF_RUNTIME_ERROR("Invalid info from DCC: \"%s\" does not exist", layerPath.GetText());
                auto status = parentLayer->children.emplace(change.primId, CreateLayerNode(parentLayer, layerPath));
                it = status.first;
            }

            layer = it->second.get();
        }

        if (change.primType == DCC::PrimitiveType::Mesh) {
            UpdateMeshLayer(layer, layerPath, m_dcc.GetMesh(change.primId));
        } else if (change.primType == DCC::PrimitiveType::Material) {
            UpdateMaterialLayer(layer, layerPath, m_dcc.GetMaterial(change.primId));
        }

        layer->UpdateTimestamp();
        m_iprPort->NotifyLayerEdit(layerPath.GetString(), layer->timestamp);
    }
}

void Plugin::UpdateMeshLayer(LayerNode* node, SdfPath const& layerPath, Mesh const& meshData) {
    auto mesh = UsdGeomMesh::Define(node->stage, layerPath);

    mesh.CreateFaceVertexCountsAttr(VtValue(meshData.faceCounts));
    mesh.CreateFaceVertexIndicesAttr(VtValue(meshData.faceIndices));
    mesh.CreatePointsAttr(VtValue(meshData.points));
    mesh.MakeMatrixXform().Set(meshData.transform);

    auto meshPrim = mesh.GetPrim();
    auto materialRel = meshPrim.CreateRelationship(UsdShadeTokens->materialBinding, false);
    materialRel.SetTargets({GetMaterialPath(meshData.materialId)});

    node->stage->SetDefaultPrim(meshPrim);
}

void Plugin::UpdateMaterialLayer(LayerNode* node, SdfPath const& layerPath, Material const& materialData) {
    auto material = UsdShadeMaterial::Define(node->stage, layerPath);

    auto shaderPath = layerPath.AppendElementString("surface");
    auto shader = UsdShadeShader::Define(node->stage, shaderPath);
    shader.CreateIdAttr().Set(_tokens->UsdPreviewSurface);
    shader.CreateInput(_tokens->diffuseColor, pxr::SdfValueTypeNames->Color3f).Set(materialData.color);

    material.CreateSurfaceOutput().ConnectToSource(shader, _tokens->surface);

    node->stage->SetDefaultPrim(material.GetPrim());
}

void Plugin::LayerNode::UpdateTimestamp() {
    timestamp = std::chrono::steady_clock::now().time_since_epoch().count() / 1000;
}

std::unique_ptr<Plugin::LayerNode> Plugin::CreateLayerNode(LayerNode* parent, SdfPath const& layerPath) {
    auto layerNode = std::make_unique<LayerNode>();
    layerNode->parent = parent;
    layerNode->timestamp = 0u;
    if (!layerPath.IsEmpty()) {
        // Ideally, we would like to avoid any interaction with the disk.
        // Semantically the best fit for this purpose would be UsdStage::CreateInMemory.
        // But we cannot use it because whenever we will try to reference or sublayer this
        // stage from the root stage or any other stage it will trigger errors (e.g. "could
        // not find such layer"). Referencing in such a case would not work at all,
        // sublayering will take effect but with errors in the console.
        //
        // UsdStage::CreateNew costs us a file creation with "#usda 1.0" as its contents.
        // Until we call UsdStage::Save, which we never do, nothing more is written to the disk.
        layerNode->stage = UsdStage::CreateNew(GetLayerAssetPath(layerPath, ArchGetTmpDir()));
    }
    return layerNode;
}

Plugin::LayerNode* Plugin::FindLayer(std::string const& layerPath) {
    if (layerPath[0] != '/') {
        return nullptr;
    }

    if (layerPath == "/") {
        return m_rootLayer.get();
    }

    LayerNode* parentNode;

    auto tokens = TfStringTokenize(layerPath, "/");
    if (tokens.size() != 2) {
        return nullptr;
    }

    if (tokens[0] == "meshes") {
        parentNode = m_meshesNode;
    } else if (tokens[0] == "materials") {
        parentNode = m_materialsNode;
    } else {
        return nullptr;
    }

    auto layerIt = parentNode->children.find(tokens[1]);
    if (layerIt == parentNode->children.end()) {
        return nullptr;
    }

    return layerIt->second.get();
}

void Plugin::SendLayer(std::string const& layerPath, LayerNode* layer) {
    if (!layer->stage) {
        return;
    }

    std::string encodedLayer;
    if (layerPath == "/") {
        if (!layer->stage->GetSessionLayer()->ExportToString(&encodedLayer)) {
            printf("Plugin: failed to export root layer to string\n");
            return;
        }
        printf("Root layer:\n%s\n", encodedLayer.c_str());
    } else {
        if (!layer->stage->ExportToString(&encodedLayer)) {
            printf("Plugin: failed to export \"%s\" layer to string\n", layerPath.c_str());
            return;
        }
    }

    m_iprPort->SendLayer(layerPath, layer->timestamp, std::move(encodedLayer));
}

std::string Plugin::GetLayerAssetPath(SdfPath const& layerPath, const char* prefix) {
    return ArchNormPath(TfStringPrintf("%s%s.usda", prefix, layerPath.GetText()));
}

SdfPath Plugin::GetMaterialPath(std::string const& id) const {
    return kMaterialsScopePath.AppendElementString(id);
}

SdfPath Plugin::GetMeshPath(std::string const& id) const {
    return kMeshesScopePath.AppendElementString(id);
}

PXR_NAMESPACE_CLOSE_SCOPE
