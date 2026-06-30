#include "StdAfx.h"
#include "Gfx.h"
#include "Transform.h"
#include "GView.h"
#include "GSceneUtils.h"
#include "iMain.h"
#include "G2DView.h"
#include "..\Misc\StrProc.h"
#include "..\MiscDll\Commands.h"
#include "..\Input\Bind.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataDifficulty.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
#include "..\DBFormat\DataAck.h"        // NDb::CDBAck / CDBAckInfo (voice-preview ack lookup)
#include "..\ADOImport\BasicDB.h"       // NDatabase::GetTable / CDBIterator
#include "wInterface.h"
#include "Sound.h"
#include "RPGUnit.h"
#include "RPGMerc.h"
#include "RPGGlobal.h"
#include "LSHead.h"      // NLSHead::CHeadInfo (pOldHead member + CreateLSHeadInfo bake/commit on play)
#include "RWGame.h"
#include "RWSound.h"
#include "Interface.h"
#include "iCommonUI.h"
#include "iDesktopWindow.h"
#include "iAdvFaceGen.h"
#include "iRenderWorld.h"
#include "iGlobalMap.h"
#include "..\DBFormat\DataCamera.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
// The advanced ("custom head") face-gen UI container. The GetUIContainer id is register-noise in the decomp
// (NGame::CAdvFaceGenMenuInterface::Initialize @0x1910d0 loads it from a garbled register), but it was RECOVERED
// from the retail content DB (SQL "A5GAME_English (USA)", UIContainers/UIControls): container 398 ("Main",
// 1024x768) is the only one holding the 14 morph-scroll controls (Age/Gender/Nationality/Lips/Chin/Nose/Brows/
// Cheeks/Men Hair/Women Hair/Eyes color/Face Damage/Eye-glasses/Facial Colors) + play/cancel/view/background
// + the 3 voice1/2/3 controls (Steam game.db UIControls ids 2599/2600/2601). (361 is the BASIC FaceGen
// container; only it carries the customhead control -- but BOTH 361 and 398 carry voice1/2/3.)
const int N_ADVFACEGEN_CONTAINER = 398;   // verified retail UIContainers.ID for the advanced face editor

// The morphable base head the editor installs on entry (so the sliders sculpt the AdvancedFaceGen rig rather
// than whatever fixed preset the merc carried in). Recovered from the decomp (Initialize @0x1910d0:
// `mov ecx,0x5e; call GetComplexHead`) and confirmed in the retail content DB: ComplexHead 94 = head src
// Heads\GDP\AdvancedFaceGen\800_v6_n1_h6.gdp.
const int N_ADVFACEGEN_BASE_HEAD = 94;

// NUI::bGermanVersion (release .data @0x9c6200) -- the German-build UI flag the "ui_germanversion"
// console var drives via NGlobal::VarBoolHandler (1.0 -> the voice1/2/3 buttons are hidden). Registered
// at module-init by the iAdvFaceGenInit static-init object at the bottom of this file (START_REGISTER) --
// that registrar is the only genuinely-missing release symbol from iAdvFaceGen.obj (ctor @0x192900).
// Defaults false, so behaviour is unchanged (voice buttons stay visible, matching this build's
// documented locale adaptation).
bool bGermanVersion = false;

