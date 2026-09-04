#ifndef __IGLOBAL_COMMONUI_H_
#define __IGLOBAL_COMMONUI_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\Misc\2DArray.h"
namespace NDb
{
	class CTAmbientLight;
	class CDBCamera;
	class CSequence;
	class CAnimation;
	class CSound;
	enum ECameraType;
}
namespace NGScene
{
	class IGameView;
	class CCFBTransform;
}
namespace NRender
{
	class IRenderGame;
	class IShowUnit;
}
namespace NGame
{
	class IMission;
}
namespace NWorld
{
	class CUnit;
	class CPlayerItem;
}
namespace NRPG
{
	class CUnit;
	class IInventory;
	class IInventoryItem;
}
#include "iActionDecorator.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const int
	N_STANDART_FLASHTIME = 500,
	N_STANDART_MORPHTIME = N_STANDART_FLASHTIME / 1.5f;
float CalcFlashCoeff( float fCoeff, float fTargetCoeff, const STime &sTime, const STime &sFlashTime, const STime &sMorphTime = N_STANDART_MORPHTIME );
// The per-voice greeting-ack preview sound for a merc (defined in iAdvFaceGen.cpp). Shared so BOTH the basic
// (iFaceGen) and advanced (iAdvFaceGen) face editors can play the chosen voice on a voice-button click.
// Mirrors retail NUI::GetPersAck @0x2452c0 via the dev-native FindVoicePersId/CDBAck-condition-102 derivation.
NDb::CSound* GetPersVoiceAck( NRPG::CUnit *pMerc );
////////////////////////////////////////////////////////////////////////////////////////////////////
// CLineBar
////////////////////////////////////////////////////////////////////////////////////////////////////
class CLineBar: public CImage
{
	OBJECT_BASIC_METHODS(CLineBar)
private:
	ZDATA_(CImage)
	int nBarWidth;
	CPtr<CImage> pBar;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CImage*)this); f.Add(2,&nBarWidth); f.Add(3,&pBar); return 0; }

public:
	CLineBar() {}
	CLineBar( const SWindowInfo &sInfo );

	void Set( float fBar );

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CImageNumber
////////////////////////////////////////////////////////////////////////////////////////////////////
class CImageNumber: public CWindow
{
	OBJECT_BASIC_METHODS(CImageNumber)
public:
	enum EType
	{
		TYPE_UNITINFOPANEL
	};

private:
	ZDATA_(CWindow)
	int nValue;
	SPoint sRealSize;
	vector<int> textureIDs;
	NGfx::SPixel8888 sColor;
	list<CObj<CImage> > imagesList;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&nValue); f.Add(3,&sRealSize); f.Add(4,&textureIDs); f.Add(5,&sColor); f.Add(6,&imagesList); return 0; }

public:
	CImageNumber() {}
	CImageNumber( const SWindowInfo &sInfo, EType eType );

	void Set( int nValue );
	void SetColor( const NGfx::SPixel8888 &_sColor );

	const SPoint& GetRealSize() const;

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CShrinkButton
////////////////////////////////////////////////////////////////////////////////////////////////////
class CShrinkButton: public CButton
{
	OBJECT_BASIC_METHODS(CShrinkButton)
public:
	enum EState
	{
		STATE_NORMAL_UP,
		STATE_NORMAL_DOWN,
		STATE_CHECKED_UP,
		STATE_CHECKED_DOWN,
		STATE_DISABLED,

		STATE_MAXVALUE
	};

private:
	ZDATA_(CButton)
	bool bChecked;
	vector<CObj<CWindow> > statesSet;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CButton*)this); f.Add(2,&bChecked); f.Add(3,&statesSet); return 0; }

public:
	CShrinkButton() {}
	CShrinkButton( const SWindowInfo &sInfo );

	bool IsChecked() const;
	void SetChecked( bool bState );

	CImage* AddImageToState( EState eState, NDb::CUITexture *pTexture, const NGfx::SPixel8888 &sColor, const CVec2 &vScale );

	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CComplexButton
