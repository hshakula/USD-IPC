set(PXR_PREFIX rpr/imaging)
set(PXR_PACKAGE ipc)

set(USD_LIBRARIES
    arch
    tf
    sdf
    usd)

pxr_library(ipc
    DISABLE_PRECOMPILED_HEADERS

    PUBLIC_CLASSES
        server
        client
        tokens
        debugCodes

    PUBLIC_HEADERS
        api.h

    PRIVATE_CLASSES
        zmqContext

    PRIVATE_HEADERS
        common.h
        boostIncludePath.h

    PYTHON_CPPFILES
        moduleDeps.cpp

    PYMODULE_CPPFILES
        module.cpp
        wrapClient.cpp

    PYMODULE_FILES
        __init__.py

    LIBRARIES
        ${USD_LIBRARIES}
        cppzmq)

if(HoudiniUSD_FOUND)
    target_compile_definitions(ipc PUBLIC -DBOOST_NS=hboost)
endif()
