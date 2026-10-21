// Copyright (c) 2019 OPEN CASCADE SAS
//
// This file is part of the examples of the Open CASCADE Technology software library.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE

#include "GlfwOcctView.h"

#include <AIS_Shape.hxx>
#include <Aspect_Handle.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <AIS_TextLabel.hxx>
#include <Aspect_TypeOfLine.hxx>
#include <BRep_Builder.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <HLRAlgo_Projector.hxx>
#include <HLRBRep_Algo.hxx>
#include <HLRBRep_HLRToShape.hxx>
#include <Message.hxx>
#include <Message_Messenger.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <Prs3d_LineAspect.hxx>
#include <PrsDim_LengthDimension.hxx>
#include <PrsDim_RadiusDimension.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>

#include <iostream>

#include <GLFW/glfw3.h>

namespace
{
  //! Convert GLFW mouse button into Aspect_VKeyMouse.
  static Aspect_VKeyMouse mouseButtonFromGlfw (int theButton)
  {
    switch (theButton)
    {
      case GLFW_MOUSE_BUTTON_LEFT:   return Aspect_VKeyMouse_LeftButton;
      case GLFW_MOUSE_BUTTON_RIGHT:  return Aspect_VKeyMouse_RightButton;
      case GLFW_MOUSE_BUTTON_MIDDLE: return Aspect_VKeyMouse_MiddleButton;
    }
    return Aspect_VKeyMouse_NONE;
  }

  //! Convert GLFW key modifiers into Aspect_VKeyFlags.
  static Aspect_VKeyFlags keyFlagsFromGlfw (int theFlags)
  {
    Aspect_VKeyFlags aFlags = Aspect_VKeyFlags_NONE;
    if ((theFlags & GLFW_MOD_SHIFT) != 0)
    {
      aFlags |= Aspect_VKeyFlags_SHIFT;
    }
    if ((theFlags & GLFW_MOD_CONTROL) != 0)
    {
      aFlags |= Aspect_VKeyFlags_CTRL;
    }
    if ((theFlags & GLFW_MOD_ALT) != 0)
    {
      aFlags |= Aspect_VKeyFlags_ALT;
    }
    if ((theFlags & GLFW_MOD_SUPER) != 0)
    {
      aFlags |= Aspect_VKeyFlags_META;
    }
    return aFlags;
  }

  //! HLR (Hidden Line Removal) 投影: 计算形状在给定观察方向下的可见/隐藏边。
  //! theViewCoord 观察坐标系: Z 轴指向观察者(视向), X/Y 是投影平面的横/纵轴。
  void hlrProject (const TopoDS_Shape& theShape,
                   const gp_Ax2&       theViewCoord,
                   TopoDS_Shape&       theVisible,
                   TopoDS_Shape&       theHidden)
  {
    Handle(HLRBRep_Algo) anAlgo = new HLRBRep_Algo();
    anAlgo->Add (theShape, 0);
    anAlgo->Projector (HLRAlgo_Projector (theViewCoord));
    anAlgo->Update(); // 计算轮廓(剪影)
    anAlgo->Hide();   // 计算可见/隐藏线

    HLRBRep_HLRToShape anExtractor (anAlgo);
    // 注意: 各 *Compound() 在没有对应边时会返回空(TopoDS_Shape::IsNull()),
    // 不能直接 BRep_Builder::Add 空形状(会解引用空指针崩溃), 必须逐个判空!
    // 可见边 = 剪影轮廓 + 尖锐边 + 相切光滑边 (7.9 已无 BRepBuilderAPI_MakeCompound, 用 BRep_Builder)
    BRep_Builder aBuilder;
    TopoDS_Compound aVis;
    aBuilder.MakeCompound (aVis);
    const TopoDS_Shape aVisOutline = anExtractor.OutLineVCompound();
    const TopoDS_Shape aVisSharp   = anExtractor.VCompound();
    const TopoDS_Shape aVisSmooth  = anExtractor.Rg1LineVCompound();
    if (!aVisOutline.IsNull()) { aBuilder.Add (aVis, aVisOutline); }
    if (!aVisSharp.IsNull())   { aBuilder.Add (aVis, aVisSharp); }
    if (!aVisSmooth.IsNull())  { aBuilder.Add (aVis, aVisSmooth); }
    theVisible = aVis;

    // 隐藏边(被遮挡的轮廓+棱边)
    TopoDS_Compound aHid;
    aBuilder.MakeCompound (aHid);
    const TopoDS_Shape aHidOutline = anExtractor.OutLineHCompound();
    const TopoDS_Shape aHidSharp   = anExtractor.HCompound();
    if (!aHidOutline.IsNull()) { aBuilder.Add (aHid, aHidOutline); }
    if (!aHidSharp.IsNull())   { aBuilder.Add (aHid, aHidSharp); }
    theHidden = aHid;

    // 调试: 统计边数确认 HLR 确实产出了几何
    Standard_Integer aNbVis = 0, aNbHid = 0;
    for (TopExp_Explorer anExp (theVisible, TopAbs_EDGE); anExp.More(); anExp.Next()) { ++aNbVis; }
    for (TopExp_Explorer anExp (theHidden,  TopAbs_EDGE); anExp.More(); anExp.Next()) { ++aNbHid; }
    Message::DefaultMessenger()->Send (
        TCollection_AsciiString ("HLR: visible edges=") + aNbVis + ", hidden edges=" + aNbHid,
        Message_Info);
  }

