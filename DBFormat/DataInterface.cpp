#include "StdAfx.h"
#include "DataFormat.h"
#include "DataInterface.h"
#include "DataSound.h"
#include "DataRPG.h"		// NDb::CScript (complete type for CPtr<CScript> serialize)

namespace NDb
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUIControl
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIControl::Import()
{
	ASSERT( 6 == N_CTRL_TEXTURES );
	ASSERT( 5 == N_CTRL_MODELS );
  NDatabase::ImportField( "Type", reinterpret_cast<int*>( &type ) );
	NDatabase::ImportField( "IDText", &szID );
	NDatabase::ImportField( "Depth", &nDepth );
	NDatabase::ImportField( "Color", &nColor );
	NDatabase::ImportField( "Left", &rect.left );
	NDatabase::ImportField( "Top", &rect.top );
	NDatabase::ImportField( "Right", &rect.right );
	NDatabase::ImportField( "Bottom", &rect.bottom );
	NDatabase::ImportField( "Sound0", &pSounds[0] );
	NDatabase::ImportField( "Sound1", &pSounds[1] );
	NDatabase::ImportField( "Sound2", &pSounds[2] );
	NDatabase::ImportField( "Sound3", &pSounds[3] );
	NDatabase::ImportField( "Sound4", &pSounds[4] );
	NDatabase::ImportField( "Model0", &pModels[0] );
	NDatabase::ImportField( "Model1", &pModels[1] );
	NDatabase::ImportField( "Model2", &pModels[2] );
	NDatabase::ImportField( "Model3", &pModels[3] );
	NDatabase::ImportField( "Model4", &pModels[4] );
	NDatabase::ImportField( "Texture0", &pTextures[0] );
	NDatabase::ImportField( "Texture1", &pTextures[1] );
	NDatabase::ImportField( "Texture2", &pTextures[2] );
	NDatabase::ImportField( "Texture3", &pTextures[3] );
	NDatabase::ImportField( "Texture4", &pTextures[4] );
	NDatabase::ImportField( "Texture5", &pTextures[5] );
	NDatabase::ImportField( "StringID", &pString );
	NDatabase::ImportField( "UIContainerID", &pContainer );
	NDatabase::ImportField( "NestedUIContainerID", &pNestedUIContainer );
	NDatabase::ImportField( "TooltipID", &pToolTip );
	NDatabase::ImportField( "Visible", &bVisible );
	NDatabase::ImportField( "Transparent", &bTransparent );
	NDatabase::ImportField( "Topmost", &bTopmost );
	NDatabase::ImportField( "DefaultWindow", &bDefault );
	NDatabase::ImportField( "Bottommost", &bBottommost );
	if ( IsValid( pContainer ) )
		pContainer->controls.push_back( this );

	// release-added: tooltip anchor corner + pixel offset
	wstring sAnchor;
	NDatabase::ImportField( "ToolTipAnchorType", &sAnchor );
	if ( sAnchor == L"LeftTop" )
		toolTipAnchorType = UIA_LEFTTOP;
	else if ( sAnchor == L"RightTop" )
		toolTipAnchorType = UIA_RIGHTTOP;
	else if ( sAnchor == L"LeftBottom" )
		toolTipAnchorType = UIA_LEFTBOTTOM;
	else if ( sAnchor == L"RightBottom" )
		toolTipAnchorType = UIA_RIGHTBOTTOM;
	else
		toolTipAnchorType = UIA_NONE;
	NDatabase::ImportField( "ToolTipAnchorX", &sToolTipAnchor.x );
	NDatabase::ImportField( "ToolTipAnchorY", &sToolTipAnchor.y );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIControl::operator&( CStructureSaver &f )
{ 
	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &type );
	f.Add( 3, &szID );
	f.Add( 4, &rect );
	f.Add( 5, &pString );
	f.Add( 6, &pContainer );
	f.Add( 7, &pNestedUIContainer );
	f.Add( 8, &pToolTip );
	f.Add( 9, &nDepth );
	for ( int i = 0; i < N_CTRL_SOUNDS; ++i )
		f.Add( 10 + i, &pSounds[i] ); 
	for ( int i = 0; i < N_CTRL_MODELS; ++i )
		f.Add( 20 + i, &pModels[i] ); 
	for ( int i = 0; i < N_CTRL_TEXTURES; ++i )
		f.Add( 30 + i, &pTextures[i] ); 
	f.Add( 40, &bVisible );
	f.Add( 41, &bTransparent );
	f.Add( 42, &bTopmost );
	f.Add( 43, &nColor );
	f.Add( 44, &bDefault );
	f.Add( 45, &bBottommost );
	f.Add( 46, &toolTipAnchorType );
	f.Add( 47, &sToolTipAnchor );

	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUIContainer
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIContainer::Import()
{
	NDatabase::ImportField( "Width", &nWidth );
	NDatabase::ImportField( "Height", &nHeight );
	NDatabase::ImportField( "ScriptID", &pScript );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIContainer::operator&( CStructureSaver &f )
{ 
	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &nWidth );
	f.Add( 3, &nHeight );
	f.Add( 4, &controls );
	f.Add( 5, &pScript );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUITexture
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUITexture::Import()
{
	NDatabase::ImportField( "R_800x600",   &pTextures[UIM_800x600] );
	NDatabase::ImportField( "R_1024x768",  &pTextures[UIM_1024x768] );
	NDatabase::ImportField( "R_1280x960",  &pTextures[UIM_1280x1024] );
	NDatabase::ImportField( "R_1600x1200", &pTextures[UIM_1600x1200] );

	nWidth = 0;
	nHeight = 0;
	if ( pTextures[NDb::UIM_1024x768] )
	{
		nWidth = pTextures[NDb::UIM_1024x768]->nWidth;
		nHeight = pTextures[NDb::UIM_1024x768]->nHeight;
	}
	else
	{
		if ( pTextures[NDb::UIM_1600x1200] )
		{
			nWidth = pTextures[NDb::UIM_1600x1200]->nWidth * 1024 / 1600;
			nHeight = pTextures[NDb::UIM_1600x1200]->nHeight * 768 / 1200;
		}
		else if ( pTextures[NDb::UIM_1280x1024] )
		{
			nWidth = pTextures[NDb::UIM_1280x1024]->nWidth * 1024 / 1280;
			nHeight = pTextures[NDb::UIM_1280x1024]->nHeight * 768 / 1024;
		}
		else if ( pTextures[NDb::UIM_800x600] )
		{
			nWidth = pTextures[NDb::UIM_800x600]->nWidth * 1024 / 800;
			nHeight = pTextures[NDb::UIM_800x600]->nHeight * 768 / 600;
		}
		else
		{
		//	ASSERT( 0 );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUITexture::operator&( CStructureSaver &f )
{
	f.Add( 1, (CDBRecord*)this );
	f.Add( 2, &nWidth );
	f.Add( 3, &nHeight );
	for ( int i = 0; i < UIM_MAX; ++i )
		f.Add( 10 + i, &pTextures[i] ); 
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NDb;
REGISTER_SAVELOAD_CLASS( 0x002a1170, CUIControl );
REGISTER_SAVELOAD_CLASS( 0x002a1171, CUIContainer );
