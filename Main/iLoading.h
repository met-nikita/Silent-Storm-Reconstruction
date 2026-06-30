#ifndef __A5_ILOADING_H__
#define __A5_ILOADING_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "Interface.h"   // CWindow/SWindowInfo, CImage, CVideoPlayer, CInterface, ICursor, SEvent, NDb::CUITexture
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLoadingUI  --  the loading-screen window (release module iLoading.obj).  A CWindow that owns a
// background splash (CImage) and an animated progress clip (CVideoPlayer whose current frame encodes
// the 0..100 percent done).  Transient: built by NGame::InitLoadingScreen, torn down by
// TermLoadingScreen; never serialized -- the release iLoading.obj carries no class-factory registrar,
// and it derives single-publicly from CObjectBase, so it needs neither REGISTER_SAVELOAD_CLASS nor
// BASIC_REGISTER_CLASS.  OBJECT_BASIC_METHODS only (the copy ctor @0x1f2420 is real; the implicit
// member-wise copy reproduces it: nImageID/nProgress + CPtr/CObj AddRef).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CLoadingUI: public CWindow
{
	OBJECT_BASIC_METHODS(CLoadingUI);
private:
	ZDATA_(CWindow)
	int nImageID;                  // +0x80  splash UITexture record id (-1 = none yet)
	int nProgress;                 // +0x84  0..100
	CPtr<CImage> pBackground;      // +0x88  splash image window
	CObj<CVideoPlayer> pProgress;  // +0x8c  progress clip
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&nImageID); f.Add(3,&nProgress); f.Add(4,&pBackground); f.Add(5,&pProgress); return 0; }

public:
	CLoadingUI() {}
	CLoadingUI( const SWindowInfo &sInfo );

	void SetImage( NDb::CUITexture *pTexture );
	void SetProgress( int nValue );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace NUI
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// iLoading.obj module-level loading-screen lifecycle (free fns over three file-scope UI globals).
////////////////////////////////////////////////////////////////////////////////////////////////////
void InitLoadingScreen();                           // build cursor + interface + loading window, show splash
void TermLoadingScreen();                           // release the three globals
void SetLoadingImage( NDb::CUITexture *pTexture );  // point the splash at a texture (default 0x373 fallback)
void ShowLoadingScreen( int nProgress );            // ~50ms-throttled progress step + interface draw + flip
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace NGame
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
