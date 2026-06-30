#ifndef __RPGCRITICAL_H_
#define __RPGCRITICAL_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "RPGUnit.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAI
{
	enum EHitLocation;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NRPG
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnit;
class IUnitMission;
class CSkillModifier;
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SCritical
{
	int   nDC; // Difficulty Class
	int   nDuration;
	float fValue;
	NDb::ECritical eCritical;
	NDb::ECriticalLocation eCl;

	SCritical() {}
	SCritical( NDb::ECriticalLocation hl, NDb::ECritical cr, int _nDuration = -1, float _fValue = 0, int _nDC = N_MAX_DC )
		: eCl(hl), eCritical(cr), nDuration(_nDuration), fValue(_fValue), nDC(_nDC) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCritical: public ICriticalInfo
{
	//OBJECT_NOCOPY_METHODS(CCritical);

protected:
	void PushModifier( CSkillModifier *pModifier ) { modifiers.push_back( pModifier ); }

private:
	ZDATA
	int nTurn;
	vector<CPtr<CSkillModifier> > modifiers;
protected:
	SCritical critical;

public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nTurn); f.Add(3,&modifiers); f.Add(4,&critical); return 0; }

	CCritical() {}
	CCritical( const SCritical &crit );

	// false, если для этого критикала не требуется отмена (не было повешено никаких модификаторов)
	virtual bool SetModifiers( CUnit *pRPGUnit, IUnitMission *pRPGMission  )
	{
		OutputDebugString( "Empty critical\n" );
		return false;
	}
	virtual void RemoveModifiers() { modifiers.clear(); }
	virtual bool CanBeSuspended() = 0;
	virtual bool CanBeMerged() const { return true; }

	enum { WEAKER, MERGED, OTHER }; // результаты функции Merge
	int Merge( CCritical *pCritical ) const;

	bool NextTurn();		// true пока время действия данного critical'а не истекло

	int  GetRemainingTime() const;
	bool IsTemporarily() const { return critical.nDuration >= 0; }
	float GetModifier() const;
	const SCritical& GetCritical() const { return critical; }

	virtual int GetDifficultyClass() const { return critical.nDC; }
	virtual NDb::ECritical GetCriticalType() const { return critical.eCritical; }
	virtual NDb::ECriticalLocation GetCriticalLocation() const { return critical.eCl; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CCritical* CreateCritical( const SCritical &critical );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif //__RPGCRITICAL_H_
