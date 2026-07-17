#ifndef __IACTION_DECORATOR_H_
#define __IACTION_DECORATOR_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "iMission.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CActionDecorator
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class Type>
class CActionDecorator: public Type
{
protected:
	// protected (not private): NUI::CSlot's version-gated serialization (retail CSlot::OnSerialize
	// @0x1c5c90) assigns this pMission (the decorator's, CSlot+0x84) directly on legacy v0 loads.
	ZDATA_(Type)
	bool bMouseEnter;
	CPtr<NGame::IMission> pMission;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(Type*)this); f.Add(2,&bMouseEnter); f.Add(3,&pMission); return 0; }

public:
	typedef CActionDecorator<Type> TBaseClass;

public:
	CActionDecorator() {}
	CActionDecorator( const SWindowInfo &sInfo, NGame::IMission *_pMission ):
		Type( sInfo ), pMission( _pMission ), bMouseEnter( false ) {}

	virtual bool CanHandleState( NGame::IState *pState ) const = 0;
	virtual CObjectBase* GetTarget() = 0;

	bool ProcessMessage( const SEvent &sEvent )
	{
		switch( sEvent.nEvent )
		{
		case EVENT_ACTIVATE:
			{
				if ( sEvent.nVal & EAF_DEACTIVATE )
					bMouseEnter = false;
				break;
			}
		case EVENT_MOUSEENTER:
			{
				bMouseEnter = true;
				break;
			}
		case EVENT_MOUSEEXIT:
			{
				bMouseEnter = false;
				break;
			}
		}

		// retail (@0x1ef530 for the CInteractiveUnitView instantiation): the state's handler returns
		// whether it consumed the event. Swallow ONLY then -- and reset the mouse capture. An
		// unhandled event still reaches the base, which is what lets CInteractiveUnitView arm its
		// spin on LBUTTONDOWN while CanHandleState is broadly true.
		if ( CanHandleState( pMission->GetState() ) )
		{
			bool bHandled = false;
			switch( sEvent.nEvent )
			{
			case EVENT_LBUTTONUP:
				bHandled = pMission->GetState()->OnLButtonUp( sEvent.nX, sEvent.nY );
				break;
			case EVENT_LBUTTONDOWN:
				bHandled = pMission->GetState()->OnLButtonDown( sEvent.nX, sEvent.nY );
				break;
			case EVENT_LBUTTONDBLCLK:
				bHandled = pMission->GetState()->OnLButtonDblClk( sEvent.nX, sEvent.nY );
				break;
			}
			if ( bHandled )
			{
				GetInterface()->ResetMouseCapture();
				return true;
			}
			if ( sEvent.nEvent == EVENT_MOUSEMOVE )
				GetInterface()->SetCursorInfo( pMission->GetState()->GetCursorInfo() );
		}

		return Type::ProcessMessage( sEvent );
	}
	void Update( const STime &sTime, NGScene::I2DGameView *pView )
	{
		// retail @0x1ef4c0: claim the state target only if this window can handle the CURRENT state.
		// Ungated, an item icon under the cursor claims the target during a drag even though it
		// declines the state, and the drop is classified GROUND instead of hitting the cell beneath.
		if ( GetStyle( STYLE_VISIBLE ) && bMouseEnter && CanHandleState( pMission->GetState() ) )
			pMission->SetStateTarget( GetTarget() );

		Type::Update( sTime, pView );
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
