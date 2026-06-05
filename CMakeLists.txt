cmake_minimum_required(VERSION 3.24)

# Project setup
project(EngineSO VERSION 0.1.0 LANGUAGES CXX)

add_executable(EngineSO)

target_compile_features(EngineSO PRIVATE cxx_std_20)

# Compiler-specific options
target_compile_options(EngineSO PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/permissive->
)

# Platform-specific definitions
target_compile_definitions(EngineSO PRIVATE
    $<$<PLATFORM_ID:Windows>:VK_USE_PLATFORM_WIN32_KHR>

    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_FORCE_RIGHT_HANDED
)

# Dependencies
find_package(Vulkan REQUIRED)

find_package(glfw3 CONFIG REQUIRED)

# Link libraries (modern CMake style)
target_link_libraries(EngineSO PRIVATE
    Vulkan::Vulkan
    glfw
)

# Source files
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    src/*.cpp
    src/*.hpp
    src/*.h
    src/*.ixx
)

# Remove accidental build artifacts if they appear in source tree
list(FILTER ENGINE_SOURCES EXCLUDE REGEX ".*\\.pch$")
list(FILTER ENGINE_SOURCES EXCLUDE REGEX ".*\\.ifc$")

target_sources(EngineSO PRIVATE ${ENGINE_SOURCES})

# IDE grouping (Visual Studio, etc.)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src"
    PREFIX "Source Files"
    FILES ${ENGINE_SOURCES}
)

# C++ Modules (important note)
set_target_properties(EngineSO PROPERTIES
    CXX_MODULE_DIR "${CMAKE_BINARY_DIR}/modules"
)

# Shader compilation (Slang)
include(FetchContent)

set(SLANG_REQUIRED_VERSION "2026.10.2")

FetchContent_Declare(
    slang
    URL https://github.com/shader-slang/slang/releases/download/v${SLANG_REQUIRED_VERSION}/slang-${SLANG_REQUIRED_VERSION}-windows-x86_64.zip
)

FetchContent_MakeAvailable(slang)

file(GLOB_RECURSE SLANGC_CANDIDATES
    "${slang_SOURCE_DIR}/**/slangc.exe"
)

list(LENGTH SLANGC_CANDIDATES NUM_CANDIDATES)
if(NUM_CANDIDATES EQUAL 0)
    message(FATAL_ERROR "slangc.exe not found in Slang package")
endif()

list(GET SLANGC_CANDIDATES 0 SLANGC)

message(STATUS "Using Slang compiler: ${SLANGC}")

# Shader compilation pipeline
set(SHADER_SRC_DIR "${CMAKE_SOURCE_DIR}/assets/shaders")
set(SHADER_OUT_DIR "${CMAKE_BINARY_DIR}/shaders")

file(MAKE_DIRECTORY "${SHADER_OUT_DIR}")

file(GLOB_RECURSE ALL_SHADER_SOURCES CONFIGURE_DEPENDS
    "${SHADER_SRC_DIR}/*.slang"
)

# Filter to only non-stage files (modules)
list(FILTER SHADER_MODULES EXCLUDE REGEX "\\.(vert|frag|comp)\\.slang$")

set(ALL_SPV_FILES)


foreach(SHADER_SRC ${ALL_SHADER_SOURCES})

    file(RELATIVE_PATH REL_SRC "${SHADER_SRC_DIR}" "${SHADER_SRC}")

    # Expecting naming convention:
    # file.vert.slang / file.frag.slang / file.comp.slang
    if("${REL_SRC}" MATCHES "\\.(vert|frag|comp)\\.slang$")
        set(STAGE_SHORT "${CMAKE_MATCH_1}")
    else()
        continue()  # no warning — these are module files
    endif()


    # Map shader stage to Vulkan stage name
    if(STAGE_SHORT STREQUAL "vert")
        set(VK_STAGE vertex)
    elseif(STAGE_SHORT STREQUAL "frag")
        set(VK_STAGE fragment)
    elseif(STAGE_SHORT STREQUAL "comp")
        set(VK_STAGE compute)
    endif()

    string(REGEX REPLACE "\\.slang$" ".spv" SPV_FILE "${REL_SRC}")
    set(SPV_PATH "${SHADER_OUT_DIR}/${SPV_FILE}")

    add_custom_command(
        OUTPUT "${SPV_PATH}"

        # Ensure output directory exists
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_OUT_DIR}"

        # Compile shader
        COMMAND ${SLANGC}
            "${SHADER_SRC}"
            -entry main
            -stage ${VK_STAGE}
            -I${SHADER_SRC_DIR}
            -o "${SPV_PATH}"

        DEPENDS "${SHADER_SRC}" ${SHADER_MODULES}  # rebuild if any module changes

        COMMENT "Compiling shader: ${REL_SRC}"
        VERBATIM
    )

    list(APPEND ALL_SPV_FILES "${SPV_PATH}")

endforeach()

# Show raw shader source files in the IDE
add_custom_target(ShaderSources SOURCES ${ALL_SHADER_SOURCES})

add_custom_target(Shaders ALL DEPENDS ${ALL_SPV_FILES})

# Ensure shaders are built before the executable
add_dependencies(EngineSO Shaders)

# Copy shaders next to executable
add_custom_command(TARGET EngineSO POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:EngineSO>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${ALL_SPV_FILES}
        "$<TARGET_FILE_DIR:EngineSO>/shaders"
)

# Output directories
set_target_properties(EngineSO PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Debug output (remove later in production)
message(STATUS "Engine sources:")
foreach(SRC ${ENGINE_SOURCES})
    message(STATUS "  ${SRC}")
endforeach()