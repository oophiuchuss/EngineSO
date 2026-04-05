module;

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <vulkan/vulkan_raii.hpp>

module Rendergraph;


void Rendergraph::Compile()
{
    std::vector<std::vector<size_t>> Dependencies(Passes.size());
    std::vector<std::vector<size_t>> Dependents(Passes.size());

    std::unordered_map<std::string, size_t> ResourceWriters;

    // Analyze each pass to determine data flow relationships
    for (size_t i = 0; i < Passes.size(); i++)
    {
        const Pass& CurPass = Passes[i];

        for(const std::string& Input : CurPass.Inputs)
        {
            auto it = ResourceWriters.find(Input);
            if (it != ResourceWriters.end())
            {
                Dependencies[i].push_back(it->second);
                Dependents[it->second].push_back(i);
            }
        }
        
        for(const std::string& Output: CurPass.Outputs)
        {
            // TODO: shouldn't be checked for several passes that output to same resource?
            ResourceWriters[Output] = i;
        }
    }

    // Topological Sort for optimal execution order (depth-first algorithm)
    std::vector<bool> Visited(Passes.size(), false); // Tracks complited nodes
    std::vector<bool> InStack(Passes.size(), false); // Tracks current recursion path

    std::function<void(size_t)> Visit = [&](size_t Node)
        {
            if (InStack[Node])
            {
                throw std::runtime_error("Cycle detected in rendergraph");
            }

            if (Visited[Node])
            {
                return; // Was already processed
            }

            InStack[Node] = true;

            for (size_t Dependent : Dependents[Node])
            {
                Visit(Dependent);
            }

            InStack[Node] = false;
            Visited[Node] = true;

            ExecOrder.push_back(Node);
        };

    for (size_t i = 0; i < Passes.size(); i++)
    {
        if (!Visited[i])
        {
            Visit(i);
        }
    }

    // Automatic Synchronization Object Creation
    for (size_t i = 0; i < Passes.size(); i++)
    {
        for (auto Dep : Dependencies[i])
        {
            // Create a GPU semaphore which will delay dependent pass execution
            Semaphores.emplace_back(Device.createSemaphore({}));
            SemaphoreWaitPairs.emplace_back(Dep, i); // (producer, consumer) pair
        }
    }

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
            .setSubresourceRange({ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });  // Full Image access

        CurResource.View = Device.createImageView(ViewInfo);
    }

}

