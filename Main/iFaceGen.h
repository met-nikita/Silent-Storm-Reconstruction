#ifndef __A5_I_FACEGEN_H_
#define __A5_I_FACEGEN_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
	class CSide;
	class CRPGPers;
	class CNationality;
	class CDBDifficulty;   // fwd: CDBPtr member needs only a fwd-decl; complete type via the .cpp
}
namespace NRPG
{
	class CGlobalPlayer;
	class CUnit;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICFaceGen -- release merc-based form (ctor @0x1d34b0): the caller (HeroMenu/CharGen) creates the merc
// (CreateMerc) and threads it + the chosen nationality/difficulty in. (The merc display + head/voice change
// use the existing CreateMerc path; the byte-faithful CUnit pHeadInfo mutation is gated by the CUnit
// re-architecture and tracked separately.)
////////////////////////////////////////////////////////////////////////////////////////////////////
class CICFaceGen: public NMainLoop::CInterfaceCommand
{
	OBJECT_BASIC_METHODS(CICFaceGen);
private:
	CPtr<NRPG::CUnit> pMerc;
	CDBPtr<NDb::CSide> pSide;
	CDBPtr<NDb::CNationality> pNationality;
	CDBPtr<NDb::CDBDifficulty> pDifficulty;   // chosen difficulty, threaded HeroMenu/CharGen -> CICBeginGame

public:
	CICFaceGen() {}
	CICFaceGen( NDb::CSide *pSide, NDb::CNationality *pNationality, NDb::CDBDifficulty *pDifficulty, NRPG::CUnit *pMerc );

	virtual void Exec();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