// NOTE on voice buttons: the advanced editor's container (398) DOES carry the voice1/2/3 controls in the Steam
// game.db (UIControls ids 2599/2600/2601, type 4), exactly like the basic FaceGen container 361 (2557-2559).
// (An earlier bundled-DB reading suggested 398 lacked them; that was WRONG for the Steam runtime the build runs
// against.) The retail binary builds the 3 voice CHoverCheckButtons unconditionally (below) and gives them
// per-state art, so they are visible and clicking one previews that voice (see ProcessMessage). CLoader::
// GetControl stays null-safe (logs a UI-ERROR + returns an empty 0x0 placeholder) for any genuinely missing id.
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAdvFaceGenUI -- the advanced ("custom head") face-generation panel. Mirrors the basic CFaceGenUI
// (iFaceGen.cpp) but exposes the full retail set of per-feature morph spinners and drives the LIVE head morph
// through the SESSION-32 IShowUnit chain: each scroll value maps to [-1,+1] and is pushed via
// pUnitView->GetShowUnit()->SetLSHeadParam(name,v) -> CShowRPGUnit -> CFakeRPGUnit -> CHeadTransformInfo.
//
// Reconstructed from the answer-key decode (decomp src/s2_cadvfacegenui.h: CAdvFaceGenUI ctor @0x190f40,
// UpdateHead @0x1909f0, CreateLSHeadInfo @0x1909a0, ProcessMessage @0x191690, reg 0xb3415140), translated from
// the answer-key free-fn-over-mirror+hook style to real engine types.
//
// FUNCTIONAL ADAPTATIONS carried from SESSION 32 (see docs/CONVERGENCE_PROGRESS.md):
//  - TEXTURE morph deferred: the mesh (face-shape) morph is live + faithful, but the texture-only params
//    (EyesColor/HairColor/FacialColor/EyeGlasses/FaceDamage) move the tension map without recolouring the
//    skin/eye textures (the Sentinels-LS TexMixer path is not reconstructed). Expected + documented.
//  - No bGermanVersion global in this build, so the voice buttons are not hidden for a German locale.
//  - The per-voice acknowledgement sound IS played via the dev-native GetPersVoiceAck (= retail NUI::GetPersAck
//    @0x2452c0), deriving the per-voice pers with FindVoicePersId in lieu of retail's GetAckHolder. Both this
//    advanced screen and the basic CFaceGenUI now play it on a voice click (mirrors retail @0x1d27e0).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAdvFaceGenUI: public CDesktopWindow
{
	OBJECT_BASIC_METHODS(CAdvFaceGenUI);
private:
	ZDATA_(CDesktopWindow)
	int nVoice;
	CObj<NRPG::CUnit> pMerc;              // the edited merc (answer-key pUnit)
	CDBPtr<NDb::CNationality> pNationality;
	bool bChanged;
	CObj<CImage> pBackground;
	CObj<CHoverButton> pPlay;
	CObj<CHoverButton> pBack;
	CObj<CInteractiveUnitView> pUnitView;
	// 14 face-morph spinners, looked up from the template by id in EVENT_TEMPLATELOADCOMPLETE.
	CPtr<CScroll> pAgeScroll;
	CPtr<CScroll> pGenderScroll;
	CPtr<CScroll> pNationalityScroll;
	CPtr<CScroll> pLipsScroll;
	CPtr<CScroll> pChinScroll;
	CPtr<CScroll> pNoseScroll;
	CPtr<CScroll> pBrowsScroll;
	CPtr<CScroll> pCheersScroll;
	CPtr<CScroll> pMenHairScroll;
	CPtr<CScroll> pWomenHairScroll;
	CPtr<CScroll> pEyesColorScroll;
	CPtr<CScroll> pFaceDamageScroll;
	CPtr<CScroll> pEyeGlassesScroll;
	CPtr<CScroll> pFacialColorsScroll;
	CObj<CHoverCheckButton> pVoice1;      // 3 voice radio checkbuttons (release controls voice1/2/3)
	CObj<CHoverCheckButton> pVoice2;
	CObj<CHoverCheckButton> pVoice3;
	ZEND int operator&( CStructureSaver &f )
	{
		f.Add(1,(CDesktopWindow*)this); f.Add(2,&nVoice); f.Add(3,&pMerc); f.Add(4,&pNationality); f.Add(5,&bChanged);
		f.Add(6,&pBackground); f.Add(7,&pPlay); f.Add(8,&pBack); f.Add(9,&pUnitView);
		f.Add(10,&pAgeScroll); f.Add(11,&pGenderScroll); f.Add(12,&pNationalityScroll); f.Add(13,&pLipsScroll);
		f.Add(14,&pChinScroll); f.Add(15,&pNoseScroll); f.Add(16,&pBrowsScroll); f.Add(17,&pCheersScroll);
		f.Add(18,&pMenHairScroll); f.Add(19,&pWomenHairScroll); f.Add(20,&pEyesColorScroll); f.Add(21,&pFaceDamageScroll);
		f.Add(22,&pEyeGlassesScroll); f.Add(23,&pFacialColorsScroll);
		f.Add(24,&pVoice1); f.Add(25,&pVoice2); f.Add(26,&pVoice3);
		return 0;
	}

protected:
	void SetupScroll( CScroll *pScroll, int nValue, int nMaxValue );
	void UpdateHead();
	void UpdateVoiceChecks();

public:
	CAdvFaceGenUI() {}
	CAdvFaceGenUI( const SWindowInfo &sInfo, NRPG::CUnit *pMerc, NDb::CNationality *pNationality );

	NLSHead::CHeadInfo *CreateLSHeadInfo();

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CAdvFaceGenUI::CAdvFaceGenUI( const SWindowInfo &sInfo, NRPG::CUnit *_pMerc, NDb::CNationality *_pNationality ):
	CDesktopWindow( sInfo ), nVoice( 0 ), pMerc( _pMerc ), pNationality( _pNationality ), bChanged( false )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// One scroll's EVENT_TEMPLATELOADCOMPLETE seeding. The answer-key/decomp order is SetValue-then-SetMaxValue,
// but CSlider::SetValue clamps to [0,max] and SetMaxValue re-clamps the value, so seeding the value before the
// max would clamp it against the template's stale max. We set the max FIRST (a functional correction) so the
// seed lands; the horizontal scroll style matches the basic CFaceGenScroll.
void CAdvFaceGenUI::SetupScroll( CScroll *pScroll, int nValue, int nMaxValue )
{
	if ( !IsValid( pScroll ) )
		return;
	pScroll->SetStyle( SCRLSTYLE_HORZ, true );
	pScroll->SetMaxValue( nMaxValue );
	pScroll->SetValue( nValue );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Push the current value of all 14 morph spinners onto the live shown head. Each 0..100 spinner maps to
// [-1,+1] via v = value*0.02 - 1.0 (the decomp constants 0.02f / 1.0f), then is sent through the IShowUnit
// named-parameter setter (the wired SESSION-32 chain). The 3 voice buttons are NOT touched here.
void CAdvFaceGenUI::UpdateHead()
{
	if ( !IsValid( pUnitView ) )
		return;
	NRender::IShowUnit *pShow = pUnitView->GetShowUnit();
	if ( !IsValid( pShow ) )
		return;

	struct SMorphParam { CScroll *pScroll; const char *szName; };
	const SMorphParam params[] =
	{
		{ pAgeScroll,          "Age"         },
		{ pGenderScroll,       "Gender"      },
		{ pNationalityScroll,  "Nationality" },
		{ pLipsScroll,         "Lips"        },
		{ pChinScroll,         "Chin"        },
		{ pNoseScroll,         "Nose"        },
		{ pBrowsScroll,        "Brows"       },
		{ pCheersScroll,       "Cheeks"      },
		{ pMenHairScroll,      "HairColor"   },
		{ pWomenHairScroll,    "WomanHair"   },
		{ pEyesColorScroll,    "EyesColor"   },
		{ pEyeGlassesScroll,   "EyeGlasses"  },
		{ pFaceDamageScroll,   "FaceDamage"  },
		{ pFacialColorsScroll, "FacialColor" },
	};
	const int nParams = sizeof( params ) / sizeof( params[0] );
	for ( int i = 0; i < nParams; ++i )
	{
		if ( !IsValid( params[i].pScroll ) )
			continue;
		float fValue = params[i].pScroll->GetValue() * 0.02f - 1.0f;
		pShow->SetLSHeadParam( params[i].szName, fValue );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAdvFaceGenUI::UpdateVoiceChecks()
{
	// Radio-check the 3 voice buttons on the selected voice.
	if ( IsValid( pVoice1 ) ) pVoice1->SetChecked( nVoice == 0 );
	if ( IsValid( pVoice2 ) ) pVoice2->SetChecked( nVoice == 1 );
	if ( IsValid( pVoice3 ) ) pVoice3->SetChecked( nVoice == 2 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Bake the currently-previewed head into a fresh CHeadInfo (the IShowUnit head-info factory; the answer-key
// "chosen head"). The menu interface can apply this to the merc on "play" once CUnit head-commit is wired.
NLSHead::CHeadInfo *CAdvFaceGenUI::CreateLSHeadInfo()
{
	if ( !IsValid( pUnitView ) )
		return 0;
	NRender::IShowUnit *pShow = pUnitView->GetShowUnit();
	return IsValid( pShow ) ? pShow->CreateLSHeadInfo() : 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// GetPersVoiceAck -- the per-voice preview sound (retail NUI::GetPersAck @0x2452c0, dev-native form). Find the
// merc's pers "on-select greeting" acknowledgement (the CDBAck whose nConditionID is the greeting condition 102
// and whose nRPGPersID matches the merc), take its first ack-info, and return the sound for the merc's CURRENT
// voice. The dev authors the per-voice sounds in CDBAckInfo::voices[0..5] (the SoundID/SoundID1.. columns), so
// GetVoice(nVoice) is voice-specific -- no GetAckHolder needed (which the dev lacks). Played on a voice click.
// The "alt-voice pers" for the picked voice: retail's GetAckHolder maps a hero to a DIFFERENT pers per voice
// (each pers has a fixed Voice and its own command-ack voices[0]); the per-voice variation is by PERS, not by the
// voices[] index. The dev has no runtime alt-voice list, so derive it from the data: a hero pers in the merc's
// "voice family" (same Side + gender) whose nVoice == the picked voice. Falls back to the merc's own pers.
static int FindVoicePersId( NRPG::CUnit *pMerc, int nVoice )
{
	NDb::CRPGPers *pMercPers = pMerc->GetPers();
	if ( !IsValid( pMercPers ) )
		return pMerc->GetRPGPersID();
	CDBTable<NDb::CRPGPers> *pTable = NDatabase::GetTable<NDb::CRPGPers>();
	if ( pTable )
	{
		CDBIterator<NDb::CRPGPers> it( *pTable );
		while ( it.MoveNext() )
		{
			NDb::CRPGPers *p = it.Get();
			if ( p && p->nVoice == nVoice && p->bIsFemale == pMercPers->bIsFemale
				&& (NDb::CSide*)p->pSide == (NDb::CSide*)pMercPers->pSide )
				return p->nRPGPersID;
		}
	}
	return pMerc->GetRPGPersID();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// External linkage (declared in iCommonUI.h) so the basic FaceGen screen (iFaceGen.cpp) shares this helper;
// FindVoicePersId above stays file-static (only this fn calls it). Mirrors retail NUI::GetPersAck @0x2452c0.
NDb::CSound* GetPersVoiceAck( NRPG::CUnit *pMerc )
{
	if ( !IsValid( pMerc ) )
		return 0;
	CDBTable<NDb::CDBAck> *pTable = NDatabase::GetTable<NDb::CDBAck>();
	if ( !pTable )
		return 0;
	int nVoice = pMerc->GetVoice();
	int persId = FindVoicePersId( pMerc, nVoice );   // the per-voice pers (NOT the merc's own single-voice pers)
	CDBIterator<NDb::CDBAck> it( *pTable );
	while ( it.MoveNext() )
	{
		NDb::CDBAck *rec = it.Get();
		if ( !rec || !IsValid( rec->pAckSequence ) )
			continue;
		if ( rec->nConditionID != 102 )          // the on-select greeting condition (retail pCondition id 0x66)
			continue;
		if ( rec->nRPGPersID != persId )
			continue;
		NDb::CDBAckInfo *info = rec->pAckSequence->pDBAckInfo[0];
		if ( !IsValid( info ) || info->voices.empty() )
			continue;
		return info->GetVoice( nVoice ).pSound;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAdvFaceGenUI::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			// Gold-Impact markup = 11129 Normal / 11130 Hover / 17339 Disabled (the dev's 10892/10893/10894
			// don't exist in the Strings table -> empty prefix -> default font).
			pPlay = new CHoverButton( sEvent.pLoader->GetControl( "play" ) );
			pPlay->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 11130 ) + GetDBString( 18755 ) );
			pPlay->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 11129 ) + GetDBString( 18755 ) );
			pPlay->AddTextState( CHoverButton::STATE_DISABLED, GetDBString( 17339 ) + GetDBString( 18755 ) );

			// Retail decomp gives pBack only HOVER + NORMAL (it dropped the DISABLED state the basic screen had).
			pBack = new CHoverButton( sEvent.pLoader->GetControl( "cancel" ) );
			pBack->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 11130 ) + GetDBString( 10895 ) );
			pBack->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 11129 ) + GetDBString( 10895 ) );

			// 3 voice radio checkbuttons (release controls voice1/2/3). Give them the per-voice art (UITextures
			// 908-916, normal/hover/checked) EXACTLY as the basic CFaceGenUI does -- the FaceGen containers ship
			// these voice controls ARTLESS, so without explicit image states the buttons draw nothing (invisible).
			// If this screen's container provides the voice controls, they now light up (matching the basic screen).
			pVoice1 = new CHoverCheckButton( sEvent.pLoader->GetControl( "voice1" ) );
			pVoice1->AddImageState( 0, NDb::GetUITexture( 908 ) );
			pVoice1->AddImageState( 1, NDb::GetUITexture( 909 ) );
			pVoice1->AddImageState( 3, NDb::GetUITexture( 910 ) );
			pVoice2 = new CHoverCheckButton( sEvent.pLoader->GetControl( "voice2" ) );
			pVoice2->AddImageState( 0, NDb::GetUITexture( 911 ) );
			pVoice2->AddImageState( 1, NDb::GetUITexture( 912 ) );
			pVoice2->AddImageState( 3, NDb::GetUITexture( 913 ) );
			pVoice3 = new CHoverCheckButton( sEvent.pLoader->GetControl( "voice3" ) );
			pVoice3->AddImageState( 0, NDb::GetUITexture( 914 ) );
			pVoice3->AddImageState( 1, NDb::GetUITexture( 915 ) );
			pVoice3->AddImageState( 3, NDb::GetUITexture( 916 ) );

			// The live, rotatable 3D unit view; binding the merc wires its IShowUnit morph chain.
			pUnitView = new CInteractiveUnitView( sEvent.pLoader->GetControl( "view" ) );
			pUnitView->SetUnit( pMerc, NDb::GetDBCamera( 5014 ) );   // AdvancedFaceGen perspective head camera (release @0x592832: GetDBCamera 0x1396)

			// Create the 14 morph scrolls up-front and set HORZ NOW (before their nested templates load) so
			// the inner CSlider inherits horizontal orientation -- the auto-created (fetched) path defaulted to
			// VERT and the later SetupScroll flip came too late, pinning every handle to one end. Bare CScroll
			// (its base wires the auto-created +/- CButtons + CSlider; retail @0x191690 also just GetUIWindows).
			pAgeScroll = new CScroll( sEvent.pLoader->GetControl( "Age" ) );  pAgeScroll->SetStyle( SCRLSTYLE_HORZ, true );
			pGenderScroll = new CScroll( sEvent.pLoader->GetControl( "Gender" ) );  pGenderScroll->SetStyle( SCRLSTYLE_HORZ, true );
			pNationalityScroll = new CScroll( sEvent.pLoader->GetControl( "Nationality" ) );  pNationalityScroll->SetStyle( SCRLSTYLE_HORZ, true );
			pLipsScroll = new CScroll( sEvent.pLoader->GetControl( "Lips" ) );  pLipsScroll->SetStyle( SCRLSTYLE_HORZ, true );
			pChinScroll = new CScroll( sEvent.pLoader->GetControl( "Chin" ) );  pChinScroll->SetStyle( SCRLSTYLE_HORZ, true );
			pNoseScroll = new CScroll( sEvent.pLoader->GetControl( "Nose" ) );  pNoseScroll->SetStyle( SCRLSTYLE_HORZ, true );
			pBrowsScroll = new CScroll( sEvent.pLoader->GetControl( "Brows" ) );  pBrowsScroll->SetStyle( SCRLSTYLE_HORZ, true );
			pCheersScroll = new CScroll( sEvent.pLoader->GetControl( "Cheeks" ) );  pCheersScroll->SetStyle( SCRLSTYLE_HORZ, true );
			pMenHairScroll = new CScroll( sEvent.pLoader->GetControl( "Men Hair" ) );  pMenHairScroll->SetStyle( SCRLSTYLE_HORZ, true );
			pWomenHairScroll = new CScroll( sEvent.pLoader->GetControl( "Women Hair" ) );  pWomenHairScroll->SetStyle( SCRLSTYLE_HORZ, true );
			pEyesColorScroll = new CScroll( sEvent.pLoader->GetControl( "Eyes color" ) );  pEyesColorScroll->SetStyle( SCRLSTYLE_HORZ, true );
			pFaceDamageScroll = new CScroll( sEvent.pLoader->GetControl( "Face Damage" ) );  pFaceDamageScroll->SetStyle( SCRLSTYLE_HORZ, true );
			pEyeGlassesScroll = new CScroll( sEvent.pLoader->GetControl( "Eye-glasses" ) );  pEyeGlassesScroll->SetStyle( SCRLSTYLE_HORZ, true );
			pFacialColorsScroll = new CScroll( sEvent.pLoader->GetControl( "Facial Colors" ) );  pFacialColorsScroll->SetStyle( SCRLSTYLE_HORZ, true );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			nVoice = 0;            // voice 1 is the default selection
			UpdateVoiceChecks();

			pBackground = GetUIWindow<CImage>( this, "background" );
			if ( IsValid( pBackground ) && IsValid( pNationality ) && IsValid( pNationality->pCustomHeadBackground ) )
				pBackground->SetImage( pNationality->pCustomHeadBackground );

			// Fetch + seed the 14 morph scrolls, in decomp order (seed/max from the answer-key decode).
			SetupScroll( pAgeScroll,          50, 100 );
			SetupScroll( pGenderScroll,       50, 100 );
			SetupScroll( pNationalityScroll,   0, 100 );

			SetupScroll( pLipsScroll,   50, 100 );
			SetupScroll( pChinScroll,   50, 100 );
			SetupScroll( pNoseScroll,   50, 100 );
			SetupScroll( pBrowsScroll,  50, 100 );
			SetupScroll( pCheersScroll, 50, 100 );

			SetupScroll( pMenHairScroll,     100, 100 );
			SetupScroll( pWomenHairScroll,   100, 100 );
			SetupScroll( pEyesColorScroll,     0, 100 );
			SetupScroll( pFaceDamageScroll,    0, 100 );
			SetupScroll( pEyeGlassesScroll,    0, 100 );
			SetupScroll( pFacialColorsScroll,  0, 100 );

			UpdateHead();
			break;
		}
	case EVENT_NOTIFY:
		{
			// men-hair / women-hair mutual exclusion: changing one snaps the other to 100; for persID 0xAA,
			// changing women-hair snaps BOTH to 100 (answer-key decode).
			if ( sEvent.szID == "Men Hair" )
			{
				if ( IsValid( pWomenHairScroll ) ) pWomenHairScroll->SetValue( 100 );
			}
			else if ( sEvent.szID == "Women Hair" )
			{
				if ( IsValid( pMenHairScroll ) ) pMenHairScroll->SetValue( 100 );
				if ( IsValid( pMerc ) && pMerc->GetRPGPersID() == 0xAA && IsValid( pWomenHairScroll ) )
					pWomenHairScroll->SetValue( 100 );
			}

			// voice radio: select the voice, set it on the unit, re-check the set, and PLAY a preview of the
			// chosen voice (the pers's greeting ack sound). Handled here (no base chain).
			if ( sEvent.szID == "voice1" || sEvent.szID == "voice2" || sEvent.szID == "voice3" )
			{
				nVoice = ( sEvent.szID == "voice1" ) ? 0 : ( sEvent.szID == "voice2" ) ? 1 : 2;
				if ( IsValid( pMerc ) )
				{
					pMerc->SetVoice( nVoice );
					PlaySound( GetPersVoiceAck( pMerc ) );   // preview the selected voice (PlaySound null-guards)
				}
				UpdateVoiceChecks();
				return true;
			}

			// any other notify (a morph scroll, or a hair change) -> re-push the morph params, then base.
			UpdateHead();
			break;
		}
	}

	return CDesktopWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NUI
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAdvFaceGenMenuInterface -- the modal advanced face-gen screen. Mirrors CFaceGenMenuInterface
// (iFaceGen.cpp): a CRenderBaseInterface 3D backdrop (nationality nFaceGenTemplate) + camera that hosts the
// CAdvFaceGenUI panel. Reconstructed from the answer-key decode (decomp
// src/s2_cadvfacegenmenuinterface.h: Initialize @0x1910d0, Step @0x190ee0, ProcessEvent @0x1914a0,
// reg 0xb3415141). The answer key derives from NMainLoop::IInterfaceBase with its own cursor + CInterface; we
// FOLLOW THE FRANKENSTEIN pattern (CRenderBaseInterface, cursor/CInterface in the base) for consistency with
// the basic FaceGen screen, exactly as SESSION 33 directs.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAdvFaceGenMenuInterface: public CRenderBaseInterface
{
	OBJECT_BASIC_METHODS(CAdvFaceGenMenuInterface);
private:
	NInput::CBind bindClose, bindPlay;

	ZDATA_(CRenderBaseInterface)
	CObj<NRPG::CUnit> pMerc;
	CDBPtr<NDb::CSide> pSide;
	CDBPtr<NDb::CNationality> pNationality;
	CDBPtr<NDb::CDBDifficulty> pDifficulty;
	CObj<NUI::CAdvFaceGenUI> pMenuUI;
	CObj<NLSHead::CHeadInfo> pOldHead;        // the merc's head on entry, restored on "cancel"
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CRenderBaseInterface*)this); f.Add(2,&pMerc); f.Add(3,&pSide); f.Add(4,&pNationality); f.Add(5,&pDifficulty); f.Add(6,&pMenuUI); f.Add(7,&pOldHead); return 0; }

