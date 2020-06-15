/************************************************************************
Copyright 2020 Advanced Micro Devices, Inc
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
************************************************************************/

#include "mesh.h"
#include "renderParam.h"

PXR_NAMESPACE_OPEN_SCOPE

HdRprMesh::HdRprMesh(SdfPath const& id, SdfPath const& instancerId)
    : HdMesh(id, instancerId) {

}

HdDirtyBits HdRprMesh::_PropagateDirtyBits(HdDirtyBits bits) const {
    return bits;
}

HdDirtyBits HdRprMesh::GetInitialDirtyBitsMask() const {
    // The initial dirty bits control what data is available on the first
    // run through _PopulateMesh(), so it should list every data item
    // that _PopluateMesh requests.
    int mask = HdChangeTracker::Clean
        | HdChangeTracker::DirtyPoints
        | HdChangeTracker::DirtyTopology
        | HdChangeTracker::DirtyTransform
        | HdChangeTracker::DirtyPrimvar
        | HdChangeTracker::DirtyNormals
        | HdChangeTracker::DirtyMaterialId
        | HdChangeTracker::DirtySubdivTags
        | HdChangeTracker::DirtyDisplayStyle
        | HdChangeTracker::DirtyVisibility
        | HdChangeTracker::DirtyInstancer
        | HdChangeTracker::DirtyInstanceIndex
        | HdChangeTracker::DirtyDoubleSided
        ;

    return (HdDirtyBits)mask;
}

void HdRprMesh::_InitRepr(TfToken const& reprName,
                          HdDirtyBits* dirtyBits) {
    TF_UNUSED(reprName);
    TF_UNUSED(dirtyBits);

    // No-op
}