////////////////////////////////////////////////////////////////////////////////////////////////////
class CComplexButton: public CShrinkButton
{
	OBJECT_BASIC_METHODS(CComplexButton)
public:
	enum EState
	{
		NORMAL,
		CHECKED,
		UNCHECKED
	};

private:
	ZDATA_(CShrinkButton)
	CObj<CToolTip> pToolTip;
	//// disabled
	CObj<CImage> pDisabled;
	//// normal
	CObj<CImage> pNormalUp;
	CObj<CImage> pNormalUpIcon;
	CObj<CImage> pNormalUpCheck;
	CObj<CImage> pNormalDown;
	CObj<CImage> pNormalDownIcon;
	CObj<CImage> pNormalDownCheck;
	//// checked
	CObj<CImage> pCheckedUp;
	CObj<CImage> pCheckedUpIcon;
	CObj<CImage> pCheckedUpCheck;
	CObj<CImage> pCheckedDown;
	CObj<CImage> pCheckedDownIcon;
	CObj<CImage> pCheckedDownCheck;
public:
	// public so the derived CComplexButtonFlash can serialize this base via f.Add(1,(CComplexButton*)this)
	// (a serializable base needs a public operator&, like its own base CShrinkButton).
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CShrinkButton*)this); f.Add(2,&pToolTip); f.Add(3,&pDisabled); f.Add(4,&pNormalUp); f.Add(5,&pNormalUpIcon); f.Add(6,&pNormalUpCheck); f.Add(7,&pNormalDown); f.Add(8,&pNormalDownIcon); f.Add(9,&pNormalDownCheck); f.Add(10,&pCheckedUp); f.Add(11,&pCheckedUpIcon); f.Add(12,&pCheckedUpCheck); f.Add(13,&pCheckedDown); f.Add(14,&pCheckedDownIcon); f.Add(15,&pCheckedDownCheck); return 0; }

public:
	CComplexButton() {}
	CComplexButton( const SWindowInfo &sInfo, NDb::CUITexture *pUp, NDb::CUITexture *pDown, NDb::CUITexture *pUnchecked, NDb::CUITexture *pChecked );

	CToolTip* GetToolTip() const;

	void Set( NDb::CUITexture *pIcon = 0, NDb::CUITexture *pIconDisabled = 0, EState eState = NORMAL, const string &szID = "" );
	void SetColor( const NGfx::SPixel8888 &sColor );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CComplexButtonFlash -- release store-category button with a one-shot flash overlay.
class CComplexButtonFlash: public CComplexButton
{
	OBJECT_BASIC_METHODS(CComplexButtonFlash)
	ZDATA_(CComplexButton)
	bool bShowFlash = false;
	STime sFlashTime = 0;
	CObj<CImageDraw> pImage;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CComplexButton*)this); f.Add(2,&bShowFlash); f.Add(3,&sFlashTime); f.Add(4,&pImage); return 0; }
	CComplexButtonFlash() {}
	CComplexButtonFlash( const SWindowInfo &sInfo, NDb::CUITexture *pUp, NDb::CUITexture *pDown, NDb::CUITexture *pUnchecked, NDb::CUITexture *pChecked );

	void SetShowFlash( bool bState ) { bShowFlash = bState; }
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHoverButton
////////////////////////////////////////////////////////////////////////////////////////////////////
class CHoverButton: public CButton
{
	OBJECT_BASIC_METHODS(CHoverButton)
public:
	enum
	{
		STATE_NORMAL,
		STATE_HOVER,
		STATE_DISABLED
	};

private:
	ZDATA_(CButton)
	int nStateID;
	bool bForceState;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CButton*)this); f.Add(2,&nStateID); f.Add(3,&bForceState); return 0; }

public:
	CHoverButton() {}
	CHoverButton( const SWindowInfo &sInfo );

	void ForceState( bool bForce, int nStateID );

	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHoverCheckButton -- a CHoverButton with a checked state (release widget, reg 0xB3529130). While
// checked it forces state 3 (the "selected/down" art); OnAction toggles the check. Used e.g. for the
// face-gen voice selector. operator& @0x1cbae0 (base + bChecked@+0xcc); Draw @0x1bdff0; OnAction @0x1be010.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CHoverCheckButton: public CHoverButton
{
	OBJECT_BASIC_METHODS(CHoverCheckButton);
private:
	ZDATA_(CHoverButton)
	bool bChecked;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CHoverButton*)this); f.Add(2,&bChecked); return 0; }

