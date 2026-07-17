#include "StdAfx.h"
#include "GView.h"
#include "G2DView.h"
#include "Transform.h"
#include "RPGUnitInfo.h"
#include "RPGUnit.h"        // NRPG::CUnit complete type (GetRPGUnit()->GetVoice() for the in-game ack voice)
#include "wInterface.h"
#include "Sound.h"
#include "Interface.h"
#include "UIML.h"
#include "iMission.h"
#include "iCommonUI.h"
#include "iMissionUI.h"
#include "iMissionDlgUI.h"
#include "iMissionExec.h"
#include "..\Misc\StrProc.h"
#include "..\Input\Bind.h"
#include "..\DBFormat\DataAck.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataLight.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataCamera.h"
#include "..\DBFormat\DataInterface.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
// retail CMissionDlgUI::UpdateDesktop @0x207490: every dialog fade/move stage runs 500 ms
// (oracle s2_cmissiondlgui.h kFadeStageTime/kMoveStageTime), not the dev's old 1000.
const int
	N_FADE_STAGE_TIME	= 500,
	N_MOVEUNITVIEW_STAGE_TIME = 500;
const int
	N_DISPLACE_DISTANCE = 200;
const int
	N_UNITCAMERA_LEFT = 62,
	N_UNITCAMERA_RIGHT = 63;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAnimUnitView
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAnimUnitView: public CUnitView
{
	OBJECT_NOCOPY_METHODS(CAnimUnitView);
private:
	ZDATA_(CUnitView)
	int nDistance;
	SPoint sBasePosition;
	CDBPtr<NDb::CDBCamera> pCamera;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CUnitView*)this); f.Add(2,&nDistance); f.Add(3,&sBasePosition); f.Add(4,&pCamera); return 0; }

