#include "plugin.h"
#include "iprPort.h"
#include "debugCodes.h"

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

static SdfPath GetRelativeLayerPath(std::string primId) {
    // TODO: sanitize id
    return SdfPath(primId);
}

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
    , m_dcc(dcc)
    , m_iprPort(new IPRPort(this)) {
    m_scopes[kMaterialScope] = {SdfPath("/material"), {}};
    m_scopes[kGeometryScope] = {SdfPath("/geo"), {}};
}

std::string Plugin::ProcessCommand(std::string const& command) {
    TF_DEBUG(HD_USD_IPC_DEBUG_IPR_COMMANDS).Msg("ipr command: %s", command.c_str());

    if (_tokens->getStage == command) {
        m_fullStageResync = true;
    } else if (TfStringStartsWith(command, _tokens->getLayer)) {
        auto tokens = TfStringTokenize(command);
        if (tokens.size() != 2) {
            return "invalid args";
        }

        if (!m_fullStageResync) {
            TF_DEBUG(HD_USD_IPC_DEBUG_IPR_COMMANDS).Msg("getLayer: %s", tokens[1].c_str());

            auto layerPath = SdfPath(tokens[1]);
            if (layerPath == SdfPath::EmptyPath()) {
                TF_RUNTIME_ERROR("Invalid layer requested: %s", tokens[1].c_str());
                return "invalid path";
            }
            m_requestedLayers.insert(std::move(layerPath));
        }
    } else {
        return "unknown";
    }

    return "ok";
}

void Plugin::Update() {
    UpdateStage();

    if (m_fullStageResync) {
        m_fullStageResync = false;

        for (auto& scope : m_scopes) {
            for (auto& entry : scope.layers) {
                SendLayer(scope.GetLayerPath(entry.first), entry.second);
            }
        }
    } else if (!m_requestedLayers.empty()) {
        for (auto& layerPath : m_requestedLayers) {
            if (auto layer = FindLayer(layerPath)) {
                SendLayer(layerPath, *layer);
            }
        }
    }
    m_requestedLayers.clear();

    m_iprPort->Update();
}

void Plugin::UpdateStage() {
    std::vector<DCC::Change> changes;
    m_dcc.GetChanges(&changes);

    for (auto& change : changes) {
        auto relLayerPath = GetRelativeLayerPath(change.primId);

        Scope* scope;
        if (change.primType == DCC::PrimitiveType::Mesh) {
            scope = &m_scopes[kGeometryScope];
        } else if (change.primType == DCC::PrimitiveType::Material) {
            scope = &m_scopes[kMaterialScope];
        } else {
            TF_CODING_ERROR("Unknown primitive type");
            return;
        }

        if (change.type == DCC::Change::Type::Remove) {
            auto it = scope->layers.find(relLayerPath);
            if (it == scope->layers.end()) {
                TF_RUNTIME_ERROR("Invalid info from DCC: \"%s\" does not exist", relLayerPath.GetText());
                continue;
            }
            scope->layers.erase(it);

            m_iprPort->NotifyLayerRemove(scope->GetLayerPath(relLayerPath));
        } else {
            Layer* layer;
            if (change.type == DCC::Change::Type::Add) {
                // Sanity check: if primitive already exists
                auto it = scope->layers.find(relLayerPath);
                if (it != scope->layers.end()) {
                    TF_RUNTIME_ERROR("Invalid info from DCC: \"%s\" already exists", relLayerPath.GetText());
                    scope->layers.erase(it);
                }

                auto status = scope->layers.emplace(relLayerPath, Layer(relLayerPath));
                layer = &status.first->second;
            } else {
                // Sanity check: if primitive does not exists
                auto it = scope->layers.find(relLayerPath);
                if (it == scope->layers.end()) {
                    TF_RUNTIME_ERROR("Invalid info from DCC: \"%s\" does not exist", relLayerPath.GetText());
                    auto status = scope->layers.emplace(relLayerPath, Layer(relLayerPath));
                    it = status.first;
                }

                layer = &it->second;
            }

            auto layerPath = scope->GetLayerPath(relLayerPath);
            if (change.primType == DCC::PrimitiveType::Mesh) {
                UpdateMeshLayer(layer, layerPath, m_dcc.GetMesh(change.primId));
            } else if (change.primType == DCC::PrimitiveType::Material) {
                UpdateMaterialLayer(layer, layerPath, m_dcc.GetMaterial(change.primId));
            }

            layer->timestamp = std::chrono::steady_clock::now().time_since_epoch().count() / 1000;
            m_iprPort->NotifyLayerEdit(layerPath, layer->timestamp);
        }
    }
}