  //! 平移形状到布局位置
  TopoDS_Shape translated (const TopoDS_Shape& theShape,
                           Standard_Real theDx, Standard_Real theDy, Standard_Real theDz)
  {
    gp_Trsf aTrsf;
    aTrsf.SetTranslation (gp_Vec (theDx, theDy, theDz));
    return BRepBuilderAPI_Transform (theShape, aTrsf).Shape();
  }

  //! 显示一个 3D 文本标签
  void addTextLabel (const Handle(AIS_InteractiveContext)& theCtx,
                     const TCollection_ExtendedString& theText,
                     const gp_Pnt& thePos,
                     Standard_Real theHeight)
  {
    Handle(AIS_TextLabel) aLabel = new AIS_TextLabel();
    aLabel->SetText (theText);
    aLabel->SetPosition (thePos);
    aLabel->SetColor (Quantity_NOC_BLACK);
    aLabel->SetHeight (theHeight); // 模型坐标下的字高
    theCtx->Display (aLabel, false);
  }

  //! 显示三视图的一组边: 可见边(黑) + 隐藏边(灰虚线)
  void displayViewEdges (const Handle(AIS_InteractiveContext)& theCtx,
                         const TopoDS_Shape& theVisible,
                         const TopoDS_Shape& theHidden,
                         Standard_Real theDx, Standard_Real theDy, Standard_Real theDz)
  {
    if (!theVisible.IsNull())
    {
      Handle(AIS_Shape) aVisShape = new AIS_Shape (translated (theVisible, theDx, theDy, theDz));
      // 可见边: 黑色实线加粗
      aVisShape->Attributes()->SetWireAspect (
          new Prs3d_LineAspect (Quantity_NOC_BLACK, Aspect_TOL_SOLID, 2.0));
      theCtx->Display (aVisShape, AIS_WireFrame, 0, false);
    }
    if (!theHidden.IsNull())
    {
      Handle(AIS_Shape) aHidShape = new AIS_Shape (translated (theHidden, theDx, theDy, theDz));
      // 线型改成虚线: Prs3d_Drawer::WireAspect 控制 AIS_Shape 的线框边显示
      aHidShape->Attributes()->SetWireAspect (
          new Prs3d_LineAspect (Quantity_NOC_GRAY50, Aspect_TOL_DASH, 1.0));
      theCtx->Display (aHidShape, AIS_WireFrame, 0, false);
    }
  }
}

