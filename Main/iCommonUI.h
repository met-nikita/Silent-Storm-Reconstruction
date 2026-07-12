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
	class IShowUnitHead;
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
// CComplexButtonFlash -- release-new CComplexButton variant with a flash-overlay animation. DEAD in
// this predecessor UI (panels still create CComplexButton); reconstructed registered + serializable so
// release saves whose buttons are CComplexButtonFlash load polymorphically (CObj<CComplexButton> holds
// it via inheritance). operator& byte-exact to decode @0x243db0; flash Draw behavior deferred (inherits
// CComplexButton::Draw -- never instantiated in this tree).
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
// CUnitHead
////////////////////////////////////////////////////////////////////////////////////////////////////
class CUnitHead: public CWindow
{
	OBJECT_BASIC_METHODS(CUnitHead);
protected:
	ZDATA_(CWindow)
	float fScale;
	CObj<NGScene::IGameView> p3DView;
	CPtr<NRender::IRenderGame> pRenderGame;
	CObj<NGScene::CCFBTransform> pTransform;
	CObj<NRender::IShowUnitHead> pHead;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&fScale); f.Add(3,&p3DView); f.Add(4,&pRenderGame); f.Add(5,&pTransform); f.Add(6,&pHead); return 0; }

public:
	CUnitHead() {}
	CUnitHead( const SWindowInfo &sInfo, NRender::IRenderGame *pRender, float fScale = 1.0f );

	void SetUnit( NWorld::CUnit *pUnit );
	void SetSequence( NDb::CSequence *pSequence, NDb::CSequence *pExpression = 0 );

	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
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
	ZDATA_(CWindow)
	float fScale;
	float fFOV;
	float fYaw;
	float fPitch;
	float fDistance;
	CVec3 vAnchor;
	CTimeCounter sTimer;
	CObj<NGScene::IGameView> p3DView;
	CPtr<NRender::IShowUnit> pInventoryUnit;
	CPtr<NRender::IRenderGame> pRenderGame;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&fScale); f.Add(3,&fFOV); f.Add(4,&fYaw); f.Add(5,&fPitch); f.Add(6,&fDistance); f.Add(7,&vAnchor); f.Add(8,&sTimer); f.Add(9,&p3DView); f.Add(10,&pInventoryUnit); f.Add(11,&pRenderGame); return 0; }

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
	void SetLight( NDb::CTAmbientLight *pLight );
	// release @0x1bf030: (lipsync seq, expression seq) -- both forwarded to the shown unit's head
	void SetSequence( NDb::CSequence *pSequence, NDb::CSequence *pExpression = 0 );
	void PlayAnimation( NDb::CAnimation *pAnim, bool bLoop );

	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CInteractiveUnitView
////////////////////////////////////////////////////////////////////////////////////////////////////
class CInteractiveUnitView: public CWindow
{
	OBJECT_BASIC_METHODS(CInteractiveUnitView);
private:
	ZDATA_(CWindow)
	bool bButtonDown;
	float fAngle;
	SPoint sLastPoint;
	CTimeCounter sTimer;
	CObj<CObjectBase> pMouseCapture;
	CObj<NGScene::IGameView> p3DView;
	CPtr<NRender::IShowUnit> pInventoryUnit;
	CPtr<NRender::IRenderGame> pRenderGame;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&bButtonDown); f.Add(3,&fAngle); f.Add(4,&sLastPoint); f.Add(5,&sTimer); f.Add(6,&pMouseCapture); f.Add(7,&p3DView); f.Add(8,&pInventoryUnit); f.Add(9,&pRenderGame); return 0; }
private:
	// Optional perspective camera (TRANSIENT -- not serialized, so no save-format change). Set only by the
	// CDBCamera SetUnit overload (AdvFaceGen passes DataCamera 5014); when unset, Draw keeps the orthographic
	// full-body view CharGen + the inventory model show rely on.
	bool bHasCamera = false;
	float fYaw = 0, fPitch = 0, fDistance = 0, fFOV = 60;
	CVec3 vAnchor;

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
	// retail Set @0x1c0f50 builds the WHOLE static tooltip (all per-type text); the owning unit (may be
	// null) is cached for Draw's per-frame "familiarity" refresh.
	void Set( NRPG::IInventoryItem* pItem, NDb::ECameraType eCameraType, NWorld::CUnit* pUnit = 0 );

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
// CSlot
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSlot: public CWindow
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
	ZDATA_(CWindow)
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
	CArray2D<SHilight> hilights;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pMission); f.Add(3,&nWidth); f.Add(4,&nHeight); f.Add(5,&bTrackMouse); f.Add(6,&bAlwaysHilight); f.Add(7,&sMousePoint); f.Add(8,&pSlotView); f.Add(9,&pHilight); f.Add(10,&itemsSet); f.Add(11,&eCameraType); f.Add(12,&hilights); return 0; }

protected:
	void GetInSlotPos( int nX, int nY, SPoint *pCoords );
	void GetItemInSlotPos( int nX, int nY, const SPoint &sItemSize, SPoint *pCoords );
	bool GetDragItem( NWorld::IPlayer::SItemInfo *pInfo );
	NGame::IMission* GetGame();

public:
	CSlot() {}
	CSlot( const SWindowInfo &sInfo, NGame::IMission *pMission, int nWidth, int nHeight, NDb::ECameraType eCameraType, bool bAlwaysHilight );

	virtual void Take( int nX, int nY ) = 0;
	virtual void Place( int nX, int nY, const NWorld::SItem &sItem ) = 0;
	virtual bool CanPlace( int nX, int nY, const NWorld::SItem &sItem, int *nAP = 0 ) = 0;
	virtual void GetItemsList( vector<SItem> *pItemsSet ) = 0;

	void SetSize( const SPoint &sSize );
	void SetSlotSize( int nWidth, int nHeight );

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
