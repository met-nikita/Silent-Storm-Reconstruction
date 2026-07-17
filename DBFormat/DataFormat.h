#ifndef __DATAFORMAT_H_
#define __DATAFORMAT_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\ADOImport\BasicDB.h"
#include "..\Misc\Geom.h"
#include "DataConst.h"
#include "DataGeometry.h"
#include "DataAnimation.h"
#include "DataRPG.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
// Hoisted from DataMap.cpp so every DB-record TU shares one definition. PushItem<T> is the dedup
// append used by record Import()s: linear scan of the CPtr vector, return false if the item is already
// present, else push_back + true. Matches the release NDb::PushItem<T,U> template (base @0x41a170 in
// DataFormat.obj); its per-TU COMDAT clones (DataMap/DataInterface/DataFaceGen) all fold onto this body.
// ASSERT / find / vector / CPtr are all in scope here via StdAfx.h (included before DataFormat.h in
// every TU). Calls with a dependent first argument resolve find() through _STL ADL at instantiation.
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> inline bool PushItem( vector<CPtr<T> > *pItems, T *p )
{
	ASSERT( pItems );
	vector<CPtr<T> >::const_iterator i = find( pItems->begin(), pItems->end(), p );
	if ( i == pItems->end() )
	{
		pItems->push_back( p );
		return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CString: public CDBRecord
{
	OBJECT_BASIC_METHODS(CString);
public:
	wstring szStr;

	virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTexture: public CDBRecord
{
	OBJECT_BASIC_METHODS(CTexture);
public:
	// retail enum ORDER (PDB NDb::CTexture::EType): 2D=0, ORDINARY=1, TRANSPARENT=2 —
	// raw-serialized in game.db records, so the order is part of the wire format
	enum EType
	{
		TEXTURE_USAGE_2D,
		TEXTURE_USAGE_ORDINARY,
		TEXTURE_USAGE_TRANSPARENT
	};
	ZDATA_(CDBRecord)
	int nWidth;
	int nHeight;
	float fGain;
	DWORD dwAverageColor;
	bool bIsDXT, bInstantLoad;
	EType usage;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&nWidth); f.Add(3,&nHeight); f.Add(4,&fGain); f.Add(6,&dwAverageColor); f.Add(7,&bIsDXT); f.Add(8,&bInstantLoad); f.Add(9,&usage); return 0; } // retail @0x3ffcf0: usage@9, no tag 5

	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCubeTexture: public CDBRecord
{
	OBJECT_BASIC_METHODS(CCubeTexture);
public:
	ZDATA_(CDBRecord)
	CPtr<CTexture> pPositiveX, pPositiveY, pPositiveZ;
	CPtr<CTexture> pNegativeX, pNegativeY, pNegativeZ;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&pPositiveX); f.Add(3,&pPositiveY); f.Add(4,&pPositiveZ); f.Add(5,&pNegativeX); f.Add(6,&pNegativeY); f.Add(7,&pNegativeZ); return 0; }
	
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CBRDF: public CDBRecord
{
	OBJECT_BASIC_METHODS(CBRDF);
public:
	float fFake;
	virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTMaterial;
class CMaterial: public CDBRecord
{
	OBJECT_BASIC_METHODS(CMaterial);
public:
	enum EAlpha
	{
		A_OPAQUE,
		A_ALPHA_TEST,
		A_TRANSPARENT,
		A_OVERLAY,
		A_SELF_ILLUM,
		A_SELF_ILLUM_AT,
		A_TRANSPARENT_2SIDED,
		A_PREDATOR,
		A_EXPLOSION_DECAL
	};
	enum EAddressMode
	{
		AM_WRAP,
		AM_CLAMP
	};
	
	CPtr<CBRDF> pBRDF;
	CPtr<CTexture> pTexture;
	CPtr<CTexture> pBump;
	CPtr<CTexture> pGloss, pMirror;
	CPtr<CTMaterial> pTemplate;
	EAlpha alpha;
	EAddressMode addrMode;
	float fSpecFactor, fMetalMirror, fDielMirror;
	CVec3 vSpecColor;
	bool bCastShadow;
	vector<SVariantFlags> flags;

	virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTMaterial : public CRndPtr<CMaterial>
{
	OBJECT_BASIC_METHODS(CTMaterial);
public:
	CMaterial* GetMaterial( SRand *pRand ) const;
	CMaterial* GetMaterial( SRand *pRand, const vector<int> &params ) const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRPGArmor;
class CSkeleton;
class CGeometry;
class CAIGeometry;
class CTRndModel;
class CModel: public CObjectBase
{
	OBJECT_BASIC_METHODS(CModel);
public:
	CDBPtr<CMaterial> pMaterials[N_MODEL_MATERIALS];
	CDBPtr<CGeometry> pGeometry;
	CDBPtr<CSkeleton> pSkeleton;
	CDBPtr<CRPGArmor> pRPGArmor;
	// retail CModel::operator& @0x3f9e60 tag 0xc=12: the CTRndModel template this model was rolled
	// from (stowed by CRndModel::CreateModel @0x3f7670); NRender uses it to RE-ROLL materials.
	// Without it a save/load loses the re-roll link. Not DB-column-fed.
	CDBPtr<CTRndModel> pSrcRndModel;
	//
	int operator&( CStructureSaver &f );
	//
	int GetMaxVP();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRndModel;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTRndModel : public CRndPtr<CRndModel>
{
	OBJECT_BASIC_METHODS(CTRndModel);
public:
	CModel* CreateModel( SRand *pRand );
	CModel* CreateModel( SRand *pRand, const vector<int> &flags );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
class CEffect;
class CTEffect: public CRndPtr<CEffect>
{
	OBJECT_BASIC_METHODS(CTEffect);
public:
	CEffect* GetEffect( SRand *pRand ) const;
	CEffect* GetEffect( SRand *pRand, const vector<int> &params ) const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CParticleInstance;
class CLightInstance;
class CEffect: public CDBRecord
{
	OBJECT_BASIC_METHODS(CEffect);
public:
	CPtr<CTEffect> pTemplate;
	vector<SVariantFlags> flags;
	vector< CPtr<CParticleInstance> > instances;
	vector< CPtr<CLightInstance> > lights;
	//
	virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CParticle: public CDBRecord
{
	OBJECT_BASIC_METHODS(CParticle);
public:
	CPtr<CAIGeometry> pAIGeometry;
	CPtr<CRPGArmor> pRPGArmor;
	SBound bound;
	// retail CParticle +0x34 (operator& @0x3fa050 tag 5, 8-byte DataChunk; Import @0x3f8d80 columns
	// "WrapX"/"WrapY"): texture wrap size fed to the particle animator's value.vWrap.
	CVec2 vWrapSize;
	//
	virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CParticleInstance: public CDBRecord
{
	OBJECT_BASIC_METHODS(CParticleInstance);
public:
	CPtr<CEffect> pEffect;
	CPtr<CParticle> pParticle;
	// space
	CVec3 position;
	CQuat rotation;
	float fScale;
	// time
	float fSpeed;
	float fOffset;
	float fEndCycle;
	int nCycleCount;
	// visualization
	enum ELight
	{
		L_NORMAL,
		L_LIT
	};
	enum EStatic
	{
		P_STATIC,
		P_DYNAMIC
	};
	ELight light;
	EStatic isStatic;
	bool bIsCrown;
	bool bDoesCastShadow;
	CVec2 pivot;
	CPtr<CTexture> pTextures[N_PARTICLE_TEXTURES];
	int nGlueToBone;
	//
	virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAnimLight: public CDBRecord
{
	OBJECT_BASIC_METHODS(CAnimLight);
public:
	//
	virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CLightInstance: public CDBRecord
{
	OBJECT_BASIC_METHODS(CLightInstance);
public:
	CPtr<CEffect> pEffect;
	CPtr<CAnimLight> pLight;
	// space
	CVec3 position;
	CQuat rotation;
	float fScale;
	// time
	float fSpeed;
	float fOffset;
	float fEndCycle;
	int nCycleCount;
	// glue to:
	int nGlueToBone;
	//
	virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTypeface: public CDBRecord
{
	OBJECT_BASIC_METHODS(CTypeface);
public:
	string szName;
	CPtr<CTexture> pTexture;
	//
	virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDebris;
class CDebrisMaterial: public CDBRecord
{
	OBJECT_BASIC_METHODS( CDebrisMaterial );
public:
	vector< CPtr<CDebris> > debris;
	//
	CDebris* GetDebris();
	//
	virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDebris: public CDBRecord
{
	OBJECT_BASIC_METHODS( CDebris );
public:
	CPtr<CTRndModel> pModel;
	CPtr<CDebrisMaterial> pDebrisMaterial;
	int nVolume;
	//
	virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRPGWeapon;
class CTRndObject;
class CSound;
class CTSound;
class CDoor: public CDBRecord
{
	OBJECT_BASIC_METHODS( CDoor );
public:
	ZDATA_(CDBRecord)
	CPtr<CTSound> pOpenSound, pCloseSound;
	CPtr<CTRndObject> pObject;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&pOpenSound); f.Add(3,&pCloseSound); f.Add(4,&pObject); return 0; }
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CGun: public CDBRecord
{
	OBJECT_BASIC_METHODS( CGun );
public:
	ZDATA_(CDBRecord)
	CPtr<CRPGWeapon> pWeapon;
	CPtr<CTRndObject> pObject;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&pWeapon); f.Add(3,&pObject); f.Add(4,&fMinClearDist); f.Add(5,&ptCannonAttackOrig); return 0; }
	float fMinClearDist = 0.0f;
	CVec3 ptCannonAttackOrig;
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPassageObject: public CDBRecord
{
	OBJECT_BASIC_METHODS( CPassageObject );
public:
	ZDATA
	ZPARENT( CDBRecord );
	CPtr<CTRndObject> pObject;
	CPtr<CSound> pUseSound;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CDBRecord *)this); f.Add(3,&pObject); f.Add(4,&pUseSound); return 0; }
	//
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CContainerModel;
class CRPGChestLayout;
class CTRPGChest;
class CObject: public CObjectBase
{
	OBJECT_BASIC_METHODS( CObject );
public:
/*	enum EInteractiveType
	{
		IT_DEFAULT,
		IT_WINDOW_DOOR,
		IT_CANNON,
	};*/
	//CDBPtr<CRPGWeapon> pWeapon;
	//EInteractiveType eType;
	CPtr<CContainerModel> pModels[N_DESTROY_STAGES];
	CDBPtr<CDebrisMaterial> pDebrisMaterial;
	bool bTargetable, bIsDeploySpot;
	CDBPtr<CDoor> pDoor;
	CDBPtr<CGun> pGun;
	CDBPtr<CPassageObject> pPassage;
	CPtr<CObject> pChild;
	int nParentID;
	bool bKeepDecals;
	CDBPtr<CRPGGrenade> pGrenade;	// @0x40 (DB col "RPGGrenade") -- explodable-object self-detonation grenade
									// (gas tanks / fuel barrels). Armed onto the runtime CObjectServerBase::pAttachedGrenade
									// at creation (CWorld::AddObject); fired by ProcessAttack path A on any damaging hit.
	CDBPtr<CRPGChestLayout> pChestLayout;	// retail @0x44 (tag 22) -- shelf layout of a lootable container (chest-loot spill orientation)
	CDBPtr<CTRPGChest> pDefaultChest;		// retail @0x48 (tag 23) -- default loot-chest template of the object type

	int operator&( CStructureSaver &f );
	//
	int GetStagesQuantity();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CString;
class CUITexture;
class CUIContainer;
class CHeadTextures;
class CRace;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CHead: public CDBRecord
{
	OBJECT_BASIC_METHODS(CHead);
public:
	CPtr<CTMaterial> pMaterial;
	// release-added transformable-head fields (tags 3..6 / cols TransformableTextures, IsTransformable,
	// TransformableGDP, TransformableMMT)
	CPtr<CHeadTextures> pTransformableTextures;
	bool isTransformable;
	string szTransformableGDP;
	string szTransformableMMT;
	//
	virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Which ambient-idle table an idle-flagged facial sequence joins (release enum NDb::ESequenceIdleType;
// NLSHead::InitializeIdleAnimations @0x2602f0 buckets HeadSeqs records by it).
enum ESequenceIdleType
{
	SIT_NORMAL = 0,
	SIT_DEATH  = 1,
};
class CSequence: public CDBRecord
{
	OBJECT_BASIC_METHODS(CSequence);
public:
	// release @0x400d60 (tags 2/3) / Import @0x42dbc0: the HeadSeqs facial-idle columns. Idle-flagged
	// records ("Blink"/"Blink01" normal, "Death" death in retail data) feed the ambient facial-idle
	// tables the head animator arms between spoken sequences (CHeadAnimator::Recalc @0x265a30).
	bool bIdleAnimation;              // IsIdleAnimation
	ESequenceIdleType eIdleType;      // IdleType ("Death" -> SIT_DEATH, anything else -> SIT_NORMAL)
	CSequence(): bIdleAnimation( false ), eIdleType( SIT_NORMAL ) {}
	virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CComplexHead: public CDBRecord
{
	OBJECT_BASIC_METHODS(CComplexHead);
public:
	CPtr<CHead> pHead;
	CPtr<CTRndModel> pHair;
	CPtr<CTRndModel> pHairInCap;
	CPtr<CTRndModel> pMeshes[N_HEAD_MESHES];
	CPtr<CTRndModel> pIFMeshes[N_HEAD_MESHES];
	// release-added fields (tags 5,6,7 / cols IsFemale, CanBeListed, BodyColor). pHairInCap is kept for
	// GView.cpp; the release dropped it (its column is simply absent in newer game.db, read as null).
	bool bIsFemale;
	bool bCanBeListed;
	CPtr<CRace> pBodyColor;
	//
	virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CString* GetString( int nID );
CUITexture* GetUITexture( int nID );
CUIContainer* GetUIContainer( int nID );
class CUIHint;
CUIHint* GetUIHint( int nID );			// release Hints table (0x6d) lookup -- the ShowHint script binding
class CUICursor;
CUICursor* GetUICursor( int nID );		// retail UICursors table (0x71) lookup -- SCursorInfo carries the CUICursor record
///
CModel* GetModel( int nModelID );
CModel* GetModelVariant( int nModelVariantID, SRand *pRand );
CTEffect* GetTEffect( int nTEffectID );
CEffect* GetEffect( int nEffectID );
CSkeleton *GetSkeleton( int nID );
class CAnimation;
CAnimation* GetAnimation( int nAnimID );
CTexture* GetTexture( int nTextureID );
CGeometry* GetGeometry( int nGeometryID );
CMaterial* GetMaterial( int nMaterialID );
CTMaterial* GetTMaterial( int nTMaterialID );
CCubeTexture* GetCubeTexture( int nID );
class CTemplate;
class CTemplVariant;
CComplexHead* GetComplexHead( int nID );
CSequence* GetSequence( int nID );
CTemplate* GetTemplate( int nID );
CTemplVariant* GetTemplVariant( const CTemplate *pTempl, const vector<int> &params, int nVarID, SRand *pRand );
CTemplVariant* GetTemplVariant( int nID );
////
class CDBScenarioZone;
CDBScenarioZone* GetDBScenarioZone( int nID );
class CScenarioGoal;
CScenarioGoal* GetScenarioGoal( int nID );		// release ScenarioGoals table (0x6f) lookup -- script goals
CDoor *GetDoorWindow( int nID );
////
class CChapterMap;
CChapterMap* GetChapterMap( int nID );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif