// ais_object.md 教程入口: GLFW + AIS_ViewController 驱动 MyAisObject
// 结构参考 OCCT-7_9_3/samples/glfw (GlfwOcctView) —— 事件全部走 AIS_ViewController,
// 它负责拾取/动态高亮/选择/Owner::HandleMouseClick/动画时钟的统一调度。
#include "OcctWindow.h"
#include "MyAisObject.h"

#include <AIS_InteractiveContext.hxx>
#include <AIS_ViewController.hxx>
#include <Message.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>

#include <GLFW/glfw3.h> // 只用基础 API; native 部分隔离在 OcctWindow.cpp
#include <iostream>

namespace
{
  void messageInfo (const TCollection_AsciiString& theText)
  {
    Message::DefaultMessenger()->Send (theText, Message_Info);
  }

  //! GLFW 鼠标键 → OCCT Aspect_VKeyMouse (来自官方 glfw 示例)
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

  //! GLFW 修饰键 → OCCT Aspect_VKeyFlags
  Aspect_VKeyFlags keyFlagsFromGlfw (int theFlags)
  {
    Aspect_VKeyFlags aFlags = Aspect_VKeyFlags_NONE;
    if ((theFlags & GLFW_MOD_SHIFT)   != 0) { aFlags |= Aspect_VKeyFlags_SHIFT; }
    if ((theFlags & GLFW_MOD_CONTROL) != 0) { aFlags |= Aspect_VKeyFlags_CTRL; }
    if ((theFlags & GLFW_MOD_ALT)     != 0) { aFlags |= Aspect_VKeyFlags_ALT; }
    if ((theFlags & GLFW_MOD_SUPER)   != 0) { aFlags |= Aspect_VKeyFlags_META; }
    return aFlags;
  }
}

