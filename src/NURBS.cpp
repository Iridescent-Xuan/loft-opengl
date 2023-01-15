#include "NURBS.h"

#define FLOAT_ERR 0.00001f

NURBS::NURBS() {
	const char* NURBS_vs =
		"#version 330 core\n"
		"layout(location = 0) in vec2 aPosition;\n"

		"void main() {\n"
		"	gl_Position = vec4(aPosition, 0.f, 1.f);\n"
		"}\n";

	const char* NURBS_fs =
		"#version 330 core\n"
		"out vec4 color;\n"

		"uniform vec4 color_in;\n"

		"void main() {\n"
		"	color = color_in;\n"
		"}\n";

	_NURBSshader.reset(new GLSLProgram);
	_NURBSshader->attachVertexShader(NURBS_vs);
	_NURBSshader->attachFragmentShader(NURBS_fs);
	_NURBSshader->link();
}

void NURBS::generateSplineBuffers() {
	// release previous resources
	if (_splineVao) {
		glDeleteVertexArrays(1, &_splineVao);
		_splineVao = 0;
	}
	if (_splineVbo) {
		glDeleteBuffers(1, &_splineVbo);
		_splineVbo = 0;
	}

	// init new resources
	glGenVertexArrays(1, &_splineVao);
	glBindVertexArray(_splineVao);

	nurbsSpline();
	if (_geometric)
		generateGeometric();

	glGenBuffers(1, &_splineVbo);
	glBindBuffer(GL_ARRAY_BUFFER, _splineVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec2) * _spline.size(), _spline.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(0);

	glBindVertexArray(0);
}

void NURBS::generateControlPointsBuffers() {
	// release previous resources
	if (_controlPointsVao) {
		glDeleteVertexArrays(1, &_controlPointsVao);
		_controlPointsVao = 0;
	}
	if (_controlPointsVbo) {
		glDeleteBuffers(1, &_controlPointsVbo);
		_controlPointsVbo = 0;
	}

	// init new resources
	glGenVertexArrays(1, &_controlPointsVao);
	glBindVertexArray(_controlPointsVao);
	glGenBuffers(1, &_controlPointsVbo);
	glBindBuffer(GL_ARRAY_BUFFER, _controlPointsVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec2) * _controlPoints.size(), _controlPoints.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(0);

	glBindVertexArray(0);
}

void NURBS::generateGeometricBuffers(std::vector<glm::vec2> geom) {
	// release previous resources
	if (_geomVao) {
		glDeleteVertexArrays(1, &_geomVao);
		_geomVao = 0;
	}
	if (_geomVbo) {
		glDeleteBuffers(1, &_geomVbo);
		_geomVbo = 0;
	}

	// init new resources
	glGenVertexArrays(1, &_geomVao);
	glBindVertexArray(_geomVao);
	glGenBuffers(1, &_geomVbo);
	glBindBuffer(GL_ARRAY_BUFFER, _geomVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec2) * geom.size(), geom.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(0);

	glBindVertexArray(0);
}

void NURBS::draw() {
	// draw control points
	if (_controlPoints.size() > 0) {
		glBindVertexArray(_controlPointsVao);
		_NURBSshader->use();
		_NURBSshader->setUniformVec4("color_in", glm::vec4(1.f, 1.f, 0.f, 0.f));

		for (int i = 0; i < _controlPoints.size(); i++)
		{
			glPointSize(15 * _weights[i]);
			if (_weights[i] == 0.f)
				glColorMask(GL_FALSE, GL_TRUE, GL_TRUE, GL_TRUE);
			else
				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			glDrawArrays(GL_POINTS, i, 1);
		}

		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

		glBindVertexArray(0);
	}
	// draw splines
	if (_controlPoints.size() > 1) {
		glBindVertexArray(_splineVao);
		_NURBSshader->use();
		_NURBSshader->setUniformVec4("color_in", glm::vec4(1.f, 1.f, 1.f, 0.f));

		glDrawArrays(GL_LINE_STRIP, 0, _spline.size());

		glBindVertexArray(0);

		if (_geometric) {
			for (int i = 0; i < _geom.size(); i++)
			{
				generateGeometricBuffers(_geom[i]);

				glBindVertexArray(_geomVao);
				_NURBSshader->use();
				_NURBSshader->setUniformVec4("color_in", glm::vec4(1.f, 0.f, 0.f, 0.f));

				glPointSize(10);
				glDrawArrays(GL_POINTS, 0, _geom[i].size());
				glDrawArrays(GL_LINE_STRIP, 0, _geom[i].size());

				glBindVertexArray(0);
			}
		}
	}
}