void HdRprMesh::Sync(HdSceneDelegate* sceneDelegate,
                     HdRenderParam* renderParam,
                     HdDirtyBits* dirtyBits,
                     TfToken const& reprName) {
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    auto rprRenderParam = static_cast<HdRprRenderParam*>(renderParam);

    SdfPath const& id = GetId();

    ////////////////////////////////////////////////////////////////////////
    // 1. Pull scene data.

    bool updateLayer = false;

    if (!m_layer) {
        m_layer = rprRenderParam->ipcServer->AddLayer(id);
        if (!m_layer) {
            *dirtyBits = HdChangeTracker::Clean;
            return;
        }

        auto stage = m_layer->GetStage();
        m_mesh = UsdGeomMesh::Define(stage, id);
        stage->SetDefaultPrim(m_mesh.GetPrim());

        updateLayer = true;
    }

    // auto meshPrim = m_mesh.GetPrim();
    // auto materialRel = meshPrim.CreateRelationship(UsdShadeTokens->materialBinding, false);
    // materialRel.SetTargets({m_scopes[kMaterialScope].GetLayerPath(meshData.materialId)});

    if (HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, HdTokens->points)) {
        m_mesh.CreatePointsAttr(sceneDelegate->Get(id, HdTokens->points));

        updateLayer = true;
    }

    if (HdChangeTracker::IsTopologyDirty(*dirtyBits, id)) {
        auto topology = GetMeshTopology(sceneDelegate);
        m_mesh.CreateFaceVertexCountsAttr(VtValue(topology.GetFaceVertexCounts()));
        m_mesh.CreateFaceVertexIndicesAttr(VtValue(topology.GetFaceVertexIndices()));
        m_mesh.CreateSubdivisionSchemeAttr(VtValue(topology.GetScheme()));

        updateLayer = true;
    }

    // std::map<HdInterpolation, HdPrimvarDescriptorVector> primvarDescsPerInterpolation = {
    //     {HdInterpolationFaceVarying, sceneDelegate->GetPrimvarDescriptors(id, HdInterpolationFaceVarying)},
    //     {HdInterpolationVertex, sceneDelegate->GetPrimvarDescriptors(id, HdInterpolationVertex)},
    //     {HdInterpolationConstant, sceneDelegate->GetPrimvarDescriptors(id, HdInterpolationConstant)},
    // };

    // if (HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, HdTokens->normals)) {
    //     m_authoredNormals = GetPrimvarData(HdTokens->normals, sceneDelegate, primvarDescsPerInterpolation, m_normals, m_normalIndices);

    //     updateLayer = true;
    // }

    // if (*dirtyBits & HdChangeTracker::DirtyMaterialId) {
    //     m_cachedMaterialId = sceneDelegate->GetMaterialId(id);
    // }

    // auto material = static_cast<const HdRprMaterial*>(sceneDelegate->GetRenderIndex().GetSprim(HdPrimTypeTokens->material, m_cachedMaterialId));
    // if (material) {
    //     if (HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, material->GetStName())) {
    //         GetPrimvarData(material->GetStName(), sceneDelegate, primvarDescsPerInterpolation, m_uvs, m_uvIndices);

    //         updateLayer = true;
    //     }
    // }

    if (*dirtyBits & HdChangeTracker::DirtyVisibility) {
        m_mesh.CreateVisibilityAttr(VtValue(sceneDelegate->GetVisible(id)));

        // TODO: consider some sort of optimization for this (maybe wrap this layer into another layer with wrapping Xform)
        updateLayer = true;
    }

    ////////////////////////////////////////////////////////////////////////
    // 2. Resolve drawstyles

    // bool isRefineLevelDirty = false;
    // if (*dirtyBits & HdChangeTracker::DirtyDisplayStyle) {
    //     m_displayStyle = sceneDelegate->GetDisplayStyle(id);
    //     if (m_refineLevel != m_displayStyle.refineLevel) {
    //         isRefineLevelDirty = true;
    //         m_refineLevel = m_displayStyle.refineLevel;
    //     }
    // }

    // bool isVisibilityMaskDirty = false;
    // if (*dirtyBits & HdChangeTracker::DirtyPrimvar) {
    //     HdRprGeometrySettings geomSettings = {};
    //     geomSettings.visibilityMask = kVisibleAll;
    //     HdRprParseGeometrySettings(sceneDelegate, id, primvarDescsPerInterpolation.at(HdInterpolationConstant), &geomSettings);

    //     if (m_refineLevel != geomSettings.subdivisionLevel) {
    //         m_refineLevel = geomSettings.subdivisionLevel;
    //         isRefineLevelDirty = true;
    //     }

    //     if (m_visibilityMask != geomSettings.visibilityMask) {
    //         m_visibilityMask = geomSettings.visibilityMask;
    //         isVisibilityMaskDirty = true;
    //     }
    // }

    // m_smoothNormals = m_displayStyle.flatShadingEnabled;
    // // Don't compute smooth normals on a refined m_mesh. They are implicitly smooth.
    // m_smoothNormals = m_smoothNormals && !(m_enableSubdiv && m_refineLevel > 0);

    // if (!m_authoredNormals && m_smoothNormals) {
    //     if (!m_adjacencyValid) {
    //         m_adjacency.BuildAdjacencyTable(&m_topology);
    //         m_adjacencyValid = true;
    //         m_normalsValid = false;
    //     }

    //     if (!m_normalsValid) {
    //         m_normals = Hd_SmoothNormals::ComputeSmoothNormals(&m_adjacency, m_points.size(), m_points.cdata());
    //         m_normalsValid = true;

    //         updateLayer = true;
    //     }
    // }

    if (*dirtyBits & HdChangeTracker::DirtyTransform) {
        m_mesh.MakeMatrixXform().Set(sceneDelegate->GetTransform(id));

        // TODO: consider some sort of optimization for this (maybe wrap this layer into another layer with wrapping Xform)
        updateLayer = true;
    }

    if (updateLayer) {
        rprRenderParam->ipcServer->OnLayerEdit(id, m_layer);
    }

    *dirtyBits = HdChangeTracker::Clean;
}

void HdRprMesh::Finalize(HdRenderParam* renderParam) {
    if (m_layer) {
        auto rprRenderParam = static_cast<HdRprRenderParam*>(renderParam);

        rprRenderParam->ipcServer->RemoveLayer(GetId());
        m_layer = nullptr;
    }

    HdMesh::Finalize(renderParam);
}

PXR_NAMESPACE_CLOSE_SCOPE
