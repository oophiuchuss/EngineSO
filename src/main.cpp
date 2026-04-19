
#include <iostream>
#include "vulkan/vulkan_raii.hpp"

import Engine;
import HotReloadResourceManager;
import Texture;
import Mesh;
import Shader;
import Entity;
import AsyncResourceLoader;
import MeshComponent;

int main() {
    try
    {
        VulkanEngine engine;
        engine.run();


        HotReloadResourceManager resourceManager;
		
        auto TextureResource = resourceManager.Load<Texture>("example_texture");
		auto MeshResource = resourceManager.Load<Mesh>("example_mesh");
		auto ShaderResource = resourceManager.Load<Shader>("example_shader", vk::ShaderStageFlagBits::eVertex);
		auto FragmentResource = resourceManager.Load<Shader>("example_shader", vk::ShaderStageFlagBits::eFragment);
        
		if (TextureResource.IsValid() && MeshResource.IsValid() && ShaderResource.IsValid() && FragmentResource.IsValid())
        {
            Material material/*(vertexShader, fragmentShader)*/;
        
            // Set texture in material
            //material.SetTexture("diffuse", texture);
        
			Entity entity("ExampleEntity");
        
            auto meshComponent = entity.AddComponent<MeshComponent>(MeshResource.Get(), &material);
        }
        
        //resourceManager.Release(TextureResource.GetResourceID());

		AsyncResourceLoader asyncLoader(&resourceManager);

        asyncLoader.LoadAsync<Texture>("async_texture", [](ResourceHandle<Texture> LoadedTexture) {
            if (LoadedTexture.IsValid())
            {
                std::cout << "Asynchronously loaded texture: " << LoadedTexture.GetResourceID() << std::endl;
            }
            else
            {
                std::cout << "Failed to load texture asynchronously." << std::endl;
            }
			});



        std::getchar();
    }
    catch (const std::exception& err)
    {
        std::cerr << "std::exception: " << err.what() << std::endl;
        return 1;
    }
    return 0;
}