#ifndef __DATATERRAIN_H_
#define __DATATERRAIN_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\ADOImport\BasicDB.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
class CSound;
class CTexture;
class CTMaterial;
class CTSound;
class CRPGArmor;
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EHMBlendType
{
	BT_NORMAL,
	BT_ADD,
	BT_SUBTRACT
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTerrainTile: public CDBRecord
{
	OBJECT_BASIC_METHODS(CTerrainTile);
public:
	ZDATA_(CDBRecord)
	int nPriority;
	int nMaskVariants;
	int nTextureVariants;
	CPtr<CTexture> pBump;
	CPtr<CTexture> pMask;
	CPtr<CTexture> pTexture;
	CPtr<CRPGArmor> pArmor;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&nPriority); f.Add(3,&nMaskVariants); f.Add(4,&nTextureVariants); f.Add(5,&pBump); f.Add(6,&pMask); f.Add(7,&pTexture); f.Add(8,&pArmor); return 0; }
	//
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CGrass: public CDBRecord
{
	OBJECT_BASIC_METHODS(CGrass);
public:
	ZDATA_(CDBRecord)
	CPtr<CTexture> pTexture;
	CVec2 ptScale;
	CVec2 ptPivot;
	CPtr<CTMaterial> pSpotMaterial;
	float fSpotScale;
	int nSideSize; // texture consists of (nSideSize x nSideSize) sprites
	float fScaleRange;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&pTexture); f.Add(3,&ptScale); f.Add(4,&ptPivot); f.Add(5,&pSpotMaterial); f.Add(6,&fSpotScale); f.Add(7,&nSideSize); f.Add(8,&fScaleRange); return 0; }
	
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSpot: public CDBRecord
{
	OBJECT_BASIC_METHODS(CSpot);
public:
	ZDATA_(CDBRecord)
	CPtr<CTMaterial> pMaterial;
	CPtr<CRPGArmor> pArmor;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&pMaterial); f.Add(3,&pArmor); return 0; }
	//
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CGrass* GetGrass( int nID );
CTerrainTile* GetTerrainTile( int nID );
CSpot* GetSpot( int nID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __DATATERRAIN_H_
