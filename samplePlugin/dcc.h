#ifndef DCC_H
#define DCC_H

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/vt/array.h>

#include <vector>
#include <map>

PXR_NAMESPACE_OPEN_SCOPE

struct Mesh {
    VtVec3fArray points;
    VtIntArray faceCounts;
    VtIntArray faceIndices;

    std::string materialId;

    GfMatrix4d transform;
};

struct Material {
    GfVec3f color;
};

class DCC {
public:
    DCC();

    void Update();

    enum class PrimitiveType {
        Mesh,
        Material
    };

    struct Change {
        enum class Type {
            Add,
            Remove,
            Edit
        };
        Type type;

        PrimitiveType primType;

        std::string primId;
    };
    void GetChanges(std::vector<Change>* changes) {
        std::swap(m_changes, *changes);
    }

    Mesh const& GetMesh(std::string const& id) { return m_meshes.at(id); }
    Material const& GetMaterial(std::string const& id) { return m_materials.at(id); }

private:
    std::map<std::string, Mesh> m_meshes;
    std::map<std::string, Material> m_materials;

    std::vector<Change> m_changes;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // DCC_H
