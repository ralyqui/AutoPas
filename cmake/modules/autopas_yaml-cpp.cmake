# first try: check if we find any installed version
find_package(yaml-cpp QUIET)
if (yaml-cpp_FOUND AND "${yaml-cpp_VERSION}" VERSION_GREATER_EQUAL 0.6.3)
    message(STATUS "yaml-cpp - using installed system version ${yaml-cpp_VERSION}")
else ()
    # system version not found -> install bundled version
    message(STATUS "yaml-cpp - not found or version older than 0.6.3")
    message(
        STATUS
            "yaml-cpp - if you want to use your version point the cmake variable yaml-cpp_DIR to the directory containing  yaml-cpp-config.cmake in order to pass hints to find_package"
    )
    message(STATUS "yaml-cpp - using bundled version 0.6.3 (commit 72f699f)")

    # Enable ExternalProject CMake module
    include(ExternalProject)

    # Extract yaml-cpp
    ExternalProject_Add(
        yaml-cpp_external
        URL
            # yaml-cpp-master:
            # https://github.com/jbeder/yaml-cpp/archive/master.zip
            # commit 72f699f:
            ${CMAKE_SOURCE_DIR}/libs/yaml-cpp-master.zip
        URL_HASH MD5=6186f7ea92b907e9128bc74c96c1f791
        # needed to compile with ninja
        BUILD_BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/yaml-cpp/src/yaml-cpp-build/libyaml-cpp.a
        PREFIX ${CMAKE_CURRENT_BINARY_DIR}/yaml-cpp
        # Disable install steps
        INSTALL_COMMAND
            ""
            # Disable everything we don't need and set build type to release. Also disable warnings.
        CMAKE_ARGS
            -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
            -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
            -DCMAKE_BUILD_TYPE=RELEASE
            -DBUILD_TESTING=OFF
            -DYAML_CPP_BUILD_TESTS=OFF
            -DYAML_CPP_BUILD_CONTRIB=OFF
            -DYAML_CPP_BUILD_TOOLS=OFF
            -DCMAKE_CXX_FLAGS=-w
    )

    # Get GTest source and binary directories from CMake project
    ExternalProject_Get_Property(yaml-cpp_external install_dir binary_dir)

    add_library(
        yaml-cpp
        STATIC
        IMPORTED
        GLOBAL
    )

    add_dependencies(yaml-cpp yaml-cpp_external)

    file(MAKE_DIRECTORY "${install_dir}/src/yaml-cpp_external/include")

    # Set libgtest properties
    set_target_properties(
        yaml-cpp
        PROPERTIES
            "IMPORTED_LOCATION"
            "${binary_dir}/libyaml-cpp.a"
            "INTERFACE_INCLUDE_DIRECTORIES"
            "${install_dir}/src/yaml-cpp_external/include"
    )
endif ()
