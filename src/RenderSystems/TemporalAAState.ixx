module;

#include <cstdint>
#include <unordered_map>

#include <glm/glm.hpp>

export module TemporalAAState;

import Entity;

export class TemporalAAState
{
public:
    // Starts collecting transforms for the frame currently being built.
    void BeginFrame();

    // Returns the previous submitted frame's camera matrix.
    // For the first valid frame, CurrentViewProjection is returned.
    glm::mat4 ResolvePreviousViewProjection(const glm::mat4& CurrentViewProjection) const;

    // Records the entity's current transform and returns its transform from
    // the previous submitted frame.
    //
    // New entities use CurrentModel as their previous transform, preventing
    // invalid first-frame motion.
    glm::mat4 ResolvePreviousModel(const Entity* EntityPtr, const glm::mat4& CurrentModel);

    // Called only after the frame has been successfully submitted.
    void CommitFrame(const glm::mat4& SubmittedViewProjection);

    // Invalidates all previous-frame information.
    void Invalidate();

    bool HasPreviousFrame() const { return bHasPreviousFrame; }

    uint64_t GetTemporalFrameIndex() const { return TemporalFrameIndex; }

private:
    bool bHasPreviousFrame = false;
    uint64_t TemporalFrameIndex = 0;

    glm::mat4 PreviousViewProjection = glm::mat4(1.0f);

    std::unordered_map<const Entity*, glm::mat4> PreviousTransforms;
    std::unordered_map<const Entity*, glm::mat4> CurrentTransforms;
};