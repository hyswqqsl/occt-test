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
#include <Bnd_Box.hxx>
#include <BRep_Builder.hxx>
#include <BRepBndLib.hxx>
#include <Graphic3d_Camera.hxx>
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

#include <cmath>
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

  //! 按布局变换摆放形状: Moved 只改 TopoDS_Location (旋转+平移), 不复制几何数据, O(1) 开销
  TopoDS_Shape layoutShape (const TopoDS_Shape& theShape, const gp_Trsf& theLayout)
  {
    return theShape.Moved (theLayout);
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

  //! 在图纸平面上加两点长度标注 (工程图样式)。
  //! theP1/theP2: 该视图的图纸局部坐标 (z=0, 即 HLR 投影点经布局旋转后的平面);
  //! theSheetOffset: 该视图在图纸上的摆放平移 (含 z 抬升);
  //! theFlyout: 尺寸线引出距离 (负值引向图纸下方)。标注面法向固定 (0,0,1) — 所有标注都躺在图纸平面内。
  void addViewLengthDim (const Handle(AIS_InteractiveContext)& theCtx,
                         const gp_Pnt&   theP1,
                         const gp_Pnt&   theP2,
                         const gp_XYZ&   theSheetOffset,
                         Standard_Real   theFlyout)
  {
    const gp_Pnt aP1 (theP1.X() + theSheetOffset.X(), theP1.Y() + theSheetOffset.Y(), theSheetOffset.Z());
    const gp_Pnt aP2 (theP2.X() + theSheetOffset.X(), theP2.Y() + theSheetOffset.Y(), theSheetOffset.Z());
    Handle(PrsDim_LengthDimension) aDim = new PrsDim_LengthDimension (aP1, aP2, gp_Pln (aP1, gp_Dir (0, 0, 1)));
    aDim->SetFlyout (theFlyout);
    theCtx->Display (aDim, false);
  }

  //! 显示三视图的一组边: 隐藏边先画(灰虚线), 可见边后画(黑实线盖住重合部分) — 工程制图惯例
  void displayViewEdges (const Handle(AIS_InteractiveContext)& theCtx,
                         const TopoDS_Shape& theVisible,
                         const TopoDS_Shape& theHidden,
                         const gp_Trsf&      theLayout)
  {
    if (!theHidden.IsNull())
    {
      Handle(AIS_Shape) aHidShape = new AIS_Shape (layoutShape (theHidden, theLayout));
      // 线型改成虚线: Prs3d_Drawer::WireAspect 控制 AIS_Shape 的线框边显示
      aHidShape->Attributes()->SetWireAspect (
          new Prs3d_LineAspect (Quantity_NOC_GRAY50, Aspect_TOL_DASH, 1.0));
      theCtx->Display (aHidShape, AIS_WireFrame, 0, false);
    }
    if (!theVisible.IsNull())
    {
      Handle(AIS_Shape) aVisShape = new AIS_Shape (layoutShape (theVisible, theLayout));
      // 可见边: 黑色实线加粗
      aVisShape->Attributes()->SetWireAspect (
          new Prs3d_LineAspect (Quantity_NOC_BLACK, Aspect_TOL_SOLID, 2.0));
      theCtx->Display (aVisShape, AIS_WireFrame, 0, false);
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
  //aViewer->ActivateGrid (Aspect_GT_Rectangular, Aspect_GDM_Lines);
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

  // ===================== 三视图: 工程图纸布局 (全在 z=0 平面内) =====================
  // 把立方体+圆锥合成一个复合体, 一次性投影
  TopoDS_Compound aModel;
  BRep_Builder aBuilder;
  aBuilder.MakeCompound (aModel);
  aBuilder.Add (aModel, aBox->Shape());
  aBuilder.Add (aModel, aCone->Shape());

  // 第一角投影排布: 主视图左, 俯视图在正下方, 左视图在右方 (GB 制图惯例)
  // ★ HLRBRep_HLRToShape 返回的边就是 2D 投影坐标 (x', y', 0), 已经躺在 z=0 平面!
  //   (包围盒调试证实: 正视图 vis bbox=(-30,0,0)-(80,70,0), 与投影方向无关)
  //   所以每个视图只需平移, 不需要任何旋转:
  //   正视图 x'∈[-30,80], y'∈[0,70]   → 平移 (-500, 0)
  //   左视图 x'∈[-180,0], y'∈[0,70]   → 平移 (-200, 0)
  //   俯视图 x'∈[-30,80], y'∈[0,180]  → 平移 (-500,-220)

  // 摆放到图纸位置 (z=5: 稍抬离网格避免共面闪烁)
  const gp_XYZ aFrontOff (-500.0,    0.0, 5.0);
  const gp_XYZ aLeftOff  (-200.0,    0.0, 5.0);
  const gp_XYZ aTopOff   (-500.0, -220.0, 5.0);
  gp_Trsf aMoveFront; aMoveFront.SetTranslation (gp_Vec (aFrontOff));
  gp_Trsf aMoveLeft;  aMoveLeft.SetTranslation  (gp_Vec (aLeftOff));
  gp_Trsf aMoveTop;   aMoveTop.SetTranslation   (gp_Vec (aTopOff));

  struct HlrViewDef
  {
    const char* Name;     // 视图名称
    gp_Dir      ViewDir;  // 观察方向(指向观察者, 即 gp_Ax2 的 Z)
    gp_Dir      XDir;     // 投影平面横轴
    gp_Trsf     Layout;   // 布局变换: 平移到图纸位置
    gp_XYZ      LabelPos; // 视图名称标签位置
  };
  const HlrViewDef aViewDefs[] =
  {
    {"Front view", gp_Dir (0, -1, 0), gp_Dir (1, 0, 0), aMoveFront, gp_XYZ (-540, -30, 5)},
    {"Left view",  gp_Dir (-1, 0, 0), gp_Dir (0, -1, 0), aMoveLeft,  gp_XYZ (-375, -30, 5)},
    {"Top view",   gp_Dir (0, 0, 1),  gp_Dir (1, 0, 0),  aMoveTop,   gp_XYZ (-540, -255, 5)}
  };
  for (const HlrViewDef& aDef : aViewDefs)
  {
    TopoDS_Shape aVis, aHid;
    hlrProject (aModel, gp_Ax2 (gp_Pnt (0, 0, 0), aDef.ViewDir, aDef.XDir), aVis, aHid);
    displayViewEdges (myContext, aVis, aHid, aDef.Layout);
    addTextLabel (myContext, aDef.Name,
                  gp_Pnt (aDef.LabelPos.X(), aDef.LabelPos.Y(), aDef.LabelPos.Z()), 15.0);
  }

  // ===================== 三视图上的尺寸标注 (全在图纸平面内) =====================
  // 标注点 = 视图图纸局部坐标 (z=0) + 该视图摆放平移, 与 displayViewEdges 完全同一平面

  // 正视图: 盒宽 50 / 圆锥高 70 / 圆锥底宽 110
  addViewLengthDim (myContext, gp_Pnt (0, 0, 0),   gp_Pnt (50, 0, 0),   aFrontOff, -12.0);
  addViewLengthDim (myContext, gp_Pnt (25, 0, 0),  gp_Pnt (25, 0, 70),  aFrontOff,  20.0);
  addViewLengthDim (myContext, gp_Pnt (-30, 0, 0), gp_Pnt (80, 0, 0),   aFrontOff, -35.0);

  // 左视图: 盒深 50 / 圆锥高 70 (局部横轴 = -Y)
  addViewLengthDim (myContext, gp_Pnt (0, 0, 0),    gp_Pnt (-50, 0, 0),  aLeftOff, -12.0);
  addViewLengthDim (myContext, gp_Pnt (-125, 0, 0), gp_Pnt (-125, 70, 0), aLeftOff, 20.0);

  // 俯视图: 盒宽 50 / 圆锥底半径 55
  addViewLengthDim (myContext, gp_Pnt (0, 0, 0),   gp_Pnt (50, 0, 0),   aTopOff, -12.0);
  gp_Circ aTopCirc (gp_Ax2 (gp_Pnt (25.0 + aTopOff.X(), 125.0 + aTopOff.Y(), aTopOff.Z()),
                            gp_Dir (0, 0, 1)), 55.0);
  Handle(PrsDim_RadiusDimension) aTopRadDim = new PrsDim_RadiusDimension (aTopCirc);
  myContext->Display (aTopRadDim, false);

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

  // ============ 工程 2D 视角: 正交投影 + 从 +Z 正对图纸, 只适配图纸区域 ============
  // (3D 模型仍在原点, 缩小/旋转即可看到; DRAW 的 vtop/vcortho 就是这两步)
  myContext->UpdateCurrentViewer();
  myView->Camera()->SetProjectionType (Graphic3d_Camera::Projection_Orthographic);
  myView->SetProj (V3d_Zpos);
  Bnd_Box aSheetBox;   // 包住三个视图+标注的图纸区域
  aSheetBox.Add (gp_Pnt (-560.0, -300.0, 0.0));
  aSheetBox.Add (gp_Pnt (-190.0,  110.0, 0.0));
  myView->FitAll (aSheetBox, 0.9);
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
