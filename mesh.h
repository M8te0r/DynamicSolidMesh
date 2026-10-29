#include <slot_map.h>
#include <Eigen/Core>
namespace MyMesh
{
    enum class FaceType{
        UNSET,
        TRIANGLE,
        QUADRILATERAL
    };

    enum class CellType{
        UNSET,
        TET,
        HEX,
        PYRAMID
    };

    namespace CellOrderTemplate{
        constexpr int tet_faces[4][3] = {
            {1, 2, 3},
            {0, 3, 2},
            {0, 1, 3},
            {0, 2, 1}};
    }

    // ============================================================
    // mesh 元素定义
    // ============================================================
    struct Vertex;
    struct Dedge;
    struct Halfface;
    struct Cell;

    struct Vertex
    {
        Eigen::Vector3d pos;
    };

    struct Dedge{
        Vertex *v0 = nullptr;
        Vertex *v1 = nullptr;
    };

    struct Halfface{
        FaceType type = FaceType::UNSET;
        std::array<Dedge*,4> dedges{}; // 对齐之后的指针，一个face最多4个顶点
        Halfface *opposite = nullptr;
        Cell *cell = nullptr;
    };

    struct Cell{
        CellType type = CellType::UNSET;
        std::array<Halfface*,6> hfs{}; // 对齐之后的指针，一个cell最多6个面，outward orientation
    };

    // ============================================================
    // mesh本体类
    // ============================================================

    class Mesh
    {
        using SizeType = uint32_t;

        // 使用uint32_t存储元素个数，所以使用uint64_t存储对应的键位
        using VertexStorage = dod::slot_map64<Vertex>;
        using DedgeStorage = dod::slot_map64<Dedge>;
        using HalffaceStorage = dod::slot_map64<Halfface>;
        using CellStorage = dod::slot_map64<Cell>;

        using VertexKey = VertexStorage::key;
        using DedgeKey = DedgeStorage::key;
        using HalffaceKey = HalffaceStorage::key;
        using CellKey = CellStorage::key;

    public:
        Mesh() = default;
        ~Mesh() = default;

    public:
        // 基本信息
        SizeType num_vertices() const;
        SizeType num_dedges() const;
        SizeType num_halffaces() const;
        SizeType num_cells() const;

        // range for 遍历
        vertices();
        dedges();
        halffaces();
        cells();

        // 索引遍历
        vertex(VertexKey key);
        dedge(DedgeKey key);
        halfface(HalffaceKey key);
        cell(CellKey key);

    public:
        // 查询

        // 邻接 1-ring 
        std::vector<const Vertex *> vertex_vertices(const Vertex *v) const; // vertex

        std::vector<const Cell *> edge_cells(const Dedge *e) const; // edge
        std::vector<const Cell *> edge_cells_ccw(const Dedge *e) const;

        std::vector<const Cell*> cell_cells(const Cell* c) const;   // cell



        //

    private:
        VertexStorage vertices_;
        DedgeStorage dedges_;
        HalffaceStorage halffaces_;
        CellStorage cells_;

        std::unordered_map
    };
}