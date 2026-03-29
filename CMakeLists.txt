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

# Add the executable target
add_executable(EngineSO)

target_sources(EngineSO
    PRIVATE
        src/main.cpp
        src/engine.ixx
        src/engine.cpp
        )

# Link the Vulkan library to the target
target_link_libraries(EngineSO PRIVATE Vulkan::Vulkan)

# Include the Vulkan headers
target_include_directories(EngineSO PRIVATE ${Vulkan_INCLUDE_DIRS})

# Set the output directory for the executable
set_target_properties(EngineSO PROPERTIES
    CXX_MODULE_DIR "${CMAKE_BINARY_DIR}/modules"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
