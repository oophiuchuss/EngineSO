module;

#include <string>
#include <vector>

export module PerformanceStats;

export struct GPUScopeTiming
{
    std::string Label;
    double Milliseconds = 0.0;
};

export struct PerformanceStats
{
    double CPUFrameMilliseconds = 0.0;
    double FramesPerSecond = 0.0;

    double TotalGPUFrameMilliseconds = 0.0;
    std::vector<GPUScopeTiming> GPUScopeTimings;
};