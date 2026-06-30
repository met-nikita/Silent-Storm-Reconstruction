#ifndef __A5_UI_CURSOR_H__
#define __A5_UI_CURSOR_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "../DBFormat/DataInterface.h"
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
struct SCursorInfo
{
	ZDATA
	CVec2 vCenter;
	wstring wsText;
	CDBPtr<NDb::CUITexture> pTexture;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&vCenter); f.Add(3,&wsText); f.Add(4,&pTexture); return 0; }

	SCursorInfo( NDb::CUITexture *_pTexture = 0, const wstring &_wsText = L"", const CVec2 &_vCenter = CVec2( 0, 0 ) ): vCenter( _vCenter ), wsText( _wsText ), pTexture( _pTexture ) {}
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