protected:
	void OnAction();

public:
	CHoverCheckButton() {}
	CHoverCheckButton( const SWindowInfo &sInfo );

	void SetChecked( bool bState ) { bChecked = bState; }
	bool IsChecked() const { return bChecked; }

	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHoverFlashButton -- a CHoverButton that overlays a pulsing alpha-blended "flash" image (registered
// 0xB3708180, release iCommonUI.obj). While enabled it morphs an overlay (pImage) toward a target
// coefficient: full while hovered+flash, a triangle-wave pulse while flagged-but-not-hovered, else
// fading out (CalcFlashCoeff). SetShowFlash arms the pulse; used as the "perks" tab button of the
// biography panel to signal an unspent perk point. ctor @0x1c4e60; Draw @0x1be6a0; SetShowFlash
// @0x1bdfe0; operator& @0x1cba30.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CHoverFlashButton: public CHoverButton
{
	OBJECT_BASIC_METHODS(CHoverFlashButton);
private:
	ZDATA_(CHoverButton)
	bool bShowFlash = false;
	float fCoeff = 0;
	STime sMorphTime = 0;
	CObj<CImageDraw> pImage;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CHoverButton*)this); f.Add(2,&bShowFlash); f.Add(3,&fCoeff); f.Add(4,&sMorphTime); f.Add(5,&pImage); return 0; }

public:
	CHoverFlashButton() {}
	CHoverFlashButton( const SWindowInfo &sInfo );

	void SetShowFlash( bool bState ) { bShowFlash = bState; }

	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CButtonsLine -- release-new horizontal row-of-buttons container (iCommonUI.obj). Each button is
// built from a caption (auto-sized to its text) and added as a child; Draw re-flows them, evenly
// distributing the buttons across the line's width. The main menu wraps the line_1/line_2 template
// controls in two CButtonsLine and fills them with AddHoverButton (see iMainMenu.cpp).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CButtonsLine: public CWindow
{
	OBJECT_BASIC_METHODS(CButtonsLine);
private:
	ZDATA_(CWindow)
	bool bUpdated;
	vector<CObj<CButton> > buttonsSet;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&bUpdated); f.Add(3,&buttonsSet); return 0; }

public:
	CButtonsLine() {}
	CButtonsLine( const SWindowInfo &sInfo );

	// nTooltipID is a DB string id for the button's tooltip (-1 = none); the four text states are the
	// NORMAL / HOVER / (spare state 3) / DISABLED captions.
	void AddButton( CButton *pButton, int nTooltipID, const wstring &wsNormal, const wstring &wsHover, const wstring &wsState3, const wstring &wsDisabled );
	CHoverButton* AddHoverButton( const string &szID, int nTooltipID, const wstring &wsNormal, const wstring &wsHover, const wstring &wsDisabled );

	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CFlashButton
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFlashButton: public CButton
{
	OBJECT_BASIC_METHODS(CFlashButton)
private:
	ZDATA_(CButton)
	bool bFlashMode;
	float fCoeff;
	STime sMorphTime;
	CPtr<CImage> pActive;
	CPtr<CImage> pBackground;
	CDBPtr<NDb::CUITexture> pActiveTexture;
	CDBPtr<NDb::CUITexture> pBackgroundTexture;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CButton*)this); f.Add(2,&bFlashMode); f.Add(3,&fCoeff); f.Add(4,&sMorphTime); f.Add(5,&pActive); f.Add(6,&pBackground); f.Add(7,&pActiveTexture); f.Add(8,&pBackgroundTexture); return 0; }

public:
	CFlashButton() {}
	CFlashButton( const SWindowInfo &sInfo, NDb::CUITexture *pBackgroundTexture = 0, NDb::CUITexture *pActiveTexture = 0 );

	bool GetFlashMode() const;
	void SetFlashMode( bool bState );

	bool ProcessMessage( const SEvent &sEvent );
	void Update( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CScrollWindowBase
////////////////////////////////////////////////////////////////////////////////////////////////////
class CScrollWindowBase: public CWindow
{
	OBJECT_BASIC_METHODS(CScrollWindowBase);
private:
	ZDATA_(CWindow)
	CVec2 vValue;
	CObj<CWindow> pClient;
	////
	CPtr<CScroll> pHScroll;
	CPtr<CScroll> pVScroll;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&vValue); f.Add(3,&pClient); f.Add(4,&pHScroll); f.Add(5,&pVScroll); return 0; }

