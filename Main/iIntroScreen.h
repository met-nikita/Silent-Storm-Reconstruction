#ifndef __iIntroScreen_H_
#define __iIntroScreen_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
class IMission;
// Queue the .seq cut-scene player (the file-local NGame::CICPlaySequence in iIntroScreen.cpp) onto the
// main loop. Entry point for callers outside iIntroScreen.cpp: the boot intro and PlayVideo. bExclusive
// replaces the interface stack (boot intro); false pushes the sequence on top (in-mission PlayVideo).
void PlayVideoSequence( IMission *pMission, int nEventID, const string &szFileName, bool bExclusive );
} // namespace NGame
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __iIntroScreen_H_
