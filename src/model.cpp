#include <iostream>
#include <limits>
#include <unordered_map>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <map>

#include "model.h"

#define SSCANF_BUFFER_SIZE (4096)

Model::Model(const std::string& filepath) {
    attrib_t attrib;
    std::vector<shape_t> shapes;

    std::string err;

    std::string::size_type index = filepath.find_last_of("/");
    std::string mtlBaseDir = filepath.substr(0, index + 1);

    std::cout << "Loading obj: " << filepath << std::endl;
    if (!LoadObj(&attrib, &shapes, &_materials, &err, filepath.c_str(), mtlBaseDir.c_str())) {
        throw std::runtime_error("load " + filepath + " failure: " + err);
    }

    if (!err.empty()) {
        std::cerr << err << std::endl;
    }

    std::vector<Vertex> vertices;
    std::vector<VertexMaterial> vertex_material;
    std::vector<uint32_t> indices;
    std::unordered_map<Vertex, uint32_t> uniqueVertices;

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};

            vertex.position.x = attrib.vertices[3 * index.vertex_index + 0];
            vertex.position.y = attrib.vertices[3 * index.vertex_index + 1];
            vertex.position.z = attrib.vertices[3 * index.vertex_index + 2];

            if (index.normal_index >= 0) {
                vertex.normal.x = attrib.normals[3 * index.normal_index + 0];
                vertex.normal.y = attrib.normals[3 * index.normal_index + 1];
                vertex.normal.z = attrib.normals[3 * index.normal_index + 2];
            }

            if (index.texcoord_index >= 0) {
                vertex.texCoord.x = attrib.texcoords[2 * index.texcoord_index + 0];
                vertex.texCoord.y = attrib.texcoords[2 * index.texcoord_index + 1];
            }

            // check if the vertex appeared before to reduce redundant data
            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
                // assume that materials for a certain object is uniform
                vertex_material.push_back(VertexMaterial(vertex, shape.mesh.material_ids[0]));
            }

            indices.push_back(uniqueVertices[vertex]);
        }
    }

    _vertices = vertices;
    _vertex_material = vertex_material;
    _indices = indices;

    computeBoundingBox();

    initGLResources();

    initBoxGLResources();

    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        cleanup();
        throw std::runtime_error("OpenGL Error: " + std::to_string(error));
    }
}

Model::Model(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
    : _vertices(vertices), _indices(indices) {

    computeBoundingBox();

    initGLResources();

    initBoxGLResources();

    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        cleanup();
        throw std::runtime_error("OpenGL Error: " + std::to_string(error));
    }
}

Model::Model(Model&& rhs) noexcept
    : _vertices(std::move(rhs._vertices)),
    _indices(std::move(rhs._indices)),
    _boundingBox(std::move(rhs._boundingBox)),
    _vao(rhs._vao), _vbo(rhs._vbo), _ebo(rhs._ebo),
    _boxVao(rhs._boxVao), _boxVbo(rhs._boxVbo), _boxEbo(rhs._boxEbo) {
    std::cerr << "Warning: Model::Model(Model&& rhs) is unsafe!" << std::endl;
    rhs._vao = 0;
    rhs._vbo = 0;
    rhs._ebo = 0;
    rhs._boxVao = 0;
    rhs._boxVbo = 0;
    rhs._boxEbo = 0;
}

Model::~Model() {
    cleanup();
}

BoundingBox Model::getBoundingBox() const {
    return _boundingBox;
}

