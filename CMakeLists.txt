cmake_minimum_required(VERSION 3.16)
project(occt_viewer CXX)
set(CMAKE_CXX_STANDARD 17)

find_package(OpenCASCADE REQUIRED)
find_package(glfw3 REQUIRED)

add_executable(occt_viewer main.cpp)
target_include_directories(occt_viewer PRIVATE ${OpenCASCADE_INCLUDE_DIR})
target_link_libraries(occt_viewer PRIVATE
  TKernel TKMath TKG2d TKG3d TKGeomBase TKBRep TKGeomAlgo TKTopAlgo
  TKPrim TKBO TKFillet TKShHealing TKMesh
  TKDESTEP TKXSBase
  TKService TKV3d TKOpenGl
  glfw)

# 瓶子建模教程 (OCCT-7_9_3/dox/tutorial/tutorial.md) 学习子项目
add_subdirectory(learning)
