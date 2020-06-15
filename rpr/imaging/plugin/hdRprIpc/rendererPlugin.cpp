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

#include "rendererPlugin.h"
#include "renderDelegate.h"

#include "pxr/imaging/hd/rendererPluginRegistry.h"

PXR_NAMESPACE_OPEN_SCOPE

TF_REGISTRY_FUNCTION(TfType) {
    HdRendererPluginRegistry::Define<HdRprIpcPlugin>();
}

HdRenderDelegate* HdRprIpcPlugin::CreateRenderDelegate() {
    return new HdRprIpcDelegate(HdRenderSettingsMap());
}

HdRenderDelegate* HdRprIpcPlugin::CreateRenderDelegate(HdRenderSettingsMap const& settingsMap) {
    return new HdRprIpcDelegate(settingsMap);
}

void HdRprIpcPlugin::DeleteRenderDelegate(HdRenderDelegate* renderDelegate) {
    delete renderDelegate;
}

PXR_NAMESPACE_CLOSE_SCOPE
