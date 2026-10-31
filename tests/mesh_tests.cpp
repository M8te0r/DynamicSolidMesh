#include "mesh_test_support.h"
#include <limits>
#include <type_traits>
using namespace MyMesh;
using namespace test;

static_assert(!std::is_invocable_v<decltype(&Mesh::add_hex), Mesh&,
              std::array<VertexHandle, 7>, decltype(CellOrderTemplate::hex_faces)>);
static_assert(!std::is_invocable_v<decltype(&Mesh::add_tet), Mesh&,
              std::array<VertexHandle, 5>, decltype(CellOrderTemplate::tet_faces)>);
static_assert(!std::is_invocable_v<decltype(&Mesh::add_pyramid), Mesh&,
              std::array<VertexHandle, 4>, decltype(CellOrderTemplate::pyramid_faces)>);

template <std::size_t V, std::size_t F, std::size_t E>
void custom_order(const std::array<std::array<int, E>, F>& default_order,
                  CellHandle (Mesh::*insert)(const std::array<VertexHandle, V>&,
                      const std::array<std::array<int, E>, F>&))
{
    Mesh m;
    std::array<VertexHandle, V> vertices;
    for (std::size_t i = 0; i < V; ++i)
        vertices[i] = m.add_isolated_vertex(i, i % 2, i % 3);

    // 同时改变局部顶点编号、面排列和每个面环的起点。
    auto order = default_order;
    std::reverse(vertices.begin(), vertices.end());
    std::reverse(order.begin(), order.end());
    for (auto& row : order)
    {
        for (int& index : row)
            if (index >= 0) index = int(V) - 1 - index;
        const auto end = std::find(row.begin(), row.end(), -1);
        std::rotate(row.begin(), row.begin() + 1, end);
    }

    const auto cell = (m.*insert)(vertices, order);
    CHECK(!cell.empty());
    for (std::size_t f = 0; f < F; ++f)
    {
        const auto* half = m.halfface(m.cell(cell)->halffaces[f]);
        for (std::size_t i = 0; i < E; ++i)
        {
            const int index = order[f][i];
            if (index < 0) CHECK(half->dedges[i].empty());
            else CHECK(m.dedge(half->dedges[i])->v0 == vertices[std::size_t(index)]);
        }
    }
    consistent(m);
    CHECK(m.delete_cell(cell));
    CHECK(m.num_cells() == 0 && m.num_halffaces() == 0 && m.num_dedges() == 0);
    consistent(m);
}