public:
	CAdvFaceGenMenuInterface();

	void Initialize( NDb::CSide *pSide, NDb::CNationality *pNationality, NDb::CDBDifficulty *pDifficulty, NRPG::CUnit *pMerc );

	void Step();
	bool ProcessEvent( const NInput::SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CAdvFaceGenMenuInterface::CAdvFaceGenMenuInterface():
	bindClose( "cancel" ), bindPlay( "play" )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAdvFaceGenMenuInterface::Initialize( NDb::CSide *_pSide, NDb::CNationality *_pNationality, NDb::CDBDifficulty *_pDifficulty, NRPG::CUnit *_pMerc )
{
	pMerc = _pMerc;
	pSide = _pSide;
	pNationality = _pNationality;
	pDifficulty = _pDifficulty;

	// Remember the merc's current head so "cancel" can restore it, then install the morphable AdvancedFaceGen
	// base head (the sliders sculpt this rig). MUST happen before the panel's CInteractiveUnitView::SetUnit
	// below builds the preview from the merc's head. (Decomp Initialize @0x1910d0; faithful order.)
	if ( IsValid( pMerc ) )
	{
		pOldHead = pMerc->GetHeadInfo();
		pMerc->SetHead( NDb::GetComplexHead( NUI::N_ADVFACEGEN_BASE_HEAD ) );
	}

	// Render a 3D backdrop world from the nationality's face-gen template (the hero-menu / basic-facegen pattern).
	CRenderBaseInterface::Initialize( IsValid( pNationality ) ? pNationality->nFaceGenTemplate : 0 );

	CPtr<NDb::CDBCamera> pDBCamera = NDb::GetDBCamera( 5023 );   // N_FACEGEN_CAMERA
	if ( IsValid( pDBCamera ) )
	{
		ICamera::SCameraPos sCameraPos( pDBCamera->vAnchor, pDBCamera->fDistance, pDBCamera->fPitch, pDBCamera->fYaw, pDBCamera->fRoll, pDBCamera->fFOV );
		GetCamera()->SetPlacement( sCameraPos );
	}

	pMenuUI = new NUI::CAdvFaceGenUI( NUI::SWindowInfo( GetInterface(), NUI::SPoint( 0, 0 ), NUI::SPoint( 1024, 768 ), "facegenUI", NUI::STYLE_ENABLED ), pMerc, pNationality );
	NUI::LoadTemplate( pMenuUI, NDb::GetUIContainer( NUI::N_ADVFACEGEN_CONTAINER ) );
	pMenuUI->ShowWindow( NUI::SWTYPE_SHOW );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAdvFaceGenMenuInterface::Step()
{
	CRenderBaseInterface::Step();

	if ( CanRender() )
	{
		GetCamera()->SetScreenRect( CTRect<float>( 0.0f, 0.0f, 1.0f, 1.0f ) );
		RenderFrame( GetTime(), GetCamera() );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAdvFaceGenMenuInterface::ProcessEvent( const NInput::SEvent &sEvent )
{
	NInput::SetSection( "menu" );

	if ( CRenderBaseInterface::ProcessEvent( sEvent ) )
		return true;

	if ( bindClose.ProcessEvent( sEvent ) )
	{
		// "cancel" -> restore the head the merc had on entry, then close the modal.
		if ( IsValid( pMerc ) )
			pMerc->SetHeadInfo( pOldHead );
		NMainLoop::Command( new NMainLoop::CICExitModal() );
		return true;
	}
	else if ( bindPlay.ProcessEvent( sEvent ) )
	{
		// "play" -> bake the morphed preview head and commit it to the merc, then begin the game.
		if ( IsValid( pMenuUI ) && IsValid( pMerc ) )
		{
			NLSHead::CHeadInfo *pChosen = pMenuUI->CreateLSHeadInfo();
			if ( IsValid( pChosen ) )
				pMerc->SetHeadInfo( pChosen );
		}

		NRPG::CGlobalPlayer *pPlayer = NRPG::CreateGlobalPlayer( pSide );
		pPlayer->mercs.push_back( pMerc );

		vector<CObj<NRPG::CGlobalPlayer> > playersSet;
		playersSet.push_back( pPlayer );

		NMainLoop::Command( new NGame::CICBeginGame( pSide->nGlobalMapID, playersSet, pDifficulty ) );
		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CICAdvFaceGen
////////////////////////////////////////////////////////////////////////////////////////////////////
CICAdvFaceGen::CICAdvFaceGen( NDb::CSide *_pSide, NDb::CNationality *_pNationality, NDb::CDBDifficulty *_pDifficulty, NRPG::CUnit *_pMerc ):
	pMerc( _pMerc ), pSide( _pSide ), pNationality( _pNationality ), pDifficulty( _pDifficulty )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CICAdvFaceGen::Exec()
{
	ASSERT( IsValid( pMerc ) );
	if ( !IsValid( pMerc ) )
	{
		csSystem << CC_RED << L"ERROR: AdvFaceGen merc not set!" << endl;
		return;
	}

	CAdvFaceGenMenuInterface *pRes = new CAdvFaceGenMenuInterface();
	pRes->Initialize( pSide, pNationality, pDifficulty, pMerc );
	PushInterface( pRes );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace NGame
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB3415140, CAdvFaceGenUI );
using namespace NGame;
REGISTER_SAVELOAD_CLASS( 0xB3415141, CAdvFaceGenMenuInterface );
////////////////////////////////////////////////////////////////////////////////////////////////////
// iAdvFaceGenInit -- the module's per-compiland static-init console-var registrar (release
// iAdvFaceGen.obj ctor @0x192900). It registers exactly ONE variable, "ui_germanversion", bound to
// NUI::bGermanVersion through the generic bool handler (pContext = &bGermanVersion, default CValue(0.f),
// bSave = false). The var is already consumed via NGlobal::GetVar( "ui_germanversion" )
// (iOptionsMenu.cpp:652) but was never registered/cached in this build; this restores the missing
// registration + backing global. START_REGISTER(iAdvFaceGen) expands to
// `static struct iAdvFaceGenInit { iAdvFaceGenInit(){...} } init;`, emitting the release symbol at
// global scope. Behaviour-neutral: the registered default (false) == this build's effective value today.
START_REGISTER(iAdvFaceGen)
	REGISTER_VAR_EX( "ui_germanversion", NGlobal::VarBoolHandler, &NUI::bGermanVersion, NGlobal::CValue( 0.f ), false )
FINISH_REGISTER
