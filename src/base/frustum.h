#pragma once

#include <iostream>
#include "plane.h"
#include "bounding_box.h"
#include "transform.h"

struct Frustum {
public:
	Plane planes[6];
	enum {
		LeftFace = 0,
		RightFace = 1,
		BottomFace = 2,
		TopFace = 3,
		NearFace = 4,
		FarFace = 5
	};

	bool intersect(const BoundingBox& aabb, const glm::mat4& modelMatrix) const {
		// TODO: judge whether the frustum intersects the bounding box
		// write your code here
		// ------------------------------------------------------------

		Transform transform;
		transform.setFromTRS(modelMatrix);
		const glm::vec3 global_center = transform.getLocalMatrix() 
						* glm::vec4((aabb.min + aabb.max) / 2.0f, 1.0f);
		glm::vec3 extents = (aabb.max - aabb.min) / 2.0f;
		// scaled orientation
		const glm::vec3 right = transform.getRight() * extents.x;
		const glm::vec3 up = transform.getUp() * extents.y;
		const glm::vec3 forward = transform.getFront() * extents.z;

		// new extents
		const float newIi = std::abs(glm::dot(glm::vec3{ 1.f, 0.f, 0.f }, right)) +
			std::abs(glm::dot(glm::vec3{ 1.f, 0.f, 0.f }, up)) +
			std::abs(glm::dot(glm::vec3{ 1.f, 0.f, 0.f }, forward));
		const float newIj = std::abs(glm::dot(glm::vec3{ 0.f, 1.f, 0.f }, right)) +
			std::abs(glm::dot(glm::vec3{ 0.f, 1.f, 0.f }, up)) +
			std::abs(glm::dot(glm::vec3{ 0.f, 1.f, 0.f }, forward));
		const float newIk = std::abs(glm::dot(glm::vec3{ 0.f, 0.f, 1.f }, right)) +
			std::abs(glm::dot(glm::vec3{ 0.f, 0.f, 1.f }, up)) +
			std::abs(glm::dot(glm::vec3{ 0.f, 0.f, 1.f }, forward));

		extents = glm::vec3(newIi, newIj, newIk);
		
		for (int i = 0; i < 6; ++i)
		{
			const Plane& plan = planes[i];
			// Compute the projection interval radius of b onto L(t) = b.c + t * p.n
			// https://gdbooks.gitbooks.io/3dcollisions/content/Chapter2/static_aabb_plane.html
			const float r = extents.x * std::abs(plan.normal.x) +
				extents.y * std::abs(plan.normal.y) + extents.z * std::abs(plan.normal.z);

			bool result = -r <= plan.getSignedDistanceToPoint(global_center);
			if (!result) return false;
		}

		 return true;

		// ------------------------------------------------------------
	}
};

inline std::ostream& operator<<(std::ostream& os, const Frustum& frustum) {
	os << "frustum: \n";
	os << "planes[Left]:   " << frustum.planes[0] << "\n";
	os << "planes[Right]:  " << frustum.planes[1] << "\n";
	os << "planes[Bottom]: " << frustum.planes[2] << "\n";
	os << "planes[Top]:    " << frustum.planes[3] << "\n";
	os << "planes[Near]:   " << frustum.planes[4] << "\n";
	os << "planes[Far]:    " << frustum.planes[5] << "\n";

	return os;
}