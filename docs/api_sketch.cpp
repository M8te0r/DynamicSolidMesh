#include <iostream>
#include <DynamicSolidMesh/mesh.h>

int main()
{
    using namespace MyMesh;
    // 构造 ==================================================
    Mesh origin_mesh;

    // 拷贝 ==================================================
    Mesh replica = origin_mesh.copy();

    // mesh元素的合法性由调用者判断，比如，判断返回的元素是否为空指针，方法返回的bool值是否为负，等。

    // 增 ==================================================
    VertexHandle v0 = replica.add_isolated_vertex(0.0, 0.0, 0.0);
    VertexHandle v1 = replica.add_isolated_vertex(1.0, 0.0, 0.0);
    VertexHandle v2 = replica.add_isolated_vertex(0.0, 1.0, 0.0);
    VertexHandle v3 = replica.add_isolated_vertex(0.0, 0.0, 1.0);

    CellHandle tet = replica.add_tet({v0, v1, v2, v3});

    VertexHandle v4 = replica.add_isolated_vertex(1.0, 0.0, 1.0);
    VertexHandle v5 = replica.add_isolated_vertex(1.0, 1.0, 1.0);
    VertexHandle v6 = replica.add_isolated_vertex(0.0, 1.0, 1.0);
    VertexHandle v7 = replica.add_isolated_vertex(1.0, 1.0, 0.0);

    VertexHandle v8 = replica.add_isolated_vertex(1.0, 1.0, 2.0);

    CellHandle hex = replica.add_hex({v0, v1, v7, v2, v3, v4, v5, v6});
    // add_hex 接收固定长度的 std::array<VertexHandle, 8>。

    // 删 ==================================================
    bool is_delete_cell_flag = replica.delete_cell(tet);
    bool is_delete_vertex_flag0 = replica.delete_isolated_vertex(v0); // 这些顶点仍然被hex使用，所以实际上并没有删除
    bool is_delete_vertex_flag1 = replica.delete_isolated_vertex(v2);
    bool is_delete_vertex_flag2 = replica.delete_isolated_vertex(v3);
    bool is_delete_vertex_flag3 = replica.delete_isolated_vertex(v4);
    bool is_delete_vertex_flag4 = replica.delete_isolated_vertex(v8); // v8是真正的孤立顶点，所以成功删除了


    // 改 ==================================================
    // for(DedgeHandle edge:some_edge_filter_method(replica)){
    //     // 支持动态地对网格进行修改拓扑，并自动维护
    //     VertexHandle collapse_vertex = replica.edge_collapse(edge);    // 当前暂时不要求具体实现，预留一下有待后续实现
    // }

    // 查 ==================================================
    for(const auto& ch:replica.cells()){    // range for
        const Cell *cell = replica.cell(ch);
        for(const auto& hfh:cell->halffaces){   // 元素内链式查询
            const Halfface *hf = replica.halfface(hfh);
            for(const auto& deh:hf->dedges){
                const Dedge *de = replica.dedge(deh);
                for(const auto& vh:{de->v0,de->v1}){
                    const Vertex *v = replica.vertex(vh);
                }
            }
        }
    }

    // 索引查询
    // DedgeHandle e1_handle = some_get_handle_method();  // 如果是slotmap，那么这个id应该是slotmap的id
    // const Dedge* e1 = replica.dedge(e1_handle); // 如果不存在，返回空指针
    
    // 1-ring
    std::vector<VertexHandle> v1_adj_vertices = replica.vertex_vertices(v1);   
    printf(" - v1 adjacent vertices num: %d", v1_adj_vertices.size());
    //std::vector<CellHandle> e1_adj_cells = replica.edge_cells_ccw(e1_handle);
    // ...

    return 0;

}
