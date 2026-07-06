#include "StdAfx.h"
#include "G2DView.h"
#include "wInterface.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataAck.h"
#include "..\DBFormat\DataInterface.h"
#include "Sound.h"
#include "iMission.h"
#include "Interface.h"
#include "iDesktopWindow.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const int
	N_ASK_TTL = 3000;
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAckEvent
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail NUI::CAckEvent::Set @0x1d0cf0: flip bReady so the deferred voice+lipsync can fire, and
// arm the TTL floor. pEvent (the world-side ack) is seeded by the ctor now, not here.
void CAckEvent::Set( const STime &sTime )
{
	bReady = true;
	bComplete = false;
	sEndTime = sTime + N_ASK_TTL;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAckEvent::Cancel()
{
	sEndTime = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const STime& CAckEvent::GetEndTime() const
{
	return sEndTime;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NWorld::CAckEvent* CAckEvent::GetAckEvent() const
{
	return pEvent;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAckEvent::SetEndTime( const STime &_sEndTime )
{
	sEndTime = _sEndTime;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDesktopWindow
////////////////////////////////////////////////////////////////////////////////////////////////////
CDesktopWindow::CDesktopWindow( const SWindowInfo &sInfo ):
	CWindow( sInfo )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CWindow* CDesktopWindow::GetClientWindow() const
{
	return pClientWindow;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDesktopWindow::PlayAck( NWorld::CAckEvent *pEvent )
{
	// retail @0x1d0e60: a new ack is accepted only if STRICTLY higher priority than both the
	// active and the queued one -- an equal-priority bark arriving while one plays is DROPPED.
	// The dev's strict-greater test let every equal-priority bark REPLACE the current one each
	// segment: constant churn, no cooldown, and the portrait state machine re-armed forever.
	if ( IsValid( pActiveEvent ) && ( pActiveEvent->GetAckEvent()->nPriority >= pEvent->nPriority ) )
	{
		return;
	}
	if ( IsValid( pNextAckEvent ) && ( pNextAckEvent->nPriority >= pEvent->nPriority ) )
	{
		return;
	}
	pNextAckEvent = pEvent;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CDesktopWindow::ProcessEvent( const NInput::SEvent &sEvent )
{
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CDesktopWindow::ProcessMessage( const SEvent &sEvent )
{
	switch( sEvent.nEvent )
	{
	case EVENT_TEMPLATELOADCOMPLETE:
		{
			pClientWindow = GetUIWindow<CWindow>( this, "view" );
			break;
		}
	}

	return CWindow::ProcessMessage( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDesktopWindow::Draw( const STime &sTime, NGScene::I2DGameView *pView )
{
	if ( IsValid( pNextAckEvent ) )
	{
		pActiveEvent = PlayAckEvent( sTime, pNextAckEvent );
		pNextAckEvent = 0;
	}
	// retail CDesktopWindow::Draw @0x1d0ef0: retire the active ack only once it is truly COMPLETE
	// (bReady && TTL elapsed && the voice has finished), not on a fixed end-time -- the deferred
	// voice may start well after the event was queued (once the speaker's face is on screen).
	if ( IsValid( pActiveEvent ) && pActiveEvent->IsComplete( sTime ) )
		pActiveEvent = 0;

	CWindow::Draw( sTime, pView );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB1228130, CDesktopWindow );
REGISTER_SAVELOAD_CLASS( 0xB1228131, CAckEvent );
