// ============================================================================
// OCCT 瓶子建模教程 —— 对应文档: OCCT-7_9_3/dox/tutorial/tutorial.md
//
// 按教程章节渐进实现 makeBottle(), 每完成一章编译运行查看一次:
//   [x] 第 1 步  sec2  Building the Profile    瓶子轮廓(当前显示的就是它)
//   [ ] 第 2 步  sec3  Building the Body       瓶身: 拉伸/圆角/瓶颈/抽壳
//   [ ] 第 3 步  sec4  Building the Threading  瓶颈螺纹
//   [ ] 第 4 步  sec5  Building the Compound   合成最终瓶子
//
// 模型规格 (sec1.3):
//   myWidth = 50mm, myHeight = 70mm, myThickness = 30mm, 底面中心在原点
// ============================================================================

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>

#include <cmath>  // M_PI

// ---------------------------------------------------------------------------
// 教程 sec2 用到的 OCCT 头文件
// ---------------------------------------------------------------------------
#include <gp_Pnt.hxx>                    // sec2.1 3D 点 (值语义, 短生命周期)
#include <gp_Ax1.hxx>                    // sec2.4 3D 轴 (点 + 方向)
#include <gp_Dir.hxx>                    // sec2.4 3D 单位方向向量
#include <gp_Trsf.hxx>                   // sec2.4 3D 几何变换 (平移/旋转/镜像...)
#include <gp.hxx>                        // gp::OX() 等全局坐标系统常量
#include <GC_MakeSegment.hxx>            // sec2.2 两点作线段 (GC 算法包)
#include <GC_MakeArcOfCircle.hxx>         // sec2.2 三点作圆弧 (GC 算法包)
#include <Geom_TrimmedCurve.hxx>         // sec2.2 裁剪曲线 (GC 算法返回类型)
#include <TopoDS.hxx>                    // sec2.4 TopoDS::Wire() 等向下转型
#include <TopoDS_Edge.hxx>               // sec2.3 拓扑: 边
#include <TopoDS_Wire.hxx>               // sec2.3 拓扑: 线框
#include <BRepBuilderAPI_MakeEdge.hxx>   // sec2.3 曲线 -> 拓扑边
#include <BRepBuilderAPI_MakeWire.hxx>   // sec2.3 边 -> 拓扑线框
#include <BRepBuilderAPI_Transform.hxx>  // sec2.4 对形状应用 gp_Trsf 变换

// --- 可视化 (与根目录查看器一致) ---
#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <AIS_ViewController.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <Cocoa_Window.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>
#include <TopAbs_ShapeEnum.hxx>

#include <iostream>

// ============================================================================
// makeBottle —— 教程主函数 (随步骤逐步扩展)
// ============================================================================
TopoDS_Shape makeBottle(const Standard_Real myWidth,
                        const Standard_Real myHeight,
                        const Standard_Real myThickness)
{
  (void)myHeight;  // 第 2 步 (拉伸) 会用到

  // =========================================================================
  // 第 1 步: Building the Profile (tutorial.md sec2)
  // =========================================================================

  // ---- sec2.1 Defining Support Points ------------------------------------
  // XOY 平面内 5 个特征点 (见教程图 tutorial_image003):
  //   底面半宽 myWidth/2, 底部内凹深度 myThickness/2, 过渡点 myThickness/4
  gp_Pnt aPnt1(-myWidth / 2., 0, 0);
  gp_Pnt aPnt2(-myWidth / 2., -myThickness / 4., 0);
  gp_Pnt aPnt3(0, -myThickness / 2., 0);
  gp_Pnt aPnt4(myWidth / 2., -myThickness / 4., 0);
  gp_Pnt aPnt5(myWidth / 2., 0, 0);

  // ---- sec2.2 Defining the Geometry ---------------------------------------
  // 半个轮廓 = 2 段线段 + 1 段圆弧:
  //   GC_MakeArcOfCircle(P1,P2,P3): 过三点的圆弧 (P2 为中间经过点)
  //   GC_MakeSegment(P1,P2):        两端点线段
  // 返回 Geom_TrimmedCurve: 在参数区间上裁剪的基曲线 (线/圆)
  Handle(Geom_TrimmedCurve) anArcOfCircle = GC_MakeArcOfCircle(aPnt2, aPnt3, aPnt4);
  Handle(Geom_TrimmedCurve) aSegment1     = GC_MakeSegment(aPnt1, aPnt2);
  Handle(Geom_TrimmedCurve) aSegment2     = GC_MakeSegment(aPnt4, aPnt5);

  // ---- sec2.3 Defining the Topology ---------------------------------------
  // 几何曲线只是数据, 还需拓扑结构把它们连成整体:
  //   曲线 -> Edge (BRepBuilderAPI_MakeEdge)
  //   边   -> Wire (BRepBuilderAPI_MakeWire, ≤4 条边可用构造函数直连)
  TopoDS_Edge anEdge1 = BRepBuilderAPI_MakeEdge(aSegment1);
  TopoDS_Edge anEdge2 = BRepBuilderAPI_MakeEdge(anArcOfCircle);
  TopoDS_Edge anEdge3 = BRepBuilderAPI_MakeEdge(aSegment2);
  TopoDS_Wire aWire   = BRepBuilderAPI_MakeWire(anEdge1, anEdge2, anEdge3);

  // ---- sec2.4 Completing the Profile --------------------------------------
  // 上半轮廓沿 X 轴镜像, 再与原线框合并成完整轮廓:
  //   镜像轴 = 全局 X 轴 (gp::OX() 即过原点、方向 (1,0,0) 的 gp_Ax1)
  gp_Ax1 xAxis = gp::OX();
  gp_Trsf aTrsf;
  aTrsf.SetMirror(xAxis);  // 定义关于轴的镜像变换, 矩阵自动计算

  BRepBuilderAPI_Transform aBRepTrsf(aWire, aTrsf);  // 对形状应用变换
  TopoDS_Shape  aMirroredShape = aBRepTrsf.Shape();  // 结果是 TopoDS_Shape
  TopoDS_Wire   aMirroredWire  = TopoDS::Wire(aMirroredShape);  // 转回 Wire

  // 两个 Wire 合并为一个: 先建 MakeWire 对象, 再逐个 Add
  BRepBuilderAPI_MakeWire mkWire;
  mkWire.Add(aWire);
  mkWire.Add(aMirroredWire);
  TopoDS_Wire myWireProfile = mkWire.Wire();

  // >>> 第 1 步完成: 返回完整轮廓 wire (第 2 步将在此基础上生成瓶身)
  return myWireProfile;
}

