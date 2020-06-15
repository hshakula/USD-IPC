set(PXR_PREFIX rpr/imaging)
set(PXR_PACKAGE hdRprIpc)

if(USDMonolithic_FOUND)
    set(USD_LIBRARIES usd_ms)
else()
    set(USD_LIBRARIES
        trace
        tf
        vt
        gf
        hf
        hd
        usdGeom)
endif()

pxr_plugin(hdRprIpc
    DISABLE_PRECOMPILED_HEADERS

    LIBRARIES
        ${USD_LIBRARIES}
        ipc

    PRIVATE_CLASSES
        rendererPlugin
        renderDelegate
        renderThread
        renderPass
        mesh
        # material
        # light
        renderBuffer

    PRIVATE_HEADERS
        renderParam.h

    RESOURCE_FILES
        plugInfo.json
)

install(
    CODE
    "FILE(WRITE \"${CMAKE_INSTALL_PREFIX}/houdini/dso/usd_plugins/plugInfo.json\"
    \"{
    \\\"Includes\\\": [ \\\"../../../plugin/usd/\\\" ]
}\")")
