// OCAF 学习教程 (源: OCCT-7_9_3/dox/samples/ocaf.md + samples/ocafsamples/)
// 控制台程序, 依次演示 5 课:
//   第1课 创建应用与文档 (现代 DefineFormat, 替代文档里的 Plugin/Resources 文件机制)
//   第2课 标签树与标准属性 (Name/Integer/Real/Comment/Reference)
//   第3课 自定义属性 + 撤销/重做 (MyPackage_Transformation)
//   第4课 TNaming 把形状存进框架
//   第5课 保存 / 关闭 / 打开 (持久化验证)
#include "MyPackage_Transformation.hxx"

#include <TDocStd_Application.hxx>
#include <TDocStd_Document.hxx>
#include <TDataStd_Name.hxx>
#include <TDataStd_Integer.hxx>
#include <TDataStd_Real.hxx>
#include <TDataStd_Comment.hxx>
#include <TDF_Reference.hxx>
#include <TDF_ChildIterator.hxx>
#include <TNaming_NamedShape.hxx>
#include <TNaming_Builder.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <BinDrivers_DocumentRetrievalDriver.hxx>
#include <BinDrivers_DocumentStorageDriver.hxx>

#include <algorithm>
#include <cmath>
#include <iostream>

namespace
{
  //! 打印标签上的常用属性摘要 (名字/整数/自定义属性...)
  void dumpLabel (const TDF_Label& theLabel, const char* theTag)
  {
    Handle(TDataStd_Name) aName;
    theLabel.FindAttribute (TDataStd_Name::GetID(), aName);
    Handle(TDataStd_Integer) anInt;
    theLabel.FindAttribute (TDataStd_Integer::GetID(), anInt);
    Handle(MyPackage_Transformation) aTrsf;
    theLabel.FindAttribute (MyPackage_Transformation::GetID(), aTrsf);

    std::cout << "  [" << theTag << "] 标签";
    if (!aName.IsNull())
    {
      std::cout << "  Name=\"" << aName->Get() << "\"";
    }
    if (!anInt.IsNull())
    {
      std::cout << "  Integer=" << anInt->Get();
    }
    if (!aTrsf.IsNull())
    {
      std::cout << "  ";
      aTrsf->Dump (std::cout);
    }
    std::cout << "\n";
  }
}

