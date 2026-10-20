// ============================================================================
// OCCT 瓶子建模教程 —— 对应文档: OCCT-7_9_3/dox/tutorial/tutorial.md
//
// 按教程章节渐进实现 makeBottle(), 每完成一章编译运行查看一次:
//   [x] 第 1 步  sec2  Building the Profile    瓶子轮廓(当前显示的就是它)
//   [x] 第 2 步  sec3  Building the Body       瓶身: 拉伸/圆角/瓶颈/抽壳
//   [x] 第 3 步  sec4  Building the Threading  瓶颈螺纹 (当前显示)
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

// --- 第 2 步 (sec3 Building the Body) 新增头文件 ---
#include <gp_Vec.hxx>                       // sec3.1 拉伸方向向量
#include <gp_Ax2.hxx>                       // sec3.3 右手坐标系 (圆柱定位)
#include <TopoDS_Face.hxx>                  // sec3.1 拓扑: 面
#include <BRepBuilderAPI_MakeFace.hxx>      // sec3.1 wire -> face
#include <BRepPrimAPI_MakePrism.hxx>        // sec3.1 棱柱拉伸 (sweep)
#include <BRepFilletAPI_MakeFillet.hxx>     // sec3.2 圆角
#include <TopExp_Explorer.hxx>              // sec3.2/3.4 拓扑遍历 (边、面)
#include <BRepPrimAPI_MakeCylinder.hxx>     // sec3.3 圆柱 (瓶颈)
#include <BRepAlgoAPI_Fuse.hxx>             // sec3.3 布尔并
#include <BRep_Tool.hxx>                    // sec3.4 从拓扑取几何 (面 -> 表面)
#include <Geom_Surface.hxx>                 // sec3.4 表面基类
#include <Geom_Plane.hxx>                   // sec3.4 平面 (找顶面用)
#include <Standard_Type.hxx>                // sec3.4 STANDARD_TYPE / DynamicType
#include <TopTools_ListOfShape.hxx>         // sec3.4 形状列表 (抽壳入口参数)
#include <BRepOffsetAPI_MakeThickSolid.hxx> // sec3.4 抽壳 (中空实体)

// --- 第 3 步 (sec4 Building the Threading) 新增头文件 ---
#include <gp_Pnt2d.hxx>                     // sec4.2 2D 点 (参数空间 U,V)
#include <gp_Dir2d.hxx>                     // sec4.2 2D 方向
#include <gp_Ax2d.hxx>                      // sec4.2 2D 坐标系
#include <Geom_CylindricalSurface.hxx>      // sec4.1 圆柱参数曲面
#include <Geom2d_Ellipse.hxx>               // sec4.2 2D 椭圆 (Geom2d 包)
#include <Geom2d_TrimmedCurve.hxx>          // sec4.2 2D 裁剪曲线
#include <GCE2d_MakeSegment.hxx>            // sec4.2 2D 两点线段
#include <BRepLib.hxx>                      // sec4.3 BuildCurves3d 反求 3D 曲线
#include <BRepOffsetAPI_ThruSections.hxx>   // sec4.4 放样 (loft)

