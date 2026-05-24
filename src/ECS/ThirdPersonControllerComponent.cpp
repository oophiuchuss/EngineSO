module; 

#include <glm/glm.hpp>

module ThirdPersonControllerComponent;

ThirdPersonControllerComponent::ThirdPersonControllerComponent(float InFollowDistance, float InFollowHeight, float InFollowSmoothness, float InMinDistance)
{
	FollowDistance = InFollowDistance;
	FollowHeight = InFollowHeight;
	FollowSmoothness = InFollowSmoothness;
	MinDistance = InMinDistance;
}

void ThirdPersonControllerComponent::UpdatePosition(const glm::vec3 TargetPos, const glm::vec3 TargetFwd, float DeltaTime)
{
	/*	TargetPosition = TargetPos;
		TargetForward = TargetFwd;

		// Calculate the desired camera position based on the target's position and forward direction
		glm::vec3 Offset = -TargetForward * FollowDistance;
		Offset.y = FollowHeight; // Set height offset

		DesiredPosition = TargetPosition + Offset;

		// Smooth camera movement using exponential smoothing
		Position = glm::mix(Position, DesiredPosition, 1.0f - pow(FollowSmoothness, DeltaTime + 60.0f));

		// Update the camera's orientation to look at the target
		Front = glm::normalize(TargetPosition - Position);

		// Recalculate Right and Up vectors
		Right = glm::normalize(glm::cross(Front, WorldUp));
		Up = glm::normalize(glm::cross(Right, Front));*/
}

void ThirdPersonControllerComponent::HandleOcclusion(const Scene& InScene)
{
	/*	Ray RayToCamera(TargetPosition, glm::normalize(DesiredPosition - TargetPosition));

		RayCastHit Hit;

		bool bIsOccluded = InScene.Raycast(RayToCamera, Hit, RaycastDistance);

		if (bIsOccluded)
		{
			// Subtract small offset to prevent clipping into the occluding object
			float OffsetDistance = 0.2f; // Adjust this value as needed
			Position = Hit.Point - (RayToCamera.Direction * OffsetDistance);

			// Ensure we don't get too close to the target
			float CurrentDistance = glm::length(Position - TargetPosition);
			if (CurrentDistance < MinDistance)
			{
				Position = TargetPosition + (RayToCamera.Direction * MinDistance);
			}

			//Update the camera's orientation to look at the target
			Front = glm::normalize(TargetPosition - Position);
			Right = glm::normalize(glm::cross(Front, WorldUp));
			Up = glm::normalize(glm::cross(Right, Front));
		}*/
}

void ThirdPersonControllerComponent::OrbitAroundTarget(float HorizontalAngle, float VerticalAngle)
{
	/*// Update yaw and pitch based on input angles
	Yaw += HorizontalAngle;
	Pitch += VerticalAngle;

	// Constrain pitch to prevent flipping
	Pitch = std::clamp(Pitch, -89.0f, 89.0f);

	// Calculate the new offset from the target based on spherical coordinates
	float Radius = FollowDistance;
	float YawRad = glm::radians(Yaw);
	float PitchRad = glm::radians(Pitch);

	// Convert spherical coordinates to Cartesian coordinates
	glm::vec3 Offset;
	Offset.x = Radius * cos(PitchRad) * sin(YawRad);
	Offset.y = Radius * sin(PitchRad);
	Offset.z = Radius * cos(PitchRad) * cos(YawRad);

	DesiredPosition = TargetPosition + Offset;

	Front = glm::normalize(TargetPosition - Position);
	Right = glm::normalize(glm::cross(Front, WorldUp));
	Up = glm::normalize(glm::cross(Right, Front));*/
}
