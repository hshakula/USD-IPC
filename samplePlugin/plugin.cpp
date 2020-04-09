#include "plugin.h"
#include "iprPort.h"

#include <pxr/base/vt/value.h>
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
);

Plugin::Plugin(DCC& dcc)
    : m_dcc(dcc)
    , m_iprPort(new IPRPort(this)) {
    auto usdcFileFormat = SdfFileFormat::FindById(_tokens->usdc);
    auto layer = SdfLayer::CreateAnonymous("layer", usdcFileFormat);
    m_stage = UsdStage::CreateInMemory("stage", layer);
    if (!m_stage) {
        throw std::runtime_error("Failed to create stage");
    }
}

void Plugin::Update() {
    if (m_dcc.IsSceneChanged()) {
        UpdateStage();

        m_iprPort->StageChanged();
    }

    m_iprPort->Update();
}

void Plugin::UpdateStage() {

    std::map<std::string, UsdShadeMaterial> shadeMaterials;

    SdfPath materialsScopePath("/materials");

    // Remove all previous materials
    m_stage->RemovePrim(materialsScopePath);

    UsdGeomScope::Define(m_stage, materialsScopePath);
    for (auto& material : m_dcc.GetMaterials()) {
        auto materialPath = materialsScopePath.AppendElementString(material.id);
        auto shadeMaterial = UsdShadeMaterial::Define(m_stage, materialPath);

        auto shaderPath = materialPath.AppendElementString("surface");
        auto shader = UsdShadeShader::Define(m_stage, shaderPath);
        shader.CreateIdAttr().Set(_tokens->UsdPreviewSurface);
        shader.CreateInput(_tokens->diffuseColor, pxr::SdfValueTypeNames->Color3f).Set(material.color);

        shadeMaterial.CreateSurfaceOutput().ConnectToSource(shader, _tokens->surface);

        shadeMaterials[material.id] = shadeMaterial;
    }

    SdfPath meshesScopePath("/meshes");

    // Remove all previous meshes
    m_stage->RemovePrim(meshesScopePath);

    UsdGeomScope::Define(m_stage, meshesScopePath);
    for (auto& mesh : m_dcc.GetMeshes()) {
        auto meshPath = meshesScopePath.AppendElementString(mesh.id);
        auto meshPrim = UsdGeomMesh::Define(m_stage, meshPath);

        meshPrim.CreateFaceVertexCountsAttr(VtValue(mesh.faceCounts));
        meshPrim.CreateFaceVertexIndicesAttr(VtValue(mesh.faceIndices));
        meshPrim.CreatePointsAttr(VtValue(mesh.points));

        meshPrim.AddTransformOp().Set(mesh.transform);

        auto materialBinding = UsdShadeMaterialBindingAPI::Apply(meshPrim.GetPrim());
        materialBinding.Bind(shadeMaterials[mesh.materialId]);
    }
}

std::string Plugin::GetEncodedStage() {
    printf("Plugin: getting encoded stage\n");

    std::string encodedStage;
    if (!m_stage->ExportToString(&encodedStage)) {
        TF_RUNTIME_ERROR("Failed to export stage to string");
        return "";
    }
    return encodedStage;
}

PXR_NAMESPACE_CLOSE_SCOPE