protected:
	void UpdateScrollers();

public:
	CScrollWindowBase() {}
	CScrollWindowBase( const SWindowInfo &sInfo );

	CWindow* GetClient() const;
	void SetClient( CWindow *oClient );

	const CVec2& GetValue() const;
	void SetValue( const CVec2 &_vValue );

	CScroll* GetHScroll() const;
	void SetHScroll( CScroll *pScroll );

	CScroll* GetVScroll() const;
	void SetVScroll( CScroll *pScroll );

	bool ProcessMessage( const SEvent &sEvent );
	void Update( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CScrollWindow
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TYPE>
class CScrollWindow: public CScrollWindowBase
{
	OBJECT_BASIC_METHODS(CScrollWindow);
private:
	ZDATA_(CScrollWindowBase)
	CObj<TYPE> pScrollWindow;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CScrollWindowBase*)this); f.Add(2,&pScrollWindow); return 0; }

public:
	CScrollWindow() {}
	CScrollWindow( const SWindowInfo &sInfo ): CScrollWindowBase( sInfo )
	{
		pScrollWindow = new TYPE( SWindowInfo( this, SPoint( 0, 0 ), GetSize(), GetWindowID(), STYLE_ENABLED | STYLE_VISIBLE ) );
		SetClient( pScrollWindow );
	}

	TYPE* GetClientWindow() const {	return pScrollWindow; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitView
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitView: public CWindow
{
	OBJECT_BASIC_METHODS(CUnitView);
public:
	enum ECameraType
	{
		CAMERA_FACEGEN,
		CAMERA_PORTRAIT
	};

protected:
	// retail NUI::CUnitView layout / operator& @0x1cab00: {1 CWindow, 2 fScale, 3 fFOV, 4 fAngle,
	// 5 sCamera (SHMatrix 0x40), 6 sTimer, 7 p3DView, 8 pInventoryUnit, 9 pRenderGame}. Dev previously
	// split the camera into fYaw/fPitch/fDistance/vAnchor at tags 4-7, which shifted p3DView to tag 9 --
	// so a RETAIL save's p3DView (tag 7) was read into dev's vAnchor and dev's p3DView (tag 9) picked up
	// retail's pRenderGame ref -> CastToUserObject<IGameView> fails -> p3DView null -> CUnitView::Draw AV
	// (the "missing heads" + click-a-party-member crash). Match retail: store the composed camera as the
	// serialized sCamera (recomputed by RecalcCamera from the runtime orbit params below).
	ZDATA_(CWindow)
	float fScale;
	float fFOV;
	float fAngle;              // retail tag 4: unit spin angle (dev CUnitView doesn't spin -> stays 0)
	SHMatrix sCamera;          // retail tag 5 (0x40): the camera transform used by Draw
	CTimeCounter sTimer;
	CObj<NGScene::IGameView> p3DView;
	CPtr<NRender::IShowUnit> pInventoryUnit;
	CPtr<NRender::IRenderGame> pRenderGame;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&fScale); f.Add(3,&fFOV); f.Add(4,&fAngle); f.Add(5,&sCamera); f.Add(6,&sTimer); f.Add(7,&p3DView); f.Add(8,&pInventoryUnit); f.Add(9,&pRenderGame); return 0; }
protected:
	// runtime-only orbit params (NOT serialized -- retail stores the composed sCamera instead); set by
	// SetUnit, folded into sCamera by RecalcCamera().
	float fYaw;
	float fPitch;
	float fDistance;
	CVec3 vAnchor;
	void RecalcCamera();