void basic_and_invalid() {
    Mesh m;
    CHECK(!m.vertex({})&&!m.dedge({})&&!m.halfface({})&&!m.cell({}));
    CHECK(!m.delete_cell({})&&!m.delete_isolated_vertex({}));
    CHECK(m.vertex_vertices({}).empty()&&m.edge_cells({}).empty()&&m.edge_cells_ccw({}).empty()&&m.cell_cells({}).empty());
    CHECK(m.add_isolated_vertex(std::numeric_limits<Real>::quiet_NaN(),0,0).empty());consistent(m);
    auto a=m.add_isolated_vertex(0,0,0),b=m.add_isolated_vertex(1,0,0),c=m.add_isolated_vertex(0,1,0),d=m.add_isolated_vertex(0,0,1);
    Snapshot initial(m);
    CHECK(m.add_tet({a,b,c,{}}).empty());
    CHECK(m.add_tet({a,b,c,c}).empty());CHECK(initial.matches(m));
    auto first=m.add_tet({a,b,c,d});CHECK(!first.empty());
    CHECK(m.num_dedges()==12&&m.num_halffaces()==4&&!m.delete_isolated_vertex(a));
    auto e=m.add_isolated_vertex(0,0,-1);
    auto second=m.add_tet({a,c,b,e});
    CHECK(!second.empty()&&m.num_dedges()==18&&m.num_halffaces()==8);
    CHECK(m.cell_cells(first)==std::vector<CellHandle>{second});
    // 移动两个单元共享的顶点，只有坐标改变，所有连接与 handle 保持不变。
    Snapshot moved(m);
    const Vector3 position(Real(0.125), Real(0.25), Real(0.5));
    CHECK(m.set_vertex_position(a,position.x(),position.y(),position.z()));
    for (auto& entry : moved.vertices)
        if (entry.first == a) entry.second.pos = position;
    CHECK(moved.matches(m));
    CHECK(!m.set_vertex_position({},0,0,0));
    CHECK(!m.set_vertex_position(a,std::numeric_limits<Real>::quiet_NaN(),0,0));
    CHECK(!m.set_vertex_position(a,0,std::numeric_limits<Real>::infinity(),0));
    CHECK(!m.set_vertex_position(a,0,0,std::numeric_limits<Real>::infinity()));
    CHECK(moved.matches(m));
    consistent(m);
    auto oldface=m.cell(first)->halffaces[0];
    CHECK(m.delete_cell(first)&&!m.cell(first)&&!m.halfface(oldface));
    CHECK(m.num_cells()==1&&m.num_dedges()==12&&m.num_halffaces()==4&&m.cell_cells(second).empty());
    CHECK(m.delete_isolated_vertex(d)&&!m.vertex(d)&&!m.delete_isolated_vertex(d));
    CHECK(!m.set_vertex_position(d,0,0,0));
    CHECK(m.delete_cell(second)&&m.num_dedges()==0&&m.num_halffaces()==0);
    CHECK(m.add_tet({a,b,c,d}).empty());consistent(m);
}

void edge_cell_order() {
    Mesh m;
    auto s=m.add_isolated_vertex(0,0,-1),t=m.add_isolated_vertex(0,0,1);
    auto a=m.add_isolated_vertex(1,0,0),b=m.add_isolated_vertex(0,1,0),c=m.add_isolated_vertex(-1,0,0),d=m.add_isolated_vertex(0,-1,0);
    auto c0=m.add_tet({s,t,a,b});CHECK(!c0.empty());
    auto c1=m.add_tet({s,t,b,c}),c2=m.add_tet({s,t,c,d});CHECK(!c1.empty()&&!c2.empty());
    auto forward=edge(m,s,t),reverse=edge(m,t,s);
    CHECK((m.edge_cells_ccw(forward)==std::vector<CellHandle>{c0,c1,c2}));
    CHECK((m.edge_cells_ccw(reverse)==std::vector<CellHandle>{c2,c1,c0}));
    auto c3=m.add_tet({s,t,d,a});CHECK(!c3.empty());
    CHECK((m.edge_cells_ccw(forward)==std::vector<CellHandle>{c0,c1,c2,c3}));
    CHECK((m.edge_cells_ccw(reverse)==std::vector<CellHandle>{c0,c3,c2,c1}));
    CHECK(m.delete_cell(c1));CHECK((m.edge_cells_ccw(forward)==std::vector<CellHandle>{c2,c3,c0}));consistent(m);
}

void hybrid() {
    Mesh m;std::vector<VertexHandle> v;
    for(auto p:{Vector3(0,0,0),Vector3(1,0,0),Vector3(1,1,0),Vector3(0,1,0),
                Vector3(0,0,1),Vector3(1,0,1),Vector3(1,1,1),Vector3(0,1,1),
                Vector3(.5,.5,2),Vector3(.5,-1,1.5)})v.push_back(m.add_isolated_vertex(p.x(),p.y(),p.z()));
    auto hex=m.add_hex({v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7]});
    auto pyramid_order=CellOrderTemplate::pyramid_faces;
    std::reverse(pyramid_order.begin(),pyramid_order.end());
    auto pyramid=m.add_pyramid({v[4],v[5],v[6],v[7],v[8]},pyramid_order);
    auto tet=m.add_tet({v[9],v[4],v[8],v[5]});
    CHECK(!hex.empty()&&!pyramid.empty()&&!tet.empty());CHECK(m.num_halffaces()==15&&m.num_dedges()==38);
    CHECK(m.cell_cells(pyramid).size()==2);consistent(m);
    CHECK(m.delete_cell(tet)&&m.delete_cell(pyramid)&&m.delete_cell(hex));
    CHECK(m.num_vertices()==10&&m.num_dedges()==0);consistent(m);
}

