#ifndef DCC_H
#define DCC_H

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/vt/array.h>

#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

struct Mesh {
    VtVec3fArray points;
    VtIntArray faceCounts;
    VtIntArray faceIndices;

    std::string id;

    std::string materialId;

    GfMatrix4d transform;
};

struct Material {
    std::string id;

    GfVec3f color;
};

class DCC {
public:
    DCC();

    void Update();
    bool IsSceneChanged();

    std::vector<Mesh> const& GetMeshes() const { return m_meshes; }
    std::vector<Material> const& GetMaterials() const { return m_materials; }

private:
    bool m_isChanged = true;
    std::vector<Mesh> m_meshes;
    std::vector<Material> m_materials;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // DCC_H
