#pragma once
#include "mesh_element.h"
#include <array>

namespace MyMesh
{

    // ============================================================
    // mesh本体类
    // ============================================================

    class Mesh
    {
        using VertexStorage   = ElementStorage<Vertex>;
        using DedgeStorage    = ElementStorage<Dedge>;
        using HalffaceStorage = ElementStorage<Halfface>;
        using CellStorage     = ElementStorage<Cell>;

    public:
        Mesh() = default;
        ~Mesh() = default;
        Mesh(const Mesh&) = default;
        Mesh& operator=(const Mesh& other);
        Mesh(Mesh&& other);
        Mesh& operator=(Mesh&& other);

    public:
        using SizeType = VertexStorage::size_type;

        // 基本信息
        SizeType num_vertices() const;
        SizeType num_dedges() const;
        SizeType num_halffaces() const;
        SizeType num_cells() const;

        // 无效 handle 返回空 handle / false；插入时也检查顶点是否重复。
        // 调用方负责保证 order 合法，以及增删后的网格仍为共形流形。
        // 不提供内存分配异常时的回滚保证。
        VertexHandle add_isolated_vertex(Real x, Real y, Real z);
        CellHandle add_hex(const std::array<VertexHandle, 8>& vertices,
                           const std::array<std::array<int, 4>, 6>& order = CellOrderTemplate::hex_faces);
        CellHandle add_tet(const std::array<VertexHandle, 4>& vertices,
                           const std::array<std::array<int, 3>, 4>& order = CellOrderTemplate::tet_faces);
        CellHandle add_pyramid(const std::array<VertexHandle, 5>& vertices,
                               const std::array<std::array<int, 4>, 5>& order = CellOrderTemplate::pyramid_faces);
        bool delete_isolated_vertex(VertexHandle handle);
        bool delete_cell(CellHandle handle); // 清理无用边和半面，保留孤立顶点。

        // range for 遍历；增删前应先收集 handle 快照。
        HandleRange<VertexStorage, VertexHandle> vertices() const noexcept;
        HandleRange<DedgeStorage, DedgeHandle> dedges() const noexcept;
        HandleRange<HalffaceStorage, HalffaceHandle> halffaces() const noexcept;
        HandleRange<CellStorage, CellHandle> cells() const noexcept;

        // 元素访问
        const Vertex *vertex(VertexHandle handle) const;
        const Dedge *dedge(DedgeHandle handle) const;
        const Halfface *halfface(HalffaceHandle handle) const;
        const Cell *cell(CellHandle handle) const;

        // 复制
        Mesh copy() const;

        // 修改
        bool set_vertex_position(VertexHandle handle, Real x, Real y, Real z);

    public:
        // 邻接 1-ring
        std::vector<VertexHandle> vertex_vertices(VertexHandle handle) const; // vertex

        std::vector<CellHandle> edge_cells(DedgeHandle handle) const; // edge
        // 正向模板下，从 target 看向 source 为逆时针。边界返回开链，内部返回环（不重复首项）。
        std::vector<CellHandle> edge_cells_ccw(DedgeHandle handle) const;

        std::vector<CellHandle> cell_cells(CellHandle handle) const; // cell

    private:
        template <std::size_t V, std::size_t F, std::size_t E>
        CellHandle insert_cell(CellType type, const std::array<VertexHandle, V>& vertices,
                               const std::array<std::array<int, E>, F>& order);
        DedgeHandle find_dedge(VertexHandle source, VertexHandle target) const;
        HalffaceHandle find_opposite(const std::vector<VertexHandle>& face) const;
        void swap(Mesh& other) noexcept;

    private:
        VertexStorage vertices_;
        DedgeStorage dedges_;
        HalffaceStorage halffaces_;
        CellStorage cells_;
    };
}
