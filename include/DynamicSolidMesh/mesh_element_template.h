#pragma once
#include <array>

namespace MyMesh
{
    enum class FaceType
    {
        UNSET,
        TRIANGLE,
        QUADRILATERAL
    };

    enum class CellType
    {
        UNSET,
        TET,
        HEX,
        PYRAMID
    };

    namespace CellOrderTemplate
    {
        // 面环按单元外侧观察为逆时针。顶点编号须遵守文档中的正向约定。
        constexpr std::array<std::array<int, 3>, 4> tet_faces = {{
            {1, 2, 3},
            {0, 3, 2},
            {0, 1, 3},
            {0, 2, 1}}};

        // 0,1,2,3 为底面一圈，4,5,6,7 为对应顶面。
        constexpr std::array<std::array<int, 4>, 6> hex_faces = {{
            {0, 3, 2, 1}, {4, 5, 6, 7},
            {0, 1, 5, 4}, {1, 2, 6, 5},
            {2, 3, 7, 6}, {3, 0, 4, 7}}};

        // 0,1,2,3 为底面一圈，4 为顶点；-1 表示三角面的空位。
        constexpr std::array<std::array<int, 4>, 5> pyramid_faces = {{
            {0, 3, 2, 1}, {0, 1, 4, -1}, {1, 2, 4, -1},
            {2, 3, 4, -1}, {3, 0, 4, -1}}};
    }
}
