#include "StdAfx.h"
#include "Gfx.h"
#include "GSceneUtils.h"
#include "G2DView.h"
#include "..\Input\Bind.h"
#include "..\Game\WinFrame.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
#include "Interface.h"
#include "UIWrap.h"
#include "UIML.h"     // BUG 8: NUI::IML / CreateML -- retail draws the cursor caption through the CML engine (outline + pt-size)
#include "..\MiscDll\Commands.h"      // REGISTER_VAR_EX (ui_hwcursor)
#include "..\FileIO\BasicChunk1.h"    // START_REGISTER / FINISH_REGISTER
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGfx
{
	HWND GetHWND();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const int N_TRANSITION_TIME	= 250;
////////////////////////////////////////////////////////////////////////////////////////////////////
static CVec2 vCursorPos = CVec2( 0, 0 );
static bool bHWCursor = false;
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retail v1.2 CHWCursor::Reload 0x57f950 loads the native .cur/.ani asset,
// not a hardware copy of the software texture. Handles are transient resources.
class CNativeCursorCache
{
	unordered_map<string, HCURSOR> cursors;
public:
	~CNativeCursorCache()
	{
		NWinFrame::SetCursor( 0 );
		for ( unordered_map<string, HCURSOR>::iterator i = cursors.begin(); i != cursors.end(); ++i )
			DestroyCursor( i->second );
	}
	HCURSOR Get( NDb::CUICursor *pCursor )
	{
		if ( !IsValid(pCursor) || pCursor->szFileName.empty() )
			return 0;
		unordered_map<string, HCURSOR>::iterator i = cursors.find( pCursor->szFileName );
		if ( i != cursors.end() )
			return i->second;
		string path = string(".\\res\\Cursors\\") + pCursor->szFileName;
		HCURSOR hNative = LoadCursorFromFileA( path.c_str() );
		if ( hNative )
			cursors[pCursor->szFileName] = hNative;
		return hNative;
	}
};
static CNativeCursorCache nativeCursors;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCursor
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCursor: public ICursor
{
	OBJECT_BASIC_METHODS(CCursor);
protected:
	NInput::CBind bindX, bindY;
	// BUG 8: retail draws the cursor ToHit/AP caption through the CML markup engine (IML), NOT the legacy
	// GText CTextDraw -- so it renders the DB-string markup (Courier, 16pt, colour, 1px outline) like retail.
	// The shared CTextDraw stays GText (14 other consumers); only the cursor gets its own IML. Transient.
	CObj<IML> pTextML;
	ZDATA
	int nDisableCount; // retail Enable/Disable nesting counter (gates ProcessEvent @0xd60a0); serialized first
	bool bShow;
	STime sTransitionTime;
	SCursorInfo sInfo;
	SCursorInfo sOldInfo;
	CObj<CTextDraw> pText;
	CObj<CImageDraw> pImage;
	CObj<CImageDraw> pOldImage;
	// retail v1.2 wire @0xd7550: {2 nDisableCount, 3 bShow, 4 sTransitionTime, 5 sInfo, 6 sOldInfo,
	// 7 pText, 8 pImage, 9 pOldImage}. The old Jan03 table put bShow@2 -> it read retail's
	// nDisableCount low byte (0) -> cursor permanently hidden after loading a retail save.
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nDisableCount); f.Add(3,&bShow); f.Add(4,&sTransitionTime); f.Add(5,&sInfo); f.Add(6,&sOldInfo); f.Add(7,&pText); f.Add(8,&pImage); f.Add(9,&pOldImage); return 0; }

