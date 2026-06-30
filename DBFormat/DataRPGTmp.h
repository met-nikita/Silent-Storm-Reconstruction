#ifndef __DATARPGTMP_H_
#define __DATARPGTMP_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "DataRPG.h"
#include "DataMisc.h"	// CRPGPicklock, for the release-added RPGPicklocks4Pers join table
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
class CDBDifficulty;   // fwd for CRPGClip4Pers::pDifficulty (release save-format; CPtr needs only fwd-decl)
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRPGLink4Pers: public CDBRecord
{
public:
	ZDATA_(CDBRecord)
	CPtr<CRPGPers>	pPers;
	int nQuantity;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&pPers); f.Add(3,&nQuantity); return 0; }
};
template <class L> void ImportItem( L *pLink );
template<class T>
class CRPGSomethingForPers : public CRPGLink4Pers
{
	OBJECT_BASIC_METHODS(CRPGSomethingForPers);
public:
	ZDATA_(CRPGLink4Pers)
	CPtr<T> pItem;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CRPGLink4Pers*)this); f.Add(2,&pItem); return 0; }
	virtual void Import()
	{
		ImportItem(this);
	}
};
typedef CRPGSomethingForPers<CRPGWeapon> CRPGWeapon4Pers;
typedef CRPGSomethingForPers<CRPGGrenade> CRPGGrenade4Pers;
typedef CRPGSomethingForPers<CRPGFirstAid> CRPGFirstAid4Pers;
typedef CRPGSomethingForPers<CRPGMeleeWeapon> CRPGMeleeWeapon4Pers;
typedef CRPGSomethingForPers<CRPGMineDetector> CRPGMineDetector4Pers;
typedef CRPGSomethingForPers<CRPGMine> CRPGMine4Pers;
typedef CRPGSomethingForPers<CRPGTool> CRPGTool4Pers;
typedef CRPGSomethingForPers<CRPGKey> CRPGKey4Pers;
// release-added per-person inventory join tables (0xE0000026 / 0xE0000028)
typedef CRPGSomethingForPers<CRPGPicklock> CRPGPicklock4Pers;
typedef CRPGSomethingForPers<CRPGEngGrenade> CRPGEngGrenade4Pers;
/*class CRPGWeapon4Pers: public CRPGLink4Pers
{
	OBJECT_BASIC_METHODS(CRPGWeapon4Pers);
public:
	ZDATA_(CRPGLink4Pers)
	CPtr<CRPGWeapon> pItem;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CRPGLink4Pers*)this); f.Add(2,&pItem); return 0; }
	virtual void Import();
};*/
class CRPGClip4Pers: public CRPGLink4Pers
{
	OBJECT_BASIC_METHODS(CRPGClip4Pers);
public:
	ZDATA_(CRPGLink4Pers)
	CPtr<CRPGClip> pItem;
	CPtr<CRPGAmmo> pAmmo;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CRPGLink4Pers*)this); f.Add(2,&pItem); f.Add(3,&pAmmo); f.Add(4,&pDifficulty); return 0; }
	CPtr<CDBDifficulty> pDifficulty;
	virtual void Import();
};
/*class CRPGGrenade4Pers: public CRPGLink4Pers
{
	OBJECT_BASIC_METHODS(CRPGGrenade4Pers);
public:
	ZDATA_(CRPGLink4Pers)
	CPtr<CRPGGrenade> pItem;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CRPGLink4Pers*)this); f.Add(2,&pItem); return 0; }
	virtual void Import();
};
class CRPGFirstAid4Pers: public CRPGLink4Pers
{
	OBJECT_BASIC_METHODS(CRPGFirstAid4Pers);
public:
	ZDATA_(CRPGLink4Pers)
	CPtr<CRPGFirstAid> pItem;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CRPGLink4Pers*)this); f.Add(2,&pItem); return 0; }
	virtual void Import();
};
class CRPGMeleeWeapon4Pers: public CRPGLink4Pers
{
	OBJECT_BASIC_METHODS(CRPGMeleeWeapon4Pers);
public:
	ZDATA_(CRPGLink4Pers)
	CPtr<CRPGMeleeWeapon> pItem;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CRPGLink4Pers*)this); f.Add(2,&pItem); return 0; }
	virtual void Import();
};
class CRPGMineDetector4Pers: public CRPGLink4Pers
{
	OBJECT_BASIC_METHODS(CRPGMineDetector4Pers);
public:
	ZDATA_(CRPGLink4Pers)
	CPtr<CRPGMineDetector> pItem;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CRPGLink4Pers*)this); f.Add(2,&pItem); return 0; }
	virtual void Import();
};
class CRPGMine4Pers: public CRPGLink4Pers
{
	OBJECT_BASIC_METHODS(CRPGMine4Pers);
public:
	ZDATA_(CRPGLink4Pers)
	CPtr<CRPGMine> pItem;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CRPGLink4Pers*)this); f.Add(2,&pItem); return 0; }
	virtual void Import();
};*/
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRPGItem2Uniform: public CDBRecord
{
	OBJECT_BASIC_METHODS(CRPGItem2Uniform);
public:
	ZDATA_(CDBRecord)
	CPtr<CRPGItem> pItem;
	CPtr<CRPGUniform> pUniform;
	CPtr<CTRndModel> pModelActive;
	CPtr<CTRndModel> pModelInactive;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&pItem); f.Add(3,&pUniform); f.Add(4,&pModelActive); f.Add(5,&pModelInactive); return 0; }
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif