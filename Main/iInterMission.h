#ifndef __IINTERMISSION_H_
#define __IINTERMISSION_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "Gfx.h"
#include "iMain.h"
#include "..\Misc\2DArray.h"
namespace NDb
{
	class CUITexture;
	class CUIContainer;
}
namespace NRPG
{
	class CGlobalGame;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CICInterMission: public NMainLoop::CInterfaceCommand
{
	OBJECT_BASIC_METHODS(CICInterMission);
private:
	string szConfig;
	wstring wsMessage;

public:
	CICInterMission() {}
	CICInterMission( const string &szConfig, const wstring &wsMessage = L"" );
	virtual void Exec();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CICInterMissionWait: public NMainLoop::CInterfaceCommand
{
	OBJECT_BASIC_METHODS(CICInterMissionWait);
private:
	CDBPtr<NDb::CUITexture> pBackground;
	CDBPtr<NDb::CUIContainer> pInterface;
	////
	bool bScreenShot;
	CArray2D<NGfx::SPixel8888> sScreenShot;

public:
	CICInterMissionWait(): bScreenShot( false ) { ASSERT( 0 ); }
	CICInterMissionWait( NDb::CUITexture *pBackground );
	CICInterMissionWait( NDb::CUIContainer *pInterface, const CArray2D<NGfx::SPixel8888> &sScreenShot );

	virtual void Exec();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif