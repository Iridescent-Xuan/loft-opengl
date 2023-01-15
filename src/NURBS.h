#pragma once

#include <glm/glm.hpp>
#include <glad/glad.h>

#include <vector>
#include <memory>

#include "./base/glsl_program.h"

class NURBS {
public:
	NURBS();

	void draw();

	void generateControlPointsBuffers();
	void generateSplineBuffers();
	void generateGeometricBuffers(std::vector<glm::vec2> geom);

	void nurbsSpline();
	glm::vec2 nurbsCurve(float u);

	void generateGeometric();

public:
	std::vector<glm::vec2> _controlPoints;
	std::vector<float> _weights;
	int _order = 2;
	float _uInc = 0.01f;
	std::vector<float> _knots;
	std::vector<glm::vec2> _spline;

	bool _geometric = false;
	std::vector<std::vector<glm::vec2> > _geom;
	float _uDisplay = 0.45f;

	GLuint _controlPointsVao = 0;
	GLuint _controlPointsVbo = 0;
	GLuint _splineVao = 0;
	GLuint _splineVbo = 0;
	GLuint _geomVao = 0;
	GLuint _geomVbo = 0;

	std::unique_ptr<GLSLProgram> _NURBSshader;
};