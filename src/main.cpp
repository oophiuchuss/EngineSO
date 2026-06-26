
#include <iostream>
#include <vulkan/vulkan_raii.hpp>
#include <glm/glm.hpp>

import VulkanEngine;
import HotReloadResourceManager;

import Shader;
import Entity;
import MeshComponent;
import TransformComponent;
import CameraComponent;
import LightComponent;
import MeshData;
import Material;
import FlyCameraControllerComponent;
import ResourceHandle;
import GltfSceneData;

int main() {
    VulkanEngine Engine;

    Engine.GetTaskScheduler()->RunOnMainThread([&]()
        {
            auto* MainCamera = Engine.GetMainScene()->CreateCameraEntity("MainCamera");
            MainCamera->AddComponent<FlyCameraControllerComponent>(Engine.GetEventSystem());

            Engine.GetMainScene()->SetActiveCameraEntity(MainCamera);

            Entity* SunEntity = Engine.GetMainScene()->CreateEntity("Sun");
            SunEntity->AddComponent<TransformComponent>();
            auto* SunComp = SunEntity->AddComponent<LightComponent>();
            SunComp->SetDirection(glm::normalize(glm::vec3(0.3f, -1.0f, 0.5f)));
            SunComp->SetIntensity(1.5f);
            SunComp->SetColor(glm::vec3(1.0f, 0.95f, 0.85f));


            auto SceneDataHandle = Engine.GetResourceManager()->Load<GltfSceneData>("main_sponza/NewSponza_Main_glTF_003.gltf", *Engine.GetResourceManager());

            if (SceneDataHandle.Get())
            {
                Engine.GetMainScene()->InstantiateSceneFromData(*(SceneDataHandle.Get()), *Engine.GetResourceManager());
            }
        });

    Engine.Run();

    return 0;
}