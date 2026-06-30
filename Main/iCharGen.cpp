#include "StdAfx.h"
#include "Gfx.h"
#include "iMain.h"
#include "G2DView.h"
#include "..\Misc\StrProc.h"
#include "..\MiscDll\Commands.h"
#include "..\Input\Bind.h"
#include "..\Misc\RandomGen.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataDifficulty.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
#include "RPGUnit.h"
#include "RPGMerc.h"
#include "RPGGlobal.h"
#include "Interface.h"
#include "iCommonUI.h"
#include "iCharGen.h"
#include "iFaceGen.h"
#include "iGlobalMap.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
const int N_STAT_MAX_VALUE = 10;
struct SClassToID
{
	const char *pszID;
	NDb::CSide::EPersClass eClass;
};
static SClassToID sClassTable[] = 
{
	"medic", NDb::CSide::MEDIC,
	"scout", NDb::CSide::SCOUT,
	"sniper", NDb::CSide::SNIPER,
	"soldier", NDb::CSide::SOLDIER,
	"engineer", NDb::CSide::ENGINEER,
	"grenadier", NDb::CSide::GRENADIER
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// NUI::GetPers  @0x1b5840  -- the CharGen model-override roll. Lazily caches every "listable" persona
// (NDb::CRPGPers::bCanBeListed), then walks a rolling static index by nDirection (+1/-1) to the next
// persona matching the chosen side + gender. model_left/right step through the cached set.
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CRPGPers* GetPers( int nDirection, NDb::CSide *pSide, bool bFemale )
{
	static vector<CPtr<NDb::CRPGPers> > persSet;
	static bool bInitialized = false;
	static int nCounter = 0;

	if ( !bInitialized )
	{
		bInitialized = true;
		CDBTable<NDb::CRPGPers> *pTable = NDatabase::GetTable<NDb::CRPGPers>();
		CDBIterator<NDb::CRPGPers> it( *pTable );
		while ( it.MoveNext() )
		{
			NDb::CRPGPers *pPers = it.Get();
			if ( pPers->bCanBeListed )
				persSet.push_back( pPers );
		}
	}

	int nSize = persSet.size();
	for ( int i = 0; i < nSize; i++ )
	{
		nCounter += nDirection;
		if ( nCounter < 0 ) nCounter = nSize - 1;
		if ( nCounter >= nSize ) nCounter = 0;

		NDb::CSide *pPersSide = persSet[nCounter]->pSide;
		if ( pPersSide == pSide && persSet[nCounter]->bIsFemale == bFemale )
			return persSet[nCounter];
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStatPoints
////////////////////////////////////////////////////////////////////////////////////////////////////
class CStatPoints: public CObjectBase
{
	OBJECT_BASIC_METHODS(CStatPoints);
private:
	ZDATA
	int nValue;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nValue); return 0; }

public:
	CStatPoints(): nValue( 0 ) {}

	bool Inc() { nValue++; return true; }
	bool Dec() { if ( nValue <= 0 ) return false; nValue--; return true; }

	int GetValue() const { return nValue; }
	void SetValue( int _nValue ) { nValue = _nValue; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CStatChange
////////////////////////////////////////////////////////////////////////////////////////////////////
class CStatChange: public CWindow
{
	OBJECT_BASIC_METHODS(CStatChange);
private:
	ZDATA_(CWindow)
	int nValue;
	CPtr<CStatPoints> pPoints;
	////
	CPtr<CText> pText;
	CPtr<CComplexButton> pUp;
	CPtr<CComplexButton> pDown;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&nValue); f.Add(3,&nDefaultValue); f.Add(4,&pPoints); f.Add(5,&pText); f.Add(6,&pUp); f.Add(7,&pDown); return 0; }
	int nDefaultValue = 0;

public:
	CStatChange() {}
	CStatChange( const SWindowInfo &sInfo, CStatPoints *pPoints );

	int GetValue() const;
	void SetValue( int nValue );
	void SetDefaultValue( int v ) { nDefaultValue = v; }   // release: the "default"/reset snap-back target
	int  GetDefaultValue() const { return nDefaultValue; }

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CStatChange::CStatChange( const SWindowInfo &sInfo, CStatPoints *_pPoints ):
	CWindow( sInfo ), nValue( 0 ), pPoints( _pPoints )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CStatChange::GetValue() const
{
	return nValue;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStatChange::SetValue( int _nValue )
{
	nValue = _nValue;

	WCHAR wsText[256];
	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", nValue );
	pText->SetText( wsText );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStatChange::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_NOTIFY:
		{
			if ( sEvent.szID == "up" )
			{
				if ( GetValue() > 2 )
				{
					if ( pPoints->Inc() )
						SetValue( GetValue() - 1 );
				}
				return true;
			}
			else if ( sEvent.szID == "down" )
			{
				if ( GetValue() < N_STAT_MAX_VALUE )
				{
					if ( pPoints->Dec() )
						SetValue( GetValue() + 1 );
				}
				return true;
			}

			break;
		}
	case EVENT_TEMPLATELOAD:
		{
			pUp = new CComplexButton( sEvent.pLoader->GetControl( "up" ), 0, 0, 0, 0 );
			pDown = new CComplexButton( sEvent.pLoader->GetControl( "down" ), 0, 0, 0, 0 );
			pUp->Set( NDb::GetUITexture( 398 ), 0, CComplexButton::NORMAL, "up" );
			pDown->Set( NDb::GetUITexture( 399 ), 0, CComplexButton::NORMAL, "down" );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pText = GetUIWindow<CText>( this, "text" );
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CHilight  --  a char-gen section "step blink" guide overlay.
////////////////////////////////////////////////////////////////////////////////////////////////////
// Release @0x1b4340 (ctor) / 0x1b3cb0 (Set) / 0x1b3d00 (Update). A CImage parked over one of the four
// locked sections (nationality/class/name/stats). It is STYLE_TRANSPARENT, so it renders but never
// receives or blocks input -- CWindow::ProcessMessage and HitTest both SKIP transparent children
// (UIWindow.cpp:388/146, where STYLE_ENABLED is NOT consulted). Without that, the visible blink would
// block the very section click it is prompting -- exactly the bug the SESSION-36 stop-gap hid by hiding
// these overlays. The runtime container-345 *_hilight controls do NOT carry bTransparent (CLoader::Load
// would have applied it), so we force STYLE_TRANSPARENT in the ctor.
//   Set(bWant) latches a desired-visible goal and restarts the blink when it changes; Update() fades the
// overlay alpha toward the goal via CalcFlashCoeff and commits the goal (hiding it) once the fade ends.
// Members match the release layout: bMode@0x84 bTargetMode@0x85 sLastTime@0x88 fCoeff@0x8c (sizeof 0x90).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CHilight: public CImage
{
	OBJECT_BASIC_METHODS(CHilight);
private:
	ZDATA_(CImage)
	bool bMode;          // the goal currently committed (visible state actually reached)
	bool bTargetMode;    // the goal we are fading toward (set by Set())
	STime sLastTime;     // last Update time, == 0 restarts the fade
	float fCoeff;        // current fade coefficient 0..1 (alpha multiplier)
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CImage*)this); f.Add(2,&bMode); f.Add(3,&bTargetMode); f.Add(4,&sLastTime); f.Add(5,&fCoeff); return 0; }

public:
	CHilight() {}
	CHilight( const SWindowInfo &sInfo );

	void Set( bool bMode );

	void Update( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CHilight::CHilight( const SWindowInfo &sInfo ):
	CImage( sInfo ), bMode( true ), bTargetMode( true ), sLastTime( 0 ), fCoeff( 1.0f )
{
	// The blink guide must NEVER block the section click it is prompting (see class note).
	SetStyle( STYLE_TRANSPARENT, true );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHilight::Set( bool bNewMode )
{
	// Release @0x1b3cb0: latch a new desired-visible goal; restart the blink + force visible when it flips.
	if ( bNewMode != bTargetMode )
	{
		sLastTime = 0;
		if ( bTargetMode != bMode )
			bMode = bTargetMode;
		SetStyle( STYLE_VISIBLE, true );
	}
	bTargetMode = bNewMode;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHilight::Update( const STime &sTime, NGScene::I2DGameView *pView )
{
	// Release @0x1b3d00: fade the overlay alpha toward the latched goal, then commit (and hide) at the end.
	if ( bTargetMode != bMode )
	{
		if ( sLastTime == 0 )
			sLastTime = sTime;
		float fTarget = bTargetMode ? 1.0f : 0.0f;
		fCoeff = CalcFlashCoeff( fCoeff, fTarget, sTime, sLastTime );
		sLastTime = sTime;
		if ( fCoeff == fTarget )
		{
			bMode = bTargetMode;
			SetStyle( STYLE_VISIBLE, bTargetMode );
		}
	}

	// Release @0x5b3d91: a fixed dark-slate (0x39,0x4A,0x51) at alpha = fCoeff*128 (~50% when fully on) --
	// a faint translucent step-guide, NOT opaque white. The RGB is hardcoded (the control's own colour and
	// any texture are ignored for the tint); fCoeff only fades the alpha in/out.
	SetColor( NGfx::SPixel8888( 0x39, 0x4A, 0x51, 0x80 * fCoeff ) );
	CImage::Update( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCharGenUI
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCharGenUI: public CWindow
{
	OBJECT_BASIC_METHODS(CCharGenUI);
private:
	ZDATA_(CWindow)
	CObj<NRPG::CUnit> pMerc;
	CDBPtr<NDb::CSide> pSide;
	CDBPtr<NDb::CRPGPers> pPers;
	CDBPtr<NDb::CComplexHead> pHead;
	CDBPtr<NDb::CNationality> pNationality;
	////
	bool bChanged;
	bool bResetStats;   // release: one-shot "snap stats back to the rolled defaults" latch ("default" button)
	////
	CObj<NRPG::CUnit> pTempMerc;
	CObj<CHoverButton> pPlay;
	CObj<CHoverButton> pBack;
	CObj<CButtonsLine> pButtonsLine;            // release: hosts play/cancel (container 345 has only "line")
	CObj<CHoverButton> pModelLeft;              // release: model-override nav (prev appearance)
	CObj<CHoverButton> pModelRight;             // release: model-override nav (next appearance)
	CObj<CInteractiveUnitView> pUnitView;
	////
	CObj<CComplexButton> pMale;
	CObj<CComplexButton> pFemale;
	////
	CObj<CComplexButton> pNation1;
	CObj<CComplexButton> pNation2;
	CObj<CComplexButton> pNation3;
	////
	CDBPtr<NDb::CComplexHead> pOverrideHead;     // release model-override: head of the rolled override persona
	CObj<NDb::CModel> pOverrideModel;            // release model-override: pre-built body model
	CDBPtr<NDb::CRPGUniform> pOverrideUniform;   // release model-override: uniform of the rolled override persona
	////
	vector<CObj<CComplexButton> > classIconsSet;
	////
	CPtr<CText> pPointsText;
	CPtr<CImage> pBackground;                    // release: nationality CharGen backdrop image (control "background")
	CObj<CStatPoints> pPoints;
	CObj<CStatChange> pStrength;
	CObj<CStatChange> pDexteriry;
	CObj<CStatChange> pIntelligence;
	////
	CPtr<CEdit> pName;
	CPtr<CEdit> pSurname;
	CPtr<CEdit> pNickname;
	////
	CPtr<CText> pEvasion;
	CPtr<CText> pActionPoints;
	CPtr<CText> pVitalityPoints;
	////
	CPtr<CText> pHide;
	CPtr<CText> pSpot;
	CPtr<CText> pBurst;
	CPtr<CText> pMelee;
	CPtr<CText> pSnipe;
	CPtr<CText> pMedicine;
	CPtr<CText> pShooting;
	CPtr<CText> pThrowing;
	CPtr<CText> pInterrupt;
	CPtr<CText> pEngineering;
		////
		// Release @0x1b5da0: the four section step-blink guides (container 345 nation/class/names/stats_hilight),
		// built as transparent CHilight overlays in EVENT_TEMPLATELOAD and driven by Draw's completion gate.
		CObj<CHilight> pNationalityHilight;
		CObj<CHilight> pClassHilight;
		CObj<CHilight> pNameHilight;
		CObj<CHilight> pStatsHilight;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CWindow*)this); f.Add(2,&pMerc); f.Add(3,&pSide); f.Add(4,&pPers); f.Add(5,&pHead); f.Add(6,&pNationality); f.Add(7,&bChanged); f.Add(8,&pTempMerc); f.Add(9,&pPlay); f.Add(10,&pBack); f.Add(11,&pUnitView); f.Add(12,&pMale); f.Add(13,&pFemale); f.Add(14,&pNation1); f.Add(15,&pNation2); f.Add(16,&pNation3); f.Add(17,&classIconsSet); f.Add(18,&pPointsText); f.Add(19,&pPoints); f.Add(20,&pStrength); f.Add(21,&pDexteriry); f.Add(22,&pIntelligence); f.Add(23,&pName); f.Add(24,&pSurname); f.Add(25,&pNickname); f.Add(26,&pEvasion); f.Add(27,&pActionPoints); f.Add(28,&pVitalityPoints); f.Add(29,&pHide); f.Add(30,&pSpot); f.Add(31,&pBurst); f.Add(32,&pMelee); f.Add(33,&pSnipe); f.Add(34,&pMedicine); f.Add(35,&pShooting); f.Add(36,&pThrowing); f.Add(37,&pInterrupt); f.Add(38,&pEngineering); f.Add(39,&pButtonsLine); f.Add(40,&bResetStats); f.Add(41,&pBackground); f.Add(42,&pModelLeft); f.Add(43,&pModelRight); f.Add(44,&pOverrideModel); f.Add(45,&pOverrideUniform); f.Add(46,&pOverrideHead); f.Add(47,&pNationalityHilight); f.Add(48,&pClassHilight); f.Add(49,&pNameHilight); f.Add(50,&pStatsHilight); return 0; }

protected:
	void Generate();
	void GenerateEmpty();
	void UpdateNationality();
	void UpdateUnit();

public:
	CCharGenUI() {}
	CCharGenUI( const SWindowInfo &sInfo, NDb::CSide *pSide );

	NDb::CRPGPers * GetPers() const;
	NDb::CComplexHead* GetHead() const;
	NDb::CNationality* GetNationality() const;
	NRPG::CUnit* GetUnit();

	bool ProcessMessage( const SEvent &sEvent );
	void Draw( const STime &sTime, NGScene::I2DGameView *pView );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CCharGenUI::CCharGenUI( const SWindowInfo &sInfo, NDb::CSide *_pSide ):
	CWindow( sInfo ), pSide( _pSide ), bChanged( false ), bResetStats( false )
{
	pPoints = new CStatPoints;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CRPGPers* CCharGenUI::GetPers() const
{
	return pPers;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CComplexHead* CCharGenUI::GetHead() const
{
	return pHead;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CNationality* CCharGenUI::GetNationality() const
{
	return pNationality;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NRPG::CUnit* CCharGenUI::GetUnit()
{
	// release @0x1b43d0: stamp the three name edits into the rolled merc's full name
	// ( <name> "<nickname>" <surname> ) and return the LIVE pMerc (not a fresh re-roll), so the
	// custom name AND the spinner-adjusted stats survive into CICFaceGen / the roster.
	pMerc->wsFullName = pName->GetText() + L" \"" + pNickname->GetText() + L"\" " + pSurname->GetText();
	return pMerc;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CCharGenUI::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_NOTIFY:
		{
			if ( sEvent.szID == "default" )
			{
				// release: latch a one-shot stat reset (snap to the rolled defaults), NOT a full re-roll.
				bResetStats = true;
				return true;
			}
			else if ( sEvent.szID == "reset" )
			{
				pPoints->SetValue( pPoints->GetValue() + pStrength->GetValue() - 2 );
				pStrength->SetValue( 2 );

				pPoints->SetValue( pPoints->GetValue() + pDexteriry->GetValue() - 2 );
				pDexteriry->SetValue( 2 );

				pPoints->SetValue( pPoints->GetValue() + pIntelligence->GetValue() - 2 );
				pIntelligence->SetValue( 2 );
				return true;
			}

			int nProfession = -1;
			bool bMale = false, bFemale = false;
			bool bNational0 = false, bNational1 = false, bNational2 = false;
			if ( sEvent.szID == "male" )
				bMale = true;
			else if ( sEvent.szID == "female" )
				bFemale = true;
			if ( bMale || bFemale )
			{
				bChanged = true;
				bResetStats = true;
				// changing gender invalidates any model override.
				pOverrideHead = 0;
				pOverrideModel = 0;
				pOverrideUniform = 0;
				pMale->SetChecked( bMale );
				pFemale->SetChecked( bFemale );
				return true;
			}

			if ( sEvent.szID == "national_0" )
				bNational0 = true;
			else if ( sEvent.szID == "national_1" )
				bNational1 = true;
			else if ( sEvent.szID == "national_2" )
				bNational2 = true;
			if ( bNational0 || bNational1 || bNational2 )
			{
				bChanged = true;
				bResetStats = true;
				pOverrideHead = 0;   // the national path drops only the override HEAD
				pNation1->SetChecked( bNational0 );
				pNation2->SetChecked( bNational1 );
				pNation3->SetChecked( bNational2 );
				return true;
			}

			for ( int nTemp = 0; nTemp < classIconsSet.size(); nTemp++ )
			{
				if ( classIconsSet[nTemp]->GetWindowID() == sEvent.szID )
					nProfession = nTemp;
			}
			if ( nProfession != -1 )
			{
				bChanged = true;
				bResetStats = true;

				for ( int nTemp = 0; nTemp < classIconsSet.size(); nTemp++ )
					classIconsSet[nTemp]->SetChecked( false );

				pOverrideHead = 0;
				pOverrideModel = 0;
				pOverrideUniform = 0;

				classIconsSet[nProfession]->SetChecked( true );
				return true;
			}

			// release: model-override nav -- roll the prev/next listable persona for the chosen
			// side+gender and take its head/model/uniform as an override. ALWAYS falls through to base.
			if ( sEvent.szID == "model_left" || sEvent.szID == "model_right" )
			{
				int nDirection = ( sEvent.szID == "model_right" ) ? 1 : -1;
				NDb::CRPGPers *pOverridePers = NUI::GetPers( nDirection, pSide, pFemale->IsChecked() );
				if ( IsValid( pOverridePers ) )
				{
					SRand rand;
					bChanged = true;
					pOverrideHead = pOverridePers->pHead;
					pOverrideModel = pOverridePers->pModel->CreateModel( &rand );
					pOverrideUniform = pOverridePers->pUniform;
				}
			}

			break;
		}
	case EVENT_TEMPLATELOAD:
		{
			// Release: container 345 ships NO static "play"/"cancel" controls (the dev looked them up by
			// name -> dead, default-font buttons). Build them programmatically in one CButtonsLine over the
			// "line" control (the main-menu / hero-menu pattern). Add order = cancel, play.
			// The gold-Impact button markup lives in the shared "Menu - ButtonState - *" strings
			// (11129 Normal / 11130 Hover / 17339 Disabled -- confirmed gold-Impact in the runtime,
			// the same markup MainMenu/HeroMenu use). The dev's 10892/10893/10894 ids DO NOT EXIST in
			// the Strings table -> GetDBString returned empty -> BACK/NEXT rendered in the default font.
			pButtonsLine = new CButtonsLine( sEvent.pLoader->GetControl( "line" ) );
			pBack = pButtonsLine->AddHoverButton( "cancel", -1,
				GetDBString( 11129 ) + GetDBString( 10895 ),
				GetDBString( 11130 ) + GetDBString( 10895 ),
				GetDBString( 17339 ) + GetDBString( 10895 ) );
			pPlay = pButtonsLine->AddHoverButton( "play", -1,
				GetDBString( 11129 ) + GetDBString( 10896 ),
				GetDBString( 11130 ) + GetDBString( 10896 ),
				GetDBString( 17339 ) + GetDBString( 10896 ) );

			// Release: the model-override nav buttons (prev/next appearance), hidden until a class is chosen.
			// Container 345 gives model_left/model_right NO art (Texture0-2 = 0); retail (CCharGenUI::
			// ProcessMessage @0x1b5da0) loads the arrow art via AddImageState state 0 NORMAL_UP / 1 NORMAL_DOWN
			// (UITexture 845/846 left, 847/848 right). Without this they drew nothing (invisible nav arrows).
			pModelLeft = new CHoverButton( sEvent.pLoader->GetControl( "model_left" ) );
			pModelLeft->AddImageState( 0, NDb::GetUITexture( 845 ) );
			pModelLeft->AddImageState( 1, NDb::GetUITexture( 846 ) );
			pModelRight = new CHoverButton( sEvent.pLoader->GetControl( "model_right" ) );
			pModelRight->AddImageState( 0, NDb::GetUITexture( 847 ) );
			pModelRight->AddImageState( 1, NDb::GetUITexture( 848 ) );

			pUnitView = new CInteractiveUnitView( sEvent.pLoader->GetControl( "unitshow" ) );

			pMale = new CComplexButton( sEvent.pLoader->GetControl( "male" ), 0, 0, NDb::GetUITexture( 566 ), NDb::GetUITexture( 408 ) );
			pFemale = new CComplexButton( sEvent.pLoader->GetControl( "female" ), 0, 0, NDb::GetUITexture( 566 ), NDb::GetUITexture( 408 ) );
			pMale->Set( NDb::GetUITexture( 580 ), NDb::GetUITexture( 582 ), CComplexButton::UNCHECKED, "male" );
			pFemale->Set( NDb::GetUITexture( 581 ), NDb::GetUITexture( 583 ), CComplexButton::UNCHECKED, "female" );

			pNation1 = new CComplexButton( sEvent.pLoader->GetControl( "national_0" ), 0, 0, NDb::GetUITexture( 566 ), NDb::GetUITexture( 408 ) );
			pNation2 = new CComplexButton( sEvent.pLoader->GetControl( "national_1" ), 0, 0, NDb::GetUITexture( 566 ), NDb::GetUITexture( 408 ) );
			pNation3 = new CComplexButton( sEvent.pLoader->GetControl( "national_2" ), 0, 0, NDb::GetUITexture( 566 ), NDb::GetUITexture( 408 ) );
			if ( IsValid( pSide->pNationality1 ) )
			{
				pNation1->Set( pSide->pNationality1->pIconNormal, pSide->pNationality1->pIconDisabled, CComplexButton::UNCHECKED, "national_0" );
				pNation1->GetToolTip()->SetText( GetDBString( pSide->pNationality1->pToolTip ) );
			}
			if ( IsValid( pSide->pNationality2 ) )
			{
				pNation2->Set( pSide->pNationality2->pIconNormal, pSide->pNationality2->pIconDisabled, CComplexButton::UNCHECKED, "national_1" );
				pNation2->GetToolTip()->SetText( GetDBString( pSide->pNationality2->pToolTip ) );
			}
			if ( IsValid( pSide->pNationality3 ) )
			{
				pNation3->Set( pSide->pNationality3->pIconNormal, pSide->pNationality3->pIconDisabled, CComplexButton::UNCHECKED, "national_2" );
				pNation3->GetToolTip()->SetText( GetDBString( pSide->pNationality3->pToolTip ) );
			}

			const vector<CPtr<NDb::CRPGPers> > &classes = pSide->malePersesSet;

			classIconsSet.resize( ARRAY_SIZE( sClassTable ) );
			for ( int nTemp = 0; nTemp < ARRAY_SIZE( sClassTable ); nTemp++ )
			{
				CPtr<NDb::CRPGPers> pTempPers = classes[sClassTable[nTemp].eClass];
				CPtr<CComplexButton> pButton = new CComplexButton( sEvent.pLoader->GetControl( sClassTable[nTemp].pszID ), 0, 0, NDb::GetUITexture( 566 ), NDb::GetUITexture( 408 ) );

				if ( IsValid( pTempPers ) && IsValid( pTempPers->pClass ) )
				{
					pButton->Set( pTempPers->pClass->pIcon, pTempPers->pClass->pIconDisabled, CComplexButton::UNCHECKED );
					// The class description is DATA-DRIVEN by the control's own TooltipID (container 345:
					// medic/scout/sniper/soldier/engineer/grenadier carry TooltipID 11086-11091). Retail
					// (CCharGenUI::ProcessMessage @0x1b5da0) sets tooltips ONLY on the nationality icons
					// (national_0/1/2 have TooltipID=0 so they MUST be set in code); it does NOT touch the class
					// icons. The dev's GetToolTip()->SetText( pClass->pToolTip ) here wrote an empty/invalid
					// string over the good data-driven tooltip -> the class description stopped appearing.
				}

				classIconsSet[nTemp] = pButton;
			}

			pStrength = new CStatChange( sEvent.pLoader->GetControl( "strength" ), pPoints );
			pDexteriry = new CStatChange( sEvent.pLoader->GetControl( "dexterity" ), pPoints );
			pIntelligence = new CStatChange( sEvent.pLoader->GetControl( "intelligence" ), pPoints );

				// Release @0x1b5da0: build the four section step-blink guides as CHilight (a transparent CImage
				// subclass). Creating them here from the loader controls CLAIMS the container-345
				// nation/class/names/stats_hilight slots by id, so CLoader::Load matches them and does NOT
				// auto-create the opaque default windows that (SESSION 36) won the click dispatch and made the
				// nationality/class/name/stat sections unclickable. Managed by Draw's completion gate.
				pNationalityHilight = new CHilight( sEvent.pLoader->GetControl( "nation_hilight" ) );
				pClassHilight       = new CHilight( sEvent.pLoader->GetControl( "class_hilight" ) );
				pNameHilight        = new CHilight( sEvent.pLoader->GetControl( "names_hilight" ) );
				pStatsHilight       = new CHilight( sEvent.pLoader->GetControl( "stats_hilight" ) );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pName = GetUIWindow<CEdit>( this, "name" );
			pSurname = GetUIWindow<CEdit>( this, "surname" );
			pNickname = GetUIWindow<CEdit>( this, "nickname" );
			pName->SetTextFormat( GetDBString( 10899 ) );
			pSurname->SetTextFormat( GetDBString( 10899 ) );
			pNickname->SetTextFormat( GetDBString( 10899 ) );

			pPointsText = GetUIWindow<CText>( this, "points" );
			pBackground = GetUIWindow<CImage>( this, "background" );   // release: nationality CharGen backdrop

			pEvasion = GetUIWindow<CText>( this, "evasion" );
			pActionPoints = GetUIWindow<CText>( this, "ap" );
			pVitalityPoints = GetUIWindow<CText>( this, "vp" );

			pHide = GetUIWindow<CText>( this, "hide" );
			pSpot = GetUIWindow<CText>( this, "spot" );
			pBurst = GetUIWindow<CText>( this, "burst" );
			pMelee = GetUIWindow<CText>( this, "melee" );
			pSnipe = GetUIWindow<CText>( this, "snipe" );
			pMedicine = GetUIWindow<CText>( this, "medicine" );
			pShooting = GetUIWindow<CText>( this, "shooting" );
			pThrowing = GetUIWindow<CText>( this, "throwing" );
			pInterrupt = GetUIWindow<CText>( this, "interrupt" );
			pEngineering = GetUIWindow<CText>( this, "engineering" );
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCharGenUI::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	// Release @0x1b51d0: a 4-step completion gate (gender -> nationality -> class -> name) drives widget
	// visibility/enable, then the merc roll, stat push and Play-button enable.

	// 1. gender radios always enabled.
	pMale->SetStyle( STYLE_ENABLED, true );
	pFemale->SetStyle( STYLE_ENABLED, true );
	bool bGender = pMale->IsChecked() || pFemale->IsChecked();

	// 2. nationality radios enabled once a gender is chosen.
	pNation1->SetStyle( STYLE_ENABLED, bGender );
	pNation2->SetStyle( STYLE_ENABLED, bGender );
	pNation3->SetStyle( STYLE_ENABLED, bGender );
	bool bNation = bGender && ( pNation1->IsChecked() || pNation2->IsChecked() || pNation3->IsChecked() );

	// 3. class icons enabled once a nationality is chosen.
	for ( int nTemp = 0; nTemp < classIconsSet.size(); nTemp++ )
		classIconsSet[nTemp]->SetStyle( STYLE_ENABLED, bNation );
	bool bAnyClass = false;
	for ( int nTemp = 0; nTemp < classIconsSet.size(); nTemp++ )
		bAnyClass = bAnyClass || classIconsSet[nTemp]->IsChecked();
	bool bClass = bNation && bAnyClass;

	// 4. model-override nav visible once a class is chosen.
	pModelLeft->SetStyle( STYLE_VISIBLE, bClass );
	pModelRight->SetStyle( STYLE_VISIBLE, bClass );

	// 5. the stat block is gated on a non-empty nickname too (release).
	bool bName = bClass && !pNickname->GetText().empty();
	pPointsText->SetStyle( STYLE_VISIBLE, bName );
	pStrength->SetStyle( STYLE_VISIBLE, bName );
	pDexteriry->SetStyle( STYLE_VISIBLE, bName );
	pIntelligence->SetStyle( STYLE_VISIBLE, bName );

	// 6. nationality CharGen backdrop, refreshed once a nationality is chosen.
	pBackground->SetStyle( STYLE_VISIBLE, bNation );
	if ( bNation )
	{
		UpdateNationality();
		if ( IsValid( pBackground ) && IsValid( pNationality ) && IsValid( pNationality->pCharGenBackground ) )
			pBackground->SetImage( pNationality->pCharGenBackground );
	}

	// 7. section step-blink guides: each hilight overlays the section that stays LOCKED until the previous
	// step is done, so it flags the FIRST incomplete step (release @0x1b51d0). CHilight is STYLE_TRANSPARENT,
	// so the blink renders but never blocks the very section click it is prompting.
	if ( IsValid( pNationalityHilight ) ) pNationalityHilight->Set( !bGender );
	if ( IsValid( pClassHilight ) )       pClassHilight->Set( !bNation );
	if ( IsValid( pNameHilight ) )        pNameHilight->Set( !bClass );
	if ( IsValid( pStatsHilight ) )       pStatsHilight->Set( !bName );

	// 8. re-roll the previewed merc when the selection changed.
	if ( bClass && bChanged )
	{
		bChanged = false;
		UpdateUnit();

		if ( IsValid( pMerc ) )
		{
			pUnitView->SetUnit( pMerc );
			pUnitView->SetStyle( STYLE_VISIBLE, true );
		}
	}
	else if ( bGender && bChanged )
	{
		bChanged = false;

		if ( pMale->IsChecked() )
			pTempMerc = NRPG::CreateMerc( NDb::GetPers( 887 ) );   // "Male" gender-preview persona (release)
		else if ( pFemale->IsChecked() )
			pTempMerc = NRPG::CreateMerc( NDb::GetPers( 888 ) );   // "Female" gender-preview persona (release)

		if ( IsValid( pTempMerc ) )
		{
			pUnitView->SetUnit( pTempMerc );
			pUnitView->SetStyle( STYLE_VISIBLE, true );
		}
	}

	// 9. one-shot stat reset -- snap the spinners back to the rolled defaults (the "default" button).
	if ( bName && bResetStats && IsValid( pMerc ) )
	{
		bResetStats = false;
		pPoints->SetValue( 0 );
		pStrength->SetValue( pStrength->GetDefaultValue() );
		pDexteriry->SetValue( pDexteriry->GetDefaultValue() );
		pIntelligence->SetValue( pIntelligence->GetDefaultValue() );
	}

	// 10. push the spinner values into the merc + refresh the readouts, else blank them.
	if ( IsValid( pMerc ) && bName )
	{
		NRPG::CUnit* pUnit = pMerc;

		WCHAR wsText[256];
		swprintf( wsText, L"<font face=Courier size=16pt><color=black><center>%d", pPoints->GetValue() );
		pPointsText->SetText( wsText );

		pUnit->Skills( NDb::ST_STR ).SetNewBaseValue( pStrength->GetValue() );
		pUnit->Skills( NDb::ST_DEX ).SetNewBaseValue( pDexteriry->GetValue() );
		pUnit->Skills( NDb::ST_INT ).SetNewBaseValue( pIntelligence->GetValue() );
		pUnit->UpdateSkills();

		Generate();
	}
	else
		GenerateEmpty();

	// 11. commit the nickname into the merc name + enable Play once all points are spent.
	if ( bName && ( pPoints->GetValue() == 0 ) )
	{
		pMerc->SetName( pNickname->GetText() );
		pPlay->SetStyle( STYLE_ENABLED, true );
	}
	else
		pPlay->SetStyle( STYLE_ENABLED, false );

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCharGenUI::Generate()
{
	if ( !IsValid( pMerc ) )
		return;

	NRPG::CUnit* pUnit = pMerc;

	WCHAR wsText[256];

	pStrength->SetValue( (int)pUnit->Skills( NDb::ST_STR ) );
	pDexteriry->SetValue( (int)pUnit->Skills( NDb::ST_DEX ) );
	pIntelligence->SetValue( (int)pUnit->Skills( NDb::ST_INT ) );

	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", (int)pUnit->Skills( NDb::ST_IC ) );
	pEvasion->SetText( wsText );
	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", (int)pUnit->Skills( NDb::ST_AP ) );
	pActionPoints->SetText( wsText );
	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", (int)pUnit->Skills( NDb::ST_VP ) );
	pVitalityPoints->SetText( wsText );

	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", (int)pUnit->Skills( NDb::ST_STEALTH ) );
	pHide->SetText( wsText );
	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", (int)pUnit->Skills( NDb::ST_SPOT ) );
	pSpot->SetText( wsText );
	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", (int)pUnit->Skills( NDb::ST_BURST ) );
	pBurst->SetText( wsText );
	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", (int)pUnit->Skills( NDb::ST_MELEE ) );
	pMelee->SetText( wsText );
	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", (int)pUnit->Skills( NDb::ST_SNIPE ) );
	pSnipe->SetText( wsText );
	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", (int)pUnit->Skills( NDb::ST_MEDICINE ) );
	pMedicine->SetText( wsText );
	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", (int)pUnit->Skills( NDb::ST_SHOOTING ) );
	pShooting->SetText( wsText );
	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", (int)pUnit->Skills( NDb::ST_THROWING ) );
	pThrowing->SetText( wsText );
	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", (int)pUnit->Skills( NDb::ST_INTERRUPT ) );
	pInterrupt->SetText( wsText );
	swprintf( wsText, L"<font face=Courier size=16pt><center>%d", (int)pUnit->Skills( NDb::ST_ENGINEERING ) );
	pEngineering->SetText( wsText );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCharGenUI::GenerateEmpty()
{
	pEvasion->SetText( L"" );
	pActionPoints->SetText( L"" );
	pVitalityPoints->SetText( L"" );

	pHide->SetText( L"" );
	pSpot->SetText( L"" );
	pBurst->SetText( L"" );
	pMelee->SetText( L"" );
	pSnipe->SetText( L"" );
	pMedicine->SetText( L"" );
	pShooting->SetText( L"" );
	pThrowing->SetText( L"" );
	pInterrupt->SetText( L"" );
	pEngineering->SetText( L"" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCharGenUI::UpdateNationality()
{
	// release @0x1b4730: the active nationality follows the first checked nationality radio. Split out of
	// UpdateUnit so Draw can resolve it (for the backdrop) before re-rolling the merc.
	if ( pNation1->IsChecked() )
		pNationality = pSide->pNationality1;
	else if ( pNation2->IsChecked() )
		pNationality = pSide->pNationality2;
	else if ( pNation3->IsChecked() )
		pNationality = pSide->pNationality3;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCharGenUI::UpdateUnit()
{
	// nationality is resolved by Draw (UpdateNationality) before this runs; guard defensively.
	if ( !IsValid( pNationality ) )
		return;

	vector<CPtr<NDb::CRPGPers> > classes;
	if ( pMale->IsChecked() )
	{
		pHead = pNationality->pMaleHead;
		classes = pSide->malePersesSet;
	}
	else if ( pFemale->IsChecked() )
	{
		pHead = pNationality->pFemaleHead;
		classes = pSide->femalePersesSet;
	}

	for ( int nTemp = 0; nTemp < classIconsSet.size(); nTemp++ )
	{
		if ( classIconsSet[nTemp]->IsChecked() )
		{
			ASSERT( nTemp < ARRAY_SIZE(sClassTable) );
			pPers = classes[sClassTable[nTemp].eClass];
			break;
		}
	}

	// release model-override: a rolled override head replaces the nationality head.
	if ( IsValid( pOverrideHead ) )
		pHead = pOverrideHead;

	// roll the merc with the (optional) override body model, then overlay the override uniform.
	// bHero=true: the chargen merc IS the campaign's main hero. Retail rolls it as a hero; CGlobalGame::GetHero()
	// scans the roster for IsHero(), and a combat mission ends in instant Game Over when no living hero exists.
	// The old accept used CreateMerc(...,true) for exactly this -- it must stay on the rolled pMerc that GetUnit()
	// now returns (this is what regressed the custom-character campaign start when GetUnit replaced that call).
	pMerc = NRPG::CreateMerc( pPers, pOverrideModel, pHead, true );
	if ( IsValid( pOverrideUniform ) && IsValid( pMerc ) )
		pMerc->pUniform = pOverrideUniform;

	// release: stash the rolled baseline as the spinners' default (the "default"/reset target).
	if ( IsValid( pMerc ) )
	{
		pStrength->SetDefaultValue( (int)pMerc->Skills( NDb::ST_STR ) );
		pDexteriry->SetDefaultValue( (int)pMerc->Skills( NDb::ST_DEX ) );
		pIntelligence->SetDefaultValue( (int)pMerc->Skills( NDb::ST_INT ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CCharGenMenuInterface
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCharGenMenuInterface: public NMainLoop::IInterfaceBase
{
	OBJECT_BASIC_METHODS(CCharGenMenuInterface);
private:
	NInput::CBind bindClose, bindPlay;

	ZDATA
	CDBPtr<NDb::CSide> pSide;
	////
	CObj<NUI::ICursor> pCursor;
	CObj<NUI::CInterface> pInterface;
	////
	CObj<NUI::CCharGenUI> pMenuUI;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pSide); f.Add(3,&pDifficulty); f.Add(4,&pCursor); f.Add(5,&pInterface); f.Add(6,&pMenuUI); return 0; }
	CDBPtr<NDb::CDBDifficulty> pDifficulty;

public:
	CCharGenMenuInterface();

	void Initialize( NDb::CSide *pSide, NDb::CDBDifficulty *pDifficulty );

	void Step();
	void OnGetFocus();
	bool ProcessEvent( const NInput::SEvent &eEvent );
	void RenderFrame();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CCharGenMenuInterface::CCharGenMenuInterface():
	bindClose( "cancel" ), bindPlay( "play" )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCharGenMenuInterface::Initialize( NDb::CSide *_pSide, NDb::CDBDifficulty *_pDifficulty )
{
	pSide = _pSide;
	pDifficulty = _pDifficulty;   // release @0x1b4f30: the member existed but was never set in the dev

	pCursor = NUI::ICursor::Create();
	pInterface = new NUI::CInterface( pCursor );

	pMenuUI = new NUI::CCharGenUI( NUI::SWindowInfo( pInterface, NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "chargenUI", NUI::STYLE_ENABLED ), pSide );
	NUI::LoadTemplate( pMenuUI, NDb::GetUIContainer( 345 ) );
	pMenuUI->ShowWindow( NUI::SWTYPE_SHOW );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCharGenMenuInterface::Step()
{
	MarkNewDGFrame();
	if ( CanRender() )
	{
		pInterface->UpdateCursor();
		pInterface->Step( GetTime() );
		RenderFrame();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCharGenMenuInterface::OnGetFocus()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CCharGenMenuInterface::ProcessEvent( const NInput::SEvent &sEvent )
{
	NInput::SetSection( "menu" );

	pCursor->ProcessEvent( sEvent );

	if ( pInterface->ProcessEvent( sEvent ) )
		return true;

	if ( bindClose.ProcessEvent( sEvent ) )
	{
		NMainLoop::Command( new NMainLoop::CICExitModal() );
		return true;
	}
	else if ( bindPlay.ProcessEvent( sEvent ) )
	{
		// Create the merc from the custom-built pers + chosen head, thread it (+ nationality/difficulty) into
		// the merc-based CICFaceGen (release form).
		NMainLoop::Command( new NGame::CICFaceGen( pSide, pMenuUI->GetNationality(), pDifficulty,
			pMenuUI->GetUnit() ) );
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CCharGenMenuInterface::RenderFrame()
{
	NGScene::ClearScreen( CVec3(0.5f, 0.5f, 0.5f ) );
	pInterface->Draw( GetTime() );
	NGScene::Flip();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICMission
////////////////////////////////////////////////////////////////////////////////////////////////////
CICCharGen::CICCharGen( NDb::CSide *_pSide, NDb::CDBDifficulty *_pDifficulty ):
	pSide( _pSide ), pDifficulty( _pDifficulty )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICCharGen::Exec()
{
	// Release @0x1b5160 forwards pDifficulty into CCharGenMenuInterface; the difficulty is carried through
	// the custom-char screen to CICFaceGen -> CICBeginGame so the chosen difficulty takes effect in-game.
	CCharGenMenuInterface *pRes = new CCharGenMenuInterface();
	pRes->Initialize( pSide, pDifficulty );
	PushInterface( pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB3703120, CHilight );      // release id @0x89e280 (factory NewCHilight 0x5b8f80)
REGISTER_SAVELOAD_CLASS( 0xB1025160, CStatChange );
REGISTER_SAVELOAD_CLASS( 0xB1025161, CCharGenUI );
REGISTER_SAVELOAD_CLASS( 0xB1025162, CStatPoints );
using namespace NGame;
REGISTER_SAVELOAD_CLASS( 0xB1023180, CCharGenMenuInterface );
