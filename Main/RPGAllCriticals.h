#ifndef __RPGALLCRITICALS_H_
#define __RPGALLCRITICALS_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "RPGCritical.h"
#include "RPGUnitMission.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NRPG
{
////////////////////////////////////////////////////////////////////////////////////////////////////
#define CRITICAL_METHODS( a ) \
	OBJECT_NOCOPY_METHODS( a ); \
	~a() { RemoveModifiers(); }
////////////////////////////////////////////////////////////////////////////////////////////////////
// модификация статов в зависимости от VP
////////////////////////////////////////////////////////////////////////////////////////////////////
class CVPCritical: public CCritical
{
	CRITICAL_METHODS(CVPCritical);
public:
	CVPCritical() {}
	CVPCritical( const SCritical &crit ): CCritical( crit ) {}

	virtual bool SetModifiers( CUnit *pRPGUnit, IUnitMission *pRPGMission );
	virtual bool CanBeSuspended() { return true; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAPCritical: public CCritical
{
	CRITICAL_METHODS(CAPCritical);
public:
	CAPCritical() {}
	CAPCritical( const SCritical &crit ): CCritical( crit ) {}

	virtual bool SetModifiers( CUnit *pRPGUnit, IUnitMission *pRPGMission );
	virtual bool CanBeSuspended() { return true; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CWeaponSkillCritical: public CCritical
{
	CRITICAL_METHODS(CWeaponSkillCritical);
public:
	CWeaponSkillCritical() {}
	CWeaponSkillCritical( const SCritical &crit ): CCritical( crit ) {}

	virtual bool SetModifiers( CUnit *pRPGUnit, IUnitMission *pRPGMission );
	virtual bool CanBeSuspended() { return true; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// необратимый
class CDeathCritical: public CCritical
{
	CRITICAL_METHODS(CDeathCritical);
public:
	CDeathCritical() {}
	CDeathCritical( const SCritical &crit ): CCritical( crit ) {}

	virtual bool SetModifiers( CUnit *pRPGUnit, IUnitMission *pRPGMission );
	virtual bool CanBeSuspended() { return false; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMotionlessCritical: public CCritical
{
	CRITICAL_METHODS(CMotionlessCritical);
	ZDATA_(CCritical)
	CPtr<IUnitMission> pRPGMission;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCritical*)this); f.Add(2,&pRPGMission); return 0; }
	
	CMotionlessCritical() {}
	CMotionlessCritical( const SCritical &crit ): CCritical( crit ) {}

	virtual bool SetModifiers( CUnit *pRPGUnit, IUnitMission *pRPGMission );
	virtual void RemoveModifiers();
	virtual bool CanBeSuspended() { return true; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CLostWeaponCritical: public CCritical
{
	CRITICAL_METHODS(CLostWeaponCritical);
public:
	CLostWeaponCritical() {}
	CLostWeaponCritical( const SCritical &crit ): CCritical( crit ) {}

	virtual bool SetModifiers( CUnit *pRPGUnit, IUnitMission *pRPGMission );
	virtual bool CanBeSuspended() { return false; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CIdleHandCritical: public CCritical
{
	CRITICAL_METHODS(CIdleHandCritical);
	ZDATA_(CCritical)
	CPtr<IUnitMission> pRPGMission;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCritical*)this); f.Add(2,&pRPGMission); return 0; }

	CIdleHandCritical() {}
	CIdleHandCritical( const SCritical &crit ): CCritical( crit ) {}

	virtual bool SetModifiers( CUnit *pRPGUnit, IUnitMission *pRPGMission );
	virtual void RemoveModifiers();
	virtual bool CanBeSuspended() { return true; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAccidentalShotCritical: public CCritical
{
	CRITICAL_METHODS(CAccidentalShotCritical);
public:
	CAccidentalShotCritical() {}
	CAccidentalShotCritical( const SCritical &crit ): CCritical( crit ) {}

	virtual bool SetModifiers( CUnit *pRPGUnit, IUnitMission *pRPGMission );
	virtual bool CanBeSuspended() { return false; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDamageWeaponCritical: public CCritical
{
	CRITICAL_METHODS(CDamageWeaponCritical);
public:
	CDamageWeaponCritical() {}
	CDamageWeaponCritical( const SCritical &crit ): CCritical( crit ) {}

	virtual bool SetModifiers( CUnit *pRPGUnit, IUnitMission *pRPGMission );
	virtual bool CanBeSuspended() { return false; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CBlindCritical: public CCritical
{
	CRITICAL_METHODS(CBlindCritical);
	ZDATA_(CCritical)
	CPtr<IUnitMission> pRPGMission;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCritical*)this); f.Add(2,&pRPGMission); return 0; }

	CBlindCritical() {}
	CBlindCritical( const SCritical &crit ): CCritical( crit ) {}

	virtual bool SetModifiers( CUnit *pRPGUnit, IUnitMission *pRPGMission );
	virtual void RemoveModifiers();
	virtual bool CanBeSuspended() { return true; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CStunCritical: public CCritical
{
	CRITICAL_METHODS(CStunCritical);
	ZDATA_(CCritical)
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCritical*)this); return 0; }

	CStunCritical() {}
	CStunCritical( const SCritical &crit ): CCritical( crit ) {}

	virtual bool SetModifiers( CUnit *pRPGUnit, IUnitMission *pRPGMission );
	virtual bool CanBeSuspended() { return true; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPatientCritical: public CCritical
{
	CRITICAL_METHODS(CPatientCritical);
	ZDATA_(CCritical)
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCritical*)this); return 0; }

	CPatientCritical() {}
	CPatientCritical( const SCritical &crit ): CCritical( crit ) {}

	virtual bool SetModifiers( CUnit *pRPGUnit, IUnitMission *pRPGMission ) { return true; }
	virtual bool CanBeMerged() const { return false; }
	virtual bool CanBeSuspended() { return false; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDeafCritical: public CCritical
{
	CRITICAL_METHODS(CDeafCritical);
	ZDATA_(CCritical)
	CPtr<IUnitMission> pRPGMission;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCritical*)this); f.Add(2,&pRPGMission); return 0; }

	CDeafCritical() {}
	CDeafCritical( const SCritical &crit ): CCritical( crit ) {}

	virtual bool SetModifiers( CUnit *pRPGUnit, IUnitMission *pRPGMission );
	virtual void RemoveModifiers();
	virtual bool CanBeSuspended() { return true; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPanzerkleinCritical: public CCritical
{
	CRITICAL_METHODS(CPanzerkleinCritical);
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCritical*)this); return 0; }

	CPanzerkleinCritical() {}
	CPanzerkleinCritical( const SCritical &crit ): CCritical( crit ) {}

	virtual bool SetModifiers( CUnit *pRPGUnit, IUnitMission *pRPGMission ) { return true; }
	virtual bool CanBeSuspended() { return false; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CBleedingCritical: public CCritical
{
	CRITICAL_METHODS( CBleedingCritical );
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CCritical*)this); return 0; }

	CBleedingCritical() {}
	CBleedingCritical( const SCritical &crit ): CCritical( crit ) {}

	virtual bool SetModifiers( CUnit *pRPGUnit, IUnitMission *pRPGMission ) { return true; }
	virtual bool CanBeSuspended() { return true; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __RPGALLCRITICALS_H_