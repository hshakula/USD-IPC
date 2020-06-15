#
# Copyright 2016 Pixar
#
# Licensed under the Apache License, Version 2.0 (the "Apache License")
# with the following modification; you may not use this file except in
# compliance with the Apache License and the following modification to it:
# Section 6. Trademarks. is deleted and replaced with:
#
# 6. Trademarks. This License does not grant permission to use the trade
#    names, trademarks, service marks, or product names of the Licensor
#    and its affiliates, except as required to comply with Section 4(c) of
#    the License and to reproduce the content of the NOTICE file.
#
# You may obtain a copy of the Apache License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the Apache License with the above modification is
# distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied. See the Apache License for the specific
# language governing permissions and limitations under the Apache License.
#

# Save the current value of BUILD_SHARED_LIBS and restore it at
# the end of this file, since some of the Find* modules invoked
# below may wind up stomping over this value.
set(build_shared_libs "${BUILD_SHARED_LIBS}")

# Core USD Package Requirements 
# ----------------------------------------------

# Try to find monolithic USD
find_package(USDMonolithic QUIET)

if(NOT USDMonolithic_FOUND)
    find_package(pxr CONFIG)

    if(NOT pxr_FOUND)
        # Try to find USD as part of Houdini.
        find_package(HoudiniUSD)

        if(HoudiniUSD_FOUND)
            message(STATUS "Configuring Houdini plugin")
        endif()
    else()
        message(STATUS "Configuring usdview plugin")
    endif()
else()
    message(STATUS "Configuring usdview plugin: monolithic USD")
endif()

if(NOT pxr_FOUND AND NOT HoudiniUSD_FOUND AND NOT USDMonolithic_FOUND)
    message(FATAL_ERROR "Required: USD install or Houdini with included USD.")
endif()

# Threads.  Save the libraries needed in PXR_THREAD_LIBS;  we may modify
# them later.  We need the threads package because some platforms require
# it when using C++ functions from #include <thread>.
# set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
# find_package(Threads REQUIRED)
# set(PXR_THREAD_LIBS "${CMAKE_THREAD_LIBS_INIT}")

if(PXR_ENABLE_PYTHON_SUPPORT)
    # --Python.
    if(HoudiniUSD_FOUND)
        set(PYTHON_EXECUTABLE ${HYTHON_EXECUTABLE})
        set(PYTHON_INCLUDE_DIR ${Houdini_Python_INCLUDE_DIR})
        # set(Python_LIBRARIES ${Houdini_Python_LIB} ${Houdini_Boostpython_LIB})
    else()
        if(PXR_USE_PYTHON_3)
            find_package(PythonInterp 3.0 REQUIRED)
            find_package(PythonLibs 3.0 REQUIRED)
        else()
            find_package(PythonInterp 2.7 REQUIRED)
            find_package(PythonLibs 2.7 REQUIRED)
        endif()
    endif()

    # find_package(Boost
    #     COMPONENTS
    #         program_options
    #     REQUIRED
    # )

    # Set up a version string for comparisons. This is available
    # as Boost_VERSION_STRING in CMake 3.14+
    # set(boost_version_string "${Boost_MAJOR_VERSION}.${Boost_MINOR_VERSION}.${Boost_SUBMINOR_VERSION}")

    # if (((${boost_version_string} VERSION_GREATER_EQUAL "1.67") AND
    #      (${boost_version_string} VERSION_LESS "1.70")) OR
    #     ((${boost_version_string} VERSION_GREATER_EQUAL "1.70") AND
    #       Boost_NO_BOOST_CMAKE))
    #     # As of boost 1.67 the boost_python component name includes the
    #     # associated Python version (e.g. python27, python36). After boost 1.70
    #     # the built-in cmake files will deal with this. If we are using boost
    #     # that does not have working cmake files, or we are using a new boost
    #     # and not using cmake's boost files, we need to do the below.
    #     #
    #     # Find the component under the versioned name and then set the generic
    #     # Boost_PYTHON_LIBRARY variable so that we don't have to duplicate this
    #     # logic in each library's CMakeLists.txt.
    #     set(python_version_nodot "${PYTHON_VERSION_MAJOR}${PYTHON_VERSION_MINOR}")
    #     find_package(Boost
    #         COMPONENTS
    #             python${python_version_nodot}
    #         REQUIRED
    #     )
    #     set(Boost_PYTHON_LIBRARY "${Boost_PYTHON${python_version_nodot}_LIBRARY}")
    # else()
    #     find_package(Boost
    #         COMPONENTS
    #             python
    #         REQUIRED
    #     )
    # endif()

    # --Jinja2
    # find_package(Jinja2)
else()
    # -- Python
    # A Python interpreter is still required for certain build options.
    if (PXR_BUILD_DOCUMENTATION OR PXR_BUILD_TESTS
        OR PXR_VALIDATE_GENERATED_CODE)

        if(PXR_USE_PYTHON_3)
            find_package(PythonInterp 3.0 REQUIRED)
        else()
            find_package(PythonInterp 2.7 REQUIRED)
        endif()
    endif()
 
    # --Boost
    # find_package(Boost
    #     COMPONENTS
    #         program_options
    #     REQUIRED
    # )
endif()

# ----------------------------------------------

set(BUILD_SHARED_LIBS "${build_shared_libs}")