void Model::draw() const {
    glBindVertexArray(_vao);
    glDrawElements(GL_TRIANGLES, _indices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void Model::drawBoundingBox() const {
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glBindVertexArray(_boxVao);
    glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

GLuint Model::getVao() const {
    return _vao;
}

GLuint Model::getBoundingBoxVao() const {
    return _boxVao;
}

size_t Model::getVertexCount() const {
    return _vertices.size();
}

size_t Model::getFaceCount() const {
    return _indices.size() / 3;
}

void Model::initGLResources() {
    // create a vertex array object
    glGenVertexArrays(1, &_vao);
    // create a vertex buffer object
    glGenBuffers(1, &_vbo);
    // create an element array buffer
    glGenBuffers(1, &_ebo);

    glBindVertexArray(_vao);
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferData(GL_ARRAY_BUFFER,
        sizeof(VertexMaterial) * _vertex_material.size(), _vertex_material.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        _indices.size() * sizeof(uint32_t), _indices.data(), GL_STATIC_DRAW);

    // specify layout, size of a vertex, data type, normalize, sizeof vertex array, offset of the attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexMaterial), (void*)offsetof(VertexMaterial, position));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VertexMaterial), (void*)offsetof(VertexMaterial, normal));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(VertexMaterial), (void*)offsetof(VertexMaterial, texCoord));
    glEnableVertexAttribArray(2);
    glVertexAttribIPointer(3, 1, GL_INT, sizeof(VertexMaterial), (void*)offsetof(VertexMaterial, material_id));
    glEnableVertexAttribArray(3);
    glBindVertexArray(0);
}

void Model::computeBoundingBox() {
    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float minZ = std::numeric_limits<float>::max();
    float maxX = -std::numeric_limits<float>::max();
    float maxY = -std::numeric_limits<float>::max();
    float maxZ = -std::numeric_limits<float>::max();

    for (const auto& v : _vertices) {
        minX = std::min(v.position.x, minX);
        minY = std::min(v.position.y, minY);
        minZ = std::min(v.position.z, minZ);
        maxX = std::max(v.position.x, maxX);
        maxY = std::max(v.position.y, maxY);
        maxZ = std::max(v.position.z, maxZ);
    }

    _boundingBox.min = glm::vec3(minX, minY, minZ);
    _boundingBox.max = glm::vec3(maxX, maxY, maxZ);
}

void Model::initBoxGLResources() {
    std::vector<glm::vec3> boxVertices = {
        glm::vec3(_boundingBox.min.x, _boundingBox.min.y, _boundingBox.min.z),
        glm::vec3(_boundingBox.max.x, _boundingBox.min.y, _boundingBox.min.z),
        glm::vec3(_boundingBox.min.x, _boundingBox.max.y, _boundingBox.min.z),
        glm::vec3(_boundingBox.max.x, _boundingBox.max.y, _boundingBox.min.z),
        glm::vec3(_boundingBox.min.x, _boundingBox.min.y, _boundingBox.max.z),
        glm::vec3(_boundingBox.max.x, _boundingBox.min.y, _boundingBox.max.z),
        glm::vec3(_boundingBox.min.x, _boundingBox.max.y, _boundingBox.max.z),
        glm::vec3(_boundingBox.max.x, _boundingBox.max.y, _boundingBox.max.z),
    };

    std::vector<uint32_t> boxIndices = {
        0, 1,
        0, 2,
        0, 4,
        3, 1,
        3, 2,
        3, 7,
        5, 4,
        5, 1,
        5, 7,
        6, 4,
        6, 7,
        6, 2
    };

    glGenVertexArrays(1, &_boxVao);
    glGenBuffers(1, &_boxVbo);
    glGenBuffers(1, &_boxEbo);

    glBindVertexArray(_boxVao);
    glBindBuffer(GL_ARRAY_BUFFER, _boxVbo);
    glBufferData(GL_ARRAY_BUFFER, boxVertices.size() * sizeof(glm::vec3), boxVertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _boxEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, boxIndices.size() * sizeof(uint32_t), boxIndices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), 0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void Model::cleanup() {
    if (_boxEbo) {
        glDeleteBuffers(1, &_boxEbo);
        _boxEbo = 0;
    }

    if (_boxVbo) {
        glDeleteBuffers(1, &_boxVbo);
        _boxVbo = 0;
    }

    if (_boxVao) {
        glDeleteVertexArrays(1, &_boxVao);
        _boxVao = 0;
    }

    if (_ebo != 0) {
        glDeleteBuffers(1, &_ebo);
        _ebo = 0;
    }

    if (_vbo != 0) {
        glDeleteBuffers(1, &_vbo);
        _vbo = 0;
    }

    if (_vao != 0) {
        glDeleteVertexArrays(1, &_vao);
        _vao = 0;
    }
}

bool Model::LoadObj(attrib_t* attrib, std::vector<shape_t>* shapes,
    std::vector<material_t>* materials, std::string* err,
    const char* filename, const char* mtl_basedir)
{
    attrib->vertices.clear();
    attrib->normals.clear();
    attrib->texcoords.clear();
    shapes->clear();

    std::stringstream errss;

    std::ifstream inStream(filename);
    if (!inStream) {
        errss << "Cannot open file [" << filename << "]" << std::endl;
        if (err) {
            (*err) = errss.str();
        }
        return false;
    }

    std::string baseDir;
    if (mtl_basedir) {
        baseDir = mtl_basedir;
    }

    std::vector<float> v;
    std::vector<float> vn;
    std::vector<float> vt;
    std::vector<std::vector<vertex_index> > faceGroup;
    std::string name;

    std::map<std::string, int> material_map;
    int material = -1;

    shape_t shape;

    std::string linebuf;
    while (inStream.peek() != -1) {
        safeGetline(inStream, linebuf);

        // Trim newline '\r\n' or '\n'
        if (linebuf.size() > 0) {
            if (linebuf[linebuf.size() - 1] == '\n')
                linebuf.erase(linebuf.size() - 1);
        }
        if (linebuf.size() > 0) {
            if (linebuf[linebuf.size() - 1] == '\r')
                linebuf.erase(linebuf.size() - 1);
        }

        // Skip if empty line.
        if (linebuf.empty()) {
            continue;
        }

        // Skip leading space.
        const char* token = linebuf.c_str();
        token += strspn(token, " \t");

        assert(token);
        if (token[0] == '\0') continue;  // empty line

        if (token[0] == '#') continue;  // comment line

        // vertex
        if (token[0] == 'v' && (token[1] == ' ' || token[1] == '\t')) {
            token += 2;
            float x, y, z;
            std::istringstream in(token);
            in >> x >> y >> z;
            v.push_back(x);
            v.push_back(y);
            v.push_back(z);
            continue;
        }

        // normal
        if (token[0] == 'v' && token[1] == 'n' && (token[2] == ' ' || token[2] == '\t')) {
            token += 3;
            float x, y, z;
            std::istringstream in(token);
            in >> x >> y >> z;
            vn.push_back(x);
            vn.push_back(y);
            vn.push_back(z);
            continue;
        }

        // texcoord
        if (token[0] == 'v' && token[1] == 't' && (token[2] == ' ' || token[2] == '\t')) {
            token += 3;
            float x, y;
            std::istringstream in(token);
            in >> x >> y;
            vt.push_back(x);
            vt.push_back(y);
            continue;
        }

        // face
        if (token[0] == 'f' && (token[1] == ' ' || token[1] == '\t')) {
            token += 2;
            token += strspn(token, " \t");

            std::vector<vertex_index> face;
            face.reserve(3);

            while (!((token[0] == '\r') || (token[0] == '\n') || (token[0] == '\0'))) {
                vertex_index vi(-1);
                int vsize = static_cast<int>(v.size() / 3);
                int vnsize = static_cast<int>(vn.size() / 3);
                int vtsize = static_cast<int>(vt.size() / 2);
                vi.v_idx = fixIndex(atoi((token)), vsize);
                token += strcspn((token), "/ \t\r");
                if ((token)[0] != '/') {
                    face.push_back(vi);
                    size_t n = strspn(token, " \t\r");
                    token += n;
                    continue;
                }
                token++;

                // i//k
                if (token[0] == '/') {
                    token++;
                    vi.vn_idx = fixIndex(atoi(token), vnsize);
                    token += strcspn(token, "/ \t\r");
                    face.push_back(vi);
                    size_t n = strspn(token, " \t\r");
                    token += n;
                    continue;
                }

                // i/j/k or i/j
                vi.vt_idx = fixIndex(atoi(token), vtsize);
                token += strcspn(token, "/ \t\r");
                if (token[0] != '/') {
                    face.push_back(vi);
                    size_t n = strspn(token, " \t\r");
                    token += n;
                    continue;
                }

                // i/j/k
                token++;  // skip '/'
                vi.vn_idx = fixIndex(atoi(token), vnsize);
                token += strcspn(token, "/ \t\r");
                face.push_back(vi);
                size_t n = strspn(token, " \t\r");
                token += n;
            }

            // replace with emplace_back + std::move on C++11
            faceGroup.push_back(std::vector<vertex_index>());
            faceGroup[faceGroup.size() - 1].swap(face);

            continue;
        }

        // use mtl
        if ((0 == strncmp(token, "usemtl", 6)) && (token[6] == ' ' || token[6] == '\t')) {
            char namebuf[SSCANF_BUFFER_SIZE];
            token += 7;
            std::sscanf(token, "%s", namebuf);
            int newMaterialId = -1;
            if (material_map.find(namebuf) != material_map.end()) {
                newMaterialId = material_map[namebuf];
            }
            else {
                // { error!! material not found }
            }

            if (newMaterialId != material) {
                if (!faceGroup.empty())
                {
                    // Flatten vertices and indices
                    for (size_t i = 0; i < faceGroup.size(); i++) {
                        const std::vector<vertex_index>& face = faceGroup[i];

                        vertex_index i0 = face[0];
                        vertex_index i1(-1);
                        vertex_index i2 = face[1];

                        size_t npolys = face.size();

                        // Polygon -> triangle fan conversion
                        for (size_t k = 2; k < npolys; k++) {
                            i1 = i2;
                            i2 = face[k];

                            index_t idx0, idx1, idx2;
                            idx0.vertex_index = i0.v_idx;
                            idx0.normal_index = i0.vn_idx;
                            idx0.texcoord_index = i0.vt_idx;
                            idx1.vertex_index = i1.v_idx;
                            idx1.normal_index = i1.vn_idx;
                            idx1.texcoord_index = i1.vt_idx;
                            idx2.vertex_index = i2.v_idx;
                            idx2.normal_index = i2.vn_idx;
                            idx2.texcoord_index = i2.vt_idx;

                            shape.mesh.indices.push_back(idx0);
                            shape.mesh.indices.push_back(idx1);
                            shape.mesh.indices.push_back(idx2);

                            shape.mesh.num_face_vertices.push_back(3);
                            shape.mesh.material_ids.push_back(material);
                        }
                    }
                    shape.name = name;
                    shapes->push_back(shape);
                }
                shape = shape_t();

                faceGroup.clear();
                material = newMaterialId;
            }

            continue;
        }

        // load mtl
        if ((0 == strncmp(token, "mtllib", 6)) && (token[6] == ' ' || token[6] == '\t')) {
            token += 7;

            std::vector<std::string> filenames;
            std::stringstream ss(token);
            std::string item;
            while (std::getline(ss, item, ' ')) {
                filenames.push_back(baseDir + item);
            }

            if (filenames.empty()) {
                if (err) {
                    (*err) +=
                        "WARN: Looks like empty filename for mtllib. Use default "
                        "material. \n";
                }
            }
            else {
                bool found = false;
                for (size_t s = 0; s < filenames.size(); s++) {
                    std::string err_mtl;
                    bool ok = materialFileReader(filenames[s].c_str(), materials,
                        &material_map, &err_mtl);
                    if (err && (!err_mtl.empty())) {
                        (*err) += err_mtl;  // This should be warn message.
                    }

                    if (ok) {
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    if (err) {
                        (*err) +=
                            "WARN: Failed to load material file(s). Use default "
                            "material.\n";
                    }
                }
            }

            continue;
        }

        // group name
        if (token[0] == 'g' && (token[1] == ' ' || token[1] == '\t')) {
            // flush previous face group.
            if (!faceGroup.empty())
            {
                // Flatten vertices and indices
                for (size_t i = 0; i < faceGroup.size(); i++) {
                    const std::vector<vertex_index>& face = faceGroup[i];

                    vertex_index i0 = face[0];
                    vertex_index i1(-1);
                    vertex_index i2 = face[1];

                    size_t npolys = face.size();

                    // Polygon -> triangle fan conversion
                    for (size_t k = 2; k < npolys; k++) {
                        i1 = i2;
                        i2 = face[k];

                        index_t idx0, idx1, idx2;
                        idx0.vertex_index = i0.v_idx;
                        idx0.normal_index = i0.vn_idx;
                        idx0.texcoord_index = i0.vt_idx;
                        idx1.vertex_index = i1.v_idx;
                        idx1.normal_index = i1.vn_idx;
                        idx1.texcoord_index = i1.vt_idx;
                        idx2.vertex_index = i2.v_idx;
                        idx2.normal_index = i2.vn_idx;
                        idx2.texcoord_index = i2.vt_idx;

                        shape.mesh.indices.push_back(idx0);
                        shape.mesh.indices.push_back(idx1);
                        shape.mesh.indices.push_back(idx2);

                        shape.mesh.num_face_vertices.push_back(3);
                        shape.mesh.material_ids.push_back(material);
                    }
                }
                shape.name = name;

                shapes->push_back(shape);
            }

            shape = shape_t();

            // material = -1;
            faceGroup.clear();

            std::vector<std::string> names;
            names.reserve(2);

            while (!((token[0] == '\r') || (token[0] == '\n') || (token[0] == '\0'))) {
                std::string s;
                token += strspn(token, " \t");
                size_t e = strcspn(token, " \t\r");
                s = std::string(token, &token[e]);
                token += e;
                names.push_back(s);
                token += strspn(token, " \t\r");  // skip tag
            }

            assert(names.size() > 0);

            // names[0] must be 'g', so skip the 0th element.
            if (names.size() > 1) {
                name = names[1];
            }
            else {
                name = "";
            }

            continue;
        }

        // object name
        if (token[0] == 'o' && (token[1] == ' ' || token[1] == '\t')) {
            // flush previous face group.
            if (!faceGroup.empty())
            {
                // Flatten vertices and indices
                for (size_t i = 0; i < faceGroup.size(); i++) {
                    const std::vector<vertex_index>& face = faceGroup[i];

                    vertex_index i0 = face[0];
                    vertex_index i1(-1);
                    vertex_index i2 = face[1];

                    size_t npolys = face.size();

                    // Polygon -> triangle fan conversion
                    for (size_t k = 2; k < npolys; k++) {
                        i1 = i2;
                        i2 = face[k];

                        index_t idx0, idx1, idx2;
                        idx0.vertex_index = i0.v_idx;
                        idx0.normal_index = i0.vn_idx;
                        idx0.texcoord_index = i0.vt_idx;
                        idx1.vertex_index = i1.v_idx;
                        idx1.normal_index = i1.vn_idx;
                        idx1.texcoord_index = i1.vt_idx;
                        idx2.vertex_index = i2.v_idx;
                        idx2.normal_index = i2.vn_idx;
                        idx2.texcoord_index = i2.vt_idx;

                        shape.mesh.indices.push_back(idx0);
                        shape.mesh.indices.push_back(idx1);
                        shape.mesh.indices.push_back(idx2);

                        shape.mesh.num_face_vertices.push_back(3);
                        shape.mesh.material_ids.push_back(material);
                    }
                }
                shape.name = name;

                shapes->push_back(shape);
            }

            // material = -1;
            faceGroup.clear();
            shape = shape_t();

            // @todo { multiple object name? }
            char namebuf[SSCANF_BUFFER_SIZE];
            token += 2;
            std::sscanf(token, "%s", namebuf);
            name = std::string(namebuf);

            continue;
        }
    }

    if (!faceGroup.empty())
    {
        // Flatten vertices and indices
        for (size_t i = 0; i < faceGroup.size(); i++) {
            const std::vector<vertex_index>& face = faceGroup[i];

            vertex_index i0 = face[0];
            vertex_index i1(-1);
            vertex_index i2 = face[1];

            size_t npolys = face.size();

            // Polygon -> triangle fan conversion
            for (size_t k = 2; k < npolys; k++) {
                i1 = i2;
                i2 = face[k];

                index_t idx0, idx1, idx2;
                idx0.vertex_index = i0.v_idx;
                idx0.normal_index = i0.vn_idx;
                idx0.texcoord_index = i0.vt_idx;
                idx1.vertex_index = i1.v_idx;
                idx1.normal_index = i1.vn_idx;
                idx1.texcoord_index = i1.vt_idx;
                idx2.vertex_index = i2.v_idx;
                idx2.normal_index = i2.vn_idx;
                idx2.texcoord_index = i2.vt_idx;

                shape.mesh.indices.push_back(idx0);
                shape.mesh.indices.push_back(idx1);
                shape.mesh.indices.push_back(idx2);

                shape.mesh.num_face_vertices.push_back(3);
                shape.mesh.material_ids.push_back(material);
            }
        }
        shape.name = name;
    }
    if (faceGroup.empty() || shape.mesh.indices.size()) {
        shapes->push_back(shape);
    }
    faceGroup.clear();  // for safety

    if (err) {
        (*err) += errss.str();
    }

    attrib->vertices.swap(v);
    attrib->normals.swap(vn);
    attrib->texcoords.swap(vt);

    return true;
}

bool Model::materialFileReader(const std::string& filepath,
    std::vector<material_t>* materials,
    std::map<std::string, int>* matMap,
    std::string* err)
{
    std::ifstream inStream(filepath.c_str());
    if (!inStream) {
        std::stringstream ss;
        ss << "WARN: Material file [ " << filepath << " ] not found." << std::endl;
        if (err) {
            (*err) += ss.str();
        }
        return false;
    }

    std::string warning;

    // Create a default material anyway.
    material_t material;
    material.name = "";
    for (int i = 0; i < 3; i++) {
        material.ka[i] = 0.f;
        material.kd[i] = 0.f;
        material.ks[i] = 0.f;
        material.ke[i] = 0.f;
    }
    material.illum = 0;
    material.ni = 1.f;
    material.ns = 1.f;
    material.d = 1.f;

    std::stringstream ss;

    std::string linebuf;
    while (inStream.peek() != -1) {
        safeGetline(inStream, linebuf);
        // Trim trailing whitespace.
        if (linebuf.size() > 0) {
            linebuf = linebuf.substr(0, linebuf.find_last_not_of(" \t") + 1);
        }

        // Trim newline '\r\n' or '\n'
        if (linebuf.size() > 0) {
            if (linebuf[linebuf.size() - 1] == '\n')
                linebuf.erase(linebuf.size() - 1);
        }
        if (linebuf.size() > 0) {
            if (linebuf[linebuf.size() - 1] == '\r')
                linebuf.erase(linebuf.size() - 1);
        }

        // Skip if empty line.
        if (linebuf.empty()) {
            continue;
        }

        // Skip leading space.
        const char* token = linebuf.c_str();
        token += strspn(token, " \t");

        assert(token);
        if (token[0] == '\0') continue;  // empty line

        if (token[0] == '#') continue;  // comment line

        // new mtl
        if ((0 == strncmp(token, "newmtl", 6)) && (token[6] == ' ' || token[6] == '\t')) {
            // flush previous material.
            if (!material.name.empty()) {
                matMap->insert(std::pair<std::string, int>(
                    material.name, static_cast<int>(materials->size())));
                materials->push_back(material);
            }

            // initial temporary material
            material.name = "";
            for (int i = 0; i < 3; i++) {
                material.ka[i] = 0.f;
                material.kd[i] = 0.f;
                material.ks[i] = 0.f;
                material.ke[i] = 0.f;
            }
            material.illum = 0;
            material.ni = 1.f;
            material.ns = 1.f;
            material.d = 1.f;

            // set new mtl name
            char namebuf[SSCANF_BUFFER_SIZE];
            token += 7;
            std::sscanf(token, "%s", namebuf);
            material.name = namebuf;

            continue;
        }

        // ambient
        if (token[0] == 'K' && token[1] == 'a' && (token[2] == ' ' || token[2] == '\t')) {
            token += 2;
            float r, g, b;
            std::istringstream in(token);
            in >> r >> g >> b;
            material.ka[0] = r;
            material.ka[1] = g;
            material.ka[2] = b;
            continue;
        }

        // diffuse
        if (token[0] == 'K' && token[1] == 'd' && (token[2] == ' ' || token[2] == '\t')) {
            token += 2;
            float r, g, b;
            std::istringstream in(token);
            in >> r >> g >> b;
            material.kd[0] = r;
            material.kd[1] = g;
            material.kd[2] = b;
            continue;
        }

        // specular
        if (token[0] == 'K' && token[1] == 's' && (token[2] == ' ' || token[2] == '\t')) {
            token += 2;
            float r, g, b;
            std::istringstream in(token);
            in >> r >> g >> b;
            material.ks[0] = r;
            material.ks[1] = g;
            material.ks[2] = b;
            continue;
        }

        // emission
        if (token[0] == 'K' && token[1] == 'e' && (token[2] == ' ' || token[2] == '\t')) {
            token += 2;
            float r, g, b;
            std::istringstream in(token);
            in >> r >> g >> b;
            material.ke[0] = r;
            material.ke[1] = g;
            material.ke[2] = b;
            continue;
        }

        // shininess
        if (token[0] == 'N' && token[1] == 's' && (token[2] == ' ' || token[2] == '\t')) {
            token += 2;
            float ns;
            std::istringstream in(token);
            in >> ns;
            material.ns = ns;
            continue;
        }

        // ior(index of refraction)
        if (token[0] == 'N' && token[1] == 'i' && (token[2] == ' ' || token[2] == '\t')) {
            token += 2;
            float ni;
            std::istringstream in(token);
            in >> ni;
            material.ni = ni;
            continue;
        }

        // illum model
        if (0 == strncmp(token, "illum", 5) && (token[5] == ' ' || token[5] == '\t')) {
            token += 6;
            int illum;
            std::istringstream in(token);
            in >> illum;
            material.illum = illum;
            continue;
        }

        // dissolve
        if (token[0] == 'd' && (token[1] == ' ' || token[1] == '\t')) {
            token += 1;
            float d;
            std::istringstream in(token);
            in >> d;
            material.d = d;
            continue;
        }

        // unknown parameter
        const char* _space = strchr(token, ' ');
        if (!_space) {
            _space = strchr(token, '\t');
        }
        if (_space) {
            std::ptrdiff_t len = _space - token;
            std::string key(token, static_cast<size_t>(len));
            std::string value = _space + 1;
            material.unknown_parameter.insert(
                std::pair<std::string, std::string>(key, value));
        }
    }

    // flush last material.
    matMap->insert(std::pair<std::string, int>(
        material.name, static_cast<int>(materials->size())));
    materials->push_back(material);

    warning = ss.str();

    if (!warning.empty()) {
        if (err) {
            (*err) += warning;
        }
    }

    return true;
}
