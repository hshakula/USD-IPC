set(USD_LIBRARIES
    vt
    arch
    tf
    gf
    sdf
    usd
    usdGeom
    usdShade)

find_package(pxr REQUIRED)
find_package(PythonInterp 3.0 REQUIRED)
find_package(PythonLibs 3.0 REQUIRED)
find_package(ZeroMQ REQUIRED)
find_package(cppzmq REQUIRED)

add_definitions(-DNOMINMAX)

add_definitions(-DBOOST_ALL_DYN_LINK)
link_directories(${pxr_DIR}/lib)

add_executable(plugin
    main.cpp
    dcc.h
    dcc.cpp
    plugin.h
    plugin.cpp
    iprPort.h
    iprPort.cpp
    debugCodes.h
    debugCodes.cpp)

target_link_libraries(plugin PRIVATE
    ${USD_LIBRARIES}
    libzmq-static
    cppzmq)

target_include_directories(plugin PRIVATE
    ${USD_INCLUDE_DIR})

get_target_property(plugin_SOURCES plugin SOURCES)
foreach(FILE ${plugin_SOURCES}) 
    get_filename_component(ABS_FILE ${FILE} ABSOLUTE)
    file(RELATIVE_PATH relPath ${CMAKE_CURRENT_SOURCE_DIR} ${ABS_FILE})

    string(FIND "${relPath}" ".." out)
    if("${out}" EQUAL 0)
        source_group("external" FILES "${FILE}")
    else()
        get_filename_component(PARENT_DIR "${FILE}" DIRECTORY)
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" GROUP "${PARENT_DIR}")
        string(REPLACE "/" "\\" GROUP "${GROUP}")

        source_group("${GROUP}" FILES "${FILE}")
    endif()
endforeach()

install(TARGETS plugin)