// ================================================================
// Function : GlfwOcctView
// Purpose  :
// ================================================================
GlfwOcctView::GlfwOcctView()
{
}

// ================================================================
// Function : ~GlfwOcctView
// Purpose  :
// ================================================================
GlfwOcctView::~GlfwOcctView()
{
}

// ================================================================
// Function : toView
// Purpose  :
// ================================================================
GlfwOcctView* GlfwOcctView::toView (GLFWwindow* theWin)
{
  return static_cast<GlfwOcctView*>(glfwGetWindowUserPointer (theWin));
}

// ================================================================
// Function : errorCallback
// Purpose  :
// ================================================================
void GlfwOcctView::errorCallback (int theError, const char* theDescription)
{
  Message::DefaultMessenger()->Send (TCollection_AsciiString ("Error") + theError + ": " + theDescription, Message_Fail);
}

// ================================================================
// Function : run
// Purpose  :
// ================================================================
void GlfwOcctView::run()
{
  initWindow (800, 600, "glfw occt");
  initViewer();
  initDemoScene();
  if (myView.IsNull())
  {
    return;
  }

  myView->MustBeResized();
  myOcctWindow->Map();
  mainloop();
  cleanup();
}

// ================================================================
// Function : initWindow
// Purpose  :
// ================================================================
void GlfwOcctView::initWindow (int theWidth, int theHeight, const char* theTitle)
{
  glfwSetErrorCallback (GlfwOcctView::errorCallback);
  glfwInit();
  const bool toAskCoreProfile = true;
  if (toAskCoreProfile)
  {
    glfwWindowHint (GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint (GLFW_CONTEXT_VERSION_MINOR, 3);
#if defined (__APPLE__)
    glfwWindowHint (GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    glfwWindowHint (GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  }
  myOcctWindow = new GlfwOcctWindow (theWidth, theHeight, theTitle);
  glfwSetWindowUserPointer       (myOcctWindow->getGlfwWindow(), this);
  // window callback
  glfwSetWindowSizeCallback      (myOcctWindow->getGlfwWindow(), GlfwOcctView::onResizeCallback);
  glfwSetFramebufferSizeCallback (myOcctWindow->getGlfwWindow(), GlfwOcctView::onFBResizeCallback);
  // mouse callback
  glfwSetScrollCallback          (myOcctWindow->getGlfwWindow(), GlfwOcctView::onMouseScrollCallback);
  glfwSetMouseButtonCallback     (myOcctWindow->getGlfwWindow(), GlfwOcctView::onMouseButtonCallback);
  glfwSetCursorPosCallback       (myOcctWindow->getGlfwWindow(), GlfwOcctView::onMouseMoveCallback);
}

// ================================================================
// Function : initViewer
// Purpose  :
// ================================================================
void GlfwOcctView::initViewer()
{
  if (myOcctWindow.IsNull()
   || myOcctWindow->getGlfwWindow() == nullptr)
  {
    return;
  }

  Handle(OpenGl_GraphicDriver) aGraphicDriver = new OpenGl_GraphicDriver (myOcctWindow->GetDisplay(), false);
  Handle(V3d_Viewer) aViewer = new V3d_Viewer (aGraphicDriver);
  aViewer->SetDefaultLights();
  aViewer->SetLightOn();
  aViewer->SetDefaultTypeOfView (V3d_PERSPECTIVE);
  aViewer->ActivateGrid (Aspect_GT_Rectangular, Aspect_GDM_Lines);
  myView = aViewer->CreateView();
  myView->SetImmediateUpdate (false);
  myView->SetWindow (myOcctWindow, myOcctWindow->NativeGlContext());
  myView->ChangeRenderingParams().ToShowStats = true;
  myContext = new AIS_InteractiveContext (aViewer);
}

// ================================================================
// Function : initDemoScene
// Purpose  :
// ================================================================
void GlfwOcctView::initDemoScene()
{
  if (myContext.IsNull())
  {
    return;
  }

  myView->TriedronDisplay (Aspect_TOTP_LEFT_LOWER, Quantity_NOC_GOLD, 0.08, V3d_WIREFRAME);

  gp_Ax2 anAxis;
  anAxis.SetLocation (gp_Pnt (0.0, 0.0, 0.0));
  Handle(AIS_Shape) aBox = new AIS_Shape (BRepPrimAPI_MakeBox (anAxis, 50, 50, 50).Shape());
  myContext->Display (aBox, AIS_Shaded, 0, false);
  anAxis.SetLocation (gp_Pnt (25.0, 125.0, 0.0));
  Handle(AIS_Shape) aCone = new AIS_Shape (BRepPrimAPI_MakeCone (anAxis, 55, 0, 70).Shape());
  myContext->Display (aCone, AIS_Shaded, 0, false);

  // ===================== 三视图 (HLR 隐藏线消除投影) =====================
  // 把立方体+圆锥合成一个复合体, 一次性投影
  TopoDS_Compound aModel;
  BRep_Builder aBuilder;
  aBuilder.MakeCompound (aModel);
  aBuilder.Add (aModel, aBox->Shape());
  aBuilder.Add (aModel, aCone->Shape());

  struct HlrViewDef
  {
    const char* Name;   // 视图名称
    gp_Dir      ViewDir;  // 观察方向(指向观察者, 即 gp_Ax2 的 Z)
    gp_Dir      XDir;     // 投影平面横轴
    gp_XYZ      Offset;   // 平移到布局位置
  };
  const HlrViewDef aViewDefs[] =
  {
    // 从 -Y 看 → 投影到 XZ 平面 (x'=X, y'=Z)
    {"Front view", gp_Dir (0, -1, 0), gp_Dir (1, 0, 0), gp_XYZ (0, -350, 0)},
    // 从 -X 看 → 投影到 YZ 平面 (x'=-Y, y'=Z)
    {"Left view",  gp_Dir (-1, 0, 0), gp_Dir (0, -1, 0), gp_XYZ (350, -350, 0)},
    // 从 +Z 看 → 投影到 XY 平面 (x'=X, y'=Y)
    {"Top view",   gp_Dir (0, 0, 1), gp_Dir (1, 0, 0), gp_XYZ (0, -700, 0)}
  };
  for (const HlrViewDef& aDef : aViewDefs)
  {
    TopoDS_Shape aVis, aHid;
    hlrProject (aModel, gp_Ax2 (gp_Pnt (0, 0, 0), aDef.ViewDir, aDef.XDir), aVis, aHid);
    displayViewEdges (myContext, aVis, aHid, aDef.Offset.X(), aDef.Offset.Y(), aDef.Offset.Z());
    addTextLabel (myContext, aDef.Name,
                  gp_Pnt (aDef.Offset.X() - 40, aDef.Offset.Y() - 60, aDef.Offset.Z()), 15.0);
  }

  // ===================== 尺寸标注 (PrsDim, 7.9 中 AIS_*Dimension 的正式名字) =====================
  // 立方体底边长度 50: 标在正对观察者的 XZ 平面(z=0 平面, 法向 -Y)
  gp_Pln anXZPlane (gp_Pnt (0.0, 0.0, 0.0), gp_Dir (0.0, -1.0, 0.0));
  Handle(PrsDim_LengthDimension) aLenDim =
      new PrsDim_LengthDimension (gp_Pnt (0, 0, 0), gp_Pnt (50, 0, 0), anXZPlane);
  aLenDim->SetFlyout (15.0); // 尺寸线离标注对象的引出距离
  myContext->Display (aLenDim, false);

  // 立方体高度 50: 标在侧面(YZ 平面, 法向 -X)
  gp_Pln aYZPlane (gp_Pnt (0.0, 0.0, 0.0), gp_Dir (-1.0, 0.0, 0.0));
  Handle(PrsDim_LengthDimension) aHgtDim =
      new PrsDim_LengthDimension (gp_Pnt (0, 0, 0), gp_Pnt (0, 0, 50), aYZPlane);
  aHgtDim->SetFlyout (15.0);
  myContext->Display (aHgtDim, false);

  // 圆锥底面半径 55: 直接给 gp_Circ
  gp_Circ aBaseCirc (gp_Ax2 (gp_Pnt (25.0, 125.0, 0.0), gp_Dir (0.0, 0.0, 1.0)), 55.0);
  Handle(PrsDim_RadiusDimension) aRadDim = new PrsDim_RadiusDimension (aBaseCirc);
  myContext->Display (aRadDim, false);

  // 把整个场景(含下方/侧方的三视图)适配到当前相机视野
  myContext->UpdateCurrentViewer();
  myView->FitAll (0.9);   // 留边系数
  myView->ZFitAll();

  TCollection_AsciiString aGlInfo;
  {
    TColStd_IndexedDataMapOfStringString aRendInfo;
    myView->DiagnosticInformation (aRendInfo, Graphic3d_DiagnosticInfo_Basic);
    for (TColStd_IndexedDataMapOfStringString::Iterator aValueIter (aRendInfo); aValueIter.More(); aValueIter.Next())
    {
      if (!aGlInfo.IsEmpty()) { aGlInfo += "\n"; }
      aGlInfo += TCollection_AsciiString("  ") + aValueIter.Key() + ": " + aValueIter.Value();
    }
  }
  Message::DefaultMessenger()->Send (TCollection_AsciiString("OpenGL info:\n") + aGlInfo, Message_Info);
}

// ================================================================
// Function : mainloop
// Purpose  :
// ================================================================
void GlfwOcctView::mainloop()
{
  while (!glfwWindowShouldClose (myOcctWindow->getGlfwWindow()))
  {
    // glfwPollEvents() for continuous rendering (immediate return if there are no new events)
    // and glfwWaitEvents() for rendering on demand (something actually happened in the viewer)
    //glfwPollEvents();
    glfwWaitEvents();
    if (!myView.IsNull())
    {
      FlushViewEvents (myContext, myView, true);
    }
  }
}

// ================================================================
// Function : cleanup
// Purpose  :
// ================================================================
void GlfwOcctView::cleanup()
{
  if (!myView.IsNull())
  {
    myView->Remove();
  }
  if (!myOcctWindow.IsNull())
  {
    myOcctWindow->Close();
  }
  glfwTerminate();
}

// ================================================================
// Function : onResize
// Purpose  :
// ================================================================
void GlfwOcctView::onResize (int theWidth, int theHeight)
{
  if (theWidth  != 0
   && theHeight != 0
   && !myView.IsNull())
  {
    myView->Window()->DoResize();
    myView->MustBeResized();
    myView->Invalidate();
    myView->Redraw();
  }
}

// ================================================================
// Function : onMouseScroll
// Purpose  :
// ================================================================
void GlfwOcctView::onMouseScroll (double theOffsetX, double theOffsetY)
{
  if (!myView.IsNull())
  {
    UpdateZoom (Aspect_ScrollDelta (myOcctWindow->CursorPosition(), int(theOffsetY * 8.0)));
  }
}

// ================================================================
// Function : onMouseButton
// Purpose  :
// ================================================================
void GlfwOcctView::onMouseButton (int theButton, int theAction, int theMods)
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

// ================================================================
// Function : onMouseMove
// Purpose  :
// ================================================================
void GlfwOcctView::onMouseMove (int thePosX, int thePosY)
{
  const Graphic3d_Vec2i aNewPos (thePosX, thePosY);
  if (!myView.IsNull())
  {
    UpdateMousePosition (aNewPos, PressedMouseButtons(), LastMouseFlags(), false);
  }
}
