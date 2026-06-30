#ifndef __RPGTOHIT_H_
#define __RPGTOHIT_H_

namespace NAI
{
	class IAIUnit;
	struct SPosition;
	struct SUnitPosition;
}

namespace NDb
{
	class CRPGGrenade;
}

namespace NWorld
{
	class CObjectServerBase;
	class CUnitServer;
}

namespace NRPG
{
class CWeaponItem;
class IInventoryItem;
class IGrenadeItem;
class IMeleeWeaponItem;
////////////////////////////////////////////////////////////////////////////////////////////////////
// IToHitCalcer
////////////////////////////////////////////////////////////////////////////////////////////////////
class IToHitCalcer:	public CObjectBase
{
public:
	virtual int GetToHit() = 0;
	virtual void Log() = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CToHitCalcer
////////////////////////////////////////////////////////////////////////////////////////////////////
// Release migration (byte-faithful, session 25): the per-shot accumulator now keeps a FLOAT hit-cover
// (was int nHitCover), plus the attacking unit's server (pUnitServer) and a night flag (bNight). The
// ctor takes the CUnitServer* and pulls the mission out of it (server->GetUnitRPG()). The aura /
// weather / day-night to-hit subsystems are release-new and ABSENT in this predecessor dev tree
// (no NRPG::GetAuraAdd, no CWorld::BadWeather/IsNight) -> GetAuraToHitAdd/GetWeatherPenalty return 0
// and bNight is wired false at every call site (documented elisions); the members/methods are kept so
// the save format (operator& tags 9/18/19) is byte-exact and the subsystems can be wired in later.
class CToHitCalcer: public IToHitCalcer
{
	ZDATA
public:
	int nSkill;
	float fToHit;
	float fMovePenalty;
	SWeaponInfo sWeaponInfo;
	CPtr<IUnitMission> pUnitMission;
	int nExtraAP; // ��� careful shot
	int nSnipeAP;
	float fHitCover;
	int nDistance;
	CVec3 ptAttacker;
	bool bFirstRound;
	NAI::EPose eCurPose;
	CVec3 ptIllumination;
	int nBullet;
	CPtr<CWeaponItem> pWeaponItem;
	bool bBackStab;
	CPtr<NWorld::CUnitServer> pUnitServer;
	bool bNight;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nSkill); f.Add(3,&fToHit); f.Add(4,&fMovePenalty); f.Add(5,&sWeaponInfo); f.Add(6,&pUnitMission); f.Add(7,&nExtraAP); f.Add(8,&nSnipeAP); f.Add(9,&fHitCover); f.Add(10,&nDistance); f.Add(11,&ptAttacker); f.Add(12,&bFirstRound); f.Add(13,&eCurPose); f.Add(14,&ptIllumination); f.Add(15,&nBullet); f.Add(16,&pWeaponItem); f.Add(17,&bBackStab); f.Add(18,&pUnitServer); f.Add(19,&bNight); return 0; }

	// ������� ������������� �������� � xls ��������
	virtual float GetStance();
	virtual float GetD1();
	virtual float GetD2();
	virtual float GetRS();
	virtual float GetSMove();
	virtual float GetLight();
	virtual float GetFRMult();
	virtual float GetAllMult();
	virtual float GetSnipeAdd();
	virtual float GetTArea();
	virtual float GetCA();
	virtual float GetAllAdd();
	virtual int GetSkill() { return nSkill; }
	// release-new terms (RVA 0x2b85a0 / 0x2b8540 / 0x2b7090). Weather + aura are absent in this
	// predecessor tree -> 0 (documented elision); careful-shoot perk uses the present IUnitMission::HasPerk.
	float GetWeatherPenalty();
	float GetCarefulShootPerk();
	virtual float GetAuraToHitAdd();

	CToHitCalcer() {}
	CToHitCalcer( NWorld::CUnitServer *_pUnitServer, NAI::EPose _eCurPose,
		int _nDistance, CVec3 _ptAttacker, int _nExtraAP, int _nSnipeAP,
		float _fHitCover, bool _bFirstRound, bool _bNight, CVec3 _ptIllumination, int _nBullet, bool _bBackStab );

