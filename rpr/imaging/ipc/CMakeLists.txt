set(IPCSERVER_SRC_DIR ${RPR_VIEWER_LOCATION}/RPRViewer/src/ipc)

add_library(ipc SHARED
    ${IPCSERVER_SRC_DIR}/server.h
    ${IPCSERVER_SRC_DIR}/server.cpp
    ${IPCSERVER_SRC_DIR}/tokens.h
    ${IPCSERVER_SRC_DIR}/tokens.cpp
    ${IPCSERVER_SRC_DIR}/common.h
    ${IPCSERVER_SRC_DIR}/debugCodes.h
    ${IPCSERVER_SRC_DIR}/debugCodes.cpp
    ${IPCSERVER_SRC_DIR}/zmqContext.h
    ${IPCSERVER_SRC_DIR}/zmqContext.cpp)

target_link_libraries(ipc PUBLIC
    arch
    tf
    sdf
    usd
    cppzmq)

target_include_directories(ipc PUBLIC ${IPCSERVER_SRC_DIR})
target_compile_definitions(ipc PRIVATE IPC_EXPORTS)

install(TARGETS ipc)
