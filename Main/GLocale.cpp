#include "StdAfx.h"
#include "DG.h"
#include "FontFormat.h"
#include "GView.h"
#include "GLocale.h"
#include "GFont.h"
#include "GTexture.h"
#include "..\Misc\BasicShare.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataMap.h"
#include "..\Misc\StrProc.h"
#include "..\MiscDll\LogStream.h"   // csSystem (font-load diagnostics)
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
externA5 CBasicShare<int, CFileFont> shareFonts;
externA5 CBasicShare<STextureKey, CFileTexture, STextureKeyHash> shareTextures;
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SFontInfo
{
	int nSize;
	int nSizeIndex;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTextLocaleInfo
////////////////////////////////////////////////////////////////////////////////////////////////////
CTextLocaleInfo::CTextLocaleInfo()
{
	CDBTable<NDb::CTypeface> *pFontTable = NDatabase::GetTable<NDb::CTypeface>();
	CDBIterator<NDb::CTypeface> iTempFont( *pFontTable );
	int nRecords = 0, nLoaded = 0;
	while( iTempFont.MoveNext() )
	{
		++nRecords;
		CPtr<NDb::CTypeface> pType = iTempFont.Get();

		CDGPtr< CPtrFuncBase<CFontFormatInfo> > pFormatInfo ( shareFonts.Get( pType->GetRecordID() ) );
		pFormatInfo.Refresh();
		const CFontFormatInfo *pInfo = pFormatInfo->GetValue();
		if ( !pInfo )
		{
			continue;
		}

		++nLoaded;
		fonts.push_back( new CFontInfo( SFont( pInfo->GetHeight(), pType->szName ), shareTextures.Get( pType->pTexture->GetRecordID() ), pFormatInfo ) );
	}
	// nRecords==0 -> db layer (CTypeface table not found / type-id or schema mismatch).
	// nRecords>0 && nLoaded==0 -> res layer (Fonts.res CFileFont format mismatch).
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CTextLocaleInfo::Setup( const CVec2 &_vScreenRect )
{
	if ( vScreenRect == _vScreenRect )
		return;

//	fontCache.clear();
	vScreenRect = _vScreenRect;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CFontInfo* CTextLocaleInfo::SearchFont( const SFont &sFont )
{
	int nSize;
	CPtr<CFontInfo> pResFontInfo;

	for ( vector< CObj<CFontInfo> >::iterator iTemp = fonts.begin(); iTemp != fonts.end(); iTemp++ )
	{
		const SFont &sDBFont = (*iTemp)->GetType();
		if ( sDBFont.szName != sFont.szName )
			continue;

		if ( IsValid( pResFontInfo ) )
		{
			if ( abs( sDBFont.nSize - sFont.nSize ) < abs( nSize - sFont.nSize ) )
			{
				nSize = sDBFont.nSize;
				pResFontInfo = (*iTemp);
			}
		}
		else
		{
			nSize = sDBFont.nSize;
			pResFontInfo = (*iTemp);
		}
	}

	return pResFontInfo;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CFontInfo* CTextLocaleInfo::GetFont( const SFont &sFont )
{
	CPtr<CFontInfo> pResFontInfo = 0;

	pResFontInfo = SearchFont( sFont );
	if ( !pResFontInfo )
		pResFontInfo = SearchFont( SFont( 16, "System" ) );

	return pResFontInfo;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
using namespace NGScene;
REGISTER_SAVELOAD_CLASS( 0x02931162, CTextLocaleInfo )
REGISTER_SAVELOAD_CLASS( 0x020c1140, CFontInfo )
