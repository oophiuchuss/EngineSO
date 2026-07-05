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
import GPUProfiler;

void Rendergraph::Compile()
{
    // Physical Resource Allocation and Creation (transforms to actual GPU resource)
    for (auto& [CurName, CurResource] : Resources)
    {
        if (ExternalImages.count(CurName) > 0)
        {
            continue;   // external image, don't create GPU resources
        }

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

    if (bIsPassesDirty)
    {
        SortPasses();
        bIsPassesDirty = false;
    }
}

void Rendergraph::Reset()
{
	// RAII -based resource cleanup (automatic when objects go out of scope)
	Resources.clear();
	Passes.clear();
	SortedPasses.clear();
	CurrentLayouts.clear();
    ExternalImages.clear();
    FrameExternalViews.clear();
    bIsPassesDirty = true;
}

void Rendergraph::Execute(
    vk::raii::CommandBuffer& CommandBuffer, 
    vk::Queue Queue, 
    FrameData& CurrentFrameData,
    GPUProfiler* Profiler/* = nullptr*/,
    uint32_t FrameIndex /*= 0*/)
{
    if (bIsPassesDirty)
    {
        SortPasses();
        bIsPassesDirty = false;
    }

    // Reset current layout for external resources
    for (const auto& [Name, Res] : Resources)
    {
        if (ExternalImages.count(Name))
        {
            CurrentLayouts[Name] = Res.InitialLayout;
        }
    }

    // Pass execution with dependency-sage order
    for (const auto& CurPass: SortedPasses)
    {
        // Transition input resources to shader-readable layouts
        for (const ResourceUsage& Usage : CurPass->GetInputs())
        {
            Resource& CurResource = Resources[Usage.Name];
            TransitionResourceImageLayout(CommandBuffer, &CurResource, CurrentLayouts[Usage.Name], Usage.RequiredLayout);
        }

        // Transition output resources to correct layouts based on the usage
        for (const ResourceUsage& Usage : CurPass->GetOutputs())
        {
            Resource& CurResource = Resources[Usage.Name];
            TransitionResourceImageLayout(CommandBuffer, &CurResource, CurrentLayouts[Usage.Name], Usage.RequiredLayout);
        }

        // Pass execution of actual rendering logic
        if (Profiler)
        {
            auto ProfileScope = Profiler->BeginScope(CommandBuffer, FrameIndex, CurPass->GetName());
            CurPass->Execute(CommandBuffer, *this, CurrentFrameData);
        }
        else
        {
            CurPass->Execute(CommandBuffer, *this, CurrentFrameData);
        }
    }

    // Final layout transition
    // Transition output resources to final reuired layouts 

    for (auto& [CurOutput, CurResource] : Resources)
    {
        vk::ImageLayout TargetLayout = CurResource.FinalLayout; // Layout required at frame end

        TransitionResourceImageLayout(CommandBuffer, &CurResource, CurrentLayouts[CurOutput], TargetLayout);
    }

    ExternalImages.clear();
    FrameExternalViews.clear();

    // Note: Removed per-pass submissions and semaphores because:
    // 1. Submitting one command buffer is more efficient (less driver overhead)
    // 2. Command buffers execute sequentially, so passes naturally run in order
    // 3. Barriers inside the command buffer handle all synchronization
}

void Rendergraph::TransitionResourceImageLayout(
    vk::raii::CommandBuffer& CommandBuffer,
    Resource* ResourcePtr,
    vk::ImageLayout OldLayout,
    vk::ImageLayout NewLayout)
{
    if (ResourcePtr == nullptr)
    {
        return;
    }

    if (CurrentLayouts[ResourcePtr->Name] == NewLayout)
    {
        return;
    }

    // Resolve the actual VkImage (external or internal)
    vk::Image Image;
    auto extIt = ExternalImages.find(ResourcePtr->Name);
    if (extIt != ExternalImages.end())
    {
        Image = extIt->second;
    }
    else
    {
        Image = *ResourcePtr->Image;
    }

    VulkanUtils::TransitionImageLayout(
        CommandBuffer,
        Image,
        ResourcePtr->Aspect,
        CurrentLayouts[ResourcePtr->Name],
        NewLayout);

    CurrentLayouts[ResourcePtr->Name] = NewLayout;
}

void Rendergraph::AddResource(const std::string& Name, vk::Format Format, vk::Extent2D Extent, vk::ImageUsageFlags Usage, vk::ImageAspectFlags Aspect, vk::ImageLayout InitLayout, vk::ImageLayout FinalLayout)
{
    Resources.try_emplace(Name, Name, Format, Extent, Usage, Aspect, InitLayout, FinalLayout);
}

void Rendergraph::ImportExternalImage(
    const std::string& Name,
    vk::Format Format,
    vk::Extent2D Extent,
    vk::ImageUsageFlags Usage,
    vk::ImageAspectFlags Aspect,
    vk::ImageLayout InitialLayout,
    vk::ImageLayout FinalLayout)
{
    // Create a metadata entry in Resources (no GPU allocation)
    Resource R;
    R.Name = Name;
    R.Format = Format;
    R.Extent = Extent;
    R.Usage = Usage;
    R.Aspect = Aspect;
    R.InitialLayout = InitialLayout;
    R.FinalLayout = FinalLayout;
    R.Image = nullptr;      // graph never owns this image
    R.View = nullptr;      // view provided per‑frame later

    ExternalImages[Name] = VK_NULL_HANDLE;

    Resources[Name] = std::move(R);

    // Track the initial layout
    CurrentLayouts[Name] = InitialLayout;
}

void Rendergraph::SetFrameExternalImage(const std::string& Name, vk::Image Image, vk::ImageView View)
{
    ExternalImages[Name] = Image;
    FrameExternalViews[Name] = View;
}

vk::ImageView Rendergraph::GetResourceView(const std::string& Name) const
{
    // Per‑frame temporary view
    auto frameIt = FrameExternalViews.find(Name);
    if (frameIt != FrameExternalViews.end())
    {
        return frameIt->second;
    }

    // Graph‑owned resource view
    auto resIt = Resources.find(Name);
    if (resIt != Resources.end())
    {
        return resIt->second.View;
    }

    return nullptr;
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

    std::unordered_map<std::string, std::vector<std::string>> ResourceWriters;

    // Analyze each pass to determine data flow relationships
    for (const auto& [CurName, CurPass] : Passes)
    {
        for (const ResourceUsage& Output : CurPass->GetOutputs())
        {
            ResourceWriters[Output.Name].push_back(CurName);
        }
    }

    for (const auto& [CurName, CurPass] : Passes)
    {
        for (const ResourceUsage& Input : CurPass->GetInputs())
        {
            auto It = ResourceWriters.find(Input.Name);
            if (It != ResourceWriters.end())
            {
                // A pass that both reads and writes the same resource must depend on every other writer of that resource
                for (const std::string& Writer : It->second)
                {
                    if (Writer != CurName)
                    {
                        Dependencies[CurName].push_back(Writer);
                        Dependents[Writer].push_back(CurName);
                    }
                }
            }
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
