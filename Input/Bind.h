#ifndef __BIND_H__
#define __BIND_H__
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "Input.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NInput
{
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SBindCommand;
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EMappingType
{
	MTYPE_EVENT,
	MTYPE_EVENT_UP,
	MTYPE_SLIDER,
	MTYPE_SLIDER_MINUS,
	MTYPE_UNKNOWN
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SBind
{
	string szSection;
	EMappingType eType;
	vector<string> controlsSet;
};
struct SEvent
{
	SMessage mMessage;
	vector<SBindCommand*> commands;
};
class CBind
{
	float fDelta;
	SBindCommand* pBindCommand;
public:
	CBind( const string &sCmd );

	bool IsActive();
	float GetDelta();
	float GetSpeed();

	bool ProcessEvent( const NInput::SEvent &eEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
const string& GetSection();
void SetSection( const string &_szSection, bool bUpdate = true );
////
void Bind( const string &szCmd, const SBind &sCmdBind );
void Unbind( const string &szCmd );
void GetBind( const string &szCmd, list<SBind> *pRes );
void UpdateBinds();
////
float GetControlCoeff( const string &szControl );
void SetControlCoeff( const string &szControl, float fCoeff );
// SetCommandCoeff @0x3d0880 -- set a bound COMMAND's coeff (per-command, not per-control). Scales that
// command's fDelta (Bind.cpp: fDelta = nValue * SCommand::fCoeff / 100000). This is the hook the retail
// camera sensitivity/invert config drives (UpdateCameraFromConfig @0xcd180, Camera.cpp).
void SetCommandCoeff( const string &szCommand, float fCoeff );
////
bool GetEvent( SEvent *sEvent );
void PostEvent( const string &sEvent );
////////////////////////////////////////////////////////////////////////////////////////////////////
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