public:
	CCursor( bool bShow = true );

	const CVec2& GetPos() const;
	void SetPos( const CVec2 &vCursorPos );
	
	const SCursorInfo& GetCursor() const;
	void SetCursor( const SCursorInfo &sInfo );

	void SetCursorText( const wstring &wsText );

	void Update();

	void ProcessEvent( const NInput::SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
ICursor* ICursor::Create( bool bShowCursor, CVec2 vBegPos )
{
	CCursor *pCursor = new CCursor( bShowCursor );
	if ( ( vBegPos.x > 0 ) && ( vBegPos.y > 0 ) )
		pCursor->SetPos( vBegPos );

	return pCursor;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCursor
////////////////////////////////////////////////////////////////////////////////////////////////////
CCursor::CCursor( bool _bShow ):
	bindX( "cursor_x" ), bindY( "cursor_y" ), nDisableCount(0), bShow(_bShow)
{
	pText = new CTextDraw();
	pTextML = CreateML();   // BUG 8: the cursor's own CML markup text (renders the DB-string font/colour/outline)
	pImage = new CImageDraw();
	pOldImage = new CImageDraw();

}
////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec2& CCursor::GetPos() const
{
	return vCursorPos;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCursor::SetPos( const CVec2 &_vCursorPos )
{
	vCursorPos = _vCursorPos;
	if ( NWinFrame::IsAppActive() )
	{
		POINT point = { Float2Int(vCursorPos.x), Float2Int(vCursorPos.y) };
		if ( ClientToScreen( NGfx::GetHWND(), &point ) )
		{
			ClipCursor( 0 );
			SetCursorPos( point.x, point.y );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const SCursorInfo& CCursor::GetCursor() const
{
	return sInfo;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCursor::SetCursor( const SCursorInfo &_sInfo )
{
	if ( sInfo.pCursor != _sInfo.pCursor )
	{
		sOldInfo = sInfo;
		sTransitionTime = 0;
	}

	sInfo = _sInfo;
	pText->SetText( _sInfo.wsText );
	if ( IsValid( pTextML ) )
		pTextML->SetText( _sInfo.wsText, 0 );   // 0 = process the <font>/<color> tags from the DB strings
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCursor::Update()
{
	// Retail v1.2 0x4d5eb0 uses OS coordinates in both cursor modes. Keeping a
	// separate accelerated position would desynchronize native pointing and clicks.
	if ( !NWinFrame::IsAppActive() )
	{
		ClipCursor( 0 );
		return;
	}
	RECT client;
	POINT origin = { 0, 0 };
	HWND hWnd = NGfx::GetHWND();
	if ( !GetClientRect( hWnd, &client ) || !ClientToScreen( hWnd, &origin ) )
		return;
	RECT clip = { origin.x, origin.y, origin.x + client.right, origin.y + client.bottom };
	if ( bindX.IsActive() )
	{
		POINT point;
		if ( GetCursorPos( &point ) && ScreenToClient( hWnd, &point ) )
		{
			vCursorPos.x = Clamp( float(point.x), 0.0f, Max(0.0f, float(client.right - 1)) );
			vCursorPos.y = Clamp( float(point.y), 0.0f, Max(0.0f, float(client.bottom - 1)) );
		}
	}
	else
	{
		// Camera rotate/zoom bindings temporarily freeze the pointing cursor.
		clip.left = origin.x + Float2Int(vCursorPos.x);
		clip.top = origin.y + Float2Int(vCursorPos.y);
		clip.right = clip.left + 1;
		clip.bottom = clip.top + 1;
		SetCursorPos( clip.left, clip.top );
	}
	ClipCursor( &clip );
	if ( nDisableCount < 1 )
	{
		bindX.GetDelta();
		bindY.GetDelta();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCursor::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( sTransitionTime == 0 )
		sTransitionTime = sTime;
	NWinFrame::ShowCursor( bHWCursor && bShow );
	NWinFrame::SetCursor( bHWCursor ? nativeCursors.Get(sInfo.pCursor) : 0 );

	if ( bShow )
	{
		float fCoeff = float( sTime - sTransitionTime ) / N_TRANSITION_TIME;
		if ( fCoeff > 1.0f )
		{
			fCoeff = 1.0f;
			sOldInfo.pCursor = 0;
		}

		CVec2 vVirtCursorPos( vCursorPos.x * 1024.0f / pView->GetViewportSize().x, vCursorPos.y * 768.0f / pView->GetViewportSize().y );

		// retail draws from the UICursors RECORD: texture = pCursor->pUITexture, anchor = retail
		// CalcCursorPos @0xd6200: sPos = (int)( vVirt - texSize * nCenter ), nCenterX/Y from the record.
		if ( IsValid( sInfo.pCursor ) && IsValid( sInfo.pCursor->pUITexture ) )
		{
			NDb::CUITexture *pTex = sInfo.pCursor->pUITexture;
			SPoint sPos( vVirtCursorPos.x - float( pTex->nWidth ) * float( sInfo.pCursor->nCenterX ), vVirtCursorPos.y - float( pTex->nHeight ) * float( sInfo.pCursor->nCenterY ) );
			pImage->SetWindow( SRect( sPos.x, sPos.y, sPos.x + pTex->nWidth, sPos.y + pTex->nHeight ) );
			pImage->SetImage( pTex );
			pImage->SetColor( NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF * fCoeff ) );
			if ( !bHWCursor )
				pImage->Draw( 0, sTime, pView );

			// BUG 8: draw the caption through the cursor's OWN CML markup engine (retail cursor path), so the
			// DB-string markup renders as retail does -- Courier, 16pt, the DB colour, and the 1px black
			// outline (which GText cannot draw). Scale the virtual (1024x768) text anchor to screen, generate
			// the ML at full width (the short caption never wraps), and Render.
			if ( IsValid( pTextML ) )
			{
				CVec2 vScr = pView->GetViewportSize();
				SPoint sTextVirt( sPos.x + pTex->nWidth, sPos.y );
				CTPoint<float> sScrPos( sTextVirt.x * vScr.x / 1024.0f, sTextVirt.y * vScr.y / 768.0f );
				pTextML->Generate( pView, (int)vScr.x );
				CTRect<float> sScrWindow( sScrPos.x, sScrPos.y, vScr.x, vScr.y );
				pTextML->Render( pView, sScrPos, sScrWindow );
			}
		}
		if ( !bHWCursor && IsValid( sOldInfo.pCursor ) && IsValid( sOldInfo.pCursor->pUITexture ) )
		{
			NDb::CUITexture *pTex = sOldInfo.pCursor->pUITexture;
			SPoint sPos( vVirtCursorPos.x - float( pTex->nWidth ) * float( sOldInfo.pCursor->nCenterX ), vVirtCursorPos.y - float( pTex->nHeight ) * float( sOldInfo.pCursor->nCenterY ) );
			pOldImage->SetWindow( SRect( sPos.x, sPos.y, sPos.x + pTex->nWidth, sPos.y + pTex->nHeight ) );
			pOldImage->SetImage( pTex );
			pOldImage->SetColor( NGfx::SPixel8888( 0xFF, 0xFF, 0xFF, 0xFF * ( 1.0f - fCoeff ) ) );
			pOldImage->Draw( 0, sTime, pView );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCursor::ProcessEvent( const NInput::SEvent &sEvent )
{
	if ( nDisableCount < 1 )
	{
		bindX.ProcessEvent( sEvent );
		bindY.ProcessEvent( sEvent );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class CEditorCursor: public CCursor
{
	OBJECT_BASIC_METHODS(CEditorCursor);
public:
	void ProcessEvent( const NInput::SEvent &eEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorCursor::ProcessEvent( const NInput::SEvent &eEvent )
{
	POINT sPoint;
	GetCursorPos( &sPoint );
	ScreenToClient( NGfx::GetHWND(), &sPoint );

	CVec2 scrSize = NGfx::GetScreenRect();
	vCursorPos.x = sPoint.x;
	vCursorPos.y = sPoint.y;
	vCursorPos.x = Max( vCursorPos.x, 0.0f );
	vCursorPos.x = Min( vCursorPos.x, scrSize.x ); 
	vCursorPos.y = Max( vCursorPos.y, 0.0f );
	vCursorPos.y = Min( vCursorPos.y, scrSize.y );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
ICursor* ICursor::CreateEditorCursor()
{
	return new CEditorCursor;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail Cursor.obj registrar: single var, VarBoolHandler -> bHWCursor @0x98db60, default 0, saved
START_REGISTER(Cursor)
	REGISTER_VAR_EX( "ui_hwcursor", NGlobal::VarBoolHandler, &bHWCursor, 0, true )
FINISH_REGISTER
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0x00821183, CCursor );
REGISTER_SAVELOAD_CLASS( 0xA2812160, CEditorCursor );
////////////////////////////////////////////////////////////////////////////////////////////////////
