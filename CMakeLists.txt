cmake_minimum_required(VERSION 3.20)
project(EngineSO VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /experimental:module /std:c++latest")
endif()

#Find Vulkan
find_package(Vulkan REQUIRED)

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

# Define for IntelliSense (Visual Studio specific)
if(MSVC)
    target_compile_definitions(EngineSO PRIVATE 
        $<$<CONFIG:Debug>:VK_USE_PLATFORM_WIN32_KHR>
    )
endif()

# Link the Vulkan library to the target
target_link_libraries(EngineSO PRIVATE Vulkan::Vulkan)

# Include the Vulkan headers
target_include_directories(EngineSO PRIVATE ${Vulkan_INCLUDE_DIRS})

# Set the output directory for the executable
set_target_properties(EngineSO PROPERTIES
    CXX_MODULE_DIR "${CMAKE_BINARY_DIR}/modules"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Print what files were found (debugging)
message(STATUS "Found source files: ${ENGINE_SOURCES}")