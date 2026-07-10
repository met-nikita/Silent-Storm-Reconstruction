#ifndef __INPUT_H__
#define __INPUT_H__
////////////////////////////////////////////////////////////////////////////////////////////////////
typedef DWORD STime;
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NInput
{
////////////////////////////////////////////////////////////////////////////////////////////////////
	enum EPOVAxis
	{
		PA_UNKNOWN,
		PA_X,
		PA_Y
	};
	enum EControlType
	{
		CT_KEY,
		CT_POV,
		CT_AXIS,
		CT_TIME,
		CT_LIMAXIS,
		CT_UNKNOWN,
		// Win32 message-derived control types (retail carries these at 0/1 and shifts
		// the rest down; we APPEND them instead so the existing CT_* ordinals -- read
		// symbolically throughout the DirectInput path and never serialized -- stay put.
		// See AddWinMessage: CT_WIN_CHAR carries a translated WM_CHAR wide char, CT_WIN_KEY
		// a WM_KEYDOWN virtual-key. The numeric value is immaterial (all uses are symbolic).
		CT_WIN_CHAR,
		CT_WIN_KEY
	};
////////////////////////////////////////////////////////////////////////////////////////////////////
	struct SMessage
	{
		int nAction;
		EPOVAxis ePOVAxis;
		EControlType cType;

		int nParam;
		bool bState;
		STime tTime;
	};
////////////////////////////////////////////////////////////////////////////////////////////////////
	bool InitInput( HWND hWnd, bool bNonExclusiveMode = false, int nSampleBufferSize = -1 );
	bool DoneInput();

	void PumpMessages( bool bFocus );
	bool GetMessage( SMessage *pMsg );
	// Push a synthesised Win32-message-derived input onto the NInput queue (the
	// Win32->NInput bridge that gives held keys OS auto-repeat). @0x3ccb00.
	void AddWinMessage( EControlType cType, int nParam );
	bool GetCharForKey( int nVirtualKey, WCHAR *pwcChar );
	bool GetKeyForMessage( const SMessage &mMsg, int *pnVirtualKey );
		
	int GetControlID( const string &sCommand );
	void GetControlInfo( int nAction, EControlType *pcType, float *pfGranularity );

	void StartSaveInput( CDataStream *pStream );
	void StopSaveInput();
	void StartEmulateInput( CDataStream *pStream );
	void StopEmulateInput();
////////////////////////////////////////////////////////////////////////////////////////////////////
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
