#include "StdAfx.h"
#include "MapEdit.h"
#include "..\ADOImport\BasicDB.h"

#include "TemplDBCmd.h"
#include "TreeDBCmd.h"
#include "VariantsDBCmd.h"

string gszDBServer;
const LPCWSTR GetDBProvider()
{
  USES_CONVERSION;
  static wchar_t pszwStr[2048];
  const char REG_DB_KEY[] =  "DB";

  gszDBServer = theApp.GetProfileString( "", REG_DB_KEY, "a5server"  );
  string szConnection = NDatabase::GetDBConnectionStr( gszDBServer.c_str() );
/*
  szConnection = "Provider=Microsoft.Jet.OLEDB.4.0;Data Source=";
  szConnection += "W:\\Data\\game.mdb";
  szConnection += ";Persist Security Info=False";
*/
//	szConnection = "DRIVER=SQL Server;SERVER=192.168.0.115;UID=Admin;PWD=;WSID=ALEXANDERS;DATABASE=A5GameSQL;Network=DBMSSOCN;Address=192.168.0.115,1433";
	//szConnection = "DRIVER=SQL Server;SERVER=a5server;UID=sa;PWD=simple;DATABASE=test_game;";
	//szConnection = "DRIVER=SQL Server;SERVER=sabelnika;UID=sa;PWD=simple;DATABASE=A5GAME;";
  wcscpy( pszwStr, A2W( szConnection.c_str() ) );

  return pszwStr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT InitDB( CSession &session, CDBPropSet &propset )
{
  CDataSource connection;
/*  
  dbinit.AddProperty(DBPROP_INIT_DATASOURCE, DB_NAME );
  dbinit.AddProperty(DBPROP_INIT_DATASOURCE, pszDSN );
  dbinit.AddProperty(DBPROP_INIT_PROMPT, (short)4);
*/
  propset.AddProperty(DBPROP_IRowsetChange, true);
  propset.AddProperty(DBPROP_UPDATABILITY, DBPROPVAL_UP_CHANGE | DBPROPVAL_UP_INSERT | DBPROPVAL_UP_DELETE);
	propset.AddProperty( DBPROP_ISequentialStream, true );
	propset.AddProperty( DBPROP_IStream, true, DBPROPOPTIONS_OPTIONAL );
  propset.AddProperty(DBPROP_INIT_LCID, (long)1049);
	//propset.AddProperty(DBPROP_CLIENTCURSOR, true);
	
  HRESULT hr = connection.OpenFromInitializationString( SysAllocString( GetDBProvider() ) );
//  HRESULT hr = connection.Open( HWND(0) );
//  HRESULT hr = connection.Open( _T("MSDASQL"), &dbinit);
  if (FAILED(hr))
  {
    DisplayOLEDBErrorRecords( hr );
    return hr;
  }
  hr = session.Open(connection);
  if ( FAILED(hr)  )
  {
    DisplayOLEDBErrorRecords( hr );
    return hr;
  }
  return S_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
externA5 void InitParticleInstancesDB( SDBConnection *pConnection );
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT OpenDBConnection( SDBConnection *pConnection )
{
	HRESULT hr = InitDB( pConnection->session, pConnection->propset );
	InitParticleInstancesDB( pConnection );
	return hr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Вывод сообщения об ошибке при работе с базой данных
////////////////////////////////////////////////////////////////////////////////////////////////////
void _cdecl OLEDBErrorMessageBox(LPCSTR lpszFormat, ...) 
{ 
    va_list args; 
    va_start(args, lpszFormat); 

    int nBuf; 
    char szBuffer[2048]; 

    nBuf = _vsnprintf(szBuffer, sizeof(szBuffer), lpszFormat, args); 
    ATLASSERT(nBuf < sizeof(szBuffer)); //Output truncated as it was > sizeof(szBuffer) 

//		if ( !theApp.GetMainWnd() || !IsWindow( theApp.GetMainWnd()->m_hWnd ) )
			::MessageBox( 0, szBuffer, _T("OLE DB Error Message"), MB_OK );
//		else
//			theApp.GetMainWnd()->MessageBox( szBuffer, _T("OLE DB Error Message"), MB_OK );
    va_end(args); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void DisplayOLEDBErrorRecords( HRESULT hrErr = S_OK ) 
{
  CDBErrorInfo ErrorInfo; 
  ULONG        cRecords; 
  HRESULT      hr; 
  ULONG        i; 
  CComBSTR     bstrDesc, bstrHelpFile, bstrSource; 
  GUID         guid; 
  DWORD        dwHelpContext; 
  WCHAR        wszGuid[40]; 
  USES_CONVERSION; 
  
  // If the user passed in an HRESULT then trace it 
//  if (hrErr != S_OK) 
//    OLEDBErrorMessageBox( _T("OLE DB Error Record dump for hr = 0x%x\n"), hrErr); 
  
  LCID lcLocale = GetSystemDefaultLCID(); 
  
  hr = ErrorInfo.GetErrorRecords(&cRecords); 
  if (FAILED(hr) && ErrorInfo.m_spErrorInfo == NULL) 
  { 
    OLEDBErrorMessageBox( _T("No OLE DB Error Information found: hr = 0x%x\n"), hrErr ); 
  } 
  else 
  { 
    for (i = 0; i < cRecords; i++) 
    { 
      hr = ErrorInfo.GetAllErrorInfo(i, lcLocale, &bstrDesc, &bstrSource, &guid, 
        &dwHelpContext, &bstrHelpFile); 
      if (FAILED(hr)) 
      { 
        OLEDBErrorMessageBox( 
          _T("OLE DB Error Record dump retrieval failed: hr = 0x%x\n"), hr ); 
        return; 
      } 
      StringFromGUID2(guid, wszGuid, sizeof(wszGuid) / sizeof(WCHAR)); 
      OLEDBErrorMessageBox( 
        _T("Source:\"%s\"\nDescription:\"%s\"\nHelp File:\"%s\"\nHelp Context:%4d\nGUID:%s\n"), 
        OLE2T(bstrSource), OLE2T(bstrDesc), OLE2T(bstrHelpFile), dwHelpContext, OLE2T(wszGuid)); 
      bstrSource.Empty(); 
      bstrDesc.Empty(); 
      bstrHelpFile.Empty(); 
    } 
  } 
} 
