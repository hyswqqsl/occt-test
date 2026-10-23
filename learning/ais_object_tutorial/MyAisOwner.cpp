#include "MyAisOwner.h"
#include "MyAisObject.h"

#include <AIS_InteractiveContext.hxx>
#include <AIS_AnimationObject.hxx>
#include <Graphic3d_ArrayOfTriangles.hxx>
#include <Graphic3d_Group.hxx>
#include <Graphic3d_ZLayerId.hxx>
#include <Prs3d_Arrow.hxx>
#include <Prs3d_ArrowAspect.hxx>
#include <Prs3d_ShadingAspect.hxx>
#include <PrsMgr_PresentationManager.hxx>
#include <SelectMgr_SortCriterion.hxx>
#include <StdSelect_ViewerSelector3d.hxx>
#include <TopLoc_Datum3D.hxx>
#include <gp_Ax1.hxx>
#include <gp.hxx>
#include <math_BullardGenerator.hxx>
#include <cmath>

MyAisOwner::MyAisOwner (const Handle(MyAisObject)& theObj, int thePriority)
: SelectMgr_EntityOwner (theObj, thePriority)
{
}

// =====================================================================
// 高亮:
//  - 即时高亮 (IsImmediateModeOn, 鼠标移动/动态高亮): presentation 必须
//    通过 AddToImmediateList() 上屏, 下一帧自动清除。此处画 "拾取点处
//    的表面法线箭头": 从 MainSelector 的拾取结果 (SelectMgr_SortCriterion)
//    取 3D 点和法向, 用 Prs3d_Arrow::DrawShaded 生成箭头三角化。
//  - 持久高亮 (选中): 显示包围盒 myPrs (懒构建, 复用 MyAisObject::Compute
//    的 MyDispMode_Highlight 分支), 需要显式 Display() / Erase()。
// =====================================================================
void MyAisOwner::HilightWithColor (const Handle(PrsMgr_PresentationManager)& thePrsMgr,
                                   const Handle(Prs3d_Drawer)& theStyle,
                                   const Standard_Integer theMode)
{
  (void )theMode;
  MyAisObject* anObj = dynamic_cast<MyAisObject*> (mySelectable);
  if (myPrs.IsNull())
  {
    myPrs = new Prs3d_Presentation (thePrsMgr->StructureManager());
    anObj->Compute (thePrsMgr, myPrs, MyAisObject::MyDispMode_Highlight); // 复用包围盒模式
  }

  if (thePrsMgr->IsImmediateModeOn())
  {
    // 遍历拾取结果找自己, 取出拾取点 + 三角面法向
    const Handle(StdSelect_ViewerSelector3d)& aSelector = anObj->InteractiveContext()->MainSelector();
    SelectMgr_SortCriterion aPickPnt;
    for (int aPickIter = 1; aPickIter <= aSelector->NbPicked(); ++aPickIter)
    {
      if (aSelector->Picked (aPickIter) == this)
      {
        aPickPnt = aSelector->PickedData (aPickIter);
        break;
      }
    }

    // GetHilightPresentation: 高亮表现直接存放在交互对象内部 (不用 Owner 持有)
    Handle(Prs3d_Presentation) aPrs = mySelectable->GetHilightPresentation (thePrsMgr);
    aPrs->Clear();
    Handle(Graphic3d_Group) aGroup = aPrs->NewGroup();
    aGroup->SetGroupPrimitivesAspect (theStyle->ArrowAspect()->Aspect());

    // 法线在全局坐标系; 箭头生成在对象局部坐标系, 需先求逆变换
    gp_Trsf aTrsfInv (mySelectable->InversedTransformation().Trsf());
    gp_Dir aNorm (aPickPnt.Normal.x(), aPickPnt.Normal.y(), aPickPnt.Normal.z());
    Handle(Graphic3d_ArrayOfTriangles) aTris =
      Prs3d_Arrow::DrawShaded (gp_Ax1 (aPickPnt.Point, aNorm).Transformed (aTrsfInv),
                               1.0, 25.0, 6.0, 7.0, 10);
    aGroup->AddPrimitiveArray (aTris);

    aPrs->SetZLayer (Graphic3d_ZLayerId_Top); // 画在最上层, 不被深度遮挡
    thePrsMgr->AddToImmediateList (aPrs);
  }
  else
  {
    // 选中 (持久高亮): 跟随对象当前变换后上屏
    myPrs->SetTransformation (mySelectable->TransformationGeom());
    myPrs->Display();
  }
}

void MyAisOwner::Unhilight (const Handle(PrsMgr_PresentationManager)& thePrsMgr,
                            const Standard_Integer theMode)
{
  (void )thePrsMgr; (void )theMode;
  if (!myPrs.IsNull()) { myPrs->Erase(); } // 自建表现必须自己擦除
}

// =====================================================================
// 鼠标点击 (AIS_InteractiveContext::SelectDetected 内部回调):
//  1) 随机颜色换装 —— 属性挂在对象 Attributes() 上, SynchronizeAspects()
//     把新 aspect 同步到已构建的 presentation, 无需重算;
//  2) 绕 X 轴 ±90° 旋转动画 —— 通过 AIS_AnimationObject + 宿主动画通道,
//     由 AIS_ViewController 在事件循环里驱动 (main 里 FlushViewEvents)。
// =====================================================================
bool MyAisOwner::HandleMouseClick (const Graphic3d_Vec2i& thePoint,
                                   Aspect_VKeyMouse theButton,
                                   Aspect_VKeyFlags theModifiers,
                                   bool theIsDoubleClick)
{
  (void )thePoint; (void )theButton; (void )theModifiers; (void )theIsDoubleClick;

  // 1) 随机换色
  static math_BullardGenerator aRandGen;
  Quantity_Color aRandColor (float(aRandGen.NextInt() % 256) / 255.0f,
                             float(aRandGen.NextInt() % 256) / 255.0f,
                             float(aRandGen.NextInt() % 256) / 255.0f,
                             Quantity_TOC_sRGB);
  mySelectable->Attributes()->ShadingAspect()->SetColor (aRandColor);
  mySelectable->SynchronizeAspects();

  // 2) 旋转动画 (教程为简单用了 static 变量, 生产代码别这么做)
  if (!myAnim.IsNull())
  {
    static bool isFirst = true;
    isFirst = !isFirst;
    MyAisObject* anObj = dynamic_cast<MyAisObject*> (mySelectable);

    gp_Trsf aTrsfTo;
    aTrsfTo.SetRotation (gp_Ax1 (gp::Origin(), gp::DX()),
                         isFirst ? M_PI * 0.5 : -M_PI * 0.5);
    gp_Trsf aTrsfFrom = anObj->LocalTransformation();
    Handle(AIS_AnimationObject) anAnim =
      new AIS_AnimationObject ("MyAnim", anObj->InteractiveContext(), anObj, aTrsfFrom, aTrsfTo);
    anAnim->SetOwnDuration (2.0);

    myAnim->Clear();
    myAnim->Add (anAnim);
    myAnim->StartTimer (0.0, 1.0, true);
  }
  return true; // true = 事件已消化, 不再走默认选择逻辑
}
