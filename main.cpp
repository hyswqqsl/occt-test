#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>

// ============================================================================
// OCCT (Open CASCADE Technology) 核心头文件
// 3D 可视化与交互
// ============================================================================
#include <AIS_InteractiveContext.hxx>    // OCCT 交互式上下文，管理选择和高亮
#include <AIS_Shape.hxx>                 // OCCT 形状显示对象
#include <AIS_ViewController.hxx>        // 视图控制器，处理鼠标/键盘交互
#include <Aspect_DisplayConnection.hxx> // 显示连接，连接 OCCT 与窗口系统
#include <Graphic3d_Camera.hxx>        // 相机 (透视/正交投影)
#include <Cocoa_Window.hxx>              // macOS Cocoa 窗口封装
#include <OpenGl_GraphicDriver.hxx>
#include <V3d_View.hxx>                  // 3D 视图
#include <V3d_Viewer.hxx>               // 3D 查看器，管理光源和视图
#include <Prs3d_Drawer.hxx>              // 显示属性（颜色、材质等）

// ============================================================================
// OCCT 几何建模头文件
// 创建和处理 B-Rep (边界表示) 形状
// ============================================================================
#include <BRepPrimAPI_MakeBox.hxx>       // 创建立方体几何
#include <BRepPrimAPI_MakeCylinder.hxx>  // 创建圆柱体几何
#include <BRepAlgoAPI_Cut.hxx>           // 布尔运算：求差
#include <BRepFilletAPI_MakeFillet.hxx>  // 倒角/圆角
#include <TopExp_Explorer.hxx>          // 拓扑遍历（边、面、顶点）
#include <TopoDS.hxx>                   // TopoDS 形状类型转换
#include <STEPControl_Reader.hxx>        // STEP 文件读取

#include <iostream>

// PI 常量 (非标准 C++, 一些平台需自定义避免依赖 _USE_MATH_DEFINES)
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static TopoDS_Shape makeTestShape() {
  // 创建立方体 (Box): 长100 × 宽60 × 高40
  TopoDS_Shape box = BRepPrimAPI_MakeBox(100, 60, 40).Shape();

  // 创建圆柱体（Cutter）:
  //   - gp_Ax2(gp_Pnt(50,30,-10), gp::DZ()): 圆柱体坐标系
  //       - 起点 (50,30,-10) — 位于立方体中心偏低位置，确保孔穿透顶面
  //       - 轴向 DZ — 沿 Z 轴正方向
  //   - 半径 15, 高度 60（足够穿透整个立方体）
  TopoDS_Shape cyl = BRepPrimAPI_MakeCylinder(
      gp_Ax2(gp_Pnt(50, 30, -10), gp::DZ()), 15, 60).Shape();

  // 布尔求差 (Cut): box - cyl，得到带圆柱孔的实体
  TopoDS_Shape s = BRepAlgoAPI_Cut(box, cyl).Shape();

  // 创建圆角构建器, 在所有边上添加 R=3 的圆角
  BRepFilletAPI_MakeFillet f(s);
  // 遍历形状中的所有边 (TopAbs_EDGE)
  for (TopExp_Explorer e(s, TopAbs_EDGE); e.More(); e.Next()) {
    f.Add(3.0, TopoDS::Edge(e.Current()));  // 对每条边添加半径3的圆角
  }
  f.Build();  // 执行圆角计算

  // 返回圆角后的形状; 若失败则返回求差结果
  return f.IsDone() ? f.Shape() : s;
}

static TopoDS_Shape loadStep(const char* path) {
  // 从 .step / .stp 文件读取几何模型
  STEPControl_Reader r;
  // ReadFile() 返回 IFSelect_RetDone 表示读取成功
  if (r.ReadFile(path) != IFSelect_RetDone) return {};
  // TransferRoots(): 转换所有根实体到 OCCT 内部数据结构
  r.TransferRoots();
  // OneShape(): 将所有转换结果合并为一个 TopoDS_Compound 形状
  return r.OneShape();
}