public:
	CAnimUnitView() {}
	CAnimUnitView( const SWindowInfo &sInfo, NRender::IRenderGame *pRenderGame, int nDistance, NDb::CDBCamera *pCamera );

	void SetUnit( NWorld::CUnit *pUnit );
	void SetCoeff( float fCoeff );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CAnimUnitView::CAnimUnitView( const SWindowInfo &sInfo, NRender::IRenderGame *pRenderGame, int _nDistance, NDb::CDBCamera *_pCamera ):
	CUnitView( sInfo, pRenderGame, 0.7f ), nDistance( _nDistance ), pCamera( _pCamera )
{
	sBasePosition = GetPosition();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnimUnitView::SetUnit( NWorld::CUnit *pUnit )
{
	// release CMissionDlgUI unit rebind (s2_cmissiondlgui.h @0x1c03d0 site): SetUnit(unit, camera,
	// false, true, false) -- bItems=false (no weapon stance in the talking-body view), bShowCap=true,
	// bPlayIdle=false (dialog animations drive the body, not the interface idle).
	CUnitView::SetUnit( pUnit, pCamera, false, true, false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnimUnitView::SetCoeff( float fCoeff )
{
	SetPosition( SPoint( sBasePosition.x + nDistance * ( 1.0f - fCoeff ), sBasePosition.y ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMissionDlgUI
////////////////////////////////////////////////////////////////////////////////////////////////////
CMissionDlgUI::CMissionDlgUI():
	bindCancel( "cancel" ), bindNext( "next" ), bindPrev( "back" )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CMissionDlgUI::CMissionDlgUI( const SWindowInfo &sInfo, NGame::IMission *_pMission, CDesktopWindow *_pTransition,
	const string &_szDialogCode, const vector<CObj<NWorld::CUnit> > &_unitsSet, const vector<CPtr<NWorld::CAckEvent> > &_phrasesSet, int _nID ):

	CDesktopWindow( sInfo ), eStage( START ), sStageTime( 0 ), nStage( 0 ),
	pMission( _pMission ), pTransition( _pTransition ), szDialogCode( _szDialogCode ), unitsSet( _unitsSet ), phrasesSet( _phrasesSet ),
	bindCancel( "cancel" ), bindNext( "next" ), bindPrev( "back" ), nID( _nID )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMissionDlgUI::ShowDesktop()
{
	eStage = START;
	nStage = 0;

	csSystem << "WARNING: Dialog start!" << endl;

	ShowWindow( SWTYPE_SHOW );
	pMission->PushDesktop( this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMissionDlgUI::HideDesktop()
{
	// retail @0x205660: once the dialog is ALREADY closing (eStage >= FINISH) a skip/ESC is a no-op. Without this
	// guard, pressing ESC during the MOVEUNITVIEWOUT/FADEOUT stages snaps eStage back to FINISH and UpdateDesktop
	// restarts the heads' slide-out, so holding ESC keeps re-triggering it and the dialog never ends.
	if ( eStage < FINISH )
	{
		eStage = FINISH;
		// retail: closing/skipping the dialog stops the active voiceline (pSound CObj release) AND the
		// speaking head's lipsync (pSequenceHolder clear) immediately -- without this the head keeps
		// silently lipsyncing through the slide-out and after.
		pSound = 0;
		if ( IsValid( pSequenceHolder ) )
		{
			pSequenceHolder->SetSequence( 0 );
			pSequenceHolder = 0;
		}
		csSystem << "WARNING: Dialog end!" << endl;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMissionDlgUI::UpdateDesktop( const STime &sTime )
{
	switch( eStage )
	{
	case START:
		{
			eStage = FADEIN;
			sStageTime = sTime;
			nPanelsStateSave = pMission->GetPanelState( NGame::PANEL_ALL );
			pMission->SetPanelState( NGame::PANEL_ALL, false );
			UpdatePhrases( GetInterface()->GetView() );
			StartDialog();
		}
	case FADEIN:
		{
			if ( sStageTime + N_FADE_STAGE_TIME > sTime )
			{
				float fCoeff = float( sTime - sStageTime ) / N_FADE_STAGE_TIME;
				pTopBackground->SetColor( NGfx::SPixel8888( 0, 0, 0, 0xFF * fCoeff ) );
				pBottomBackground->SetColor( NGfx::SPixel8888( 0, 0, 0, 0xFF * fCoeff ) );
				break;
			}

			eStage = MOVEUNITVIEWIN;
			sStageTime = sTime;
			pTransition->SetStyle( STYLE_VISIBLE, false );
			pTopBackground->SetColor( NGfx::SPixel8888( 0, 0, 0, 0xFF ) );
			pBottomBackground->SetColor( NGfx::SPixel8888( 0, 0, 0, 0xFF ) );
			for ( int nTemp = 0; nTemp < Min( unitViewsSet.size(), unitsSet.size() ); nTemp++ )
			{
				unitViewsSet[nTemp]->SetStyle( STYLE_VISIBLE, true );
				unitViewsSet[nTemp]->SetLight( NDb::GetTAmbientLight( 113 ) );
			}
		}
	case MOVEUNITVIEWIN:
		{
			if ( sStageTime + N_MOVEUNITVIEW_STAGE_TIME > sTime )
			{
				float fCoeff = float( sTime - sStageTime ) / N_MOVEUNITVIEW_STAGE_TIME;
				for ( int nTemp = 0; nTemp < Min( unitViewsSet.size(), unitsSet.size() ); nTemp++ )
					unitViewsSet[nTemp]->SetCoeff( fCoeff );
				break;
			}

			eStage = SHOWDIALOG;
			sStageTime = sTime;
			pDialog->SetStyle( STYLE_VISIBLE, true );
			for ( int nTemp = 0; nTemp < Min( unitViewsSet.size(), unitsSet.size() ); nTemp++ )
				unitViewsSet[nTemp]->SetCoeff( 1.0f );
			SetStage( 0 );
		}
	case SHOWDIALOG:
		{
			break;
		}
	case FINISH:
		{
			eStage = MOVEUNITVIEWOUT;
			sStageTime = sTime;
			pBack->SetStyle( STYLE_VISIBLE, false );
			pNext->SetStyle( STYLE_VISIBLE, false );
			pExit->SetStyle( STYLE_VISIBLE, false );
			pDialog->SetStyle( STYLE_VISIBLE, false );
		}
	case MOVEUNITVIEWOUT:
		{
			if ( sStageTime + N_MOVEUNITVIEW_STAGE_TIME > sTime )
			{
				float fCoeff = 1.0f - float( sTime - sStageTime ) / N_MOVEUNITVIEW_STAGE_TIME;
				for ( int nTemp = 0; nTemp < Min( unitViewsSet.size(), unitsSet.size() ); nTemp++ )
					unitViewsSet[nTemp]->SetCoeff( fCoeff );
				break;
			}

			eStage = FADEOUT;
			sStageTime = sTime;
			pTransition->SetStyle( STYLE_VISIBLE, true );
			for ( int nTemp = 0; nTemp < Min( unitViewsSet.size(), unitsSet.size() ); nTemp++ )
				unitViewsSet[nTemp]->SetCoeff( 0.0f );
		}
	case FADEOUT:
		{
			if ( sStageTime + N_FADE_STAGE_TIME > sTime )
			{
				float fCoeff = 1.0f - float( sTime - sStageTime ) / N_FADE_STAGE_TIME;
				pTopBackground->SetColor( NGfx::SPixel8888( 0, 0, 0, 0xFF * fCoeff ) );
				pBottomBackground->SetColor( NGfx::SPixel8888( 0, 0, 0, 0xFF * fCoeff ) );
				break;
			}

			EndDialog();
			pMission->SetPanelState( nPanelsStateSave, true );
			pMission->PopDesktop( this );
			return;
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NGame::CUICmdExec* CMissionDlgUI::CreateExecutor( NWorld::CUICmd *pCmd )
{
	return NGame::CreateExecutor( pCmd, pMission );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMissionDlgUI::ProcessEvent( const NInput::SEvent &sEvent )
{
	if ( bindCancel.ProcessEvent( sEvent ) )
	{
		HideDesktop();
		return true;
	}

	if ( bindNext.ProcessEvent( sEvent ) )
	{
		if ( eStage != SHOWDIALOG )
			return true;

		SetStage( nStage + 1 );
	}
	else if ( bindPrev.ProcessEvent( sEvent ) )
	{
		if ( eStage != SHOWDIALOG )
			return true;

		SetStage( nStage - 1 );
	}

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMissionDlgUI::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOAD:
		{
			pBack = new CHoverButton( sEvent.pLoader->GetControl( "back" ) );
			pBack->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 11204 ) + GetDBString( 11207 ) );
			pBack->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 11203 ) + GetDBString( 11207 ) );
			pBack->AddTextState( CHoverButton::STATE_DISABLED, GetDBString( 11205 ) + GetDBString( 11207 ) );

			pNext = new CHoverButton( sEvent.pLoader->GetControl( "next" ) );
			pNext->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 11204 ) + GetDBString( 11206 ) );
			pNext->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 11203 ) + GetDBString( 11206 ) );
			pNext->AddTextState( CHoverButton::STATE_DISABLED, GetDBString( 11205 ) + GetDBString( 11206 ) );

			pExit = new CHoverButton( sEvent.pLoader->GetControl( "cancel" ) );
			pExit->AddTextState( CHoverButton::STATE_HOVER, GetDBString( 11204 ) + GetDBString( 11208 ) );
			pExit->AddTextState( CHoverButton::STATE_NORMAL, GetDBString( 11203 ) + GetDBString( 11208 ) );
			pExit->AddTextState( CHoverButton::STATE_DISABLED, GetDBString( 11205 ) + GetDBString( 11208 ) );

			unitViewsSet.resize( 4 );
			unitViewsSet[0] = new CAnimUnitView( sEvent.pLoader->GetControl( "unitview1" ), pMission->GetRenderGame(), -N_DISPLACE_DISTANCE, NDb::GetDBCamera( N_UNITCAMERA_LEFT ) );
			unitViewsSet[1] = new CAnimUnitView( sEvent.pLoader->GetControl( "unitview2" ), pMission->GetRenderGame(), N_DISPLACE_DISTANCE, NDb::GetDBCamera( N_UNITCAMERA_RIGHT ) );
			unitViewsSet[2] = new CAnimUnitView( sEvent.pLoader->GetControl( "unitview3" ), pMission->GetRenderGame(), -N_DISPLACE_DISTANCE, NDb::GetDBCamera( N_UNITCAMERA_LEFT ) );
			unitViewsSet[3] = new CAnimUnitView( sEvent.pLoader->GetControl( "unitview4" ), pMission->GetRenderGame(), N_DISPLACE_DISTANCE, NDb::GetDBCamera( N_UNITCAMERA_RIGHT ) );

			pDialog = new CText( sEvent.pLoader->GetControl( "dialogtext" ) );
			break;
		}
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pTopBackground = GetUIWindow<CImage>( this, "top_background" );
			pBottomBackground = GetUIWindow<CImage>( this, "bottom_background" );
			break;
		}
	}

	return CDesktopWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMissionDlgUI::SetStage( int _nStage )
{
	if ( _nStage >= parsedPhrasesSet.size() )
	{
		ASSERT( 0 );
		return;
	}

	nStage = _nStage;

	const SAckEvent &sEvent = parsedPhrasesSet[nStage];

	pDialog->SetText( GetDBString( 11209 ) + sEvent.wsText );

	for ( int nTemp = 0; nTemp < Min( unitViewsSet.size(), unitsSet.size() ); nTemp++ )
	{
		if ( unitsSet[nTemp].GetPtr() != sEvent.pUnit.GetPtr() )
		{
			unitViewsSet[nTemp]->SetLight( NDb::GetTAmbientLight( 113 ) );
			continue;
		}

		unitViewsSet[nTemp]->SetLight( NDb::GetTAmbientLight( 7 ) );
		if ( IsValid( sEvent.pSound ) )
		{
			// retail @0x2059c0: STORE the voiceline handle in the pSound member -- reassigning the CObj releases the
			// prior handle, STOPPING the previous section's voiceline. Continuation pages of the SAME phrase carry a
			// null pSound (UpdatePhrases first-page gate, retail bVar9), so paging "Next" through a long subtitle
			// keeps the one voiceline playing instead of restarting it.
			NSound::ISoundScene *pScene = GetInterface()->GetSound();
			if ( IsValid( pScene ) )
				pSound = pScene->Add2DSound( sEvent.pSound );
		}
		if ( IsValid( sEvent.pSequence ) )
		{
			// retail @0x2059c0: stop the PREVIOUS speaker's head before starting the new phrase's
			// lipsync -- without this a skipped voiceline keeps silently lipsyncing. Continuation
			// pages (null pSequence) skip this block, so in-phrase paging never cuts the mouth.
			if ( IsValid( pSequenceHolder ) )
				pSequenceHolder->SetSequence( 0 );
			unitViewsSet[nTemp]->SetSequence( sEvent.pSequence, sEvent.pExpression );
			pSequenceHolder = unitViewsSet[nTemp];
		}
	}

	bool bBegPhrase = nStage == 0;
	bool bEndPhrase = nStage == parsedPhrasesSet.size() - 1;
	pBack->SetStyle( STYLE_VISIBLE, !bBegPhrase );
	pExit->SetStyle( STYLE_VISIBLE, bEndPhrase );
	pNext->SetStyle( STYLE_VISIBLE, !bEndPhrase );
	////
	pMission->Command( new NWorld::CCmdCallScriptFunction( "OnDialogPhrase", "si", szDialogCode.c_str(), nStage ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMissionDlgUI::UpdatePhrases( NGScene::I2DGameView *pView )
{
	parsedPhrasesSet.reserve( phrasesSet.size() );

	CObj<IML> pML = CreateML();
	for ( int nTemp = 0; nTemp < phrasesSet.size(); nTemp++ )
	{
		NWorld::CAckEvent *pEvent = phrasesSet[nTemp];
		if ( !IsValid( pEvent->pUnit ) )
			continue;
		if ( !IsValid( pEvent->pAckInfo ) )
			continue;

		// The UNIT's chosen voice (NRPG::CUnit::nVoice via GetRPGUnit()->GetVoice()), not the persona's preset, so
		// the player's FaceGen voice choice is heard in dialogs (retail indexes voices[] by GetRPGUnit()'s nVoice).
		const NDb::SAckVoice &sVoice = pEvent->pAckInfo->voices[pEvent->pUnit->GetRPG()->GetRPGUnit()->GetVoice()];

		SPoint sRealSize( 0, 0 );
		wstring wsText = GetDBString( 11209 ) + GetDBString( pEvent->pAckInfo->pText );
		// retail UpdatePhrases @0x206d60 (the bVar9 gate): the sound/lipsync/expression attach to the
		// FIRST page of each phrase only; continuation pages carry nulls so SetStage neither restarts
		// the voiceline nor re-triggers the head on "Next".
		bool bFirstPage = true;
		NDb::CSequence *pExpression = NDb::GetSequenceByExpression( sVoice.eExpression );
		do
		{
			pML->SetText( wsText, 0 );
			pML->Generate( pView, pDialog->GetSize().x );

			sRealSize = pML->GetSize();
			CVec2 vScreenRect = pView->GetViewportSize();
			sRealSize.x = sRealSize.x * 1024 / vScreenRect.x;
			sRealSize.y = sRealSize.y * 768 / vScreenRect.y;

			if ( sRealSize.y > pDialog->GetSize().y )
			{
				list<SRect> rects;
				pML->Render( &rects, SPoint( 0, 0 ), SRect( 0, 0, 0, 0 ) );

				int nCutChar = 0;
				for ( list<SRect>::const_iterator iRect = rects.begin(); iRect != rects.end(); iRect++ )
				{
					float fY = iRect->y2 * 768 / vScreenRect.y;
					if ( iRect->y2 > pDialog->GetSize().y )
						break;

					nCutChar++;
				}

				if ( nCutChar == 0 )
				{
					csSystem << "ERROR: Text-line height bigger then dialog size!" << endl;
					ASSERT( 0 && "Text size bigger then dialog size!" );
					return;
				}

				int nTemp = 0, nCursor = 0;
				while( nCursor < wsText.length() )
				{
					WCHAR wcChar = wsText.c_str()[nCursor];

					if ( wcChar == '<' )
					{
						int nFind = wsText.find_first_of( '>', nCursor );
						if ( nFind != wstring::npos )
							nCursor = nFind + 1;
						else
							nCursor = wsText.length();

						continue;
					}

					nTemp++;
					nCursor++;
					if ( nTemp == nCutChar )
					{
						SAckEvent &sEvent = *parsedPhrasesSet.insert( parsedPhrasesSet.end(), SAckEvent());
						sEvent.pUnit = pEvent->pUnit;
						sEvent.wsText = wsText.substr( 0, nCursor );
						sEvent.nPriority = pEvent->nPriority;
						if ( bFirstPage )
						{
							sEvent.pSound = sVoice.pSound;
							sEvent.pSequence = sVoice.pSequence;
							sEvent.pExpression = pExpression;
							bFirstPage = false;
						}

						wsText = wsText.substr( nCursor );
					}
				}
			}
			else
			{
				SAckEvent &sEvent = *parsedPhrasesSet.insert( parsedPhrasesSet.end(), SAckEvent());
				sEvent.pUnit = pEvent->pUnit;
				sEvent.wsText = wsText;
				sEvent.nPriority = pEvent->nPriority;
				if ( bFirstPage )
				{
					sEvent.pSound = sVoice.pSound;
					sEvent.pSequence = sVoice.pSequence;
					sEvent.pExpression = pExpression;
					bFirstPage = false;
				}
			}

		} while( !wsText.empty() && ( sRealSize.y > pDialog->GetSize().y ) );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMissionDlgUI::StartDialog()
{
	for ( int nTemp = 0; nTemp < Min( unitViewsSet.size(), unitsSet.size() ); nTemp++ )
		unitViewsSet[nTemp]->SetUnit( unitsSet[nTemp] );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMissionDlgUI::EndDialog()
{
	// Release the DialogPlay wait id (the release CScript id-queue): post CCmdInterfaceEvent(nID) so
	// WaitForUI(DialogPlay(...)) unblocks. nID == -1 for a dialog not started via the script (no waiter).
	if ( nID >= 0 )
		pMission->Command( new NWorld::CCmdInterfaceEvent( nID ) );
	pMission->Command( new NWorld::CCmdCallScriptFunction( "OnDialogFinished", "s", szDialogCode.c_str() ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB1211180, CMissionDlgUI )
REGISTER_SAVELOAD_CLASS( 0xB14b2180, CAnimUnitView )