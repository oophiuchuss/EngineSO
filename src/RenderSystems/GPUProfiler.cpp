module;

#include <vulkan/vulkan_raii.hpp>

module GPUProfiler;

GPUProfiler::GPUProfiler(
    const vk::raii::Device& Device, 
    const vk::raii::PhysicalDevice& PhysicalDevice,
    uint32_t FramesInFlight,
    uint32_t MaxScopesPerFrame):
    DeviceRef(Device), 
    MaxScopesPerFrame(MaxScopesPerFrame)
{
    TimestampPeriodNs = PhysicalDevice.getProperties().limits.timestampPeriod;

    NextQueryIndex.resize(FramesInFlight, 0);
    PendingLabels.resize(FramesInFlight);

    vk::QueryPoolCreateInfo PoolInfo;
    PoolInfo.setQueryType(vk::QueryType::eTimestamp)
        .setQueryCount(MaxScopesPerFrame * 2); // begin+end per scope

    Pools.reserve(FramesInFlight);
    for (uint32_t i = 0; i < FramesInFlight; ++i)
    {
        Pools.emplace_back(DeviceRef, PoolInfo);
    }
}

void GPUProfiler::BeginFrame(vk::raii::CommandBuffer& Cmd, uint32_t FrameIndex)
{
    auto& Labels = PendingLabels[FrameIndex];

    if (!Labels.empty())
    {
        uint32_t QueryCount = static_cast<uint32_t>(Labels.size()) * 2;

        auto ResultValue = (*DeviceRef).getQueryPoolResults<uint64_t>(
            *Pools[FrameIndex],
            0,
            QueryCount,
            QueryCount * sizeof(uint64_t),   // dataSize
            sizeof(uint64_t),                // stride
            vk::QueryResultFlagBits::e64);

        if (ResultValue.result == vk::Result::eSuccess)
        {
            const auto& Timestamps = ResultValue.value;

            LastResults.clear();
            LastResults.reserve(Labels.size());
            for (size_t i = 0; i < Labels.size(); ++i)
            {
                uint64_t BeginTicks = Timestamps[i * 2];
                uint64_t EndTicks = Timestamps[i * 2 + 1];
                double Ms = static_cast<double>(EndTicks - BeginTicks) * TimestampPeriodNs / 1'000'000.0;
                LastResults.push_back({ Labels[i], Ms });
            }
        }
        // If not eSuccess, keep last frame's LastResults
    }

    Cmd.resetQueryPool(*Pools[FrameIndex], 0, MaxScopesPerFrame * 2);
    NextQueryIndex[FrameIndex] = 0;
    Labels.clear();
}

GPUProfiler::Scope GPUProfiler::BeginScope(vk::raii::CommandBuffer& Cmd, uint32_t FrameIndex, std::string Label)
{
    return GPUProfiler::Scope(*this, Cmd, FrameIndex, std::move(Label));
}


GPUProfiler::Scope::Scope(
    GPUProfiler& InOwner,
    vk::raii::CommandBuffer& InCmd,
    uint32_t InFrameIndex,
    std::string InLabel) :
    Owner(InOwner),
    Cmd(InCmd),
    FrameIndex(InFrameIndex)
{
    uint32_t BeginQuery = Owner.NextQueryIndex[FrameIndex]++;
    EndQuery = Owner.NextQueryIndex[FrameIndex]++;

    // MaxScopesPerFrame exceeded — silently clamping would corrupt readback pairing,
    // so this is deliberately a hard assumption rather than a soft-fail.
    // (Consider an assert here once you've settled on your engine's assert convention.)

    Owner.PendingLabels[FrameIndex].push_back(std::move(InLabel));
    Cmd.writeTimestamp(vk::PipelineStageFlagBits::eTopOfPipe, *Owner.Pools[FrameIndex], BeginQuery);
}

GPUProfiler::Scope::~Scope()
{
    Cmd.writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, *Owner.Pools[FrameIndex], EndQuery);
}