// ============================================================================
// Viewer 类: 封装 GLFW 窗口 + OCCT 3D 视图 + 交互逻辑
//
// 继承 AIS_ViewController 以复用 OCCT 内置的鼠标交互控制
// (旋转/平移/缩放等)
// ============================================================================
class Viewer : public AIS_ViewController {
public:
  // ★ 透视相机的垂直视野角 (Field Of View Y), 单位: 度。
  //   Graphic3d_Camera::SetFOVy() 接受的就是「度」。
  //   30° 属于广角到标准之间, 近大远小明显, 但畸变不至于夸张。
  static constexpr double kFovYDegrees = 30.0;

  GLFWwindow* win = nullptr;                     // GLFW 原生窗口句柄
  Handle(V3d_View) view;                         // OCCT 视图
  Handle(AIS_InteractiveContext) ctx;            // OCCT 交互上下文
  float scale = 1.f;  // Retina 屏 DPI 缩放因子: GLFW 点坐标 -> 物理像素
  bool perspectiveOn = true;  // ★ 当前是否为透视投影 (按 P 切换)

  // --------------------------------------------------------------------------
  // 初始化窗口、驱动、查看器、视图, 并注册 GLFW 事件回调
  // --------------------------------------------------------------------------
  bool init(int w, int h) {
    if (!glfwInit()) return false;

    // 重要: 告诉 GLFW 不要自己创建 OpenGL 上下文,
    // 因为 OCCT 的 OpenGl_GraphicDriver 会自己创建和管理 GL 上下文
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    // 创建 GLFW 窗口
    win = glfwCreateWindow(w, h, "OCCT Viewer", nullptr, nullptr);
    if (!win) return false;

    // 把 this 指针挂到窗口 user pointer, 以便在静态回调里取回 Viewer
    glfwSetWindowUserPointer(win, this);

    // 获取 Retina 屏的 content scale (DPI)
    // scale.x 是横向 DPI 比例, 这里只关心 y 因为我们用它来转换 y 坐标
    float sy;
    glfwGetWindowContentScale(win, &scale, &sy);

    // --- 创建 OCCT 图形栈 ---
    // Aspect_DisplayConnection: 表示到 X11/Wayland/Cocoa 显示服务器的连接
    Handle(Aspect_DisplayConnection) disp = new Aspect_DisplayConnection();

    // OpenGl_GraphicDriver: OpenGL 图形驱动
    //   第二个参数 true 表示启用多线程 (MT) 渲染
    Handle(OpenGl_GraphicDriver) drv = new OpenGl_GraphicDriver(disp, true);

    // V3d_Viewer: 一个 "查看器", 管理相机、光源、可显示对象
    Handle(V3d_Viewer) viewer = new V3d_Viewer(drv);

    // 启用默认光源配置 + 打开光照
    viewer->SetDefaultLights();
    viewer->SetLightOn();

    // AIS 交互上下文: 用 viewer 创建, 用于显示/选择/高亮 AIS 对象
    ctx = new AIS_InteractiveContext(viewer);

    // --- 创建 3D 视图并绑定到 macOS Cocoa 窗口 ---
    view = viewer->CreateView();

    // 把 GLFW 的 NSView 包装成 OCCT 能识别的 Cocoa_Window
    Handle(Cocoa_Window) cw = new Cocoa_Window((NSView*)glfwGetCocoaView(win));
    view->SetWindow(cw);

    // 设置渐变背景色 (上浅下深)
    view->SetBgGradientColors(Quantity_Color(0.25, 0.27, 0.30, Quantity_TOC_RGB),  // 顶部颜色
                              Quantity_Color(0.08, 0.08, 0.10, Quantity_TOC_RGB),  // 底部颜色
                              Aspect_GradientFillMethod_Vertical);                  // 垂直渐变

    // 显示三坐标轴指示器 (Triedron/陀螺仪)
    //   - 位置: 左下角
    //   - 颜色: 白色
    //   - 大小比例: 0.08 (相对视口)
    //   - 深度测试模式: V3d_ZBUFFER (被前景遮挡时隐藏)
    view->TriedronDisplay(Aspect_TOTP_LEFT_LOWER, Quantity_NOC_WHITE, 0.08, V3d_ZBUFFER);

    // ========================================================================
    // 注册 GLFW 事件回调, 桥接到 OCCT 交互逻辑
    // ========================================================================

    // 鼠标移动 -> 调用 OCCT 的 UpdateMousePosition
    glfwSetCursorPosCallback(win, [](GLFWwindow* w, double x, double y) {
      auto* v = (Viewer*)glfwGetWindowUserPointer(w);
      v->UpdateMousePosition(v->px(x, y), v->PressedMouseButtons(), v->LastMouseFlags(), false);
    });

    // 鼠标按键: 把 GLFW 鼠标按钮 + 修饰键 映射成 OCCT 的 Aspect_VKeyMouse / Aspect_VKeyFlags
    glfwSetMouseButtonCallback(win, [](GLFWwindow* w, int btn, int act, int mods) {
      auto* v = (Viewer*)glfwGetWindowUserPointer(w);
      double x, y; glfwGetCursorPos(w, &x, &y);
      // GLFW 鼠标按钮 -> OCCT 虚拟键
      Aspect_VKeyMouse b = btn == GLFW_MOUSE_BUTTON_LEFT  ? Aspect_VKeyMouse_LeftButton
                         : btn == GLFW_MOUSE_BUTTON_RIGHT ? Aspect_VKeyMouse_RightButton
                                                          : Aspect_VKeyMouse_MiddleButton;
      // 修饰键 (Shift/Ctrl/Alt) -> OCCT 标志位
      Aspect_VKeyFlags f = Aspect_VKeyFlags_NONE;
      if (mods & GLFW_MOD_SHIFT)   f |= Aspect_VKeyFlags_SHIFT;
      if (mods & GLFW_MOD_CONTROL) f |= Aspect_VKeyFlags_CTRL;
      if (mods & GLFW_MOD_ALT)     f |= Aspect_VKeyFlags_ALT;
      // 按下/松开对应调用 OCCT 的 Press/ReleaseMouseButton
      if (act == GLFW_PRESS) v->PressMouseButton(v->px(x, y), b, f, false);
      else                   v->ReleaseMouseButton(v->px(x, y), b, f, false);
    });

    // 滚轮 -> 缩放 (Zoom): dy * 8 是滚动灵敏度因子
    glfwSetScrollCallback(win, [](GLFWwindow* w, double, double dy) {
      auto* v = (Viewer*)glfwGetWindowUserPointer(w);
      double x, y; glfwGetCursorPos(w, &x, &y);
      v->UpdateZoom(Aspect_ScrollDelta(v->px(x, y), int(dy * 8)));
    });

    // 窗口尺寸变化 (含 Retina 屏) -> 标记视图需要重新计算投影矩阵
    glfwSetFramebufferSizeCallback(win, [](GLFWwindow* w, int, int) {
      auto* v = (Viewer*)glfwGetWindowUserPointer(w);
      v->view->MustBeResized();   // 通知 OCCT viewport 已变
      v->view->Invalidate();      // 请求重绘
    });

    // 键盘快捷键:
    //   - F: FitAll, 适配所有对象到视口
    //   - ESC: 关闭窗口
    glfwSetKeyCallback(win, [](GLFWwindow* w, int key, int, int act, int) {
      auto* v = (Viewer*)glfwGetWindowUserPointer(w);
      if (act != GLFW_PRESS) return;
      if (key == GLFW_KEY_F) { v->view->FitAll(0.05, false); v->applyProjectionState(); v->view->Redraw(); }
      // P: 切换正交 / 透视, 便于肉眼对比效果
      if (key == GLFW_KEY_P) {
        v->perspectiveOn = !v->perspectiveOn;
        v->applyProjectionState();
        auto* cam = v->view->Camera().get();
        std::cout << "[view] projection = "
                  << (cam->IsOrthographic() ? "Orthographic" : "Perspective")
                  << (cam->IsOrthographic() ? "" : ", FOVy = ")
                  << (cam->IsOrthographic() ? std::string("")
                                            : std::to_string(cam->FOVy()) + " deg")
                  << std::endl;
        // 强制同步重绘, 不依赖异步渲染线程拉取
        v->view->Redraw();
      }
      if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(w, 1);
    });

    return true;
  }

