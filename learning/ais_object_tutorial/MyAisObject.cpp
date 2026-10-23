#include "MyAisObject.h"
#include "MyAisOwner.h"

#include <AIS_Animation.hxx>
#include <AIS_AnimationObject.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepBndLib.hxx>
#include <Graphic3d_ArrayOfTriangles.hxx>
#include <Graphic3d_ArrayOfSegments.hxx>
#include <Graphic3d_Group.hxx>
#include <Prs3d_BndBox.hxx>
#include <Prs3d_LineAspect.hxx>
#include <Prs3d_ShadingAspect.hxx>
#include <Prs3d_ToolCylinder.hxx>
#include <Prs3d_ToolDisk.hxx>
#include <Select3D_SensitivePrimitiveArray.hxx>
#include <TopLoc_Location.hxx>
#include <gp_Ax3.hxx>
#include <gp_Trsf.hxx>
#include <cmath>

// =====================================================================
// 构造: 预分配属性 (myDrawer), 而不是在 Compute() 里临时建 aspect ——
// 这样改属性后只需 SynchronizeAspects(), 不必重算整个表现。
// =====================================================================
MyAisObject::MyAisObject()
{
  // 高亮可以走专用显示模式 (SetHilightMode), 也可以走自定义 Owner
  // (见 MyAisOwner::HilightWithColor) —— 本教程用后者, 此处注释掉:
  // SetHilightMode (MyDispMode_Highlight);

  myDrawer->SetupOwnShadingAspect();                                        // 着色属性
  myDrawer->ShadingAspect()->SetMaterial (Graphic3d_NameOfMaterial_Silver); // 银色材质
  // 线段属性: 绿色实线, 宽 2.0
  myDrawer->SetWireAspect (new Prs3d_LineAspect (Quantity_NOC_GREEN, Aspect_TOL_SOLID, 2.0));
}

// =====================================================================
// Compute —— 表现构建
//   - StdPrs_ShadedShape / StdPrs_WFShape: B-Rep 通用构建器 (被注释, 作对照)
//   - Prs3d_ToolQuadric 系: 圆柱/圆盘直接三角化, 绕开 B-Rep + BRepMesh 的巨大开销
//   - Graphic3d_ArrayOfSegments: 手工图元数组 (圆柱截面矩形轮廓)
// =====================================================================
void MyAisObject::Compute (const Handle(PrsMgr_PresentationManager)& thePrsMgr,
                           const Handle(Prs3d_Presentation)& thePrs,
                           const Standard_Integer theMode)
{
  (void )thePrsMgr;
  const double aRadius = 100.0, aHeight = 100.0;

  if (theMode == MyDispMode_Main)
  {
    // ---- 方案 A: 标准 B-Rep 构建器 (对照用) ----
    // TopoDS_Shape aShape = BRepPrimAPI_MakeCylinder (aRadius, aHeight);
    // StdPrs_ShadedShape::Add (thePrs, aShape, myDrawer); // 着色 (三角化)
    // StdPrs_WFShape::Add (thePrs, aShape, myDrawer);    // 线框 (轮廓边)

    // ---- 方案 B: Quadric 构建器, 直接生成三角化 ----
    const int aNbSlices = 25;
    Prs3d_ToolCylinder aCyl (aRadius, aRadius, aHeight, aNbSlices, aNbSlices); // 侧面
    Prs3d_ToolDisk aDisk (0.0, aRadius, aNbSlices, 1);                         // 圆盘 (端盖)

    // 把 侧面 + 底盖 + 顶盖 合并进同一个三角形数组:
    // 每个 Graphic3d_ArrayOfPrimitives 对应一次 GPU draw call,
    // 合并数组在大场景下对性能影响巨大。
    Handle(Graphic3d_ArrayOfTriangles) aTris =
      new Graphic3d_ArrayOfTriangles (aCyl.VerticesNb() + 2 * aDisk.VerticesNb(),
                                      3 * (aCyl.TrianglesNb() + 2 * aDisk.TrianglesNb()),
                                      Graphic3d_ArrayFlags_VertexNormal);
    aCyl.FillArray (aTris, gp_Trsf());

    gp_Trsf aDisk2Trsf; // 顶盖: 平移到 z=aHeight 并翻转法向 (-DZ)
    aDisk2Trsf.SetTransformation (gp_Ax3 (gp_Pnt (0.0, 0.0, aHeight), -gp::DZ(), gp::DX()), gp::XOY());
    aDisk.FillArray (aTris, gp_Trsf());
    aDisk.FillArray (aTris, aDisk2Trsf);

    Handle(Graphic3d_Group) aGroupTris = thePrs->NewGroup();
    aGroupTris->SetGroupPrimitivesAspect (myDrawer->ShadingAspect()->Aspect());
    aGroupTris->AddPrimitiveArray (aTris);
    aGroupTris->SetClosed (true); // 闭合实体: 允许背面剔除/封盖优化

    // ---- 手工图元数组: 圆柱 X 截面矩形轮廓 (4 顶点 + 4 条索引线段) ----
    Handle(Graphic3d_ArrayOfSegments) aSegs =
      new Graphic3d_ArrayOfSegments (4, 4 * 2, Graphic3d_ArrayFlags_None);
    aSegs->AddVertex (gp_Pnt (0.0, -aRadius, 0.0));
    aSegs->AddVertex (gp_Pnt (0.0, -aRadius, aHeight));
    aSegs->AddVertex (gp_Pnt (0.0,  aRadius, aHeight));
    aSegs->AddVertex (gp_Pnt (0.0,  aRadius, 0.0));
    aSegs->AddEdges (1, 2);
    aSegs->AddEdges (2, 3);
    aSegs->AddEdges (3, 4);
    aSegs->AddEdges (4, 1);

    Handle(Graphic3d_Group) aGroupSegs = thePrs->NewGroup();
    aGroupSegs->SetGroupPrimitivesAspect (myDrawer->WireAspect()->Aspect());
    aGroupSegs->AddPrimitiveArray (aSegs);
  }
  else if (theMode == MyDispMode_Highlight)
  {
    // 辅助模式: 包围盒 (Prs3d_BndBox), 供选中/高亮表现复用 (见 MyAisOwner)
    TopoDS_Shape aShape = BRepPrimAPI_MakeCylinder (aRadius, aHeight);
    Bnd_Box aBox;
    BRepBndLib::Add (aShape, aBox);
    Prs3d_BndBox::Add (thePrs, aBox, myDrawer);
  }
}

// =====================================================================
// ComputeSelection —— 选择构建 (与表现完全独立, 但最好几何一致)
//   - Select3D_SensitivePrimitiveArray: 直接复用圆柱三角化作为拾取体,
//     精确贴合显示的几何 (对照: Select3D_SensitiveBox 只给包围盒精度;
//     StdSelect_BRepSelectionTool 则是 B-Rep 的标准选择构建器)
//   - SelectMgr_EntityOwner 是拾取结果的标识物: 0 模式整个对象共用一个 Owner
// =====================================================================
void MyAisObject::ComputeSelection (const Handle(SelectMgr_Selection)& theSel,
                                    const Standard_Integer theMode)
{
  if (theMode != 0) { return; } // 只支持第 0 选择模式 (整个对象)

  const double aRadius = 100.0, aHeight = 100.0;
  Handle(MyAisOwner) anOwner = new MyAisOwner (this);
  anOwner->SetAnimation (myAnim);

  Handle(Graphic3d_ArrayOfTriangles) aTris =
    Prs3d_ToolCylinder::Create (aRadius, aRadius, aHeight, 25, 25, gp_Trsf());
  Handle(Select3D_SensitivePrimitiveArray) aSensTri = new Select3D_SensitivePrimitiveArray (anOwner);
  aSensTri->InitTriangulation (aTris->Attributes(), aTris->Indices(), TopLoc_Location());
  theSel->Add (aSensTri);
}
