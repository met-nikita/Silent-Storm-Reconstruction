#ifndef __A5_UI_CURSOR_H__
#define __A5_UI_CURSOR_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "../DBFormat/DataInterface.h"
#include "../DBFormat/DataMisc.h"		// NDb::CUICursor -- retail SCursorInfo carries the UICursors record
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NInput
{
	struct SEvent;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// Cursor
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NUI::SCursorInfo operator& @0xd4570: 2=wsText (string chunk), 3=pCursor (CDBPtr<NDb::CUICursor>).
// The cursor is the UICursors DB RECORD (texture + hotspot center), not a bare UITexture; the dev
// vCenter member did not exist in retail and is dropped (the center lives in the CUICursor record).
struct SCursorInfo
{
	ZDATA
	wstring wsText;
	CDBPtr<NDb::CUICursor> pCursor;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&wsText); f.Add(3,&pCursor); return 0; }

	SCursorInfo( NDb::CUICursor *_pCursor = 0, const wstring &_wsText = L"" ): wsText( _wsText ), pCursor( _pCursor ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class ICursor: public CObjectBase
{
public:
	static ICursor* Create( bool bShowCursor = true, CVec2 vBegPos = CVec2( -1, -1 ) );
	static ICursor* CreateEditorCursor();

	virtual const CVec2& GetPos() const = 0;
	virtual void SetPos( const CVec2 &vPos ) = 0;
	
	virtual const SCursorInfo& GetCursor() const = 0;
	virtual void SetCursor( const SCursorInfo &sInfo ) = 0;

	virtual void Update() = 0;

	virtual void ProcessEvent( const NInput::SEvent &sEvent ) = 0;
	virtual void Draw( const STime &sTime, NGScene::I2DGameView *pView ) = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
