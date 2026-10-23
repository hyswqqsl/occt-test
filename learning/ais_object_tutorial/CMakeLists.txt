# AIS 自定义交互对象教程 (OCCT-7_9_3/dox/samples/ais_object.md)
# find_package(OpenCASCADE)/project() 由根工程统一提供
add_executable(ais_object_tutorial
  main.cpp
  OcctWindow.h OcctWindow.cpp
  MyAisObject.h MyAisObject.cpp
  MyAisOwner.h MyAisOwner.cpp
)
target_include_directories(ais_object_tutorial PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${OpenCASCADE_INCLUDE_DIR})
target_link_libraries(ais_object_tutorial PRIVATE
  TKernel TKMath TKG2d TKG3d TKGeomBase    # 基础/数学/gp
  TKBRep TKPrim                            # TopoDS/BRepPrimAPI 圆柱
  TKService TKV3d TKOpenGl                 # Graphic3d/Prs3d/AIS/SelectMgr + OpenGL 驱动
  glfw)
