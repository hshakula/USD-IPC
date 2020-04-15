#include "dcc.h"

#include <random>

PXR_NAMESPACE_OPEN_SCOPE

static std::random_device dev;
static std::mt19937 rng(dev());
static std::uniform_real_distribution<double> distribution(0.0, 1.0);

size_t RandomIndex(size_t maxSize) {
    double weight = distribution(rng);
    weight *= (maxSize - 1);
    return static_cast<size_t>(std::round(weight));
}

static Mesh GenerateTriangle() {
    Mesh triangle;
    triangle.points.push_back(GfVec3f(0, 0, 1));
    triangle.points.push_back(GfVec3f(-0.5, 0, 0));
    triangle.points.push_back(GfVec3f(0.5, 0, 0));
    triangle.faceCounts.push_back(3);
    triangle.faceIndices.push_back(0);
    triangle.faceIndices.push_back(1);
    triangle.faceIndices.push_back(2);
    return triangle;
}

Mesh triangle = GenerateTriangle();

static size_t m_randomMaterialCounter = 0;

std::pair<std::string, Material> RandomMaterial() {
    Material randomMaterial;
    randomMaterial.color = GfVec3f(distribution(rng), distribution(rng), distribution(rng));
    return {"randomMaterial" + std::to_string(m_randomMaterialCounter++), randomMaterial};
}

static size_t m_randomMeshCounter = 0;

std::pair<std::string, Mesh> RandomMesh(std::string materialId) {
    auto mesh = triangle;
    mesh.materialId = materialId;
    GfVec3f pos(distribution(rng) * 5 - 2.5f, 0.0, distribution(rng) * 5 - 2.5f);
    mesh.transform = GfMatrix4d(1).SetTranslateOnly(pos);
    return {"randomMesh" + std::to_string(m_randomMeshCounter++), mesh};
}

DCC::DCC() {
    Mesh redTriangle = triangle;
    redTriangle.materialId = "red";
    redTriangle.transform = GfMatrix4d(1).SetTranslateOnly(GfVec3f(-0.5, 0, 0));
    m_meshes["redTriangle"] = redTriangle;
    m_changes.push_back({DCC::Change::Type::Add, DCC::PrimitiveType::Mesh, "redTriangle"});

    Mesh blueTriangle = triangle;
    blueTriangle.materialId = "blue";
    blueTriangle.transform = GfMatrix4d(1).SetTranslateOnly(GfVec3f(0.5, 0, 0));
    m_meshes["blueTriangle"] = blueTriangle;
    m_changes.push_back({DCC::Change::Type::Add, DCC::PrimitiveType::Mesh, "blueTriangle"});

    Material redMaterial;
    redMaterial.color = GfVec3f(1, 0, 0);
    m_materials["red"] = redMaterial;
    m_changes.push_back({DCC::Change::Type::Add, DCC::PrimitiveType::Material, "red"});

    Material blueMaterial;
    blueMaterial.color = GfVec3f(0, 0, 1);
    m_materials["blue"] = blueMaterial;
    m_changes.push_back({DCC::Change::Type::Add, DCC::PrimitiveType::Material, "blue"});

    auto st = m_materials.emplace(RandomMaterial());
    auto stm = m_meshes.emplace(RandomMesh(st.first->first));
    m_changes.push_back({DCC::Change::Type::Add, DCC::PrimitiveType::Mesh, stm.first->first});
    m_changes.push_back({DCC::Change::Type::Add, DCC::PrimitiveType::Material, st.first->first});
}

static size_t updateCount = 0;

DCC::PrimitiveType RandomPrimitiveType() {
    if (distribution(rng) < 0.5) {
        return DCC::PrimitiveType::Mesh;
    } else {
        return DCC::PrimitiveType::Material;
    }
}

DCC::Change::Type RandomChangeType() {
    double weight = distribution(rng);
    if (weight < 0.3) {
        return DCC::Change::Type::Remove;
    } else if (weight < 0.8) {
        return DCC::Change::Type::Add;
    } else {
        return DCC::Change::Type::Edit;
    }
}

void DCC::Update() {
    updateCount++;
    static const size_t sceneChangeFrequency = 1000;
    if (updateCount % sceneChangeFrequency == 0) {
        DCC::Change change;
        change.primType = RandomPrimitiveType();
        change.type = RandomChangeType();

        if (change.primType == DCC::PrimitiveType::Mesh) {
            if (change.type == DCC::Change::Type::Remove && m_meshes.empty()) {
                change.type = DCC::Change::Type::Add;
            }

            if (change.type == DCC::Change::Type::Add) {
                std::string materialId;

                if (distribution(rng) < 0.5) {
                    auto st = m_materials.emplace(RandomMaterial());
                    materialId = st.first->first;
                    m_changes.push_back({DCC::Change::Type::Add, DCC::PrimitiveType::Material, materialId});
                } else {
                    auto it = m_materials.begin();
                    std::advance(it, RandomIndex(m_materials.size()));
                    materialId = it->first;
                }

                auto st = m_meshes.emplace(RandomMesh(materialId));
                change.primId = st.first->first;
            } else {
                auto it = m_meshes.begin();
                std::advance(it, RandomIndex(m_meshes.size()));
                change.primId = it->first;

                if (change.type == DCC::Change::Type::Remove) {
                    m_meshes.erase(it);
                }
            }
        } else {
            if (change.type == DCC::Change::Type::Remove && m_materials.empty()) {
                change.type = DCC::Change::Type::Add;
            }

            if (change.type == DCC::Change::Type::Add) {
                auto st = m_materials.emplace(RandomMaterial());
                change.primId = st.first->first;
            } else {
                auto it = m_materials.begin();
                std::advance(it, RandomIndex(m_materials.size()));
                change.primId = it->first;

                if (change.type == DCC::Change::Type::Remove) {
                    m_materials.erase(it);
                }
            }
        }
        m_changes.push_back(change);
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
