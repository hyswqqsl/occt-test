# OCAF 学习教程 (OCCT-7_9_3/dox/samples/ocaf.md)
# 数据框架 / 标准属性 / 自定义 TDF_Attribute / 撤销重做 / TNaming / 持久化
add_executable(ocaf_tutorial
  ocaf_tutorial.cpp
  MyPackage_Transformation.cpp
)
target_include_directories(ocaf_tutorial PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${OpenCASCADE_INCLUDE_DIR})
target_link_libraries(ocaf_tutorial PRIVATE
  TKernel TKMath          # 基础 / gp 数学
  TKCDF TKLCAF            # 文档基座(CDM,CDF,TDF) / 标准属性+文档 (7.9 中 TDF 并入 TKLCAF)
  TKCAF                   # TNaming 拓扑命名
  TKBin                   # 完整 OCAF 二进制持久化驱动 (DefineFormat 使用, 含 TNaming)
  TKBRep TKPrim TKTopAlgo # 形状与基本体 (TNaming 演示)
)

# 第6课: OCAF 数据接 3D 视图 (TPrsStd 三步法) — 复用 glfw 示例的窗口适配类
add_executable(ocaf_viewer ocaf_viewer.cpp MyPackage_Transformation.cpp ../glfw/GlfwOcctWindow.cpp)
target_include_directories(ocaf_viewer PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../glfw
  ${OpenCASCADE_INCLUDE_DIR})
target_link_libraries(ocaf_viewer PRIVATE
  TKernel TKMath
  TKCDF TKLCAF TKCAF TKVCAF   # 数据框架 + TPrsStd (OCAF↔AIS 桥梁, 在 TKVCAF)
  TKV3d TKService TKOpenGl    # 3D 查看器/窗口服务/OpenGL 驱动
  TKBin                       # 持久化
  TKBRep TKPrim TKTopAlgo
  glfw)
