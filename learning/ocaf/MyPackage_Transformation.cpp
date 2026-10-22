// MyPackage_Transformation — 实现 (源: dox/samples/ocaf.md, 修正了原文 Dump() 的两处笔误:
// 1) 原文实现体里使用了未声明的 anOS (参数实为 theOS); 2) 原文声明/实现 const 不一致)
#include "MyPackage_Transformation.hxx"

#include <Standard_GUID.hxx>
#include <TDF_Label.hxx>
#include <TDataStd_RealArray.hxx>

//=======================================================================
// GetID — 属性唯一 GUID
//=======================================================================
const Standard_GUID& MyPackage_Transformation::GetID()
{
  static Standard_GUID anID ("4443368E-C808-4468-984D-B26906BA8573");
  return anID;
}

//=======================================================================
// Set — Find-or-Create 惯用法
//=======================================================================
Handle(MyPackage_Transformation) MyPackage_Transformation::Set (const TDF_Label& theLabel)
{
  Handle(MyPackage_Transformation) aT;
  if (!theLabel.FindAttribute (MyPackage_Transformation::GetID(), aT))
  {
    aT = new MyPackage_Transformation();
    theLabel.AddAttribute (aT);
  }
  return aT;
}

//=======================================================================
// StoreMatrix — 把变换矩阵镜像到 TDataStd_RealArray(1,12) (标准属性, 自动持久化)
//=======================================================================
void MyPackage_Transformation::StoreMatrix (const TDF_Label& theLabel, const gp_Trsf& theTrsf)
{
  Handle(TDataStd_RealArray) anArr = TDataStd_RealArray::Set (theLabel, 1, 12);
  int aK = 1;
  for (int i = 1; i <= 3; ++i)
  {
    for (int j = 1; j <= 4; ++j, ++aK)
    {
      anArr->SetValue (aK, theTrsf.Value (i, j));
    }
  }
}

//=======================================================================
// ReadMatrix — 从 TDataStd_RealArray(1,12) 恢复变换
//=======================================================================
Standard_Boolean MyPackage_Transformation::ReadMatrix (const TDF_Label& theLabel, gp_Trsf& theTrsf)
{
  Handle(TDataStd_RealArray) anArr;
  if (!theLabel.FindAttribute (TDataStd_RealArray::GetID(), anArr)
      || anArr->Length() < 12)
  {
    return false;
  }
  theTrsf.SetValues (anArr->Value(1),  anArr->Value(2),  anArr->Value(3),  anArr->Value(4),
                     anArr->Value(5),  anArr->Value(6),  anArr->Value(7),  anArr->Value(8),
                     anArr->Value(9),  anArr->Value(10), anArr->Value(11), anArr->Value(12));
  return true;
}

//=======================================================================
// Get — 按记录的类型组装 gp_Trsf
//=======================================================================
gp_Trsf MyPackage_Transformation::Get() const
{
  gp_Trsf aTrsf;
  switch (myType)
  {
    case gp_Rotation:     aTrsf.SetRotation (myAx1, myAngle); break;
    case gp_Translation:  aTrsf.SetTranslation (myFirstPoint, mySecondPoint); break;
    case gp_PntMirror:    aTrsf.SetMirror (myFirstPoint); break;
    case gp_Ax1Mirror:    aTrsf.SetMirror (myAx1); break;
    case gp_Ax2Mirror:    aTrsf.SetMirror (myAx2); break;
    case gp_Scale:        aTrsf.SetScale (myFirstPoint, myScale); break;
    case gp_CompoundTrsf: aTrsf.SetTransformation (myFirstAx3, mySecondAx3); break;
    case gp_Identity:
    case gp_Other:
    default: break;
  }
  return aTrsf;
}

//=======================================================================
// 各 Set 方法 — 修改前 Backup() 打快照, 撤销/重做才有 delta 可回滚
//=======================================================================
void MyPackage_Transformation::SetRotation (const gp_Ax1& theAxis, const Standard_Real theAngle)
{
  Backup();
  myType = gp_Rotation;
  myAx1  = theAxis;
  myAngle = theAngle;
}