void Plugin::UpdateMeshLayer(Layer* node, SdfPath const& layerPath, Mesh const& meshData) {
    auto mesh = UsdGeomMesh::Define(node->stage, layerPath);

    mesh.CreateFaceVertexCountsAttr(VtValue(meshData.faceCounts));
    mesh.CreateFaceVertexIndicesAttr(VtValue(meshData.faceIndices));
    mesh.CreatePointsAttr(VtValue(meshData.points));
    mesh.MakeMatrixXform().Set(meshData.transform);

    auto meshPrim = mesh.GetPrim();
    auto materialRel = meshPrim.CreateRelationship(UsdShadeTokens->materialBinding, false);
    materialRel.SetTargets({m_scopes[kMaterialScope].GetLayerPath(meshData.materialId)});

    node->stage->SetDefaultPrim(meshPrim);
}

void Plugin::UpdateMaterialLayer(Layer* node, SdfPath const& layerPath, Material const& materialData) {
    auto material = UsdShadeMaterial::Define(node->stage, layerPath);

    auto shaderPath = layerPath.AppendElementString("surface");
    auto shader = UsdShadeShader::Define(node->stage, shaderPath);
    shader.CreateIdAttr().Set(_tokens->UsdPreviewSurface);
    shader.CreateInput(_tokens->diffuseColor, pxr::SdfValueTypeNames->Color3f).Set(materialData.color);

    material.CreateSurfaceOutput().ConnectToSource(shader, _tokens->surface);

    node->stage->SetDefaultPrim(material.GetPrim());
}

Plugin::Layer* Plugin::FindLayer(SdfPath const& absoluteLayerPath) {
    Scope* scope = nullptr;
    for (auto& entry : m_scopes) {
        if (absoluteLayerPath.HasPrefix(entry.path)) {
            scope = &entry;
            break;
        }
    }
    if (!scope) {
        return nullptr;
    }

    auto layerPath = absoluteLayerPath.MakeRelativePath(scope->path);
    auto layerIt = scope->layers.find(layerPath);
    if (layerIt == scope->layers.end()) {
        return nullptr;
    }

    return &layerIt->second;
}

void Plugin::SendLayer(SdfPath const& layerPath, Layer const& layer) {
    if (!layer.stage) {
        return;
    }

    std::string encodedLayer;
    if (!layer.stage->ExportToString(&encodedLayer)) {
        TF_RUNTIME_ERROR("failed to export \"%s\" layer to string\n", layerPath.GetText());
        return;
    }

    m_iprPort->SendLayer(layerPath, layer.timestamp, std::move(encodedLayer));
}

SdfPath Plugin::Scope::GetLayerPath(SdfPath const& relLayerPath) {
    return path.AppendPath(relLayerPath);
}

SdfPath Plugin::Scope::GetLayerPath(std::string primId) {
    return GetLayerPath(GetRelativeLayerPath(primId));
}

std::string GetLayerAssetPath(SdfPath const& layerPath, const char* prefix) {
    return ArchNormPath(TfStringPrintf("%s%s.usda", prefix, layerPath.GetText()));
}

Plugin::Layer::Layer(SdfPath const& layerPath)
    : timestamp(0u) {
    if (!layerPath.IsEmpty()) {
        stage = UsdStage::CreateInMemory(GetLayerAssetPath(layerPath, ArchGetTmpDir()));
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