  // --------------------------------------------------------------------------
  // ★ 应用当前所选的投影模式 (透视 / 正交) 到相机
  //
  // 每次 FitAll() 之后以及每帧事件处理后调用, 以确保投影矩阵是最新的。
  // --------------------------------------------------------------------------
  void applyProjectionState() {
    if (!view || !view->Camera().get()) return;
    Handle(Graphic3d_Camera) cam = view->Camera();

    if (perspectiveOn) {
      cam->SetProjectionType(Graphic3d_Camera::Projection_Perspective);
      // ★ 注意: Graphic3d_Camera::SetFOVy() 的单位是「度」, 不是弧度!
      //   (内部存的是 Tan(FOVy * PI / 360), 见 myFOVyTan)
      //   如果误传弧度 (如 45 * PI / 180 ≈ 0.785), 会得到约 0.8 度的
      //   超长焦镜头, 透视畸变微乎其微, 看起来跟正交一模一样。
      cam->SetFOVy(kFovYDegrees);
    } else {
      cam->SetProjectionType(Graphic3d_Camera::Projection_Orthographic);
    }
  }

  // GLFW 回调的像素坐标都是 "点", 需要乘以 scale 转成实际物理像素
  Graphic3d_Vec2i px(double x, double y) const {
    return Graphic3d_Vec2i(int(x * scale), int(y * scale));
  }