//! 继承 AIS_ViewController 以复用输入事件 → 拾取/高亮/选择/动画 的整套逻辑
class TutorialViewer : protected AIS_ViewController
{
public:
  void run()
  {
    initWindow (1024, 768, "ais_object.md tutorial - MyAisObject");
    initViewer();
    initScene();
    if (myView.IsNull()) { return; }
    myView->MustBeResized();
    myOcctWindow->Map();
    mainloop();
    cleanup();
  }

private:
  // ---------------- GLFW 窗口 ----------------
  void initWindow (int theWidth, int theHeight, const char* theTitle)
  {
    glfwSetErrorCallback (errorCallback);
    glfwInit();
    // macOS 缺省只有 legacy GL, 需要显式要 core profile 3.3 + forward-compatible
    glfwWindowHint (GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint (GLFW_CONTEXT_VERSION_MINOR, 3);
  #if defined (__APPLE__)
    glfwWindowHint (GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  #endif
    glfwWindowHint (GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    myOcctWindow = new OcctWindow (theWidth, theHeight, theTitle);
    GLFWwindow* aWin = myOcctWindow->getGlfwWindow();
    glfwSetWindowUserPointer (aWin, this);
    glfwSetWindowSizeCallback  (aWin, onResizeCallback);
    glfwSetFramebufferSizeCallback (aWin, onResizeCallback);
    glfwSetScrollCallback      (aWin, onMouseScrollCallback);
    glfwSetMouseButtonCallback (aWin, onMouseButtonCallback);
    glfwSetCursorPosCallback   (aWin, onMouseMoveCallback);
    glfwSetKeyCallback         (aWin, onKeyCallback);
  }

  // ---------------- V3d_View + AIS_InteractiveContext ----------------
  void initViewer()
  {
    if (myOcctWindow.IsNull() || myOcctWindow->getGlfwWindow() == nullptr)
    {
      throw std::runtime_error ("Failed to create GLFW window");
    }
    Handle(OpenGl_GraphicDriver) aDriver = new OpenGl_GraphicDriver (myOcctWindow->GetDisplay(), false);
    Handle(V3d_Viewer) aViewer = new V3d_Viewer (aDriver);
    aViewer->SetDefaultLights();
    aViewer->SetLightOn();
    aViewer->SetDefaultTypeOfView (V3d_PERSPECTIVE);

    myView = aViewer->CreateView();
    myView->SetImmediateUpdate (false);                       // 攒批更新, 手动 Redraw
    myView->SetWindow (myOcctWindow, myOcctWindow->NativeGlContext());
    myView->ChangeRenderingParams().ToShowStats = true;       // 左上角统计 (FPS 等)
    myContext = new AIS_InteractiveContext (aViewer);
  }

  // ---------------- 场景: 展示 MyAisObject ----------------
  void initScene()
  {
    myView->TriedronDisplay (Aspect_TOTP_LEFT_LOWER, Quantity_NOC_GOLD, 0.08, V3d_WIREFRAME);

    myObject = new MyAisObject();
    // 把视图控制器的动画容器挂给对象 → Owner::HandleMouseClick 里启动的
    // 旋转动画会由本类 (AIS_ViewController) 在事件循环中驱动
    myObject->SetAnimation (ObjectsAnimation());
    // 显示: 主显示模式, 激活第 0 选择模式 (0 个敏感实体整挂在一个 Owner 上)
    myContext->Display (myObject, MyAisObject::MyDispMode_Main, 0, false);
    myContext->UpdateCurrentViewer();

    messageInfo ("==================================================\n"
                 " ais_object.md tutorial\n"
                 "   鼠标悬停圆柱 : 动态高亮 (包围盒 + 拾取点法线箭头)\n"
                 "   左键点击圆柱 : 随机换色 + 绕 X 轴旋转动画 (2s)\n"
                 "   左键空白处   : 清除选择\n"
                 "   拖拽/滚轮    : 旋转/缩放视角\n"
                 "   F: 适配视图   ESC: 退出\n"
                 "==================================================");
  }

  void mainloop()
  {
    while (!glfwWindowShouldClose (myOcctWindow->getGlfwWindow()))
    {
      // glfwWaitEvents(): 有事件才渲染 (按需渲染); 连续动画期间 FlushViewEvents
      // 内部会请求重绘, glfwPostEmptyEvent 会唤醒这里的等待
      glfwWaitEvents();
      if (!myView.IsNull())
      {
        // 核心: 分发输入事件 → 拾取/动态高亮/选择/Owner::HandleMouseClick,
        // 并推进 ObjectsAnimation() 动画时钟, 需要时重绘
        FlushViewEvents (myContext, myView, true);
      }
    }
  }

  void cleanup()
  {
    if (!myView.IsNull()) { myView->Remove(); }
    if (!myOcctWindow.IsNull()) { myOcctWindow->Close(); }
    glfwTerminate();
  }

  // ---------------- 事件 → AIS_ViewController ----------------
  void onResize (int theWidth, int theHeight)
  {
    if (theWidth != 0 && theHeight != 0 && !myView.IsNull())
    {
      myView->Window()->DoResize();
      myView->MustBeResized();
      myView->Invalidate();
    }
  }

  void onMouseScroll (double, double theOffsetY)
  {
    if (!myView.IsNull())
    {
      UpdateZoom (Aspect_ScrollDelta (myOcctWindow->CursorPosition(), int(theOffsetY * 8.0)));
    }
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
      // 释放时完成拾取 → 选中 → 回调 MyAisOwner::HandleMouseClick
      ReleaseMouseButton (aPos, mouseButtonFromGlfw (theButton), keyFlagsFromGlfw (theMods), false);
    }
  }

  void onMouseMove (int thePosX, int thePosY)
  {
    if (!myView.IsNull())
    {
      UpdateMousePosition (Graphic3d_Vec2i (thePosX, thePosY),
                           PressedMouseButtons(), LastMouseFlags(), false);
    }
  }

  void onKey (GLFWwindow* theWin, int theKey, int, int theAction, int)
  {
    if (theAction != GLFW_PRESS) { return; }
    if (theKey == GLFW_KEY_ESCAPE) { glfwSetWindowShouldClose (theWin, 1); }
    else if (theKey == GLFW_KEY_F && !myView.IsNull()) { myView->FitAll (0.7, false); myView->ZFitAll(); }
  }

  // ---------------- GLFW 静态回调 ----------------
  static void errorCallback (int theError, const char* theDescription)
  {
    Message::DefaultMessenger()->Send (TCollection_AsciiString ("Error ") + theError + ": " + theDescription, Message_Fail);
  }

  static TutorialViewer* toView (GLFWwindow* theWin)
  {
    return static_cast<TutorialViewer*>(glfwGetWindowUserPointer (theWin));
  }

  static void onResizeCallback (GLFWwindow* theWin, int theW, int theH)
  { toView (theWin)->onResize (theW, theH); }

  static void onMouseScrollCallback (GLFWwindow* theWin, double theX, double theY)
  { toView (theWin)->onMouseScroll (theX, theY); }

  static void onMouseButtonCallback (GLFWwindow* theWin, int theB, int theA, int theM)
  { toView (theWin)->onMouseButton (theB, theA, theM); }

  static void onMouseMoveCallback (GLFWwindow* theWin, double theX, double theY)
  { toView (theWin)->onMouseMove ((int )theX, (int )theY); }

  static void onKeyCallback (GLFWwindow* theWin, int theK, int theS, int theA, int theM)
  { toView (theWin)->onKey (theWin, theK, theS, theA, theM); }

private:
  Handle(OcctWindow)             myOcctWindow;
  Handle(V3d_View)               myView;
  Handle(AIS_InteractiveContext) myContext;
  Handle(MyAisObject)            myObject;
};

int main (int, char**)
{
  try
  {
    TutorialViewer anApp;
    anApp.run();
  }
  catch (const std::exception& theError)
  {
    std::cerr << theError.what() << std::endl;
    return EXIT_FAILURE;
  }
  return 0;
}
