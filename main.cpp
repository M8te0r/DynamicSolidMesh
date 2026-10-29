#include <iostream>
#include "mesh.h"

int main()
{
    using namespace MyMesh;
    // 构造 ==================================================
    Mesh origin_mesh;

    // 拷贝 ==================================================
    Mesh replica = origin_mesh.copy();

    // mesh元素的合法性由调用者判断，比如，判断返回的元素是否为空指针，方法返回的bool值是否为负，等。

    // 增 ==================================================
    const Vertex *v0 = replica.add_isolated_vertex(0.0, 0.0, 0.0);
    const Vertex *v1 = replica.add_isolated_vertex(1.0, 0.0, 0.0);
    const Vertex *v2 = replica.add_isolated_vertex(0.0, 1.0, 0.0);
    const Vertex *v3 = replica.add_isolated_vertex(0.0, 0.0, 1.0);

    const Cell *tet = replica.add_cell(CellType::TET, {v0, v1, v2, v3});

    const Vertex *v4 = replica.add_isolated_vertex(1.0, 0.0, 1.0);
    const Vertex *v5 = replica.add_isolated_vertex(1.0, 1.0, 1.0);
    const Vertex *v6 = replica.add_isolated_vertex(0.0, 1.0, 1.0);
    const Vertex *v7 = replica.add_isolated_vertex(1.0, 1.0, 0.0);

    const Vertex *v8 = replica.add_isolated_vertex(1.0, 1.0, 2.0);

    const Cell *hex = replica.add_cell(CellType::HEX, {v0, v1, v7, v2, v3, v4, v5, v6});
    const Cell *invalid_hex = replica.add_cell(CellType::HEX, {v0, v1, v7, v2, v3, v4, v5});    // 非法，所以返回空指针

    // 删 ==================================================
    bool is_delete_cell_flag = replica.delete_cell(tet);
    bool is_delete_vertex_flag0 = replica.delete_isolated_vertex(v0); // 这些顶点仍然被hex使用，所以实际上并没有删除
    bool is_delete_vertex_flag1 = replica.delete_isolated_vertex(v2);
    bool is_delete_vertex_flag2 = replica.delete_isolated_vertex(v3);
    bool is_delete_vertex_flag3 = replica.delete_isolated_vertex(v4);
    bool is_delete_vertex_flag4 = replica.delete_isolated_vertex(v8); // v8是真正的孤立顶点，所以成功删除了


    // 改 ==================================================
    for(auto *edge:some_edge_filter_method(replica)){
        // 支持动态地对网格进行修改拓扑，并自动维护
        const Vertex *collapse_vertex = replica.edge_collapse({v0, v1});    // 当前暂时不要求具体实现，但是后续实现
    }

    // 查 ==================================================
    for(const auto* cell:replica.cells()){    // range for
        printf(cell->id);
        for(const auto* hf:replica.cell_halffaces()){   // 元素内链式查询
            printf(hf->id);
            for(const auto* v:replica.halfface_vertices()){
                printf(v->id);
            }
        }
    }

    // 索引查询
    uint64_t e1_id = some_get_id_method();  // 如果是slotmap，那么这个id应该是slotmap的id
    Dedge *e1 = replica.dedge(e1_id); // 如果不存在，返回空指针
    
    // 1-ring
    std::vector<const Vertex *> v1_adj_vertices = replica.vertex_vertices(v1);   
    std::vector<const Cell *> e1_adj_cells = replica.edge_cells_ccw(e1);


}