module;

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <memory>
#include <vulkan/vulkan_raii.hpp>

module Rendergraph;

import RenderPassBase;
import VulkanUtils;
import FrameData;
import VulkanUtils;

void Rendergraph::Compile()
{
    // Physical Resource Allocation and Creation (transforms to actual GPU resource)
    for (auto& [CurName, CurResource] : Resources)
    {
        // Configure Image creation parameters
        vk::ImageCreateInfo ImageInfo;
        ImageInfo.setImageType(vk::ImageType::e2D)                                  // 2d texture/render target
            .setFormat(CurResource.Format)                                          // Pixel format from description
            .setExtent({ CurResource.Extent.width, CurResource.Extent.height, 1 })  // Dimensions
            .setMipLevels(1)                                                        // Single mip level for simplicity
            .setArrayLayers(1)                                                      // Single layer (not array texture)
            .setSamples(vk::SampleCountFlagBits::e1)                                // No multisampling
            .setTiling(vk::ImageTiling::eOptimal)                                   // GPU-optimal memory layout
            .setUsage(CurResource.Usage)                                            // Usage flags from registration
            .setSharingMode(vk::SharingMode::eExclusive)                            // Single queue family access
            .setInitialLayout(CurResource.InitialLayout);                           // Init layout
    
        CurResource.Image = Device.createImage(ImageInfo);

        // Alocate backing memory for the image
        vk::MemoryRequirements MemRequirements = CurResource.Image.getMemoryRequirements();

        vk::MemoryAllocateInfo AllocInfo;
        AllocInfo.setAllocationSize(MemRequirements.size) // Required memory size
            .setMemoryTypeIndex(VulkanUtils::FindMemoryType(PhysicalDevice, MemRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal)); // GPU-local memory

        CurResource.Memory = Device.allocateMemory(AllocInfo);  // Allocate GPU memory
        CurResource.Image.bindMemory(*CurResource.Memory, 0);   // Bind memory to image TODO: cannot do offsets for better memory management?

        // Create image view for shader access
        vk::ImageViewCreateInfo ViewInfo;
        ViewInfo.setImage(*CurResource.Image)                                       // Reference created image
            .setViewType(vk::ImageViewType::e2D)                                    // 2d View type
            .setFormat(CurResource.Format)                                          // Match image format
            .setSubresourceRange({ CurResource.Aspect, 0, 1, 0, 1 });               // Full Image access

        CurResource.View = Device.createImageView(ViewInfo);

        CurrentLayouts[CurName] = CurResource.InitialLayout;    // Initialize Layouts of Resources
    }
}

void Rendergraph::Reset()
{
	// RAII -based resource cleanup (automatic when objects go out of scope)
	Resources.clear();
	Passes.clear();
	SortedPasses.clear();
	CurrentLayouts.clear();
    bIsPassesDirty = true;
}

void Rendergraph::Execute(vk::raii::CommandBuffer& CommandBuffer, vk::Queue Queue, FrameData& CurrentFrameData)
{
    if (bIsPassesDirty)
    {
        SortPasses();
        bIsPassesDirty = false;
    }

    // Pass execution with dependency-sage order
    for (const auto& CurPass: SortedPasses)
    {
        // Transition input resources to shader-readable layouts
        for (const std::string& CurInput : CurPass->GetInputs())
        {
            Resource& CurResource = Resources[CurInput];

            vk::ImageLayout TargetLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

			TransitionResourceImageLayout(CommandBuffer, &CurResource, CurrentLayouts[CurInput], TargetLayout);
        }

        // Transition output resources to correct layouts based on the usage
        for (const std::string& CurOutput : CurPass->GetOutputs())
        {
            Resource& CurResource = Resources[CurOutput];

            vk::ImageLayout TargetLayout;

            if (CurResource.Usage & vk::ImageUsageFlagBits::eColorAttachment)
            {
                TargetLayout = vk::ImageLayout::eColorAttachmentOptimal; // Color Render target
            }
            else if(CurResource.Usage & vk::ImageUsageFlagBits::eDepthStencilAttachment)
            {
                TargetLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal; // Depth Buffer
            }
            else
            {
                TargetLayout = vk::ImageLayout::eGeneral; // Fallback (rarely used)
            }

            TransitionResourceImageLayout(CommandBuffer, &CurResource, CurrentLayouts[CurOutput], TargetLayout);
        }

        // Pass execution of actual rendering logic
        // This calls the user-provided function that contains:
        // - vkCmdBeginRendering (with dynamic rendering)
        // - Draw calls
        // - vkCmdEndRendering
        CurPass->Execute(CommandBuffer, *this, CurrentFrameData); 
    }

    // Final layout transition
    // Transition output resources to final reuired layouts 

    for (auto& [CurOutput, CurResource] : Resources)
    {
        vk::ImageLayout TargetLayout = CurResource.FinalLayout; // Layout required at frame end

        TransitionResourceImageLayout(CommandBuffer, &CurResource, CurrentLayouts[CurOutput], TargetLayout);
    }

    // Note: Rremoved per-pass submissions and semaphores because:
    // 1. Submitting one command buffer is more efficient (less driver overhead)
    // 2. Command buffers execute sequentially, so passes naturally run in order
    // 3. Barriers inside the command buffer handle all synchronization
}

