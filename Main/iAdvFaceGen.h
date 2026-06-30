#ifndef __A5_I_ADVFACEGEN_H_
#define __A5_I_ADVFACEGEN_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
	class CSide;
	class CNationality;
	class CDBDifficulty;   // fwd: CDBPtr member needs only a fwd-decl; complete type via the .cpp
}
namespace NRPG
{
	class CUnit;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICAdvFaceGen -- the queued main-loop command that opens the ADVANCED face-generation (custom head)
// screen for a merc. It mirrors CICFaceGen (iFaceGen.h): the caller (the basic FaceGen screen's
// "customhead" button) threads the same merc + chosen nationality/side/difficulty in; Exec() builds a
// CAdvFaceGenMenuInterface and pushes it. The advanced editor drives the live head morph through the
// SESSION-32 IShowUnit chain (CInteractiveUnitView -> CShowRPGUnit -> CFakeRPGUnit -> CHeadTransformInfo).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CICAdvFaceGen: public NMainLoop::CInterfaceCommand
{
	OBJECT_BASIC_METHODS(CICAdvFaceGen);
private:
	CPtr<NRPG::CUnit> pMerc;
	CDBPtr<NDb::CSide> pSide;
	CDBPtr<NDb::CNationality> pNationality;
	CDBPtr<NDb::CDBDifficulty> pDifficulty;

public:
	CICAdvFaceGen() {}
	CICAdvFaceGen( NDb::CSide *pSide, NDb::CNationality *pNationality, NDb::CDBDifficulty *pDifficulty, NRPG::CUnit *pMerc );

	virtual void Exec();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
