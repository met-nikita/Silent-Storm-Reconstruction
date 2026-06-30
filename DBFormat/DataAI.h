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
// NDb::SAISound (release-new, UDT size 12) -- a sound descriptor passed to CUnitMission::GetHearingProbability:
// the AI sound record + its type + a silencer attenuation. Built transiently (e.g. from NDb::GetAISound(n)) and
// NOT serialized; pAISound is a non-owning DB reference (CDBPtr, matching the binary). Dead until the AI hearing
// query / assassin reaction consumes it.
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