	virtual void FillWeaponInfo();
	virtual void Prepare();
	virtual int GetToHit(); // �������� �������� ToHit
	virtual void Log();		// ������� Log
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitToHitCalcer
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitToHitCalcer: public CToHitCalcer
{
	OBJECT_BASIC_METHODS(CUnitToHitCalcer)
	ZDATA_(CToHitCalcer)
public:
	CPtr<NWorld::CUnitServer> pTarget;
	NAI::SPosition sTargetPosition;
	NAI::EHitLocation eHitLocation;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CToHitCalcer*)this); f.Add(2,&pTarget); f.Add(3,&sTargetPosition); f.Add(4,&eHitLocation); return 0; }

	virtual float GetTArea();
	virtual float GetTMove();
	virtual float GetAllAdd();
	virtual float GetCA();          // release-new override (headshot weighting + ORIGINAL BUG no-cover fdiv)
	float GetTAuraEvasionAdd();      // aura subsystem absent -> 0 (documented elision)

	CUnitToHitCalcer() {}
	CUnitToHitCalcer(	NWorld::CUnitServer *_pUnitServer, NAI::EPose _eCurPose, int _nDistance,
		CVec3 _ptAttacker, NAI::SPosition _sTargetPosition, int _nExtraAP, int _nSnipeAP,
		float _fHitCover, bool _bFirstRound, bool _bNight, CVec3 _ptIllumination, NAI::EHitLocation _eHitLocation,
		NWorld::CUnitServer *_pTarget, int _nBullet, bool _bBackStab	);

	virtual int GetToHit();
	virtual void Log();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTileToHitCalcer
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTileToHitCalcer: public CToHitCalcer
{
	OBJECT_BASIC_METHODS(CTileToHitCalcer)
	ZDATA_(CToHitCalcer)
	CVec3 ptTilePos;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CToHitCalcer*)this); f.Add(2,&ptTilePos); return 0; }

	virtual float GetTArea();

	CTileToHitCalcer() {}
	CTileToHitCalcer( NWorld::CUnitServer *_pUnitServer, NAI::EPose _eCurPose, int _nDistance,
		CVec3 _ptAttacker, int _nExtraAP, float _fHitCover, bool _bFirstRound, bool _bNight, CVec3 _ptIllumination,
		CVec3 _ptTilePos, int _nBullet );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CGrenadeToHitCalcer
////////////////////////////////////////////////////////////////////////////////////////////////////
class CGrenadeToHitCalcer: public CToHitCalcer
{
	OBJECT_BASIC_METHODS(CGrenadeToHitCalcer)
	ZDATA_(CToHitCalcer)
	CVec3 ptTilePos;
	CPtr<NRPG::IGrenadeItem> pGrenade;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CToHitCalcer*)this); f.Add(2,&ptTilePos); f.Add(3,&pGrenade); return 0; }

	virtual void Prepare();
	virtual float GetMaxImp();
	virtual float GetRelWeight();
	int GetGrenadeMaxDistance();
	float GetMaxGrenadeVelocity();
	virtual void FillWeaponInfo();
	virtual float GetStance();

	virtual float GetAllAdd();
	virtual int GetToHit();
	virtual void Log();

	CGrenadeToHitCalcer() {}
	// _pGrenade: the grenade being evaluated/thrown. Defaults to 0 -> derive it from the unit's active
	// item (release behaviour, correct for the player throw and the equipped-grenade exec path). The AI
	// evaluates a grenade still in the inventory, so it must pass the item explicitly (else GetActive
	// returns the equipped weapon, the cast is null, and Prepare() crashes).
	CGrenadeToHitCalcer( NWorld::CUnitServer *_pUnitServer, NAI::EPose _eCurPose, int _nDistance,
		CVec3 _ptAttacker, bool _bFirstRound, bool _bNight, CVec3 _ptIllumination, CVec3 _ptTilePos,
		NRPG::IGrenadeItem *_pGrenade = 0 );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIUnitToHitCalcer
////////////////////////////////////////////////////////////////////////////////////////////////////
// The ctor keeps the dev IAIUnit* shape (an internal API, not serialized) and derives both
// CUnitServer*s + bNight internally to chain the release CUnitToHitCalcer ctor -- so the AI callers
// (aiUnit/aiWeapon) need no change. bNight derives false (IsNight absent in this tree, documented).
class CAIUnitToHitCalcer: public CUnitToHitCalcer
{
	OBJECT_BASIC_METHODS(CAIUnitToHitCalcer)
	ZDATA
	ZPARENT(CUnitToHitCalcer);
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CUnitToHitCalcer*)this); return 0; }

	virtual float GetTArea();
	virtual float GetTMove();

	CAIUnitToHitCalcer() {}
	CAIUnitToHitCalcer(	NAI::IAIUnit *pShooter, const NAI::SUnitPosition &shooterPos,
		NAI::IAIUnit *pTarget, int nHitCover,	NAI::EHitLocation _eHitLocation, int _nBullet, IInventoryItem *_pWeapon, int _nExtraAP );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CThrowKnifeToHitCalcer
