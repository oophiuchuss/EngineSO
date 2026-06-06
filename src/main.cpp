
#include <iostream>
#include <vulkan/vulkan_raii.hpp>
#include <glm/glm.hpp>

import VulkanEngine;
import HotReloadResourceManager;
import Texture;

import Shader;
import Entity;
import AsyncResourceLoader;
import MeshComponent;
import TransformComponent;
import CameraComponent;
import MeshData;
import Material;
import FlyCameraControllerComponent;

int main() {
    VulkanEngine Engine;

    auto* MainCamera = Engine.GetMainScene()->CreateCameraEntity("MainCamera");
    MainCamera->AddComponent<FlyCameraControllerComponent>(Engine.GetEventSystem());

	Engine.GetMainScene()->SetActiveCameraEntity(MainCamera);


    ResourceHandle<Material> NewMaterial = Engine.GetResourceManager()->Load<Material>("basic_material");

    // Triangle mesh (procedural)
    ResourceHandle<MeshData> NewMeshData = Engine.GetResourceManager()->Load<MeshData>("Triangle");
    
	Entity* TriangleEntity = Engine.GetMainScene()->CreateMeshEntity("TriangleEntity", NewMeshData, NewMaterial);
    
	TriangleEntity->GetComponent<TransformComponent>()->SetPosition(glm::vec3(0.0f, 0.0f, -20.0f));



    Engine.Run();


    HotReloadResourceManager resourceManager;
		
    //auto TextureResource = resourceManager.Load<Texture>("example_texture");
	//auto MeshResource = resourceManager.Load<Mesh>("example_mesh");
	//auto ShaderResource = resourceManager.Load<Shader>("example_shader", vk::ShaderStageFlagBits::eVertex);
	//auto FragmentResource = resourceManager.Load<Shader>("example_shader", vk::ShaderStageFlagBits::eFragment);
        
/*	if (TextureResource.IsValid() && MeshResource.IsValid() && ShaderResource.IsValid() && FragmentResource.IsValid())
    {
        Material material(vertexShader, fragmentShader);
        
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
		});*/

    return 0;
}