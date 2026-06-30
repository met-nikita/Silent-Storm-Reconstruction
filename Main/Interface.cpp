#include "StdAfx.h"
#include "GSceneUtils.h"
#include "RectLayout.h"
#include "GView.h"
#include "G2DView.h"
#include "..\Input\Bind.h"
#include "Interface.h"
#include "UIBaseCtrls.h"
#include "UICommCtrls.h"
#include "Console.h"
#include "Cursor.h"
#include "Transform.h"
#include "..\Misc\StrProc.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataInterface.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const int N_DEFAULT_CURSOR = 292;	// UITexture "Normal" (arrow); 202 was "HitLocation - Head". Kept in sync with UIInterface.cpp.
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMouseCaptureHandler
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMouseCaptureHandler: public CObjectBase
{
	OBJECT_NOCOPY_METHODS(CMouseCaptureHandler)
private:
	ZDATA
	bool bMouseCover;
	CPtr<IWindow> pWindow;
	CPtr<IInterface> pInterface;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&bMouseCover); f.Add(3,&pWindow); f.Add(4,&pInterface); return 0; }

public:
	CMouseCaptureHandler() {}
	CMouseCaptureHandler( IInterface *pInterface, IWindow *pWindow );
	~CMouseCaptureHandler();

	IWindow* GetWindow() const;

	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CMouseCaptureHandler::CMouseCaptureHandler( IInterface *_pInterface, IWindow *_pWindow ): 
	pInterface( _pInterface ), bMouseCover( true )
{
	CPtr<IWindow> pParent = _pWindow->GetParent();
	if ( IsValid( pParent ) )
	{
		list< CPtr<IWindow> > childerList;
		pParent->GetChildrenList( &childerList );
		for ( list< CPtr<IWindow> >::iterator iTemp = childerList.begin(); iTemp != childerList.end(); iTemp++ )
		{
			if ( (*iTemp)->IsSame( _pWindow ) )
			{
				pWindow = iTemp->GetPtr();
				break;
			}
		}
	}
	else
		pWindow = _pWindow;

	if ( !IsValid( pWindow ) )
	{
		ASSERT( 0 );
		pWindow = _pWindow;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CMouseCaptureHandler::~CMouseCaptureHandler()
{
	if ( IsValid( pInterface ) && IsValid( pWindow ) )
		pWindow->ProcessMessage( SEvent( EVENT_MOUSECAPTURELOSE ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IWindow* CMouseCaptureHandler::GetWindow() const
{
	return pWindow;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMouseCaptureHandler::ProcessMessage( const SEvent &sEvent )
{
	ASSERT( IsValid( pWindow ) );

	switch( sEvent.nEvent )
	{
		case EVENT_MOUSEMOVE:
		{
			if ( pWindow->HitTest( sEvent.nX, sEvent.nY ) )
			{
				if ( !bMouseCover )
				{
					bMouseCover = true;
					pWindow->ProcessMessage( SEvent( EVENT_MOUSEENTER, sEvent.nX, sEvent.nY ) );
				}
			}
			else
			{
				if ( bMouseCover )
				{
					bMouseCover = false;
					pWindow->ProcessMessage( SEvent( EVENT_MOUSEEXIT, sEvent.nX, sEvent.nY ) );
				}
			}
			break;
		}
		case EVENT_LBUTTONUP:
		case EVENT_LBUTTONDOWN:
		case EVENT_LBUTTONDBLCLK:
		case EVENT_RBUTTONUP:
		case EVENT_RBUTTONDOWN:
		case EVENT_RBUTTONDBLCLK:
			if ( pWindow->ProcessMessage( sEvent ) )
				return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NUI;
REGISTER_SAVELOAD_CLASS( 0xB2841121, CLoader );
REGISTER_SAVELOAD_CLASS( 0xB2841122, CInterface );
REGISTER_SAVELOAD_CLASS( 0xB2841123, CMouseCaptureHandler );
