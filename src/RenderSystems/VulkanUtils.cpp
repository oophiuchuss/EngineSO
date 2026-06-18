module;

#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <stdexcept>

module VulkanUtils;

uint32_t VulkanUtils::FindMemoryType(const vk::raii::PhysicalDevice& PhysDev, uint32_t TypeFilter, const vk::MemoryPropertyFlags& Properties)
{
    vk::PhysicalDeviceMemoryProperties MemProperties = PhysDev.getMemoryProperties();

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

void VulkanUtils::TransitionImageLayout(
    vk::raii::CommandBuffer& CommandBuffer,
    vk::Image Image,
    vk::ImageAspectFlags Aspect,
    vk::ImageLayout OldLayout,
    vk::ImageLayout NewLayout)
{
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

    // Delegate to explicit overload
    TransitionImageLayout(
        CommandBuffer, Image, Aspect,
        OldLayout, NewLayout,
        SrcStage, DstStage, SrcAccess, DstAccess);
}

void VulkanUtils::TransitionImageLayout(
    vk::raii::CommandBuffer& CommandBuffer,
    vk::Image Image,
    vk::ImageAspectFlags Aspect,
    vk::ImageLayout OldLayout,
    vk::ImageLayout NewLayout,
    vk::PipelineStageFlags SrcStage,
    vk::PipelineStageFlags DstStage,
    vk::AccessFlags SrcAccess,
    vk::AccessFlags DstAccess)
{
    // Configure image barrier strcture to coordinate memory access

    vk::ImageMemoryBarrier Barrier;
    Barrier.setOldLayout(OldLayout)                         // Current resource layuout
        .setNewLayout(NewLayout)                            // Final resource layuout
        .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)    // No queue family transfer
        .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .setImage(Image)                                    // Target Image
        .setSubresourceRange({ Aspect, 0, 1, 0, 1 })        // Full Image range
        .setSrcAccessMask(SrcAccess)
        .setDstAccessMask(DstAccess);

    CommandBuffer.pipelineBarrier(
        SrcStage,                           // Wait for this operations to complete
        DstStage,                           // Before allowing these operations to begin
        vk::DependencyFlagBits::eByRegion,  // Enable region-local optimization
        {}, {}, { Barrier }                 // Apply image barrier
    );
}
