#include "StdAfx.h"
#include "rpgobject.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataGeometry.h"
#include "..\DBFormat\DataMap.h"
#include "RPGUnit.h"
#include "RPGAttackMech.h"
#include "..\MiscDll\LogStream.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NRPG
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CObject: public IObject, public IAttackable
{
	OBJECT_BASIC_METHODS(CObject);
public:
	ZDATA
	int nVP;
	int nDestroyStages, nStage;
	int nMaxVP;
	// retail CObject @+0x1c -- the per-destroy-stage armour, one CDBPtr per stage
	// (stagesArmors.size() == nDestroyStages). Filled by the ctor @0x2ad680 from the DB object's
	// stage models and serialized as tag 6 by operator& @0x2addc0 (DoVector @0x2ade40 -> one
	// {1:{1:int nID}} chunk per element = 8 bytes; a stage whose model carries no armour stores a
	// null ref, written as record id -1). This member is also what makes sizeof(CObject) == 0x38,
	// the size retail's CreateObject @0x2ad830/@0x2ad8a0 pass to operator new.
	vector< CDBPtr<NDb::CRPGArmor> > stagesArmors;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nVP); f.Add(3,&nDestroyStages); f.Add(4,&nStage); f.Add(5,&nMaxVP); f.Add(6,&stagesArmors); return 0; }

	CObject() : nVP(0), nDestroyStages(0), nStage(0) {}
	CObject( int nStages, NDb::CModel *pModel, int nStartStage, NDb::CObject *pDBObject );
	virtual int ProcessAttack( NWorld::IWorld *pWorld, int nUserID, CAttackPortion *pAttack,
		const CVec3 &vDir, NDb::CRPGArmor *pArmor );
	virtual bool IsDead() const;
	virtual int GetDestroyStage();
	virtual void SetDestroyStage( int _nStage );
	virtual void Kill();
	virtual int GetHP() { return nVP; }   // luaObjectGetHP: current vitality points
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CObject::CObject( int nStages, NDb::CModel *pModel, int nStartStage, NDb::CObject *pDBObject ): nDestroyStages(nStages), nStage(0)
{
	nMaxVP = IsValid( pModel ) ? pModel->GetMaxVP() : 1;
	// retail CObject ctor @0x2ad680 (disasm: cmp eax,1; jg skip; mov eax,1): clamp nMaxVP >= 1.
	// GetMaxVP = volume/0.7 * solidPart * material->nVP rounds/truncates to 0 for small hulls
	// (the shed gas tank), and ProcessAttack's `nMaxVP <= 0 -> return 0` early-out then made the
	// object INVULNERABLE to structural damage: its own 10x ignition blast hit it for 300..650
	// and returned 0, so it never advanced a destroy
	// stage, never played the destroy sound, never chained. Retail instead gives such props
	// 1 VP -- any hit >= 1 dmg jumps the stage recompute to the FINAL stage in one blow.
	if ( nMaxVP <= 1 )
		nMaxVP = 1;
	SetDestroyStage( nStartStage );
	// retail ctor @0x2ad680 tail: when built from a DB object (the CreateObject( NDb::CObject*, int )
	// path @0x2ad8a0), stow the armour of every destruction stage. The stage armour is the RPGArmor
	// of that stage's container model, so a hit that breaks through into stage N is then resolved
	// against stage N's own material rather than the caller's armour. Plain null checks, not
	// IsValid() -- retail tests only "pModels[i] == 0 || pModels[i]->pModel == 0" (disasm: cmp/je on
	// the CPtr and on [ptr+0xc]); a stage model with no armour stores a null ref (wire id -1).
	if ( pDBObject )
	{
		stagesArmors.resize( nDestroyStages );
		for ( int i = 0; i < nDestroyStages; ++i )
		{
			if ( pDBObject->pModels[i] && pDBObject->pModels[i]->pModel )
				stagesArmors[i] = pDBObject->pModels[i]->pModel->pRPGArmor;
			else
				stagesArmors[i] = 0;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObject::SetDestroyStage( int _nStage )
{
	ASSERT( _nStage >= 0 && _nStage <= nDestroyStages );
	int nTmpStage = Clamp( _nStage, 0, nDestroyStages );
	nVP = nMaxVP - ( nDestroyStages > 0 ? ( nTmpStage * nMaxVP ) / nDestroyStages : 0 );
	nStage = nTmpStage;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x2ad3f0 (raw disasm authoritative: Ghidra's decomp is unusable here -- the CReceivedDmg
// sret shifts every argument by one and `this` is the IAttackable subobject at CObject+8, so it
// renames all four ints and both vector pointers one member out of step).
// THIS is the consumer of stagesArmors (wire tag 6, landed 2891ac1: round-tripped but never read).
// Dev resolved the whole attack against the caller's single pArmor in one shot; retail walks the
// destruction stages, re-arming with each stage's own armour as the damage breaks through.
int CObject::ProcessAttack( NWorld::IWorld *pWorld, int nUserID, CAttackPortion *pAttack,
	const CVec3 &vDir, NDb::CRPGArmor *pArmor )
{
	// nUserID and vDir are accepted but unread here (retail reads neither).
	if ( pAttack->atkType == AT_CLICK_OF_DEATH )
	{
		// ORIGINAL BUG (confirmed by raw disasm @0x6ad48a: `add eax,1; mov [esi+0xc],eax`, no clamp):
		// a click of death on an already-destroyed prop pushes nStage PAST nDestroyStages, and since
		// IsDead() is `nStage == nDestroyStages` the object then reads as ALIVE again.
		if ( nStage >= nDestroyStages )
		{
			++nStage;
			return 1;
		}
		int nOldVP = nVP;
		++nStage;
		// retail 0x6ad423..0x6ad442, on the ALREADY-incremented nStage. Dev computed
		// `nMaxVP - Float2Int( nMaxVP * (nStage + 0.5f) )` -- never divided by nDestroyStages, so it
		// drove nVP to 0 on the first click for any nStage >= 1. Fixed: this branch is part of @0x2ad3f0.
		if ( nDestroyStages > 0 )
			nVP = Max( 0, Float2Int( ( nDestroyStages - ( nStage + 0.5f ) ) * nMaxVP / nDestroyStages ) );
		else
			nVP = 0;
		ASSERT( nVP <= nOldVP );
		return nOldVP - nVP;
	}
	if ( nMaxVP <= 0 )
		return 0;
	int nTotalDmg = 0;
	NDb::CRPGArmor *pStageArmor = pArmor;
	// retail 0x6ad4cb / 0x6ad5dd `cmp [esi+0xc],4`: a hard literal cap of 4 stages, which is also the
	// largest nDestroyStages the data can produce (N_DESTROY_STAGES 5 - 1). Kept as retail's literal.
	while ( nStage < 4 )
	{
		// VP at which the current stage gives way to the next one
		int nBoundaryVP = 0;
		if ( nDestroyStages > 0 )
			nBoundaryVP = Max( 0, ( ( nDestroyStages - nStage - 1 ) * nMaxVP ) / nDestroyStages );
		// VP still absorbable in this stage; retail replaces a non-positive figure with 100000,
		// i.e. "this stage cannot stop anything, let the hit land whole".
		int nRoom = nVP - nBoundaryVP;
		if ( nRoom <= 0 )
			nRoom = 100000;
		// CalcStructDmg subtracts nTotalDmg, so each pass yields only the damage this attack has not
		// yet dealt -- without it the loop would apply a fresh full-strength hit per stage.
		int nDmg = Max( 0, pAttack->CalcStructDmg( pWorld, pStageArmor, nTotalDmg ) );
		if ( nDmg <= nRoom )
		{
			// absorbed within the current stage -> settle and stop
			nTotalDmg += nDmg;
			nVP = Max( 0, nVP - nDmg );
			nStage = ( ( nMaxVP - nVP ) * nDestroyStages ) / nMaxVP;
			csRPG << "Shoot hit object, result damage = " << nDmg << ", remain " << nVP << " of " << nMaxVP << "\n";
			break;
		}
		// the hit breaks through: spend the stage, step to the next one and re-arm with its armour
		nTotalDmg += nRoom;
		++nStage;
		nVP = nBoundaryVP;
		// plain null test (NOT IsValid), same as the ctor @0x2ad680: a stage whose model carried no
		// armour falls back to the caller's.
		if ( nStage < (int)stagesArmors.size() && stagesArmors[nStage] )
			pStageArmor = stagesArmors[nStage];
		else
			pStageArmor = pArmor;
		csRPG << "Shoot hit object, destroy stage changed, result damage (partial) = " << nRoom << ", remain " << nVP << " of " << nMaxVP << "\n";
	}
	return nTotalDmg;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CObject::IsDead() const
{
	return nStage == nDestroyStages;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CObject::GetDestroyStage()
{
	return nStage;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObject::Kill()
{
	nStage = nDestroyStages;
	nVP = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IObject* CreateObject( int nStages, NDb::CModel *pModel, int nStartStage )
{
	// retail @0x2ad830: no DB object on this path -> no per-stage armours (empty tag 6).
	return new CObject( nStages, pModel, nStartStage, 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IObject *CreateObject( NDb::CObject *pDBObject, int nStartStage )
{
	ASSERT( IsValid( pDBObject ) );
	if ( !IsValid( pDBObject ) )
		return 0;
	//
	int nStages = pDBObject->GetStagesQuantity();
	ASSERT( nStages > 0 );
  if ( nStages > 0 )
		// retail @0x2ad8a0 constructs CObject directly here (it does NOT route through the
		// CreateObject( int, CModel*, int ) overload) so that pDBObject reaches the ctor and the
		// per-stage armours get stowed. Routing through the 3-arg overload would drop pDBObject
		// and leave stagesArmors empty.
		return new CObject( nStages - 1, pDBObject->pModels[0]->pModel, nStartStage, pDBObject );
	else
		return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
/*IObject* CreateTerrain()
{
	CObject *pRes = new CObject;
	pRes->pArmor = NDb::GetArmor(NDb::CRPGArmor::GROUND);
	pRes->nVP = -1;
	return pRes;
}*/
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CRPGArmor* GetTerrainArmor()
{
	return NDb::GetArmor(NDb::N_DEFAULT_ARMOR);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
REGISTER_SAVELOAD_CLASS_NM( 0x02781180, CObject, NRPG )
using namespace NRPG;
BASIC_REGISTER_CLASS( IObject )
