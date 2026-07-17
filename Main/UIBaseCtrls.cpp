#include "StdAfx.h"
#include "G2DView.h"
#include "Transform.h"
#include "GSceneUtils.h"
#include "RectLayout.h"
#include "GView.h"
#include "G2DView.h"
#include "..\Misc\StrProc.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
#include "Interface.h"
#include "UIWrap.h"
#include "UIBaseCtrls.h"
#include "UIML.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CVALUEHandler -- retail NUI::CVALUEHandler (saveload id 0xB0241956): the <value id = X> markup
// substitutor. Retail holds a CPtr<CText> (op& @0x312f80: tag 2) and its Exec @0x312eb0 INSERTS the
// substituted string into the dispatching IML's parse stream at the caret (so the value text is
// tokenized inline) instead of the old dev AddObject of a wstring text object.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CVALUEHandler: public IMLHandler
{
	OBJECT_BASIC_METHODS(CVALUEHandler);
private:
	ZDATA
	CPtr<CText> pText;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pText); return 0; }

public:
	CVALUEHandler() {}
	CVALUEHandler( CText *_pText ): pText( _pText ) {}

	void Exec( IML *pML, IMLLayout *pLayout, const vector<wstring> &paramsSet )
	{
		if ( paramsSet.size() != 4 )
			return;

		wstring wsVal;
		if ( pText->GetVal( paramsSet[3], &wsVal ) )
			pML->GetStream()->InsertString( wsVal );   // parsed inline from the caret (retail)
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CText -- retail bodies (see UIBaseCtrls.h banner).
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail ctor @0x312b60: CWindow(info); nSize = 0; empty wsText/valuesMap; pText = CreateML();
// pText->SetHandler(L"value", new CVALUEHandler(this)).
CText::CText( const SWindowInfo &sInfo ):
	CWindow( sInfo ), nSize( 0 )
{
	pText = CreateML();
	pText->SetHandler( L"value", new CVALUEHandler( this ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const wstring& CText::GetText() const
{
	return wsText;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail SetText @0x3128d0: self-assign-guarded copy; nSize = 0 (force a layout regenerate); push
// into the IML (retail always passes flag 0 -- bProcessTAGs is accepted for source compatibility).
void CText::SetText( const wstring &_wsText, bool bProcessTAGs )
{
	if ( &_wsText != &wsText )
		wsText = _wsText;
	nSize = 0;

	pText->SetText( wsText, 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CText::GetVal( const wstring &szID, wstring *pVal )
{
	unordered_map<wstring,wstring>::const_iterator iTemp = valuesMap.find( szID );
	if ( iTemp == valuesMap.end() )
		return false;

	*pVal = iTemp->second;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CText::SetVal( const wstring &szID, int nVal )
{
	WCHAR wsBuffer[128];
	swprintf( wsBuffer, L"%d", nVal );
	valuesMap[szID] = wsBuffer;

	SetUpdated();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CText::SetVal( const wstring &szID, float fVal )
{
	WCHAR wsBuffer[256];
	swprintf( wsBuffer, L"%.2f", fVal );
	valuesMap[szID] = wsBuffer;

	SetUpdated();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CText::SetVal( const wstring &szID, const wstring &wsVal )
{
	valuesMap[szID] = wsVal;

	SetUpdated();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IML* CText::GetIML()
{
	return pText;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CText::SetUpdated()
{
	nSize = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail GetRealSize @0x312760: refresh the layout for the live view, then map its measured size
// back into virtual 1024x768 coordinates -- ROUNDED in retail ((x<<10)/vp.x via fistp).
void CText::GetRealSize( SPoint *pRes )
{
	UpdateText( GetInterface()->GetView() );

	*pRes = pText->GetSize();

	CVec2 vScreenRect = GetInterface()->GetView()->GetViewportSize();
	pRes->x = Float2Int( float( pRes->x * 1024 ) / vScreenRect.x );
	pRes->y = Float2Int( float( pRes->y * 768 ) / vScreenRect.y );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CText::ProcessMessage( const SEvent &sEvent )
{
	if ( sEvent.nEvent == EVENT_TEMPLATECREATE )
	{
		if ( sEvent.pControl->pString )
			SetText( sEvent.pControl->pString->szStr );
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail Draw @0x312820: refresh the layout, map to screen (ClientToScreen abort on false), render
// the IML, then chain CWindow::Draw (inside the visible arm).
void CText::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	UpdateText( pView );

	SRect sScrWindow;
	SPoint sScrPosition;
	if ( !ClientToScreen( &sScrPosition, &sScrWindow ) )
		return;
	VirtualToScreen( &sScrPosition, &sScrWindow );

	pText->Render( pView, sScrPosition, sScrWindow );

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail UpdateText @0x312480: the layout width in screen pixels (ROUNDED in retail); regenerate
// only on change.
void CText::UpdateText( NGScene::I2DGameView *pView )
{
	CVec2 vScreenRect = pView->GetViewportSize();
	int nNewSize = Float2Int( float( GetSize().x ) * vScreenRect.x / 1024.0f );
	if ( nNewSize != nSize )
	{
		nSize = nNewSize;
		pText->Generate( pView, nSize );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CImage
////////////////////////////////////////////////////////////////////////////////////////////////////
CImage::CImage( const SWindowInfo &sInfo ):
	CWindow( sInfo )
{
	pImage = new CImageDraw();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CImage::SetScale( const CVec2 &vScale )
{
	pImage->SetScale( vScale );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CImage::SetColor( const NGfx::SPixel8888 &_sColor )
{
	pImage->SetColor( _sColor );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CImage::SetImage( NDb::CUITexture* _pTexture, const SRect &sTexRect )
{
	SRect sRect( sTexRect );
	pImage->SetImage( _pTexture, sRect );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CImage::SetSizeFromImage( NDb::CUITexture* pTexture )
{
	if ( !IsValid( pTexture ) )
		return;

	SetSize( SPoint( pTexture->nWidth, pTexture->nHeight ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CImage::ProcessMessage( const SEvent &sEvent )
{
	if ( sEvent.nEvent == EVENT_TEMPLATECREATE )
	{
		NGfx::SPixel8888 sColor;
		sColor.color = sEvent.pControl->nColor;
		SetColor( sColor );
		SetImage( sEvent.pControl->pTextures[0] );
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CImage::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	pImage->SetWindow( SRect( 0, 0, GetSize().x, GetSize().y ) );
	pImage->Draw( this, sTime, pView );

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CModel
////////////////////////////////////////////////////////////////////////////////////////////////////
CModel::CModel( const SWindowInfo &sInfo ):
	CWindow( sInfo )
{
	pModel = new CModelDraw();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const NGfx::SPixel8888& CModel::GetColor() const
{
	return pModel->GetColor();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CModel::SetColor( const NGfx::SPixel8888 &sColor )
{
	pModel->SetColor( sColor );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CModel* CModel::GetModel() const
{
	return pModel->GetModel();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CModel::SetModel( NDb::CModel *_pModel )
{
	pModel->SetModel( _pModel );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail thin forwarders into the owned CModelDraw @0x312670/0x312680/0x312690.
////////////////////////////////////////////////////////////////////////////////////////////////////
void CModel::SetScene( NGScene::IGameView *pView, bool bFast )
{
	pModel->SetScene( pView, bFast );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CModel::SetModelTransform( const SHMatrix &sMatrix )
{
	pModel->SetModelTransform( sMatrix );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CModel::SetCameraTransform( const SHMatrix &sMatrix )
{
	pModel->SetCameraTransform( sMatrix );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CModel::ProcessMessage( const SEvent &sEvent )
{
	if ( sEvent.nEvent == EVENT_TEMPLATECREATE )
	{
		SRand sRnd;
		if ( sEvent.pControl->pModels[0] )
			SetModel( sEvent.pControl->pModels[0]->CreateModel( &sRnd ) );

		return true;
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CModel::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	pModel->SetWindow( SRect( 0, 0, GetSize().x, GetSize().y ) );
	pModel->Draw( this, sTime, pView );

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB0241952, CText );
REGISTER_SAVELOAD_CLASS( 0xB0241954, CImage );
REGISTER_SAVELOAD_CLASS( 0xB0241955, CModel );
REGISTER_SAVELOAD_CLASS( 0xB0241956, CVALUEHandler );