// ============================================================================
// Viewer: GLFW 窗口 + OCCT 3D 视图 (精简自根目录 main.cpp, 用法相同)
//   鼠标左键旋转 / 中键平移 / 滚轮缩放, F 适配视口, P 切透视, ESC 退出
// ============================================================================
class Viewer : public AIS_ViewController {
public:
  GLFWwindow* win = nullptr;
  Handle(V3d_View) view;
  Handle(AIS_InteractiveContext) ctx;
  float scale = 1.f;
  bool perspectiveOn = true;

  bool init(int w, int h) {
    if (!glfwInit()) return false;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);  // GL 上下文交给 OCCT 管理
    win = glfwCreateWindow(w, h, "OCCT Bottle Tutorial", nullptr, nullptr);
    if (!win) return false;
    glfwSetWindowUserPointer(win, this);

    float sy;
    glfwGetWindowContentScale(win, &scale, &sy);

    // --- OCCT 图形栈: 显示连接 -> GL 驱动 -> 查看器 -> 交互上下文 -> 视图 ---
    Handle(Aspect_DisplayConnection) disp = new Aspect_DisplayConnection();
    Handle(OpenGl_GraphicDriver) drv = new OpenGl_GraphicDriver(disp, true);
    Handle(V3d_Viewer) viewer = new V3d_Viewer(drv);
    viewer->SetDefaultLights();
    viewer->SetLightOn();
    ctx = new AIS_InteractiveContext(viewer);

    view = viewer->CreateView();
    Handle(Cocoa_Window) cw = new Cocoa_Window((NSView*)glfwGetCocoaView(win));
    view->SetWindow(cw);
    view->SetBgGradientColors(Quantity_Color(0.25, 0.27, 0.30, Quantity_TOC_RGB),
                              Quantity_Color(0.08, 0.08, 0.10, Quantity_TOC_RGB),
                              Aspect_GradientFillMethod_Vertical);
    view->TriedronDisplay(Aspect_TOTP_LEFT_LOWER, Quantity_NOC_WHITE, 0.08, V3d_ZBUFFER);

