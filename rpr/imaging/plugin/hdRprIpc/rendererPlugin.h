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

#ifndef HDRPR_RENDERER_PLUGIN_H
#define HDRPR_RENDERER_PLUGIN_H

#include "pxr/imaging/hd/rendererPlugin.h"

PXR_NAMESPACE_OPEN_SCOPE

class HdRprIpcPlugin final : public HdRendererPlugin {
public:
    HdRprIpcPlugin() = default;
    ~HdRprIpcPlugin() override = default;

    HdRprIpcPlugin(const HdRprIpcPlugin&) = delete;
    HdRprIpcPlugin& operator =(const HdRprIpcPlugin&) = delete;

    HdRenderDelegate* CreateRenderDelegate() override;

    HdRenderDelegate* CreateRenderDelegate(HdRenderSettingsMap const& settingsMap) override;

    void DeleteRenderDelegate(HdRenderDelegate* renderDelegate) override;

    bool IsSupported() const override { return true; }
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDRPR_RENDERER_PLUGIN_H