void NURBS::nurbsSpline() {
	unsigned int minNumOfKnots = _controlPoints.size() + _order;
	float uStep = (1.f / (_controlPoints.size() - _order + 1));

	// knots vector
	_knots.clear();
	for (int i = 0; i < minNumOfKnots; ++i) {
		if (i < _order)
			_knots.push_back(0.f);
		else if (i > _controlPoints.size())
			_knots.push_back(1.f);
		else
			_knots.push_back(_knots[i - 1] + uStep);
	}

	// calculate the B-spline
	_spline.clear();
	for (float u = 0.f; u < 1.f; u += _uInc)
		_spline.push_back(nurbsCurve(u));
	// due to floating point errors, if we made the previous for loop <= 1.f, it might not reach 1.f.
	// so we restrict it to < 1.f to ensure that it never reaches 1.f and then discretly add in the last control point as the final point.
	// https://gitee.com/liu_wei792966953/nurbs-in-glfw/blob/master/NURBS/src/Geometry.cpp
	_spline.push_back(_controlPoints.back());
}

glm::vec2 NURBS::nurbsCurve(float u) {
	std::vector<glm::vec2> c;
	std::vector<float> w;
	int delta = 0;

	// determine the delta needed for the specific value of u
	while (u >= _knots[delta + 1]) { delta++; }

	// get all the control points that are relavent to our u value
	for (int i = 0; i <= _order - 1; i++)
	{
		c.push_back(_controlPoints[delta - i] * _weights[delta - i]);
		w.push_back(_weights[delta - i]);
	}

	// use the efficient implentation to determine placement of the final point
	for (int r = _order; r >= 2; r--)
	{
		int i = delta;
		for (int s = 0; s < r - 1; s++)
		{
			float   denominator = _knots[i + r - 1] - _knots[i],
				omega = 0.f;
			if (denominator > FLOAT_ERR)
				omega = (u - _knots[i]) / denominator;
			c[s] = omega * c[s] + (1 - omega) * c[s + 1];
			w[s] = omega * w[s] + (1 - omega) * w[s + 1];
			i--;
		}
	}

	return c[0] / w[0];
}

void NURBS::generateGeometric() {
	_geom.clear();

	std::vector<glm::vec2> c, temp;
	std::vector<float> w;
	int delta = 0;

	// determine the delta needed for the specific value of u
	while (_uDisplay >= _knots[delta + 1]) { delta++; }

	// get all the control points that are relavent to our u value
	for (int i = 0; i <= _order - 1; i++)
	{
		c.push_back(_controlPoints[delta - i] * _weights[delta - i]);
		w.push_back(_weights[delta - i]);
		temp.push_back(c.back() / w.back());
	}

	_geom.push_back(temp);
	temp.clear();


	// use the efficient implentation to determine placement of the final point
	for (int r = _order; r >= 2; r--)
	{
		int i = delta;
		for (int s = 0; s < r - 1; s++)
		{
			float   denominator = _knots[i + r - 1] - _knots[i],
				omega = 0.f;
			if (denominator > FLOAT_ERR)
				omega = (_uDisplay - _knots[i]) / denominator;
			c[s] = omega * c[s] + (1 - omega) * c[s + 1];
			w[s] = omega * w[s] + (1 - omega) * w[s + 1];
			// since we are looking to view all the in-between geometry, we need to record every point that we gnerate
			temp.push_back(c[s] / w[s]);
			i--;
		}
		_geom.push_back(temp);
		temp.clear();
	}
}