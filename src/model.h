#pragma once

#include <string>
#include <vector>
#include <map>

#include <glad/glad.h>

#include "./base/bounding_box.h"
#include "./base/transform.h"
#include "./base/vertex.h"

class Model {
    // obj loader
    typedef struct {
        std::vector<float> vertices;
        std::vector<float> normals;
        std::vector<float> texcoords;
    } attrib_t;

    typedef struct {
        int vertex_index;
        int normal_index;
        int texcoord_index;
    } index_t;

    typedef struct {
        std::vector<index_t> indices;
        std::vector<unsigned char> num_face_vertices;  // The number of vertices per
                                                       // face. 3 = polygon, 4 = quad,
                                                       // ... Up to 255.
        std::vector<int> material_ids;                 // per-face material ID
    } mesh_t;

    typedef struct {
        std::string name;
        mesh_t mesh;
    } shape_t;

    typedef struct {
        std::string name;
        float ka[3];
        float kd[3];
        float ks[3];
        float ke[3];
        float ns;
        float ni;
        float d;
        int illum;
        std::map<std::string, std::string> unknown_parameter;
    } material_t;

    struct VertexMaterial {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 texCoord;
        int material_id;
        VertexMaterial() : material_id(-1){}
        VertexMaterial(const Vertex& vertex, int material)
            : position(vertex.position), normal(vertex.normal), texCoord(vertex.texCoord), material_id(material) {}
    };

    struct vertex_index {
        int v_idx, vt_idx, vn_idx;
        vertex_index() : v_idx(-1), vt_idx(-1), vn_idx(-1) {}
        explicit vertex_index(int idx) : v_idx(idx), vt_idx(idx), vn_idx(idx) {}
        vertex_index(int vidx, int vtidx, int vnidx)
            : v_idx(vidx), vt_idx(vtidx), vn_idx(vnidx) {}
    };

    bool LoadObj(attrib_t* attrib, std::vector<shape_t>* shapes,
        std::vector<material_t>* materials, std::string* err,
        const char* filename, const char* mtl_basedir);

    bool materialFileReader(const std::string& filepath,
        std::vector<material_t>* materials,
        std::map<std::string, int>* matMap,
        std::string* err);

    // See
    // http://stackoverflow.com/questions/6089231/getting-std-ifstream-to-handle-lf-cr-and-crlf
    static std::istream& safeGetline(std::istream& is, std::string& t) {
        t.clear();

        // The characters in the stream are read one-by-one using a std::streambuf.
        // That is faster than reading them one-by-one using the std::istream.
        // Code that uses streambuf this way must be guarded by a sentry object.
        // The sentry object performs various tasks,
        // such as thread synchronization and updating the stream state.

        std::istream::sentry se(is, true);
        std::streambuf* sb = is.rdbuf();

        for (;;) {
            int c = sb->sbumpc();
            switch (c) {
            case '\n':
                return is;
            case '\r':
                if (sb->sgetc() == '\n') sb->sbumpc();
                return is;
            case EOF:
                // Also handle the case when the last line has no line ending
                if (t.empty()) is.setstate(std::ios::eofbit);
                return is;
            default:
                t += static_cast<char>(c);
            }
        }
    }

    // Make index zero-base, and also support relative index.
    inline int fixIndex(int idx, int n) {
        if (idx > 0) return idx - 1;
        if (idx == 0) return 0;
        return n + idx;  // negative value = relative
    }

public:
    Model(const std::string& filepath);

    Model(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);

    Model(Model&& rhs) noexcept;

    virtual ~Model();

    GLuint getVao() const;

    GLuint getBoundingBoxVao() const;

    size_t getVertexCount() const;

    size_t getFaceCount() const;

    BoundingBox getBoundingBox() const;

    virtual void draw() const;

    virtual void drawBoundingBox() const;

    const std::vector<uint32_t>& getIndices() const { return _indices; }
    const std::vector<Vertex>& getVertices() const { return _vertices; }
    const Vertex& getVertex(int i) const { return _vertices[i]; }
public:
    Transform transform;
    std::vector<material_t> _materials;

protected:
    // vertices of the table represented in model's own coordinate
    std::vector<Vertex> _vertices;
    std::vector<VertexMaterial> _vertex_material;
    std::vector<uint32_t> _indices;

    // bounding box
    BoundingBox _boundingBox;

    // opengl objects
    GLuint _vao = 0;
    GLuint _vbo = 0;
    GLuint _ebo = 0;

    GLuint _boxVao = 0;
    GLuint _boxVbo = 0;
    GLuint _boxEbo = 0;

    void computeBoundingBox();

    void initGLResources();

    void initBoxGLResources();

    void cleanup();
};