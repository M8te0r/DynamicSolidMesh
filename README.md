# DynamicSolidMesh

一个 C++17 混合单元体积网格库，提供元素计数、孤立顶点与单元的增删、
handle 查询和遍历、深拷贝，以及顶点/边/单元邻接查询。
支持 TET、HEX、PYRAMID，使用项目中的 `dod::slot_map64` 存储。

## 在其他 CMake 项目中使用

将本仓库放在父项目的 `DynamicSolidMesh/` 目录中，然后在父项目的 `CMakeLists.txt` 中加入：

```cmake
add_subdirectory(DynamicSolidMesh)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE DynamicSolidMesh::DynamicSolidMesh)
```

`add_subdirectory` 创建库目标，`target_link_libraries` 将库及其头文件路径、第三方依赖和
C++17 要求传递给使用者，无需手动设置 include 路径或添加 `mesh.cpp`。

在父项目的源文件中：

```cpp
#include <DynamicSolidMesh/mesh.h>

int main()
{
    MyMesh::Mesh mesh;
    auto vertex = mesh.add_isolated_vertex(0, 0, 0);
    return mesh.set_vertex_position(vertex, 1, 2, 3) ? 0 : 1;
}
```

库以静态库形式构建，并开启 PIC，可链接进可执行文件或父项目的动态库。
`DynamicSolidMesh::DynamicSolidMesh` 是公开链接名称，实际构建目标为 `dynamic_solid_mesh`。
构建只定义库目标，不添加示例、测试或基准程序，也不修改父项目的全局编译选项。

若父项目已提供 `Eigen3::Eigen`，库会复用该目标；否则使用仓库中的 `third_party/eigen/` 子模块。
使用附带 Eigen 时，首次获取仓库后需初始化子模块：

```sh
git submodule update --init --recursive
```

## 目录结构与独立构建

```text
DynamicSolidMesh/
├── CMakeLists.txt
├── include/DynamicSolidMesh/  # 公开头文件
│   ├── mesh.h
│   ├── mesh_element.h
│   ├── mesh_element_template.h
│   └── handle.h
├── src/mesh.cpp               # 库实现
├── third_party/
│   ├── SlotMap/slot_map.h     # dod 容器
│   └── eigen/                # Eigen Git 子模块
└── docs/api_sketch.cpp        # 原 main.cpp 接口草稿，不参与构建
```

也可以独立编译库：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
```

## 顶点编号和面方向

坐标数值类型集中定义在 `include/DynamicSolidMesh/mesh_element.h`：

```cpp
using Real = double;
using Vector3 = Eigen::Vector<Real, 3>;
```

`Vertex::pos` 使用 `Vector3`，新增和修改顶点坐标的参数均使用 `Real`。
修改 `Real` 后重新编译库与调用方，即可统一切换坐标精度。

```cpp
auto v = mesh.add_isolated_vertex(0, 0, 0);
bool updated = mesh.set_vertex_position(v, 1, 2, 3);
```

修改坐标不会改变 handle 或连接关系。无效 handle、NaN 或无穷坐标返回 `false`，
原坐标保持不变；成功返回 `true`。

所有面模板见 `include/DynamicSolidMesh/mesh_element_template.h`。
调用方应提供正向单元编号，面环从单元外侧看为逆时针。

- TET：参考坐标依次为 `(0,0,0), (1,0,0), (0,1,0), (0,0,1)`。
- HEX：底层 `0,1,2,3` 为 `(0,0,0), (1,0,0), (1,1,0), (0,1,0)`；
  上层 `4,5,6,7` 为对应点加 `(0,0,1)`。
- PYRAMID：底面 `0,1,2,3` 同 HEX，`4` 为顶点，例如 `(.5,.5,1)`。

插入接口按单元类型分开，顶点数组的长度由参数类型确定：

```cpp
CellHandle add_hex(const std::array<VertexHandle, 8>& vertices,
                   const std::array<std::array<int, 4>, 6>& order = CellOrderTemplate::hex_faces);
CellHandle add_tet(const std::array<VertexHandle, 4>& vertices,
                   const std::array<std::array<int, 3>, 4>& order = CellOrderTemplate::tet_faces);