void Rendergraph::TransitionResourceImageLayout(vk::raii::CommandBuffer& CommandBuffer, Resource* ResourcePtr, vk::ImageLayout OldLayout, vk::ImageLayout NewLayout)
{
    if (ResourcePtr == nullptr)
    {
		return;
    }

    if (CurrentLayouts[ResourcePtr->Name] != NewLayout)
    {
        // Transition image to target layout
        VulkanUtils::TransitionImageLayout(CommandBuffer, *ResourcePtr->Image, ResourcePtr->Aspect, CurrentLayouts[ResourcePtr->Name], NewLayout);

        // Update this resource current layout 
        CurrentLayouts[ResourcePtr->Name] = NewLayout;
    }
}

void Rendergraph::AddResource(const std::string& Name, vk::Format Format, vk::Extent2D Extent, vk::ImageUsageFlags Usage, vk::ImageAspectFlags Aspect, vk::ImageLayout InitLayout, vk::ImageLayout FinalLayout)
{
    Resources.try_emplace(Name, Name, Format, Extent, Usage, Aspect, InitLayout, FinalLayout);
}

void Rendergraph::RemoveRenderPass(const std::string& Name)
{
    auto it = Passes.find(Name);
    if (it != Passes.end())
    {
        Passes.erase(it);
        bIsPassesDirty = true;
    }
}

void Rendergraph::SortPasses()
{
    SortedPasses.clear();

    // Build dependency graph: For each pass, list of passes it depends on (writers of its inputs)
    std::unordered_map<std::string, std::vector<std::string>> Dependencies;
    std::unordered_map<std::string, std::vector<std::string>> Dependents;   // optional, not used in sort

    std::unordered_map<std::string, std::string> ResourceWriters;

    // Analyze each pass to determine data flow relationships
    for (const auto& [CurName, CurRenderPass] : Passes)
    {
        for(const std::string& Input : CurRenderPass->GetInputs())
        {
            auto it = ResourceWriters.find(Input);
            if (it != ResourceWriters.end())
            {
                Dependencies[CurName].push_back(it->second);    // current depends on writer
                Dependents[it->second].push_back(CurName);
            }
        }
        
        for(const std::string& Output: CurRenderPass->GetOutputs())
        {
            ResourceWriters[Output] = CurName;
        }
    }

    // Topological Sort for optimal execution order (depth-first algorithm)
    std::unordered_set<std::string> Visited;    // Tracks complited nodes
    std::unordered_set<std::string> Visiting;   // Tracks current recursion path

    for (const auto& [CurName, CurPass] : Passes)
    {
        if (Visited.find(CurName) == Visited.end())
        {
            TopologicalSort(CurName, Dependencies, Visited, Visiting);
        }
    }

}

void Rendergraph::TopologicalSort(const std::string& Name, const std::unordered_map<std::string, std::vector<std::string>>& Dependencies, std::unordered_set<std::string>& Visited, std::unordered_set<std::string>& Visiting)
{
    if (Visiting.count(Name))
    {
        throw std::runtime_error("Cycle detected in rendergraph");
    }

    if (Visited.count(Name))
    {
        return; // Was already processed
    }

    Visiting.insert(Name);

    const auto& CurDeps = Dependencies.find(Name);

    if (CurDeps != Dependencies.end())
    {
        for (const auto& Dep : CurDeps->second)
        {
            if (Visited.find(Dep) == Visited.end())
            {
                TopologicalSort(Dep, Dependencies, Visited, Visiting);
            }
        }
    }

    Visiting.erase(Name);
    Visited.insert(Name);

    SortedPasses.push_back(Passes[Name].get());
}
