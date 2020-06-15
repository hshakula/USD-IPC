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

#include "renderDelegate.h"

#include "renderPass.h"
#include "mesh.h"
// #include "light.h"
// #include "material.h"
#include "renderBuffer.h"

#include <pxr/imaging/hd/instancer.h>
#include <pxr/imaging/hd/camera.h>

#include <ctime>
#include <iomanip>
#include <sstream>
#include <cstdio>

PXR_NAMESPACE_OPEN_SCOPE

const TfTokenVector HdRprIpcDelegate::SUPPORTED_RPRIM_TYPES = {
    HdPrimTypeTokens->mesh,
};

const TfTokenVector HdRprIpcDelegate::SUPPORTED_SPRIM_TYPES = {
    HdPrimTypeTokens->camera,
    // HdPrimTypeTokens->material,
    // HdPrimTypeTokens->rectLight,
    // HdPrimTypeTokens->sphereLight,
    // HdPrimTypeTokens->cylinderLight,
    // HdPrimTypeTokens->diskLight,
    // HdPrimTypeTokens->distantLight,
};

const TfTokenVector HdRprIpcDelegate::SUPPORTED_BPRIM_TYPES = {
    HdPrimTypeTokens->renderBuffer
};

HdRprIpcDelegate::HdRprIpcDelegate(HdRenderSettingsMap const& renderSettings)
    : m_ipcServer(std::make_unique<RprIpcServer>(this))
    , m_renderParam(std::make_unique<HdRprRenderParam>(m_ipcServer.get(), &m_renderThread)) {
    for (auto& entry : renderSettings) {
        SetRenderSetting(entry.first, entry.second);
    }

    m_renderThread.SetRenderCallback([this]() {
        // no-op
    });
    m_renderThread.StartThread();
}

HdRprIpcDelegate::~HdRprIpcDelegate() = default;

HdRenderParam* HdRprIpcDelegate::GetRenderParam() const {
    return m_renderParam.get();
}

void HdRprIpcDelegate::CommitResources(HdChangeTracker* tracker) {
    // CommitResources() is called after prim sync has finished, but before any
    // tasks (such as draw tasks) have run.
}

TfToken HdRprIpcDelegate::GetMaterialNetworkSelector() const {
    return TfToken("rpr");
}

TfTokenVector const& HdRprIpcDelegate::GetSupportedRprimTypes() const {
    return SUPPORTED_RPRIM_TYPES;
}

TfTokenVector const& HdRprIpcDelegate::GetSupportedSprimTypes() const {
    return SUPPORTED_SPRIM_TYPES;
}

TfTokenVector const& HdRprIpcDelegate::GetSupportedBprimTypes() const {
    return SUPPORTED_BPRIM_TYPES;
}

HdResourceRegistrySharedPtr HdRprIpcDelegate::GetResourceRegistry() const {
    return HdResourceRegistrySharedPtr(new HdResourceRegistry());
}

HdRenderPassSharedPtr HdRprIpcDelegate::CreateRenderPass(HdRenderIndex* index,
                                                      HdRprimCollection const& collection) {
    return HdRenderPassSharedPtr(new HdRprRenderPass(index, collection, m_renderParam.get()));
}

HdInstancer* HdRprIpcDelegate::CreateInstancer(HdSceneDelegate* delegate,
                                            SdfPath const& id,
                                            SdfPath const& instancerId) {
    return new HdInstancer(delegate, id, instancerId);
}

void HdRprIpcDelegate::DestroyInstancer(HdInstancer* instancer) {
    delete instancer;
}

HdRprim* HdRprIpcDelegate::CreateRprim(TfToken const& typeId,
                                    SdfPath const& rprimId,
                                    SdfPath const& instancerId) {
    if (typeId == HdPrimTypeTokens->mesh) {
        return new HdRprMesh(rprimId, instancerId);
    }

    TF_CODING_ERROR("Unknown Rprim Type %s", typeId.GetText());
    return nullptr;
}