// --- 网格模式(附加功能)头文件 ---
#include <TopoDS_Compound.hxx>              // compound: 网格边集合容器
#include <BRep_Builder.hxx>                 // 手工构建 compound
#include <Poly_Triangulation.hxx>           // 三角剖分数据
#include <BRepMesh_IncrementalMesh.hxx>     // 离散化 (BRepMesh 在 TKMesh, 已链接)

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

  // =========================================================================
  // 第 2 步: Building the Body (tutorial.md sec3)
  // =========================================================================

  // ---- sec3.1 Prism the Profile ------------------------------------------
  // wire 是 1 维的, 参照教程 Shape/Generates 表: Wire -> Face -> 拉伸后 Solid。
  // wire 落在平面上时 MakeFace 自动生成平面表面。
  TopoDS_Face myFaceProfile = BRepBuilderAPI_MakeFace(myWireProfile);
  // 沿 Z 轴正向拉伸 myHeight 高度 (gp_Vec = 有限棱柱的拉伸向量)
  gp_Vec aPrismVec(0, 0, myHeight);
  TopoDS_Shape myBody = BRepPrimAPI_MakePrism(myFaceProfile, aPrismVec);

  // ---- sec3.2 Applying Fillets -------------------------------------------
  // 对瓶身所有边倒圆角, 半径 = myThickness/12。
  // 用 TopExp_Explorer 遍历实体全部边 (More/Current/Next 三板斧)
  BRepFilletAPI_MakeFillet mkFillet(myBody);
  TopExp_Explorer anEdgeExplorer(myBody, TopAbs_EDGE);
  while (anEdgeExplorer.More()) {
    TopoDS_Edge anEdge = TopoDS::Edge(anEdgeExplorer.Current());
    mkFillet.Add(myThickness / 12., anEdge);  // 添加边 + 圆角半径
    anEdgeExplorer.Next();
  }
  myBody = mkFillet.Shape();  // 取回倒角后的形状

  // ---- sec3.3 Adding the Neck --------------------------------------------
  // 瓶颈 = 圆柱体, 放在瓶身顶面 (0,0,myHeight), 沿 Z 正向。
  // gp_Ax2: 由原点 + 主方向(Z)定义的局部坐标系, X 方向默认自动计算
  gp_Pnt neckLocation(0, 0, myHeight);
  gp_Dir neckAxis = gp::DZ();
  gp_Ax2 neckAx2(neckLocation, neckAxis);

  Standard_Real myNeckRadius  = myThickness / 4.;   // 30/4 = 7.5mm
  Standard_Real myNeckHeight  = myHeight / 10.;     // 70/10 = 7mm

  BRepPrimAPI_MakeCylinder mkCylinder(neckAx2, myNeckRadius, myNeckHeight);
  TopoDS_Shape myNeck = mkCylinder.Shape();

  // 布尔并: 瓶身 + 瓶颈 融合为一体 (BRepAlgoAPI 布尔运算包)
  myBody = BRepAlgoAPI_Fuse(myBody, myNeck);

  // ---- sec3.4 Creating a Hollowed Solid ----------------------------------
  // 瓶子要能装东西: 从瓶颈顶面开始抽壳成中空实体。
  // 壁厚 -myThickness/50 (负值 = 壳向实体内部生成)。
  // 难点: 找到要移除的面 —— 位于最高处 (Z 最大) 的平面:
  //   遍历所有面 -> BRep_Tool::Surface 取表面几何 -> DynamicType 判断是否 Geom_Plane
  //   -> DownCast 转型 -> Geom_Plane::Location() 比较原点 Z 值
  TopoDS_Face faceToRemove;
  Standard_Real zMax = -1;

  for (TopExp_Explorer aFaceExplorer(myBody, TopAbs_FACE); aFaceExplorer.More(); aFaceExplorer.Next()) {
    TopoDS_Face aFace = TopoDS::Face(aFaceExplorer.Current());
    Handle(Geom_Surface) aSurface = BRep_Tool::Surface(aFace);
    if (aSurface->DynamicType() == STANDARD_TYPE(Geom_Plane)) {
      Handle(Geom_Plane) aPlane = Handle(Geom_Plane)::DownCast(aSurface);
      gp_Pnt aPnt = aPlane->Location();
      Standard_Real aZ = aPnt.Z();
      if (aZ > zMax) {
        zMax = aZ;
        faceToRemove = aFace;
      }
    }
  }

  // MakeThickSolidByJoin 接收"要移除的面"列表 (可能移除多个), 用 TopTools 集合
  TopTools_ListOfShape facesToRemove;
  facesToRemove.Append(faceToRemove);
  BRepOffsetAPI_MakeThickSolid aSolidMaker;
  aSolidMaker.MakeThickSolidByJoin(myBody, facesToRemove, -myThickness / 50, 1.e-3);
  myBody = aSolidMaker.Shape();

  // =========================================================================
  // 第 3 步: Building the Threading (tutorial.md sec4)
  // 思路: 不直接造 3D 螺旋线 (数学很难), 而是——
  //   在圆柱曲面的 2D 参数空间 (U,V) 里画椭圆弧/线段, 让 OCCT 把它映射到曲面上:
  //   圆柱面参数方程 P(U,V) = O + R*(cosU*xDir + sinU*yDir) + V*zDir
  //     U ∈ [0, 2PI] (圆周角), V 无限 (高度)
  //   斜着的 2D 线段在 3D 里就是螺旋线!
  // =========================================================================

  // ---- sec4.1 Creating Surfaces ------------------------------------------
  // 两张与瓶颈同轴的圆柱面 (复用 sec3.3 的 neckAx2 坐标系):
  //   aCyl1: R*0.99 略小于瓶颈 —— 螺纹底面, 保证与瓶颈保持接触
  //   aCyl2: R*1.05 略大于瓶颈 —— 螺纹顶面, 稍突出于瓶颈表面
  Handle(Geom_CylindricalSurface) aCyl1 = new Geom_CylindricalSurface(neckAx2, myNeckRadius * 0.99);
  Handle(Geom_CylindricalSurface) aCyl2 = new Geom_CylindricalSurface(neckAx2, myNeckRadius * 1.05);

  // ---- sec4.2 Defining 2D Curves -----------------------------------------
  // 在 (U,V) 参数空间建局部坐标系: 原点在 (2PI, neckHeight/2),
  // X 方向取 (2PI, neckHeight/4) —— 让两段椭圆弧各占瓶颈参数空间的一半
  gp_Pnt2d aPnt(2. * M_PI, myNeckHeight / 2.);
  gp_Dir2d aDir(2. * M_PI, myNeckHeight / 4.);
  gp_Ax2d anAx2d(aPnt, aDir);

  // 两张半椭圆 (长轴 2PI 跨半个圆周, 短轴控制螺距):
  //   椭圆弧 + 线段围成螺纹的一个牙型截面; 因为 U 是圆周角, 弧线在 3D 里呈螺旋状
  Standard_Real aMajor = 2. * M_PI;
  Standard_Real aMinor = myNeckHeight / 10;

  Handle(Geom2d_Ellipse) anEllipse1 = new Geom2d_Ellipse(anAx2d, aMajor, aMinor);
  Handle(Geom2d_Ellipse) anEllipse2 = new Geom2d_Ellipse(anAx2d, aMajor, aMinor / 4);
  Handle(Geom2d_TrimmedCurve) anArc1 = new Geom2d_TrimmedCurve(anEllipse1, 0, M_PI);
  Handle(Geom2d_TrimmedCurve) anArc2 = new Geom2d_TrimmedCurve(anEllipse2, 0, M_PI);
  // 取椭圆两端点, 用线段连接 (GCE2d 是 GC 的 2D 版)
  gp_Pnt2d anEllipsePnt1 = anEllipse1->Value(0);
  gp_Pnt2d anEllipsePnt2 = anEllipse1->Value(M_PI);

  Handle(Geom2d_TrimmedCurve) aSegment = GCE2d_MakeSegment(anEllipsePnt1, anEllipsePnt2);

  // ---- sec4.3 Building Edges and Wires -----------------------------------
  // BRepBuilderAPI_MakeEdge 的「曲线+曲面」重载: 2D 曲线落在曲面参数空间上 → 3D pcurve 边
  TopoDS_Edge anEdge1OnSurf1 = BRepBuilderAPI_MakeEdge(anArc1, aCyl1);
  TopoDS_Edge anEdge2OnSurf1 = BRepBuilderAPI_MakeEdge(aSegment, aCyl1);
  TopoDS_Edge anEdge1OnSurf2 = BRepBuilderAPI_MakeEdge(anArc2, aCyl2);
  TopoDS_Edge anEdge2OnSurf2 = BRepBuilderAPI_MakeEdge(aSegment, aCyl2);

  TopoDS_Wire threadingWire1 = BRepBuilderAPI_MakeWire(anEdge1OnSurf1, anEdge2OnSurf1);
  TopoDS_Wire threadingWire2 = BRepBuilderAPI_MakeWire(anEdge1OnSurf2, anEdge2OnSurf2);

  // 这些边只有 2D pcurve, 没有真实的 3D 曲线; BuildCurves3d 自动反求 (数学很复杂, OCCT 代劳)
  BRepLib::BuildCurves3d(threadingWire1);
  BRepLib::BuildCurves3d(threadingWire2);

  // ---- sec4.4 Creating Threading -----------------------------------------
  // 放样 (loft): 在两条螺纹线框之间张出实体 (构造函数第一个参数 true = 生成 Solid 而非 Shell)
  // CheckCompatibility(false): 关闭「两 wire 边数一致」检查, 本例两 wire 各 2 条边, 无需检查
  BRepOffsetAPI_ThruSections aTool(Standard_True);
  aTool.AddWire(threadingWire1);
  aTool.AddWire(threadingWire2);
  aTool.CheckCompatibility(Standard_False);

  TopoDS_Shape myThreading = aTool.Shape();

  // >>> 第 3 步完成: 返回螺纹实体 (第 4 步把瓶身+螺纹合并成最终 Compound)
  return myThreading;
}

