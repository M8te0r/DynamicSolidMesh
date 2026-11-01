cmake_minimum_required(VERSION 3.16)
project(DynamicSolidMesh LANGUAGES CXX)

add_library(dynamic_solid_mesh STATIC src/mesh.cpp)
add_library(DynamicSolidMesh::DynamicSolidMesh ALIAS dynamic_solid_mesh)

target_compile_features(dynamic_solid_mesh PUBLIC cxx_std_17)
set_target_properties(dynamic_solid_mesh PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(dynamic_solid_mesh PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_include_directories(dynamic_solid_mesh SYSTEM PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party"
)

# 优先复用父项目的 Eigen 目标，否则直接使用附带子模块的头文件。
if(TARGET Eigen3::Eigen)
    target_link_libraries(dynamic_solid_mesh PUBLIC Eigen3::Eigen)
else()
    target_include_directories(dynamic_solid_mesh SYSTEM PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/eigen"
    )
endif()
