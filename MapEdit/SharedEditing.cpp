#include "StdAfx.h"
#include "dbDefs.h"
#include "MapEdit.h"
#include <afxsock.h>

const int ME_SOCKET_PORT = 16115;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMESocket: public CObjectBase, public CSocket
{
	OBJECT_NOCOPY_METHODS(CMESocket);
	list<CObj<CMESocket> > sockets;

	void Send( const string &sz )
	{
		CSocket::Send( sz.c_str(), sz.size() + 1 );
	}

public:
	virtual void OnAccept( int nErrorCode )
	{
		CMESocket *p = new CMESocket();
		sockets.push_back( p );
		Accept( *p );
		CSocket::OnAccept( nErrorCode );
	}
	virtual void OnReceive(int nErrorCode) 
	{
		TCHAR buff[4096];
		int nRead;
		nRead = Receive(buff, sizeof(buff) );

		if ( nRead == 0 || nRead == SOCKET_ERROR )
			Send( "false" );
		int nReceivedID = atoi( buff );
		int nTree, nID, nVar;
		theApp.GetActiveItem( &nTree, &nID, &nVar );
		if ( nTree != IDC_TEMPLATE_TREE || nVar != nReceivedID )
			Send( "false" );
		Send( "true" );
		CSocket::OnReceive( nErrorCode );
	}
}	meSocket;
////////////////////////////////////////////////////////////////////////////////////////////////////
bool OpenMESocket()
{
	if ( !AfxSocketInit() )
		return false;
	if ( !meSocket.Create( ME_SOCKET_PORT ) )
		return false;
	return meSocket.Listen();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CloseMESocket()
{
	meSocket.Close();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class CShareAccessor
{
public:
	LONG  m_nPlacementID;
	CHAR  m_szComputerName[MAX_COMPUTERNAME_LENGTH + 1];
	CHAR  m_szTime[255];

	BEGIN_COLUMN_MAP( CShareAccessor )
		COLUMN_ENTRY(1, m_nPlacementID)
		COLUMN_ENTRY(2, m_szComputerName)
		COLUMN_ENTRY(4, m_szTime)
	END_COLUMN_MAP()
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CShareDB: public CBaseDBCmd<CAccessor<CShareAccessor> >
{
	void GetComputerName()
	{
		DWORD dwSize = sizeof( m_szComputerName );
		::GetComputerName( m_szComputerName, &dwSize );
	}
public:
	bool OpenShare( int nID );
	bool DeleteShare( int nID, const CHAR *pszComputer = 0 );

	bool GetPlacementOwners( int nID, vector<string> *pNames );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CShareDB::OpenShare( int nID )
{
	HRESULT hr = Open( "SELECT * FROM SharedMaps WHERE PlacementID = -1" );
	if ( FAILED(hr) )
		return false;

	m_nPlacementID = nID;

  __time64_t ltime;
	_time64( &ltime );
	string szTime = _ctime64( &ltime );
	strcpy( m_szTime, szTime.c_str() );

	GetComputerName();

	hr = Insert();
	
	if ( FAILED( hr ) )
	{
		DisplayOLEDBErrorRecords( hr );
		return false;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CShareDB::DeleteShare( int nID, const CHAR *pszComputer )
{
	if ( !pszComputer )
		GetComputerName();
	else
		strcpy( m_szComputerName, pszComputer );
	string szQuery = "SELECT * FROM SharedMaps WHERE PlacementID = ";
	szQuery += IToA( nID ) + " AND ComputerName = '";
	szQuery += m_szComputerName;
	szQuery += "'";

	HRESULT hr = Open( szQuery );
	if ( FAILED( hr ) || S_OK != MoveNext() )
		return false;
	
	hr = Delete();
	if ( FAILED( hr ) )
	{
		DisplayOLEDBErrorRecords( hr );
		return false;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CShareDB::GetPlacementOwners( int nID, vector<string> *pNames )
{
	GetComputerName();
	string szLocalhost = m_szComputerName;
	string szQuery = "SELECT * FROM SharedMaps WHERE PlacementID = ";
	szQuery += IToA( nID );
	//
	HRESULT hr = Open( szQuery );
	if ( FAILED( hr ) )
		return false;

	CSocket sock;
	sock.Create();

	vector<string> v2Delete;
	vector<string> vOwners;
	string szID = IToA( nID );

	while( MoveNext() == S_OK )
	{
		if ( szLocalhost == m_szComputerName || !sock.Connect( m_szComputerName, ME_SOCKET_PORT ) )
		{
			v2Delete.push_back( m_szComputerName );
			continue;
		}
		sock.Send( szID.c_str(), szID.size() + 1 );
		TCHAR buff[4096];
		int nRead;
		nRead = sock.Receive(buff, sizeof(buff) );
		string szRet = buff;
		if ( szRet == "true" )
			vOwners.push_back( m_szComputerName );
		else
			v2Delete.push_back( m_szComputerName );
	}
	//
	for ( int i = 0; i < v2Delete.size(); ++i )
		DeleteShare( nID, v2Delete[i].c_str() );
	//
	if ( !vOwners.empty() )
	{
		CString str = "This map is opened on workstations:\n";
		for ( int i = 0; i < vOwners.size(); ++i )
		{
			if ( vOwners[i] == szLocalhost )
				continue;
			str += "\n\t";
			str += vOwners[i].c_str();
		}
		MessageBox( theApp.m_pMainWnd->m_hWnd, str, "Warning", MB_OK | MB_ICONWARNING );
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static CShareDB db;
bool CheckPlacementShare( int nID )
{
	vector<string> owners;
	if ( !db.GetPlacementOwners( nID, &owners ) )
		false;
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void OpenPlacement( int nID )
{
	CheckPlacementShare( nID );
	db.OpenShare( nID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void ClosePlacement( int nID )
{
	db.DeleteShare( nID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////