// OCAF 第6课: 把 OCAF 文档里的形状显示到 3D 视图 (对应文档 TPrsStd_Sample)
// 数据流: OCAF 标签树 --TPrsStd_AISViewer 属性--> AIS_InteractiveContext --> glfw 窗口
//
// 运行前提: 先跑过 ocaf_tutorial (生成 ocaf_tutorial.sta);
//           找不到该文件则现场新建一个等价文档。
#include "GlfwOcctWindow.h"
#include "MyPackage_Transformation.hxx"

#include <AIS_InteractiveContext.hxx>
#include <AIS_ViewController.hxx>
#include <BinDrivers_DocumentRetrievalDriver.hxx>
#include <BinDrivers_DocumentStorageDriver.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <Message.hxx>
#include <Message_Messenger.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <Prs3d_LineAspect.hxx>
#include <TDataStd_Name.hxx>
#include <TDocStd_Application.hxx>
#include <TDocStd_Document.hxx>
#include <TDF_ChildIterator.hxx>
#include <TNaming_Builder.hxx>
#include <TNaming_NamedShape.hxx>
#include <TPrsStd_AISPresentation.hxx>
#include <TPrsStd_AISViewer.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>

#include <fstream>
#include <iostream>

#include <GLFW/glfw3.h>

namespace
{
  //! GLFW 鼠标键 → OCCT 枚举 (同 glfw 示例)
  Aspect_VKeyMouse mouseButtonFromGlfw (int theButton)
  {
    switch (theButton)
    {
      case GLFW_MOUSE_BUTTON_LEFT:   return Aspect_VKeyMouse_LeftButton;
      case GLFW_MOUSE_BUTTON_RIGHT:  return Aspect_VKeyMouse_RightButton;
      case GLFW_MOUSE_BUTTON_MIDDLE: return Aspect_VKeyMouse_MiddleButton;
    }
    return Aspect_VKeyMouse_NONE;
  }

  Aspect_VKeyFlags keyFlagsFromGlfw (int theFlags)
  {
    Aspect_VKeyFlags aFlags = Aspect_VKeyFlags_NONE;
    if ((theFlags & GLFW_MOD_SHIFT)   != 0) { aFlags |= Aspect_VKeyFlags_SHIFT; }
    if ((theFlags & GLFW_MOD_CONTROL) != 0) { aFlags |= Aspect_VKeyFlags_CTRL; }
    if ((theFlags & GLFW_MOD_ALT)     != 0) { aFlags |= Aspect_VKeyFlags_ALT; }
    if ((theFlags & GLFW_MOD_SUPER)   != 0) { aFlags |= Aspect_VKeyFlags_META; }
    return aFlags;
  }