public:
	CUnitView() {}
	CUnitView( const SWindowInfo &sInfo, NRender::IRenderGame *pRender = 0, float fScale = 1.0f );

	void SetUnit( NRPG::CUnit *pUnit, ECameraType eType = CAMERA_PORTRAIT );
	void SetUnit( NWorld::CUnit *pUnit, ECameraType eType = CAMERA_PORTRAIT );
	// release @0x1c03d0: SetUnit(unit, camera, b1, b2, b3) with b1=bItems, b2=bShowCap, b3=bPlayIdle -- the
	// @0x1c043c..3e push order feeds CreateShowUnit's three bools as (b1, b3, b2) = (bItems, bPlayIdle, bShowCap).
	// Decoded retail call sites: HUD unit face @0x254cc0 (false, true, true); inventory doll (true, true, false);
	// mission-dialog body view (false, true, false). Defaults reproduce the old dev callers' behavior.
	void SetUnit( NWorld::CUnit *pUnit, NDb::CDBCamera *pCamera, bool bItems = true, bool bShowCap = true, bool bPlayIdle = false );
	void SetUnit( NRPG::CUnit *pUnit, NDb::CDBCamera *pCamera );    // release @0x1c0310: global-camera variant FaceGen calls (NRPG::CUnit*)
	void SetRenderGame( NRender::IRenderGame *pRender ) { pRenderGame = pRender; }   // retail @0x1c0490
	void SetLight( NDb::CTAmbientLight *pLight );
	// release @0x1bf030: (lipsync seq, expression seq) -- both forwarded to the shown unit's head
	void SetSequence( NDb::CSequence *pSequence, NDb::CSequence *pExpression = 0 );
	void PlayAnimation( NDb::CAnimation *pAnim, bool bLoop );

	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CInteractiveUnitView
////////////////////////////////////////////////////////////////////////////////////////////////////
class CInteractiveUnitView: public CUnitView
{
	OBJECT_BASIC_METHODS(CInteractiveUnitView);
protected:
	// retail NUI::CInteractiveUnitView (PDB, 252 bytes) derives from CUnitView (224) and adds ONLY
	// {bCapture@0xe0, bButtonDown@0xe1, fAngle@0xe4, sLastPoint@0xe8, sTimer@0xf0, pMouseCapture@0xf8};
	// the 3D view / shown unit / render game live in the CUnitView base. operator& @0x1cb010 =
	// {1 CUnitView, 2 bCapture, 3 bButtonDown, 4 fAngle, 5 sLastPoint(8B), 6 sTimer, 7 pMouseCapture}.
	// (dev previously derived from CWindow with the view members re-declared here at tags 3-9, so a
	// retail save's chunk was misread from tag 3 on -- wire audit CUnitModelShow 1.1.*.) fAngle and
	// sTimer intentionally shadow the CUnitView members: retail's layout has BOTH copies and both go
	// on the wire (base tag 4/6 inside chunk 1, own tag 4/6 here). protected (not private) because
	// CUnitModelShow::CanHandleState (retail @0x1edf60) reads bButtonDown.
	ZDATA_(CUnitView)
	bool bCapture;
	bool bButtonDown;
	float fAngle;
	SPoint sLastPoint;
	CTimeCounter sTimer;
	CObj<CObjectBase> pMouseCapture;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CUnitView*)this); f.Add(2,&bCapture); f.Add(3,&bButtonDown); f.Add(4,&fAngle); f.Add(5,&sLastPoint); f.Add(6,&sTimer); f.Add(7,&pMouseCapture); return 0; }
public:
	CInteractiveUnitView() {}
	CInteractiveUnitView( const SWindowInfo &sInfo, NRender::IRenderGame *pRender = 0 );

	void SetUnit( NRPG::CUnit *pUnit );
	void SetUnit( NWorld::CUnit *pUnit );
	void SetUnit( NRPG::CUnit *pUnit, NDb::CDBCamera *pCamera );   // perspective head view (AdvFaceGen, DataCamera 5014)

	// The wrapped render object (a CShowRPGUnit -> CFakeRPGUnit chain) carries the live head-morph virtuals
	// SetLSHeadParam/CreateLSHeadInfo. The advanced FaceGen editor (CAdvFaceGenUI) drives them through this.
	NRender::IShowUnit *GetShowUnit() const { return pInventoryUnit; }

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CItemModel
////////////////////////////////////////////////////////////////////////////////////////////////////
class CShowItemModel: public CModel
{
	OBJECT_BASIC_METHODS(CShowItemModel)
private:
	ZDATA_(CModel)
	CPtr<NRPG::IInventoryItem> pItem;
	////
	CObj<CToolTip> pToolTip;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CModel*)this); f.Add(2,&pUnit); f.Add(3,&pItem); f.Add(4,&pToolTip); return 0; }
	CPtr<NWorld::CUnit> pUnit;

