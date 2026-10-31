#pragma once
#include "mesh.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <utility>
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#c); std::abort(); } } while(false)
namespace test {
using namespace MyMesh;
template<class T> bool has(const std::vector<T>& list,T v) { return std::find(list.begin(),list.end(),v)!=list.end(); }
inline DedgeHandle edge(const Mesh& m,VertexHandle a,VertexHandle b) {
    for(auto e:m.vertex(a)->outgoing) if(m.dedge(e)->v1==b)return e;
    return {};
}
inline void consistent(const Mesh& m) {
    unsigned nv=0,ne=0,nf=0,nc=0;
    for(auto h:m.vertices()) {
        ++nv; CHECK(m.vertex(h)); std::vector<VertexHandle> neighbors;
        for(auto e:m.vertex(h)->outgoing) {
            CHECK(m.dedge(e) && m.dedge(e)->v0==h);
            CHECK(!has(neighbors,m.dedge(e)->v1)); neighbors.push_back(m.dedge(e)->v1);
        }
        CHECK(neighbors==m.vertex_vertices(h));
    }
    for(auto h:m.dedges()) {
        ++ne; const auto* e=m.dedge(h);
        CHECK(e && m.vertex(e->v0) && m.vertex(e->v1) && e->v0!=e->v1);
        CHECK(has(m.vertex(e->v0)->outgoing,h) && !edge(m,e->v1,e->v0).empty());
        CHECK(!e->incident_halffaces.empty()); std::vector<CellHandle> owners;
        for(auto f:e->incident_halffaces) {
            CHECK(m.halfface(f)); const auto& list=m.halfface(f)->dedges;
            CHECK(std::count(list.begin(),list.end(),h)==1 && !has(owners,m.halfface(f)->cell));
            owners.push_back(m.halfface(f)->cell);
        }
        CHECK(owners==m.edge_cells(h)); const auto ordered=m.edge_cells_ccw(h);
        CHECK(ordered.size()==owners.size());
        for(auto c:owners)CHECK(std::count(ordered.begin(),ordered.end(),c)==1);
    }
    for(auto h:m.halffaces()) {
        ++nf; const auto* f=m.halfface(h); CHECK(f && m.cell(f->cell));
        const auto& list=m.cell(f->cell)->halffaces; CHECK(std::count(list.begin(),list.end(),h)==1);
        const unsigned n=f->type==FaceType::TRIANGLE?3:4;
        CHECK(f->type==FaceType::TRIANGLE || f->type==FaceType::QUADRILATERAL);
        for(unsigned i=0;i<n;++i) {
            const auto* e=m.dedge(f->dedges[i]); CHECK(e && has(e->incident_halffaces,h));
            CHECK(e->v1==m.dedge(f->dedges[(i+1)%n])->v0);
        }
        for(unsigned i=n;i<4;++i)CHECK(f->dedges[i].empty());
        if(!f->opposite.empty()) {
            const auto* o=m.halfface(f->opposite);
            CHECK(o && o->opposite==h && o->type==f->type && o->cell!=f->cell);
            bool reverse=false;
            for(unsigned start=0;start<n;++start) {
                bool same=true;
                for(unsigned i=0;i<n;++i) same= same && m.dedge(f->dedges[i])->v0==m.dedge(o->dedges[(start+n-i)%n])->v0;
                reverse=reverse||same;
            }
            CHECK(reverse);
        }
    }
    for(auto h:m.cells()) {
        ++nc; const auto* c=m.cell(h); CHECK(c);
        const unsigned n=c->type==CellType::TET?4:c->type==CellType::HEX?6:5;
        for(unsigned i=0;i<n;++i)CHECK(m.halfface(c->halffaces[i])->cell==h);
        for(unsigned i=n;i<6;++i)CHECK(c->halffaces[i].empty());
        for(auto neighbor:m.cell_cells(h))CHECK(has(m.cell_cells(neighbor),h));
    }
    CHECK(nv==m.num_vertices() && ne==m.num_dedges() && nf==m.num_halffaces() && nc==m.num_cells());
}
struct Snapshot {
    std::vector<std::pair<VertexHandle,Vertex>> vertices;
    std::vector<std::pair<DedgeHandle,Dedge>> edges;
    std::vector<std::pair<HalffaceHandle,Halfface>> faces;
    std::vector<std::pair<CellHandle,Cell>> cells;
    explicit Snapshot(const Mesh& m) {
        for(auto h:m.vertices()) vertices.emplace_back(h,*m.vertex(h));
        for(auto h:m.dedges()) edges.emplace_back(h,*m.dedge(h));
        for(auto h:m.halffaces()) faces.emplace_back(h,*m.halfface(h));
        for(auto h:m.cells()) cells.emplace_back(h,*m.cell(h));
    }
    bool matches(const Mesh& m)const {
        if(vertices.size()!=m.num_vertices()||edges.size()!=m.num_dedges()||faces.size()!=m.num_halffaces()||cells.size()!=m.num_cells())return false;
        for(const auto& x:vertices) {auto p=m.vertex(x.first);if(!p||!(p->pos.array()==x.second.pos.array()).all()||p->outgoing!=x.second.outgoing)return false;}
        for(const auto& x:edges) {auto p=m.dedge(x.first);const auto& old=x.second;if(!p||p->v0!=old.v0||p->v1!=old.v1||p->incident_halffaces!=old.incident_halffaces)return false;}
        for(const auto& x:faces) {auto p=m.halfface(x.first);const auto& old=x.second;if(!p||p->type!=old.type||p->dedges!=old.dedges||p->cell!=old.cell||p->opposite!=old.opposite)return false;}
        for(const auto& x:cells) {auto p=m.cell(x.first);const auto& old=x.second;if(!p||p->type!=old.type||p->halffaces!=old.halffaces)return false;}
        return true;
    }
};
}
