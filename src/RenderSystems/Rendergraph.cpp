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
            .setMemoryTypeIndex(FindMemoryType(MemRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal)); // GPU-local memory

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

void Rendergraph::Execute(vk::raii::CommandBuffer& CommandBuffer, vk::Queue Queue)
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
        CurPass->Execute(CommandBuffer, *this); 
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
        TransitionImageLayout(CommandBuffer, *ResourcePtr->Image, ResourcePtr->Aspect, CurrentLayouts[ResourcePtr->Name], NewLayout);

        // Update this resource current layout 
        CurrentLayouts[ResourcePtr->Name] = NewLayout;
    }
}

void Rendergraph::TransitionImageLayout(vk::raii::CommandBuffer& CommandBuffer, vk::Image Image, vk::ImageAspectFlags Aspect, vk::ImageLayout OldLayout, vk::ImageLayout NewLayout)
{
    // Configure image barrier strcture to coordinate memory access

    vk::ImageMemoryBarrier Barrier;
    Barrier.setOldLayout(OldLayout)                         // Current resource layuout
        .setNewLayout(NewLayout)                            // Final resource layuout
        .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)    // No queue family transfer
        .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .setImage(Image)                                    // Target Image
        .setSubresourceRange({ Aspect, 0, 1, 0, 1 });  // Full Image range
        
    // Initialize pipeline stage tracking for syncronization
    vk::PipelineStageFlags SrcStage;    // When previous operations must finish
    vk::PipelineStageFlags DstStage;    // When subsequent operation can start
    vk::AccessFlags SrcAccess;          // What kind of previous access we're waiting for
    vk::AccessFlags DstAccess;          // What kind of new access we're enabling


    // Undefined-to-transfer layout transition
    // Common for preparing images to data uploads
    if (OldLayout == vk::ImageLayout::eUndefined && NewLayout == vk::ImageLayout::eTransferDstOptimal)
    {
        // Memory permisions
        SrcAccess = vk::AccessFlagBits::eNone;                          // No previous access 
        DstAccess = vk::AccessFlagBits::eTransferWrite;                 // Enable transfer write operations

        // Pipeline stage syncronization
        SrcStage = vk::PipelineStageFlagBits::eTopOfPipe;               // No previous work to wait
        DstStage = vk::PipelineStageFlagBits::eTransfer;                // Transfer operations can procees
    }
    // Transfer-to-Shader layout transition
    // Common for uploaded images to shader sampling
    else if (OldLayout == vk::ImageLayout::eTransferDstOptimal && NewLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
    {
        // Memory permisions
        SrcAccess = vk::AccessFlagBits::eTransferWrite;                 // Previous tranfer writes should complete
        DstAccess = vk::AccessFlagBits::eShaderRead;                    // Enable shader read access

        // Pipeline stage syncronization
        SrcStage = vk::PipelineStageFlagBits::eTransfer;                // Tranfer operations must complete
        DstStage = vk::PipelineStageFlagBits::eFragmentShader;          // Fragment shaders can access
    }
    // Undefined-to-DepthStencilAttachmentOptimal layout transition
    // Common for resource newly created and be rendered to
    else if (OldLayout == vk::ImageLayout::eUndefined && NewLayout == vk::ImageLayout::eColorAttachmentOptimal)
    {
        // Memory permisions
        SrcAccess = vk::AccessFlagBits::eNone;                          // No previous access 
        DstAccess = vk::AccessFlagBits::eColorAttachmentWrite;          // Will write to color attachment

        // Pipeline stage syncronization
        SrcStage = vk::PipelineStageFlagBits::eTopOfPipe;               // Don't wait for anything
        DstStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;   // Wait until color attachment writes are allowed
    }
    // Transfer-to-Shader layout transition
    // Common for resource newly created and be used for depth testing
    else if (OldLayout == vk::ImageLayout::eUndefined && NewLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
    {
        // Memory permisions
        SrcAccess = vk::AccessFlagBits::eNone;                          // No previous access 
        DstAccess = vk::AccessFlagBits::eDepthStencilAttachmentWrite;   // Will write to depth stencil attachment

        // Pipeline stage syncronization
        SrcStage = vk::PipelineStageFlagBits::eTopOfPipe;               // Don't wait for anything
        DstStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;      // Wait until early fragment tests
    }
    // ColorAttachment-to-ShaderRead layout transition
    // After rendering to G-buffer, now want to sample it as texture in lighting pass
    else if (OldLayout == vk::ImageLayout::eColorAttachmentOptimal && NewLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
    {
        // Memory permisions
        SrcAccess = vk::AccessFlagBits::eColorAttachmentWrite;          // Wait for previous color writes to finish
        DstAccess = vk::AccessFlagBits::eShaderRead;                    // Allow shaders to read the texture

        // Pipeline stage syncronization
        SrcStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;   // After color writes complete
        DstStage = vk::PipelineStageFlagBits::eFragmentShader;          // Before fragment shader reads
    }
    // DepthAttachment-to-ShaderRead layout transition
    // After depth testing, want to read depth values in shader (e.g., for shadows)
    else if (OldLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal && NewLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
    {
        // Memory permisions
        SrcAccess = vk::AccessFlagBits::eDepthStencilAttachmentWrite;   // Wait for depth stencil writes to finish 
        DstAccess = vk::AccessFlagBits::eShaderRead;                    // Allow shader reads

        // Pipeline stage syncronization
        SrcStage = vk::PipelineStageFlagBits::eLateFragmentTests;       // After late fragment tests (depth writes)
        DstStage = vk::PipelineStageFlagBits::eFragmentShader;          // Before fragment shader reads
    }
    // ShaderRead-to-ColorAttachment layout transition
    // Reusing a texture that was read from as a new render target (e.g., ping-pong rendering)
    else if (OldLayout == vk::ImageLayout::eShaderReadOnlyOptimal && NewLayout == vk::ImageLayout::eColorAttachmentOptimal)
    {
        // Memory permisions
        SrcAccess = vk::AccessFlagBits::eShaderRead;                    // Wait for previoues shader reads to complete 
        DstAccess = vk::AccessFlagBits::eColorAttachmentWrite;          // Enable color writes

        // Pipeline stage syncronization
        SrcStage = vk::PipelineStageFlagBits::eFragmentShader;          // After shader reads complete
        DstStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;   // Before color writes begin
    }
    // ColorAttachment-to-TransferSrc layout transition
    // Used when a render target needs to be copied/blitted to another image (e.g., to the swapchain)
    else if (OldLayout == vk::ImageLayout::eColorAttachmentOptimal && NewLayout == vk::ImageLayout::eTransferSrcOptimal)
    {
        // Memory permissions
        SrcAccess = vk::AccessFlagBits::eColorAttachmentWrite;          // Wait for colour writes to finish
        DstAccess = vk::AccessFlagBits::eTransferRead;                  // Enable transfer reads

        // Pipeline stage synchronisation
        SrcStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;   // After all colour writes
        DstStage = vk::PipelineStageFlagBits::eTransfer;                // Before transfer operations
    }
    // TransferSrc-to-ColorAttachment layout transition
    // Happens at the start of a new frame when the previous frame left the resource as TransferSrc
    else if (OldLayout == vk::ImageLayout::eTransferSrcOptimal && NewLayout == vk::ImageLayout::eColorAttachmentOptimal)
    {
        // Memory permissions
        SrcAccess = vk::AccessFlagBits::eTransferRead;           // Wait for previous transfer reads to finish
        DstAccess = vk::AccessFlagBits::eColorAttachmentWrite;   // Enable colour writes

        // Pipeline stage synchronisation
        SrcStage = vk::PipelineStageFlagBits::eTransfer;                // After any transfer ops
        DstStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;   // Before colour attachment writes
    }
    // TODO: Add more transitions as needed:
    // - Transfer Dst -> Shader Read (for uploading textures)
    // - Shader Read -> Transfer Src (for reading back textures to CPU)
    else
    {
        // Handle unsopported transition combinations
        throw std::invalid_argument("Unsupported layout transition!");
    }

    // Apply the access masks
    Barrier.setSrcAccessMask(SrcAccess).setDstAccessMask(DstAccess);

    CommandBuffer.pipelineBarrier(
        SrcStage,                           // Wait for this operations to complete
        DstStage,                           // Before allowing these operations to begin
        vk::DependencyFlagBits::eByRegion,  // Enable region-local optimization
        {}, {}, { Barrier }                 // Apply image barrier
    );
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

uint32_t Rendergraph::FindMemoryType(const uint32_t& TypeFilter, const vk::MemoryPropertyFlags& Properties) const
{
    vk::PhysicalDeviceMemoryProperties MemProperties =  PhysicalDevice.getMemoryProperties();

    for (uint32_t i = 0; i < MemProperties.memoryTypeCount; i++)
    {
        if (TypeFilter & (1 << i))
        {
            vk::MemoryPropertyFlags RequiredPropertiesMatch = MemProperties.memoryTypes[i].propertyFlags & Properties;
        
            if (RequiredPropertiesMatch == Properties)
            {
                return i;
            }
        }

    }

    throw std::runtime_error("Failed to find suitable memory type");
}