void MyPackage_Transformation::SetTranslation (const gp_Vec& theVector)
{
  Backup();
  myType = gp_Translation;
  myFirstPoint.SetCoord (0.0, 0.0, 0.0);
  mySecondPoint.SetCoord (theVector.X(), theVector.Y(), theVector.Z());
}

void MyPackage_Transformation::SetMirror (const gp_Pnt& thePoint)
{
  Backup();
  myType = gp_PntMirror;
  myFirstPoint = thePoint;
}

void MyPackage_Transformation::SetMirror (const gp_Ax1& theAxis)
{
  Backup();
  myType = gp_Ax1Mirror;
  myAx1 = theAxis;
}

void MyPackage_Transformation::SetMirror (const gp_Ax2& thePlane)
{
  Backup();
  myType = gp_Ax2Mirror;
  myAx2 = thePlane;
}

void MyPackage_Transformation::SetScale (const gp_Pnt& thePoint, const Standard_Real theScale)
{
  Backup();
  myType = gp_Scale;
  myFirstPoint = thePoint;
  myScale = theScale;
}

void MyPackage_Transformation::SetTransformation (const gp_Ax3& theFrom, const gp_Ax3& theTo)
{
  Backup();
  myType = gp_CompoundTrsf;
  myFirstAx3  = theFrom;
  mySecondAx3 = theTo;
}

//=======================================================================
// TDF_Attribute 虚方法
//=======================================================================
const Standard_GUID& MyPackage_Transformation::ID() const
{
  return GetID();
}

void MyPackage_Transformation::Restore (const Handle(TDF_Attribute)& theAttribute)
{
  Handle(MyPackage_Transformation) aT = Handle(MyPackage_Transformation)::DownCast (theAttribute);
  myType        = aT->myType;
  myAx1         = aT->myAx1;
  myAx2         = aT->myAx2;
  myFirstAx3    = aT->myFirstAx3;
  mySecondAx3   = aT->mySecondAx3;
  myAngle       = aT->myAngle;
  myScale       = aT->myScale;
  myFirstPoint  = aT->myFirstPoint;
  mySecondPoint = aT->mySecondPoint;
}

Handle(TDF_Attribute) MyPackage_Transformation::NewEmpty() const
{
  return new MyPackage_Transformation();
}

void MyPackage_Transformation::Paste (const Handle(TDF_Attribute)& theAttribute,
                                      const Handle(TDF_RelocationTable)&) const
{
  Handle(MyPackage_Transformation) aT = Handle(MyPackage_Transformation)::DownCast (theAttribute);
  aT->myType        = myType;
  aT->myAx1         = myAx1;
  aT->myAx2         = myAx2;
  aT->myFirstAx3    = myFirstAx3;
  aT->mySecondAx3   = mySecondAx3;
  aT->myAngle       = myAngle;
  aT->myScale       = myScale;
  aT->myFirstPoint  = myFirstPoint;
  aT->mySecondPoint = mySecondPoint;
}

Standard_OStream& MyPackage_Transformation::Dump (Standard_OStream& theOS) const
{
  theOS << "Transformation: ";
  switch (myType)
  {
    case gp_Rotation:     theOS << "gp_Rotation"; break;
    case gp_Translation:  theOS << "gp_Translation"; break;
    case gp_PntMirror:    theOS << "gp_PntMirror"; break;
    case gp_Ax1Mirror:    theOS << "gp_Ax1Mirror"; break;
    case gp_Ax2Mirror:    theOS << "gp_Ax2Mirror"; break;
    case gp_Scale:        theOS << "gp_Scale"; break;
    case gp_CompoundTrsf: theOS << "gp_CompoundTrsf"; break;
    case gp_Identity:     theOS << "gp_Identity"; break;
    default:              theOS << "gp_Other"; break;
  }
  return theOS;
}

MyPackage_Transformation::MyPackage_Transformation()
: myType (gp_Identity)
{
}
