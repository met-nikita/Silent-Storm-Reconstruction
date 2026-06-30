// Export.cpp : implementation file
//
#include "stdafx.h"
#include "mapedit.h"
#include "Export.h"
#include "ItemsMgr.h"
#include "dbDefs.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataSound.h"
#include "..\Main\GGeometry.h"
#include "..\Main\GObjectInfo.h"
#include "..\Main\GTexture.h"
#include "..\Main\GfxBuffers.h"
#include "..\Main\GParticleFormat.h"
#include "..\Main\mmpformat.h"

#include <process.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
string GetExportSrcDir()
{
  return (LPCTSTR)theApp.GetProfileString( "", REG_EXPORT_SRC, "W:\\Art\\ForComplete\\" );
//	return (LPCTSTR)theApp.GetProfileString( "", REG_EXPORT_SRC, "C:\\A5\\ForComplete\\" );
}
string GetExportDstDir()
{
  return (LPCTSTR)theApp.GetProfileString( "", REG_EXPORT_DST, "W:\\complete\\" );
//	return (LPCTSTR)theApp.GetProfileString( "", REG_EXPORT_DST, "C:\\A5\\" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
string MakeDoubleSlash( string str )
{
  for ( string::iterator it = str.begin(); it != str.end(); ++it )
    if ( *it == '\\' )
    {
      int l = 1;
      while ( ++it != str.end() && *it == '\\' )
        ++l;
      if ( l < 2 )
      {
        str.insert( it, 1, '\\' );
        return MakeDoubleSlash( str );
      }
      if ( it == str.end() )
        return str;
    }
  return str;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
string MakeOneSlash( string str )
{
  for ( string::iterator it = str.begin(); it != str.end(); ++it )
    if ( *it == '\\' )
    {
      if ( ++it == str.end() )
        return str;
      if ( *it == '\\' )
      {
        str.erase( it );
        return MakeOneSlash( str );
      }
    }
    return str;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
string IToA( int n )
{
  char buf[32];

  itoa( n, buf, 10 );
  return buf;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static string GetBatFile( FILE **ppFile, const string &szBat )
{
  *ppFile = 0;
  char szBatPath[512];
  const static int nBatSz = szBat.length();
 
  // Получаем имя бат файла, в кторый записываются команды экспорта  
  if ( GetTempPath( sizeof(szBatPath), szBatPath ) > sizeof(szBatPath) )
  {
		HWND hwnd = theApp.GetMainWnd() ? theApp.GetMainWnd()->m_hWnd : 0;
    MessageBox( hwnd, GetResString( IDS_ERR_GENERIC ).c_str(), 
      GetResString( IDS_ERR_EXPORT_CAPT ).c_str(), MB_OK | MB_ICONWARNING );
    return "";
  }
  strcat( szBatPath, szBat.c_str() );
  *ppFile = fopen( szBatPath, "w" );
  if ( !*ppFile )
  {
		HWND hwnd = theApp.GetMainWnd() ? theApp.GetMainWnd()->m_hWnd : 0;
    MessageBox( hwnd, GetResString( IDS_ERR_EXPORT_BAT ).c_str(), 
      GetResString( IDS_ERR_EXPORT_CAPT ).c_str(), 
      MB_OK | MB_ICONWARNING );
    return "";
  }
  return szBatPath;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SortBySrcName( CItemsMgr *pItems, vector<int> *pnItemIDs )
{
	unordered_map<string, vector<int> > srcnamemap;

	for ( int i = 0; i < pnItemIDs->size(); ++i )
	{
		const CPropMap* pProps = pItems->GetPropList( (*pnItemIDs)[i] );
		if ( !pProps )
			continue;
		CPropMap::const_iterator it = pProps->find( "SrcName" );
		if ( pProps->end() == it )
			return;
		srcnamemap[(string)it->second->GetValue()].push_back( (*pnItemIDs)[i] );
	}
	pnItemIDs->clear();
	for ( unordered_map<string, vector<int> >::const_iterator it = srcnamemap.begin(); it != srcnamemap.end(); ++it )
		pnItemIDs->insert( pnItemIDs->end(), it->second.begin(), it->second.end() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void ErrorBox( const string &szCaption, const string &szName )
{
  char buf[512];
  
  DWORD dwErr = GetLastError();
  FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM, 0, dwErr, 0, buf, sizeof(buf), 0 );
  string msg = szName + "\n" + buf;
	HWND hwnd = theApp.GetMainWnd() ? theApp.GetMainWnd()->m_hWnd : 0;
  MessageBox( hwnd, msg.c_str(), szCaption.c_str(), MB_OK | MB_ICONWARNING );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool CheckSrcDst( string szSrc, string szDst, bool bForceExport )
{
	szSrc = MakeOneSlash( szSrc );
	szDst = MakeOneSlash( szDst );
	bool bRet = false;
  BY_HANDLE_FILE_INFORMATION srcInfo, dstInfo;
	HANDLE hSrc = CreateFile( szSrc.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0 );
	HANDLE hDst = CreateFile( szDst.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0 );
	if ( INVALID_HANDLE_VALUE == hSrc )
	{
		ErrorBox( GetResString( IDS_ERR_EXPORT_SRC ), szSrc );
    return bRet;
	}
  //if ( !GetFileAttributesEx( szSrc.c_str(), GetFileExInfoStandard, &srcInfo ) )
	if ( !GetFileInformationByHandle( hSrc, &srcInfo ) )
  {
		ErrorBox( GetResString( IDS_ERR_EXPORT_SRC ), szSrc );
    return bRet;
  }
  
  //if ( GetFileAttributesEx( szDst.c_str(), GetFileExInfoStandard, &dstInfo ) )
	if ( INVALID_HANDLE_VALUE != hDst && GetFileInformationByHandle( hDst, &dstInfo ) )
  {
    ULARGE_INTEGER srcTm = { srcInfo.ftLastWriteTime.dwLowDateTime, srcInfo.ftLastWriteTime.dwHighDateTime };
    ULARGE_INTEGER dstTm = { dstInfo.ftLastWriteTime.dwLowDateTime, dstInfo.ftLastWriteTime.dwHighDateTime };
    if ( srcTm.QuadPart > dstTm.QuadPart || bForceExport )
			bRet = true;
  }
	else
		bRet = true;
	CloseHandle( hSrc );
	CloseHandle( hDst );
  return bRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CopyLastVersion()
{
	NGScene::CloseAllResources();
  string szMakeCurrent = GetExportDstDir() + "makeCurrent.bat";
  int ret = _spawnlp( _P_WAIT, szMakeCurrent.c_str(), szMakeCurrent.c_str(), 0 );
  if ( -1 == ret )
  {
		HWND hwnd = theApp.GetMainWnd() ? theApp.GetMainWnd()->m_hWnd : 0;
    MessageBox( hwnd, strerror( errno ), GetResString( IDS_ERR_EXPORT_COPY ).c_str() , MB_OK | MB_ICONWARNING );
    return;
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
typedef bool (*MAKE_CMD_PTR)( int nID, CItemsMgr*, char *pszCmdBuf, int nSize, string *pszLastSrc, bool bForceExport );
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool MkGeomExportCmd( int nID, CItemsMgr *pItems, char *pszCmdBuf, int nSize, string *pszLastSrc, bool bForceExport )
{
  pszCmdBuf[0] = '\0';
  const CPropMap *pProps = pItems->GetPropList( nID );
  if ( !pProps )
    return false;
	const char *pName = pItems->GetItemName( nID );
	string szItemName = pName ? pName : "";
  CPropMap::const_iterator itSN = pProps->find( "SrcName" );
  if ( itSN == pProps->end() )
    return false;

  string szName   = itSN->second->GetValue();

  string szID = IToA( nID );
	string szGeomDir = MakeDoubleSlash( GetExportDstDir() + "Geometries\\\\" );
  string szSrc     = MakeDoubleSlash( GetExportSrcDir() + "Models\\\\" + szName );
  string szGeomDst = szGeomDir + szID;
	string szTriDst  = MakeDoubleSlash( GetExportDstDir() + "Binds\\\\" );
	string szLocatorsDst  = MakeDoubleSlash( GetExportDstDir() + "Locators\\\\" );
  
  if ( !CheckSrcDst( szSrc, szGeomDst, bForceExport ) )
    return false;

	int nOffset = 0;
	// удаляем старые ресурсы
	for ( int i = 0; i < 4; ++i )
	{
		nOffset += _snprintf( pszCmdBuf + nOffset, nSize, "sysFile -del \"%s\\%d\";\n", szGeomDir.c_str(), (i << 16) + nID );
	}
  // Model
  CPropMap::const_iterator itRJ = pProps->find( "RootJoint" );
  if ( itRJ == pProps->end() )
    return false;
  string szRootJoint = itRJ->second->GetValue();
  string szType = "mesh";// : "skin";

	if ( !pszLastSrc || (pszLastSrc && szSrc != *pszLastSrc) )
	{
		nOffset += _snprintf( pszCmdBuf + nOffset, nSize, "\nprint \"%s\n\";\n \
			file -o -f \"%s\";\n triangulateAll;\n", szSrc.c_str(), szSrc.c_str() );
	}
	_snprintf( pszCmdBuf + nOffset, nSize, "print \"%s:\n\";select -cl;\n select \"%s\";\n \
		file -es -f -typ \"A5ExportModel\" -op \"%s=1;binds_path=%s;eff_path=%s\" \"%s\";\n",
		szItemName.c_str(), szRootJoint.c_str(), szType.c_str(), 
		szTriDst.c_str(), szLocatorsDst.c_str(), szGeomDst.c_str() );

	*pszLastSrc = szSrc;
  pItems->ReleasePropList( pProps );
  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool MkAIGeomExportCmd( int nID, CItemsMgr *pItems, char *pszCmdBuf, int nSize, string *pszLastSrc, bool bForceExport )
{
  pszCmdBuf[0] = '\0';
  const CPropMap *pProps = pItems->GetPropList( nID );
  if ( !pProps )
    return false;
	const char *pName = pItems->GetItemName( nID );
	string szItemName = pName ? pName : "";
  CPropMap::const_iterator itSN = pProps->find( "SrcName" );
  if ( itSN == pProps->end() )
    return false;
  
  string szName   = itSN->second->GetValue();
  
  string szID = IToA( nID );
  string szSrc = MakeDoubleSlash( GetExportSrcDir() + "Models\\\\" + szName );
  string szDst = MakeDoubleSlash( GetExportDstDir() + "AIGeometries\\\\" + szID );
	string szBinds = MakeDoubleSlash( GetExportDstDir() + "AIBinds\\\\" );
  
  if ( !CheckSrcDst( szSrc, szDst, bForceExport ) )
    return false;
  
  // AI Model
  CPropMap::const_iterator itRJ = pProps->find( "RootJoint" );
  if ( itRJ == pProps->end() )
    return false;
  string szRootJoint = itRJ->second->GetValue();  
	string szType = "ai_mesh";// : "ai_skin";

	int nOffset = 0;
	if ( !pszLastSrc || (pszLastSrc && szSrc != *pszLastSrc) )
	{
		nOffset = _snprintf( pszCmdBuf, nSize, "print \"%s\n\";\n \
			file -o -f \"%s\";\n triangulateAll;\n", szSrc.c_str(), szSrc.c_str() );
	}
  _snprintf( pszCmdBuf + nOffset, nSize, "print \"%s:\n\";select -cl;\n select -ne \"%s\";\n \
    file -es -f -typ \"A5ExportModel\" -op \"%s=1;binds_path=%s;\" \"%s\";\n",
    szItemName.c_str(), szRootJoint.c_str(), szType.c_str(), szBinds.c_str(), 
		szDst.c_str() );

  pItems->ReleasePropList( pProps );
	*pszLastSrc = szSrc;
  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool MayaExport( CItemsMgr *pItems, vector<int> nItemIDs, bool bForceExport, MAKE_CMD_PTR pCmdFunc, const char *pszPlugin = 0 )
{
  ASSERT( pItems );
  ASSERT( pCmdFunc );

  FILE *pFile;
  string szBatPath = GetBatFile( &pFile, "maya.mel" );
  if ( !pFile )
    return false;
	if ( !pszPlugin )
		fputs( "loadPlugin \"W:\\\\Tools\\\\A5ExportModel.mll\";\n", pFile );
	else
		fputs( (string( "loadPlugin \"" ) + pszPlugin + "\";\n").c_str(), pFile );
  char szCmd[10240];
  bool bNeedExport = false;

	string szLastSrc = "";		
  for ( int i = 0; i < nItemIDs.size(); ++i )
  {
    bNeedExport = (*pCmdFunc)( nItemIDs[i], pItems, szCmd, sizeof(szCmd), &szLastSrc, bForceExport ) || bNeedExport;
    if ( EOF == fputs( szCmd, pFile ) )
    {
			HWND hwnd = theApp.GetMainWnd() ? theApp.GetMainWnd()->m_hWnd : 0;
      MessageBox( hwnd, GetResString( IDS_ERR_EXPORT_BAT ).c_str(), 
        GetResString( IDS_ERR_EXPORT_CAPT ).c_str(), 
        MB_OK | MB_ICONWARNING );
      fclose( pFile );
      return false;
    }
  }
	bool bGUI = theApp.GetProfileInt( "", REG_EXPORT_GUI, 0 );
	if ( bGUI )
		fputs( "\nquit -a;", pFile );
  fclose( pFile );
  
  if ( bNeedExport )
  {
    string szScript = string("-script ") + "\"" + szBatPath + "\"";
    string szLog = string(" -log ") + GetExportDstDir() + "Logs\\\\" + "maya.log";
		string szBatch = bGUI ?	"" : " -batch ";
		string szMaya = string("maya.exe ") + szBatch + szScript + szLog;
    //int ret =  _spawnlp( _P_WAIT, "maya", "maya.exe", szBatch.c_str(), szScript.c_str(), szLog.c_str(), 0 );
		int ret =  system( (string("start /min /wait ") + szMaya).c_str() );
    if ( -1 == ret )
    {
			string szErr =  string( "maya" ) + szBatch + szScript + " " + szLog 
										+ "\n\n" + strerror( errno );
			HWND hwnd = theApp.GetMainWnd() ? theApp.GetMainWnd()->m_hWnd : 0;
      MessageBox( hwnd, szErr.c_str(), GetResString( IDS_ERR_EXPORT_CAPT ).c_str() , MB_OK | MB_ICONWARNING );
      return false;
    }
    CExportResultDlg dlg( GetExportDstDir() + "Logs\\\\" + "maya.log" );
    dlg.DoModal();
  }
  else
  {
		HWND hwnd = theApp.GetMainWnd() ? theApp.GetMainWnd()->m_hWnd : 0;
    MessageBox( hwnd, GetResString( IDS_EXPORT_EMPTY ).c_str(), 
      GetResString( AFX_IDS_APP_TITLE ).c_str(), 
      MB_OK | MB_ICONWARNING );
		return false;
  }
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool MkSkeletonExportCmd( int nID, CItemsMgr *pItems, char *pszCmdBuf, int nSize, string *pszLastSrc, bool bForceExport )
{
  pszCmdBuf[0] = '\0';
  const CPropMap *pProps = pItems->GetPropList( nID );
  if ( !pProps )
    return false;
  CPropMap::const_iterator itSN = pProps->find( "SrcName" );
	CPropMap::const_iterator itMSR = pProps->find( "MSRFormat" );
  if ( itSN == pProps->end() || itMSR == pProps->end() )
    return false;
  
  string szName = itSN->second->GetValue();
	bool bMSR = itMSR->second->GetValue();
  
  string szID = IToA( nID );
  string szSrc = MakeDoubleSlash( GetExportSrcDir() + "Models\\\\" + szName );
  string szDst = MakeDoubleSlash( GetExportDstDir() + "Skeletons\\\\" + szID );
  
  if ( !CheckSrcDst( szSrc, szDst, bForceExport ) )
    return false;
  
  CPropMap::const_iterator itRJ = pProps->find( "RootJoint" );
  if ( itRJ == pProps->end() )
    return false;
  string szRootJoint = itRJ->second->GetValue();
  
  _snprintf( pszCmdBuf, nSize, "print \"%s\n\";\n \
              file -o -f \"%s\";\n select -cl;\n select \"%s\";\n \
              file -es -f -typ \"A5ExportModel\" -op \"skeleton=1;scale=%d;\" \"%s\";\n",
              szSrc.c_str(), szSrc.c_str(), szRootJoint.c_str(), bMSR, szDst.c_str() );
  pItems->ReleasePropList( pProps );
  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool MkAnimationExportCmd( int nID, CItemsMgr *pItems, char *pszCmdBuf, int nSize, string *pszLastSrc, bool bForceExport )
{
  pszCmdBuf[0] = '\0';
  const CPropMap *pProps = pItems->GetPropList( nID );
  if ( !pProps )
    return false;
  CPropMap::const_iterator itSN  = pProps->find( "SrcName" );
	CPropMap::const_iterator itPers  = pProps->find( "PersName" );
  CPropMap::const_iterator itSN2  = pProps->find( "SrcName2" );
	CPropMap::const_iterator itPers2  = pProps->find( "PersName2" );
	CPropMap::const_iterator itFPS = pProps->find( "FPS" );
	CPropMap::const_iterator itStart = pProps->find( "StartFrame" );
	CPropMap::const_iterator itEnd   = pProps->find( "EndFrame" );
	CPropMap::const_iterator itAllFr = pProps->find( "DefaultFrames" );
	CPropMap::const_iterator e = pProps->end();
  if ( e == itSN || e == itFPS || itPers == e || e == itStart || e == itEnd || e == itAllFr )
    return false;
  
  string szName = MakeDoubleSlash( (string)itSN->second->GetValue() );
	string szName2 = MakeDoubleSlash( (string)itSN2->second->GetValue() );
  
  string szID = IToA( nID );
  string szSrc = MakeDoubleSlash( GetExportSrcDir() + "Models\\\\" + szName );
	string szSrc2 = MakeDoubleSlash( GetExportSrcDir() + "Models\\\\" + szName2 );
  string szDst = MakeDoubleSlash( GetExportDstDir() + "Animations\\\\" + szID );
	string szPersName = itPers->second->GetValue();
	string szPersName2 = itPers2->second->GetValue();
  
  if ( !CheckSrcDst( szSrc, szDst, bForceExport ) && !CheckSrcDst( szSrc2, szDst, bForceExport ) )
    return false;
  
  // откапываем скелет для анимации
  CPropMap::const_iterator itSk = pProps->find( "SkeletonID" );
  if ( itSk == pProps->end() )
    return false;
  int nSkeletonID = itSk->second->GetValue();
  const SResTree *pSkeletons = theApp.GetResTree( IDC_SKELETONS_TREE );
  if ( !pSkeletons || !pSkeletons->pItemsTree )
    return false;
  //
  const CPropMap *pSkProps = pSkeletons->pItemsTree->GetPropList( nSkeletonID );
  //
  CPropMap::const_iterator itSkSN = pSkProps->find( "SrcName" );
	CPropMap::const_iterator itSkMSR = pSkProps->find( "MSRFormat" );
  if ( itSkSN == pSkProps->end() || itSkMSR == pSkProps->end() )
    return false;
  string szSkeletSrc = MakeDoubleSlash( GetExportSrcDir() + "Models\\\\" + (string)itSkSN->second->GetValue() );
	bool bMSR = itSkMSR->second->GetValue();
  //
  CPropMap::const_iterator itSkRJ = pSkProps->find( "RootJoint" );
  if ( itSkRJ == pSkProps->end() )
    return false;
  string szRootJoint = itSkRJ->second->GetValue();  
	string szClip2 = szName2 == "" ? "" : "loadClip \"" + szSrc2 + "\" \"" + szPersName2 +"\";\n";
  
  
  _snprintf( pszCmdBuf, nSize, "print \"%s\n\";\nprint \"%s\n\";\n\n \
file -o -f \"%s\";\n \
loadClip \"%s\" \"%s\";\n\
%s\
select -cl;\n select \"%s\";\n \
file -es -f -typ \"A5ExportModel\" -op \"animation=1;fps=%d;start=%d;end=%d;all_frames=%d;scale=%d;\" \"%s\";\n",
    szSkeletSrc.c_str(), szSrc.c_str(), szSkeletSrc.c_str(), szSrc.c_str(), szPersName.c_str(), szClip2.c_str(), szRootJoint.c_str(), 
		(int)itFPS->second->GetValue(), (int)itStart->second->GetValue(), 
		(int)itEnd->second->GetValue(), (int)itAllFr->second->GetValue(), bMSR, szDst.c_str() );
  pItems->ReleasePropList( pProps );
	pItems->ReleasePropList( pSkProps );
  return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool MkParticlesExportCmd( int nID, CItemsMgr *pItems, char *pszCmdBuf, int nSize, string *pszLastSrc, bool bForceExport )
{
  pszCmdBuf[0] = '\0';
  const CPropMap *pProps = pItems->GetPropList( nID );
  if ( !pProps )
    return false;
  CPropMap::const_iterator itSN = pProps->find( "SrcName" );
	CPropMap::const_iterator itEP = pProps->find( "ExportPrefix" );
  if ( itSN == pProps->end() || itEP == pProps->end() )
    return false;
	
  string szName   = itSN->second->GetValue();
	string szExportPrefix = itEP->second->GetValue();
	
  string szID = IToA( nID );
  string szSrc = MakeDoubleSlash( GetExportSrcDir() + szName );
  string szDst = MakeDoubleSlash( GetExportDstDir() + "Effects\\\\" + szID );
  
  if ( !CheckSrcDst( szSrc, szDst, bForceExport ) )
    return false;
	
		int nOffset = _snprintf( pszCmdBuf, nSize, "print \"%s\n\";\n \
			file -o -f \"%s\";\n string $list[] = `ls -type objectSet \"%s*\"`;\n \
			select -cl;\nfor ($item in $list)\nselect -add -ne $item;\n \
			file -es -f -typ \"A5ExportModel\" -op \"particles=1;\" \"%s\";\n",
			szSrc.c_str(), szSrc.c_str(), szExportPrefix.c_str(), szDst.c_str() );
		
		pItems->ReleasePropList( pProps );
		return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool MkLightExportCmd( int nID, CItemsMgr *pItems, char *pszCmdBuf, int nSize, string *pszLastSrc, bool bForceExport )
{
  pszCmdBuf[0] = '\0';
  const CPropMap *pProps = pItems->GetPropList( nID );
  if ( !pProps )
    return false;
  CPropMap::const_iterator itSN = pProps->find( "SrcName" );
	CPropMap::const_iterator itNode = pProps->find( "SelectNode" );
  if ( itSN == pProps->end() || itNode == pProps->end() )
    return false;
	
  string szName = itSN->second->GetValue();
	string szNode = itNode->second->GetValue();
	
  string szID = IToA( nID );
  string szSrc = MakeDoubleSlash( GetExportSrcDir() + szName );
  string szDst = MakeDoubleSlash( GetExportDstDir() + "Lights\\\\" + szID );
  
  if ( !CheckSrcDst( szSrc, szDst, bForceExport ) )
    return false;
	
  _snprintf( pszCmdBuf, nSize, "print \"%s\n\";\n \
              file -o -f \"%s\";\n select -cl;\n select \"%s\";\n \
              file -es -f -typ \"A5ExportModel\" -op \"light=1;\" \"%s\";\n",
              szSrc.c_str(), szSrc.c_str(), szNode.c_str(), szDst.c_str() );
		
		pItems->ReleasePropList( pProps );
		return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void ExportGeometry( CItemsMgr *pItems, vector<int> nItemIDs, bool bForceExport )
{
	SortBySrcName( pItems, &nItemIDs );
  MayaExport( pItems, nItemIDs, bForceExport, &MkGeomExportCmd );	
	GeometryUpdateDBData( pItems, nItemIDs );
	NDatabase::Refresh<NDb::CGeometry>();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void GeometryUpdateDBData( CItemsMgr *pItems, const vector<int> &nItemIDs )
{
	for ( int i = 0; i < nItemIDs.size(); ++i )
	{
		const CPropMap *pProps = pItems->GetPropList( nItemIDs[i] );
		if ( !pProps )
			continue;
		CPropMap::const_iterator ix = pProps->find( "SizeX" );
		CPropMap::const_iterator iy = pProps->find( "SizeY" );
		CPropMap::const_iterator iz = pProps->find( "SizeZ" );
		CPropMap::const_iterator icx = pProps->find( "CenterX" );
		CPropMap::const_iterator icy = pProps->find( "CenterY" );
		CPropMap::const_iterator icz = pProps->find( "CenterZ" );
		CPropMap::const_iterator e = pProps->end();
		if ( e == ix || e == iy || e == iz || e == icx || e == icy || e == icz )
			break;
		
		CVec3 ptHalf = VNULL3;
		CVec3 ptCenter = VNULL3;
		int nParts = 0;
		for ( int j = 0; j < NDb::N_MODEL_MATERIALS; ++j )
		{
			CPtr<NGScene::CObjectInfoLoader> pOInfo = new NGScene::CObjectInfoLoader;
			pOInfo->SetKey( NGScene::SPartKey( nItemIDs[i], j ) );
			CDGPtr<CPtrFuncBase<NGScene::CObjectInfo> > pFunc = pOInfo;
			pFunc.Refresh();
			NGScene::CObjectInfo *pInfo = pFunc->GetValue();
			while ( !pInfo )
			{
				Sleep( 0 );
				pInfo = pFunc->GetValue();
			}
			if ( !pInfo || pInfo->GetVertices().empty() )
				continue;
			SBound bound;
			pInfo->CalcBound( &bound );
			ptHalf.x = Max( ptHalf.x, bound.ptHalfBox.x );
			ptHalf.y = Max( ptHalf.y, bound.ptHalfBox.y );
			ptHalf.z = Max( ptHalf.z, bound.ptHalfBox.z );
			ptCenter += bound.s.ptCenter;
			++nParts;
		}
		if ( nParts )
			ptCenter /= nParts;
		ix->second->SetValue( ptHalf.x * 2 );
		iy->second->SetValue( ptHalf.y * 2 );
		iz->second->SetValue( ptHalf.z * 2 );
		icx->second->SetValue( ptCenter.x );
		icy->second->SetValue( ptCenter.y );
		icz->second->SetValue( ptCenter.z );
		pItems->ReleasePropList( pProps );
	}	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void ExportAIGeometry( CItemsMgr *pItems, vector<int> nItemIDs, bool bForceExport )
{
  MayaExport( pItems, nItemIDs, bForceExport, &MkAIGeomExportCmd );
	AIGeometryUpdateDBData( pItems, nItemIDs );
	NDatabase::Refresh<NDb::CAIGeometry>();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void ExportSkeletons( CItemsMgr *pItems, vector<int> nItemIDs, bool bForceExport )
{
  MayaExport( pItems, nItemIDs, bForceExport, &MkSkeletonExportCmd );
	NDatabase::Refresh<NDb::CSkeleton>();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void ExportAnimations( CItemsMgr *pItems, vector<int> nItemIDs, bool bForceExport )
{
  MayaExport( pItems, nItemIDs, bForceExport, &MkAnimationExportCmd );
	NDatabase::Refresh<NDb::CAnimation>();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void ExportLights( CItemsMgr *pItems, vector<int> nItemIDs, bool bForceExport )
{
  MayaExport( pItems, nItemIDs, bForceExport, &MkLightExportCmd );
	NDatabase::Refresh<NDb::CAnimation>();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void ExportParticles( CItemsMgr *pItems, vector<int> nItemIDs, bool bForceExport )
{
  MayaExport( pItems, nItemIDs, bForceExport, &MkParticlesExportCmd );	
	ParticlesUpdateDBData( pItems, nItemIDs );
	NDatabase::Refresh<NDb::CParticle>();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void ParticlesUpdateDBData( CItemsMgr *pItems, const vector<int> &nItemIDs )
{
	for ( int i = 0; i < nItemIDs.size(); ++i )
	{
		CPtr<NGScene::CParticlesLoader> pParticle = new NGScene::CParticlesLoader;
		pParticle->SetKey( nItemIDs[i] );
		CDGPtr<CPtrFuncBase<NGScene::CParticlesInfo> > pFunc = pParticle;
		pFunc.Refresh();
		NGScene::CParticlesInfo *pInfo = pFunc->GetValue();
		while ( !pInfo )
		{
			Sleep( 0 );
			pInfo = pFunc->GetValue();
		}
		if ( !pInfo )
			continue;
		SBound bound;
		pInfo->CalcBound( &bound );
		const CPropMap *pProps = pItems->GetPropList( nItemIDs[i] );
		CPropMap::const_iterator ixc = pProps->find( "CenterX" );
		CPropMap::const_iterator iyc = pProps->find( "CenterY" );
		CPropMap::const_iterator izc = pProps->find( "CenterZ" );
		CPropMap::const_iterator ixh = pProps->find( "HalfBoxX" );
		CPropMap::const_iterator iyh = pProps->find( "HalfBoxY" );
		CPropMap::const_iterator izh = pProps->find( "HalfBoxZ" );
		CPropMap::const_iterator e = pProps->end();
		
		if ( e == ixc || e == iyc || e == izc || e == ixh || e == iyh || e == izh )
			return;
		
		ixc->second->SetValue( bound.s.ptCenter.x );
		iyc->second->SetValue( bound.s.ptCenter.y );
		izc->second->SetValue( bound.s.ptCenter.z );
		ixh->second->SetValue( bound.ptHalfBox.x );
		iyh->second->SetValue( bound.ptHalfBox.y );
		izh->second->SetValue( bound.ptHalfBox.z );
		
		pItems->ReleasePropList( pProps );
	}	
	Sleep(0);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void ExportTextures( CItemsMgr *pItems, vector<int> nItemIDs, bool bForceExport )
{
  ASSERT( pItems );
  FILE *pFile;
  string szBatPath = GetBatFile( &pFile, "texture_export.bat" );
  if ( !pFile )
    return;

  // составляем команды для экспорта
  int i, nExportNum = 0;
  const int n = nItemIDs.size();
  for ( i = 0; i < n; ++i )
  {
    const CPropMap *pProps = pItems->GetPropList( nItemIDs[i] );
    if ( !pProps )
      continue;
    CPropMap::const_iterator itSrc = pProps->find( "SrcName" );
    CPropMap::const_iterator itFmt = pProps->find( "Format" );
		CPropMap::const_iterator itType = pProps->find( "Type" );
		CPropMap::const_iterator itMip = pProps->find( "NMips" );
		CPropMap::const_iterator itMSize = pProps->find( "MappingSize" );
		CPropMap::const_iterator itAddr = pProps->find( "AddrType" );
		CPropMap::const_iterator itGain = pProps->find( "BumpGain" );
		CPropMap::const_iterator end = pProps->end();
    if ( itSrc == end || itFmt == end || itType == end || itMip == end || itMSize == end || itAddr == end || itGain == end )
      return;
    string szSrc = MakeOneSlash( GetExportSrcDir() + "Textures\\" + (string)itSrc->second->GetValue() );
    string szDst = MakeOneSlash( GetExportDstDir() + "Textures\\" + IToA( nItemIDs[i] ) );
		string szType = itType->second->GetValue();
		string szAddrType = itAddr->second->GetValue();
		string szFormat = itFmt->second->GetValue();
		int nMips = itMip->second->GetValue();
		float fGain = itGain->second->GetValue();
		float fMSize = float( itMSize->second->GetValue() ) / (fGain > FP_EPSILON ? fGain : FP_EPSILON);
    if ( !CheckSrcDst( szSrc, szDst, bForceExport ) )
      continue;
    ++nExportNum;
    fprintf( pFile, "w:\\tools\\texconv -t%s -f%s -m%d -a%s -s%f \"%s\" %s\n", 
      szType.c_str(), szFormat.c_str(),  nMips, szAddrType.c_str(), fMSize, szSrc.c_str(), szDst.c_str() );
		string szDXTcheck = szFormat.substr( 0, 3 );
		if ( szDXTcheck == "dxt" )
		{
			szDst = MakeDoubleSlash( GetExportDstDir() + "Textures\\" + IToA( nItemIDs[i] | 0x01000000 ) );
	    fprintf( pFile, "w:\\tools\\texconv -t%s -f8888 -m%d -a%s -s%f \"%s\" %s\n", 
		    szType.c_str(), nMips, szAddrType.c_str(), fMSize, szSrc.c_str(), szDst.c_str() );
		}
    pItems->ReleasePropList( pProps );
  }
  //fprintf( pFile, "pause\n" );
  fclose( pFile );
	HWND hwnd = theApp.GetMainWnd() ? theApp.GetMainWnd()->m_hWnd : 0;
  if ( 0 == nExportNum )
  {
    MessageBox( hwnd, GetResString( IDS_EXPORT_EMPTY ).c_str(), 
			GetResString( AFX_IDS_APP_TITLE ).c_str(), MB_OK | MB_ICONWARNING );
    return;
  }
  int ret = _spawnlp( _P_WAIT, szBatPath.c_str(), szBatPath.c_str(), 0 );
  if ( -1 == ret )
    MessageBox( hwnd, strerror( errno ), GetResString( IDS_ERR_EXPORT_CAPT ).c_str() , MB_OK | MB_ICONWARNING );
  else
  {
    string msg = "Exported " + IToA( nExportNum ) + " textures";
    MessageBox( hwnd, msg.c_str(), GetResString( AFX_IDS_APP_TITLE ).c_str() , MB_OK | MB_ICONINFORMATION );
  }
	TextureUpdateDBData( pItems, nItemIDs );
	NDatabase::Refresh<NDb::CTexture>();
	NGScene::CloseAllResources();
	ClearHoldQueue();
	Sleep(0);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void TextureUpdateDBData( CItemsMgr *pItems, const vector<int> &nItemIDs )
{
	for ( int i = 0; i < nItemIDs.size(); ++i )
	{
		const CPropMap *pProps = pItems->GetPropList( nItemIDs[i] );
		if ( !pProps )
			continue;
		CPropMap::const_iterator ix = pProps->find( "Width" );
		CPropMap::const_iterator iy = pProps->find( "Height" );
		CPropMap::const_iterator ic = pProps->find( "AverageColor" );
		CPropMap::const_iterator e = pProps->end();
		if ( e == ix || e == iy || e == ic )
			break;
		//
		CObj<NGScene::CFileRequest> pRequest;
		try
		{
			SMMPFileHeader hdr;
			pRequest = new NGScene::CFileRequest( "Textures", nItemIDs[i] );
			pRequest->Read();
			NGScene::CFileRequest &file = *pRequest;
			file->Read( &hdr, sizeof(hdr) );
			ic->second->SetValue( (int)hdr.dwAverageColor );
			ix->second->SetValue( hdr.nSizeX );
			iy->second->SetValue( hdr.nSizeY );
		}
		catch(...)
		{
		}
		pItems->ReleasePropList( pProps );
	}
	Sleep(0);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void ExportFonts( CItemsMgr *pItems, vector<int> nItemIDs, bool bForceExport )
{
  ASSERT( pItems );
  FILE *pFile;
  string szBatPath = GetBatFile( &pFile, "A5font_export.bat" );
  if ( !pFile )
    return;

  // составляем команды для экспорта
  int nExportNum = 0;
  const int n = nItemIDs.size();
  for ( int i = 0; i < n; ++i )
  {
    const CPropMap *pProps = pItems->GetPropList( nItemIDs[i] );
    if ( !pProps )
      continue;
    CPropMap::const_iterator itTex = pProps->find( "TextureID" );
    CPropMap::const_iterator itH   = pProps->find( "Height" );
    CPropMap::const_iterator itW   = pProps->find( "Thickness" );
    CPropMap::const_iterator itIt  = pProps->find( "Italic" );
    CPropMap::const_iterator itAa  = pProps->find( "Antialiased" );
    CPropMap::const_iterator itPt  = pProps->find( "Pitch" );
    CPropMap::const_iterator itChr = pProps->find( "Charset" );
    CPropMap::const_iterator itFc  = pProps->find( "FaceName" );
    CPropMap::const_iterator ie = pProps->end();
    if ( itTex == ie || itH  == ie || itW == ie || itIt == ie ||
         itAa  == ie || itPt == ie || itChr == ie || itFc == ie )
      continue;
    // Получаем исходнау текстуру для шрифта
    const SResTree *pTexs = theApp.GetResTree( IDC_TEXTURES_TREE );
    if ( !pTexs || !pTexs->pItemsTree )
      continue;
    const CPropMap *pTexProps = pTexs->pItemsTree->GetPropList( itTex->second->GetValue() );
    if ( !pTexProps )
      continue;
    CPropMap::const_iterator itSrc = pTexProps->find( "SrcName" );
    if ( itSrc == pTexProps->end() )
      continue;
    
    string szAa = itAa->second->GetValue() ? "-aa " : " ";
    string szIt = itIt->second->GetValue() ? "-it " : " ";
    string szSrc = MakeDoubleSlash( GetExportSrcDir() + "Textures\\" + (string)itSrc->second->GetValue() );
    string szDst = MakeDoubleSlash( GetExportDstDir() + "Fonts\\" + IToA( nItemIDs[i] ) );

    ++nExportNum;
    fprintf( pFile, "w:\\tools\\fontgen -h%s -w%s %s %s -%s -%s \"%s\" \"%s\" \"%s\" \n", 
      ((string)itH->second->GetValue()).c_str(), 
      ((string)itW->second->GetValue()).c_str(), 
      szAa.c_str(), 
      szIt.c_str(), 
      ((string)itPt->second->GetValue()).c_str(), 
      ((string)itChr->second->GetValue()).c_str(), 
      ((string)itFc->second->GetValue()).c_str(), 
      szDst.c_str(), szSrc.c_str() );
    pItems->ReleasePropList( pProps );
  }
//  fprintf( pFile, "pause\n" );
  fclose( pFile );
	HWND hwnd = theApp.GetMainWnd() ? theApp.GetMainWnd()->m_hWnd : 0;
  if ( 0 == nExportNum )
  {
    MessageBox( hwnd, GetResString( IDS_EXPORT_EMPTY ).c_str(), 
      GetResString( AFX_IDS_APP_TITLE ).c_str(), MB_OK | MB_ICONWARNING );
    return;
  }
  int ret = _spawnlp( _P_WAIT, szBatPath.c_str(), szBatPath.c_str(), 0 );
  if ( -1 == ret )
    MessageBox( hwnd, strerror( errno ), GetResString( IDS_ERR_EXPORT_CAPT ).c_str() , MB_OK | MB_ICONWARNING );
  else
  {
    string msg = "Exported " + IToA( nExportNum ) + " fonts";
    MessageBox( hwnd, msg.c_str(), GetResString( AFX_IDS_APP_TITLE ).c_str() , MB_OK | MB_ICONINFORMATION );
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void ExportSounds( CItemsMgr *pItems, vector<int> nItemIDs, bool bForceExport )
{
  ASSERT( pItems );
  FILE *pFile;
  string szBatPath = GetBatFile( &pFile, "A5sound_export.bat" );
  if ( !pFile )
    return;
	const int n = nItemIDs.size();
	int nExported = 0;
	const string szFlags = bForceExport ? " /Y " : " /Y "; // /D ";
	const string szSrcDir = GetExportSrcDir();
	const string szExportDir = GetExportDstDir() + "Sounds\\";
	
  for ( int i = 0; i < n; ++i )
  {
    const CPropMap *pProps = pItems->GetPropList( nItemIDs[i] );
    if ( !pProps )
      continue;
    CPropMap::const_iterator iSrc = pProps->find( "SrcName" );
		if ( pProps->end() == iSrc )
			break;
		const string szSrc = iSrc->second->GetValue();
		string szCmd = "copy" + szFlags + szSrcDir + szSrc + " " + szExportDir + IToA( nItemIDs[i] ) + "\n";
		fputs( szCmd.c_str(), pFile );
		++nExported;
	}
	fclose( pFile );
  int ret = _spawnlp( _P_WAIT, szBatPath.c_str(), szBatPath.c_str(), 0 );
	NDatabase::Refresh<NDb::CSound>();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
extern string StrToProps( const CPropMap *pProps, const string &szStr );
////////////////////////////////////////////////////////////////////////////////////////////////////
void ExportHeads( CItemsMgr *pItems, vector<int> nItemIDs, bool bForceExport )
{
	const SResTree *pm = theApp.GetResTree( IDC_MATERIALS_TREE );
	const SResTree *pt = theApp.GetResTree( IDC_TEXTURES_TREE );
	if ( !pm || !pt )
		return;
	CItemsMgr *pMaterials = pm->pItemsTree;
	CItemsMgr *pTextures  = pt->pItemsTree;
	//
	for ( int i = 0; i < nItemIDs.size(); ++i )
	{
		int nID = nItemIDs[i];

		const CPropMap *pProps = pItems->GetPropList( nID );
		if ( !pProps )
			continue;
		CPropMap::const_iterator isrc = pProps->find( "SrcName" );
		if ( isrc == pProps->end() )
			continue;
		//
		string szTexOut = GetExportSrcDir() + "Textures\\Heads\\" + IToA( nID ) + '.' + pItems->GetItemName( nID ) + "\\";
		CreateDirectory( szTexOut.c_str(), 0 );
		string szSrc = GetExportSrcDir() + (string)isrc->second->GetValue();
		string szOutput = GetExportDstDir() + "Heads\\" + IToA( nID );
		int ret = _spawnlp( _P_WAIT, "w:\\tools\\lsconverter.exe", "lsconverter.exe", (string( "-t" ) + szTexOut).c_str(), "-g", szSrc.c_str(), szOutput.c_str(), 0 );
		// 
		const int TEXTURE_HEAD_FOLDER = 440;
		const int MATERIAL_HEAD_FOLDER = 221;
		const string szItemPath = IToA( nID ) + '.' + pItems->GetItemPath( nID );
		int nTexFolder = pTextures->DoesFolderExist( szItemPath, TEXTURE_HEAD_FOLDER );
		int nMatFolder = pMaterials->DoesFolderExist( szItemPath, MATERIAL_HEAD_FOLDER );
		if ( nTexFolder <= 0 )
			nTexFolder = pTextures->AddFolder( -1, TEXTURE_HEAD_FOLDER, szItemPath );
		if ( nMatFolder <= 0 )
			nMatFolder = pMaterials->AddFolder( -1, MATERIAL_HEAD_FOLDER, szItemPath );
		//
		for ( int k = 0; k < 1; ++k )
		{
			string szMatID = IToA( k );
			string szMat = "Material" + szMatID;
			CPropMap::const_iterator im = pProps->find( szMat );
			if ( im == pProps->end() )
				continue;
			if ( pMaterials->IsExist( im->second->GetValue() ) )
				continue;
			string szTexSrc = szTexOut + szMatID + ".tga";
			string szSrcName = "Heads\\" + IToA( nID ) + '.' + pItems->GetItemName( nID ) + "\\" + szMatID + ".tga";
			CFileStream fcheck;
			try
			{
				fcheck.OpenRead( szTexSrc.c_str() );
			}
			catch ( ... )
			{
				continue;
			}
			int nTexID = pTextures->AddItem( -1, nTexFolder, szMatID );
			const CPropMap *pTexProps = pTextures->GetPropList( nTexID );
			if ( !pTexProps )
			{
				ASSERT(0);
				continue;
			}
			string szTexProps;
			try
			{
				CFileStream texf;
				texf.OpenRead( (szTexOut + szMatID + ".tex" ).c_str() );
				vector<char> buf( texf.GetSize() + 1 );
				texf.Read( &buf[0], texf.GetSize() );
				buf.back() = '\0';
				szTexProps = &buf[0];
			}
			catch ( ... )
			{
				continue;
			}
			szTexProps += "\nSrcName=";
			szTexProps += szSrcName;
			StrToProps( pTexProps, szTexProps );
			pTextures->SetItemProps( nTexID, pTexProps );
			pTextures->ReleasePropList( pTexProps );
			//
			int nMatID = pMaterials->AddItem( -1, nMatFolder, szMatID );
			const CPropMap *pMatProps = pMaterials->GetPropList( nMatID );
			if ( !pMatProps )
			{
				ASSERT(0);
				continue;
			}
			string szMatProps;
			try
			{
				CFileStream matf;
				matf.OpenRead( (szTexOut + szMatID + ".mat" ).c_str() );
				vector<char> buf( matf.GetSize() + 1 );
				matf.Read( &buf[0], matf.GetSize() );
				buf.back() = '\0';
				szMatProps = &buf[0];
			}
			catch ( ... )
			{
				continue;
			}
			szMatProps += "\nTextureID=";
			szMatProps += IToA( nTexID );
			StrToProps( pMatProps, szMatProps );
			pMaterials->SetItemProps( nMatID, pMatProps );
			pMaterials->ReleasePropList( pMatProps );
			//
			CPropMap::const_iterator imaterial = pProps->find( szMat );
			if ( imaterial != pProps->end() )
				imaterial->second->SetValue( nMatID );
		}
		pItems->ReleasePropList( pProps );
	}
	NDatabase::Refresh<NDb::CTexture>();
	NDatabase::Refresh<NDb::CTMaterial>();
	NDatabase::Refresh<NDb::CMaterial>();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void ExportHeadSeqs( CItemsMgr *pItems, vector<int> nItemIDs, bool bForceExport )
{
	for ( int i = 0; i < nItemIDs.size(); ++i )
	{
		int nID = nItemIDs[i];

		const CPropMap *pProps = pItems->GetPropList( nID );
		if ( !pProps )
			continue;
		CPropMap::const_iterator isrc = pProps->find( "SrcName" );
		if ( isrc == pProps->end() )
			continue;
		//
		string szSrc = GetExportSrcDir() + (string)isrc->second->GetValue();
		string szOutput = GetExportDstDir() + "Sequences\\" + IToA( nID );
		int ret = _spawnlp( _P_WAIT, "w:\\tools\\lsconverter.exe", "lsconverter.exe", "-s", szSrc.c_str(), szOutput.c_str(), 0 );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void ExportAckInfos( CItemsMgr *pItems, vector<int> nItemIDs, bool bForceExport )
{
	const SResTree *pTree = theApp.GetResTree( IDC_HEADSEQS_TREE );
	CItemsMgr *pSeqs = pTree->pItemsTree;
	for ( int i = 0; i < nItemIDs.size(); ++i )
	{
		int nID = nItemIDs[i];

		const CPropMap *pProps = pItems->GetPropList( nID );
		if ( !pProps )
			continue;
		CPropMap::const_iterator ihs = pProps->find( "HeadSequenceID" );
		if ( ihs == pProps->end() )
			continue;
		const CPropMap *pSeqProps = pSeqs->GetPropList( ihs->second->GetValue() );
		if ( !pSeqProps )
		{
			int nSID = pSeqs->AddItem( -1, 1, pItems->GetItemName( nID ) );
			pSeqProps = pSeqs->GetPropList( nSID );
			if ( !pSeqProps )
				return;
			ihs->second->SetValue( nSID );
		}
		CPropMap::const_iterator isrc = pSeqProps->find( "SrcName" );
		if ( isrc != pSeqProps->end() )
		{
			isrc->second->SetValue( string( "Heads\\MMS\\" ) + pItems->GetItemName( nID ) + ".mms" );
		}
		pSeqs->ReleasePropList( pSeqProps );
		pItems->ReleasePropList( pProps );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
// CExportResultDlg dialog
////////////////////////////////////////////////////////////////////////////////////////////////////

CExportResultDlg::CExportResultDlg( const string &szLog, CWnd* pParent /*=NULL*/)
: CDialog(CExportResultDlg::IDD, pParent)
{
  //{{AFX_DATA_INIT(CExportResultDlg)
  m_Log = _T("");
	m_bHideWarnings = FALSE;
	//}}AFX_DATA_INIT
  
  FILE *pFile = fopen( szLog.c_str(), "r" );
  vector<char> lbuf( 1, 0 );
  if ( pFile )
  {
    fseek( pFile, 0, SEEK_END );
    int l = ftell( pFile );
    lbuf.resize( l + 1 );
    fseek( pFile, 0, SEEK_SET );
    l = fread( &lbuf[0], 1, l, pFile );
    lbuf[l] = 0;
    fclose( pFile );
  }
  const char *pStr = &lbuf[0];
  
  const int n = strlen( pStr );
  for ( int i = 0; i < n; ++i )
  {
    if ( '\n' == pStr[i] || '\r' == pStr[i] )
      m_Log += "\r\n";
    else
      m_Log += pStr[i];
  }
	m_Log += "\r\n"; // для удобства последущей фильтрации варнингов
	m_LogCopy = m_Log;
}


void CExportResultDlg::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CExportResultDlg)
  DDX_Text(pDX, IDC_MAYA_LOG, m_Log);
	DDX_Check(pDX, IDC_EXPORT_HIDEWARN, m_bHideWarnings);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CExportResultDlg, CDialog)
//{{AFX_MSG_MAP(CExportResultDlg)
	ON_BN_CLICKED(IDC_EXPORT_HIDEWARN, OnExportHidewarn)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CExportResultDlg message handlers

void CExportResultDlg::OnExportHidewarn() 
{
	UpdateData();
	if ( m_bHideWarnings )
	{
		m_Log = "";
		int nNewPos, nPos = 0;
		while ( -1 != (nNewPos = m_LogCopy.Find( '\n', nPos )) )
		{
			CString tmp = m_LogCopy.Mid( nPos, 1 + nNewPos - nPos );
			if ( -1 == tmp.Find( "Warning" ) && -1 == tmp.Find( "warning" ) )
				m_Log += tmp;
			nPos = nNewPos + 1;
		}
	}
	else
		m_Log = m_LogCopy;
	//
	UpdateData( false );
}
