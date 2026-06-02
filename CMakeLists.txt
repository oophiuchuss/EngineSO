cmake_minimum_required(VERSION 3.20)
message(STATUS "CMAKE_TOOLCHAIN_FILE = ${CMAKE_TOOLCHAIN_FILE}")
project(EngineSO VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /experimental:module /std:c++latest")
endif()

#Find Vulkan
find_package(Vulkan REQUIRED)

#Find GLFW
find_package(glfw3 CONFIG REQUIRED)

#Find GLM
#find_path(GLM_INCLUDE_DIRS "glm/glm.hpp")

# Glob all source files in the src directory
file(GLOB_RECURSE ENGINE_SOURCES 
    CONFIGURE_DEPENDS
    "src/*.cpp"
    "src/*.ixx"
    "src/*.h"
    "src/*.hpp"
)

# Exclude build artifacts files from the sources
list(FILTER ENGINE_SOURCES EXCLUDE REGEX ".*\\.pch$")
list(FILTER ENGINE_SOURCES EXCLUDE REGEX ".*\\.ifc$")

# Add the executable target
add_executable(EngineSO ${ENGINE_SOURCES})

# Organize source files in the IDE
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" 
    PREFIX "Source Files" 
    FILES ${ENGINE_SOURCES}
)

# Define a preprocessor macro to enable dynamic loading of Vulkan functions
target_compile_definitions(EngineSO PRIVATE 
    VK_USE_PLATFORM_WIN32_KHR
)

# Define preprocessor macros for GLM configuration
target_compile_definitions(EngineSO PRIVATE 
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_FORCE_RIGHT_HANDED
)

# Define for IntelliSense (Visual Studio specific)
if(MSVC)
    target_compile_definitions(EngineSO PRIVATE 
        $<$<CONFIG:Debug>:VK_USE_PLATFORM_WIN32_KHR>
    )
endif()

# Link the Vulkan library to the target
target_link_libraries(EngineSO PRIVATE Vulkan::Vulkan)
target_link_libraries(EngineSO PRIVATE glfw)

# Include the Vulkan headers
target_include_directories(EngineSO PRIVATE ${Vulkan_INCLUDE_DIRS})

# Include the GLM headers
#target_include_directories(EngineSO PRIVATE ${GLM_INCLUDE_DIRS})

# Find glslc
find_program(GLSLC glslc REQUIRED)

# Define shader source directory and output directory
set(SHADER_SRC_DIR "${CMAKE_SOURCE_DIR}/assets/shaders")
set(SHADER_OUT_DIR "${CMAKE_BINARY_DIR}/shaders")

# Create the output directory for compiled shaders
file(MAKE_DIRECTORY ${SHADER_OUT_DIR})

# Discover all shader source files
file(GLOB_RECURSE ALL_SHADER_SOURCES CONFIGURE_DEPENDS
    "${SHADER_SRC_DIR}/*.vert"
    "${SHADER_SRC_DIR}/*.frag"
    "${SHADER_SRC_DIR}/*.comp"           # future compute shaders
)

# Initialize a list to hold the paths of all compiled SPIR-V files
set(ALL_SPV_FILES "")

# Compile all sahders
foreach(SHADER_SRC ${ALL_SHADER_SOURCES})
    file(RELATIVE_PATH REL_SRC "${SHADER_SRC_DIR}" "${SHADER_SRC}")

    string(REGEX REPLACE "\\.(vert|frag|comp)$" ".\\1.spv" SPV_FILE "${REL_SRC}")
    set(SPV_PATH "${SHADER_OUT_DIR}/${SPV_FILE}")

    # Extract the shader stage from the file extension
    get_filename_component(SHADER_EXT "${SHADER_SRC}" LAST_EXT)
    string(SUBSTRING "${SHADER_EXT}" 1 -1 SHADER_STAGE) # Remove the dot from the extension

    # Function to compile shaders using glslc
    add_custom_command(
        OUTPUT "${SPV_PATH}"
        COMMAND ${GLSLC} -fshader-stage=${SHADER_STAGE} -o "${SPV_PATH}" "${SHADER_SRC}"
        DEPENDS "${SHADER_SRC}"
        COMMENT "Compiling ${REL_SRC} shader to: ${SPV_PATH}"
        VERBATIM
    )

    # Add to the list of all SPIR-V files
    list(APPEND ALL_SPV_FILES "${SPV_PATH}")
endforeach()

# Create a custom target that builds all shaders
add_custom_target(Shaders ALL DEPENDS ${ALL_SPV_FILES})
add_dependencies(EngineSO Shaders)

add_custom_command(TARGET EngineSO POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:EngineSO>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ALL_SPV_FILES} "$<TARGET_FILE_DIR:EngineSO>/shaders"
    COMMENT "Copying shader binaries to runtime directory"
)

add_custom_target(ShaderSources SOURCES ${ALL_SHADER_SOURCES})

# Set the output directory for the executable
set_target_properties(EngineSO PROPERTIES
    CXX_MODULE_DIR "${CMAKE_BINARY_DIR}/modules"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Print what files were found (debugging)
message(STATUS "Found source files: ${ENGINE_SOURCES}")