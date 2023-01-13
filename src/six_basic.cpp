#include <cmath>
#include <vector>
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>

#include "six_basic.h"

#define MY_PI (3.141592653)

BaseGeo::BaseGeo(glm::vec3 global_position) : _global_position(global_position) {}

BaseGeo::BaseGeo(BaseGeo&& rhs) noexcept :_vao(rhs._vao), _vbo(rhs._vbo) {
    rhs._vao = 0;
    rhs._vbo = 0;

    _vertices = std::move(rhs._vertices);
    _indices = std::move(rhs._indices);
}

BaseGeo::~BaseGeo() {
    if (_vbo) {
        glDeleteVertexArrays(1, &_vbo);
        _vbo = 0;
    }

    if (_vao) {
        glDeleteVertexArrays(1, &_vao);
        _vao = 0;
    }

    if (_ebo) {
        glDeleteBuffers(1, &_ebo);
        _ebo = 0;
    }
}

void BaseGeo::draw() const {
    glBindVertexArray(_vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(_indices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

bool BaseGeo::SaveObj(const std::string& filepath, const glm::mat4& view, std::string* err) const {
    static int count = 0;
    static int total_vertices = 1;
    std::stringstream errss;

    std::ofstream outStream;
    if (count == 0)
        outStream.open(filepath);
    else
        outStream.open(filepath, std::ios_base::app);

    if (!outStream) {
        errss << "Cannot open file [" << filepath << "]" << std::endl;
        if (err) {
            (*err) = errss.str();
        }
        return false;
    }

    if(count == 0) outStream << "# All money go my home!" << std::endl;

    assert(_vertices.size() % 3 == 0 && "Only triangular meshes are allowed!");

    outStream << "o six_basic_" << count << std::endl;

    // change local coordinates to world coordinates
    std::vector<glm::vec3> global_vertices;
    for (int i = 0; i < _vertices.size(); i += 3) {
        glm::vec4 vertex(_vertices[i], _vertices[i + 1], _vertices[i + 2], 1.0f);
        vertex = _model * vertex;
        global_vertices.push_back(glm::vec3(vertex.x, vertex.y, vertex.z));
    }

    for (int i = 0; i < global_vertices.size(); ++i)
    {
        outStream << "v " << global_vertices[i].x << " " << global_vertices[i].y 
            << " " << global_vertices[i].z << std::endl;
    }
    for (int i = 0; i < _indices.size(); i += 3)
    {
        outStream << "f " << _indices[i] + total_vertices << " "
            << _indices[i + 1] + total_vertices << " " << _indices[i + 2] + total_vertices;
        outStream << std::endl;
    }
    outStream.close();

    count = (count + 1) % 6;
    total_vertices = count == 0 ? 1 : total_vertices + static_cast<int>(global_vertices.size());

    return true;
}

Cube::Cube(glm::vec3 global_position, float scale) : BaseGeo(global_position), _scale(scale) {
    std::vector<float> vertices{
        -0.5f, -0.5f, -0.5f,
        0.5f, -0.5f, -0.5f,
        -0.5f, 0.5f, -0.5f,
        0.5f, 0.5f, -0.5f,
        -0.5f, -0.5f, 0.5f,
        0.5f, -0.5f, 0.5f,
        -0.5f, 0.5f, 0.5f,
        0.5f, 0.5f, 0.5f
    };

    for (auto& i : vertices)
        i *= _scale;

    std::vector<GLuint> indices{
        0, 1, 2,
        1, 2, 3,
        4, 5, 6,
        5, 6, 7,
        2, 6, 7,
        2, 7, 3,
        0, 4, 5,
        0, 5, 1,
        7, 1, 5,
        7, 1, 3,
        6, 0, 4,
        6, 0, 2
    };

    _vertices.assign(vertices.begin(), vertices.end());
    _indices.assign(indices.begin(), indices.end());

    glGenVertexArrays(1, &_vao);
    glGenBuffers(1, &_vbo);
    glGenBuffers(1, &_ebo);

    glBindVertexArray(_vao);

    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * _vertices.size(), _vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, _indices.size() * sizeof(GLuint), _indices.data(), GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

Cone::Cone(glm::vec3 global_position, float radius, float height, int corners)
    : BaseGeo(global_position), _radius(radius), _height(height), _corners(corners) {

    float angle = 2.0f * MY_PI / _corners;
    std::vector<float> vertices((_corners + 2) * 3);
    std::vector<GLuint> indices(_corners * 2 * 3);

    // center of the bottom circle
    vertices[0] = 0.0f; vertices[1] = 0.0f; vertices[2] = 0.0f; 
    // cone apex
    vertices[vertices.size() - 1] = 0.0f; 
    vertices[vertices.size() - 2] = _height; 
    vertices[vertices.size() - 3] = 0.0f;;

    for (int i = 0; i < _corners; ++i) {
        // divide along the bottom circle
        vertices[3 * i + 3] = cos(angle * i) * _radius;
        vertices[3 * i + 4] = 0.0f;
        vertices[3 * i + 5] = -sin(angle * i) * _radius;

        if (i > 0) {
            // triangle with 2 vertices along the circle and 1 at the center
            indices[i * 6] = 0;
            indices[i * 6 + 1] = i;
            indices[i * 6 + 2] = i + 1;
            // triangle with 2 vertices along the circle and 1 at the apex
            indices[i * 6 + 3] = _corners + 1;
            indices[i * 6 + 4] = i;
            indices[i * 6 + 5] = i + 1;
        }
        else if (i == 0) { // end to end
            indices[0] = 0;
            indices[1] = _corners;
            indices[2] = 1;
            indices[3] = _corners + 1;
            indices[4] = _corners;
            indices[5] = 1;
        }
    }

    _vertices.assign(vertices.begin(), vertices.end());
    _indices.assign(indices.begin(), indices.end());

    glGenVertexArrays(1, &_vao);
    glGenBuffers(1, &_vbo);
    glGenBuffers(1, &_ebo);

    glBindVertexArray(_vao);

    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * _vertices.size(), _vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, _indices.size() * sizeof(GLuint), _indices.data(), GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

Cylinder::Cylinder(glm::vec3 global_position, float radius, float height, int corners)
    : BaseGeo(global_position), _radius(radius), _height(height), _corners(corners) {

    float angle = 2.0f * MY_PI / _corners;
    std::vector<float> vertices((_corners + 1) * 2 * 3);
    std::vector<GLuint> indices(_corners * 4 * 3);

    // center of the bottom circle
    vertices[0] = 0.0f; vertices[1] = 0.0f; vertices[2] = 0.0f;
    // center of the top circle
    vertices[(_corners + 1) * 3] = 0.0f; 
    vertices[(_corners + 1) * 3 + 1] = _height;
    vertices[(_corners + 1) * 3 + 2] = 0.0f;

    for (int i = 0; i < _corners; ++i) {
        // divide along the bottom circle
        vertices[3 * i + 3] = cos(angle * i) * _radius;
        vertices[3 * i + 4] = 0.0f;
        vertices[3 * i + 5] = -sin(angle * i) * _radius;
        vertices[(_corners + 1) * 3 + 3 * i + 3] = cos(angle * i) * _radius;
        vertices[(_corners + 1) * 3 + 3 * i + 4] = _height;
        vertices[(_corners + 1) * 3 + 3 * i + 5] = -sin(angle * i) * _radius;

        if (i > 0) {
            // triangle with 2 vertices along the bottom edge and 1 at the bottom center
            indices[i * 12] = 0;
            indices[i * 12 + 1] = i;
            indices[i * 12 + 2] = i + 1;
            // triangle with 2 vertices along the bottom edge and 1 at the top edge
            indices[i * 12 + 3] = _corners + 1 + i;
            indices[i * 12 + 4] = i;
            indices[i * 12 + 5] = i + 1;
            // triangle with 2 vertices along the top edge and 1 at the top center
            indices[i * 12 + 6] = _corners + 1;
            indices[i * 12 + 7] = _corners + 1 + i;
            indices[i * 12 + 8] = _corners + 1 + i + 1;
            // triangle with 2 vertices along the top edge and 1 at the bottom edge
            indices[i * 12 + 9] = i + 1;
            indices[i * 12 + 10] = _corners + 1 + i;
            indices[i * 12 + 11] = _corners + 1 + i + 1;
        }
        else if (i == 0) { // end to end
            indices[0] = 0;
            indices[1] = _corners;
            indices[2] = 1;
            indices[3] = 2 * _corners + 1;
            indices[4] = _corners;
            indices[5] = 1;
            indices[6] = _corners + 1;
            indices[7] = _corners + 2;
            indices[8] = 2 * _corners + 1;
            indices[9] = 1;
            indices[10] = _corners + 2;
            indices[11] = 2 * _corners + 1;
        }
    }

    _vertices.assign(vertices.begin(), vertices.end());
    _indices.assign(indices.begin(), indices.end());

    glGenVertexArrays(1, &_vao);
    glGenBuffers(1, &_vbo);
    glGenBuffers(1, &_ebo);

    glBindVertexArray(_vao);

    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * _vertices.size(), _vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, _indices.size() * sizeof(GLuint), _indices.data(), GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

Sphere::Sphere(glm::vec3 global_position, float radius, int segments) 
    : BaseGeo(global_position), _radius(radius), _segments(segments) {
    // vertices
    for (int y = 0; y <= _segments; y++)
    {
        for (int x = 0; x <= _segments; x++)
        {
            // vertices calculated under the polar coordinate
            float xSegment = (float)x / (float)_segments;
            float ySegment = (float)y / (float)_segments;
            float xPos = std::cos(xSegment * 2.0f * MY_PI) * std::sin(ySegment * MY_PI) * _radius;
            float yPos = std::cos(ySegment * MY_PI) * _radius;
            float zPos = std::sin(xSegment * 2.0f * MY_PI) * std::sin(ySegment * MY_PI) * _radius;
            _vertices.push_back(xPos);
            _vertices.push_back(yPos);
            _vertices.push_back(zPos);
        }
    }

    // indices
    for (int i = 0; i < _segments; i++)
    {
        for (int j = 0; j < _segments; j++)
        {
            _indices.push_back(i * (_segments + 1) + j);
            _indices.push_back((i + 1) * (_segments + 1) + j);
            _indices.push_back((i + 1) * (_segments + 1) + j + 1);
            _indices.push_back(i * (_segments + 1) + j);
            _indices.push_back((i + 1) * (_segments + 1) + j + 1);
            _indices.push_back(i * (_segments + 1) + j + 1);
        }
    }

    glGenVertexArrays(1, &_vao);
    glGenBuffers(1, &_vbo);
    glGenBuffers(1, &_ebo);

    glBindVertexArray(_vao);

    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * _vertices.size(), _vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, _indices.size() * sizeof(GLuint), _indices.data(), GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

Prism::Prism(glm::vec3 global_position, int corners, float radius, float height)
    : BaseGeo(global_position), _corners(corners), _radius(radius), _height(height) {
    float angle = 2.0f * MY_PI / _corners;
    std::vector<float> vertices((_corners + 1) * 2 * 3);
    std::vector<GLuint> indices(_corners * 4 * 3);

    // center of the bottom circle
    vertices[0] = 0.0f; vertices[1] = 0.0f; vertices[2] = 0.0f;
    // center of the top circle
    vertices[(_corners + 1) * 3] = 0.0f;
    vertices[(_corners + 1) * 3 + 1] = _height;
    vertices[(_corners + 1) * 3 + 2] = 0.0f;

    for (int i = 0; i < _corners; ++i) {
        // divide along the bottom circle
        vertices[3 * i + 3] = cos(angle * i) * _radius;
        vertices[3 * i + 4] = 0.0f;
        vertices[3 * i + 5] = -sin(angle * i) * _radius;
        vertices[(_corners + 1) * 3 + 3 * i + 3] = cos(angle * i) * _radius;
        vertices[(_corners + 1) * 3 + 3 * i + 4] = _height;
        vertices[(_corners + 1) * 3 + 3 * i + 5] = -sin(angle * i) * _radius;

        if (i > 0) {
            // triangle with 2 vertices along the bottom edge and 1 at the bottom center
            indices[i * 12] = 0;
            indices[i * 12 + 1] = i;
            indices[i * 12 + 2] = i + 1;
            // triangle with 2 vertices along the bottom edge and 1 at the top edge
            indices[i * 12 + 3] = _corners + 1 + i;
            indices[i * 12 + 4] = i;
            indices[i * 12 + 5] = i + 1;
            // triangle with 2 vertices along the top edge and 1 at the top center
            indices[i * 12 + 6] = _corners + 1;
            indices[i * 12 + 7] = _corners + 1 + i;
            indices[i * 12 + 8] = _corners + 1 + i + 1;
            // triangle with 2 vertices along the top edge and 1 at the bottom edge
            indices[i * 12 + 9] = i + 1;
            indices[i * 12 + 10] = _corners + 1 + i;
            indices[i * 12 + 11] = _corners + 1 + i + 1;
        }
        else if (i == 0) { // end to end
            indices[0] = 0;
            indices[1] = _corners;
            indices[2] = 1;
            indices[3] = 2 * _corners + 1;
            indices[4] = _corners;
            indices[5] = 1;
            indices[6] = _corners + 1;
            indices[7] = _corners + 2;
            indices[8] = 2 * _corners + 1;
            indices[9] = 1;
            indices[10] = _corners + 2;
            indices[11] = 2 * _corners + 1;
        }
    }

    _vertices.assign(vertices.begin(), vertices.end());
    _indices.assign(indices.begin(), indices.end());

    glGenVertexArrays(1, &_vao);
    glGenBuffers(1, &_vbo);
    glGenBuffers(1, &_ebo);

    glBindVertexArray(_vao);

    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * _vertices.size(), _vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, _indices.size() * sizeof(GLuint), _indices.data(), GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

Frust::Frust(glm::vec3 global_position, int corners, float radius1, float radius2, float height)
    : BaseGeo(global_position), _corners(corners), _radius1(radius1), _radius2(radius2), _height(height) {
    float angle = 2.0f * MY_PI / _corners;
    std::vector<float> vertices((_corners + 1) * 2 * 3);
    std::vector<GLuint> indices(_corners * 4 * 3);

    // center of the bottom circle
    vertices[0] = 0.0f; vertices[1] = 0.0f; vertices[2] = 0.0f;
    // center of the top circle
    vertices[(_corners + 1) * 3] = 0.0f;
    vertices[(_corners + 1) * 3 + 1] = _height;
    vertices[(_corners + 1) * 3 + 2] = 0.0f;

    for (int i = 0; i < _corners; ++i) {
        // divide along the bottom circle
        vertices[3 * i + 3] = cos(angle * i) * _radius1;
        vertices[3 * i + 4] = 0.0f;
        vertices[3 * i + 5] = -sin(angle * i) * _radius1;
        vertices[(_corners + 1) * 3 + 3 * i + 3] = cos(angle * i) * _radius2;
        vertices[(_corners + 1) * 3 + 3 * i + 4] = _height;
        vertices[(_corners + 1) * 3 + 3 * i + 5] = -sin(angle * i) * _radius2;

        if (i > 0) {
            // triangle with 2 vertices along the bottom edge and 1 at the bottom center
            indices[i * 12] = 0;
            indices[i * 12 + 1] = i;
            indices[i * 12 + 2] = i + 1;
            // triangle with 2 vertices along the bottom edge and 1 at the top edge
            indices[i * 12 + 3] = _corners + 1 + i;
            indices[i * 12 + 4] = i;
            indices[i * 12 + 5] = i + 1;
            // triangle with 2 vertices along the top edge and 1 at the top center
            indices[i * 12 + 6] = _corners + 1;
            indices[i * 12 + 7] = _corners + 1 + i;
            indices[i * 12 + 8] = _corners + 1 + i + 1;
            // triangle with 2 vertices along the top edge and 1 at the bottom edge
            indices[i * 12 + 9] = i + 1;
            indices[i * 12 + 10] = _corners + 1 + i;
            indices[i * 12 + 11] = _corners + 1 + i + 1;
        }
        else if (i == 0) { // end to end
            indices[0] = 0;
            indices[1] = _corners;
            indices[2] = 1;
            indices[3] = 2 * _corners + 1;
            indices[4] = _corners;
            indices[5] = 1;
            indices[6] = _corners + 1;
            indices[7] = _corners + 2;
            indices[8] = 2 * _corners + 1;
            indices[9] = 1;
            indices[10] = _corners + 2;
            indices[11] = 2 * _corners + 1;
        }
    }

    _vertices.assign(vertices.begin(), vertices.end());
    _indices.assign(indices.begin(), indices.end());

    glGenVertexArrays(1, &_vao);
    glGenBuffers(1, &_vbo);
    glGenBuffers(1, &_ebo);

    glBindVertexArray(_vao);

    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * _vertices.size(), _vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, _indices.size() * sizeof(GLuint), _indices.data(), GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}