  // --------------------------------------------------------------------------
  // 把 TopoDS_Shape 包装成 AIS_Shape 显示到上下文中
  // --------------------------------------------------------------------------
  void show(const TopoDS_Shape& s) {
    Handle(AIS_Shape) a = new AIS_Shape(s);

    // 设置显示颜色 (RGB, 范围 0-1) — 淡灰蓝色
    a->SetColor(Quantity_Color(0.75, 0.78, 0.82, Quantity_TOC_RGB));

    // 显示面边界 (黑色描边) — 启用后呈现 "着色 + 描边" 的工程图风格
    a->Attributes()->SetFaceBoundaryDraw(true);

    // 在上下文中以 "Shaded (着色)" 模式显示; 最后一个 false 表示不立即重绘
    ctx->Display(a, AIS_Shaded, 0, false);

    // 适配整个形状到视口 (留 5% 边距, false = 不考虑 Triedron)
    view->FitAll(0.05, false);

    // ★ FitAll 之后必须再次把投影模式写回: OCCT 的 FitAll/ZFitAll
    //   会按当前投影类型计算包围盒, 可能把相机模式覆盖。
    applyProjectionState();

    // 请求一次完整重绘, 让 GPU 端投影矩阵生效
    view->Redraw();
  }

  // --------------------------------------------------------------------------
  // 主循环: 等待事件 + 让 OCCT 处理输入事件 (旋转/平移/缩放)
  // --------------------------------------------------------------------------
  void run() {
    while (!glfwWindowShouldClose(win)) {
      // 16ms 超时等待事件 (~60 FPS), 避免 CPU 100%
      glfwWaitEventsTimeout(0.016);
      // 把累积的鼠标/键盘事件分发给 OCCT 视图控制器处理
      //   第三个参数 true 表示需要时立即重绘
      FlushViewEvents(ctx, view, true);
      // ★ 强制每帧把所选投影模式写到相机, 防止 OCCT 视图控制器
      //   在交互过程中把投影矩阵覆盖回默认正交
      applyProjectionState();
    }
    // 清理资源
    view->Remove();
    glfwDestroyWindow(win);
    glfwTerminate();
  }
};

// ============================================================================
// 程序入口
// ============================================================================
int main(int argc, char** argv) {
  // 优先使用命令行参数指定的 STEP 文件; 缺省时使用内置的测试形状
  TopoDS_Shape shape = argc > 1 ? loadStep(argv[1]) : makeTestShape();
  if (shape.IsNull()) { std::cerr << "shape is null\n"; return 1; }

  // 启动查看器
  Viewer v;
  if (!v.init(1280, 800)) { std::cerr << "init failed\n"; return 1; }
  v.show(shape);
  v.run();
}
