#ifndef __A5_LOSEMENU_H_
#define __A5_LOSEMENU_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
	class CScreenshotTexture;
}
namespace NDb
{
	class CString;
}
#include "iMain.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
class IMission;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CICLoseMenu: public NMainLoop::CInterfaceCommand
{
	OBJECT_BASIC_METHODS(CICLoseMenu);
private:
	int nStringID;                                       // lose-reason DB string id (-1 = no reason, just "Game Over!")
	CObj<NGScene::CScreenshotTexture> pScreenShotTexture;

public:
	CICLoseMenu( int nStringID = -1, NGScene::CScreenshotTexture *pScreenShotTexture = 0 );

	virtual void Exec();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICLeaveZoneMenu -- release-new (iLeaveZone): queues the "leave zone" modal screen (CLeaveZoneMenuInterface).
class CICLeaveZoneMenu: public NMainLoop::CInterfaceCommand
{
	OBJECT_BASIC_METHODS(CICLeaveZoneMenu);
private:
	CPtr<IMission> pMission;
	CDBPtr<NDb::CString> pString;
	bool bAutoSave;
	CObj<NGScene::CScreenshotTexture> pScreenShotTexture;

public:
	CICLeaveZoneMenu() {}
	CICLeaveZoneMenu( IMission *pMission, NDb::CString *pString, bool bAutoSave, NGScene::CScreenshotTexture *pScreenShotTexture = 0 );

	virtual void Exec();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
