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