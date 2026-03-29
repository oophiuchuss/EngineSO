#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <iostream>

import Engine;

int main() {
    try
    {
        VulkanEngine engine;
        engine.run();
        
        std::getchar();
    }
    catch (const std::exception& err)
    {
        std::cerr << "std::exception: " << err.what() << std::endl;
        return 1;
    }
    return 0;
}