#ifndef __INTERFACE_TOPPANEL_H_
#define __INTERFACE_TOPPANEL_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTopBar
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTopBar: public CWindow
{
	OBJECT_BASIC_METHODS(CTopBar)
private:
	enum EMode
	{
		NONE,
		REALTIME,
		ALLY_TURN,
		ENEMY_TURN,
		PLAYER_TURN
	};
	ZDATA_(CWindow)
	CPtr<NGame::IMission> pMission;
	////
	EMode eMode;
	STime sModeTime;
	CPtr<CText> pText;
	CPtr<CButton> pEndMission;
	CPtr<CWindow> pFlash;
	CPtr<CWindow> pEnemyTurn;
	CPtr<CWindow> pPlayerTurn;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pMission); f.Add(3,&bCanLeaveZone); f.Add(4,&eMode); f.Add(5,&pText); f.Add(6,&pFlash); f.Add(7,&pAllyTurn); f.Add(8,&pEnemyTurn); f.Add(9,&pPlayerTurn); f.Add(10,&pEndMission); f.Add(11,&pObjectives); return 0; }
	bool bCanLeaveZone = false;
	STime sUpdateTime = 0;	// last CanLeaveZone/tooltip refresh (retail +0x84; TRANSIENT like retail)
	CPtr<CWindow> pAllyTurn;
	CPtr<CPushButton> pObjectives;

public:
	CTopBar() {}
	CTopBar( const SWindowInfo &sInfo, NGame::IMission *pMission );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