  //! 准备 OCAF 文档: 优先打开 ocaf_tutorial.sta, 否则新建等价内容
  bool prepareDocument (const Handle(TDocStd_Application)& theApp,
                        Handle(TDocStd_Document)& theDoc,
                        TDF_Label& theShapeLabel,
                        bool& theOpened)
  {
    theApp->DefineFormat ("OCAF-myApplication", "MyApplication Document Version 1.0", "sta",
                          new BinDrivers_DocumentRetrievalDriver(),
                          new BinDrivers_DocumentStorageDriver());

    // 若存档存在且两个零件都有形状+变换则直接打开; 否则新建等价内容
    for (;;)
    {
      std::ifstream aProbe ("ocaf_tutorial.sta");
      if (!aProbe.good()) { break; }

      theApp->Open (TCollection_ExtendedString ("ocaf_tutorial.sta"), theDoc);
      TDF_Label aRoot = theDoc->Main();
      TDF_Label aBox  = aRoot.FindChild (1);
      TDF_Label aCone = aRoot.FindChild (2);
      Handle(TNaming_NamedShape) aBoxNS, aConeNS;
      aBox.FindAttribute  (TNaming_NamedShape::GetID(), aBoxNS);
      aCone.FindAttribute (TNaming_NamedShape::GetID(), aConeNS);
      gp_Trsf aBoxTrsf;
      bool aHasMatrix = MyPackage_Transformation::ReadMatrix (aBox, aBoxTrsf);
      if (!aBoxNS.IsNull() && !aConeNS.IsNull() && aHasMatrix)
      {
        theShapeLabel = aBox;
        theOpened = true;
        return true;
      }
      theApp->Close (theDoc);   // 旧版数据结构 → 删掉重建
      std::remove ("ocaf_tutorial.sta");
    }

    // 新建等价文档 (数据模型与 ocaf_tutorial 一致:
    //   0:1 Box  = NamedShape(方块@原点) + Transformation(绕Z旋转45°)
    //   0:2 Cone = NamedShape(圆锥@(25,125,0)) + Transformation(平移100,0,0))
    theApp->NewDocument ("OCAF-myApplication", theDoc);
    TDF_Label aRoot = theDoc->Main();
    TDataStd_Name::Set (aRoot, "Assembly");

    TDF_Label aBoxLabel = aRoot.FindChild (1);
    TDataStd_Name::Set (aBoxLabel, "Box");
    TNaming_Builder aBoxBuilder (aBoxLabel);
    aBoxBuilder.Generated (BRepPrimAPI_MakeBox (50.0, 50.0, 50.0).Shape());
    Handle(MyPackage_Transformation) aTBox = MyPackage_Transformation::Set (aBoxLabel);
    aTBox->SetRotation (gp_Ax1 (gp_Pnt (0, 0, 0), gp_Dir (0, 0, 1)), M_PI / 4.0);
    MyPackage_Transformation::StoreMatrix (aBoxLabel, aTBox->Get());   // 镜像矩阵 → 持久化

    TDF_Label aConeLabel = aRoot.FindChild (2);
    TDataStd_Name::Set (aConeLabel, "Cone");
    TNaming_Builder aConeBuilder (aConeLabel);
    aConeBuilder.Generated (BRepPrimAPI_MakeCone (gp_Ax2 (gp_Pnt (25.0, 125.0, 0.0), gp_Dir (0, 0, 1)),
                                                  55.0, 0.0, 70.0).Shape());
    Handle(MyPackage_Transformation) aTCone = MyPackage_Transformation::Set (aConeLabel);
    aTCone->SetTranslation (gp_Vec (100.0, 0.0, 0.0));
    MyPackage_Transformation::StoreMatrix (aConeLabel, aTCone->Get());

    theShapeLabel = aBoxLabel;
    return true;
  }
}

