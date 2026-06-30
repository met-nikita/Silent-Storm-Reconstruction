#ifndef __A5_UI_EVENTS_H__
#define __A5_UI_EVENTS_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
	class CUIControl;
	class CUIContainer;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CLoader;
class CWindow;
////////////////////////////////////////////////////////////////////////////////////////////////////
// Event types
////////////////////////////////////////////////////////////////////////////////////////////////////
const int
	EVENT_FLAG_NOTIFY						= 0x01000000,	// Only target window
	EVENT_FLAG_ACTIVE						= 0x02000000,	// Only active window receive this message
	EVENT_FLAG_HITTEST					= 0x04000000,	// Window under cursor
	EVENT_FLAG_BROADCAST				= 0x08000000,	// All windows
	EVENT_FLAG_PARENTNOTIFY			= 0x10000000;	// All parents
////////////////////////////////////////////////////////////////////////////////////////////////////
// Events
////////////////////////////////////////////////////////////////////////////////////////////////////
// System 
const int
	EVENT_ACTIVATE							= 0x00000001 | EVENT_FLAG_NOTIFY,
	EVENT_ACTIVATEREQ						= 0x00000002 | EVENT_FLAG_NOTIFY,
	EVENT_TEMPLATELOAD					= 0x00000003 | EVENT_FLAG_NOTIFY,
	EVENT_TEMPLATECREATE				= 0x00000004 | EVENT_FLAG_NOTIFY,
	EVENT_TEMPLATELOADCOMPLETE	= 0x00000005 | EVENT_FLAG_NOTIFY,
	EVENT_NOTIFY								= 0x00000006 | EVENT_FLAG_PARENTNOTIFY,
// Keyboard											
	EVENT_CHAR									= 0x00000020 | EVENT_FLAG_ACTIVE,
// Mouse
	EVENT_MOUSEMOVE							= 0x00000030 | EVENT_FLAG_HITTEST, 	// Updates cursor & tooltip etc
	EVENT_LBUTTONUP							= 0x00000031 | EVENT_FLAG_HITTEST | EVENT_FLAG_ACTIVE,
	EVENT_LBUTTONDOWN						= 0x00000032 | EVENT_FLAG_HITTEST | EVENT_FLAG_ACTIVE,
	EVENT_LBUTTONDBLCLK					= 0x00000033 | EVENT_FLAG_HITTEST | EVENT_FLAG_ACTIVE,
	EVENT_RBUTTONUP							= 0x00000034 | EVENT_FLAG_HITTEST | EVENT_FLAG_ACTIVE,
	EVENT_RBUTTONDOWN						= 0x00000035 | EVENT_FLAG_HITTEST | EVENT_FLAG_ACTIVE,
	EVENT_RBUTTONDBLCLK					= 0x00000036 | EVENT_FLAG_HITTEST | EVENT_FLAG_ACTIVE,
	EVENT_SCROLL								= 0x00000037 | EVENT_FLAG_HITTEST,
	EVENT_MOUSEENTER						= 0x00000038 | EVENT_FLAG_NOTIFY,
	EVENT_MOUSEEXIT							= 0x00000039 | EVENT_FLAG_NOTIFY,
// Mouse capture
	EVENT_MOUSECAPTURELOSE			= 0x00000040 | EVENT_FLAG_NOTIFY;
////////////////////////////////////////////////////////////////////////////////////////////////////
// Flags
////////////////////////////////////////////////////////////////////////////////////////////////////
const int
// Activate flags
	EAF_ACTIVATE							= 0x000000001,
	EAF_DEACTIVATE						= 0x000000002,
	EAF_ACTIVATENOTIFY				= EAF_ACTIVATE | 0x10000000,
	EAF_DEACTIVATENOTIFY			= EAF_DEACTIVATE | 0x10000000;
////////////////////////////////////////////////////////////////////////////////////////////////////
// Event structure
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
//! Event structure
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SEvent
{
	int nEvent;

	int nX, nY, nVal;
	bool bFlag;
	float fVal;
	string szID;
	CPtr<CLoader> pLoader;
	CPtr<CWindow> pWindow;
	CPtr<NDb::CUIControl> pControl;
	CPtr<NDb::CUIContainer> pContainer;


	SEvent( int _nEvent ): nEvent( _nEvent ), nX( 0 ), nY( 0 ), nVal( 0 ), bFlag( false ) {};
	SEvent( int _nEvent, int _nVal ): nEvent( _nEvent ), nVal( _nVal ), nX( 0 ), nY( 0 ), bFlag( false ) {};
	SEvent( int _nEvent, int _nX, int _nY ): nEvent( _nEvent ), nX( _nX ), nY( _nY ), nVal( 0 ), bFlag( false ) {};
	SEvent( int _nEvent, int _nX, int _nY , float _fVal ): nEvent( _nEvent ), fVal( _fVal ), nX( _nX ), nY( _nY ), bFlag( false ) {};
	SEvent( int _nEvent, CWindow *_pWindow ): nEvent( _nEvent ), pWindow( _pWindow ), nX( 0 ), nY( 0 ), nVal( 0 ), bFlag( false ) {};
	SEvent( int _nEvent, const string &_szID ): nEvent( _nEvent ), szID( _szID ), nX( 0 ), nY( 0 ), nVal( 0 ), bFlag( false ) {};
	SEvent( int _nEvent, CLoader *_pLoader, NDb::CUIControl *_pControl ): nEvent( _nEvent ), pLoader( _pLoader ), pControl( _pControl ), nX( 0 ), nY( 0 ), nVal( 0 ), bFlag( false ) {};
	SEvent( int _nEvent, CLoader *_pLoader, NDb::CUIContainer *_pContainer ): nEvent( _nEvent ), pLoader( _pLoader ), pContainer( _pContainer ), nX( 0 ), nY( 0 ), nVal( 0 ), bFlag( false ) {};
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
