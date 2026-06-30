#ifndef __DATAINTERFACE_H_
#define __DATAINTERFACE_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\ADOImport\BasicDB.h"
#include "..\Misc\Geom.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTexture;
class CTRndModel;
class CString;
class CSound;
class CScript;
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EUIControl
{
	UI_TEXT = 1,
	UI_IMAGE,
	UI_MODEL,
	UI_BUTTON,
	UI_CHECKBUTTON,
	UI_RADIOBUTTON,
	UI_SCROLL,
	UI_EDIT,
	UI_IMAGELIST,
	UI_MESSAGEBOX,
	UI_CONTAINER,
	UI_GROUP,
	UI_WINDOW,
	UI_PUSHBUTTON,
	UI_PROGRESSBAR,
	UI_SLIDER,
	UI_COMBOBOX
};
const int N_CTRL_TEXTURES = 6;
const int N_CTRL_MODELS   = 5;
const int N_CTRL_SOUNDS   = 5;
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EUIMode
{
	UIM_800x600 = 0,
	UIM_1024x768,
	UIM_1280x1024,
	UIM_1600x1200,
	UIM_MAX,
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Anchor corner for a control's tooltip (release-added). UI_TOOLTIP positions
// relative to this corner of the control rect; UIA_NONE = no explicit anchor.
enum EUIAnchor
{
	UIA_NONE = 0,
	UIA_LEFTTOP,
	UIA_RIGHTTOP,
	UIA_LEFTBOTTOM,
	UIA_RIGHTBOTTOM,
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUITexture: public CDBRecord
{
	OBJECT_BASIC_METHODS(CUITexture);
public:
	int nWidth;
	int nHeight;
	CPtr<CTexture> pTextures[UIM_MAX];

	virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUIContainer;
class CUIControl: public CDBRecord
{
	OBJECT_BASIC_METHODS(CUIControl);
public:
	EUIControl  type;
	string szID;
	int nColor;
	int nDepth;
	bool bVisible;
	bool bDefault;
	bool bTopmost;
	bool bBottommost;
	bool bTransparent;
	CTRect<int> rect;
	CPtr<CSound> pSounds[N_CTRL_SOUNDS];
	CPtr<CUITexture> pTextures[N_CTRL_TEXTURES];
	CPtr<CTRndModel> pModels[N_CTRL_MODELS];
	CPtr<CString> pString;
	CPtr<CString> pToolTip;
	CPtr<CUIContainer> pContainer;	// parent
	CPtr<CUIContainer> pNestedUIContainer;
	EUIAnchor toolTipAnchorType;
	CTPoint<int> sToolTipAnchor;

	virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUIContainer: public CDBRecord
{
	OBJECT_BASIC_METHODS(CUIContainer);
public:
	int nWidth;
	int nHeight;
	vector< CPtr<CUIControl> > controls;
	CPtr<CScript> pScript;			// release: build-script that populates `controls`

	virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __DATAINTERFACE_H_