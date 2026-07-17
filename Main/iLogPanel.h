#ifndef __INTERFACE_LOGPANEL_H_
#define __INTERFACE_LOGPANEL_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\MiscDll\LogStream.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLogPanel  (retail model @0x1f2620..@0x1f3170)
//
// The scrolling combat/event log. Retail keeps a FIXED ring of 10 STextLine slots (linesSet): slot 0
// is a permanent sentinel/head (bUsed forced true, never scanned); slots 1..9 hold the visible lines.
// The ring is a circular doubly-linked list threaded by ARRAY INDEX (nPrev toward newest, nNext toward
// oldest). Lines arrive push-style through a CLogPanelNotify installed on the log stream (SetLogNotify)
// -- not pulled from consoleLines each frame as the Jan03 design did.
////////////////////////////////////////////////////////////////////////////////////////////////////
struct STextLine
{
	ZDATA
	bool bUsed;
	int nPrev;
	int nNext;
	STime sTime;
	CObj<CTextDraw> pText;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&bUsed); f.Add(3,&nPrev); f.Add(4,&nNext); f.Add(5,&sTime); f.Add(6,&pText); return 0; }

	STextLine(): bUsed( false ), nPrev( 0 ), nNext( 0 ), sTime( 0 ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CLogPanel;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLogPanelNotify @0x1f2cc0 -- the per-panel log-stream sink: forwards every console line of the
// panel's own stream type into that panel's AddLine.
class CLogPanelNotify: public ILogNotify
{
	OBJECT_BASIC_METHODS( CLogPanelNotify );
	ZDATA
	EStreamType eType;			// +0x0c the one stream this panel shows
	CPtr<CLogPanel> pPanel;		// +0x10 the panel to feed (weak back-pointer)
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&eType); f.Add(3,&pPanel); return 0; }
public:
	CLogPanelNotify(): eType( STREAM_SYSTEM ) {}
	CLogPanelNotify( EStreamType _eType, CLogPanel *_pPanel ): eType( _eType ), pPanel( _pPanel ) {}
	//
	virtual void OnAddConsoleLine( const SConsoleLine &line );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CLogPanel: public CWindow
{
	OBJECT_BASIC_METHODS(CLogPanel)
private:
	ZDATA_(CWindow)
	vector<STextLine> linesSet;		// fixed 10-slot index-linked ring (slot 0 = sentinel head)
	CObj<CLogPanelNotify> pNotify;	// the stream sink that feeds AddLine
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(6,&linesSet); f.Add(7,&pNotify); return 0; }

private:
	// @0x1f2710 -- link slot nIndex at the head of the ring, mark it used, reset its timer and set text.
	void AddToHead( int nIndex, const wstring &wsText );

public:
	CLogPanel() {}
	CLogPanel( const SWindowInfo &sInfo, EStreamType eType );

	// @0x1f27c0 -- append one line: claim a free slot (or evict the oldest) and thread it at the head.
	void AddLine( const wstring &wsText );
	// @0x1f25e0 -- unlink slot nIndex from the ring and free it.
	void Remove( int nIndex );

	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