public:
	CShowItemModel() {}
	CShowItemModel( const SWindowInfo &sInfo );

	NRPG::IInventoryItem* Get() const;
	// retail Set @0x1c0f50: SetScene(pView,true) -- a non-null pView (the slot's shared p3DView)
	// parents the icon mesh into that scene, null keeps an own private view -- then SetModel +
	// SetCameraTransform, then the WHOLE static tooltip; pUnit feeds Draw's familiarity refresh.
	void Set( NGScene::IGameView *pView, NWorld::CUnit *pUnit, NRPG::IInventoryItem *pItem, NDb::ECameraType eCameraType );

	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CItemModel
////////////////////////////////////////////////////////////////////////////////////////////////////
class CItemModel: public CActionDecorator<CShowItemModel>
{
	OBJECT_BASIC_METHODS(CItemModel)
private:
	ZDATA_(TBaseClass)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(TBaseClass*)this); return 0; }

public:
	CItemModel() {}
	CItemModel( const SWindowInfo &sInfo, NGame::IMission *pMission );

	bool CanHandleState( NGame::IState *pState ) const;
	CObjectBase* GetTarget();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSlotInfo -- the drag-state target descriptor a slot publishes through CActionDecorator::GetTarget()
// while the mouse hovers it (the mission state resolves it to decide where a dragged item may drop).
// retail NUI::CSlotInfo (PDB, 24 bytes: nSlot@+0xc, eType@+0x10, pUnit@+0x14), iCommonUI compiland;
// operator& @0x1c7130 = {2 nSlot, 3 eType, 4 pUnit}; saveload id 0xB3915110 (registered below in
// iCommonUI.cpp). Built by CBackPackSlot::GetTarget @0x1ee050 (BACKPACK, unit), CStoreSlot::GetTarget
// @0x241220 (STORAGE), CInfoPanelSlot::GetTarget @0x255880 (SLOT, nSlot = the NDb::ESlot, unit).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSlotInfo: public CObjectBase
{
	OBJECT_BASIC_METHODS(CSlotInfo)
public:
	// retail NUI::CSlotInfo::EPlacement (PDB enum)
	enum EPlacement
	{
		VACUUM   = 0,
		SLOT     = 1,
		STORAGE  = 2,
		BACKPACK = 3
	};

private:
	ZDATA
	int nSlot;                    // for SLOT: the NDb::ESlot of the info-panel slot; else 0
	EPlacement eType;
	CPtr<NWorld::CUnit> pUnit;    // the owning unit (null for STORAGE)
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nSlot); f.Add(3,&eType); f.Add(4,&pUnit); return 0; }

public:
	CSlotInfo(): nSlot( 0 ), eType( VACUUM ) {}
	CSlotInfo( EPlacement _eType, int _nSlot = 0, NWorld::CUnit *_pUnit = 0 ): nSlot( _nSlot ), eType( _eType ), pUnit( _pUnit ) {}

	int GetSlot() const { return nSlot; }
	EPlacement GetPlacement() const { return eType; }
	NWorld::CUnit* GetUnit() const { return pUnit; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CSlot
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSlot: public CActionDecorator<CWindow>
{
protected:
	struct SItem
	{
		ZDATA
		CTPoint<int> sPos;
		CObj<CWindow> pModel;
		CPtr<NRPG::IInventoryItem> pItem;
		ZEND int operator&( CStructureSaver &f ) { f.Add(2,&sPos); f.Add(3,&pModel); f.Add(4,&pItem); return 0; }

		SItem(): sPos( 0, 0 ) {}
	};
	struct SHilight
	{
		ZDATA
		int nID;
		CObj<CImage> pImage;
		ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nID); f.Add(3,&pImage); return 0; }
	};

private:
	ZDATA_(CActionDecorator<CWindow>)
	CPtr<NGame::IMission> pMission;
	////
	int nWidth;
	int nHeight;
	bool bTrackMouse;
	bool bAlwaysHilight;
	SPoint sMousePoint;
	CPtr<CWindow> pSlotView;
	CPtr<CWindow> pHilight;
	vector<SItem> itemsSet;
	NDb::ECameraType eCameraType;
	CArray2D<SHilight> hilights;          // retail +0xb8, ON THE WIRE at tag 12 (Do2DArray)
	CObj<NGScene::IGameView> p3DView;     // retail +0xc8, tag 13 (the slot's shared 3D item view)
