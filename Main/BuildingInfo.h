#ifndef __BuildingInfo_H_
#define __BuildingInfo_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "GResource.h"
#include "..\Misc\2DArray.h"
#include "..\DBFormat\DataConst.h"
#include "..\DBFormat\DataFormat.h"
#include "DiscretePos.h"
struct SRand;
enum ELayer;
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NBuilding
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const float WALL_HEIGHT = 2.5f;  // высота этажа
const int WALL_MAX_LEN = 7;
////////////////////////////////////////////////////////////////////////////////////////////////////
inline int MakeFragmentID( ELayer type, int nLayer ) { return type << 16 | nLayer; }
inline void GetLayerID( int nFragmentID, ELayer *pType, int *pnLayer )
{
	*pnLayer = nFragmentID & 0xffff;
	*pType = (ELayer)(nFragmentID >> 16 & 0xffff);
}
inline ELayer GetLayerType( int nLayerID ) { return (ELayer)(nLayerID >> 16 & 0xffff); }
////////////////////////////////////////////////////////////////////////////////////////////////////
enum ELadderDirection
{
	LD_FORW = 0,
	LD_BACKW,
	LD_LEFT,
	LD_RIGHT
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SLadder
{
	ZDATA
	int nID;
	SDiscretePos pos;						//положение- x,y в тайлах, z-начальный этаж
	int nHeight;								//высота лестницы в этажах
	// 
	float fBeginHeight;					//высота начальной точки
	float fEndHeight;						//высота конечной точки
	ELadderDirection eDir;			//сторона, по которой можно лезть
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nID); f.Add(3,&pos); f.Add(4,&nHeight); f.Add(5,&fBeginHeight); f.Add(6,&fEndHeight); f.Add(7,&eDir); return 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SProjectedSpot
{
	ZDATA
	CVec3 ptOrigin;
	CVec3 ptNormal;
	CVec2 ptSize;
	int nRotation;
	int nMaterialID;
	int nID;
	int nMaterialMask; // если включен бит n, значит на геометрию с материалом n спот не накладывается
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&ptOrigin); f.Add(3,&ptNormal); f.Add(4,&ptSize); f.Add(5,&nRotation); f.Add(6,&nMaterialID); f.Add(7,&nID); f.Add(8,&nMaterialMask); return 0; }
	SProjectedSpot(): nID(0), nMaterialMask(0) {}
	bool IsMaterialEnabled( int nMaterial ) const { return !(nMaterialMask & (1 << nMaterial)); }
	void EnableMaterial( int nMaterial, bool bEnable = true ) { nMaterialMask = (nMaterialMask & (~(1 << nMaterial))) | ((int)(!bEnable) << nMaterial); }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SMaterialApply
{
	enum EMapping { NORMAL, PROJECTION_X, PROJECTION_Y, PROJECTION_Z };

	ZDATA
	EMapping mapping;
	CDBPtr<NDb::CMaterial> pMaterial;
	float fScale;
	CVec2 ptShift;
	int   nRotation;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&mapping); f.Add(3,&pMaterial); f.Add(4,&fScale); f.Add(5,&ptShift); f.Add(6,&nRotation); return 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMixedMaterial: public CObjectBase
{
	OBJECT_BASIC_METHODS(CMixedMaterial);
public:
	ZDATA
	vector<SMaterialApply> layers;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&layers); return 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// струкутра, которая хранится в бинарном ресурсном файле
struct SRawMaterialApply
{
	ZDATA
	SMaterialApply::EMapping mapping;
	int   nTMaterialID;
	float fScale;
	CVec2 ptShift;
	int   nRotation;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&mapping); f.Add(3,&nTMaterialID); f.Add(4,&fScale); f.Add(5,&ptShift); f.Add(6,&nRotation); return 0; }
	SRawMaterialApply() { Zero(*this); }
	bool operator==(const SRawMaterialApply &op ) const { return 0 == memcmp( this, &op, sizeof(op) );
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SRawMixedMaterial
{
	ZDATA
	vector<SRawMaterialApply> layers;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&layers); return 0; }
	CMixedMaterial* CreateMixedMaterial( SRand *pRand ) const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
const int OF_OPEN = 0x1; // дверь\окно открыты 
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SBuildFragment
{
	int   nConstructionPartID;
	int   nSubBlockID;
	CVec3 ptPos;						// x, y измеряется в игровых тайлах; z в этажах, промежуточный этаж: z - int(z) = 0.5
	int   nRotationID;
	int   nFragmentID;      // тип и номер слоя в котором находится фрагмент
	int   nObjectFlags;			// флажки для объекта встроенного в фрагмент
	SRawMixedMaterial materials[NDb::N_CONSTRUCTION_MATERIALS];
	vector<int> spots;      // id'шники спотов
	int   nID;

	SBuildFragment(): nID(0) {}
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SLayerGroup
{
	ZDATA
	vector<int> layers;
	vector<int> floor;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&layers); f.Add(3,&floor); return 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CBuildInfo: public CObjectBase
{
	OBJECT_BASIC_METHODS(CBuildInfo);
public:
	ZDATA
	vector<SBuildFragment>  wallFragments;
	vector<SBuildFragment>  solidFragments;
	vector<CArray2D<BYTE> > roomMap;
	vector<SProjectedSpot>  spots;
	int   nMaxY;
	int   nMaxX;
	int   nMinFloor;
	int   nMaxFloor;
	vector<SLadder> ladders;
	CArray2D<bool>  cellar; // true <=> hole
	vector<SLayerGroup> lgroups;
	int nMaxSpotID; // nID, который будет присвоен следующему добавленному споту
	int nMaxLadderID;
	int nMaxFragmentID;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&wallFragments); f.Add(3,&solidFragments); f.Add(4,&roomMap); f.Add(5,&spots); f.Add(6,&nMaxY); f.Add(7,&nMaxX); f.Add(8,&nMinFloor); f.Add(9,&nMaxFloor); f.Add(10,&ladders); f.Add(11,&cellar); f.Add(12,&lgroups); f.Add(13,&nMaxSpotID); f.Add(14,&nMaxLadderID); f.Add(15,&nMaxFragmentID); return 0; }
	CBuildInfo();
	void GetSpotFragments( int nSpotID, vector<int> *pSolids, vector<int> *pWalls ) const; // возвращает индексы фрагментов для спота nSpotID
	int  CreateNextFragmentID();
	int  CreateNextSpotID();
	int  CreateNextLadderID();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CBuildInfoLoader: public NGScene::CResourceLoader<int, CBuildInfo>
{
	OBJECT_BASIC_METHODS(CBuildInfoLoader);
protected:
	virtual void Recalc();	
};
//
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif
