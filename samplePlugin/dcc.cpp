#include "dcc.h"

PXR_NAMESPACE_OPEN_SCOPE

DCC::DCC() {
    Mesh triangle;
    triangle.points.push_back(GfVec3f(0, 0, 1));
    triangle.points.push_back(GfVec3f(-0.5, 0, 0));
    triangle.points.push_back(GfVec3f(0.5, 0, 0));
    triangle.faceCounts.push_back(3);
    triangle.faceIndices.push_back(0);
    triangle.faceIndices.push_back(1);
    triangle.faceIndices.push_back(2);

    Mesh redTriangle = triangle;
    redTriangle.id = "redTriangle";
    redTriangle.materialId = "red";
    redTriangle.transform = GfMatrix4d(1).SetTranslateOnly(GfVec3f(-0.5, 0, 0));
    m_meshes.push_back(redTriangle);

    Mesh blueTriangle = triangle;
    blueTriangle.id = "blueTriangle";
    blueTriangle.materialId = "blue";
    blueTriangle.transform = GfMatrix4d(1).SetTranslateOnly(GfVec3f(0.5, 0, 0));
    m_meshes.push_back(blueTriangle);

    Material redMaterial;
    redMaterial.id = "red";
    redMaterial.color = GfVec3f(1, 0, 0);
    m_materials.push_back(redMaterial);

    Material blueMaterial;
    blueMaterial.id = "blue";
    blueMaterial.color = GfVec3f(0, 0, 1);
    m_materials.push_back(blueMaterial);
}

static size_t updateCount = 0;
void DCC::Update() {
    updateCount++;
    static const size_t sceneChangeFrequency = 1000;
    if (updateCount % sceneChangeFrequency == 0) {
        m_isChanged = true;
    }
}

bool DCC::IsSceneChanged() {
    bool isChanged = m_isChanged;
    m_isChanged = false;
    return isChanged;
}

PXR_NAMESPACE_CLOSE_SCOPE
