import MeshComponent;
#include <iostream>
#include "vulkan/vulkan.hpp"

import Engine;

import ResourceManager;
import Texture;
import Mesh;
import Shader;
import Entity;



int main() {
    try
    {
        VulkanEngine engine;
        engine.run();
        

		//ResourceManager resourceManager;
		//
        //auto TextureResource = resourceManager.Load<Texture>("example_texture");
		//auto MeshResource = resourceManager.Load<Mesh>("example_mesh");
		//auto ShaderResource = resourceManager.Load<Shader>("example_shader", vk::ShaderStageFlagBits::eVertex);
		//auto FragmentResource = resourceManager.Load<Shader>("example_shader", vk::ShaderStageFlagBits::eFragment);
        //
		//if (TextureResource.IsValid() && MeshResource.IsValid() && ShaderResource.IsValid() && FragmentResource.IsValid())
        //{
        //    Material material/*(vertexShader, fragmentShader)*/;
        //
        //    // Set texture in material
        //    //material.SetTexture("diffuse", texture);
        //
		//	Entity entity("ExampleEntity");
        //
        //    auto meshComponent = entity.AddComponent<MeshComponent>(MeshResource.Get(), &material);
        //}
        //
        //resourceManager.Release(TextureResource.GetResourceID());

        std::getchar();
    }
    catch (const std::exception& err)
    {
        std::cerr << "std::exception: " << err.what() << std::endl;
        return 1;
    }
    return 0;
}