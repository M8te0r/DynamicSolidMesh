#include "mesh.h"
#include <iostream>

int main()
{
    using namespace MyMesh;
    Mesh mesh;
    const auto a = mesh.add_isolated_vertex(0,0,0), b = mesh.add_isolated_vertex(1,0,0);
    const auto c = mesh.add_isolated_vertex(0,1,0), d = mesh.add_isolated_vertex(0,0,1);
    const auto e = mesh.add_isolated_vertex(0,0,-1);
    const auto first = mesh.add_tet({a,b,c,d});
    const auto second = mesh.add_tet({a,c,b,e});
    if (first.empty() || second.empty()) return 1;
    std::cout << "vertices=" << mesh.num_vertices() << ", dedges=" << mesh.num_dedges()
              << ", halffaces=" << mesh.num_halffaces() << ", cells=" << mesh.num_cells() << '\n';
    std::cout << "neighbors of first cell=" << mesh.cell_cells(first).size() << '\n';
    Mesh copy = mesh.copy();
    std::vector<CellHandle> snapshot;
    for (auto h : copy.cells()) snapshot.push_back(h);
    for (auto h : snapshot) if (!copy.delete_cell(h)) return 2;
    std::cout << "original cells=" << mesh.num_cells() << ", copy cells=" << copy.num_cells() << '\n';
}
