
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
import DirectionalLightComponent;
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

      /*      Entity* SunEntity = Engine.GetMainScene()->CreateEntity("Sun");
            SunEntity->AddComponent<TransformComponent>();
            auto* SunComp = SunEntity->AddComponent<DirectionalLightComponent>();
            SunComp->SetIntensity(1.5f);
            SunComp->SetColor(glm::vec3(1.0f, 0.95f, 0.85f));*/

            auto SceneDataHandle = Engine.GetResourceManager()->Load<GltfSceneData>("main_sponza/NewSponza_Main_glTF_003.gltf", *Engine.GetResourceManager(), GltfImportSettings( GltfImportSettings::NormalGenerationMode::AlwaysRegenerate ));

            if (SceneDataHandle.Get())
            {
                Engine.GetMainScene()->InstantiateSceneFromData(*(SceneDataHandle.Get()), *Engine.GetResourceManager());
            }

            /*auto SceneLightsDataHandle = Engine.GetResourceManager()->Load<GltfSceneData>("pkg_d_10k_candles/NewSponza_4_Combined_glTF.gltf", *Engine.GetResourceManager());

            if (SceneLightsDataHandle.Get())
            {
                Engine.GetMainScene()->InstantiateSceneFromData(*(SceneLightsDataHandle.Get()), *Engine.GetResourceManager());
            }*/
        });

    Engine.Run();

    return 0;
}