void Rendergraph::Execute(vk::raii::CommandBuffer& CommandBuffer, vk::Queue Queue)
{
    std::vector<vk::CommandBuffer> CmdBuffers;          // Command buffer storage
    std::vector<vk::Semaphore> WaitSemaphores;          // Synchronization dependencies for current pass
    std::vector < vk::PipelineStageFlags> WaitStages;   // Pipeline stages to wait on
    std::vector<vk::Semaphore> SignalSemaphores;        // Semaphores to signal after current pass

    // Pass execution with dependency-sage order
    for (auto PassIdx : ExecOrder)
    {
        const auto& CurPass = Passes[PassIdx];

        // Sync Setup 
        // Determine what this pass mush wait for before exec and who should this pass signal to
        WaitSemaphores.clear();
        WaitStages.clear();
        SignalSemaphores.clear();


        for (size_t i = 0; i < SemaphoreWaitPairs.size(); i++)
        {
            if (SemaphoreWaitPairs[i].first == PassIdx)
            {
                // Other passes should be notified of this pass completion
                SignalSemaphores.push_back(*Semaphores[i]);
            }

            if (SemaphoreWaitPairs[i].second == PassIdx)
            {
                // This pass depends on the complition of another pass
                WaitSemaphores.push_back(*Semaphores[i]); // Add waiting semaphore for dependency complition
                WaitStages.push_back(vk::PipelineStageFlagBits::eColorAttachmentOutput); // Wait at output stage
            }
        }


        // Command buffer prep and Resource layout transitions
        // Set up command recording and transition resources to appropriate layouts
        CommandBuffer.begin({}); // TODO: should this command buffer begin with no additional info?

        // Transition input resources to shader-readable layouts
        for (const std::string& CurInput : CurPass.Inputs)
        {
            Resource& CurResource = Resources[CurInput];

            vk::ImageMemoryBarrier Barrier;
            Barrier.setOldLayout(CurResource.InitialLayout)         // Current resource layuout
                .setNewLayout(CurResource.FinalLayout)              // Final resource layuout
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)    // No queue family transfer
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setImage(*CurResource.Image)                       // Target Image
                .setSubresourceRange({ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 })  // Full Image range
                .setSrcAccessMask(vk::AccessFlagBits::eMemoryWrite) // Previous write access
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead); // Required read access

            // Insert pipeline barrier for safe layout transition
            CommandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eAllCommands,    // Wait for all previous work
                vk::PipelineStageFlagBits::eFragmentShader, // Enable fragment shader access
                vk::DependencyFlagBits::eByRegion,          // Redion-local dependency
                {}, {}, {Barrier}                           // Image barrier only
            );
        }

        // Transition output resources to render targets layouts
        for (const std::string& CurOutput : CurPass.Outputs)
        {
            Resource& CurResource = Resources[CurOutput];

            vk::ImageMemoryBarrier Barrier;
            Barrier.setOldLayout(CurResource.InitialLayout)             // Current resource layuout
                .setNewLayout(vk::ImageLayout::eColorAttachmentOptimal) // Optimal for color output
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)        // No queue family transfer
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setImage(*CurResource.Image)                           // Target Image
                .setSubresourceRange({ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 })  // Full Image range
                .setSrcAccessMask(vk::AccessFlagBits::eMemoryRead)      // Previous read access
                .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);     // Required write access

            CommandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eAllCommands,            // Wait for all previous work
                vk::PipelineStageFlagBits::eColorAttachmentOutput,  // Enable color attachment writes
                vk::DependencyFlagBits::eByRegion,                  // Redion-local dependency
                {}, {}, {Barrier}                                   // Image barrier only
            );
        }

        // Pass execution of actual rendering logic
        // Call the user provided render function
        CurPass.ExecuteFunc(CommandBuffer); // Execute pass specific func

        // Final layout transition
        // Transition output resources to final reuired layouts 
        
        for (const std::string& CurOutput : CurPass.Outputs)
        {
            Resource& CurResource = Resources[CurOutput];

            vk::ImageMemoryBarrier Barrier;
            Barrier.setOldLayout(vk::ImageLayout::eColorAttachmentOptimal)      // Current writable layuout
                .setNewLayout(CurResource.FinalLayout)                          // Final resource layuout
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)                // No queue family transfer
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setImage(*CurResource.Image)                                   // Target Image
                .setSubresourceRange({ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 })  // Full Image range
                .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)    // Previous write opperation
                .setDstAccessMask(vk::AccessFlagBits::eMemoryRead);             // Enable subsequent reads

            CommandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eColorAttachmentOutput,  // After color writes complete
                vk::PipelineStageFlagBits::eAllCommands,            // Before any subsequent work
                vk::DependencyFlagBits::eByRegion,                  // Redion-local dependency
                {}, {}, { Barrier }                                 // Image barrier only
            );
        }

        // TODO: figure out why last output goes to memory read as next pass that will use it as input will expect it to be memory write

        // Command submission with sync
        // Submit buffer and signal semaphores
        CommandBuffer.end();

        vk::SubmitInfo SubmitInfo;
        SubmitInfo.setWaitSemaphoreCount(static_cast<uint32_t>(WaitSemaphores.size()))  // Number of dependencies to wait for
            .setPWaitSemaphores(WaitSemaphores.data())                                  // Wait dependency semaphores
            .setPWaitDstStageMask(WaitStages.data())                                    // Pipeline stages to wait for
            .setCommandBufferCount(1)                                                   // Single command buffer
            .setPCommandBuffers(&*CommandBuffer)                                        // Command buffer to exec
            .setSignalSemaphoreCount(static_cast<uint32_t>(SignalSemaphores.size()))    // Number of dependencies to signal
            .setPSignalSemaphores(SignalSemaphores.data());                             // Signal dependency semaphores

        Queue.submit(SubmitInfo);   // Submit to GPU queue
    }

}

void Rendergraph::AddResource(const std::string& Name, vk::Format Format, vk::Extent2D Extent, vk::ImageLayout InitLayout, vk::ImageLayout FinalLayout)
{
	Resources.try_emplace(Name, Name, Format, Extent, InitLayout, FinalLayout);
}

void Rendergraph::AddPass(const std::string& Name, const std::vector<std::string>& Inputs, const std::vector<std::string>& Outputs, std::function<void(vk::raii::CommandBuffer&)> ExecuteFunc)
{
    Pass NewPass;
    NewPass.Name = Name;
    NewPass.Inputs = Inputs;
    NewPass.Outputs = Outputs;
    NewPass.ExecuteFunc = std::move(ExecuteFunc);

    Passes.push_back(std::move(NewPass)); 
}

Resource* Rendergraph::GetResource(const std::string& Name)
{
    auto it = Resources.find(Name);
    if (it != Resources.end())
    {
        return &(it->second);
    }

    return nullptr;
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
