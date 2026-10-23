// ais_object.md 教程 — 自定义 SelectMgr_EntityOwner: 高亮 + 鼠标点击 + 动画
#ifndef MY_AIS_OWNER_H
#define MY_AIS_OWNER_H

#include <SelectMgr_EntityOwner.hxx>
#include <Prs3d_Presentation.hxx>
#include <AIS_Animation.hxx>

class MyAisObject;

//! 自定义 Owner。SelectMgr_EntityOwner 没有纯虚方法, 可直接实例化;
//! 子类化可以: 携带业务 id、定制高亮、响应鼠标点击。
class MyAisOwner : public SelectMgr_EntityOwner
{
  DEFINE_STANDARD_RTTI_INLINE(MyAisOwner, SelectMgr_EntityOwner)

public:
  MyAisOwner (const Handle(MyAisObject)& theObj, int thePriority = 0);

  void SetAnimation (const Handle(AIS_Animation)& theAnim) { myAnim = theAnim; }

  //! 高亮表现 (默认实现 = 主表现颜色调制; 这里换成包围盒 + 拾取点法线箭头)
  virtual void HilightWithColor (const Handle(PrsMgr_PresentationManager)& thePrsMgr,
                                 const Handle(Prs3d_Drawer)& theStyle,
                                 const Standard_Integer theMode) override;

  //! 取消高亮: 自建的 presentation 必须自己隐藏, 否则高亮残留
  virtual void Unhilight (const Handle(PrsMgr_PresentationManager)& thePrsMgr,
                          const Standard_Integer theMode) override;

  //! 强制每次鼠标移动都刷新高亮 (默认只在拾取对象变化时调用, 箭头会"冻住";
  //! 注意: 滥用会拖累性能, 仅在确需逐帧更新高亮的对象上使用)
  virtual bool IsForcedHilight() const override { return true; }

  //! 鼠标点击接口: 随机换色 + 启动旋转动画
  virtual bool HandleMouseClick (const Graphic3d_Vec2i& thePoint,
                                 Aspect_VKeyMouse theButton,
                                 Aspect_VKeyFlags theModifiers,
                                 bool theIsDoubleClick) override;

  //! 对象位置变化时 (动画) 同步高亮表现
  virtual void SetLocation (const TopLoc_Location& theLocation) override
  {
    if (!myPrs.IsNull())
    {
      myPrs->SetTransformation (new TopLoc_Datum3D (theLocation.Transformation()));
    }
  }

protected:
  Handle(Prs3d_Presentation) myPrs;    //!< 持久高亮表现 (包围盒), 选中时显示
  Handle(AIS_Animation)      myAnim;   //!< 宿主容器的动画通道
};

#endif // MY_AIS_OWNER_H
