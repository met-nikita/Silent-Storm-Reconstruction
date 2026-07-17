#ifndef __GBINKPLAYER_H__
#define __GBINKPLAYER_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  GBinkPlayer  --  the RAD Bink movie player.  Reconstructed from the release
//  module .\release\GBinkPlayer.obj (absent from this predecessor tree).
//
//    NGScene::IVideoPlayer      -- the abstract video-player interface.  It IS a
//                                  dependency-graph value node producing the
//                                  current frame as an NGfx::CTexture (so the 2D
//                                  scene can render it via CreateDynamicRects);
//                                  hence it derives CPtrFuncBase<NGfx::CTexture>,
//                                  exactly like NGScene::CScreenshotTexture.
//    NGScene::CBinkVideoPlayer  -- the concrete Bink-backed implementation.
//    NGScene::CreateVideoPlayer -- the factory the UI control (NUI::CVideoPlayer)
//                                  uses to build a player.
//
//  Vtable order verified against the binary (scan_vtables, 15 slots):
//    0 DestroyContents 1 MakeCopy(base) 2 ~dtor 3 operator& 4 NeedUpdate
//    5 Recalc 6 Play 7 Stop 8 Pause 9 IsPlaying 10 GetCurrentFrame
//    11 SetCurrentFrame 12 GetLength 13 GetNumFrames 14 GetSize.
//  VA = RVA + 0x400000.
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "DG.h"            // CPtrFuncBase, CVersioningBase, OBJECT_NOCOPY_METHODS, ZDATA/ZEND, CObj, IsValid
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGfx { class CTexture; }
struct BINK;                // the RAD Bink movie handle (complete type from bink.h in GBinkPlayer.cpp)
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class IVideoPlayer: public CPtrFuncBase<NGfx::CTexture>
{
public:
	virtual void Play( bool bLoop ) = 0;              // +0x18
	virtual bool Stop() = 0;                          // +0x1c
	virtual bool Pause( bool bPause ) = 0;            // +0x20
	virtual bool IsPlaying() = 0;                     // +0x24
	virtual int  GetCurrentFrame() = 0;              // +0x28
	virtual void SetCurrentFrame( int nFrame ) = 0;  // +0x2c
	virtual int  GetLength() = 0;                    // +0x30  (ms)
	virtual int  GetNumFrames() = 0;                 // +0x34
	virtual void GetSize( CTPoint<int> *pSize ) = 0; // +0x38
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CBinkVideoPlayer: public IVideoPlayer
{
	OBJECT_NOCOPY_METHODS(CBinkVideoPlayer);          // New factory + DestroyContents (no MakeCopy/dtor)
	ZDATA
	BINK         *hBink;          // +0x18  null => not opened
	bool          bForceUpdate;   // +0x1c
	bool          bStopped;       // +0x1d
	DWORD         dwCopyFlags;    // +0x20  -> BinkCopyToBuffer flags
	DWORD         dwPlayFlags;    // +0x24  1=force-nonmult16 2=loop 4=from-memory 8=directsound 0x10=manual-step
	string        szFileName;     // +0x28
	vector<BYTE>  buffer;         // +0x34  preload buffer (from-memory path)
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&bForceUpdate); f.Add(3,&bStopped); f.Add(4,&dwCopyFlags); f.Add(5,&dwPlayFlags); f.Add(6,&szFileName); f.Add(7,&buffer); return 0; }   // retail @0xf86a0 (convergence W2; was 2=szFileName/3=dwPlayFlags)

public:
	CBinkVideoPlayer();                                       // @0xf8180
	CBinkVideoPlayer( const string &szName, DWORD dwFlags );  // @0xf7ed0
	virtual ~CBinkVideoPlayer();                              // closes the movie (Stop)

	bool OpenBink( const char *pszName );                     // @0xf7f90 (non-virtual member)

	virtual bool NeedUpdate();                                // @0xf7e40
	virtual void Recalc();                                    // @0xf8280
	virtual void Play( bool bLoop );                          // @0xf8200
	virtual bool Stop();                                      // @0xf7e90
	virtual bool Pause( bool bPause );                        // @0xf7d80
	virtual bool IsPlaying();                                 // @0xf7db0
	virtual int  GetCurrentFrame();                          // @0xf7d30
	virtual void SetCurrentFrame( int nFrame );              // @0xf7d50
	virtual int  GetLength();                                // @0xf7dd0
	virtual int  GetNumFrames();                             // @0xf7e00
	virtual void GetSize( CTPoint<int> *pSize );             // @0xf7e10
};
////////////////////////////////////////////////////////////////////////////////////////////////////
IVideoPlayer* CreateVideoPlayer( const string &szName, int dwFlags );   // @0xf8110
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __GBINKPLAYER_H__
