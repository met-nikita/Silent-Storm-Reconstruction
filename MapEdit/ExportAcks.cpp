#include "StdAfx.h"
#include "MapEdit.h"
#include "MainFrm.h"
#include "dbDefs.h"
#include "ItemsMgr.h"
#include "TreeSelItemDlg.h"
#include "Export.h"
#include "..\Misc\StrProc.h"
#include "ExportAcks.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
int ExportPersAcks( const string &szPath, int nPersID );
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExcelAckAccessor
{
public:
	TCHAR m_szCondition[255];
	TCHAR m_szConditionDescr[255];
	TCHAR m_szParam[255];
	LONG  m_nParam;
	TCHAR m_szPhrase1[10240];
	TCHAR m_szIntonation1[255];
  LONG  m_nProbability1;
	TCHAR m_szPhrase2[10240];
	TCHAR m_szIntonation2[255];
  LONG  m_nProbability2;

BEGIN_ACCESSOR_MAP( CExcelAckAccessor, 1 )
  BEGIN_ACCESSOR( 0, true )
    COLUMN_ENTRY(1, m_szCondition)
		COLUMN_ENTRY(2, m_szConditionDescr)
		COLUMN_ENTRY(3, m_szParam)
  	COLUMN_ENTRY(4, m_szPhrase1)
    COLUMN_ENTRY(5, m_szIntonation1)
		COLUMN_ENTRY(6, m_nProbability1)
  	COLUMN_ENTRY(7, m_szPhrase2)
    COLUMN_ENTRY(8, m_szIntonation2)
		COLUMN_ENTRY(9, m_nProbability2)
  END_ACCESSOR()
END_ACCESSOR_MAP()
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckCondittionAccessor
{
public:
	LONG  m_nID;
	TCHAR m_szConditionName[255];

BEGIN_ACCESSOR_MAP( CAckCondittionAccessor, 2 )
  BEGIN_ACCESSOR( 0, true )
    COLUMN_ENTRY(1, m_nID)
  	COLUMN_ENTRY(5, m_szConditionName)
  END_ACCESSOR()
  BEGIN_ACCESSOR( 1, false )
  	COLUMN_ENTRY(5, m_szConditionName)
  END_ACCESSOR()
END_ACCESSOR_MAP()
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckAccessor
{
public:
	LONG  m_nID;
	TCHAR m_szUserName[256];
	LONG  m_nWhoID;
	LONG  m_nConditionID;
	LONG  m_nAckSeqID;
	LONG  m_nProbability;
	TCHAR m_szParam0[256];

BEGIN_ACCESSOR_MAP( CAckAccessor, 2 )
  BEGIN_ACCESSOR( 0, true )
    COLUMN_ENTRY(1, m_nID)
		COLUMN_ENTRY(2, m_szUserName)
		COLUMN_ENTRY(4, m_nWhoID)
  	COLUMN_ENTRY(5, m_nConditionID)
		COLUMN_ENTRY(6, m_nAckSeqID)
		COLUMN_ENTRY(7, m_nProbability)
		COLUMN_ENTRY(8, m_szParam0)
  END_ACCESSOR()
  BEGIN_ACCESSOR( 1, false )
		COLUMN_ENTRY(2, m_szUserName)
		COLUMN_ENTRY(4, m_nWhoID)
  	COLUMN_ENTRY(5, m_nConditionID)
		COLUMN_ENTRY(6, m_nAckSeqID)
		COLUMN_ENTRY(7, m_nProbability)
		COLUMN_ENTRY(8, m_szParam0)
  END_ACCESSOR()
END_ACCESSOR_MAP()
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckSeqAccessor
{
public:
	LONG  m_nID;
	TCHAR m_szUserName[256];
	LONG  m_nAckID0;

BEGIN_ACCESSOR_MAP( CAckSeqAccessor, 2 )
  BEGIN_ACCESSOR( 0, true )
    COLUMN_ENTRY(1, m_nID)
		COLUMN_ENTRY(2, m_szUserName)
		COLUMN_ENTRY(4, m_nAckID0)
  END_ACCESSOR()
  BEGIN_ACCESSOR( 1, false )
		COLUMN_ENTRY(2, m_szUserName)
		COLUMN_ENTRY(4, m_nAckID0)
  END_ACCESSOR()
END_ACCESSOR_MAP()
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckOperationAccessor
{
public:
	TCHAR m_szCondition[255];
	TCHAR m_szText[512];
	TCHAR m_szIntonation[255];
	TCHAR m_szFile[255];
	TCHAR m_szDate[50];
	TCHAR m_szFile1[255];
	TCHAR m_szFile2[255];
	TCHAR m_szFile3[255];
	TCHAR m_szFile4[255];
	TCHAR m_szFile5[255];
	
BEGIN_ACCESSOR_MAP( CAckOperationAccessor, 2 )
  BEGIN_ACCESSOR( 0, true )
    COLUMN_ENTRY(2, m_szCondition)
		COLUMN_ENTRY(3, m_szText)
		COLUMN_ENTRY(4, m_szIntonation)
		COLUMN_ENTRY(5, m_szFile)
		COLUMN_ENTRY(6, m_szDate)
  END_ACCESSOR()
	BEGIN_ACCESSOR( 1, true )
		COLUMN_ENTRY(2, m_szCondition)
		COLUMN_ENTRY(3, m_szText)
		COLUMN_ENTRY(4, m_szIntonation)
		COLUMN_ENTRY(5, m_szFile)
		COLUMN_ENTRY(6, m_szDate)
		COLUMN_ENTRY(7, m_szFile1)
		COLUMN_ENTRY(8, m_szFile2)
		COLUMN_ENTRY(9, m_szFile3)
		COLUMN_ENTRY(10, m_szFile4)
		COLUMN_ENTRY(11, m_szFile5)
	END_ACCESSOR()
END_ACCESSOR_MAP()

	void FillAccessor( const char *pszCondition, const char *pszText, const char *pszIntonation, const char *pszFile )
	{
		strncpy( m_szCondition, pszCondition, sizeof( m_szCondition ) );
		strncpy( m_szText, pszText, sizeof( m_szText ) );
		strncpy( m_szIntonation, pszIntonation, sizeof( m_szIntonation ) );
		strncpy( m_szFile, pszFile, sizeof( m_szFile ) );
		//
		__time64_t ltime;
		_time64( &ltime );
		strncpy( m_szDate, _ctime64( &ltime ), sizeof( m_szDate ) );
	}
	void FillAccessor( const char *pszCondition, const char *pszText, const char *pszIntonation, const string szFiles[6] )
	{
		FillAccessor( pszCondition, pszText, pszIntonation, szFiles[0].c_str() );
		int nBytes = sizeof( m_szFile );
		strncpy( m_szFile1, szFiles[1].c_str(), nBytes );
		strncpy( m_szFile2, szFiles[2].c_str(), nBytes );
		strncpy( m_szFile3, szFiles[3].c_str(), nBytes );
		strncpy( m_szFile4, szFiles[4].c_str(), nBytes );
		strncpy( m_szFile5, szFiles[5].c_str(), nBytes );
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExcelAckDB: public CBaseDBCmd<CAccessor<CExcelAckAccessor> >
{
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckCondittionDB: public CBaseDBCmd<CAccessor<CAckCondittionAccessor> >
{
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckDB: public CBaseDBCmd<CAccessor<CAckAccessor> >
{
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckSeqDB: public CBaseDBCmd<CAccessor<CAckSeqAccessor> >
{
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAckOperationDB: public CBaseDBCmd<CAccessor<CAckOperationAccessor> >
{
};
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAckInfoDB::OpenInfo( int nID )
{
	string szAckInfoQuery = "SELECT * FROM AckInfos WHERE ID=" + IToA( nID );
	if ( FAILED( CBaseDBCmd<CAccessor<CAckInfoAccessor> >::Open( szAckInfoQuery ) ) )
		return false;
	if ( MoveNext() == S_OK )
		return true;
	ASSERT(0);
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
string CAckInfoDB::GetInfoString()
{
	const SResTree *pTree = theApp.GetResTree( IDC_STRINGS_TREE );
	const CPropMap *pProps = pTree->pItemsTree->GetPropList( m_nStringID );
	string szRet="";
	if ( pProps )
	{
		const CPropMap::const_iterator i = pProps->find( "String" );
		if ( i != pProps->end() )
			szRet = (string)i->second->GetValue();
		pTree->pItemsTree->ReleasePropList( pProps );
	}
	return szRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
string GetSound( int nID )
{
	static const SResTree tree = *theApp.GetResTree( IDC_SOUNDS_TREE );
	const CPropMap *pProps = tree.pItemsTree->GetPropList( nID );
	string szRet="";
	if ( pProps )
	{
		const CPropMap::const_iterator i = pProps->find( "SrcName" );
		if ( i != pProps->end() )
			szRet = (string)i->second->GetValue();
		tree.pItemsTree->ReleasePropList( pProps );
	}
	return szRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAckInfoDB::GetInfoSound( string soundFiles[6] )
{
	soundFiles[0] = GetSound( m_nSoundID );
	soundFiles[1] = GetSound( m_nSoundID1 );
	soundFiles[2] = GetSound( m_nSoundID2 );
	soundFiles[3] = GetSound( m_nSoundID3 );
	soundFiles[4] = GetSound( m_nSoundID4 );
	soundFiles[5] = GetSound( m_nSoundID5 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAckInfoDB::DeleteInfo( int nID )
{
	if ( !OpenInfo( nID ) )
		return;
	static const SResTree strTree = *theApp.GetResTree( IDC_STRINGS_TREE );
	static const SResTree sndTree = *theApp.GetResTree( IDC_SOUNDS_TREE );
	if ( m_nStringID > 0 )
		strTree.pItemsTree->DeleteItem( -1, m_nStringID );
	//
	if ( m_nSoundID > 0 ) sndTree.pItemsTree->DeleteItem( -1, m_nSoundID );
	if ( m_nSoundID1 > 0 ) sndTree.pItemsTree->DeleteItem( -1, m_nSoundID1 );
	if ( m_nSoundID2 > 0 ) sndTree.pItemsTree->DeleteItem( -1, m_nSoundID2 );
	if ( m_nSoundID3 > 0 ) sndTree.pItemsTree->DeleteItem( -1, m_nSoundID3 );
	if ( m_nSoundID4 > 0 ) sndTree.pItemsTree->DeleteItem( -1, m_nSoundID4 );
	if ( m_nSoundID5 > 0 ) sndTree.pItemsTree->DeleteItem( -1, m_nSoundID5 );

	if ( !OpenInfo( nID ) )
		return;

	HRESULT hr = Delete();
	if ( FAILED(hr) )
		DisplayOLEDBErrorRecords( hr );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAckInfoDB::DeleteSounds( int nID )
{
	if ( !OpenInfo( nID ) )
		return false;
	static const SResTree sndTree = *theApp.GetResTree( IDC_SOUNDS_TREE );
	//
	if ( m_nSoundID > 0 ) sndTree.pItemsTree->DeleteItem( -1, m_nSoundID );
	if ( m_nSoundID1 > 0 ) sndTree.pItemsTree->DeleteItem( -1, m_nSoundID1 );
	if ( m_nSoundID2 > 0 ) sndTree.pItemsTree->DeleteItem( -1, m_nSoundID2 );
	if ( m_nSoundID3 > 0 ) sndTree.pItemsTree->DeleteItem( -1, m_nSoundID3 );
	if ( m_nSoundID4 > 0 ) sndTree.pItemsTree->DeleteItem( -1, m_nSoundID4 );
	if ( m_nSoundID5 > 0 ) sndTree.pItemsTree->DeleteItem( -1, m_nSoundID5 );

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAckInfoDB::Insert( const string &szUserName, int nStrID, int nSndID, int nWhoID, const string &szIntonation )
{
	CBaseDBCmd<CAccessor<CAckInfoAccessor> >::Open( "SELECT * FROM AckInfos WHERE ID = -1" );
	m_nSoundID = nSndID;
	m_nWhoID = nWhoID;
	m_nStringID = nStrID;
	_tcscpy( m_szUserName, szUserName.c_str() );
	_tcscpy( m_szIntonation, szIntonation.c_str() );
	HRESULT hr = CBaseDBCmd<CAccessor<CAckInfoAccessor> >::Insert( 1 );
	if ( FAILED( hr ) || MoveNext() != S_OK )
		return -1;
	return m_nID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CAckInfoDB::InsertHero( const string &szUserName, int nStrID, 
	int nSndID, int nSnd1, int nSnd2, int nSnd3, int nSnd4, int nSnd5,
	int nWhoID, const string &szIntonation )
{
	CBaseDBCmd<CAccessor<CAckInfoAccessor> >::Open( "SELECT * FROM AckInfos WHERE ID = -1" );
	m_nSoundID = nSndID;
	m_nSoundID1 = nSnd1;
	m_nSoundID2 = nSnd2;
	m_nSoundID3 = nSnd3;
	m_nSoundID4 = nSnd4;
	m_nSoundID5 = nSnd5;
	m_nWhoID = nWhoID;
	m_nStringID = nStrID;
	_tcscpy( m_szUserName, szUserName.c_str() );
	_tcscpy( m_szIntonation, szIntonation.c_str() );
	HRESULT hr = CBaseDBCmd<CAccessor<CAckInfoAccessor> >::Insert( 2 );
	if ( FAILED( hr ) || MoveNext() != S_OK )
		return -1;
	return m_nID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CAckInfoDB::SetSounds( int nAckInfoID, int nSndID, int nSnd1, int nSnd2, int nSnd3, int nSnd4, int nSnd5 )
{
	if ( !OpenInfo( nAckInfoID ) )
		return false;
	m_nSoundID = nSndID;
	m_nSoundID1 = nSnd1;
	m_nSoundID2 = nSnd2;
	m_nSoundID3 = nSnd3;
	m_nSoundID4 = nSnd4;
	m_nSoundID5 = nSnd5;
  HRESULT hr = SetData( 3 );
	if ( FAILED( hr ) )
	{
		DisplayOLEDBErrorRecords( hr );
		return false;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT InitDB( const CString &szConnection, CSession &session, CDBPropSet &propset )
{
	USES_CONVERSION;
  CDataSource connection;

  //propset.AddProperty(DBPROP_INIT_LCID, (long)1049);
  HRESULT hr = connection.OpenFromInitializationString( SysAllocString( A2W( szConnection ) ) );
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
bool ExportRecord( const CExcelAckAccessor &data, int nPersID, const string &szName, CProp *pMaxAck );
////////////////////////////////////////////////////////////////////////////////////////////////////
CFolderHash strFolderHash;
CFolderHash sndFolderHash;
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMainFrame::OnExportAcks()
{
	CFileDialog dlg( TRUE, 0, 0, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "Excel Files (*.xls)|*.xls||", this );

	if ( IDOK != dlg.DoModal() )
		return;
	CString path = dlg.GetPathName();

	const SResTree *pPersTree = theApp.GetResTree( IDC_RPG_PERS_TREE );

	if ( IDOK != pPersTree->pTreeDlg->DoModal() )
		return;
	int nTree, nPersID;
	pPersTree->pTreeDlg->GetSelectedItemID( &nTree, &nPersID );
	if ( nPersID <= 0 )
		return;
	BeginWaitCursor();
	int nRecords = ExportPersAcks( (LPCSTR)path, nPersID );
	EndWaitCursor();
	string szText = "Exported " + IToA( nRecords ) + " records";
	MessageBox( szText.c_str(), "Results" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int ExportPersAcks( const string &szPath, int nPersID )
{
	CString szConnect = "Provider=MSDASQL.1;Persist Security Info=False;Extended Properties=\"DSN=Excel Files;DBQ=";
	szConnect += szPath.c_str();
	szConnect += ";DriverId=790;MaxBufferSize=2048;PageTimeout=5;\"";

	SDBConnection connection;
	HRESULT hr = InitDB( szConnect, connection.session, connection.propset );
	if ( FAILED( hr ) )
		return 0;
	CExcelAckDB db;
	db.SetConnection( &connection );
	hr = db.Open( "SELECT * FROM `General on game zone$`" );
	if ( FAILED( hr ) )
		return 0;
	//
	const SResTree *pPersTree = theApp.GetResTree( IDC_RPG_PERS_TREE );
	//
	int nRecords = 0;
	string szAckName = GetAckName( nPersID );
	if ( szAckName == "" )
		return 0;
	//
	strFolderHash.clear();
	sndFolderHash.clear();

	if ( !pPersTree )
		return 0;
	const CPropMap *pProps = pPersTree->pItemsTree->GetPropList( nPersID );
	if ( !pProps )
		return 0;
	CPropMap::const_iterator iMaxAck = pProps->find( "MaxAck" );
	if ( iMaxAck == pProps->end() )
		return 0;

	while ( db.MoveNext() == S_OK )
	{
		if ( (CString)db.m_szCondition == "" || ((CString)db.m_szPhrase1 == "" && (CString)db.m_szPhrase2 == "") )
			continue;
		if ( !ExportRecord( db, nPersID, szAckName, iMaxAck->second ) )
			break;
		++nRecords;
	}
	pPersTree->pItemsTree->ReleasePropList( pProps );
	return nRecords;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
string GetAckName( int nPersID )
{
	const SResTree *pPersTree = theApp.GetResTree( IDC_RPG_PERS_TREE );
	const SResTree *pNatTree = theApp.GetResTree( IDC_NATIONALITY_TREE );

	if ( !pNatTree || !pPersTree )
		return "";
	const CPropMap *pProps = pPersTree->pItemsTree->GetPropList( nPersID );
	if ( !pProps )
		return "";
	string szName = pPersTree->pItemsTree->GetItemName( nPersID );
	CPropMap::const_iterator iside = pProps->find( "Side" );
	CPropMap::const_iterator inat = pProps->find( "NationalityID" );
	if ( iside == pProps->end() || inat == pProps->end() )
		return "";
	string szSide =iside->second->GetValue();
	if ( szSide == "" )
	{
		string szErr = "Side for pers ";
		szErr += szName;
		szErr += " not found";
		MessageBox( theApp.GetMainWnd()->m_hWnd, szErr.c_str(), "Warning", MB_ICONWARNING | MB_OK );
		return "";
	}
	int nNatID = inat->second->GetValue();
	pPersTree->pItemsTree->ReleasePropList( pProps );
	string szNat;
	const CPropMap *pNatProps = pNatTree->pItemsTree->GetPropList( nNatID );
	if ( pNatProps )
	{
		CPropMap::const_iterator i = pNatProps->find( "Shortcut" );
		if ( i != pNatProps->end() )
			szNat = (string)i->second->GetValue();
		pNatTree->pItemsTree->ReleasePropList( pNatProps );
	}
	return szSide + "\\" + szNat + '-' + szName + "\\" + szName;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
string ReplaceApo( const string &str )
{
	string ret;
	for ( string::const_iterator i = str.begin(); i != str.end(); ++i )
		if ( *i == '\'' )
			ret += '`';
		else
			ret += *i;
	return ret;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
static void DeleteRecords( const vector<int> &recs, const string &szTbl, CBaseDBCmd<CAccessor<T> > *pDB )
{
	for ( int i = 0; i < recs.size(); ++i )
	{
		string str = "SELECT * FROM " + szTbl + " WHERE ID=";
		str += IToA( recs[i] );
		if ( !FAILED( pDB->Open( str ) ) && pDB->MoveNext() == S_OK )
			pDB->Delete();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void DeleteInfos( const vector<int> &recs, CAckInfoDB *pDB )
{
	for ( int i = 0; i < recs.size(); ++i )
		pDB->DeleteInfo( recs[i] );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void LogAckOperation( const vector<SAckOperation> &ops, const string &szTable )
{
	static CAckOperationDB db;

	for ( int i = 0; i < ops.size(); ++i )
	{
		const SAckOperation &o = ops[i];
		string szQuery = "SELECT * FROM ";
		szQuery += szTable + " WHERE ID = -1";
		db.Open( szQuery );
		HRESULT hr;
		if ( o.b6Voices )
		{
			db.FillAccessor( o.szCondition.c_str(), o.szText.c_str(), o.szIntonation.c_str(), o.szSndFiles );
			hr = db.Insert( 1 );
		}
		else
		{
			db.FillAccessor( o.szCondition.c_str(), o.szText.c_str(), o.szIntonation.c_str(), o.szFile.c_str() );
			hr = db.Insert( 0 );
		}
		if ( FAILED( hr ) )
			DisplayOLEDBErrorRecords( hr );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
extern SDBConnection dbConnection;
static bool ExportRecord( const CExcelAckAccessor &data, int nPersID, const string &szName, CProp *pMaxAck )
{
	CAckCondittionDB condition;
	CAckDB ack;
	CAckSeqDB ackseq;
	CAckInfoDB ackinfo;
	
	condition.SetConnection( &dbConnection );
	ack.SetConnection( &dbConnection );
	ackseq.SetConnection( &dbConnection );
	ackinfo.SetConnection( &dbConnection );

	// ищем условие с нужным названием
	string szCondition = ReplaceApo( data.m_szCondition );
	string szConditionQuery = "SELECT * FROM AckConditions WHERE Comment='";
	szConditionQuery += szCondition;
	szConditionQuery += '\'';
	if ( FAILED( condition.Open( szConditionQuery ) ) )
		return false;
	if ( condition.MoveNext() != S_OK )
	{
		// такого кондишена нет - создаем его
		_tcscpy( condition.m_szConditionName, szCondition.c_str() );
		if ( FAILED (condition.Insert( 1 ) ) )
			return false;
		condition.MoveNext();
	}
	int nConditionID = condition.m_nID;
	string szAckName = szName;// + IToA( nConditionID );
	const SResTree *pTree = theApp.GetResTree( IDC_STRINGS_TREE );
	if ( !pTree )
		return false;
	CItemsMgr *pStrings = pTree->pItemsTree;
	const SResTree *pSndTree = theApp.GetResTree( IDC_SOUNDS_TREE );
	CItemsMgr *pSounds = pSndTree->pItemsTree;
	// ищем аски соответствующие кондишену и выбранному персу
	string szAckQuery = "SELECT * FROM Acks WHERE ConditionID=" + IToA( nConditionID ) + " AND WhoID=" + IToA( nPersID );
//	if ( string( data.m_szParam ) != "" )
//	{
		string szParam = data.m_szParam;
		NStr::TrimBoth( szParam );
		szAckQuery += string( " AND Param0=\'" ) + szParam + '\'';
//	}
	if ( FAILED( ack.Open( szAckQuery ) ) )
		return false;
	bool bPhraseComplete[] = { false, false }; // Phrase1 Phrase2
	vector<int> freeInfos; // айдишники AckInfos, которые не соответствуют ни одному текущему аску
	vector<int> freeSeqs;
	vector<int> freeAcks;
	vector<int> freeStrings;
	vector<int> freeSounds;
	vector<SAckOperation> ops;
	int nPhrase = -1;
	string szExcelPhrase1 = string( "N/A" ) == data.m_szPhrase1 || string( "N/A." ) == data.m_szPhrase1 ? "" : data.m_szPhrase1;
	string szExcelPhrase2 = string( "N/A" ) == data.m_szPhrase2 || string( "N/A." ) == data.m_szPhrase2 ? "" : data.m_szPhrase2;
	while ( ack.MoveNext() == S_OK )
	{
		SAckOperation ao;
		ao.szCondition = szCondition;
		++nPhrase;
		ASSERT( nPhrase < 2 );
		freeAcks.push_back( ack.m_nID );
		// достаем айдишник AckInfo из AckSeqs
		string szSeqQuery = "SELECT * FROM AckSeqs WHERE ID=" + IToA( ack.m_nAckSeqID );
		if ( FAILED( ackseq.Open( szSeqQuery ) ) )
			return false;
		if ( ackseq.MoveNext() != S_OK )
		{
			ASSERT(0);
			continue;
		}
		freeSeqs.push_back( ackseq.m_nID );
		// ищем соответствие AckInfo какой либо фразе
		if ( !ackinfo.OpenInfo( ackseq.m_nAckID0 ) )
			continue;
		freeInfos.push_back( ackinfo.m_nID );
		freeSounds.push_back( ackinfo.m_nSoundID );
		string str = ackinfo.GetInfoString();
		const CPropMap *pSndProps = pSounds->GetPropList( ackinfo.m_nSoundID );
		
		if ( !pSndProps )
			continue;
		freeStrings.push_back( ackinfo.m_nStringID );
		const CPropMap::const_iterator isndsrc = pSndProps->find( "SrcName" );
		if ( isndsrc == pSndProps->end() )
			return false;
		ao.szText = str;
		ao.szIntonation = ackinfo.m_szIntonation;
		ao.szFile = (string)isndsrc->second->GetValue();
		bool bPh1 = str == szExcelPhrase1;
		bool bPh2 = str == szExcelPhrase2;
		bool bComplete = false;
		pSounds->ReleasePropList( pSndProps );
		if ( bPh1 || bPh2 )
		{
			// фраза соотвесвует той что уже есть в базе, соотвествует ли интонация ?
			string szIntonation = bPh1 ? data.m_szIntonation1 : data.m_szIntonation2;
			if ( szIntonation == ackinfo.m_szIntonation )
			{
				freeAcks.pop_back();
				freeSeqs.pop_back();
				freeInfos.pop_back();
				freeStrings.pop_back();
				freeSounds.pop_back();
				bComplete = true;
				bPhraseComplete[bPh1? 0 : 1] = true;
			}
		}
		if ( !bComplete )
			ops.push_back( ao );
	}
	LogAckOperation( ops, "AcksDeleted" );
	// удаляем устаревшие записи
	for ( int i = 0; i < freeStrings.size(); ++i )
		pStrings->DeleteItem( -1, freeStrings[i] );
	for ( int i = 0; i < freeSounds.size(); ++i )
		pSounds->DeleteItem( -1, freeSounds[i] );
	DeleteRecords( freeAcks, "Acks", &ack );
	DeleteRecords( freeSeqs, "AckSeqs", &ackseq );
	DeleteRecords( freeInfos, "AckInfos", &ackinfo );
	// добавляем обновленные записи
	ops.clear();
	for ( int i = 0; i < 2; ++i )
	{
		if ( bPhraseComplete[i] )
			continue;
		string szPhrase = i == 0 ? szExcelPhrase1 : szExcelPhrase2;
		if ( szPhrase == "" || szPhrase == "N/A" || szPhrase == "N/A." )
			continue;
		string szIntonation = i == 0 ? data.m_szIntonation1 : data.m_szIntonation2;
		int nProb = i == 0 ? data.m_nProbability1 : data.m_nProbability2;
		//
		int nStrFolder = GetFolder( IDC_STRINGS_TREE, 11, szAckName, strFolderHash );
		int nStringID = AddString( nStrFolder, data.m_szCondition, szPhrase );
		if ( nStringID <= 0 )
			continue;
  	// sound
		int nAckID = pMaxAck->GetValue(); // RPGPers.MaxAck
		string szUserName = szAckName + IToA( nAckID );
		pMaxAck->SetValue( nAckID + 1 );
		string szWav = "Sounds\\Acks\\" + szUserName + ".wav";
		int nSndFolder = GetFolder( IDC_SOUNDS_TREE, 83, szAckName, sndFolderHash );
		int nSID = AddSound( nSndFolder, szUserName, szWav );
		SAckOperation ao;
		ao.szCondition = data.m_szCondition;
		ao.szText = szPhrase;
		ao.szIntonation = szIntonation;
		ao.szFile = szWav;
		ops.push_back( ao );
		//
		int nInfoID = ackinfo.Insert( szUserName, nStringID, nSID, nPersID, szIntonation );
		if ( nInfoID < 0 )
			continue;
		ackseq.Open( "SELECT * FROM AckSeqs WHERE ID = -1" );
		ackseq.m_nAckID0 = nInfoID;
		_tcscpy( ackseq.m_szUserName, szUserName.c_str() );
		HRESULT hr = ackseq.Insert( 1 );
		if ( FAILED( hr ) || ackseq.MoveNext() != S_OK )
			continue;
		ack.Open( "SELECT * FROM Acks WHERE ID = -1" );
		ack.m_nWhoID = nPersID;
		ack.m_nConditionID = nConditionID;
		ack.m_nAckSeqID = ackseq.m_nID;
		ack.m_nProbability = nProb;
		_tcscpy( ack.m_szUserName, szUserName.c_str() );
		string szParam = data.m_szParam;
		NStr::TrimBoth( szParam );
		_tcscpy( ack.m_szParam0, szParam.c_str() );
		ack.Insert( 1 );
	}
	LogAckOperation( ops, "AcksInserted" );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int AddString( int nFolder, const string &szName, const string &szText )
{
	const SResTree *pTree = theApp.GetResTree( IDC_STRINGS_TREE );
	int nStringID = pTree->pItemsTree->AddItem( -1, nFolder, szName );
	const CPropMap *pProps = pTree->pItemsTree->GetPropList( nStringID );
	if ( pProps )
	{
		CPropMap::const_iterator it = pProps->find( "String" );
		if ( it == pProps->end() )
			return -1;
		it->second->SetValue( szText );
		pTree->pItemsTree->ReleasePropList( pProps );
	}
	return nStringID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int AddSound( int nFolder, const string &szName, const string &szSrc )
{
	const SResTree *pTree = theApp.GetResTree( IDC_SOUNDS_TREE );
	int nID = pTree->pItemsTree->AddItem( -1, nFolder, szName );
	const CPropMap *pProps = pTree->pItemsTree->GetPropList( nID );
	if ( pProps )
	{
		CPropMap::const_iterator it = pProps->find( "SrcName" );
		if ( it == pProps->end() )
			return -1;
		it->second->SetValue( szSrc );
		pTree->pItemsTree->ReleasePropList( pProps );
	}
	return nID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int GetFolder( int nTreeID, int nRootFolder, const string &szPath )
{
	const SResTree *pTree = theApp.GetResTree( nTreeID );
	if ( !pTree )
		return -1;
	CItemsMgr *pMgr = pTree->pItemsTree;
	const char *pszRoot = pMgr->ID2FolderName( nRootFolder );
	if ( !pszRoot )
		return nRootFolder;

	vector<string> vszPath;
	NStr::SplitString( szPath, vszPath, '\\' );
	int nParent = nRootFolder;
	for ( int i = 0; i < vszPath.size() - 1; ++i )
	{
		int nNewParent = pMgr->DoesFolderExist( vszPath[i], nParent );
		if ( nNewParent < 0 )
		{
			for ( int j = i; j < vszPath.size() - 1; ++j )
			{
				nParent = pMgr->AddFolder( -1, nParent, vszPath[j] );
				if ( nParent < 0 )
					return nRootFolder;
			}
			return nParent;
		}
		nParent = nNewParent;
	}
	return nParent;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int GetFolder( int nTreeID, int nRootFolder, const string &szPath, CFolderHash &fhash )
{
	CFolderHash::const_iterator i = fhash.find( szPath );
	if ( i != fhash.end() )
		return i->second;
	int nRet = GetFolder( nTreeID, nRootFolder, szPath );
	fhash[szPath] = nRet;

	return nRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void ExportAcksTexts( CItemsMgr *pItems, vector<int> nItemIDs, bool bForceExport )
{
	for ( int i = 0; i < nItemIDs.size(); ++i )
	{
		int nPersID = nItemIDs[i];
		const CPropMap *pProps = pItems->GetPropList( nPersID );
		if ( !pProps )
			continue;
		CPropMap::const_iterator it = pProps->find( "SrcAcksFile" );
		if ( it == pProps->end() )
		{
			ASSERT(0);
			break;
		}
		string szPath = it->second->GetValue();
		pItems->ReleasePropList( pProps );
		if ( szPath != "" )
			ExportPersAcks( GetExportSrcDir() + szPath, nPersID );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
