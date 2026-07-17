#ifndef __DataAI_H_
#define __DataAI_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\ADOImport\BasicDB.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
class CSound;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAISound: public CDBRecord
{
	OBJECT_BASIC_METHODS(CAISound);
public:
	float fRadius;
	float vRadius[5];
	CPtr<CSound> pSound;
	bool bTileTypeIndependent;

	int operator&( CStructureSaver &f );
	virtual void Import();
	virtual int GetRadiusFromAISoundType( int nAISoundType );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// NDb::SAISound (release-new, UDT size 12) -- the sound descriptor carried through the whole AI-noise chain
// (CWorld::MakeAISound @0x36aaf0 -> CUnitServer::CanHearSound @0x3c32e0 -> CUnitMission::CanHearSound @0x2c2ee0
// -> GetHearingProbability @0x2bff30): the AI sound record + its tile type + a silencer attenuation (weapon
// "Silencer" coeff on shots, 1/quiet-step-perk on steps, 1.0 elsewhere). Built transiently (e.g. from
// NDb::GetAISound(n)) and NOT serialized; pAISound is a non-owning DB reference (CDBPtr, matching the binary).
struct SAISound
{
	CDBPtr<CAISound> pAISound;
	int              nSoundType;
	float            fSilencer;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitGroup: public CDBRecord
{
	OBJECT_BASIC_METHODS(CUnitGroup)
public:
	ZDATA_(CDBRecord)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); return 0; }

	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CAISound* GetAISound( int nModelID );
CUnitGroup* GetUnitGroup( int nGroupID );
}
#endif