int main()
{
  // ================= 第 1 课: 创建应用与文档 =================
  // ocaf.md 教的是传统机制: 继承 TDocStd_Application 重载 Formats()/ResourcesName(),
  // 再手写 Plugin + Resources 文件 + 设置 CSF_PluginDefaults/CSF_ResourcesDefaults 环境变量。
  // OCCT 7.9 现代方式: 一行 DefineFormat 注册存储/读取驱动, 零配置文件 (本文采用)。
  Handle(TDocStd_Application) anApp = new TDocStd_Application();
  // 用 TKBin 的完整 OCAF 二进制驱动 (而非 TKBinL 轻量驱动), 才能持久化 TNaming 形状;
  // 格式名仍用文档里的 OCAF-myApplication / .sta 后缀
  anApp->DefineFormat ("OCAF-myApplication",
                       "MyApplication Document Version 1.0",
                       "sta",
                       new BinDrivers_DocumentRetrievalDriver(),
                       new BinDrivers_DocumentStorageDriver());

  Handle(TDocStd_Document) aDoc;
  anApp->NewDocument ("OCAF-myApplication", aDoc);
  std::cout << "=== 第1课: 应用与文档创建成功, 格式=OCAF-myApplication (.sta) ===\n\n";

  // ================= 第 2 课: 标签树与标准属性 =================
  // OCAF 数据框架 = 一棵标签树, 每个标签可挂任意多个属性 (GUID 区分)
  TDF_Label aRoot = aDoc->Main();                       // 文档根标签
  TDataStd_Name::Set (aRoot, "Assembly");               // 根命名

  TDF_Label aBoxLabel = aRoot.FindChild (1);            // 子标签 1: 零件
  TDataStd_Name::Set    (aBoxLabel, "Box");
  TDataStd_Integer::Set (aBoxLabel, 1);                 // 数量 = 1
  TDataStd_Comment::Set (aBoxLabel, "示例零件: 50x50x50 立方体");

  TDF_Label aDimLabel = aBoxLabel.FindChild (1);        // 孙标签: 尺寸参数
  TDataStd_Real::Set (aDimLabel, 50.0);

  TDF_Label aRefLabel = aRoot.FindChild (3);            // 子标签 3: 引用
  TDF_Reference::Set (aRefLabel, aBoxLabel);            // 指向零件标签的持久引用

  TDF_Label aConeLabel = aRoot.FindChild (2);           // 子标签 2: 第二个零件
  TDataStd_Name::Set    (aConeLabel, "Cone");
  TDataStd_Integer::Set (aConeLabel, 1);

  std::cout << "=== 第2课: 标签树与标准属性 ===\n";
  dumpLabel (aRoot,      "根");
  dumpLabel (aBoxLabel,  "零件1");
  dumpLabel (aDimLabel,  "参数");
  dumpLabel (aConeLabel, "零件2");
  std::cout << "\n";

  // ================= 第 3 课: 自定义属性 + 撤销/重做 =================
  // 注意: 默认撤销是禁用的, 必须先 SetUndoLimit;
  // 且每组修改必须包裹在 NewCommand/CommitCommand 事务里, 才会产生可撤销的 delta
  aDoc->SetUndoLimit (10);

  // ---- 每个零件一个独立的变换属性: Box=旋转45°, Cone=平移(100,0,0) ----
  // (数据驱动显示: viewer 读这些属性并作用到显示对象上, 两个零件姿态明显不同)
  Handle(MyPackage_Transformation) aTrsfBox  = MyPackage_Transformation::Set (aBoxLabel);
  Handle(MyPackage_Transformation) aTrsfCone = MyPackage_Transformation::Set (aConeLabel);

  aDoc->NewCommand();                                    // ── 事务 1 (纯演示: 最终会被事务 2 覆盖) ──
  aTrsfBox->SetTranslation (gp_Vec (30.0, 0.0, 0.0));
  aDoc->CommitCommand();
  std::cout << "=== 第3课: 自定义属性 + 撤销/重做 ===\n";
  std::cout << "  事务1: Box 设平移(30,0,0)后: ";
  aTrsfBox->Dump (std::cout); std::cout << "\n";

  aDoc->NewCommand();                                    // ── 事务 2 (Box 最终状态) ──
  aTrsfBox->SetRotation (gp_Ax1 (gp_Pnt (0, 0, 0), gp_Dir (10, 0, 1)), M_PI / 4.0);
  MyPackage_Transformation::StoreMatrix (aBoxLabel, aTrsfBox->Get()); // ★ 同步矩阵镜像→随文档持久化, viewer 读的是这个
  aDoc->CommitCommand();
  std::cout << "  事务2: Box 再设旋转45°后: ";
  aTrsfBox->Dump (std::cout); std::cout << "\n";

  aDoc->Undo();
  std::cout << "  Undo 后 (应回到 Translation): ";
  aTrsfBox->Dump (std::cout); std::cout << "\n";

  aDoc->Redo();
  std::cout << "  Redo 后 (应回到 Rotation): ";
  aTrsfBox->Dump (std::cout); std::cout << "\n";

  aDoc->NewCommand();                                    // ── 事务 3: Cone ──
  aTrsfCone->SetTranslation (gp_Vec (500.0, 0.0, 0.0));
  MyPackage_Transformation::StoreMatrix (aConeLabel, aTrsfCone->Get()); // ★ 同步矩阵镜像
  aDoc->CommitCommand();
  std::cout << "  事务3: Cone 设平移(100,0,0)后: ";
  aTrsfCone->Dump (std::cout); std::cout << "\n\n";

  // ================= 第 4 课: TNaming 把形状存进框架 =================
  // TNaming_NamedShape 属性把 TopoDS_Shape 挂到标签上, 并记录"演化历史" (PRIMITIVE/ MODIFY...)
  // 注意: 形状挂在各自零件标签上 (变换属性也在同一标签 → viewer 可以一起读取)
  BRepPrimAPI_MakeBox aMakeBox (50.0, 50.0, 50.0);
  TNaming_Builder aBoxBuilder (aBoxLabel);
  aBoxBuilder.Generated (aMakeBox.Shape());   // 新图元 → PRIMITIVE 演化

  gp_Ax2 aConeAxis (gp_Pnt (25.0, 125.0, 0.0), gp_Dir (0, 0, 1));
  BRepPrimAPI_MakeCone aMakeCone (aConeAxis, 55.0, 0.0, 70.0);
  TNaming_Builder aConeBuilder (aConeLabel);
  aConeBuilder.Generated (aMakeCone.Shape());

  std::cout << "=== 第4课: TNaming 形状已挂到各自零件标签 (演化=PRIMITIVE) ===\n";
  Handle(TNaming_NamedShape) aNS;
  if (aBoxLabel.FindAttribute (TNaming_NamedShape::GetID(), aNS))
  {
    Bnd_Box aB;
    BRepBndLib::Add (aNS->Get(), aB);
    double aX1, aY1, aZ1, aX2, aY2, aZ2;
    aB.Get (aX1, aY1, aZ1, aX2, aY2, aZ2);
    std::cout << "  Box 包围盒: [" << aX1 << "," << aY1 << "," << aZ1 << "] - ["
              << aX2 << "," << aY2 << "," << aZ2 << "]\n";
  }
  std::cout << "\n";

  // ================= 第 5 课: 保存 / 关闭 / 打开 =================
  const TCollection_ExtendedString aPath ("ocaf_tutorial.sta");
  anApp->SaveAs (aDoc, aPath);
  anApp->Close (aDoc);
  std::cout << "=== 第5课: 文档已保存为 ocaf_tutorial.sta 并关闭 ===\n";

  Handle(TDocStd_Document) aDoc2;
  anApp->Open (aPath, aDoc2);
  TDF_Label aRoot2 = aDoc2->Main();
  std::cout << "  重新打开后读回验证:\n";
  dumpLabel (aRoot2,  "根");
  dumpLabel (aRoot2.FindChild (1), "零件1");
  dumpLabel (aRoot2.FindChild (2), "零件2");

  Handle(TNaming_NamedShape) aNS2;
  aRoot2.FindChild (1).FindAttribute (TNaming_NamedShape::GetID(), aNS2);
  std::cout << "  NamedShape 读回: " << (aNS2.IsNull() ? "失败" : "成功") << "\n";

  Handle(MyPackage_Transformation) aTrsf2;
  aRoot2.FindChild (1).FindAttribute (MyPackage_Transformation::GetID(), aTrsf2);
  std::cout << "  自定义属性读回: " << (aTrsf2.IsNull()
      ? "不存在 (预期! 自定义属性不持久化)"
      : "成功")
      << "\n";

  gp_Trsf aMatrixTrsf;
  if (MyPackage_Transformation::ReadMatrix (aRoot2.FindChild (1), aMatrixTrsf))
  {
    gp_XYZ aT = aMatrixTrsf.TranslationPart();
    std::cout << "  变换矩阵(RealArray 镜像)读回: 成功, Box 平移=("
              << aT.X() << "," << aT.Y() << "," << aT.Z() << ")\n";
  }
  else
  {
    std::cout << "  变换矩阵(RealArray 镜像)读回: 失败\n";
  }

  anApp->Close (aDoc2);
  std::cout << "\n=== 全部课程完成 ===\n";
  return 0;
}