    // --- GLFW 事件 -> OCCT 交互 ---
    glfwSetCursorPosCallback(win, [](GLFWwindow* w, double x, double y) {
      auto* v = (Viewer*)glfwGetWindowUserPointer(w);
      v->UpdateMousePosition(v->px(x, y), v->PressedMouseButtons(), v->LastMouseFlags(), false);
    });
    glfwSetMouseButtonCallback(win, [](GLFWwindow* w, int btn, int act, int mods) {
      auto* v = (Viewer*)glfwGetWindowUserPointer(w);
      double x, y; glfwGetCursorPos(w, &x, &y);
      Aspect_VKeyMouse b = btn == GLFW_MOUSE_BUTTON_LEFT  ? Aspect_VKeyMouse_LeftButton
                         : btn == GLFW_MOUSE_BUTTON_RIGHT ? Aspect_VKeyMouse_RightButton
                                                          : Aspect_VKeyMouse_MiddleButton;
      Aspect_VKeyFlags f = Aspect_VKeyFlags_NONE;
      if (mods & GLFW_MOD_SHIFT)   f |= Aspect_VKeyFlags_SHIFT;
      if (mods & GLFW_MOD_CONTROL) f |= Aspect_VKeyFlags_CTRL;
      if (mods & GLFW_MOD_ALT)     f |= Aspect_VKeyFlags_ALT;
      if (act == GLFW_PRESS) v->PressMouseButton(v->px(x, y), b, f, false);
      else                   v->ReleaseMouseButton(v->px(x, y), b, f, false);
    });
    glfwSetScrollCallback(win, [](GLFWwindow* w, double, double dy) {
      auto* v = (Viewer*)glfwGetWindowUserPointer(w);
      double x, y; glfwGetCursorPos(w, &x, &y);
      v->UpdateZoom(Aspect_ScrollDelta(v->px(x, y), int(dy * 8)));
    });
    glfwSetFramebufferSizeCallback(win, [](GLFWwindow* w, int, int) {
      auto* v = (Viewer*)glfwGetWindowUserPointer(w);
      v->view->MustBeResized();
      v->view->Invalidate();
    });
    glfwSetKeyCallback(win, [](GLFWwindow* w, int key, int, int act, int) {
      auto* v = (Viewer*)glfwGetWindowUserPointer(w);
      if (act != GLFW_PRESS) return;
      if (key == GLFW_KEY_F) { v->view->FitAll(0.05, false); v->applyProjectionState(); v->view->Redraw(); }
      if (key == GLFW_KEY_P) { v->perspectiveOn = !v->perspectiveOn; v->applyProjectionState(); v->view->Redraw(); }
      if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(w, 1);
    });
    return true;
  }

  void applyProjectionState() {
    if (!view || !view->Camera().get()) return;
    Handle(Graphic3d_Camera) cam = view->Camera();
    if (perspectiveOn) {
      cam->SetProjectionType(Graphic3d_Camera::Projection_Perspective);
      cam->SetFOVy(30.0);  // 单位是「度」
    } else {
      cam->SetProjectionType(Graphic3d_Camera::Projection_Orthographic);
    }
  }

  Graphic3d_Vec2i px(double x, double y) const {
    return Graphic3d_Vec2i(int(x * scale), int(y * scale));
  }

  void show(const TopoDS_Shape& s) {
    Handle(AIS_Shape) a = new AIS_Shape(s);
    // 颜色: AIS_Shape::SetColor 会同步传播到 WireAspect (见 AIS_Shape.cxx::setColor),
    // 所以改颜色用 SetColor 是有效的; 但要选区分度高的颜色 (1px 细线上淡色差异看不出来)
    a->SetColor(Quantity_Color(1.0, 0.0, 0.0, Quantity_TOC_RGB));  // 纯红
    a->Attributes()->SetFaceBoundaryDraw(true);
    // 第 1 步显示的是 wire (无面), 用线框模式 + 加粗线条才看得清
    // ★ 宽度注意: wire 线框由 WireAspect 绘制 (StdPrs_WFShape.cxx L111/L222),
    //   设到 LineAspect 上是无效的!
    const bool isWire = s.ShapeType() == TopAbs_WIRE;
    if (isWire) a->Attributes()->WireAspect()->SetWidth(6.0);
    // ↑ 若宽度看不出变化: macOS 的 OpenGL core profile 会把线宽钳制到 1px,
    //   属系统限制; 颜色不受影响
    ctx->Display(a, isWire ? AIS_WireFrame : AIS_Shaded, 0, false);
    view->FitAll(0.05, false);
    applyProjectionState();
    view->Redraw();
  }

  void run() {
    while (!glfwWindowShouldClose(win)) {
      glfwWaitEventsTimeout(0.016);
      FlushViewEvents(ctx, view, true);
      applyProjectionState();
    }
    view->Remove();
    glfwDestroyWindow(win);
    glfwTerminate();
  }
};

// ============================================================================
// 程序入口: 构建 50 x 70 x 30 规格的瓶子 (教程 sec1.3 规格表)
// ============================================================================
int main() {
  TopoDS_Shape shape = makeBottle(50., 70., 30.);
  if (shape.IsNull()) { std::cerr << "shape is null\n"; return 1; }
  std::cout << "makeBottle stage OK: " << shape.ShapeType() << "\n";

  Viewer v;
  if (!v.init(1280, 800)) { std::cerr << "init failed\n"; return 1; }
  v.show(shape);
  v.run();
}