public:
	// retail wire = operator& @0x1cb1c0 (own tags 2..13) + CSlot::OnSerialize @0x1c5c90 (the
	// version-gated BASE chunk 1, written LAST -- byte-walk-confirmed against the retail saves):
	//   {2 pMission, 3 nWidth, 4 nHeight, 5 bTrackMouse, 6 bAlwaysHilight, 7 sMousePoint,
	//    8 pSlotView, 9 pHilight, 10 itemsSet, 11 eCameraType, 12 hilights, 13 p3DView,
	//    1 = file version > 0 ? CActionDecorator<CWindow> base {1 CWindow, 2 bMouseEnter, 3 pMission}
	//                         : plain CWindow base, then decorator pMission adopted from own pMission}.
	// The hilight grid IS retail save state: its SHilight.pImage cells reference the CImage children
	// deserialized inside pHilight's window subtree. [dev history: an earlier fix misread the decomp
	// ('\f' = tag 12, 0xd = tag 13) and serialized the 3D view at tag 12 while dropping hilights off
	// the wire, so a retail save's 2D-array header was read into the view ref and tag 13 was orphaned;
	// and the decorator base layer was missing entirely, so the save's chunk 1.1 (decorator) was
	// misparsed as a CWindow table -- wire audit 1.1.* / 1.12 / 1.13 on all four slot classes.]
	ZEND int operator&( CStructureSaver &f )
	{
		f.Add( 2, &pMission );
		f.Add( 3, &nWidth );
		f.Add( 4, &nHeight );
		f.Add( 5, &bTrackMouse );
		f.Add( 6, &bAlwaysHilight );
		f.Add( 7, &sMousePoint );
		f.Add( 8, &pSlotView );
		f.Add( 9, &pHilight );
		f.Add( 10, &itemsSet );
		f.Add( 11, &eCameraType );
		f.Add( 12, &hilights );
		f.Add( 13, &p3DView );
		// retail CSlot::OnSerialize @0x1c5c90 (invoked at the end of operator& @0x1cb1c0):
		if ( f.GetVersion() > 0 )
			f.Add( 1, (CActionDecorator<CWindow>*)this );
		else
		{
			// legacy v0 file: CSlot serialized as a plain CWindow; adopt the slot's own mission
			// as the decorator's mission link (refcounted CPtr assignment, as retail open-codes).
			f.Add( 1, (CWindow*)this );
			CActionDecorator<CWindow>::pMission = pMission;
		}
		return 0;
	}

protected:
	void GetInSlotPos( int nX, int nY, SPoint *pCoords );
	void GetItemInSlotPos( int nX, int nY, const SPoint &sItemSize, SPoint *pCoords );
	bool GetDragItem( NWorld::SItem *pInfo );		// retail @0x1beb80
	NGame::IMission* GetGame();

public:
	CSlot() {}
	CSlot( const SWindowInfo &sInfo, NGame::IMission *pMission, int nWidth, int nHeight, NDb::ECameraType eCameraType, bool bAlwaysHilight );

	virtual void Take( int nX, int nY ) = 0;
	virtual void Place( int nX, int nY, const NWorld::SItem &sItem ) = 0;
	virtual bool CanPlace( int nX, int nY, const NWorld::SItem &sItem, int *nAP = 0 ) = 0;
	virtual void GetItemsList( vector<SItem> *pItemsSet ) = 0;
	// retail slot vtbl+0x44 (base ICF-folds to `return 0`): the owning unit Draw hands to
	// CShowItemModel::Set; CBackPackSlot @0x1edfc0 / CInfoPanelSlot @0x25a770 override it.
	virtual NWorld::CUnit* GetUnit() { return 0; }

	// CActionDecorator pure virtual: a slot handles the item-drag state (retail @0x1be990).
	// GetTarget() stays pure -- each concrete slot builds its own CSlotInfo.
	bool CanHandleState( NGame::IState *pState ) const;

	void SetSize( const SPoint &sSize );
	void SetSlotSize( int nWidth, int nHeight );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