//! OCAF 数据 + 3D 视图 (结构与学习/glfw 的 GlfwOcctView 相同, 场景换成 OCAF 数据)
class OcafViewer : protected AIS_ViewController
{
public:
  void run()
  {
    initWindow (900, 700, "ocaf viewer (TPrsStd)");
    initViewer();
    initOcafScene();
    if (myView.IsNull()) { return; }
    myView->MustBeResized();
    myOcctWindow->Map();
    mainloop();
    cleanup();
  }

private:
  void initWindow (int theW, int theH, const char* theTitle)
  {
    glfwSetErrorCallback (&OcafViewer::errorCallback);
    glfwInit();
    glfwWindowHint (GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint (GLFW_CONTEXT_VERSION_MINOR, 3);
  #if defined (__APPLE__)
    glfwWindowHint (GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  #endif
    glfwWindowHint (GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    myOcctWindow = new GlfwOcctWindow (theW, theH, theTitle);
    GLFWwindow* aWin = myOcctWindow->getGlfwWindow();
    glfwSetWindowUserPointer (aWin, this);
    glfwSetWindowSizeCallback      (aWin, &OcafViewer::onResizeCallback);
    glfwSetFramebufferSizeCallback (aWin, &OcafViewer::onFBResizeCallback);
    glfwSetScrollCallback          (aWin, &OcafViewer::onMouseScrollCallback);
    glfwSetMouseButtonCallback     (aWin, &OcafViewer::onMouseButtonCallback);
    glfwSetCursorPosCallback       (aWin, &OcafViewer::onMouseMoveCallback);
  }

  void initViewer()
  {
    Handle(OpenGl_GraphicDriver) aDriver = new OpenGl_GraphicDriver (myOcctWindow->GetDisplay(), false);
    Handle(V3d_Viewer) aViewer = new V3d_Viewer (aDriver);
    aViewer->SetDefaultLights();
    aViewer->SetLightOn();
    aViewer->SetDefaultTypeOfView (V3d_PERSPECTIVE);
    myViewer = aViewer;
    myView = aViewer->CreateView();
    myView->SetImmediateUpdate (false);
    myView->SetWindow (myOcctWindow, myOcctWindow->NativeGlContext());
    myView->ChangeRenderingParams().ToShowStats = true;
  }

  void initOcafScene()
  {
    if (myOcctWindow.IsNull() || myOcctWindow->getGlfwWindow() == nullptr) { return; }

    // 1. 准备 OCAF 文档与形状标签
    Handle(TDocStd_Application) anApp = new TDocStd_Application();
    prepareDocument (anApp, myDoc, myShapeLabel, myOpenedFrom);
    std::cout << (myOpenedFrom ? "已打开 ocaf_tutorial.sta" : "未找到 ocaf_tutorial.sta, 新建文档") << "\n";

    // 2. TPrsStd 三步法 (文档 TPrsStd_Sample 的核心):
    //    ① 把 "AIS 视图器" 作为属性挂到根标签 — 数据框架从此知道自己连着哪个 3D 视图
    TPrsStd_AISViewer::New (myDoc->Main(), myViewer);
    //    ② 从数据框架取回该属性
    Handle(TPrsStd_AISViewer) anAisViewer;
    if (!TPrsStd_AISViewer::Find (myDoc->Main(), anAisViewer)) { return; }
    //    ③ 交互上下文不自己 new, 而是从数据框架里拿!
    myContext = anAisViewer->GetInteractiveContext();

    // 3. 遍历根标签的所有子标签 (通用写法, 不限定零件个数):
    //    有 NamedShape 的就是零件 → 显示; 并读取 Transformation 属性作用到显示对象
    //    ★ 数据驱动显示: 形状相同, 变换属性不同 → 姿态不同
    int anIndex = 0;
    for (TDF_ChildIterator aChildIt (myDoc->Main()); aChildIt.More(); aChildIt.Next())
    {
      TDF_Label aPart = aChildIt.Value();
      Handle(TNaming_NamedShape) aNS;
      if (!aPart.FindAttribute (TNaming_NamedShape::GetID(), aNS)) { continue; }   // 无形状的标签跳过
      ++anIndex;

      Handle(TPrsStd_AISPresentation) aPrs =
          TPrsStd_AISPresentation::Set (aPart, TNaming_NamedShape::GetID());
      aPrs->SetColor (anIndex % 2 == 1 ? Quantity_NOC_GREEN : Quantity_NOC_GOLD);  // 颜色交替

      Handle(MyPackage_Transformation) aTrsf;
      if (aPart.FindAttribute (MyPackage_Transformation::GetID(), aTrsf))
      {
        // 自定义属性在内存里 (刚建的文档): 直接取
        Handle(AIS_InteractiveObject) anObj = aPrs->GetAIS();
        if (!anObj.IsNull())
        {
          anObj->SetLocalTransformation (aTrsf->Get());   // ★ 属性数据 → 显示变换
        }
      }
      else
      {
        // 从持久化的矩阵镜像读 (打开的文档)
        gp_Trsf aT;
        if (MyPackage_Transformation::ReadMatrix (aPart, aT))
        {
          Handle(AIS_InteractiveObject) anObj = aPrs->GetAIS();
          if (!anObj.IsNull())
          {
            anObj->SetLocalTransformation (aT);
          }
        }
      }
      aPrs->Display (true);
    }

    myView->TriedronDisplay (Aspect_TOTP_LEFT_LOWER, Quantity_NOC_GOLD, 0.08, V3d_WIREFRAME);
    myView->FitAll (0.7);
    myView->ZFitAll();
  }

  void mainloop()
  {
    while (!glfwWindowShouldClose (myOcctWindow->getGlfwWindow()))
    {
      glfwWaitEvents();
      if (!myView.IsNull())
      {
        FlushViewEvents (myContext, myView, true);
      }
    }
  }

  void cleanup()
  {
    if (!myView.IsNull())    { myView->Remove(); }
    if (!myOcctWindow.IsNull()) { myOcctWindow->Close(); }
    glfwTerminate();
  }

  void onResize (int theW, int theH)
  {
    if (theW != 0 && theH != 0 && !myView.IsNull())
    {
      myView->Window()->DoResize();
      myView->MustBeResized();
      myView->Invalidate();
      myView->Redraw();
    }
  }
  void onMouseScroll (double, double theY)
  {
    if (!myView.IsNull()) { UpdateZoom (Aspect_ScrollDelta (myOcctWindow->CursorPosition(), int (theY * 8.0))); }
  }
  void onMouseButton (int theButton, int theAction, int theMods)
  {
    if (myView.IsNull()) { return; }
    const Graphic3d_Vec2i aPos = myOcctWindow->CursorPosition();
    if (theAction == GLFW_PRESS)
    {
      PressMouseButton (aPos, mouseButtonFromGlfw (theButton), keyFlagsFromGlfw (theMods), false);
    }
    else
    {
      ReleaseMouseButton (aPos, mouseButtonFromGlfw (theButton), keyFlagsFromGlfw (theMods), false);
    }
  }
  void onMouseMove (int theX, int theY)
  {
    if (!myView.IsNull()) { UpdateMousePosition (Graphic3d_Vec2i (theX, theY), PressedMouseButtons(), LastMouseFlags(), false); }
  }

  static void errorCallback (int theErr, const char* theDesc)
  {
    Message::DefaultMessenger()->Send (TCollection_AsciiString ("Error ") + theErr + ": " + theDesc, Message_Fail);
  }
  static OcafViewer* toView (GLFWwindow* theW) { return static_cast<OcafViewer*> (glfwGetWindowUserPointer (theW)); }
  static void onResizeCallback (GLFWwindow* w, int x, int y)        { toView (w)->onResize (x, y); }
  static void onFBResizeCallback (GLFWwindow* w, int x, int y)      { toView (w)->onResize (x, y); }
  static void onMouseScrollCallback (GLFWwindow* w, double, double y) { toView (w)->onMouseScroll (0.0, y); }
  static void onMouseButtonCallback (GLFWwindow* w, int b, int a, int m) { toView (w)->onMouseButton (b, a, m); }
  static void onMouseMoveCallback (GLFWwindow* w, double x, double y)  { toView (w)->onMouseMove ((int) x, (int) y); }

private:
  Handle(GlfwOcctWindow)        myOcctWindow;
  Handle(V3d_Viewer)            myViewer;
  Handle(V3d_View)              myView;
  Handle(AIS_InteractiveContext) myContext;   // ← 来自 TPrsStd_AISViewer (数据框架), 非 new 出来
  Handle(TDocStd_Document)      myDoc;
  TDF_Label                     myShapeLabel;
  bool                          myOpenedFrom = false;
};

int main()
{
  OcafViewer anApp;
  try
  {
    anApp.run();
  }
  catch (const std::runtime_error& theErr)
  {
    std::cerr << theErr.what() << std::endl;
    return EXIT_FAILURE;
  }
  return 0;
}
