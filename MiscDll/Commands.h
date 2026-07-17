#ifndef __A5_GLOBAL_H__
#define __A5_GLOBAL_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#define REGISTER_CMD( var, func ) NGlobal::RegisterCmd( var, func );
#define REGISTER_VAR( var, func, defval, save ) NGlobal::RegisterVar( var, func, 0, defval, save );
#define REGISTER_VAR_EX( var, func, cont, defval, save ) NGlobal::RegisterVar( var, func, cont, defval, save );
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGlobal
{
////////////////////////////////////////////////////////////////////////////////////////////////////
struct CValue
{
private:
	float fVal;
	wstring szVal;

public:
	CValue();
	CValue( float fVal );
	CValue( const wstring &szVal );

	float GetFloat() const;
	int GetInt() const;      // v1.2 (new helper, v1.2 VA 0x7ce990): FLD/FISTP round-to-nearest
	const wstring& GetString() const;

	operator float () const { return fVal; }
	operator wstring () const { return szVal; }
};
typedef void (*VarHandler)( const string &szID, const CValue &sValue, void *pContext );
typedef void (*CmdHandler)( const string &szID, const vector<wstring> &paramsSet, void *pContext );
////////////////////////////////////////////////////////////////////////////////////////////////////
void RegisterCmd( const string &szID, CmdHandler pHandler = 0, void *pContext = 0 );
void RegisterVar( const string &szID, VarHandler pHandler = 0, void *pContext = 0, const CValue &sValue = CValue(), bool bSave = false );
void UnregisterCmd( const string &szID );
void UnregisterVar( const string &szID );
void GetIDList( vector<string> *pList );
////
const CValue& GetVar( const string &szID, const CValue &sDefault = CValue() );
void SetVar( const string &szVar, const CValue &sValue );
void ResetVar( const string &szVar );    // @0x3d4c80 -- restore the var's registered default value
////
void ProcessCommand( const wstring &szCmd );
void LoadConfig( const string &szFileName );
void SaveConfig( const string &szFileName );
////
void VarBoolHandler( const string &szID, const NGlobal::CValue &sValue, void *pContext );
void VarIntHandler( const string &szID, const NGlobal::CValue &sValue, void *pContext );   // retail @0x3d4350
void VarFloatHandler( const string &szID, const NGlobal::CValue &sValue, void *pContext ); // retail @0x3d4370
////////////////////////////////////////////////////////////////////////////////////////////////////
class CCmd
{
	void *pContext;
	string szID;
	CmdHandler pHandler;

public:
	CCmd( const string &szID, CmdHandler pHandler, void *pContext );
	~CCmd();

	void Run( const vector<wstring> &paramsSet );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CVar
{
	string szID;

public:
	CVar( const string &szID, VarHandler pHandler, void *pContext, const CValue &sValue = CValue(), bool bSave = false );
	~CVar();

	const CValue& Get();
	void Set( const CValue &sValue );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif