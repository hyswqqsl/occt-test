# OCCT 官方 GLFW 3D 查看器示例 (源码复制自 OCCT-7_9_3/samples/glfw) 学习用可执行程序
# 原始 CMakeLists.txt 的 project()/find_package/FindOpenCASCADE 由根工程统一提供,
# adm/ 目录(自定义 FindOpenCASCADE.cmake)在此构建方式下不再使用
add_executable(glfwocct
  main.cpp
  GlfwOcctView.cpp
  GlfwOcctWindow.cpp
)
target_include_directories(glfwocct PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${OpenCASCADE_INCLUDE_DIR})
target_link_libraries(glfwocct PRIVATE
  TKernel TKService TKV3d TKOpenGl      # 渲染栈: 基础/窗口服务/3D查看器/OpenGL驱动
  TKBRep TKPrim TKTopAlgo               # 拓扑形状/基本体(Box,Cone)/拓扑算法
  TKGeomBase TKGeomAlgo TKG3d TKG2d     # 几何内核
  glfw)
