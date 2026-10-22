// MyPackage_Transformation — 自定义 OCAF 属性 (学习用)
// 来源: OCCT-7_9_3/dox/samples/ocaf.md "Implementation of Attribute Transformation"
//
// OCAF 自定义属性四要素:
//   1. GetID()  — 属性唯一 GUID (同标签上靠它区分不同属性)
//   2. Set()    — Find-or-Create 惯用法 (已存在则返回, 否则新建挂上)
//   3. 备份     — 每个修改方法开头调用 Backup() → 撤销/重做才能回滚字段
//   4. Restore/NewEmpty/Paste — Undo/Redo 与复制粘贴所需的三个虚方法
#pragma once

#include <TDF_Attribute.hxx>

#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Ax3.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_TrsfForm.hxx>

class MyPackage_Transformation : public TDF_Attribute
{
public:
  //! 属性唯一 GUID — 靠它在同一标签的众多属性中被识别
  Standard_EXPORT static const Standard_GUID& GetID();

  //! Find-or-Create: 返回挂在 theLabel 上的本属性 (没有则创建)
  Standard_EXPORT static Handle(MyPackage_Transformation) Set (const TDF_Label& theLabel);

  //! 取出变换 (按记录的类型组装成 gp_Trsf)
  Standard_EXPORT gp_Trsf Get() const;

  //! 把变换矩阵镜像到 TDataStd_RealArray(1,12)。
  //! 自定义属性本身不随文档持久化 (需另写 BinM 驱动), 而标准属性会自动存盘 ——
  //! 镜像一份矩阵, 让 viewer 重开文档后仍能读到变换 (数据驱动显示)。
  Standard_EXPORT static void StoreMatrix (const TDF_Label& theLabel, const gp_Trsf& theTrsf);
  //! 从 TDataStd_RealArray(1,12) 读回变换; 不存在返回 false
  Standard_EXPORT static Standard_Boolean ReadMatrix (const TDF_Label& theLabel, gp_Trsf& theTrsf);

  // ---- 设置各类变换 (每次修改前 Backup() 以支持 Undo) ----
  Standard_EXPORT void SetRotation     (const gp_Ax1& theAxis, Standard_Real theAngle);
  Standard_EXPORT void SetTranslation  (const gp_Vec& theVector);
  Standard_EXPORT void SetMirror       (const gp_Pnt& thePoint);   // 点对称
  Standard_EXPORT void SetMirror       (const gp_Ax1& theAxis);    // 轴对称
  Standard_EXPORT void SetMirror       (const gp_Ax2& thePlane);   // 面对称
  Standard_EXPORT void SetScale        (const gp_Pnt& thePoint, Standard_Real theScale);
  Standard_EXPORT void SetTransformation (const gp_Ax3& theFrom, const gp_Ax3& theTo);

  // ---- TDF_Attribute 虚方法 ----
  Standard_EXPORT const Standard_GUID& ID() const override;
  //! Undo/Redo 时: 把 theAttribute 的内容拷进自身 (拷字段)
  Standard_EXPORT void Restore (const Handle(TDF_Attribute)& theAttribute) override;
  //! Copy/Paste、Undo/Redo 时: 创建本属性新实例
  Standard_EXPORT Handle(TDF_Attribute) NewEmpty() const override;
  //! Copy/Paste 时: 把自身内容拷进 theAttribute (基类为 const — 拷贝源不应被修改)
  Standard_EXPORT void Paste (const Handle(TDF_Attribute)& theAttribute,
                              const Handle(TDF_RelocationTable)& theRelocationTable) const override;
  //! 打印属性内容
  Standard_EXPORT Standard_OStream& Dump (Standard_OStream& theOS) const override;

  MyPackage_Transformation();

private:
  gp_TrsfForm myType;

  // 轴 (Ax1/Ax2/Ax3)
  gp_Ax1 myAx1;
  gp_Ax2 myAx2;
  gp_Ax3 myFirstAx3;
  gp_Ax3 mySecondAx3;

  // 标量
  Standard_Real myAngle;
  Standard_Real myScale;

  // 点
  gp_Pnt myFirstPoint;
  gp_Pnt mySecondPoint;
};
