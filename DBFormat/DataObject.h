#ifndef __DATAOBJECT_H_
#define __DATAOBJECT_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\ADOImport\BasicDB.h"
#include "DataConst.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
class CTRndObject;
class CTEffect;
class CTexture;
class CTSound;
class CDebrisMaterial;
class CDoor;
class CGun;
class CRPGItem;
class CSoundEffect;
enum ESoundType;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPlacableObject: public CDBRecord
{
  OBJECT_BASIC_METHODS(CPlacableObject);
public:
	ZDATA_(CDBRecord)
	CPtr<CTRndObject> pObject;
	CPtr<CRPGItem> pRPGItem;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&pObject); f.Add(3,&pRPGItem); return 0; }

	virtual void Import() {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRndContainerModel: public CDBRecord
{
  OBJECT_BASIC_METHODS(CRndContainerModel);
public:
	ZDATA_(CDBRecord)
  CPtr<CTRndModel> pModel;
	CPtr<CTEffect> pEffect;
	CPtr<CTexture> pSLightMask;
	CVec3 ptPLightCr;
	CVec3 ptSLightCr;
	float fPLightRadius;
	float fSLightRadius;
	float fSLightFOV;
	CVec3 ptEffectPos;
	CVec3 ptPLightPos;
	CVec3 ptSLightPos;
	CVec3 ptSLightDir;
	CVec3 ptAmbientColor;
	CPtr<CTSound> pSound, pDestroySound;
	CVec3 ptSoundPos;
	ESoundType eSoundType;
	float fSoundAvgInterval;
	float fPFlareRadius;
	CPtr<CTexture> pPFlareTexture;
	CPtr<CSoundEffect> pSoundEffect;
	CVec3 ptPLightFlarePos;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&pModel); f.Add(3,&pEffect); f.Add(4,&pSLightMask); f.Add(5,&ptPLightCr); f.Add(6,&ptSLightCr); f.Add(7,&fPLightRadius); f.Add(8,&fSLightRadius); f.Add(9,&fSLightFOV); f.Add(10,&ptEffectPos); f.Add(11,&ptPLightPos); f.Add(12,&ptSLightPos); f.Add(13,&ptSLightDir); f.Add(14,&ptAmbientColor); f.Add(15,&pSound); f.Add(16,&pDestroySound); f.Add(17,&ptSoundPos); f.Add(18,&eSoundType); f.Add(19,&fSoundAvgInterval); f.Add(20,&fPFlareRadius); f.Add(21,&pPFlareTexture); f.Add(22,&pSoundEffect); f.Add(23,&ptPLightFlarePos); f.Add(24,&bPLightShadow); f.Add(25,&pAttachedGrenade); return 0; }
	bool bPLightShadow = false;
	CPtr<CRPGGrenade> pAttachedGrenade;

  virtual void Import();
	CContainerModel* CreateContainer( SRand *pRand, const vector<int> &flags );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRndObject: public CDBRecord 
{
	OBJECT_BASIC_METHODS( CRndObject );
public:
	CPtr<CRndContainerModel> pModels[N_DESTROY_STAGES];
	CPtr<CDebrisMaterial> pDebrisMaterial;
	bool bTargetable, bIsDeploySpot;
	CPtr<CTRndObject> pTemplate;
	vector<SVariantFlags> flags;
	CPtr<CTRndObject> pChild;
	bool bKeepDecals;
	CPtr<CRPGGrenade> pGrenade;	// DB col "RPGGrenade" -- explodable-object self-detonation grenade; copied to CObject::pGrenade in CreateObject
	//
	virtual void Import();
	int operator&( CStructureSaver &f );
	CObject* CreateObject( SRand *pRand, const vector<int> &params );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTRndObject : public CRndPtr<CRndObject>
{
	OBJECT_BASIC_METHODS(CTRndObject);
public:
	ZDATA_(CRndPtr<CRndObject>)
	CPtr<CDoor> pDoor;
	CPtr<CGun> pGun;
	CPtr<CPlacableObject> pPlacable;
	CPtr<CPassageObject> pPassage;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CRndPtr<CRndObject>*)this); f.Add(2,&pDoor); f.Add(3,&pGun); f.Add(4,&pPlacable); f.Add(5,&pPassage); return 0; }
	CObject* CreateObject( SRand *pRand );
	CObject* CreateObject( SRand *pRand, const vector<int> &flags );
	void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CTRndObject* GetTRndObject( int nID );
CRndObject* GetDBRndObject( int nID );
CPlacableObject* GetPlacableObject( int nID );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __DATAOBJECT_H_