CellHandle add_pyramid(const std::array<VertexHandle, 5>& vertices,
                       const std::array<std::array<int, 4>, 5>& order = CellOrderTemplate::pyramid_faces);
```

例如 `mesh.add_tet({a, b, c, d})`，或 `mesh.add_hex(vertices, custom_order)`。
`order` 中的整数是输入顶点数组的局部下标。每行定义一个有向面环；行顺序决定
`Cell::halffaces` 的顺序，环内顺序决定 `Halfface::dedges` 的顺序。
PYRAMID 的三角面以末尾 `-1` 补齐到四项，底面行可以放在任意位置。

调用方负责提供该单元类型的合法、有向闭合模板，不对自定义模板执行下标或结构检查。
顶点 handle 必须有效且互不重复。`std::array` 的花括号初始化若少给元素，剩余项会成为
空 handle，仍会被有效性检查拒绝。连接通过相同的顶点 handle 表达，坐标重合不会自动焊接。

## 存储及查询约定

- `Dedge::v0` 是 source，`v1` 是 target。每个有向顶点对最多存储一条 dedge。
  一个物理边通常对应正反两条 dedge，所以单个 TET 的 `num_dedges()` 为 12。
- `Vertex::outgoing` 存储以该顶点为 source 的 dedge。
- `Dedge::incident_halffaces` 记录使用这个有向边的半面。
- 每个半面只属于一个单元。内部面有两个方向相反的半面，`opposite` 互相指向。
  边界面只有一个半面，其 `opposite` 为空，不创建虚拟外部单元。
- `Cell::halffaces` 的有效长度为 TET 4、HEX 6、PYRAMID 5；
  `Halfface::dedges` 的有效长度为三角面 3、四边面 4。**剩余位置为空 handle，遍历时必须跳过。**
- 查询失效/空 handle 返回 `nullptr`；邻接查询返回空 vector。
  邻接查询沿已维护关系访问，不遍历整个网格。
- `edge_cells_ccw` 根据面连接排序。在正向几何编号下，从 target 看向 source 是逆时针。
  内部边返回一个环的单次遍历，不重复首项；起点取最小底层 key。
  边界边返回从一个边界端到另一个边界端的开链。反转 dedge 会反转次序，闭环允许循环移位。
- `edge_cells`、`vertex_vertices`、`cell_cells` 不承诺几何排序。
- 编辑前先从 range 收集 handle 快照；不要在活动 range-for 中增删容器。

## 拓扑合法性与编辑

调用方负责保证增删后的网格仍为共形流形，包括共享面的方向、面关联的单元数量，
以及边和顶点邻域的流形性。Mesh 不进行局部邻域重建、重复单元检测或流形检查，
只按照给定模板与已有连接关系维护元素。网格可以有多个互不接触的连通分量和孤立顶点。

删除单元会同步断开 opposite、清理无用半面与有向边，并更新相关邻接；顶点保留。
只有没有 outgoing 的顶点才能通过 `delete_isolated_vertex` 删除。

除拒绝非有限顶点坐标外，也不验证体积/Jacobian、退化、
四边面平面性、几何自交、单元重叠或坐标重合形成的非共形接口。
这些几何要求需要单独定义；逆时针的几何含义以正确的正向编号为前提。

## 编辑与复制

无效 handle 或插入单元中的重复顶点会在修改之前被拒绝，返回空 handle / `false`。
基本检查通过后，直接创建或删除元素并更新连接关系；操作成功不代表通过了拓扑合法性验证。
当前不实现事务或异常回滚，不保证内存分配失败后的网格状态。

`copy()` 和拷贝构造深拷贝存储并保留 handle 值，所以副本内部连接不需要重新编号。
拷贝赋值采用整体 copy-and-swap；移动后源 Mesh 为空且可以继续使用。
handle 不检测 Mesh 归属，调用方负责选择对应的网格。
不要求原网格与副本在后续插入时产生相同的 handle。

`third_party/SlotMap/slot_map.h` 保留项目原有的“复制时跳过已删除槽位”修改。
目录调整不修改 dod 的实现。
