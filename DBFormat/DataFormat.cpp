#include "StdAfx.h"
#include "DataFormat.h"
#include "DataMap.h"
#include "DataRPG.h"
#include "DataRPGTmp.h"
#include "DataInterface.h"
#include "DataAnimation.h"
#include "DataTerrain.h"
#include "DataGeometry.h"
#include "DataAI.h"
#include "DataAck.h"
#include "DataObject.h"
#include "DataSound.h"
#include "DataScenario.h"
#include "DataCamera.h"
#include "DataScript.h"
#include "DataLight.h"
#include "DataDifficulty.h"
#include "DataText.h"
#include "DataPerk.h"
#include "DataRpgConstants.h"
#include "DataFaceGen.h"
#include "DataMisc.h"
#include "DataChest.h"
#include "DataPhys.h"

#include "..\Misc\StrProc.h"

const int N_DEF_BUMP_ID = 606;
static void ErrOut( const string &str, int nID )
{
	static char buf[1024];
	sprintf( buf, "%s %d\n", str.c_str(), nID );
	OutputDebugString( buf );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
union UColor
{
	DWORD dwColor;
	struct 
	{
		BYTE cR, cG, cB;
	};
};
CVec3 GetColor( DWORD dwColor )
{
	UColor c;
	c.dwColor = dwColor;
	return CVec3( c.cR / 255.0f, c.cG / 255.0f, c.cB / 255.0f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// ���������� ������� ���� � ���� ������ ��� ��������� ��� ������������
// ��� �������� �� vInputParams � ������ ������ ���
// !!! attrs � vInputParams ������� ���� ���������������� ���������
////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsSuitableVariant( const vector<int> &vInputParams, const vector<NDb::SVariantFlags> &flags )
{
	if ( flags.empty() )
		return true;
	
	for ( int i = 0; i < flags.size(); ++i )
	{
		const vector<int> &attrs = flags[i].flags;
		if ( attrs.empty() )
			return true;
		//
		if ( attrs == vInputParams )
			return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
externA5 void UnpackVariantFlags( const string &str, vector<SVariantFlags> *pFlags );
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRndModel: public CDBRecord
{
	OBJECT_BASIC_METHODS(CRndModel);
public:
	CPtr<CTMaterial> pMaterials[N_MODEL_MATERIALS];
	CPtr<CGeometry> pGeometry;
	CPtr<CSkeleton> pSkeleton;
	CPtr<CTRndModel> pTemplate;
	CPtr<CRPGArmor> pRPGArmor;
	vector<SVariantFlags> flags;
	//
	virtual void Import();
	int operator&( CStructureSaver &f );
	CModel* CreateModel( SRand *pRand, const vector<int> &flags );
};	
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRndConstructionPart: public CDBRecord
{
	OBJECT_BASIC_METHODS(CRndConstructionPart);
public:
	CPtr<CGeometry> pGeometry;
	CPtr<CGeometry> p2ndGeometry;
	int nSizeX;			// SizeX,Y ���������� � ������������ ������; SizeZ � ������
	int nSizeY;
	int nSizeZ;
	int nSubPartsMask;
	float fThickness;
	CPtr<CTMaterial> pDefMaterials[N_CONSTRUCTION_MATERIALS];
	vector<SVariantFlags> flags;
	CPtr<CTConstructionPart> pTemplate;
	CPtr<CTRndObject> pObject;
	CPtr<CRPGArmor> pArmor;
	int  nClipGroup;

	virtual void Import();
	int operator&( CStructureSaver &f );
	CConstructionPart* CreateConstructionPart( SRand *pRand, const vector<int> &params );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CEffect* CTEffect::GetEffect( SRand *pRand ) const { return GetRnd( pRand ); }
CEffect* CTEffect::GetEffect( SRand *pRand, const vector<int> &params ) const { return GetRnd( pRand, params ); }
////////////////////////////////////////////////////////////////////////////////////////////////////
CMaterial* CTMaterial::GetMaterial( SRand *pRand ) const { return GetRnd( pRand ); }
CMaterial* CTMaterial::GetMaterial( SRand *pRand, const vector<int> &params ) const { return GetRnd( pRand, params ); }
////////////////////////////////////////////////////////////////////////////////////////////////////
CSoundVariant* CTSound::GetSound( SRand *pRand ) const { return GetRnd( pRand ); }
CSoundVariant* CTSound::GetSound( SRand *pRand, const vector<int> &params ) const { return GetRnd( pRand, params ); }
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CopyAmbientLight( CAmbientLightReal *pRes, CAmbientLight *pSrc )
{
	pRes->vAmbientColor = pSrc->vAmbientColor;
	pRes->vLightColor = pSrc->vLightColor;
	pRes->vGlossColor = pSrc->vGlossColor;
	pRes->vFogColor = pSrc->vFogColor;
	pRes->vVapourColor = pSrc->vVapourColor;
	pRes->vShadowColor = pSrc->vShadowColor;
	pRes->vBackColor = pSrc->vBackColor;
	pRes->fPitch = pSrc->fPitch;
	pRes->fYaw = pSrc->fYaw;
	pRes->fFogDistance = pSrc->fFogDistance;
	pRes->fVapourHeight = pSrc->fVapourHeight;
	pRes->fVapourDensity = pSrc->fVapourDensity;
	pRes->fVapourNoiseParam = pSrc->fVapourNoiseParam;
	pRes->fVapourSpeed = pSrc->fVapourSpeed;
	pRes->fVapourSwitchTime = pSrc->fVapourSwitchTime;
	pRes->fFogStartDistance = pSrc->fFogStartDistance;
	pRes->fVapourStartHeight = pSrc->fVapourStartHeight;
	pRes->fBlurStrength = pSrc->fBlurStrength;
	pRes->pSky = pSrc->pSky;
	pRes->bInGameUse = pSrc->bInGameUse;
	pRes->vGroundAmbientColor = pSrc->vGroundAmbientColor;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CAmbientLightReal* CTAmbientLight::GetLight( SRand *pRand ) const 
{
	CAmbientLightReal *pRes = new CAmbientLightReal;
	CAmbientLight *p = GetRnd( pRand );
	CopyAmbientLight( pRes, p );
	if ( p->pGF2Light )
	{
		pRes->pGF2Light = new CAmbientLightReal;
		CopyAmbientLight( pRes->pGF2Light, p->pGF2Light->GetRnd( pRand ) );
	}
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CAmbientLightReal* CTAmbientLight::GetLight( SRand *pRand, const vector<int> &params ) const 
{ 
	CAmbientLightReal *pRes = new CAmbientLightReal;
	CAmbientLight *p = GetRnd( pRand, params );
	CopyAmbientLight( pRes, p );
	if ( p->pGF2Light )
	{
		pRes->pGF2Light = new CAmbientLightReal;
		CopyAmbientLight( pRes->pGF2Light, p->pGF2Light->GetRnd( pRand, params ) );
	}
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CString
////////////////////////////////////////////////////////////////////////////////////////////////////
void CString::Import()
{
	NDatabase::ImportField( "String", &szStr );
	wstring szDst; // CRAP
	for ( int k = 0; k < szStr.size(); ++k )
	{
		if ( szStr[k] != L'\r' )
			szDst += szStr[k];
	}
	szStr = szDst;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CString::operator&( CStructureSaver &f )
{ 
	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &szStr );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAnimation
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SAnimFlagString
{
	const char *pszFlag;
	int nFlag;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnimation::Import()
{
	NDatabase::ImportField( "SkeletonID", &pSkeleton );
	NDatabase::ImportField( "Speed", &fSpeed );
	NDatabase::ImportField( "Params", &szParams );
	NDatabase::ImportField( "Time1", &nTime1 );
	NDatabase::ImportField( "StepTime1", &nStepTime1 );
	NDatabase::ImportField( "StepTime2", &nStepTime2 );
	NDatabase::ImportField( "RndWeight", &fRndWeight );
	NDatabase::ImportField( "Angle", &nAngle );
	NDatabase::ImportField( "FallHeight", &fFallHeight );
	NDatabase::ImportField( "SideID", &pSide );
	
	bReverse = false;
	int nStartFrame = 0;
	bool bDefaultFrames;
	NDatabase::ImportField( "DefaultFrames", &bDefaultFrames );
	if ( !bDefaultFrames )
	{
		int nEndFrame = 0;
		NDatabase::ImportField( "StartFrame", &nStartFrame );
		NDatabase::ImportField( "EndFrame", &nEndFrame );
		if ( nStartFrame > nEndFrame )
			bReverse = true;
	}
	else
	{
		// ����� �� �������� ������� ������� ����� (��-�� ����������� � BasicDB.h)
		int n;
		NDatabase::ImportField( "StartFrame", &n );
		NDatabase::ImportField( "EndFrame", &n );
	}

	if ( nTime1 )
		nTime1 -= nStartFrame;
	if ( nStepTime1 )
		nStepTime1 -= nStartFrame;
	if ( nStepTime2 )
		nStepTime2 -= nStartFrame;

	struct SAnimTypeString
	{
		const char *pszType;
		int nType;
	}
	animTypes[] =
	{
		{ "Pose", POSE },
		{ "StartMove", START_MOVE },
		{ "Move", MOVE },
		{ "StartRun", START_RUN },
		{ "Run", RUN },
		{ "StartAttack", START_ATTACK },
		{ "Attack", ATTACK },
		{ "AttackUp", ATTACK_UP },
		{ "AttackDown", ATTACK_DOWN },
		{ "TurnLeft", TURN_LEFT },
		{ "TurnRight", TURN_RIGHT },
		{ "PoseStrafe", POSE_STRAFE },
		{ "StartStrafeF", START_STRAFE_F },
		{ "StrafeF", STRAFE_F },
		{ "StartStrafeL", START_STRAFE_L },
		{ "StrafeL", STRAFE_L },
		{ "StartStrafeR", START_STRAFE_R },
		{ "StrafeR", STRAFE_R },
		{ "StartStrafeB", START_STRAFE_B },
		{ "StrafeB", STRAFE_B },
		{ "ClimbLow", CLIMB_LOW },
		{ "ClimbHigh", CLIMB_HIGH },
		{ "JumpLow", JUMP_LOW },
		{ "JumpHigh", JUMP_HIGH },
		{ "JumpStart", JUMP_START },
		{ "ClimbFinish", CLIMB_FINISH },
		{ "Activate", ACTIVATE },
		{ "Deactivate", DEACTIVATE },
		{ "PoseItem", POSE_ITEM },
		{ "Use", USE },
		{ "ChangePose", CHANGE_POSE },
		{ "Death", DEATH },
		{ "PoseCorpse", POSE_CORPSE },
		{ "TakeCorpse", TAKE_CORPSE },
		{ "DropCorpse", DROP_CORPSE },
		{ "StartMoveCorpse", START_MOVE_CORPSE },
		{ "MoveCorpse", MOVE_CORPSE },
		{ "AttackLeftDown", ATTACK_LD },
		{ "AttackRightDown", ATTACK_RD },
		{ "AttackLeftUp", ATTACK_LU },
		{ "AttackRightUp", ATTACK_RU },
		{ "Open", OPEN },
		{ "Reload", RELOAD },
		{ "Idle", IDLE },
		{ "PoseHeal", POSE_HEAL },
		{ "StartHeal", START_HEAL },
		{ "Heal", HEAL },
		{ "AttackCeiling", ATTACK_CEILING },
		{ "AttackFloor", ATTACK_FLOOR },
		{ "EnterLadderUp", ENTER_LADDER_UP },
		{ "EnterLadderDown", ENTER_LADDER_DOWN },
		{ "LeaveLadderUp", LEAVE_LADDER_UP },
		{ "LeaveLadderDown", LEAVE_LADDER_DOWN },
		{ "MoveLadderUp", MOVE_LADDER_UP },
		{ "MoveLadderDown", MOVE_LADDER_DOWN },
		{ "JumpLadder", JUMP_LADDER },
		{ "PutBackpack", PUT_BACKPACK },
		{ "GetBackpack", GET_BACKPACK },
		{ "ThrowKnife", THROW_KNIFE },
		{ "Fall", FALL },
		{ "Destruct1", DESTRUCT_1 },
		{ "Destruct2", DESTRUCT_2 },
		{ "Destruct3", DESTRUCT_3 },
		{ "Destruct4", DESTRUCT_4 },
		// release CAnimation::Import string table (DataFormat.obj, decomp DataFormat.c): "InterfaceIdle" -> 0x49.
		// The retail content DB has 4 such rows (SkeletonID 8, Stand-only flags) -- the HUD unit-face idle clips.
		// NB game.db must be REGENERATED (DataImport.exe) for this mapping to reach the runtime data.
		{ "InterfaceIdle", INTERFACE_IDLE },
		// retail directional death rows (string table in release CAnimation::Import; Game.exe @0x4cfb58..7c)
		{ "DeathFront", DEATH_FRONT },
		{ "DeathBack", DEATH_BACK },
		{ "DeathRight", DEATH_RIGHT },
		{ "DeathLeft", DEATH_LEFT },
		{ 0, 0 },
	};
	SAnimFlagString animPWFlags[] =
	{
		{ "Stand", POSE_STAND },
		{ "Crouch", POSE_CROUCH },
		{ "Crawl", POSE_CRAWL },
		{ "NoWeapon", WEAPON_NONE },
		{ "Item", WEAPON_ITEM },
		{ "Pistol", WEAPON_PISTOL },
		{ "Rifle", WEAPON_RIFLE },
		{ "SubMachineGun", WEAPON_SUB_MACHINE_GUN },
		{ "Knife", WEAPON_KNIFE },
		{ "Katana", WEAPON_KATANA },
		{ "MachineGun", WEAPON_MACHINE_GUN },
		{ "RLauncher", WEAPON_RLAUNCHER },
		{ "MDetector", WEAPON_MINE_DETECTOR },
		{ "Plazmagun", PK_WEAPON_REPAIRER },
		{ "PKShooter", PK_WEAPON_SHOOTER },
		{ "PKSlasher", PK_WEAPON_SLASHER },
		{ "PKRepairer", PK_WEAPON_REPAIRER },
		{ "PKPlazmagun", PK_WEAPON_REPAIRER },
		{ 0, 0 },
	};
	SAnimFlagString animCSFlags[] =
	{
		{ "Male", SEX_MALE },
		{ "Female", SEX_FEMALE },
		{ "Combat", IN_COMBAT },
		{ "Realtime", IN_REALTIME },
		{ "Scout", SCOUT },
		{ "Sniper", SNIPER },
		{ "Grenadier", GRENADIER },
		{ "Soldier", SOLDIER },
		{ "Medic", MEDIC },
		{ "Engineer", ENGINEER },
		{ "Enemy", ENEMY },
		{ 0, 0 },
	};
	string szType;
	NDatabase::ImportField( "Type", &szType );
	for ( SAnimTypeString *pT = animTypes; pT->pszType != 0; ++pT )
	{
		if ( szType == pT->pszType )
		{
			nType = (EType)pT->nType;
			break;
		}
	}
	nPoseWeaponFlags = 0;
	for ( SAnimFlagString *pF = animPWFlags; pF->pszFlag != 0; ++pF )
	{
		bool bFlag;
		NDatabase::ImportField( pF->pszFlag, &bFlag );
		if ( bFlag )
			nPoseWeaponFlags |= pF->nFlag;
	}
	nClassSexFlags = 0;
	for ( SAnimFlagString *pF = animCSFlags; pF->pszFlag != 0; ++pF )
	{
		bool bFlag;
		NDatabase::ImportField( pF->pszFlag, &bFlag );
		if ( bFlag )
			nClassSexFlags |= pF->nFlag;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAnimation::operator&( CStructureSaver &f )
{ 
	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &pSkeleton );
	f.Add( 3, &nType );
	f.Add( 4, &nPoseWeaponFlags );
	f.Add( 5, &fSpeed );
	f.Add( 6, &szParams );
	f.Add( 7, &nTime1 );
	f.Add( 8, &nStepTime1 );
	f.Add( 9, &nStepTime2 );
	f.Add( 12, &fRndWeight );
	f.Add( 13, &nAngle );
	f.Add( 14, &bReverse );
	f.Add( 15, &fFallHeight );
	f.Add( 16, &nClassSexFlags );
	f.Add( 17, &pSide );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAnimationVector
////////////////////////////////////////////////////////////////////////////////////////////////////
CAnimation* SAnimationVector::GetAnimation( int nFlags, 
	const char *pszParams, int nClassSexFlags, CSide *pSide, bool bMostPossible ) const
{
	vector< CPtr<CAnimation> > results;
	if ( !pszParams )
	{
		for ( int i=0; i<anims.size(); ++i )
		{
			if ( anims[i]->pSide && anims[i]->pSide != pSide )
				continue;
			if ( (~anims[i]->nPoseWeaponFlags & nFlags) == 0 &&
					 (~anims[i]->nClassSexFlags & nClassSexFlags) == 0 &&
						anims[i]->szParams.empty() && anims[i]->fRndWeight > 0 )
				results.push_back( anims[i] );
		}
	}
	else
	{
		for ( int i=0; i<anims.size(); ++i )
		{
			if ( anims[i]->pSide && anims[i]->pSide != pSide )
				continue;
			if ( (~anims[i]->nPoseWeaponFlags & nFlags) == 0 && 
					 (~anims[i]->nClassSexFlags & nClassSexFlags) == 0 &&
						anims[i]->szParams == pszParams && anims[i]->fRndWeight > 0 )
				results.push_back( anims[i] );
		}
	}		
	//
	if ( results.empty() )
		return 0;
	else if ( results.size() == 1 )
		return results[0];
	else if ( bMostPossible )
	{
		// ���������� �������� ��������� ��������
		CAnimation *pRes = 0;
		for ( int i = 0; i < results.size(); ++i )
		{
			if ( pRes == 0 || results[i]->fRndWeight > pRes->fRndWeight )
				pRes = results[i];
		}
		return pRes;
	}
	else
	{
		CRoulette roulette;
		for ( int i=0; i<results.size(); ++i )
			roulette.AddSector( results[i]->fRndWeight );
		static SRand rand;
		return results[ roulette.GetRandomSector( &rand ) ];
		/*
		{
			char buf[128];
			sprintf( buf, "Getting random from %d animations: type %d, flags %d\n", results.size(), results[0]->nType, nFlags );
			OutputDebugString( buf );
			for ( int i=0; i<results.size(); ++i )
			{
				sprintf( buf, "ID: %d, weight: %f\n", results[i]->GetRecordID(), results[i]->fRndWeight );
				OutputDebugString( buf );
			}
			return results[ random.Get( results.size() ) ];
		}
		*/
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int SAnimationVector::operator&( CStructureSaver &f )
{
	f.Add( 1, &anims );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSkeleton
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSkeleton::Import()
{
	NDatabase::ImportField( "AIMeshLie", &pAIMeshLie );
	NDatabase::ImportField( "AIMeshCrouch", &pAIMeshCrouch );
	NDatabase::ImportField( "AIMeshStay", &pAIMeshStay );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CAnimation* CSkeleton::GetAnimation( const CAnimation::EType nType, const int nFlags, 
	const char *pszParams, int nClassSexFlags, CSide *pSide, bool bMostPossible ) const
{
	CAnimationMap::const_iterator it = pAnimations.find( nType );
	if ( it == pAnimations.end() )
		return 0;
	return it->second.GetAnimation( nFlags, pszParams, nClassSexFlags, pSide, bMostPossible );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CSkeleton::operator&( CStructureSaver &f )
{
	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &pAnimations );
	f.Add( 3, &pAIMeshLie );
	f.Add( 4, &pAIMeshCrouch );
	f.Add( 5, &pAIMeshStay );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CModel
////////////////////////////////////////////////////////////////////////////////////////////////////
int CModel::operator&( CStructureSaver &f )
{ 
//	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &pGeometry ); 
	
	for ( int i = 0; i < N_MODEL_MATERIALS; ++i )
		f.Add( 3 + i, &pMaterials[i] ); 
	f.Add( 10, &pSkeleton );
	f.Add( 11, &pRPGArmor );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CModel::GetMaxVP()
{
	if ( IsValid( pRPGArmor ) && IsValid( pGeometry->pAIGeometry ) )
	{
		int nArmorVP = pRPGArmor->pMaterial->nVP;
		float fVolume = pGeometry->pAIGeometry->fVolume / 0.7f;
		float fSolidPart = pGeometry->pAIGeometry->fSolidPart;
		return ( fVolume * fSolidPart ) * nArmorVP;
	}
	else
		return 1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRndModel
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRndModel::Import()
{
	ASSERT( 4 == N_MODEL_MATERIALS );
	NDatabase::ImportField( "Material0", &pMaterials[0] );
	NDatabase::ImportField( "Material1", &pMaterials[1] );
	NDatabase::ImportField( "Material2", &pMaterials[2] );
	NDatabase::ImportField( "Material3", &pMaterials[3] );
	NDatabase::ImportField( "GeometryID", &pGeometry );
	NDatabase::ImportField( "SkeletonID", &pSkeleton );
	NDatabase::ImportField( "TemplateID", &pTemplate );
	NDatabase::ImportField( "RPGArmorID", &pRPGArmor );
	string szFlags;
	NDatabase::ImportField( "Flags", &szFlags );
	UnpackVariantFlags( szFlags, &flags );
	//
	if ( IsValid( pTemplate ) )
	{
		pTemplate->variants.push_back( this );
		float fRndWeight;
		NDatabase::ImportField( "RndWeight", &fRndWeight );
		pTemplate->roulette.AddSector( fRndWeight );
	}
	else
	{
		ASSERT(0);
		ErrOut( "Bad RndModel: ", GetRecordID() );
	}

	if ( !IsValid( pGeometry ) )
		pGeometry = GetGeometry( N_SPHERE_GEOMETRY_ID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CRndModel::operator&( CStructureSaver &f )
{ 
	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &pGeometry ); 
	
	for ( int i = 0; i < N_MODEL_MATERIALS; ++i )
		f.Add( 3 + i, &pMaterials[i] ); 
	f.Add( 10, &pSkeleton );
	f.Add( 11, &pTemplate );
	f.Add( 13, &flags );
	f.Add( 14, &pRPGArmor );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CModel* CRndModel::CreateModel( SRand *pRand, const vector<int> &flags )
{
	CModel *pModel = new CModel;
	pModel->pGeometry = pGeometry;
	pModel->pSkeleton = pSkeleton;
	pModel->pRPGArmor = pRPGArmor;
	for ( int i = 0; i < N_MODEL_MATERIALS; ++i )
		pModel->pMaterials[i] = IsValid( pMaterials[i] ) ? pMaterials[i]->GetRnd( pRand, flags ) : 0;
	return pModel;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CModel* CTRndModel::CreateModel( SRand *pRand )
{
	CRndModel *pRM = GetRnd( pRand );
	if ( !pRM )
	{
		ASSERT( false );
		return 0;
	}
	vector<int> fake;
	return pRM->CreateModel( pRand, fake );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CModel* CTRndModel::CreateModel( SRand *pRand, const vector<int> &flags )
{
	CRndModel *pRM = GetRnd( pRand, flags );
	if ( !pRM )
	{
		ASSERT( false );
		return 0;
	}
	return pRM->CreateModel( pRand, flags );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRndConstructionPart
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRndConstructionPart::Import()
{
	NDatabase::ImportField( "FirstGeometryID", &pGeometry );
	NDatabase::ImportField( "SecondGeometryID", &p2ndGeometry );
	NDatabase::ImportField( "SizeX", &nSizeX );
	NDatabase::ImportField( "SizeY", &nSizeY );
	NDatabase::ImportField( "SizeZ", &nSizeZ );
	NDatabase::ImportField( "Thickness", &fThickness );
	ASSERT( N_CONSTRUCTION_MATERIALS == 8 );
	NDatabase::ImportField( "FirstGeomDefMaterial0", &pDefMaterials[0] );
	NDatabase::ImportField( "FirstGeomDefMaterial1", &pDefMaterials[1] );
	NDatabase::ImportField( "FirstGeomDefMaterial2", &pDefMaterials[2] );
	NDatabase::ImportField( "FirstGeomDefCutMaterial", &pDefMaterials[3] );
	NDatabase::ImportField( "SecondGeomDefMaterial", &pDefMaterials[4] );
	NDatabase::ImportField( "SecondGeomDefMaterial1", &pDefMaterials[5] );
	NDatabase::ImportField( "SecondGeomDefMaterial2", &pDefMaterials[6] );
	NDatabase::ImportField( "SecondGeomDefCutMaterial", &pDefMaterials[7] );
	NDatabase::ImportField( "SubPartMask", &nSubPartsMask );
	NDatabase::ImportField( "Object", &pObject );
	NDatabase::ImportField( "RPGArmorID", &pArmor );
	NDatabase::ImportField( "ClipGroupID", &nClipGroup );
	//
	string str;
	NDatabase::ImportField( "Flags", &str );
	UnpackVariantFlags( str, &flags );
	NDatabase::ImportField( "TemplateID", &pTemplate );
	//
	if ( IsValid( pTemplate ) )
	{
		pTemplate->variants.push_back( this );
		float fRndWeight;
		NDatabase::ImportField( "RndWeight", &fRndWeight );
		pTemplate->roulette.AddSector( fRndWeight );
	}
	else
	{
		ASSERT(0);
		ErrOut( "Bad RndConstructionPart: ", GetRecordID() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CConstructionPart* CRndConstructionPart::CreateConstructionPart( SRand *pRand, const vector<int> &params )
{
	CConstructionPart *pP = new CConstructionPart;
	pP->pGeometry = pGeometry;
	pP->p2ndGeometry = p2ndGeometry;
	pP->nSizeX = nSizeX;
	pP->nSizeY = nSizeY;
	pP->nSizeZ = nSizeZ;
	pP->nSubPartsMask = nSubPartsMask;
	pP->fThickness = fThickness;
	pP->pArmor = pArmor;
	pP->nClipGroup = nClipGroup;
	if ( IsValid( pObject ) )
		pP->pObject = pObject->CreateObject( pRand, params );
	for ( int i = 0; i < N_CONSTRUCTION_MATERIALS; ++i )
		pP->pDefMaterials[i] = IsValid( pDefMaterials[i] ) ? pDefMaterials[i]->GetMaterial( pRand, params ) : 0;
	return pP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CRndConstructionPart::operator&( CStructureSaver &f )
{ 
	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &pGeometry );
	f.Add( 3, &nSizeX );
	f.Add( 4, &nSizeY );
	f.Add( 5, &nSizeZ );
	f.Add( 6, &fThickness );
	f.Add( 8, &flags );
	f.Add( 9, &pTemplate );
	f.Add( 14, &nSubPartsMask );
	for ( int i = 0; i < N_CONSTRUCTION_MATERIALS; ++i )
		f.Add( 15 + i, &pDefMaterials[i] );

	f.Add( 25, &pObject );
	f.Add( 26, &p2ndGeometry );
	f.Add( 27, &pArmor );
	f.Add( 28, &nClipGroup );

	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CConstructionPart* CTConstructionPart::CreateConstructionPart( SRand *pRand ) const 
{ 
	CRndConstructionPart *pR = GetRnd( pRand );
	if ( pR )
		return pR->CreateConstructionPart( pRand, vector<int>() ); 
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CConstructionPart* CTConstructionPart::CreateConstructionPart( SRand *pRand, const vector<int> &params ) const 
{ 
	CRndConstructionPart *pR = GetRnd( pRand, params );
	if ( pR )
		return pR->CreateConstructionPart( pRand, params ); 
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CEffect
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEffect::Import()
{
	string szFlags;
	NDatabase::ImportField( "Flags", &szFlags );
	UnpackVariantFlags( szFlags, &flags );
	
	NDatabase::ImportField( "TemplateID", &pTemplate );
	//
	if ( IsValid( pTemplate ) )
	{
		pTemplate->variants.push_back( this );
		float fRndWeight;
		NDatabase::ImportField( "RndWeight", &fRndWeight );
		pTemplate->roulette.AddSector( fRndWeight );
	}
	else
	{
		ASSERT(0);
		ErrOut( "Bad Effect: ", GetRecordID() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CEffect::operator&( CStructureSaver &f )
{
	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &pTemplate );
	f.Add( 4, &flags );
	f.Add( 5, &instances );
	f.Add( 6, &lights );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CParticleInstance
////////////////////////////////////////////////////////////////////////////////////////////////////
void CParticleInstance::Import()
{
	NDatabase::ImportField( "EffectID", &pEffect );
	if ( IsValid( pEffect ) )
		pEffect->instances.push_back( this );
	NDatabase::ImportField( "ParticleID", &pParticle );
	// space
	NDatabase::ImportField( "PositionX", &position.x );
	NDatabase::ImportField( "PositionY", &position.y );
	NDatabase::ImportField( "PositionZ", &position.z );
	CVec3 euler;
	NDatabase::ImportField( "RotationX", &euler.x );
	NDatabase::ImportField( "RotationY", &euler.y );
	NDatabase::ImportField( "RotationZ", &euler.z );
	rotation.FromEulerAngles( euler.z, euler.y, euler.x );
	NDatabase::ImportField( "Scale", &fScale );
	// time
	NDatabase::ImportField( "Speed", &fSpeed );
	NDatabase::ImportField( "CycleCount", &nCycleCount );
	int nTmp;
	NDatabase::ImportField( "Offset", &nTmp );
	fOffset = nTmp / 1000.f;
	NDatabase::ImportField( "EndCycle", &nTmp );
	fEndCycle = nTmp / 1000.f;
	// visualization
	string szTmp;
	NDatabase::ImportField( "Light", &szTmp );
	if ( szTmp == "Lit" )
		light = L_LIT;
	else
		light = L_NORMAL;
	NDatabase::ImportField( "Static", &szTmp );
	if ( szTmp == "Static" )
		isStatic = P_STATIC;
	else
		isStatic = P_DYNAMIC;
	NDatabase::ImportField( "IsCrown", &bIsCrown );
	NDatabase::ImportField( "CastShadow", &bDoesCastShadow );

	NDatabase::ImportField( "PivotX", &pivot.x );
	NDatabase::ImportField( "PivotY", &pivot.y );
	char buf[32];
	for ( int i=0; i<N_PARTICLE_TEXTURES; ++i )
	{
		sprintf( buf, "Texture%d", i );
		NDatabase::ImportField( buf, &pTextures[i] );
	}
	NDatabase::ImportField( "GlueToEffectBone", &nGlueToBone );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CParticleInstance::operator&( CStructureSaver &f )
{
	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &pEffect );
	f.Add( 3, &pParticle );
	f.Add( 4, &position );
	f.Add( 5, &rotation );
	f.Add( 6, &fScale );
	f.Add( 7, &fSpeed );
	f.Add( 8, &fOffset );
	f.Add( 9, &fEndCycle );
	f.Add( 10, &nCycleCount );
	f.Add( 11, &light );
	f.Add( 12, &isStatic );
	f.Add( 13, &pivot );
	f.Add( 14, &bIsCrown );
	f.Add( 15, &bDoesCastShadow );
	f.Add( 16, &nGlueToBone ); 
	for ( int i=0; i<N_PARTICLE_TEXTURES; ++i )
		f.Add( 20 + i, &pTextures[i] );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CParticle
////////////////////////////////////////////////////////////////////////////////////////////////////
void CParticle::Import()
{
	NDatabase::ImportField( "RPGArmorID", &pRPGArmor );
	NDatabase::ImportField( "AIGeometryID", &pAIGeometry );
	// bounding box
	NDatabase::ImportField( "CenterX", &bound.s.ptCenter.x );
	NDatabase::ImportField( "CenterY", &bound.s.ptCenter.y );
	NDatabase::ImportField( "CenterZ", &bound.s.ptCenter.z );
	NDatabase::ImportField( "HalfBoxX", &bound.ptHalfBox.x );
	NDatabase::ImportField( "HalfBoxY", &bound.ptHalfBox.y );
	NDatabase::ImportField( "HalfBoxZ", &bound.ptHalfBox.z );
	bound.s.fRadius = fabs( bound.ptHalfBox );	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CParticle::operator&( CStructureSaver &f )
{
	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &pAIGeometry );
	f.Add( 3, &pRPGArmor );
	f.Add( 4, &bound );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLightInstance
////////////////////////////////////////////////////////////////////////////////////////////////////
void CLightInstance::Import()
{
	NDatabase::ImportField( "EffectID", &pEffect );
	if ( IsValid( pEffect ) )
		pEffect->lights.push_back( this );
	NDatabase::ImportField( "LightID", &pLight );
	// space
	NDatabase::ImportField( "PositionX", &position.x );
	NDatabase::ImportField( "PositionY", &position.y );
	NDatabase::ImportField( "PositionZ", &position.z );
	CVec3 euler;
	NDatabase::ImportField( "RotationX", &euler.x );
	NDatabase::ImportField( "RotationY", &euler.y );
	NDatabase::ImportField( "RotationZ", &euler.z );
	rotation.FromEulerAngles( euler.z, euler.y, euler.x );
	NDatabase::ImportField( "Scale", &fScale );
	// time
	NDatabase::ImportField( "Speed", &fSpeed );
	NDatabase::ImportField( "CycleCount", &nCycleCount );
	int nTmp;
	NDatabase::ImportField( "Offset", &nTmp );
	fOffset = nTmp / 1000.f;
	NDatabase::ImportField( "EndCycle", &nTmp );
	fEndCycle = nTmp / 1000.f;
	NDatabase::ImportField( "GlueToEffectBone", &nGlueToBone );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CLightInstance::operator&( CStructureSaver &f )
{
	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &pEffect );
	f.Add( 3, &pLight );
	f.Add( 4, &position );
	f.Add( 5, &rotation );
	f.Add( 6, &fScale );
	f.Add( 7, &fSpeed );
	f.Add( 8, &fOffset );
	f.Add( 9, &fEndCycle );
	f.Add( 10, &nCycleCount );
	f.Add( 11, &nGlueToBone );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAnimLight
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnimLight::Import()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAnimLight::operator&( CStructureSaver &f )
{
	f.Add( 1, (CDBRecord*)this );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTexture
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTexture::Import()
{
	NDatabase::ImportField( "Width", &nWidth );
	NDatabase::ImportField( "Height", &nHeight );
	NDatabase::ImportField( "BumpGain", &fGain );
	string szType;
	NDatabase::ImportField( "Type", &szType );
	if ( szType == "2D" )
		type = TEXTURE_2D;
	else if ( szType == "Transparent" )
		type = TEXTURE_TRANSPARENT;
	else if ( szType == "TransparentAdd" )
		type = TEXTURE_TRANSPARENT;
	else
		type = REGULAR;
	NDatabase::ImportField( "AverageColor", (int*)&dwAverageColor );
	string szFormat;
	NDatabase::ImportField( "Format", &szFormat );
	bIsDXT = szFormat.substr( 0, 3 ) == "dxt";
	NDatabase::ImportField( "InstantLoad", &bInstantLoad );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCubeTexture
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCubeTexture::Import()
{
	NDatabase::ImportField( "PositiveX", &pPositiveX );
	NDatabase::ImportField( "PositiveY", &pPositiveY );
	NDatabase::ImportField( "PositiveZ", &pPositiveZ );
	NDatabase::ImportField( "NegativeX", &pNegativeX );
	NDatabase::ImportField( "NegativeY", &pNegativeY );
	NDatabase::ImportField( "NegativeZ", &pNegativeZ );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CBRDF
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBRDF::Import()
{
	NDatabase::ImportField( "fake", &fFake );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CBRDF::operator&( CStructureSaver &f )
{
	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &fFake );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMaterial
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMaterial::Import()
{
	string szAlpha, szAddrMode;
	NDatabase::ImportField( "BRDFID", &pBRDF );
	NDatabase::ImportField( "TextureID", &pTexture );
	NDatabase::ImportField( "BumpID", &pBump );
	NDatabase::ImportField( "GlossID", &pGloss );
	NDatabase::ImportField( "MirrorID", &pMirror );
	NDatabase::ImportField( "SpecFactor", &fSpecFactor );
	int dwColor;
	NDatabase::ImportField( "SpecColor", &dwColor );
	vSpecColor = GetColor( dwColor );
	
	NDatabase::ImportField( "Alpha", &szAlpha );
	if ( szAlpha == "opaque" )
		alpha = A_OPAQUE;
	else if ( szAlpha == "alpha_test" )
		alpha = A_ALPHA_TEST;
	else if ( szAlpha == "transparent" )
		alpha = A_TRANSPARENT;
	else if ( szAlpha == "transparent_2sided" )
		alpha = A_TRANSPARENT_2SIDED;
	else if ( szAlpha == "add_transparent" )
		alpha = A_TRANSPARENT;
	else if ( szAlpha == "overlay" )
		alpha = A_OVERLAY;
	else if ( szAlpha == "self_illum" )
		alpha = A_SELF_ILLUM;
	else if ( szAlpha == "self_illum_alpha_test" )
		alpha = A_SELF_ILLUM_AT;
	else if ( szAlpha == "predator" )
		alpha = A_PREDATOR;
	else if ( szAlpha == "explosion_decal" )
		alpha = A_EXPLOSION_DECAL;
	else
		ASSERT( 0 );
	
	NDatabase::ImportField( "AddressMode", &szAddrMode );
	if ( szAddrMode == "Wrap" )
		addrMode = AM_WRAP;
	else if ( szAddrMode == "Clamp" )
		addrMode = AM_CLAMP;
	else
		ASSERT( 0 );
	NDatabase::ImportField( "MetalMirror", &fMetalMirror );
	NDatabase::ImportField( "DielMirror", &fDielMirror );
	NDatabase::ImportField( "CastShadow", &bCastShadow );
		
	string szFlags;
	NDatabase::ImportField( "Flags", &szFlags );
	UnpackVariantFlags( szFlags, &flags );
	
	NDatabase::ImportField( "TemplateID", &pTemplate );
	//
	if ( IsValid( pTemplate ) )
	{
		pTemplate->variants.push_back( this );
		float fRndWeight;
		NDatabase::ImportField( "RndWeight", &fRndWeight );
		pTemplate->roulette.AddSector( fRndWeight );
	}
	else
	{
		ASSERT(0);
		ErrOut( "Bad Material: ", GetRecordID() );
	}

}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CMaterial::operator&( CStructureSaver &f )
{
	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &pBRDF );
	f.Add( 3, &pTexture );
	f.Add( 4, &alpha );
	f.Add( 5, &pBump );
	f.Add( 6, &fSpecFactor );
	f.Add( 7, &vSpecColor );
	f.Add( 8, &pTemplate );
	f.Add( 9, &addrMode );
	f.Add( 10, &flags );
	f.Add( 11, &pGloss );
	f.Add( 12, &fMetalMirror );
	f.Add( 13, &fDielMirror );
	f.Add( 14, &pMirror );
	f.Add( 16, &bCastShadow );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIGeometry
////////////////////////////////////////////////////////////////////////////////////////////////////
void String2PieceLinks( SPieceLinksHash *pLinks, const string &szStr )
{
	pLinks->clear();
	if ( szStr.empty() )
		return;
	vector<string> vszPieces;
	NStr::SplitString( szStr, vszPieces, ';' );
	for ( int i = 0; i < vszPieces.size(); ++i )
	{
		vector<string> vszLinks;
		NStr::SplitString( vszPieces[i], vszLinks, ',' );
		if ( vszLinks.size() != 2 )
		{
			ASSERT( vszLinks.size() == 1 );
			continue;
		}
		SPieceLinks l;
		l.nPieceHashID = atoi( vszLinks[0].c_str() );
		BYTE data = atoi( vszLinks[1].c_str() );
		if ( data & 0x1 ) l.links.push_back( CVec3( 1, 0, 0 ) );
		if ( data & 0x2 ) l.links.push_back( CVec3( 0, 1, 0 ) );
		if ( data & 0x4 ) l.links.push_back( CVec3( 0, 0, 1 ) );
		if ( data & 0x8 ) l.links.push_back( CVec3( -1, 0, 0 ) );
		if ( data & 0x10 ) l.links.push_back( CVec3( 0, -1, 0 ) );
		if ( data & 0x20 ) l.links.push_back( CVec3( 0, 0, -1 ) );
		(*pLinks)[l.nPieceHashID] = l;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAIGeometry::Import()
{
	bool bPassability, bCover, bVision, bDamage, bItemBlocker;
	NDatabase::ImportField( "Passability", &bPassability );
	NDatabase::ImportField( "Cover", &bCover );
	NDatabase::ImportField( "Vision", &bVision );
	NDatabase::ImportField( "Damage", &bDamage );
	NDatabase::ImportField( "ItemBlocker", &bItemBlocker );
	NDatabase::ImportField( "Volume", &fVolume );
	NDatabase::ImportField( "SolidPart", &fSolidPart );
	traficability = (ETrafic) ( 
		( bPassability ? TR_PASS : 0 ) |
		( bCover ? TR_COVER : 0 ) |
		( bVision ? TR_VISION : 0 ) |
		( bDamage ? TR_DAMAGE : 0 ) |
		( bItemBlocker ? TR_ITEM_BLOCKER : 0 )
		);
	string szLinks;
	NDatabase::ImportField( "PiecesInfo", &szLinks );
	String2PieceLinks( &additionalLinks, szLinks );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CGeometry
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGeometry::Import()
{
	NDatabase::ImportField( "AIGeometryID", &pAIGeometry );
	NDatabase::ImportField( "AIGeometry2ID", &pAIGeometry2 );
	NDatabase::ImportField( "CenterX", &boundCenter.x );
	NDatabase::ImportField( "CenterY", &boundCenter.y );
	NDatabase::ImportField( "CenterZ", &boundCenter.z );
	NDatabase::ImportField( "SizeX", &boundSize.x );
	NDatabase::ImportField( "SizeY", &boundSize.y );
	NDatabase::ImportField( "SizeZ", &boundSize.z );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CGeometry::operator&( CStructureSaver &f )
{
	f.Add( 1, (CDBRecord*)this );
	f.Add( 4, &pAIGeometry );
	f.Add( 5, &boundCenter );
	f.Add( 6, &boundSize );
	f.Add( 7, &pAIGeometry2 );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTypeface
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTypeface::Import()
{
	NDatabase::ImportField( "Name", &szName );
	NDatabase::ImportField( "TextureID", &pTexture );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CTypeface::operator&( CStructureSaver &f )
{
	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &szName );
	f.Add( 3, &pTexture );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTerrainTile
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrainTile::Import()
{
	NDatabase::ImportField( "Priority", &nPriority );
	NDatabase::ImportField( "MaskVariants", &nMaskVariants );
	NDatabase::ImportField( "TextureVariants", &nTextureVariants );
	NDatabase::ImportField( "Priority", &nPriority );
	NDatabase::ImportField( "BumpID", &pBump );
	NDatabase::ImportField( "MaskID", &pMask );
	NDatabase::ImportField( "TextureID", &pTexture );
	NDatabase::ImportField( "RPGArmorID", &pArmor );
	if ( !IsValid( pBump ) )
		pBump = GetTexture( N_DEF_BUMP_ID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSpot
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSpot::Import()
{
	NDatabase::ImportField( "MaterialID", &pMaterial );
	NDatabase::ImportField( "RPGArmorID", &pArmor );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAmbientLight
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAmbientLight::Import()
{
	DWORD dwColor;
	NDatabase::ImportField( "AmbientColor", (int*)&dwColor );
	vAmbientColor = GetColor( dwColor );
	NDatabase::ImportField( "LightColor", (int*)&dwColor );
	vLightColor = GetColor( dwColor );
	vLightColor -= vAmbientColor;
	vLightColor.Maximize( VNULL3 );
	NDatabase::ImportField( "GlossColor", (int*)&dwColor );
	vGlossColor = GetColor( dwColor );
	NDatabase::ImportField( "FogColor", (int*)&dwColor );
	vFogColor = GetColor( dwColor );
	NDatabase::ImportField( "VapourColor", (int*)&dwColor );
	vVapourColor = GetColor( dwColor );
	NDatabase::ImportField( "BackLightColor", (int*)&dwColor );
	vBackColor = GetColor( dwColor );
	NDatabase::ImportField( "GroundAmbientColor", (int*)&dwColor );
	vGroundAmbientColor = GetColor( dwColor );
	
	NDatabase::ImportField( "Pitch", &fPitch );
	NDatabase::ImportField( "Yaw", &fYaw );
	NDatabase::ImportField( "FogDistance", &fFogDistance );
	NDatabase::ImportField( "VapourHeight", &fVapourHeight );
	NDatabase::ImportField( "VapourDensity", &fVapourDensity );
	NDatabase::ImportField( "VapourNoiseParam", &fVapourNoiseParam );
	NDatabase::ImportField( "VapourSpeed", &fVapourSpeed );
	NDatabase::ImportField( "VapourSwitchTime", &fVapourSwitchTime );
	NDatabase::ImportField( "FogStartDistance", &fFogStartDistance );
	NDatabase::ImportField( "VapourStartHeight", &fVapourStartHeight );
	NDatabase::ImportField( "BlurStrength", &fBlurStrength );
	NDatabase::ImportField( "SkyID", &pSky );
	NDatabase::ImportField( "ShadowColor", (int*)&dwColor );
	vShadowColor = GetColor( dwColor );
	NDatabase::ImportField( "UseInGame", &bInGameUse );
	NDatabase::ImportField( "GForce2LightID", &pGF2Light );

	NDatabase::ImportField( "TemplateID", &pTemplate );
	string szFlags;
	NDatabase::ImportField( "Flags", &szFlags );
	UnpackVariantFlags( szFlags, &flags );
	//
	if ( IsValid( pTemplate ) )
	{
		pTemplate->variants.push_back( this );
		float fRndWeight;
		NDatabase::ImportField( "RndWeight", &fRndWeight );
		pTemplate->roulette.AddSector( fRndWeight );
	}
	else
	{
		ASSERT(0);
		ErrOut( "Bad CAmbientLight: ", GetRecordID() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAmbientLight::operator&( CStructureSaver &f )
{
	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &vAmbientColor );
	f.Add( 3, &vLightColor );
	f.Add( 4, &fPitch );
	f.Add( 5, &fYaw );
	f.Add( 6, &vGlossColor );
	f.Add( 7, &vFogColor );
	f.Add( 8, &vVapourColor );
	f.Add( 9, &fFogDistance );
	f.Add( 10, &fVapourHeight );
	f.Add( 11, &fVapourDensity );
	f.Add( 12, &fVapourNoiseParam );
	f.Add( 13, &fVapourSpeed );
	f.Add( 14, &fVapourSwitchTime );
	f.Add( 15, &fFogStartDistance );
	f.Add( 16, &pSky );
	f.Add( 17, &vShadowColor );
	f.Add( 18, &bInGameUse );
	f.Add( 19, &vBackColor );
	f.Add( 20, &pGF2Light );
	f.Add( 21, &fVapourStartHeight );
	f.Add( 22, &fBlurStrength );
	f.Add( 23, &flags );
	f.Add( 25, &pTemplate );
	f.Add( 26, &vGroundAmbientColor );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSound
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSound::Import()
{
	NDatabase::ImportField( "Loop", &bLoop );
	NDatabase::ImportField( "MinDistance", &fMinDistance );
	NDatabase::ImportField( "MaxDistance", &fMaxDistance );
	NDatabase::ImportField( "Priority", &nPriority );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMusic
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMusic::Import()
{
	string szType;
	NDatabase::ImportField( "Type", &szType );
	if ( "Ambient" == szType )
		eType = MT_AMBIENT;
	else if ( "PreCombat" == szType )
		eType = MT_PRECOMBAT;
	else if ( "Combat" == szType )
		eType = MT_COMBAT;
	else
	{
		ASSERT(0);
		eType = MT_AMBIENT;
	}
	string szSrcName;
	NDatabase::ImportField( "SrcName", &szSrcName );
	vector<string> split;

	NStr::SplitString( szSrcName, split, '\\' );
	if ( !split.empty() )
	{
		szFileName = "Res\\Music\\";
		szFileName += split.back();
	}
	// retail CMusic::Import @0x3fb950: the data-driven music-machine timings (all ms). The retail
	// Steam game.db carries these columns (e.g. Music 1 "Ambient" = FadeIn 0, FadeOut 20000,
	// PlayTime 180000, RndPlayTime 120000, Silence 60000, RndSilence 120000); a db lacking them
	// keeps the retail-typical member defaults (ImportField leaves *pData untouched).
	NDatabase::ImportField( "FadeIn", &nFadeIn );
	NDatabase::ImportField( "FadeOut", &nFadeOut );
	NDatabase::ImportField( "PlayTime", &nPlayTime );
	NDatabase::ImportField( "RndPlayTime", &nRndPlayTime );
	NDatabase::ImportField( "Silence", &nSilence );
	NDatabase::ImportField( "RndSilence", &nRndSilence );
	string szFlags;
	NDatabase::ImportField( "Flags", &szFlags );
	UnpackVariantFlags( szFlags, &flags );
	// retail CMusic::Import @0x3fb950 tail: resolve the TemplateID column into the MusicTemplates
	// pool (CTMusic, table 0x7d) and register this record there with its RndWeight roulette sector
	// (same pattern as CAmbientLight::Import above). Everything that references music by id --
	// CTemplVariant.AmbientMusic/CombatMusic (retail @0x423dd0 ImportField<CTMusic>) and the
	// CMission defaults GetTMusic(1)/GetTMusic(3) -- references these POOLS, not Music records:
	// in the retail Steam game.db template ids do NOT mirror Music ids (template 124 = the base
	// ambient07 pool, while MUSIC record 124 is Combat13.wav).
	CPtr<CTMusic> pTemplate;
	NDatabase::ImportField( "TemplateID", &pTemplate );
	if ( IsValid( pTemplate ) )
	{
		pTemplate->variants.push_back( this );
		float fRndWeight = 1.0f;
		NDatabase::ImportField( "RndWeight", &fRndWeight );
		pTemplate->roulette.AddSector( fRndWeight );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDebrisMaterial
////////////////////////////////////////////////////////////////////////////////////////////////////
CDebris* CDebrisMaterial::GetDebris()
{
	if ( debris.empty() )
		return 0;
	return debris[ random.Get( debris.size() ) ];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDebrisMaterial::Import()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CDebrisMaterial::operator&( CStructureSaver &f )
{
	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &debris );
	return 0;
}	
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDebris::Import()
{
	NDatabase::ImportField( "ModelID", &pModel );
	NDatabase::ImportField( "DebrisMaterialID", &pDebrisMaterial );
	NDatabase::ImportField( "Volume", &nVolume );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CDebris::operator&( CStructureSaver &f )
{
	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &pModel );
	f.Add( 3, &pDebrisMaterial );
	f.Add( 4, &nVolume );
	return 0;
}	
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRndContainer
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRndContainerModel::Import()
{
	int nPLightCr = 0, nSLightCr = 0, nAmbient = 0, nFOV = 0;
  NDatabase::ImportField( "ModelID", &pModel );
  NDatabase::ImportField( "ParticleID", &pEffect );
	// v1.2 @0x7d9f20: five post-processing steps (the three UnpackColor conversions, the
	// SLightFOV int->float store and the SoundType mapping below) are guarded on their
	// column-read successes -- a mod db missing a column leaves the member untouched
	// (v1.1 forced black colors / 0.0f FOV / ST_PERMANENT).
	bool bHasPLightCr = NDatabase::ImportField( "PointLight", &nPLightCr );
	bool bHasSLightCr = NDatabase::ImportField( "SpotLight", &nSLightCr );
  NDatabase::ImportField( "ParticlePosX", &ptEffectPos.x );
	NDatabase::ImportField( "ParticlePosY", &ptEffectPos.y );
	NDatabase::ImportField( "ParticlePosZ", &ptEffectPos.z );
  NDatabase::ImportField( "PLightPosX", &ptPLightPos.x );
	NDatabase::ImportField( "PLightPosY", &ptPLightPos.y );
	NDatabase::ImportField( "PLightPosZ", &ptPLightPos.z );
  NDatabase::ImportField( "SLightPosX", &ptSLightPos.x );
	NDatabase::ImportField( "SLightPosY", &ptSLightPos.y );
	NDatabase::ImportField( "SLightPosZ", &ptSLightPos.z );
  NDatabase::ImportField( "SLightDirX", &ptSLightDir.x );
	NDatabase::ImportField( "SLightDirY", &ptSLightDir.y );
	NDatabase::ImportField( "SLightDirZ", &ptSLightDir.z );
	NDatabase::ImportField( "PLightRadius", &fPLightRadius );
	NDatabase::ImportField( "PLightFlareRadius", &fPFlareRadius );
	NDatabase::ImportField( "PLightFlareTexture", &pPFlareTexture );
	bool bHasFOV = NDatabase::ImportField( "SLightFOV", &nFOV );
	NDatabase::ImportField( "SLightMaskID", &pSLightMask );
	NDatabase::ImportField( "SLightRadius", &fSLightRadius );
	bool bHasAmbient = NDatabase::ImportField( "AmbientColor", &nAmbient );
	NDatabase::ImportField( "SoundID", &pSound );
	NDatabase::ImportField( "DestructionSoundID", &pDestroySound );
	NDatabase::ImportField( "SoundEffectID", &pSoundEffect );
	NDatabase::ImportField( "SoundPosX", &ptSoundPos.x );
	NDatabase::ImportField( "SoundPosY", &ptSoundPos.y );
	NDatabase::ImportField( "SoundPosZ", &ptSoundPos.z );
	NDatabase::ImportField( "PLightFlarePosX", &ptPLightFlarePos.x );
	NDatabase::ImportField( "PLightFlarePosY", &ptPLightFlarePos.y );
	NDatabase::ImportField( "PLightFlarePosZ", &ptPLightFlarePos.z );
	string szSoundType;
	// v1.2 @0x7d9f20: the SoundType mapping is guarded on the column read (see above)
	if ( NDatabase::ImportField( "SoundType", &szSoundType ) )
	{
		if ( "Permanent" == szSoundType )
			eSoundType = ST_PERMANENT;
		else if ( "Random" == szSoundType )
			eSoundType = ST_RANDOM;
		else if ( "Realtime" == szSoundType )
			eSoundType = ST_REALTIME;
		else if ( "Wind" == szSoundType )
			eSoundType = ST_WIND;
		else
		{
			ASSERT(0);
			eSoundType = ST_PERMANENT;
		}
	}
	NDatabase::ImportField( "SoundAvgInterval", &fSoundAvgInterval );
	// Steam game.db CRndContainerModel int columns "AttachedGrenade"/"PLightShadow" (retail CRndContainerModel::Import
	// @0x3f8f70 loads AttachedGrenade as its final ImportField<CRPGGrenade>). AttachedGrenade is the explosion an
	// explodable's destroyed stage spawns: CObjectServerBase::ProcessAttack arms+fires it via
	// pDbObject->pModels[stage]->pAttachedGrenade. Never importing it left the field null, so gas tanks/barrels could
	// not detonate even once they broke. (CreateContainer copies it to the runtime CContainerModel below.)
	NDatabase::ImportField( "AttachedGrenade", &pAttachedGrenade );
	NDatabase::ImportField( "PLightShadow", &bPLightShadow );

	// v1.2 @0x7d9f20: each post-processing step runs only when its column read succeeded
	if ( bHasFOV )
		fSLightFOV = nFOV;
	const float fScale = 1.0f / 255.0f;
	if ( bHasPLightCr )
		ptPLightCr = fScale * CVec3( nPLightCr & 0xff, (nPLightCr & 0xff00) >> 8, (nPLightCr & 0xff0000) >> 16 );
	if ( bHasSLightCr )
		ptSLightCr = fScale * CVec3( nSLightCr & 0xff, (nSLightCr & 0xff00) >> 8, (nSLightCr & 0xff0000) >> 16 );
	if ( bHasAmbient )
		ptAmbientColor = fScale * CVec3( nAmbient & 0xff, (nAmbient & 0xff00) >> 8, (nAmbient & 0xff0000) >> 16 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CContainerModel* CRndContainerModel::CreateContainer( SRand *pRand, const vector<int> &flags )
{
	CContainerModel *pC = new CContainerModel;
  pC->pModel = IsValid( pModel ) ? pModel->CreateModel( pRand, flags ) : 0;
	pC->pEffect = IsValid( pEffect ) ? pEffect->GetEffect( pRand, flags ) : 0;
	pC->pSLightMask = pSLightMask;
	pC->ptPLightCr = ptPLightCr;
	pC->ptSLightCr = ptSLightCr;
	pC->fPLightRadius = fPLightRadius;
	pC->fSLightFOV = fSLightFOV;
	pC->ptEffectPos = ptEffectPos;
	pC->ptPLightPos = ptPLightPos;
	pC->ptSLightPos = ptSLightPos;
	pC->ptSLightDir = ptSLightDir;
	pC->fSLightRadius = fSLightRadius;
	pC->ptAmbientColor = ptAmbientColor;
	pC->pSound = pSound;
	pC->pDestroySound = pDestroySound;
	pC->ptSoundPos = ptSoundPos;
	pC->eSoundType = eSoundType;
	pC->fSoundAvgInterval = fSoundAvgInterval;
	pC->fPFlareRadius = fPFlareRadius;
	pC->pPFlareTexture = pPFlareTexture;
	pC->pSoundEffect = pSoundEffect;
	pC->ptPLightFlarePos = ptPLightFlarePos;
	pC->pAttachedGrenade = pAttachedGrenade;   // explodable's detonation grenade (retail CreateContainer @0x3f7850)
	pC->bPLightShadow = bPLightShadow;
	return pC;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDoor
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDoor::Import()
{
	NDatabase::ImportField( "ObjectID", &pObject );
	NDatabase::ImportField( "OpenSoundID", &pOpenSound );
	NDatabase::ImportField( "CloseSoundID", &pCloseSound );
	ASSERT( IsValid( pObject ) );
	if ( IsValid( pObject ) )
		pObject->pDoor = this;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CGun
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGun::Import()
{
	NDatabase::ImportField( "ObjectID", &pObject );
	NDatabase::ImportField( "RPGWeaponID", &pWeapon );
	ASSERT( IsValid( pObject ) );
	if ( IsValid( pObject ) )
		pObject->pGun = this;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CPassageObject
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPassageObject::Import()
{
	NDatabase::ImportField( "UseSoundID", &pUseSound );
	NDatabase::ImportField( "ObjectID", &pObject );
	ASSERT( IsValid( pObject ) );
	if ( IsValid( pObject ) )
		pObject->pPassage = this;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRndObject
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTRndObject::Import()
{
	NDatabase::ImportField( "PlacableID", &pPlacable );

	if ( IsValid( pPlacable ) )
	{
		pPlacable->pObject = this;
	}
	else
	{
		ASSERT(0);
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRndObject
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRndObject::Import()
{
	NDatabase::ImportField( "Model0", &pModels[0] );
	NDatabase::ImportField( "Model1", &pModels[1] );
	NDatabase::ImportField( "Model2", &pModels[2] );
	NDatabase::ImportField( "Model3", &pModels[3] );
	NDatabase::ImportField( "Model4", &pModels[4] );
	NDatabase::ImportField( "DebrisMaterialID", &pDebrisMaterial );
	NDatabase::ImportField( "Targetable", &bTargetable );
	NDatabase::ImportField( "IsDeployPoint", &bIsDeploySpot );
	NDatabase::ImportField( "TemplateID", &pTemplate );
	NDatabase::ImportField( "KeepDecals", &bKeepDecals );
	NDatabase::ImportField( "RPGGrenade", &pGrenade );	// retail CRndObject::Import @0x3fbb20 -- explodable-object self-detonation grenade
	NDatabase::ImportField( "RPGChestLayout", &pChestLayout );		// retail CRndObject::Import @0x3fbb20 -- chest shelf layout
	NDatabase::ImportField( "DefaultRPGChest", &pDefaultChest );	// retail CRndObject::Import @0x3fbb20 -- default loot-chest template
	/*string szType;
	NDatabase::ImportField( "InteractiveType", &szType );
	if ( szType == "WindowDoor" )
		eType = CObject::IT_WINDOW_DOOR;
	else if ( szType == "Cannon" )
		eType = CObject::IT_CANNON;
	else
		eType = CObject::IT_DEFAULT;*/
//	NDatabase::ImportField( "RPGWeaponID", &pWeapon );
	NDatabase::ImportField( "ChildID", &pChild );
	
	string szFlags;
	NDatabase::ImportField( "Flags", &szFlags );
	UnpackVariantFlags( szFlags, &flags );
	//
	if ( IsValid( pTemplate ) )
	{
		pTemplate->variants.push_back( this );
		float fRndWeight;
		NDatabase::ImportField( "RndWeight", &fRndWeight );
		pTemplate->roulette.AddSector( fRndWeight );
	}
	else
	{
		ASSERT(0);
		ErrOut( "Bad RndObject: ", GetRecordID() );
	}

}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CRndObject::operator&( CStructureSaver &f )
{
	f.Add( 1, (CDBRecord*)this );
	for ( int i = 0; i < N_DESTROY_STAGES; ++i )
		f.Add( 2 + i, &pModels[i] );
	f.Add( 10, &pDebrisMaterial );
	f.Add( 13, &bTargetable );
	f.Add( 14, &pTemplate );
	f.Add( 15, &flags );
	f.Add( 16, &bIsDeploySpot );
	//f.Add( 17, &eType );
	//f.Add( 18, &pWeapon );
	f.Add( 19, &pChild );
//	f.Add( 21, &pDoor );
//	f.Add( 22, &pGun );
	f.Add( 23, &bKeepDecals );
	f.Add( 24, &pGrenade );		// retail CRndObject::operator& @0x3fcc70 index 0x18
	f.Add( 25, &pChestLayout );	// retail CRndObject::operator& @0x3fcc70 index 0x19
	f.Add( 26, &pDefaultChest );	// retail CRndObject::operator& @0x3fcc70 index 0x1a
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObject* CRndObject::CreateObject( SRand *pRand, const vector<int> &params )
{
	CObject *pO = new CObject;
	pO->pDebrisMaterial = pDebrisMaterial;
	pO->bTargetable = bTargetable;
	pO->bIsDeploySpot = bIsDeploySpot;
	pO->nParentID = GetRecordID();
	//pO->eType = eType;
	//pO->pWeapon = pWeapon;
	for ( int i = 0; i < N_DESTROY_STAGES; ++i )
		if ( pModels[i] )
			pO->pModels[i] = pModels[i]->CreateContainer( pRand, params );
	if ( IsValid( pChild ) )
		pO->pChild = pChild->CreateObject( pRand, params );
	ASSERT( IsValid( pTemplate ) );
	pO->pDoor = pTemplate->pDoor;
	pO->pGun = pTemplate->pGun;
	pO->pPassage = pTemplate->pPassage;
	pO->bKeepDecals = bKeepDecals;
	pO->pGrenade = pGrenade;	// retail CreateObject @0x3f7b00 -- carry the self-detonation grenade onto the runtime object record
	pO->pChestLayout = pChestLayout;	// retail CreateObject -- carry chest layout + default chest onto the runtime object record
	pO->pDefaultChest = pDefaultChest;
	return pO;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CObject::operator&( CStructureSaver &f )
{
	for ( int i = 0; i < N_DESTROY_STAGES; ++i )
		f.Add( 2 + i, &pModels[i] );
	f.Add( 10, &pDebrisMaterial );
	f.Add( 13, &bTargetable );
	f.Add( 14, &bIsDeploySpot );
	f.Add( 15, &pDoor );
	f.Add( 16, &pGun );
	f.Add( 17, &pChild );
	f.Add( 18, &pPassage );
	f.Add( 19, &nParentID );
	f.Add( 20, &bKeepDecals );
	f.Add( 21, &pGrenade );		// retail CObject::operator& @0x3fa960 index 0x15
	f.Add( 22, &pChestLayout );	// retail CObject::operator& @0x3fa960 index 0x16
	f.Add( 23, &pDefaultChest );	// retail CObject::operator& @0x3fa960 index 0x17
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CObject::GetStagesQuantity()
{
	int nRes = 0;
	for ( ; nRes < NDb::N_DESTROY_STAGES && IsValid( pModels[ nRes ] ); ++nRes );
	return nRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObject* CTRndObject::CreateObject( SRand *pRand )
{
	CRndObject *pRM = GetRnd( pRand );
	if ( !pRM )
	{
		ASSERT( false );
		return 0;
	}
	vector<int> fake;
	return pRM->CreateObject( pRand, fake );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CObject* CTRndObject::CreateObject( SRand *pRand, const vector<int> &flags )
{
	CRndObject *pRM = GetRnd( pRand, flags );
	if ( !pRM )
	{
		ASSERT( false );
		return 0;
	}
	return pRM->CreateObject( pRand, flags );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CGrass
////////////////////////////////////////////////////////////////////////////////////////////////////
void CGrass::Import()
{
	NDatabase::ImportField( "TextureID", &pTexture );
	NDatabase::ImportField( "ScaleX", &ptScale.x );
	NDatabase::ImportField( "ScaleY", &ptScale.y );
	NDatabase::ImportField( "PivotX", &ptPivot.x );
	NDatabase::ImportField( "PivotY", &ptPivot.y );
	NDatabase::ImportField( "SpotMaterialID", &pSpotMaterial );
	NDatabase::ImportField( "SpotScale", &fSpotScale );
	NDatabase::ImportField( "SideSize", &nSideSize );
	NDatabase::ImportField( "ScaleRange", &fScaleRange );
	fScaleRange = Clamp( fScaleRange, 0.f, 1.f );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSoundVariant
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundVariant::Import()
{
	NDatabase::ImportField( "SoundID", &pSound );
	string szFlags;
	NDatabase::ImportField( "Flags", &szFlags );
	UnpackVariantFlags( szFlags, &flags );
	NDatabase::ImportField( "TemplateID", &pTemplate );
	//
	if ( IsValid( pTemplate ) )
	{
		pTemplate->variants.push_back( this );
		float fRndWeight;
		NDatabase::ImportField( "RndWeight", &fRndWeight );
		pTemplate->roulette.AddSector( fRndWeight );
	}
	else
	{
		ASSERT(0);
		ErrOut( "Bad SoundVariant: ", GetRecordID() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHead
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHead::Import()
{
	NDatabase::ImportField( "Material0", &pMaterial );
	NDatabase::ImportField( "TransformableTextures", &pTransformableTextures );
	NDatabase::ImportField( "IsTransformable", &isTransformable );
	NDatabase::ImportField( "TransformableGDP", &szTransformableGDP );
	NDatabase::ImportField( "TransformableMMT", &szTransformableMMT );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CHead::operator&( CStructureSaver &f )
{
	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &pMaterial );
	f.Add( 3, &pTransformableTextures );
	f.Add( 4, &isTransformable );
	f.Add( 5, &szTransformableGDP );
	f.Add( 6, &szTransformableMMT );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSequence
////////////////////////////////////////////////////////////////////////////////////////////////////
// release @0x42dbc0: import the two HeadSeqs facial-idle columns; the IdleType string maps
// "Death" -> SIT_DEATH and anything else -> SIT_NORMAL (exactly the release strncmp).
void CSequence::Import()
{
	NDatabase::ImportField( "IsIdleAnimation", &bIdleAnimation );
	string szIdleType;
	// v1.2 @0x80efd0: the WHOLE IdleType mapping (default included) is guarded on the
	// column read -- a mod db lacking "IdleType" leaves the record's previous eIdleType
	// untouched (v1.1 forced SIT_NORMAL).
	if ( NDatabase::ImportField( "IdleType", &szIdleType ) )
	{
		eIdleType = SIT_NORMAL;
		if ( szIdleType == "Death" )
			eIdleType = SIT_DEATH;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CSequence::operator&( CStructureSaver &f )
{
	// release @0x400d60: tags 2/3 -- read straight from the retail game.db chunks
	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &bIdleAnimation );
	f.Add( 3, &eIdleType );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CComplexHead
////////////////////////////////////////////////////////////////////////////////////////////////////
void CComplexHead::Import()
{
	NDatabase::ImportField( "HeadID", &pHead );
	NDatabase::ImportField( "Hair", &pHair );
	NDatabase::ImportField( "HairInCap", &pHairInCap );
	NDatabase::ImportField( "Mesh0", &pMeshes[0] );
	NDatabase::ImportField( "Mesh1", &pMeshes[1] );
	NDatabase::ImportField( "Mesh2", &pMeshes[2] );
	NDatabase::ImportField( "Mesh3", &pMeshes[3] );
	NDatabase::ImportField( "IFMesh0", &pIFMeshes[0] );
	NDatabase::ImportField( "IFMesh1", &pIFMeshes[1] );
	NDatabase::ImportField( "IFMesh2", &pIFMeshes[2] );
	NDatabase::ImportField( "IFMesh3", &pIFMeshes[3] );
	NDatabase::ImportField( "IsFemale", &bIsFemale );
	NDatabase::ImportField( "CanBeListed", &bCanBeListed );
	NDatabase::ImportField( "BodyColor", &pBodyColor );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CComplexHead::operator&( CStructureSaver &f )
{
	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &pHead );
	f.Add( 3, &pHair );
	f.Add( 4, &pHairInCap );
	f.Add( 5, &bIsFemale );
	f.Add( 6, &bCanBeListed );
	f.Add( 7, &pBodyColor );
	for ( int i = 0; i < N_HEAD_MESHES; ++i )
		f.Add( 10 + i, &pMeshes[i] );
	for ( int i = 0; i < N_HEAD_MESHES; ++i )
		f.Add( 20 + i, &pIFMeshes[i] );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSoundInstance
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundInstance::Import()
{
	NDatabase::ImportField( "SoundID", &pSound );
	NDatabase::ImportField( "StartTime", &nStartTime );
	NDatabase::ImportField( "CycleCount", &nCycleCount );
	NDatabase::ImportField( "FadeIn", &bFadeIn );
	NDatabase::ImportField( "FadeOut", &bFadeOut );
	NDatabase::ImportField( "FadeSamples", &nFadeSamples );
	NDatabase::ImportField( "Volume", &nVolume );
	string szSoundType;
	NDatabase::ImportField( "SoundType", &szSoundType );
	if ( "Permanent" == szSoundType )
		eSoundType = ST_PERMANENT;
	else if ( "Random" == szSoundType )
		eSoundType = ST_RANDOM;
	else if ( "Realtime" == szSoundType )
		eSoundType = ST_REALTIME;
	else if ( "Wind" == szSoundType )
		eSoundType = ST_WIND;
	else
	{
		ASSERT(0);
		eSoundType = ST_PERMANENT;
	}
	NDatabase::ImportField( "SoundAvgInterval", &fSoundAvgInterval );
	NDatabase::ImportField( "EffectID", &pEffect );

	if ( IsValid( pEffect ) )
		pEffect->instances.push_back( this );
	else
	{
		ASSERT(0);
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSoundEffect
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoundEffect::Import()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTranslatedString
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTranslatedString::Import()
{
	NDatabase::ImportField( "String", &szStr );
	wstring szDst; // CRAP
	for ( int k = 0; k < szStr.size(); ++k )
	{
		if ( szStr[k] != L'\r' )
			szDst += szStr[k];
	}
	szStr = szDst;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TRes>
TRes* Get( int nID, TRes *p = 0 )
{
	CDBTable<TRes> *pModels = NDatabase::GetTable<TRes>();
	if ( !pModels )
		return 0;
	TRes *pRes = pModels->GetRecord( nID );
	if ( pRes == 0 )
	{
		//ASSERT( 0 );
		return 0;
	}
	return pRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CString* GetString( int nID ) { return Get<CString>( nID ); }
CUITexture* GetUITexture( int nID ) { return Get<CUITexture>( nID ); }
CUIContainer* GetUIContainer( int nID ) { return Get<CUIContainer>( nID ); }
CUIHint* GetUIHint( int nID ) { return Get<CUIHint>( nID ); }		// release ShowHint binding -- Hints table 0x6d
////
CGlobalMap* GetGlobalMap( int nID ) { return Get<CGlobalMap>( nID ); }
CChapterMap* GetChapterMap( int nID ) { return Get<CChapterMap>( nID ); }
////
CTemplate* GetTemplate( int nID ) { return Get<CTemplate>( nID ); }
CTemplVariant* GetTemplVariant( int nID ) { return Get<CTemplVariant>( nID ); }
CTRndModel* GetTRndModel( int nModelID ) { return Get<CTRndModel>( nModelID ); }
CTEffect* GetTEffect( int nTEffectID ) { return Get<CTEffect>( nTEffectID ); }
CEffect* GetEffect( int nEffectID ) { return Get<CEffect>( nEffectID ); }
CTexture* GetTexture( int nTextureID ) { return Get<CTexture>( nTextureID ); }
CAnimation* GetAnimation( int nAnimID ) { return Get<CAnimation>( nAnimID ); }
CGeometry* GetGeometry( int nGeometryID ) { return Get<CGeometry>( nGeometryID ); }
CMaterial* GetMaterial( int nMaterialID ) { return Get<CMaterial>( nMaterialID ); }
CRPGWeapon* GetWeapon( int nID ) { return Get<CRPGWeapon>( nID ); }
CRPGMeleeWeapon* GetMeleeWeapon( int nID ) { return Get<CRPGMeleeWeapon>( nID ); }
CRPGPers* GetPers( int nID ) { return Get<CRPGPers>( nID ); }
CRPGArmor* GetArmor( int nID ) { return Get<CRPGArmor>( nID ); }
CRPGDmgToArmor* GetDBDmg2Armor( int nID ) { return Get<CRPGDmgToArmor>( nID ); }
CTerrainTile* GetTerrainTile( int nID ) { return Get<CTerrainTile>( nID ); }
CAmbientLight* GetAmbientLight( int nID ) { return Get<CAmbientLight>( nID ); }
CTAmbientLight* GetTAmbientLight( int nID ) { return Get<CTAmbientLight>( nID ); }
CAttribute* GetAttribute( int nID ) { return Get<CAttribute>( nID ); }
CSound* GetSound( int nID ) { return Get<CSound>( nID ); }
CTMaterial* GetTMaterial( int nID ){ return Get<CTMaterial>( nID ); }
CTSound* GetTSound( int nID ){ return Get<CTSound>( nID ); }
CTConstructionPart* GetTConstructionPart( int nID ){ return Get<CTConstructionPart>( nID ); }
CGrass* GetGrass( int nID ) { return Get<CGrass>( nID ); }
CAISound* GetAISound( int nID ) { return Get<CAISound>( nID ); }
CCubeTexture* GetCubeTexture( int nID ) { return Get<CCubeTexture>( nID ); }
CRPGToHit* GetToHitConstants( int nID ) { return Get<CRPGToHit>( nID ); }
CRPGAISoundConstants *GetAISoundConstants( int nID ) { return Get<CRPGAISoundConstants>( nID ); }
CRPGInterruptsConstants *GetInterruptsConstants( int nID ) { return Get<CRPGInterruptsConstants>( nID ); }
CWaypoint* GetWaypoint( int nID ) { return Get<CWaypoint>( nID ); }
CWaypointName* GetWaypointName( int nID ) { return Get<CWaypointName>( nID ); }
CRPGItem* GetRPGItem( int nID ) { return Get<CRPGItem>( nID ); }
CComplexHead* GetComplexHead( int nID ) { return Get<CComplexHead>( nID ); }
CSequence* GetSequence( int nID ) { return Get<CSequence>( nID ); }
CFinalElement* GetFinalElement( int nID ) { return Get<CFinalElement>( nID ); }
CRndTerrainSpot* GetRndTerrainSpot( int nID ) { return Get<CRndTerrainSpot>( nID ); }
CRectangle* GetRectangle( int nID ) { return Get<CRectangle>( nID ); }
CTRndObject* GetTRndObject( int nID ) { return Get<CTRndObject>( nID ); }
CMusic* GetMusic( int nID ) { return Get<CMusic>( nID ); }
CTMusic* GetTMusic( int nID ) { return Get<CTMusic>( nID ); }	// retail NDb::GetTMusic @0x3f98c0 (MusicTemplates pool)
CPlacableObject* GetPlacableObject( int nID ) { return Get<CPlacableObject>( nID ); }
CSpot* GetSpot( int nID ) { return Get<CSpot>( nID ); }
CUnit* GetUnit( int nID ) { return Get<CUnit>( nID ); }
CRPGGrenade *GetRPGGrenade( int nID ) { return Get<CRPGGrenade>( nID ); }
CSkeleton *GetSkeleton( int nID ) { return Get<CSkeleton>( nID ); }
CRPGCritical *GetDBCritical( int nID ) { return Get<CRPGCritical>( nID ); }
CDBCamera *GetDBCamera( int nID ) { return Get<CDBCamera>( nID ); }
CRPGMaterial *GetRPGMaterial( int nID ) { return Get<CRPGMaterial>( nID ); }
CSoundEffect *GetSoundEffect( int nID ) { return Get<CSoundEffect>( nID ); }
CScript *GetDBScript( int nID ) { return Get<CScript>( nID ); }
CSide *GetDBSide( int nID ) { return Get<CSide>( nID ); }
CDBDialog* GetDBDialog( int nID ) { return Get<CDBDialog>( nID ); }
CDoor *GetDoorWindow( int nID ) { return Get<CDoor>( nID ); }
CAnimation *GetDBAnimation( int nID ) { return Get<CAnimation>( nID ); }
CRndObject *GetDBRndObject( int nID ) { return Get<CRndObject>( nID ); }
CAIGeometry* GetAIGeometry( int nID ) { return Get<CAIGeometry>( nID ); }
CDBDifficulty *GetDBDifficulty( int nID ) { return Get<CDBDifficulty>( nID ); }
CDBDiplomacy* GetDBDiplomacy( int nID ) { return Get<CDBDiplomacy>( nID ); }
CTranslatedString* GetTranslatedString( int nID ) { return Get<CTranslatedString>( nID ); }
CDBPerk *GetDBPerk( int nID ) { return Get<CDBPerk>( nID ); }
CDBMinesConstants *GetDBMinesConstants() { return Get<CDBMinesConstants>( 1 ); }
CModel* GetModel( int nModelID ) 
{ 
	SRand rand; 
	CTRndModel *pM = GetTRndModel( nModelID ); 
	if ( pM ) 
		return pM->CreateModel( &rand ); 
	return 0;
}
CModel* GetModelVariant( int nModelVariantID, SRand *pRand )
{
	ASSERT( pRand );
	CRndModel *pM = Get<CRndModel>( nModelVariantID ); 
	if ( pM ) 
	{
		vector<int> fake;
		return pM->CreateModel( pRand, fake ) ;
	}
	return 0;
}
CTemplVariant* GetTemplVariant( const CTemplate *pTempl, const vector<int> &params, int nVarID, SRand *pRand )
{
	ASSERT( pRand );
	if ( pTempl )
	{
		if ( nVarID > 0 )
			return pTempl->GetVariant( nVarID );
		else
			return pTempl->GetRnd( pRand, params );
	}
	return 0;
}
CContainerModel* GetContainer( int nContainerID ) 
{ 
	CRndContainerModel *pM = Get<CRndContainerModel>( nContainerID ); 
	if ( IsValid( pM ) )
	{
		SRand r;
		vector<int> fake;
		return pM->CreateContainer( &r, fake );
	}
	return 0;
}
CConstructionPart* CreateConstructionPartVariant( int nVarID )
{
	CRndConstructionPart* pPart = Get<CRndConstructionPart>( nVarID );
	if ( !pPart )
		return 0;
	SRand r;
	return pPart->CreateConstructionPart( &r, vector<int>() );
}
////
CDBScenarioZone* GetDBScenarioZone( int nID ) { return Get<CDBScenarioZone>( nID ); }
CScenarioGoal* GetScenarioGoal( int nID ) { return Get<CScenarioGoal>( nID ); }
////////////////////////////////////////////////////////////////////////////////////////////////////
START_REGISTER(DataFormat)
	StartRegisterSaveload();
	REGISTER_DATABASE_CLASS( 1, "Models", CRndModel );
	REGISTER_DATABASE_CLASS( 2, "Animations", CAnimation );
	REGISTER_DATABASE_CLASS( 3, "Textures", CTexture );
	REGISTER_DATABASE_CLASS( 4, "Templates", CTemplate );
	REGISTER_DATABASE_CLASS( 5, "TemplVariants", CTemplVariant );
	REGISTER_DATABASE_CLASS( 6, "Rects", CRectangle );
	REGISTER_DATABASE_CLASS( 7, "FinalElements", CFinalElement );
	REGISTER_DATABASE_CLASS( 8, "BRDFs", CBRDF );
	REGISTER_DATABASE_CLASS( 9, "Materials", CMaterial );
	REGISTER_DATABASE_CLASS( 10, "Geometries", CGeometry );
	REGISTER_DATABASE_CLASS( 11, "Units", CUnit );
	REGISTER_DATABASE_CLASS( 12, "Fonts", CTypeface );
	REGISTER_DATABASE_CLASS( 13, "Skeletons", CSkeleton );
	REGISTER_DATABASE_CLASS( 14, "TerrainTiles", CTerrainTile );
	REGISTER_DATABASE_CLASS( 15, "ChapterMaps", CChapterMap );
	REGISTER_DATABASE_CLASS( 16, "GlobalMaps", CGlobalMap );
	
	REGISTER_DATABASE_CLASS( 0xE0000001, "RPGWeaponTypes", CRPGWeaponType );
	REGISTER_DATABASE_CLASS( 0xE0000002, "RPGWeapons", CRPGWeapon );
	REGISTER_DATABASE_CLASS( 0xE0000003, "RPGClasses", CRPGClass );
	REGISTER_DATABASE_CLASS( 0xE0000004, "RPGPers", CRPGPers );
	REGISTER_DATABASE_CLASS( 0xE0000005, "RPGArmors", CRPGArmor );
	REGISTER_DATABASE_CLASS( 0xE0000006, "RPGAmmos", CRPGAmmo );
	REGISTER_DATABASE_CLASS( 0xE0000007, "RPGBaseValues", CRPGBaseValue );
	REGISTER_DATABASE_CLASS( 0xE0000008, "RPGClips", CRPGClip );
	REGISTER_DATABASE_CLASS( 0xE0000009, "RPGItems", CRPGItem );
	REGISTER_DATABASE_CLASS_TEMPL( 0xE000000A, "RPGWeapon4Pers", CRPGWeapon4Pers, CRPGSomethingForPers );
	REGISTER_DATABASE_CLASS( 0xE000000B, "RPGClip4Pers", CRPGClip4Pers );
	REGISTER_DATABASE_CLASS( 0xE000000C, "Scripts", CScript );
	REGISTER_DATABASE_CLASS( 0xE000000D, "RPGGrenades", CRPGGrenade );
	REGISTER_DATABASE_CLASS( 0xE000000E, "RPGUniforms", CRPGUniform );
	REGISTER_DATABASE_CLASS_TEMPL( 0xE000000F, "RPGGrenade4Pers", CRPGGrenade4Pers, CRPGSomethingForPers );
	REGISTER_DATABASE_CLASS( 0xE0000010, "RPGItem2Uniforms", CRPGItem2Uniform );
	REGISTER_DATABASE_CLASS( 0xE0000012, "RPGFirstAids", CRPGFirstAid );
	REGISTER_DATABASE_CLASS_TEMPL( 0xE0000013, "RPGFirstAid4Pers", CRPGFirstAid4Pers, CRPGSomethingForPers );
	REGISTER_DATABASE_CLASS( 0xE0000014, "RPGMeleeWeapons", CRPGMeleeWeapon );
	REGISTER_DATABASE_CLASS_TEMPL( 0xE0000015, "RPGMeleeWeapon4Pers", CRPGMeleeWeapon4Pers, CRPGSomethingForPers );
	REGISTER_DATABASE_CLASS( 0xE0000016, "RPGMineDetectors", CRPGMineDetector );
	REGISTER_DATABASE_CLASS_TEMPL( 0xE0000017, "RPGMineDetector4Pers", CRPGMineDetector4Pers, CRPGSomethingForPers );
	REGISTER_DATABASE_CLASS( 0xE0000018, "RPGStoreItems", CRPGStoreItem );
	REGISTER_DATABASE_CLASS( 0xE0000019, "RPGMines", CRPGMine );
	REGISTER_DATABASE_CLASS_TEMPL( 0xE0000020, "RPGMines4Pers", CRPGMine4Pers, CRPGSomethingForPers );
	REGISTER_DATABASE_CLASS( 0xE0000021, "RPGTools", CRPGTool );
	REGISTER_DATABASE_CLASS_TEMPL( 0xE0000022, "RPGTool4Pers", CRPGTool4Pers, CRPGSomethingForPers );
	REGISTER_DATABASE_CLASS( 0xE0000023, "RPGKeys", CRPGKey );
	REGISTER_DATABASE_CLASS_TEMPL( 0xE0000024, "RPGKey4Pers", CRPGKey4Pers, CRPGSomethingForPers );

	REGISTER_DATABASE_CLASS( 20, "AIGeometries", CAIGeometry );
	REGISTER_DATABASE_CLASS( 21, "WallModels", CWallModel );
	REGISTER_DATABASE_CLASS( 22, "FloorModels", CFloorModel );
	REGISTER_DATABASE_CLASS( 23, "Walls", CWall );
	REGISTER_DATABASE_CLASS( 24, "Floors", CFloor );
	REGISTER_DATABASE_CLASS( 25, "Particles", CParticle );
	REGISTER_DATABASE_CLASS( 26, "SolidModels", CSolidModel );
	REGISTER_DATABASE_CLASS( 27, "SolidObjects", CSolid );
	REGISTER_DATABASE_CLASS( 28, "Containers", CContainer );
	REGISTER_DATABASE_CLASS( 29, "AmbientLights", CAmbientLight );
	REGISTER_DATABASE_CLASS( 30, "Rooms", CRoom );
	REGISTER_DATABASE_CLASS( 31, "MaterialTemplates", CTMaterial );
	REGISTER_DATABASE_CLASS( 32, "ModelTemplates", CTRndModel );
	REGISTER_DATABASE_CLASS( 33, "ContainerModels", CRndContainerModel );
	REGISTER_DATABASE_CLASS( 34, "IntermediateFloors", CIntermediateFloor );
	REGISTER_DATABASE_CLASS( 35, "Attributes", CAttribute );
	REGISTER_DATABASE_CLASS( 36, "IntermediateSolids", CIntermediateSolid );
	REGISTER_DATABASE_CLASS( 37, "Sounds", CSound );
	REGISTER_DATABASE_CLASS( 38, "DebrisMaterials", CDebrisMaterial );
	REGISTER_DATABASE_CLASS( 39, "Debris", CDebris );
	REGISTER_DATABASE_CLASS( 40, "Objects", CRndObject );
	REGISTER_DATABASE_CLASS( 41, "ObjectTemplates", CTRndObject );
	REGISTER_DATABASE_CLASS( 42, "Explosions", CExplosion );
	REGISTER_DATABASE_CLASS( 43, "UIControls", CUIControl );
	REGISTER_DATABASE_CLASS( 44, "UIContainers", CUIContainer );
	REGISTER_DATABASE_CLASS( 45, "Strings", CString );
	REGISTER_DATABASE_CLASS( 46, "ConstructionPartTemplates", CTConstructionPart );
	REGISTER_DATABASE_CLASS( 47, "ConstructionParts", CRndConstructionPart );
	REGISTER_DATABASE_CLASS( 48, "Grass", CGrass );
	REGISTER_DATABASE_CLASS( 49, "UITextures", CUITexture );
	REGISTER_DATABASE_CLASS( 50, "TerrainSpots", CRndTerrainSpot );
	REGISTER_DATABASE_CLASS( 51, "RPGCriticals", CRPGCritical );
	REGISTER_DATABASE_CLASS( 52, "AISounds", CAISound );
	REGISTER_DATABASE_CLASS( 53, "CubeTextures", CCubeTexture );
	REGISTER_DATABASE_CLASS( 54, "AckInfos", CDBAckInfo );	
	REGISTER_DATABASE_CLASS( 55, "AckSeqs", CDBAckSequence );	
	REGISTER_DATABASE_CLASS( 56, "Acks", CDBAck );
	REGISTER_DATABASE_CLASS( 57, "SoundVariants", CSoundVariant );
	REGISTER_DATABASE_CLASS( 58, "SoundTemplates", CTSound );
	REGISTER_DATABASE_CLASS( 59, "RPGToHits", CRPGToHit );
	REGISTER_DATABASE_CLASS( 60, "Waypoints", CWaypoint );
	REGISTER_DATABASE_CLASS( 61, "Doors", CDoor );
	REGISTER_DATABASE_CLASS( 62, "Guns", CGun );
	REGISTER_DATABASE_CLASS( 63, "WaypointNames", CWaypointName );
	REGISTER_DATABASE_CLASS( 64, "ParticleInstances", CParticleInstance );
	REGISTER_DATABASE_CLASS( 65, "Effects", CEffect );
	REGISTER_DATABASE_CLASS( 66, "EffectTemplates", CTEffect );
	REGISTER_DATABASE_CLASS( 67, "AnimWeaponTypes", CAnimWeaponType );
	REGISTER_DATABASE_CLASS( 68, "AISoundConstants", CRPGAISoundConstants );
	REGISTER_DATABASE_CLASS( 69, "Heads", CHead );
	REGISTER_DATABASE_CLASS( 70, "HeadSeqs", CSequence );
	REGISTER_DATABASE_CLASS( 71, "InterruptsConstants", CRPGInterruptsConstants );
	REGISTER_DATABASE_CLASS( 72, "RPGDmgToArmors", CRPGDmgToArmor );
	REGISTER_DATABASE_CLASS( 73, "LightInstances", CLightInstance );
	REGISTER_DATABASE_CLASS( 74, "Lights", CAnimLight );
	REGISTER_DATABASE_CLASS( 75, "ComplexHeads", CComplexHead );
	REGISTER_DATABASE_CLASS( 76, "Musics", CMusic );
	REGISTER_DATABASE_CLASS( 77, "PlacableObjects", CPlacableObject );
	REGISTER_DATABASE_CLASS( 78, "Spots", CSpot );
	REGISTER_DATABASE_CLASS( 79, "ScenarioZones", CDBScenarioZone );
	REGISTER_DATABASE_CLASS( 80, "ScenarioStates", CDBScenarioState );
	REGISTER_DATABASE_CLASS( 81, "ScenarioClues", CDBScenarioClue );
	REGISTER_DATABASE_CLASS( 82, "ScenarioObjectives", CDBScenarioObjective );
	REGISTER_DATABASE_CLASS( 83, "Scenarios", CDBScenario );
	REGISTER_DATABASE_CLASS( 84, "ScenarioObjective2Clues", CDBScenarioObjective2Clue );
	REGISTER_DATABASE_CLASS( 86, "PassageObjects", CPassageObject );
	REGISTER_DATABASE_CLASS( 87, "Panzerkleins", CPanzerklein );
	REGISTER_DATABASE_CLASS( 88, "Cameras", CDBCamera );
	REGISTER_DATABASE_CLASS( 89, "UnitGroups", CUnitGroup );
	REGISTER_DATABASE_CLASS( 90, "RPGMaterials", CRPGMaterial );
	REGISTER_DATABASE_CLASS( 91, "AutoLoadScripts", CDBAutoLoadScript );
	REGISTER_DATABASE_CLASS( 92, "AmbientLightTemplates", CTAmbientLight );
	REGISTER_DATABASE_CLASS( 93, "SoundInstances", CSoundInstance );
	REGISTER_DATABASE_CLASS( 94, "SoundEffects", CSoundEffect );
	REGISTER_DATABASE_CLASS( 95, "Sides", CSide );
	REGISTER_DATABASE_CLASS( 96, "Nationalities", CNationality );
	REGISTER_DATABASE_CLASS( 97, "Dialogs", CDBDialog );
	REGISTER_DATABASE_CLASS( 98, "DifficultyConstants", CDBDifficulty );
	REGISTER_DATABASE_CLASS( 99, "Diplomacies", CDBDiplomacy );
	REGISTER_DATABASE_CLASS( 100, "TranslatedStrings", CTranslatedString );
	REGISTER_DATABASE_CLASS( 101, "RPGPerks", CDBPerk );
	REGISTER_DATABASE_CLASS( 102, "RPGPerkTreeNodes", CDBPerkTreeNode );
	REGISTER_DATABASE_CLASS( 103, "DialogSeqs", CDBDialogSeq );
	REGISTER_DATABASE_CLASS( 104, "DialogPers", CDBDialogPers );
	REGISTER_DATABASE_CLASS( 105, "RPGMinesConstants", CDBMinesConstants );
	// release-added transformable-head / face-customisation tables (ids 0x73..0x78, verbatim from the
	// release NDatabase init). CHead::pTransformableTextures -> CHeadTextures; CComplexHead::pBodyColor
	// -> CRace; the three texture tables share CHeadTexture and wire into CHeadTextures via THMID.
	REGISTER_DATABASE_CLASS( 0x73, "TransformableFaceTextures", CFaceTexture );
	REGISTER_DATABASE_CLASS( 0x74, "TransformableEyeTextures", CEyeTexture );
	REGISTER_DATABASE_CLASS( 0x75, "TransformableEyelashTeethTextures", CEyelashTexture );
	REGISTER_DATABASE_CLASS( 0x76, "TransformableHeadMaterials", CHeadTextures );
	REGISTER_DATABASE_CLASS( 0x77, "TransformableHeadHairs", CFaceGenHeadHair );
	REGISTER_DATABASE_CLASS( 0x78, "Races", CRace );
	REGISTER_DATABASE_CLASS( 0x79, "TransformableHeadGlasses", CFaceGenHeadGlasses );
	// release-added standalone records (medals / AP / picklock / UI hint+cursor)
	REGISTER_DATABASE_CLASS( 0xE0000025, "RPGPicklocks", CRPGPicklock );
	REGISTER_DATABASE_CLASS( 0xE0000027, "Medals", CMedal );
	REGISTER_DATABASE_CLASS( 0x6d, "Hints", CUIHint );
	REGISTER_DATABASE_CLASS( 0x71, "UICursors", CUICursor );
	REGISTER_DATABASE_CLASS( 0x7e, "RPGAPs", CRPGAP );
	// register the tables for the already-reconstructed CDBPhysParams / CDBAckCondition record classes
	// (the classes existed but their tables were never registered, so game.db could not load them)
	REGISTER_DATABASE_CLASS( 0x70, "PhysParams", CDBPhysParams );
	REGISTER_DATABASE_CLASS( 0x7c, "AckConditions", CDBAckCondition );
	// release-added RPG chest / loot cluster (DataChest.{h,cpp})
	REGISTER_DATABASE_CLASS( 0x6a, "RPGChestTemplates", CTRPGChest );
	REGISTER_DATABASE_CLASS( 0x6b, "RPGLootInstances", CRPGLootInstances );
	REGISTER_DATABASE_CLASS( 0x6c, "RPGChests", CRPGChest );
	REGISTER_DATABASE_CLASS( 0x72, "RPGChestLayouts", CRPGChestLayout );
	// release-added engineer grenade + the two new per-person inventory join tables
	REGISTER_DATABASE_CLASS( 0x7b, "RPGEngGrenades", CRPGEngGrenade );
	REGISTER_DATABASE_CLASS_TEMPL( 0xE0000026, "RPGPicklocks4Pers", CRPGPicklock4Pers, CRPGSomethingForPers );
	REGISTER_DATABASE_CLASS_TEMPL( 0xE0000028, "RPGEngGrenade4Pers", CRPGEngGrenade4Pers, CRPGSomethingForPers );
	// release-added scenario goals/tasks (DataScenario), face-expression map and music templates
	REGISTER_DATABASE_CLASS( 0x6e, "ScenarioTasks", CScenarioTask );
	REGISTER_DATABASE_CLASS( 0x6f, "ScenarioGoals", CScenarioGoal );
	REGISTER_DATABASE_CLASS( 0x7a, "FaceExpression2Sequences", CFaceExpression );
	REGISTER_DATABASE_CLASS( 0x7d, "MusicTemplates", CTMusic );
	//
	REGISTER_DATABASE_RELATION( "RPGPers2Scripts" )

FINISH_REGISTER
	//////////////////////////////////////////////////////////////////////////////////////
}
using namespace NDb;
// Format 0x[�������]DDMYHHN
REGISTER_SAVELOAD_CLASS( 0x002a1172, CString )
REGISTER_SAVELOAD_CLASS( 0x02511000, CModel )
REGISTER_SAVELOAD_CLASS( 0x02511001, CAnimation )
REGISTER_SAVELOAD_CLASS( 0x02511002, CTexture )
REGISTER_SAVELOAD_CLASS( 0x00121000, CBRDF )
REGISTER_SAVELOAD_CLASS( 0x00121001, CMaterial )
REGISTER_SAVELOAD_CLASS( 0x00121200, CGeometry )
REGISTER_SAVELOAD_CLASS( 0x00221160, CTypeface )
REGISTER_SAVELOAD_CLASS( 0x00721150, CSkeleton )
REGISTER_SAVELOAD_CLASS( 0x00531180, CAIGeometry )
REGISTER_SAVELOAD_CLASS( 0x02841135, CParticle )
REGISTER_SAVELOAD_CLASS( 0xF1801161, CTerrainTile )
REGISTER_SAVELOAD_CLASS( 0x02961130, CAmbientLight )
REGISTER_SAVELOAD_CLASS( 0x01771170, CTMaterial )
REGISTER_SAVELOAD_CLASS( 0x01771171, CRndModel )
REGISTER_SAVELOAD_CLASS( 0x01871140, CTRndModel )
REGISTER_SAVELOAD_CLASS( 0x02881170, CSound )
REGISTER_SAVELOAD_CLASS( 0x11291120, CDebrisMaterial )
REGISTER_SAVELOAD_CLASS( 0x11291123, CRndObject )
REGISTER_SAVELOAD_CLASS_NM( 0x11291121, CDebris, NDb )
REGISTER_SAVELOAD_CLASS_NM( 0x11291122, CObject, NDb )
REGISTER_SAVELOAD_CLASS( 0x11391160, CTRndObject )
REGISTER_SAVELOAD_CLASS( 0x00981160, CRndContainerModel )
REGISTER_SAVELOAD_CLASS( 0x019a1190, CRndConstructionPart )
REGISTER_SAVELOAD_CLASS( 0x025a1180, CGrass )
REGISTER_SAVELOAD_CLASS( 0x001b1140, CUITexture )
REGISTER_SAVELOAD_CLASS( 0x01712180, CCubeTexture )
REGISTER_SAVELOAD_CLASS( 0xA2112120, CSoundVariant )
REGISTER_SAVELOAD_CLASS( 0xA2112121, CTSound )
REGISTER_SAVELOAD_CLASS( 0x02822140, CDoor )
REGISTER_SAVELOAD_CLASS( 0x02822141, CGun )
REGISTER_SAVELOAD_CLASS( 0x11932120, CTEffect )
REGISTER_SAVELOAD_CLASS( 0x11932121, CEffect )
REGISTER_SAVELOAD_CLASS( 0x11932122, CParticleInstance )
REGISTER_SAVELOAD_CLASS( 0x10842170, CHead )
REGISTER_SAVELOAD_CLASS( 0x10842171, CSequence )
REGISTER_SAVELOAD_CLASS( 0x11162120, CLightInstance )
REGISTER_SAVELOAD_CLASS( 0x11162121, CAnimLight )
REGISTER_SAVELOAD_CLASS( 0x11862140, CComplexHead )
REGISTER_SAVELOAD_CLASS( 0xA1962150, CMusic )
REGISTER_SAVELOAD_CLASS( 0xB1962150, CGlobalMap )
REGISTER_SAVELOAD_CLASS( 0xB1962151, CChapterMap )
REGISTER_SAVELOAD_CLASS( 0xA2662140, CPlacableObject )
REGISTER_SAVELOAD_CLASS( 0xA2972170, CSpot )
REGISTER_SAVELOAD_CLASS( 0x51892140, CPassageObject )
REGISTER_SAVELOAD_CLASS( 0xA16A2180, CTAmbientLight )
REGISTER_SAVELOAD_CLASS( 0x020A2160, CAmbientLightReal )
REGISTER_SAVELOAD_CLASS( 0xA22A2160, CSoundInstance )
REGISTER_SAVELOAD_CLASS( 0xA22A2161, CSoundEffect )
REGISTER_SAVELOAD_CLASS( 0xA21B2160, CTranslatedString )
//BASIC_REGISTER_CLASS( CRndConstructionPart )
