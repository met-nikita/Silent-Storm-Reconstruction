#ifndef __A5_MISSIONMOVIE_UI_H__
#define __A5_MISSIONMOVIE_UI_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "iDesktopWindow.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMissionMovieUI
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMissionMovieUI: public CDesktopWindow
{
	OBJECT_NOCOPY_METHODS(CMissionMovieUI);
private:
	NInput::CBind bindCancel;

	enum EStage
	{
		START,
		FADEIN,
		SHOWSCRIPT,
		////
		FINISH,
		FADEOUT
	};
	ZDATA_(CDesktopWindow)
	CPtr<CDesktopWindow> pTransition;
	CPtr<NGame::IMission> pMission;
	////
	int nPanelsStateSave;
	////
	STime sStageTime;
	EStage eStage;
	////
	CPtr<CImage> pTopBackground;
	CPtr<CImage> pBottomBackground;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDesktopWindow*)this); f.Add(2,&bSkipFadeOut); f.Add(3,&pTransition); f.Add(4,&pMission); f.Add(5,&nPanelsStateSave); f.Add(6,&nNotifyID); f.Add(7,&sStageTime); f.Add(8,&eStage); f.Add(9,&pTopBackground); f.Add(10,&pBottomBackground); f.Add(11,&bSkipPart); return 0; }
	bool bSkipFadeOut = false;
	int nNotifyID = 0;
	bool bSkipPart = false;

protected:
	CAckEvent* PlayAckEvent( const STime &sTime, NWorld::CAckEvent *pEvent );

public:
	CMissionMovieUI();
	CMissionMovieUI( const SWindowInfo &sInfo, NGame::IMission *pMission, CDesktopWindow *pTransition );

	void ShowDesktop();
	void HideDesktop();
	void UpdateDesktop( const STime &sTime );
	NGame::CUICmdExec* CreateExecutor( NWorld::CUICmd *pCmd );

	bool ProcessEvent( const NInput::SEvent &sEvent );
	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMissionFadeUI -- release-new: the screen fade/transition desktop window (FadeIn/FadeOut script ops).
// A START/FADEIN/DUMMY/FINISH/FADEOUT state machine that ramps a full-window colour overlay's alpha in
// (on a BeginFade), holds it (DUMMY), then ramps it back out (on an EndFade) and pops itself. Modelled on
// CMissionMovieUI; bodies in iMissionMovieUI.cpp.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMissionFadeUI: public CDesktopWindow
{
	OBJECT_NOCOPY_METHODS(CMissionFadeUI);
private:
	enum EStage
	{
		START,
		FADEIN,
		DUMMY,
		////
		FINISH,
		FADEOUT
	};
	ZDATA_(CDesktopWindow)
	CVec3 vColor;					// the fade target colour (rgb, 0..1)
	STime sFadeTime;				// the fade duration (ms)
	CPtr<CDesktopWindow> pTransition;	// the desktop being faded over (hidden while fully tinted)
	CPtr<NGame::IMission> pMission;
	STime sStageTime;
	EStage eStage;
	CPtr<CImage> pFade;				// the full-window colour overlay
	int nNotifyID;					// echoed back via CCmdInterfaceEvent so lua WaitForUI(id) unblocks
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDesktopWindow*)this); f.Add(2,&vColor); f.Add(3,&sFadeTime); f.Add(4,&pTransition); f.Add(5,&pMission); f.Add(6,&sStageTime); f.Add(7,&eStage); f.Add(8,&pFade); f.Add(9,&nNotifyID); return 0; }

public:
	CMissionFadeUI();
	CMissionFadeUI( const SWindowInfo &sInfo, NGame::IMission *pMission, CDesktopWindow *pTransition, const CVec3 &vColor, STime sFadeTime );

	void ShowDesktop( int nNotifyID );
	void HideDesktop( int nNotifyID );
	void UpdateDesktop( const STime &sTime );

	// While the fade screen is the top desktop (e.g. a scripted CameraSet/CameraMove between FadeOut and
	// FadeIn), world UI commands are dispatched through it -- so it must build executors too, else the
	// command is consumed with no executor and its WaitForUI id is never released (CameraSet hangs).
	NGame::CUICmdExec* CreateExecutor( NWorld::CUICmd *pCmd );

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
