module;

#include <vulkan/vulkan_raii.hpp>
#include <string>
#include <vector>

export module GPUProfiler;

export class GPUProfiler
{
public:
    GPUProfiler(const vk::raii::Device& Device,
                const vk::raii::PhysicalDevice& PhysicalDevice,
                uint32_t FramesInFlight,
                uint32_t MaxScopesPerFrame = 16);

    void BeginFrame(vk::raii::CommandBuffer& Cmd, uint32_t FrameIndex);

    struct ScopeResult
    {
        std::string Label;
        double Milliseconds;
    };

    const std::vector<ScopeResult>& GetLastResults() const { return LastResults; }
  
    class Scope
    {
    public:
        Scope(GPUProfiler& InOwner, vk::raii::CommandBuffer& InCmd, uint32_t InFrameIndex, std::string InLabel);

        ~Scope();

        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
        Scope(Scope&&) = default;

    private:
        GPUProfiler& Owner;
        vk::raii::CommandBuffer& Cmd;
        uint32_t FrameIndex;
        uint32_t EndQuery;
    };

    Scope BeginScope(vk::raii::CommandBuffer& Cmd, uint32_t FrameIndex, std::string Label);

private:
    const vk::raii::Device& DeviceRef;
    std::vector<vk::raii::QueryPool> Pools;                 // one per frame-in-flight
    std::vector<uint32_t> NextQueryIndex;                   // per frame-in-flight, reset each BeginFrame
    std::vector<std::vector<std::string>> PendingLabels;    // per frame-in-flight, labels in write order
    std::vector<ScopeResult> LastResults;
    double TimestampPeriodNs = 1.0;
    uint32_t MaxScopesPerFrame;
};