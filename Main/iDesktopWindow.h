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
	STime sEndTime;
	CPtr<NWorld::CAckEvent> pEvent;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&sEndTime); f.Add(3,&pEvent); return 0; }

protected:
	void SetEndTime( const STime &sEndTime );

public:
	virtual void Set( const STime &sTime, NWorld::CAckEvent *pEvent );
	virtual void Cancel();

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