void interior_vertex_and_cavity() {
    Mesh m;VertexHandle v[4][4][4];CellHandle c[3][3][3];
    for(int z=0;z<4;++z)for(int y=0;y<4;++y)for(int x=0;x<4;++x)v[x][y][z]=m.add_isolated_vertex(x,y,z);
    for(int z=0;z<3;++z)for(int y=0;y<3;++y)for(int x=0;x<3;++x){
        c[x][y][z]=m.add_hex({v[x][y][z],v[x+1][y][z],v[x+1][y+1][z],v[x][y+1][z],
            v[x][y][z+1],v[x+1][y][z+1],v[x+1][y+1][z+1],v[x][y+1][z+1]});CHECK(!c[x][y][z].empty());
    }
    consistent(m);CHECK(m.cell_cells(c[1][1][1]).size()==6);CHECK(m.delete_cell(c[1][1][1]));consistent(m);
    CHECK(!m.add_hex({v[1][1][1],v[2][1][1],v[2][2][1],v[1][2][1],v[1][1][2],v[2][1][2],v[2][2][2],v[1][2][2]}).empty());
    CHECK(!m.cell(c[1][1][1]));consistent(m);
}

void copies_holes_and_growth() {
    Mesh m;std::vector<CellHandle> cells;std::vector<VertexHandle> first;
    for(int i=0;i<160;++i){
        auto a=m.add_isolated_vertex(3*i,0,0),b=m.add_isolated_vertex(3*i+1,0,0),c=m.add_isolated_vertex(3*i,1,0),d=m.add_isolated_vertex(3*i,0,1);
        if(i==0)first={a,b,c,d};cells.push_back(m.add_tet({a,b,c,d}));CHECK(!cells.back().empty());
    }
    for(std::size_t i=0;i<cells.size();i+=2)CHECK(m.delete_cell(cells[i]));
    Snapshot snap(m);Mesh copy=m.copy();CHECK(snap.matches(copy)&&copy.vertex(first[0])!=m.vertex(first[0]));
    auto a=m.add_tet({first[0],first[1],first[2],first[3]});
    auto b=copy.add_tet({first[0],first[1],first[2],first[3]});
    CHECK(!a.empty()&&!b.empty()&&!copy.cell(cells[0]));
    consistent(m);consistent(copy);
    CHECK(copy.delete_cell(b)&&m.cell(a));
    Mesh assigned;assigned=m;Snapshot complete(m);CHECK(complete.matches(assigned));
    auto* self=&assigned;assigned=*self;CHECK(complete.matches(assigned));
    Mesh moved(std::move(assigned));CHECK(complete.matches(moved)&&assigned.num_vertices()==0&&assigned.num_cells()==0);
    assigned=std::move(moved);assigned=std::move(*self);CHECK(complete.matches(assigned)&&moved.num_cells()==0);
    CHECK(!moved.add_isolated_vertex(0,0,0).empty());
    const auto* address=m.vertex(first[0]);for(int i=0;i<4200;++i)m.add_isolated_vertex(i,2,3);
    CHECK(m.vertex(first[0])==address);consistent(m);
}
int main()
{
    basic_and_invalid();
    edge_cell_order();
    hybrid();
    interior_vertex_and_cavity();
    copies_holes_and_growth();
    custom_order(CellOrderTemplate::hex_faces, &Mesh::add_hex);
    custom_order(CellOrderTemplate::tet_faces, &Mesh::add_tet);
    custom_order(CellOrderTemplate::pyramid_faces, &Mesh::add_pyramid);
    std::puts("mesh_tests: passed");
}
