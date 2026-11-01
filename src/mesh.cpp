#include <DynamicSolidMesh/mesh.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <utility>

namespace MyMesh
{
namespace
{
    using Id = HandleSize;

    template <class T> 
    void erase_value(std::vector<T>& values, T value)
    {
        values.erase(std::remove(values.begin(), values.end(), value), values.end());
    }

    template <class T> 
    bool reversed_cycle(const std::vector<T>& a, const std::vector<T>& b)
    {
        if (a.size() != b.size()) return false;
        for (std::size_t start = 0; start < b.size(); ++start)
        {
            bool same = true;
            for (std::size_t i = 0; i < a.size(); ++i)
                same = same && a[i] == b[(start + b.size() - i) % b.size()];
            if (same) return true;
        }
        return false;
    }

}

Mesh::SizeType Mesh::num_vertices() const { return vertices_.size(); }
Mesh::SizeType Mesh::num_dedges() const { return dedges_.size(); }
Mesh::SizeType Mesh::num_halffaces() const { return halffaces_.size(); }
Mesh::SizeType Mesh::num_cells() const { return cells_.size(); }

const Vertex* Mesh::vertex(VertexHandle h) const { return vertices_.get(VertexStorage::key{h.raw_}); }
const Dedge* Mesh::dedge(DedgeHandle h) const { return dedges_.get(DedgeStorage::key{h.raw_}); }
const Halfface* Mesh::halfface(HalffaceHandle h) const { return halffaces_.get(HalffaceStorage::key{h.raw_}); }
const Cell* Mesh::cell(CellHandle h) const { return cells_.get(CellStorage::key{h.raw_}); }

HandleRange<Mesh::VertexStorage, VertexHandle> Mesh::vertices() const noexcept { return HandleRange<VertexStorage, VertexHandle>{vertices_}; }
HandleRange<Mesh::DedgeStorage, DedgeHandle> Mesh::dedges() const noexcept { return HandleRange<DedgeStorage, DedgeHandle>{dedges_}; }
HandleRange<Mesh::HalffaceStorage, HalffaceHandle> Mesh::halffaces() const noexcept { return HandleRange<HalffaceStorage, HalffaceHandle>{halffaces_}; }
HandleRange<Mesh::CellStorage, CellHandle> Mesh::cells() const noexcept { return HandleRange<CellStorage, CellHandle>{cells_}; }

void Mesh::swap(Mesh& other) noexcept
{
    vertices_.swap(other.vertices_);
    dedges_.swap(other.dedges_);
    halffaces_.swap(other.halffaces_);
    cells_.swap(other.cells_);
}

Mesh& Mesh::operator=(const Mesh& other)
{
    if (this != &other) { Mesh copy(other); swap(copy); }
    return *this;
}

Mesh::Mesh(Mesh&& other) : Mesh() { swap(other); }
Mesh& Mesh::operator=(Mesh&& other)
{
    if (this != &other) { Mesh moved(std::move(other)); swap(moved); }
    return *this;
}
Mesh Mesh::copy() const { return Mesh(*this); }

bool Mesh::set_vertex_position(VertexHandle handle, Real x, Real y, Real z)
{
    auto* v = vertices_.get(VertexStorage::key{handle.raw_});
    if (!v || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return false;
    v->pos = Vector3(x, y, z);
    return true;
}

VertexHandle Mesh::add_isolated_vertex(Real x, Real y, Real z)
{
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return {};
    Vertex value;
    value.pos = Vector3(x, y, z);
    return VertexHandle{vertices_.emplace(std::move(value)).raw};
}

bool Mesh::delete_isolated_vertex(VertexHandle handle)
{
    const auto* v = vertex(handle);
    if (!v || !v->outgoing.empty()) return false;
    vertices_.erase(VertexStorage::key{handle.raw_});
    return true;
}

DedgeHandle Mesh::find_dedge(VertexHandle source, VertexHandle target) const
{
    const auto* v = vertex(source);
    if (v)
        for (auto h : v->outgoing)
            if (dedge(h)->v1 == target) return h;
    return {};
}

HalffaceHandle Mesh::find_opposite(const std::vector<VertexHandle>& face) const
{
    const auto edge = find_dedge(face[1], face[0]);
    if (edge.empty()) return {};
    for (auto h : dedge(edge)->incident_halffaces)
    {
        std::vector<VertexHandle> loop;
        for (auto e : halfface(h)->dedges)
            if (!e.empty()) loop.push_back(dedge(e)->v0);
        if (reversed_cycle(face, loop)) return h;
    }
    return {};
}

template <std::size_t V, std::size_t F, std::size_t E>
CellHandle Mesh::insert_cell(CellType type, const std::array<VertexHandle, V>& vertices,
                             const std::array<std::array<int, E>, F>& order)
{
    for (std::size_t i = 0; i < V; ++i)
        if (!vertex(vertices[i]) ||
            std::find(vertices.begin(), vertices.begin() + i, vertices[i]) != vertices.begin() + i)
            return {};

    std::vector<VertexHandle> face;
    face.reserve(E);
    Cell value;
    value.type = type;
    const auto handle = CellHandle{cells_.emplace(value).raw};
    auto* added = cells_.get(CellStorage::key{handle.raw_});
    for (std::size_t f = 0; f < F; ++f)
    {
        face.clear();
        for (int index : order[f])
            if (index >= 0) face.push_back(vertices[std::size_t(index)]);
        Halfface half;
        half.type = face.size() == 3 ? FaceType::TRIANGLE : FaceType::QUADRILATERAL;
        half.cell = handle;
        half.opposite = find_opposite(face);

        // 有向边全局共享；不存在时创建并登记到 source 的 outgoing。
        for (std::size_t i = 0; i < face.size(); ++i)
        {
            const auto source = face[i], target = face[(i + 1) % face.size()];
            auto edge = find_dedge(source, target);
            if (edge.empty())
            {
                edge = DedgeHandle{dedges_.emplace(Dedge{source, target, {}}).raw};
                vertices_.get(VertexStorage::key{source.raw_})->outgoing.push_back(edge);
            }
            half.dedges[i] = edge;
        }

        // 每个单元拥有自己的半面，并维护边关联和共享面的 opposite。
        const auto h = HalffaceHandle{halffaces_.emplace(half).raw};
        added->halffaces[f] = h;
        for (std::size_t i = 0; i < face.size(); ++i)
            dedges_.get(DedgeStorage::key{half.dedges[i].raw_})->incident_halffaces.push_back(h);
        if (!half.opposite.empty())
            halffaces_.get(HalffaceStorage::key{half.opposite.raw_})->opposite = h;
    }
    return handle;
}

CellHandle Mesh::add_hex(const std::array<VertexHandle, 8>& vertices,
                         const std::array<std::array<int, 4>, 6>& order)
{
    return insert_cell(CellType::HEX, vertices, order);
}

CellHandle Mesh::add_tet(const std::array<VertexHandle, 4>& vertices,
                         const std::array<std::array<int, 3>, 4>& order)
{
    return insert_cell(CellType::TET, vertices, order);
}

CellHandle Mesh::add_pyramid(const std::array<VertexHandle, 5>& vertices,
                             const std::array<std::array<int, 4>, 5>& order)
{
    return insert_cell(CellType::PYRAMID, vertices, order);
}

bool Mesh::delete_cell(CellHandle handle)
{
    if (!cell(handle)) return false;
    for (auto h : cell(handle)->halffaces)
    {
        if (h.empty()) continue;
        const auto* f = halfface(h);
        if (!f->opposite.empty())
            halffaces_.get(HalffaceStorage::key{f->opposite.raw_})->opposite = {};
        for (auto e : f->dedges)
        {
            if (e.empty()) continue;
            auto* edge = dedges_.get(DedgeStorage::key{e.raw_});
            erase_value(edge->incident_halffaces, h);
            if (edge->incident_halffaces.empty())
            {
                erase_value(vertices_.get(VertexStorage::key{edge->v0.raw_})->outgoing, e);
                dedges_.erase(DedgeStorage::key{e.raw_});
            }
        }
        halffaces_.erase(HalffaceStorage::key{h.raw_});
    }
    cells_.erase(CellStorage::key{handle.raw_});
    return true;
}

std::vector<VertexHandle> Mesh::vertex_vertices(VertexHandle handle) const
{
    std::vector<VertexHandle> result;
    if (const auto* v = vertex(handle))
    {
        result.reserve(v->outgoing.size());
        for (auto e : v->outgoing) result.push_back(dedge(e)->v1);
    }
    return result;
}

std::vector<CellHandle> Mesh::edge_cells(DedgeHandle handle) const
{
    std::vector<CellHandle> result;
    if (const auto* e = dedge(handle))
    {
        result.reserve(e->incident_halffaces.size());
        for (auto f : e->incident_halffaces) result.push_back(halfface(f)->cell);
    }
    return result;
}

std::vector<CellHandle> Mesh::edge_cells_ccw(DedgeHandle handle) const
{
    std::vector<CellHandle> result;
    const auto* e = dedge(handle);
    if (!e) return result;
    std::map<Id, CellHandle> next;
    std::set<Id> has_predecessor;
    for (auto h : e->incident_halffaces)
    {
        const auto* f = halfface(h);
        const auto successor = f->opposite.empty() ? CellHandle{} : halfface(f->opposite)->cell;
        next.emplace(f->cell.raw_, successor);
        if (!successor.empty()) has_predecessor.insert(successor.raw_);
    }
    if (next.empty()) return result;
    Id start = next.begin()->first;
    for (const auto& entry : next)
        if (!has_predecessor.count(entry.first)) { start = entry.first; break; }
    auto current = CellHandle{start};
    do
    {
        const auto it = next.find(current.raw_);
        if (it == next.end() || result.size() == next.size()) return {};
        result.push_back(current);
        current = it->second;
    } while (!current.empty() && current.raw_ != start);
    if (result.size() != next.size()) return {};
    return result;
}

std::vector<CellHandle> Mesh::cell_cells(CellHandle handle) const
{
    std::vector<CellHandle> result;
    if (const auto* c = cell(handle))
        for (auto h : c->halffaces)
            if (!h.empty() && !halfface(h)->opposite.empty())
            {
                const auto neighbor = halfface(halfface(h)->opposite)->cell;
                if (std::find(result.begin(), result.end(), neighbor) == result.end()) result.push_back(neighbor);
            }
    return result;
}
}