////////////////////////////////////////////////////////////////////////////////////////////////////
class CThrowKnifeToHitCalcer: public CToHitCalcer
{
	OBJECT_BASIC_METHODS(CThrowKnifeToHitCalcer)
	ZDATA_(CToHitCalcer)
public:
	CPtr<NRPG::IMeleeWeaponItem> pMeleeWeapon;  // release-new (+0x8c): the thrown blade's item
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CToHitCalcer*)this); f.Add(2,&pMeleeWeapon); return 0; }

	virtual void FillWeaponInfo();
	int GetKnifeMaxDistance();
	virtual void Prepare();

	CThrowKnifeToHitCalcer() {}
	CThrowKnifeToHitCalcer( NWorld::CUnitServer *_pUnitServer, NAI::EPose _eCurPose, int _nDistance,
		CVec3 _ptAttacker, float _fHitCover, bool _bFirstRound, bool _bNight, CVec3 _ptIllumination,
		int _nExtraAP, bool _bBackStab );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CThrowKnifeUnitToHitCalcer  (release-new: throw-knife vs a living unit)
////////////////////////////////////////////////////////////////////////////////////////////////////
class CThrowKnifeUnitToHitCalcer: public CThrowKnifeToHitCalcer
{
	OBJECT_BASIC_METHODS(CThrowKnifeUnitToHitCalcer)
	ZDATA_(CThrowKnifeToHitCalcer)
public:
	CPtr<NWorld::CUnitServer> pTarget;
	NAI::EHitLocation eHitLocation;
	NAI::SPosition targetPos;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CThrowKnifeToHitCalcer*)this); f.Add(2,&pTarget); f.Add(3,&eHitLocation); f.Add(4,&targetPos); return 0; }

	virtual int GetToHit();

	CThrowKnifeUnitToHitCalcer() {}
	CThrowKnifeUnitToHitCalcer( NWorld::CUnitServer *_pUnitServer, NAI::EPose _eCurPose, int _nDistance,
		CVec3 _ptAttacker, float _fHitCover, bool _bFirstRound, bool _bNight, CVec3 _ptIllumination,
		NWorld::CUnitServer *_pTarget, NAI::EHitLocation _eHitLocation, const NAI::SPosition &_targetPos,
		int _nExtraAP, bool _bBackStab );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CThrowKnifeTileToHitCalcer
////////////////////////////////////////////////////////////////////////////////////////////////////
class CThrowKnifeTileToHitCalcer: public CThrowKnifeToHitCalcer
{
	OBJECT_BASIC_METHODS(CThrowKnifeTileToHitCalcer)
	ZDATA_(CThrowKnifeToHitCalcer)
	CVec3 ptTilePos;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CThrowKnifeToHitCalcer*)this); f.Add(2,&ptTilePos); return 0; }

	CThrowKnifeTileToHitCalcer( NWorld::CUnitServer *_pUnitServer, NAI::EPose _eCurPose, int _nDistance,
		CVec3 _ptAttacker, float _fHitCover, bool _bFirstRound, bool _bNight, CVec3 _ptIllumination,
		CVec3 _ptTilePos, int _nExtraAP );
	CThrowKnifeTileToHitCalcer() {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CRLauncherToHitCalcer
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRLauncherToHitCalcer: public CToHitCalcer
{
	OBJECT_BASIC_METHODS(CRLauncherToHitCalcer)
	ZDATA_(CToHitCalcer)
	CVec3 ptTilePos;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CToHitCalcer*)this); f.Add(2,&ptTilePos); return 0; }

	CRLauncherToHitCalcer() {}
	CRLauncherToHitCalcer( NWorld::CUnitServer *_pUnitServer, NAI::EPose _eCurPose, int _nDistance,
		CVec3 _ptAttacker, float _fHitCover, int _nExtraAP, bool _bFirstRound, bool _bNight, CVec3 _ptIllumination,
		CVec3 _ptTilePos );

	int GetMaxDistance();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif
