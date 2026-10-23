// ais_object.md 教程 (OCCT-7_9_3/dox/samples/ais_object.md) — 自定义 AIS 交互对象
// 参考: OCCT-7_9_3/src/QADraw/QADraw_Tutorials.cxx (QATutorialAisObject 命令)
#ifndef MY_AIS_OBJECT_H
#define MY_AIS_OBJECT_H

#include <AIS_InteractiveObject.hxx>

class AIS_Animation;

//! 自定义交互对象: 一个 "手工三角化" 的圆柱体。
//! 演示 AIS 的两大独立机制:
//!  - Compute()        —— 表现 (presentation): 往 Prs3d_Presentation 里填充图元
//!  - ComputeSelection() —— 选择 (selection): 往 SelectMgr_Selection 里填充敏感实体
class MyAisObject : public AIS_InteractiveObject
{
  DEFINE_STANDARD_RTTI_INLINE(MyAisObject, AIS_InteractiveObject)

public:
  //! 显示模式枚举: AIS 不允许同时激活多个显示模式, 各模式互为备选;
  //! 非激活模式还可以用作高亮表现 (SetHilightMode)。
  enum MyDispMode
  {
    MyDispMode_Main      = 0, //!< 主模式: 着色圆柱 + 截面轮廓线段
    MyDispMode_Highlight = 1  //!< 辅助模式: 包围盒 (可用于高亮/选中表现)
  };

public:
  MyAisObject();

  //! 挂接动画容器 (通常来自 AIS_ViewController::ObjectsAnimation())
  void SetAnimation (const Handle(AIS_Animation)& theAnim) { myAnim = theAnim; }

public:
  //! 填充对象表现。theMode 即显示模式, 每个模式对应一个独立的 Prs3d_Presentation。
  virtual void Compute (const Handle(PrsMgr_PresentationManager)& thePrsMgr,
                        const Handle(Prs3d_Presentation)& thePrs,
                        const Standard_Integer theMode) override;

  //! 填充可拾取体积。第 0 选择模式 = 整个对象一个 Owner。
  virtual void ComputeSelection (const Handle(SelectMgr_Selection)& theSel,
                                 const Standard_Integer theMode) override;

  //! 声明支持的显示模式, 拒绝的模式连空 presentation 都不会创建。
  virtual bool AcceptDisplayMode (const Standard_Integer theMode) const override
  { return theMode == MyDispMode_Main || theMode == MyDispMode_Highlight; }

protected:
  Handle(AIS_Animation) myAnim;
};

#endif // MY_AIS_OBJECT_H
