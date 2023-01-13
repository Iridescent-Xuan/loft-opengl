#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <glad/glad.h>

class BaseGeo {
public:
	BaseGeo(glm::vec3 global_position = glm::vec3(0.0f, 0.0f, 0.0f));

	BaseGeo(const BaseGeo& rhs) = delete;

	BaseGeo(BaseGeo&& rhs) noexcept;

	~BaseGeo();

	virtual void draw() const;

	virtual bool SaveObj(const std::string& filepath, const glm::mat4& view, std::string* err) const;

	float _rotate_angle_self = 0.0f;
	float _rotate_angle_camera = 0.0f;
	glm::vec3 _scale = glm::vec3(1.0f, 1.0f, 1.0f);

protected:
	GLuint _vao = 0;
	GLuint _vbo = 0;
	GLuint _ebo = 0;

	std::vector<float> _vertices;
	std::vector<GLuint> _indices;

public:
	glm::vec3 _global_position;
	glm::mat4 _model{
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f };
};

class Cube : public BaseGeo {
public:
	Cube(glm::vec3 global_position, float _scale = 1.0f);

private:
	float _scale = 1.0f;
};

class Cone : public BaseGeo {
public:
	Cone(glm::vec3 global_position, float radius = 0.5f, float height = 1.0f, int corners = 36);

private:
	float _radius;
	float _height;
	int _corners;
};

class Cylinder : public BaseGeo {
public:
	Cylinder(glm::vec3 global_position, float radius = 0.5f, float height = 1.0f, int corners = 36);

private:
	float _radius;
	float _height;
	int _corners;
};

class Sphere :public BaseGeo {
public:
	Sphere(glm::vec3 global_position, float radius = 0.5f, int segments = 36);

private:
	float _radius;
	int _segments;
};

class Prism : public BaseGeo {
public:
	Prism(glm::vec3 global_position, int corners = 3, float radius = 0.5f, float height = 1.0f);

private:
	float _radius;
	float _height;
	int _corners;
};

class Frust : public BaseGeo {
public:
	Frust(glm::vec3 global_position, int corners = 3, float radius1 = 1.0f, float radius2 = 0.5f, float height = 1.0f);

private:
	float _radius1;
	float _radius2;
	float _height;
	int _corners;
};