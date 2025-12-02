#pragma once

#include "PCH.h"
#include <d3d11.h>

class Geometry {
    std::vector<RE::NiPoint3> positions;
    std::vector<uint16_t> indexes;
    const RE::TESObjectREFR* obj;

    void FetchVertices(const RE::BSGeometry* o3d, RE::BSGraphics::TriShape* triShape) {
        if (const uint8_t* vertexData = triShape->rawVertexData) {
            const uint32_t stride = triShape->vertexDesc.GetSize();
            const auto numPoints = ([](RE::ID3D11Buffer* reBuffer) -> UINT {
                const auto buffer = reinterpret_cast<ID3D11Buffer*>(reBuffer);
                D3D11_BUFFER_DESC bufferDesc = {};
                buffer->GetDesc(&bufferDesc);
                return bufferDesc.ByteWidth;
            })(triShape->vertexBuffer);
            const auto numPositions = numPoints / stride;
            positions.reserve(positions.size() + numPositions);
            for (uint32_t i = 0; i < numPoints; i += stride) {
                const uint8_t* currentVertex = vertexData + i;

                const auto position =
                    reinterpret_cast<const float*>(currentVertex + triShape->vertexDesc.GetAttributeOffset(
                                                       RE::BSGraphics::Vertex::Attribute::VA_POSITION));

                auto pos = RE::NiPoint3{position[0], position[1], position[2]};
                positions.push_back(pos);
            }
        }
    }

public:
    static RE::NiPoint3 Rotate(const RE::NiPoint3& A, const RE::NiPoint3& angles) {
        RE::NiMatrix3 R;
        R.SetEulerAnglesXYZ(angles);
        return R * A;
    }

    ~Geometry() = default;

    explicit Geometry(const RE::TESObjectREFR* obj) {
        this->obj = obj;
        if (const auto d3d = obj->Get3D()) {
            RE::BSVisit::TraverseScenegraphGeometries(
                d3d, [&](RE::BSGeometry* a_geometry) -> RE::BSVisit::BSVisitControl {
                    const auto& model = a_geometry->GetGeometryRuntimeData();

                    if (const auto triShape = model.rendererData) {
                        FetchVertices(a_geometry, triShape);
                    }

                    return RE::BSVisit::BSVisitControl::kContinue;
                });
        }

        if (positions.empty()) {
            auto from = obj->GetBoundMin();
            auto to = obj->GetBoundMax();

            if ((to - from).Length() < 1) {
                from = {-5, -5, -5};
                to = {5, 5, 5};
            }
            positions.emplace_back(from.x, from.y, from.z);
            positions.emplace_back(to.x, from.y, from.z);
            positions.emplace_back(to.x, to.y, from.z);
            positions.emplace_back(from.x, to.y, from.z);

            positions.emplace_back(from.x, from.y, to.z);
            positions.emplace_back(to.x, from.y, to.z);
            positions.emplace_back(to.x, to.y, to.z);
            positions.emplace_back(from.x, to.y, to.z);
        }
    }

    [[nodiscard]] std::pair<RE::NiPoint3, RE::NiPoint3> GetBoundingBox() const {
        auto min = RE::NiPoint3{ std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity() };
        auto max = RE::NiPoint3{ -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity() };

        const float scale = obj->GetScale();
        for (const auto& p : positions) {
            const auto p1 = p * scale;
            if (p1.x < min.x) min.x = p1.x; if (p1.x > max.x) max.x = p1.x;
            if (p1.y < min.y) min.y = p1.y; if (p1.y > max.y) max.y = p1.y;
            if (p1.z < min.z) min.z = p1.z; if (p1.z > max.z) max.z = p1.z;
        }

        return {min, max};
    }
};
