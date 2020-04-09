set(USD_LIBRARIES
    vt
    arch
    tf
    gf
    sdf
    usd
    usdGeom
    usdShade)

set(_libPrefix ${PXR_LIB_PREFIX})
# UNIX compilers adding "lib" prefix implicitly
if(NOT WIN32)
    string(REGEX REPLACE "^lib" "" _libPrefix "${PXR_LIB_PREFIX}")
endif()

set(_prefixedUsdLibraries "")
foreach(name ${USD_LIBRARIES})
    list(APPEND _prefixedUsdLibraries "${_libPrefix}${name}")
endforeach()
set(USD_LIBRARIES "${_prefixedUsdLibraries}")

find_package(USD REQUIRED)
find_package(PythonInterp 3.0 REQUIRED)
find_package(PythonLibs 3.0 REQUIRED)
find_package(ZeroMQ REQUIRED)
find_package(cppzmq REQUIRED)
find_package(TBB REQUIRED COMPONENTS tbb)
add_definitions(${TBB_DEFINITIONS})

set(boost_components filesystem)
# Set up a version string for comparisons. This is available
# as Boost_VERSION_STRING in CMake 3.14+
set(boost_version_string "${Boost_MAJOR_VERSION}.${Boost_MINOR_VERSION}.${Boost_SUBMINOR_VERSION}")

if (((${boost_version_string} VERSION_GREATER_EQUAL "1.67") AND
     (${boost_version_string} VERSION_LESS "1.70")) OR
    ((${boost_version_string} VERSION_GREATER_EQUAL "1.70") AND
      Boost_NO_BOOST_CMAKE))
    # As of boost 1.67 the boost_python component name includes the
    # associated Python version (e.g. python27, python36). After boost 1.70
    # the built-in cmake files will deal with this. If we are using boost
    # that does not have working cmake files, or we are using a new boost
    # and not using cmake's boost files, we need to do the below.
    #
    # Find the component under the versioned name and then set the generic
    # Boost_PYTHON_LIBRARY variable so that we don't have to duplicate this
    # logic in each library's CMakeLists.txt.
    set(python_version_nodot "${PYTHON_VERSION_MAJOR}${PYTHON_VERSION_MINOR}")
    list(APPEND boost_components python${python_version_nodot})
    set(Boost_PYTHON_LIBRARY "${Boost_PYTHON${python_version_nodot}_LIBRARY}")
else()
    list(APPEND boost_components python)
endif()

set(BUILD_SHARED_LIBS ON)
find_package(Boost REQUIRED COMPONENTS ${boost_components})

add_definitions(-DNOMINMAX)

link_directories(${USD_LIBRARY_DIR})

add_executable(plugin
    main.cpp
    dcc.h
    dcc.cpp
    plugin.h
    plugin.cpp
    iprPort.h
    iprPort.cpp)

target_link_libraries(plugin PRIVATE
    ${TBB_tbb_LIBRARY}
    ${PYTHON_LIBRARIES}
    ${Boost_LIBRARIES}
    ${USD_LIBRARIES}
    libzmq-static
    cppzmq)

target_include_directories(plugin PRIVATE
    ${Boost_INCLUDE_DIRS}
    ${TBB_INCLUDE_DIRS}
    ${USD_INCLUDE_DIR}
    ${PYTHON_INCLUDE_DIRS})

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
