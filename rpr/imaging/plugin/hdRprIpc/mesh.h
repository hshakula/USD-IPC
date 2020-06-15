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

#ifndef HDRPR_MESH_H
#define HDRPR_MESH_H

#include "pxr/imaging/hd/mesh.h"
#include "pxr/usd/usdGeom/mesh.h"
#include "rpr/imaging/ipc/server.h"

PXR_NAMESPACE_OPEN_SCOPE

class HdRprMesh final : public HdMesh {
public:
    HF_MALLOC_TAG_NEW("new HdRprMesh");

    HdRprMesh(SdfPath const& id, SdfPath const& instancerId = SdfPath());
    ~HdRprMesh() override = default;

    void Sync(HdSceneDelegate* sceneDelegate,
              HdRenderParam* renderParam,
              HdDirtyBits* dirtyBits,
              TfToken const& reprName) override;

    void Finalize(HdRenderParam* renderParam) override;

    HdDirtyBits GetInitialDirtyBitsMask() const override;

protected:
    HdDirtyBits _PropagateDirtyBits(HdDirtyBits bits) const override;

    void _InitRepr(TfToken const& reprName, HdDirtyBits* dirtyBits) override;

private:
    UsdGeomMesh m_mesh;
    RprIpcServer::Layer* m_layer = nullptr;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDRPR_MESH_H