void HdRprIpcDelegate::DestroyRprim(HdRprim* rPrim) {
    delete rPrim;
}

HdSprim* HdRprIpcDelegate::CreateSprim(TfToken const& typeId,
                                    SdfPath const& sprimId) {
    if (typeId == HdPrimTypeTokens->camera) {
        return new HdCamera(sprimId);
    } /*else if (typeId == HdPrimTypeTokens->rectLight ||
        typeId == HdPrimTypeTokens->sphereLight ||
        typeId == HdPrimTypeTokens->cylinderLight ||
        typeId == HdPrimTypeTokens->diskLight) {
        return new HdRprLight(sprimId, typeId);
    } else if (typeId == HdPrimTypeTokens->material) {
        return new HdRprMaterial(sprimId);
    }*/

    TF_CODING_ERROR("Unknown Sprim Type %s", typeId.GetText());
    return nullptr;
}

HdSprim* HdRprIpcDelegate::CreateFallbackSprim(TfToken const& typeId) {
    // For fallback sprims, create objects with an empty scene path.
    // They'll use default values and won't be updated by a scene delegate.
    if (typeId == HdPrimTypeTokens->camera) {
        return new HdCamera(SdfPath::EmptyPath());
    }/* else if (typeId == HdPrimTypeTokens->rectLight ||
        typeId == HdPrimTypeTokens->sphereLight ||
        typeId == HdPrimTypeTokens->cylinderLight ||
        typeId == HdPrimTypeTokens->diskLight) {
        return new HdRprLight(SdfPath::EmptyPath(), typeId);
    } else if (typeId == HdPrimTypeTokens->material) {
        return new HdRprMaterial(SdfPath::EmptyPath());
    }*/

    TF_CODING_ERROR("Unknown Sprim Type %s", typeId.GetText());
    return nullptr;
}

void HdRprIpcDelegate::DestroySprim(HdSprim* sPrim) {
    delete sPrim;
}

HdBprim* HdRprIpcDelegate::CreateBprim(TfToken const& typeId,
                                    SdfPath const& bprimId) {
    if (typeId == HdPrimTypeTokens->renderBuffer) {
        return new HdRprRenderBuffer(bprimId);
    }

    TF_CODING_ERROR("Unknown Bprim Type %s", typeId.GetText());
    return nullptr;
}

HdBprim* HdRprIpcDelegate::CreateFallbackBprim(TfToken const& typeId) {
    return nullptr;
}

void HdRprIpcDelegate::DestroyBprim(HdBprim* bPrim) {
    delete bPrim;
}

HdAovDescriptor HdRprIpcDelegate::GetDefaultAovDescriptor(TfToken const& name) const {
    if (name == HdAovTokens->color) {
        return HdAovDescriptor(HdFormatFloat32Vec4, false, VtValue(GfVec4f(0.0f)));
    }
    return HdAovDescriptor();
}

HdRenderSettingDescriptorList HdRprIpcDelegate::GetRenderSettingDescriptors() const {
    return {};
}

VtDictionary HdRprIpcDelegate::GetRenderStats() const {
    return {};
}

bool HdRprIpcDelegate::IsPauseSupported() const {
    return true;
}

bool HdRprIpcDelegate::Pause() {
    m_renderThread.PauseRender();
    return true;
}

bool HdRprIpcDelegate::Resume() {
    m_renderThread.ResumeRender();
    return true;
}

#if PXR_VERSION >= 2005

bool HdRprIpcDelegate::IsStopSupported() const {
    return true;
}

bool HdRprIpcDelegate::Stop() {
    m_renderThread.StopRender();
    return true;
}

bool HdRprIpcDelegate::Restart() {
    m_renderParam->RestartRender();
    m_renderThread.StartRender();
    return true;
}

#endif // PXR_VERSION >= 2005

bool HdRprIpcDelegate::ProcessCommand(
    std::string const& command,
    uint8_t* payload, size_t pyaloadSize) {
    // no commands yet
    return false;
}

PXR_NAMESPACE_CLOSE_SCOPE
