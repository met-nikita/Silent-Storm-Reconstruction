#ifndef __A5_DESKTOPWINDOW_H__
#define __A5_DESKTOPWINDOW_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
	class CUICmdExec;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAckEvent
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckEvent: public CObjectBase
{
	OBJECT_NOCOPY_METHODS(CAckEvent);
private:
	ZDATA
	// retail NUI::CAckEvent layout (ctor @0x1d0e00): the bReady handshake gates the DEFERRED
	// voice+lipsync. Retail drives the HUD portrait's mouth/gesture from the shared CHeadsController
	// (like the dialog UI), NOT from a GetAnimation(0/1) clip (records 0/1 are CUT content in the
	// retail game.db), so the voice must not start until the speaker's face is on screen.
	bool bComplete;
	STime sEndTime;
	CPtr<NWorld::CAckEvent> pEvent;
	bool bReady;
	// retail serialize @0x1d1130 tag order: 2=bComplete, 3=sEndTime, 4=pEvent, 5=bReady
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&bComplete); f.Add(3,&sEndTime); f.Add(4,&pEvent); f.Add(5,&bReady); return 0; }

protected:
	void SetEndTime( const STime &sEndTime );

public:
	CAckEvent(): bComplete( false ), sEndTime( 0 ), bReady( false ) {}
	// retail ctor @0x1d0e00 seeds pEvent (the world-side ack) with bComplete/bReady=false, sEndTime=0.
	CAckEvent( NWorld::CAckEvent *_pEvent ): bComplete( false ), sEndTime( 0 ), pEvent( _pEvent ), bReady( false ) {}

	// retail Set @0x1d0cf0 -- flips bReady (arms the voice/lipsync) and sets the 3000ms TTL floor.
	void Set( const STime &sTime );
	virtual void Cancel();

	// retail @0x1d0d20 / @0x1d0dc0 / @0x1d0d40
	bool IsTTLComplete( const STime &sTime ) const { return bReady && ( sEndTime <= sTime ); }
	bool IsComplete( const STime &sTime ) const { return bReady && ( sEndTime <= sTime ) && bComplete; }
	void SetComplete( bool b ) { bComplete = b; }
	bool IsReady() const { return bReady; }

	const STime& GetEndTime() const;
	NWorld::CAckEvent* GetAckEvent() const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDesktopWindow
////////////////////////////////////////////////////////////////////////////////////////////////////
class CDesktopWindow: public CWindow
{
	OBJECT_NOCOPY_METHODS(CDesktopWindow);
private:
	ZDATA_(CWindow)
	CObj<CWindow> pClientWindow;
	////
	CObj<CAckEvent> pActiveEvent;
	CPtr<NWorld::CAckEvent> pNextAckEvent;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pClientWindow); f.Add(3,&pActiveEvent); f.Add(4,&pNextAckEvent); return 0; }

protected:
	virtual CAckEvent* PlayAckEvent( const STime &sTime, NWorld::CAckEvent *pEvent ) { return 0; }

public:
	CDesktopWindow() {}
	CDesktopWindow( const SWindowInfo &sInfo );

	CWindow* GetClientWindow() const;

	virtual void ShowDesktop() {}
	virtual void HideDesktop() {}
	virtual void UpdateDesktop( const STime &sTime ) {}

	virtual void PlayAck( NWorld::CAckEvent *pEvent );
	virtual NGame::CUICmdExec* CreateExecutor( NWorld::CUICmd *pCmd ) { return 0; }

	virtual bool ProcessEvent( const NInput::SEvent &sEvent );
	virtual bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
