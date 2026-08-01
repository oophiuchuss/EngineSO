module;

#include <utility>

#include <glm/glm.hpp>

module TemporalAAState;

void TemporalAAState::BeginFrame()
{
    CurrentTransforms.clear();
}

glm::mat4 TemporalAAState::ResolvePreviousViewProjection(const glm::mat4& CurrentViewProjection) const
{
    if (!bHasPreviousFrame)
    {
        return CurrentViewProjection;
    }

    return PreviousViewProjection;
}

glm::mat4 TemporalAAState::ResolvePreviousModel(const Entity* EntityPtr, const glm::mat4& CurrentModel)
{
    if (!EntityPtr)
    {
        return CurrentModel;
    }

    CurrentTransforms.insert_or_assign(EntityPtr, CurrentModel);

    if (!bHasPreviousFrame)
    {
        return CurrentModel;
    }

    const auto PreviousIt = PreviousTransforms.find(EntityPtr);

    if (PreviousIt == PreviousTransforms.end())
    {
        return CurrentModel;
    }

    return PreviousIt->second;
}

glm::vec2 TemporalAAState::GetCurrentJitterPixels() const
{
    const uint64_t SampleIndex = TemporalFrameIndex % JitterSampleCount;

    // Halton index zero is the sequence origin. Starting at one avoids making the first sample a special corner value.
    const uint64_t HaltonIndex = SampleIndex + 1;

    return glm::vec2(ComputeHalton(HaltonIndex, 2), ComputeHalton(HaltonIndex, 3)) - glm::vec2(0.5f);
}

void TemporalAAState::CommitFrame(const glm::mat4& SubmittedViewProjection)
{
    PreviousViewProjection = SubmittedViewProjection;

    PreviousTransforms = std::move(CurrentTransforms);
    CurrentTransforms.clear();

    bHasPreviousFrame = true;
    TemporalFrameIndex++;
}

void TemporalAAState::Invalidate()
{
    bHasPreviousFrame = false;
    TemporalFrameIndex = 0;
    PreviousViewProjection = glm::mat4(1.0f);

    PreviousTransforms.clear();
    CurrentTransforms.clear();
}

float TemporalAAState::ComputeHalton(uint64_t Index, uint32_t Base)
{
    float Result = 0.0f;
    float Fraction = 1.0f;

    while (Index > 0)
    {
        Fraction /= static_cast<float>(Base);

        Result += Fraction * static_cast<float>(Index % Base);

        Index /= Base;
    }

    return Result;
}
