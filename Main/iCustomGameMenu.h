#ifndef __ICUSTOMGAMEMENU_H_
#define __ICUSTOMGAMEMENU_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
// iCustomGameMenu -- the "custom game" (mods browser) front-end screen. Reconstructed from
// .\release\iCustomGameMenu.obj (Game.exe). Only the queued main-loop command CICCustomGameMenu is
// exposed here (the precedent is iSaveLoad.h, which exports only CICSaveLoadMenu); the screen's UI
// classes (NUI::CCustomGameItem / CCustomGameView / CCustomGameUI) and the modal interface
// (NGame::CCustomGameMenuInterface) live entirely inside iCustomGameMenu.cpp.
//
// CICCustomGameMenu::Exec builds a fresh CCustomGameMenuInterface, runs its arg-less Initialize(),
// and pushes it onto the front-end interface stack (PushInterface) -- exactly the CICSaveLoadMenu
// shape, minus the screenshot/eType args (the custom-game screen takes none). Nothing in the dev
// tree issues this command yet, so the whole module is a behaviour-neutral parity surface.
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "iMain.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICCustomGameMenu @0x1ced40 -- open the custom-game (mods) setup screen.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CICCustomGameMenu: public NMainLoop::CInterfaceCommand
{
	OBJECT_BASIC_METHODS(CICCustomGameMenu);
public:
	CICCustomGameMenu() {}

	virtual void Exec();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
