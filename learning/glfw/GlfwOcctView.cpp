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
#include <BRepAdaptor_Curve.hxx>
#include <BRep_Builder.hxx>
#include <GCPnts_UniformDeflection.hxx>
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

#include <dxflib/dl_codes.h>
#include <dxflib/dl_dxf.h>
#include <dxflib/dl_entities.h>
#include <dxflib/dl_writer_ascii.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

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

  //! 三视图布局定义 (命名空间作用域, 供 DXF 导出函数使用)
  struct HlrViewDef
  {
    const char* Name;     // 视图名称
    gp_Dir      ViewDir;  // 观察方向(指向观察者, 即 gp_Ax2 的 Z)
    gp_Dir      XDir;     // 投影平面横轴
    gp_Trsf     Layout;   // 布局变换: 平移到图纸位置
    gp_XYZ      LabelPos; // 视图名称标签位置
  };

  //! DXF: 把 laid-out 形状的边离散成线段写入指定图层 (圆弧/样条按 0.05 弦高差采样)
  void dxfWriteShapeEdges (DL_Dxf& theDxf, DL_WriterA& theW,
                           const TopoDS_Shape& theShape,
                           const std::string&  theLayer,
                           int                 theColor,
                           const std::string&  theLinetype)
  {
    for (TopExp_Explorer anExp (theShape, TopAbs_EDGE); anExp.More(); anExp.Next())
    {
      const TopoDS_Edge anEdge = TopoDS::Edge (anExp.Current());
      BRepAdaptor_Curve aCurve (anEdge);
      std::vector<gp_Pnt> aPts;
      if (aCurve.GetType() == GeomAbs_Line)
      {
        aPts.push_back (aCurve.Value (aCurve.FirstParameter()));
        aPts.push_back (aCurve.Value (aCurve.LastParameter()));
      }
      else
      {
        GCPnts_UniformDeflection aDisc (aCurve, 0.05);
        if (!aDisc.IsDone() || aDisc.NbPoints() < 2) continue;
        for (Standard_Integer i = 1; i <= aDisc.NbPoints(); ++i)
        {
          aPts.push_back (aDisc.Value (i));
        }
      }
      for (size_t i = 1; i < aPts.size(); ++i)
      {
        theDxf.writeLine (theW,
                          DL_LineData (aPts[i-1].X(), aPts[i-1].Y(), 0.0,
                                       aPts[i].X(),   aPts[i].Y(),   0.0),
                          DL_Attributes (theLayer, theColor, 0, theLinetype, 1.0));
      }
    }
  }

  //! DXF 标注参数: 线性 = 两测量点 + 引出距离; 径向 = 圆心(x1,y1) + 半径(x2)
  struct DxfDimSpec
  {
    bool        radial;
    double      x1, y1;
    double      x2, y2;   // 线性: 第二测量点; 径向: 半径
    double      flyout;   // 尺寸线引出距离 (带符号定方向)
    const char* text;     // 标注文字 (如 "50" / "R55")
  };

  //! DXF: SOLID 实心三角箭头 (尖端在 theTip, 指向 theDir 方向)
  void dxfWriteArrow (DL_Dxf& theDxf, DL_WriterA& theW,
                      double theTipX, double theTipY,
                      double theDirX, double theDirY,
                      const DL_Attributes& theAttrib)
  {
    const double aBX = theTipX + theDirX * 3.5, aBY = theTipY + theDirY * 3.5;
    const double aNX = -theDirY * 1.2, aNY = theDirX * 1.2;
    theDxf.writeSolid (theW,
                       DL_SolidData (theTipX,    theTipY,    0.0,
                                     aBX + aNX, aBY + aNY, 0.0,
                                     aBX - aNX, aBY - aNY, 0.0,
                                     aBX - aNX, aBY - aNY, 0.0),
                       theAttrib);
  }

  //! DXF: 线性尺寸标注 (尺寸界线 + 尺寸线 + 双箭头 + 数值文字)
  void dxfWriteLinearDim (DL_Dxf& theDxf, DL_WriterA& theW, const DxfDimSpec& theDim)
  {
    const double aDx = theDim.x2 - theDim.x1, aDy = theDim.y2 - theDim.y1;
    const double aLen = std::sqrt (aDx*aDx + aDy*aDy);
    const double aUx = aDx / aLen, aUy = aDy / aLen;   // 尺寸方向单位向量
    const double aNx = -aUy, aNy = aUx;                 // 左法向 (flyout 方向基)
    const double aS = (theDim.flyout >= 0.0) ? 1.0 : -1.0;
    const double aB1x = theDim.x1 + aNx * theDim.flyout, aB1y = theDim.y1 + aNy * theDim.flyout;
    const double aB2x = theDim.x2 + aNx * theDim.flyout, aB2y = theDim.y2 + aNy * theDim.flyout;

    const DL_Attributes aAttr ("DIM", 3, 0, "CONTINUOUS", 1.0);  // 绿色

    // 尺寸界线: 测量点留 1.5 间隙, 超出尺寸线 2.0
    theDxf.writeLine (theW, DL_LineData (theDim.x1 + aNx*1.5*aS, theDim.y1 + aNy*1.5*aS, 0.0,
                                         aB1x + aUx*2.0, aB1y + aUy*2.0, 0.0), aAttr);
    theDxf.writeLine (theW, DL_LineData (theDim.x2 + aNx*1.5*aS, theDim.y2 + aNy*1.5*aS, 0.0,
                                         aB2x + aUx*2.0, aB2y + aUy*2.0, 0.0), aAttr);
    // 尺寸线
    theDxf.writeLine (theW, DL_LineData (aB1x, aB1y, 0.0, aB2x, aB2y, 0.0), aAttr);
    // 双箭头 (尖端朝外指向尺寸界线)
    dxfWriteArrow (theDxf, theW, aB1x, aB1y, -aUx, -aUy, aAttr);
    dxfWriteArrow (theDxf, theW, aB2x, aB2y,  aUx,  aUy, aAttr);
    // 数值文字 (尺寸线中点, 外侧偏移 2.5)
    const double aTx = (aB1x + aB2x) / 2.0 + aNx * 2.5 * aS;
    const double aTy = (aB1y + aB2y) / 2.0 + aNy * 2.5 * aS;
    double aAngleDeg = std::atan2 (aUy, aUx) * 180.0 / M_PI;
    if (aAngleDeg > 90.0)  { aAngleDeg -= 180.0; }   // 保证文字不倒置
    if (aAngleDeg < -90.0) { aAngleDeg += 180.0; }
    theDxf.writeText (theW,
                      DL_TextData (aTx, aTy, 0.0, aTx, aTy, 0.0,
                                   6.0, 1.0, 0, 1, 1,   // 居中对齐
                                   theDim.text, "Standard", aAngleDeg),
                      DL_Attributes ("DIM", 3, 0, "CONTINUOUS", 1.0));
  }

  //! DXF: 半径标注 (R 文字)
  void dxfWriteRadialDim (DL_Dxf& theDxf, DL_WriterA& theW,
                          double theCx, double theCy, double theR,
                          const char* theText)
  {
    const double aD = M_PI / 4.0;   // 指向 45° 方向圆周点
    const double aDx = std::cos (aD), aDy = std::sin (aD);
    const double aEx = theCx + aDx * theR, aEy = theCy + aDy * theR;

    const DL_Attributes aAttr ("DIM", 3, 0, "CONTINUOUS", 1.0);
    theDxf.writeLine (theW, DL_LineData (theCx, theCy, 0.0, aEx, aEy, 0.0), aAttr);
    dxfWriteArrow (theDxf, theW, aEx, aEy, aDx, aDy, aAttr);
    const double aTx = aEx + aDx * 4.0, aTy = aEy + aDy * 4.0;
    theDxf.writeText (theW,
                      DL_TextData (aTx, aTy, 0.0, aTx, aTy, 0.0,
                                   6.0, 1.0, 0, 0, 0,
                                   theText, "Standard", 0.0),
                      DL_Attributes ("DIM", 3, 0, "CONTINUOUS", 1.0));
  }

  //! 导出三视图为 DXF R2000 CAD 文件: 每视图两个图层 (可见/隐藏), 附视图名称文字,
  //! 尺寸标注 (箭头+数值) 与标题栏描述信息表。
  //! HLR 重新计算一遍 (与屏幕显示解耦)。
  bool exportViewsToDxf (const char* thePath,
                         const TopoDS_Shape&            theModel,
                         const std::vector<HlrViewDef>& theDefs,
                         const std::vector<DxfDimSpec>& theDims)
  {
    // 1. 计算全部投影并摆到图纸位置
    std::vector<TopoDS_Shape> aVisList, aHidList;
    for (const HlrViewDef& aDef : theDefs)
    {
      TopoDS_Shape aVis, aHid;
      hlrProject (theModel, gp_Ax2 (gp_Pnt (0, 0, 0), aDef.ViewDir, aDef.XDir), aVis, aHid);
      aVisList.push_back (layoutShape (aVis, aDef.Layout));
      aHidList.push_back (layoutShape (aHid, aDef.Layout));
    }

    // 2. 写 DXF
    DL_Dxf aDxf;
    DL_WriterA* aW = aDxf.out (thePath, DL_VERSION_2000);
    if (aW == NULL)
    {
      Message::DefaultMessenger()->Send (
          TCollection_AsciiString ("DXF export failed: ") + thePath, Message_Fail);
      return false;
    }

    aDxf.writeHeader (*aW);

    // --- HEADER 描述信息: 单位/精度/公制/图纸范围 (描述 table 信息) ---
    aW->dxfString (9, "$INSUNITS");    aW->dxfInt (70, 4);   // 4 = 毫米
    aW->dxfString (9, "$LUNITS");      aW->dxfInt (70, 2);   // 2 = 十进制
    aW->dxfString (9, "$MEASUREMENT"); aW->dxfInt (70, 1);   // 1 = 公制
    aW->dxfString (9, "$EXTMIN");
    aW->dxfReal (10, -600.0); aW->dxfReal (20, -330.0); aW->dxfReal (30, 0.0);
    aW->dxfString (9, "$EXTMAX");
    aW->dxfReal (10,  120.0); aW->dxfReal (20,  200.0); aW->dxfReal (30, 0.0);
    aW->sectionEnd();

    // --- TABLES: 视口/线型/图层/文字样式/视图/UCS/APPID/标注样式/块记录 ---
    aW->sectionTables();
    aDxf.writeVPort (*aW);
    aW->tableLinetypes (4);
    aDxf.writeLinetype (*aW, DL_LinetypeData ("BYBLOCK",    "", 0, 0, 0.0));
    aDxf.writeLinetype (*aW, DL_LinetypeData ("BYLAYER",    "", 0, 0, 0.0));
    aDxf.writeLinetype (*aW, DL_LinetypeData ("CONTINUOUS", "", 0, 0, 0.0));
    double aDash[2] = {4.0, -2.0};  // 长划 4 + 空 2
    aDxf.writeLinetype (*aW, DL_LinetypeData ("HIDDEN", "Hidden __ __", 0, 2, 6.0, aDash));
    aW->tableEnd();

    aW->tableLayers (1 + (int)theDefs.size() * 2 + 3);   // 0 + 每视图2层 + DIM + TITLE
    aDxf.writeLayer (*aW, DL_LayerData ("0", 0),
                     DL_Attributes ("0", 0, 0, "CONTINUOUS", 1.0));
    for (const HlrViewDef& aDef : theDefs)
    {
      // 图层名: "FRONT_VIEW" (可见) + "FRONT_HIDDEN" (隐藏)
      std::string aBase (aDef.Name);
      std::transform (aBase.begin(), aBase.end(), aBase.begin(), ::toupper);
      std::replace (aBase.begin(), aBase.end(), ' ', '_');
      const std::string aViewSuffix = "_VIEW";
      const std::string aRoot = aBase.substr (0, aBase.size() - aViewSuffix.size()); // FRONT
      DL_Attributes aVisLayer (aBase,                       7, 0, "CONTINUOUS", 1.0);
      DL_Attributes aHidLayer (aRoot + "_HIDDEN",          8, 0, "HIDDEN",     1.0);
      aDxf.writeLayer (*aW, DL_LayerData (aBase,           0), aVisLayer);
      aDxf.writeLayer (*aW, DL_LayerData (aRoot + "_HIDDEN", 0), aHidLayer);
    }
    aDxf.writeLayer (*aW, DL_LayerData ("LABEL", 0),
                     DL_Attributes ("LABEL", 7, 0, "CONTINUOUS", 1.0));
    aDxf.writeLayer (*aW, DL_LayerData ("DIM", 0),
                     DL_Attributes ("DIM", 3, 0, "CONTINUOUS", 1.0));   // 标注层: 绿色
    aDxf.writeLayer (*aW, DL_LayerData ("TITLE", 0),
                     DL_Attributes ("TITLE", 4, 0, "CONTINUOUS", 1.0)); // 标题栏层: 青色
    aW->tableEnd();

    aW->tableStyle (1);
    aDxf.writeStyle (*aW, DL_StyleData ("Standard", 0, 0.0, 1.0, 0.0, 0, 2.5, "", ""));
    aW->tableEnd();
    aDxf.writeView (*aW);  aW->tableEnd();
    aDxf.writeUcs (*aW);   aW->tableEnd();
    aW->tableAppid (1);
    aDxf.writeAppid (*aW, "ACAD");
    aW->tableEnd();
    aDxf.writeDimStyle (*aW, 2.5, 0.625, 0.625, 0.0, 0.625);
    aW->tableEnd();
    aDxf.writeBlockRecord (*aW);
    aW->tableEnd();
    aW->sectionEnd();

    // --- BLOCKS: R2000 需要模型/图纸空间块声明 ---
    aW->sectionBlocks();
    aDxf.writeBlock (*aW, DL_BlockData ("*Model_Space",  0, 0.0, 0.0, 0.0));
    aDxf.writeEndBlock (*aW, "*Model_Space");
    aDxf.writeBlock (*aW, DL_BlockData ("*Paper_Space", 0, 0.0, 0.0, 0.0));
    aDxf.writeEndBlock (*aW, "*Paper_Space");
    aW->sectionEnd();

    // --- ENTITIES: 每视图 隐藏边→可见边→标签文字 ---
    aW->sectionEntities();
    for (size_t i = 0; i < theDefs.size(); ++i)
    {
      std::string aBase (theDefs[i].Name);
      std::transform (aBase.begin(), aBase.end(), aBase.begin(), ::toupper);
      std::replace (aBase.begin(), aBase.end(), ' ', '_');
      const std::string aViewSuffix = "_VIEW";
      const std::string aRoot = aBase.substr (0, aBase.size() - aViewSuffix.size()); // FRONT
      dxfWriteShapeEdges (aDxf, *aW, aHidList[i], aRoot + "_HIDDEN", 8, "BYLAYER");
      dxfWriteShapeEdges (aDxf, *aW, aVisList[i], aBase,             7, "BYLAYER");
      aDxf.writeText (*aW,
                      DL_TextData (theDefs[i].LabelPos.X(), theDefs[i].LabelPos.Y(), 0.0,
                                   theDefs[i].LabelPos.X(), theDefs[i].LabelPos.Y(), 0.0,
                                   15.0, 1.0, 0, 0, 0,
                                   theDefs[i].Name, "Standard", 0.0),
                      DL_Attributes ("LABEL", 7, 0, "CONTINUOUS", 1.0));
    }
    // --- 标注: 尺寸线 + 箭头 + 数值文字 (DIM 层) ---
    for (const DxfDimSpec& aDim : theDims)
    {
      if (aDim.radial) { dxfWriteRadialDim (aDxf, *aW, aDim.x1, aDim.y1, aDim.x2, aDim.text); }
      else             { dxfWriteLinearDim  (aDxf, *aW, aDim); }
    }

    // --- 描述信息表 (标题栏, TITLE 层): 图框 + 工程信息 ---
    {
      const DL_Attributes aTitleAttr ("TITLE", 4, 0, "CONTINUOUS", 1.0);
      const double aX1 = -560.0, aY1 = 78.0, aX2 = -300.0, aY2 = 110.0;  // 图纸左上区域
      aDxf.writeLine (*aW, DL_LineData (aX1, aY1, 0, aX2, aY1, 0), aTitleAttr);
      aDxf.writeLine (*aW, DL_LineData (aX2, aY1, 0, aX2, aY2, 0), aTitleAttr);
      aDxf.writeLine (*aW, DL_LineData (aX2, aY2, 0, aX1, aY2, 0), aTitleAttr);
      aDxf.writeLine (*aW, DL_LineData (aX1, aY2, 0, aX1, aY1, 0), aTitleAttr);
      aDxf.writeLine (*aW, DL_LineData (aX1, aY1 + 18.0, 0, aX2, aY1 + 18.0, 0), aTitleAttr); // 行分隔
      aDxf.writeText (*aW,
                      DL_TextData (aX1 + 4.0, aY2 - 6.0, 0.0, aX1 + 4.0, aY2 - 6.0, 0.0,
                                   6.0, 1.0, 0, 0, 0,
                                   "THREE-VIEW DRAWING (HLR) — BOX + CONE", "Standard", 0.0),
                      aTitleAttr);
      aDxf.writeText (*aW,
                      DL_TextData (aX1 + 4.0, aY1 + 4.0, 0.0, aX1 + 4.0, aY1 + 4.0, 0.0,
                                   5.0, 1.0, 0, 0, 0,
                                   "UNIT: mm   SCALE 1:1   PROJECTION: FIRST-ANGLE", "Standard", 0.0),
                      aTitleAttr);
      aDxf.writeText (*aW,
                      DL_TextData (aX1 + 160.0, aY1 + 4.0, 0.0, aX1 + 160.0, aY1 + 4.0, 0.0,
                                   5.0, 1.0, 0, 0, 0,
                                   "glfwocct / OCCT 7.9", "Standard", 0.0),
                      aTitleAttr);
    }
    aW->sectionEnd();

    aDxf.writeObjects (*aW);
    aW->dxfEOF();
    aW->close();
    delete aW;
    return true;
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

  // 结构体 HlrViewDef 定义在文件头匿名命名空间 (DXF 导出函数也用它)
  const std::vector<HlrViewDef> aViewDefs =
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

  // ===================== 导出三视图为 DXF CAD 文件 =====================
  // 标注数据: 与上面 PrsDim 标注同一批数字, 在 DXF 里画成尺寸线+箭头+数值 (DIM 层)
  std::vector<DxfDimSpec> aDxfDims;
  aDxfDims.push_back ({false,   0.0 + aFrontOff.X(),   0.0 + aFrontOff.Y(),  50.0 + aFrontOff.X(),   0.0 + aFrontOff.Y(), -12.0, "50"});
  aDxfDims.push_back ({false,  25.0 + aFrontOff.X(),   0.0 + aFrontOff.Y(),  25.0 + aFrontOff.X(),  70.0 + aFrontOff.Y(),  20.0, "70"});
  aDxfDims.push_back ({false, -30.0 + aFrontOff.X(),   0.0 + aFrontOff.Y(),  80.0 + aFrontOff.X(),   0.0 + aFrontOff.Y(), -35.0, "110"});
  aDxfDims.push_back ({false,   0.0 + aLeftOff.X(),    0.0 + aLeftOff.Y(),  -50.0 + aLeftOff.X(),    0.0 + aLeftOff.Y(), -12.0, "50"});
  aDxfDims.push_back ({false, -125.0 + aLeftOff.X(),   0.0 + aLeftOff.Y(), -125.0 + aLeftOff.X(),  70.0 + aLeftOff.Y(),  20.0, "70"});
  aDxfDims.push_back ({false,   0.0 + aTopOff.X(),     0.0 + aTopOff.Y(),   50.0 + aTopOff.X(),     0.0 + aTopOff.Y(), -12.0, "50"});
  aDxfDims.push_back ({true,   25.0 + aTopOff.X(),   125.0 + aTopOff.Y(),   55.0,                    0.0,                    0.0, "R55"});
  if (exportViewsToDxf ("three_views.dxf", aModel, aViewDefs, aDxfDims))
  {
    Message::DefaultMessenger()->Send (
        TCollection_AsciiString ("DXF exported: three_views.dxf"), Message_Info);
  }

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
