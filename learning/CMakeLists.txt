# OCCT 瓶子建模教程 (OCCT-7_9_3/dox/tutorial/tutorial.md) 学习用可执行程序
add_executable(bottle_tutorial bottle_tutorial.cpp)
target_include_directories(bottle_tutorial PRIVATE ${OpenCASCADE_INCLUDE_DIR})
target_link_libraries(bottle_tutorial PRIVATE
  TKernel TKMath TKG2d TKG3d TKGeomBase TKBRep TKGeomAlgo TKTopAlgo
  TKPrim TKBO TKFillet TKShHealing TKMesh
  TKOffset  # 第 2 步+: 抽壳 BRepOffsetAPI_MakeThickSolid / 第 3 步放样 BRepOffsetAPI_ThruSections
  TKService TKV3d TKOpenGl
  glfw)
