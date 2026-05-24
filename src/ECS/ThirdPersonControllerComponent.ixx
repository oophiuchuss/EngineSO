module;

#include <glm/glm.hpp>
#include <algorithm>

export module ThirdPersonControllerComponent;

import Component;
import TransformComponent;
import Scene;                
import Entity;

export class ThirdPersonControllerComponent : public ComponentBase
{
public:
	ThirdPersonControllerComponent(float InFollowDistance, float InFollowHeight, float InFollowSmoothness, float InMinDistance);

	// Core update function
	void UpdatePosition(const glm::vec3 TargetPos, const glm::vec3 TargetFwd, float DeltaTime);
	void HandleOcclusion(const Scene& InScene);
	void OrbitAroundTarget(float HorizontalAngle, float VerticalAngle);

	// Runtime configuration functions
	void SetFollowDistance(float NewDistance) { FollowDistance = NewDistance; }
	void SetFollowHeight(float NewHeight) { FollowHeight = NewHeight; }
	void SetFollowSmoothness(float NewSmoothness) { FollowSmoothness = std::clamp(NewSmoothness, 0.0f, 1.0f); }

private:
	// Target entity the camera is following
	glm::vec3 TargetPosition;	// Position of the target in world space
	glm::vec3 TargetForward;	// Target's forward direction for contextual camera orientation

	// Camera behavior parameters
	float FollowDistance;	// Desired distance from the target
	float FollowHeight;		// Desired height above the target
	float FollowSmoothness;	// Smoothing factor for camera movement to avoid abrupt changes (0.0f = instant, 1.0f = never)

	// Oclusion handling parameters
	float MinDistance; 		// Minimum distance to maintain from the target to avoid clipping
	float RaycastDistance;	// Maximum distance for raycasting to detect occlusions

	// Internal computational state for smoothing and occlusion handling
	glm::vec3 DesiredPosition;		// Calculated desired position based on target and camera parameters
	glm::vec3 SmoothDampVelocity;	// Velocity used for smoothing camera movement
};
