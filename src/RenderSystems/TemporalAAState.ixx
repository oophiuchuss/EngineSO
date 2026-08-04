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

	// Returns the previous submitted frame's jitter UV.
    glm::vec2 ResolvePreviousJitterUV(const glm::vec2& CurrentJitterUV) const;

	// Returns the previous submitted frame's camera position.
    glm::vec3 ResolvePreviousCameraPosition(const glm::vec3& CurrentCameraPosition) const;

    // Records the entity's current transform and returns its transform from
    // the previous submitted frame.
    //
    // New entities use CurrentModel as their previous transform, preventing
    // invalid first-frame motion.
    glm::mat4 ResolvePreviousModel(const Entity* EntityPtr, const glm::mat4& CurrentModel);

    // Returns the current subpixel jitter in pixel units.
    // Each component is approximately within [-0.5, 0.5].
    glm::vec2 GetCurrentJitterPixels() const;

    // Called only after the frame has been successfully submitted.
    void CommitFrame(const glm::mat4& SubmittedViewProjection, const glm::vec2& SubmittedJitterUV, const glm::vec3& SubmittedCameraPosition);

    // Invalidates all previous-frame information.
    void Invalidate();

    bool HasPreviousFrame() const { return bHasPreviousFrame; }

    uint64_t GetTemporalFrameIndex() const { return TemporalFrameIndex; }

private:
    static float ComputeHalton(uint64_t Index, uint32_t Base);

    static constexpr uint32_t JitterSampleCount = 8;

    bool bHasPreviousFrame = false;
    uint64_t TemporalFrameIndex = 0;

    glm::mat4 PreviousViewProjection = glm::mat4(1.0f);
    glm::vec2 PreviousJitterUV = glm::vec2(0.0f);
    glm::vec3 PreviousCameraPosition = glm::vec3(0.0f);

    std::unordered_map<const Entity*, glm::mat4> PreviousTransforms;
    std::unordered_map<const Entity*, glm::mat4> CurrentTransforms;
};