#ifndef __DATACHEST_H_
#define __DATACHEST_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// Release-added RPG chest / loot cluster (tables 0x6a RPGChestTemplates, 0x6b RPGLootInstances,
// 0x6c RPGChests, 0x72 RPGChestLayouts). Reconstructed from the release Game.exe + matched PDB:
// operator& tags and Import column names are verbatim from the decompile. CTRPGChest is a weighted
// random pool (CRndPtr<CRPGChest>) just like CTRndModel/CTRndObject; each CRPGChest variant pushes
// itself into its template's roulette via the standard PushItemWithWeight idiom (RndWeight column).
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\ADOImport\BasicDB.h"
#include "DataConst.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRPGItem;
class CDBDifficulty;
class CRPGChest;
class CTRPGChest;
class CRPGLootInstances;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRPGLootInstances - one item-with-quantity entry that belongs to a CRPGChest (ChestID relation).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRPGLootInstances: public CDBRecord
{
	OBJECT_BASIC_METHODS( CRPGLootInstances );
public:
	ZDATA_(CDBRecord)
	CPtr<CRPGItem> pItem;
	int nQuantity;
	int nMaxQuantity;
	CPtr<CDBDifficulty> pDifficulty;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&pItem); f.Add(3,&nQuantity); f.Add(4,&nMaxQuantity); f.Add(5,&pDifficulty); return 0; }
	//
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTRPGChest - weighted random pool of CRPGChest variants. Serialization is inherited from CRndPtr.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTRPGChest : public CRndPtr<CRPGChest>
{
	OBJECT_BASIC_METHODS( CTRPGChest );
public:
	ZDATA_(CRndPtr<CRPGChest>)
	ZEND
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRPGChest - a concrete chest variant (a row of RPGChests). Pushes itself into pTemplate's pool.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRPGChest: public CDBRecord
{
	OBJECT_BASIC_METHODS( CRPGChest );
public:
	ZDATA_(CDBRecord)
	CPtr<CTRPGChest> pTemplate;
	vector<SVariantFlags> flags;
	vector< CPtr<CRPGLootInstances> > instances;
	int nLevel;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&pTemplate); f.Add(3,&flags); f.Add(4,&instances); f.Add(5,&nLevel); return 0; }
	//
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRPGChestLayout - shelf geometry for the chest UI panel.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRPGChestLayout: public CDBRecord
{
	OBJECT_BASIC_METHODS( CRPGChestLayout );
public:
	ZDATA_(CDBRecord)
	vector<float> shelves;
	bool bVertical;
	bool bSafeLikeLayout;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&shelves); f.Add(3,&bVertical); f.Add(4,&bSafeLikeLayout); return 0; }
	//
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __DATACHEST_H_