// ============================================================================
// 附加功能: 提取 B-Rep 三角网格, 生成"网格模式"显示用的边集合
// 原理: BRepMesh_IncrementalMesh 对形状离散化 —— 这与着色渲染用的是同一套三角剖分
//       (回忆之前讨论的: 着色模式就是把这套 mesh 填色), 然后遍历每个面的
//       Poly_Triangulation, 把每个三角形的三条边生成为 TopoDS_Edge
// ============================================================================
static TopoDS_Shape extractTriangulationEdges(const TopoDS_Shape& theShape,
                                              const Standard_Real theDeflection = 0.3)
{
  BRepMesh_IncrementalMesh aMesher(theShape, theDeflection);  // 离散化 (0.3mm 弦高)
  (void)aMesher;

  TopoDS_Compound aRes;
  BRep_Builder aBuilder;
  aBuilder.MakeCompound(aRes);

  for (TopExp_Explorer aFaceExp(theShape, TopAbs_FACE); aFaceExp.More(); aFaceExp.Next()) {
    TopoDS_Face aFace = TopoDS::Face(aFaceExp.Current());
    TopLoc_Location aLoc;
    Handle(Poly_Triangulation) aTri = BRep_Tool::Triangulation(aFace, aLoc);
    if (aTri.IsNull()) continue;

    // 节点存的是面局部坐标系下的坐标, 需经位置变换转到全局
    const gp_Trsf& aTrsf = aLoc.Transformation();
    for (int i = 1; i <= aTri->NbTriangles(); ++i) {
      int n1, n2, n3;
      aTri->Triangle(i).Get(n1, n2, n3);
      gp_Pnt p1 = aTri->Node(n1).Transformed(aTrsf);
      gp_Pnt p2 = aTri->Node(n2).Transformed(aTrsf);
      gp_Pnt p3 = aTri->Node(n3).Transformed(aTrsf);
      aBuilder.Add(aRes, BRepBuilderAPI_MakeEdge(p1, p2).Edge());
      aBuilder.Add(aRes, BRepBuilderAPI_MakeEdge(p2, p3).Edge());
      aBuilder.Add(aRes, BRepBuilderAPI_MakeEdge(p3, p1).Edge());
    }
  }
  return aRes;
}

// ============================================================================
// Viewer: GLFW 窗口 + OCCT 3D 视图 (精简自根目录 main.cpp, 用法相同)
//   鼠标左键旋转 / 中键平移 / 滚轮缩放, F 适配视口, P 切透视, W 切显示模式, ESC 退出
// ============================================================================
class Viewer : public AIS_ViewController {
public:
  GLFWwindow* win = nullptr;
  Handle(V3d_View) view;
  Handle(AIS_InteractiveContext) ctx;
  float scale = 1.f;
  bool perspectiveOn = true;
  int  displayMode   = 0;     // 显示模式: 0=线框, 1=着色, 2=网格 (W 键循环)
  TopoDS_Shape myShape;           // 最近一次 show() 的形状 (切模式时重显)
  Handle(AIS_Shape) myAisObject;  // 当前显示的 AIS 对象 (切换模式时移除重建)

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
      // W: 循环切换显示模式 线框 -> 着色 -> 网格 -> ...
      if (key == GLFW_KEY_W) {
        v->displayMode = (v->displayMode + 1) % 3;
        v->displayShape(v->myShape);
      }
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

  // 按当前 displayMode 显示形状: 0=线框, 1=着色, 2=网格(三角剖分边集合)
  void displayShape(const TopoDS_Shape& s) {
    // 切换模式时先移除上一个 AIS 对象 (三种模式的显示数据完全不同)
    if (!myAisObject.IsNull()) { ctx->Remove(myAisObject, false); myAisObject.Nullify(); }

    TopoDS_Shape s2 = s;
    int mode = AIS_WireFrame;
    if (displayMode == 2) {
      s2 = extractTriangulationEdges(s);  // 网格模式: 显示离散化边集合
    } else if (displayMode == 1) {
      mode = AIS_Shaded;
    }

    Handle(AIS_Shape) a = new AIS_Shape(s2);
    // 颜色: AIS_Shape::SetColor 会同步传播到 WireAspect (见 AIS_Shape.cxx::setColor)
    a->SetColor(Quantity_Color(0.4, 0.4, 1.0, Quantity_TOC_RGB));
    a->Attributes()->SetFaceBoundaryDraw(true);
    ctx->Display(a, mode, 0, false);
    myAisObject = a;
  }

  void show(const TopoDS_Shape& s) {
    myShape = s;
    displayShape(s);
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
