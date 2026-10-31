#pragma once
#include "handle.h"
#include <Eigen/Core>
#include <array>
#include <vector>
#include "slot_map.h"


#include "mesh_element_template.h"

namespace MyMesh
{

    // 确定当前需要使用的数值和容器类型
    using Real = double;
    using Vector3 = Eigen::Vector<Real, 3>;

    template <class T>
    using ElementStorage = dod::slot_map64<T>;

    // ============================================================
    // mesh 元素定义
    // ============================================================
    struct Vertex;
    struct Dedge;
    struct Halfface;
    struct Cell;

    // 从容器使用的完整 key 提取整数类型。
    using HandleSize = ElementStorage<Vertex>::key::id_type;

    using VertexHandle = Handle<Vertex, HandleSize>;
    using DedgeHandle = Handle<Dedge, HandleSize>;
    using HalffaceHandle = Handle<Halfface, HandleSize>;
    using CellHandle = Handle<Cell, HandleSize>;

    struct Vertex
    {
        Vector3 pos;
        std::vector<DedgeHandle> outgoing;
    };

    struct Dedge
    {
        VertexHandle v0;
        VertexHandle v1;
        std::vector<HalffaceHandle> incident_halffaces;
    };

    struct Halfface
    {
        FaceType type = FaceType::UNSET;
        std::array<DedgeHandle, 4> dedges;
        CellHandle cell;
        HalffaceHandle opposite;
    };

    struct Cell
    {
        CellType type = CellType::UNSET;
        std::array<HalffaceHandle, 6> halffaces;
    };
}
