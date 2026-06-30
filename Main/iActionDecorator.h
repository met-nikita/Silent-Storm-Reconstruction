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
private:
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

		if ( CanHandleState( pMission->GetState() ) )
		{
			switch( sEvent.nEvent )
			{
			case EVENT_LBUTTONUP:
				pMission->GetState()->OnLButtonUp( sEvent.nX, sEvent.nY );
				return true;
			case EVENT_LBUTTONDOWN:
				pMission->GetState()->OnLButtonDown( sEvent.nX, sEvent.nY );
				return true;
			case EVENT_LBUTTONDBLCLK:
				pMission->GetState()->OnLButtonDblClk( sEvent.nX, sEvent.nY );
				return true;
			case EVENT_MOUSEMOVE:
				{
					GetInterface()->SetCursorInfo( pMission->GetState()->GetCursorInfo() );
					break;
				}
			}
		}

		return Type::ProcessMessage( sEvent );
	}
	void Update( const STime &sTime, NGScene::I2DGameView *pView )
	{
		if ( GetStyle( STYLE_VISIBLE ) && bMouseEnter )
			pMission->SetStateTarget( GetTarget() );

		Type::Update( sTime, pView );
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
