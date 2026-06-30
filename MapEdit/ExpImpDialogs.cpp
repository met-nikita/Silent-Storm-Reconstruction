#include "StdAfx.h"
#include "MapEdit.h"
#include "dbDefs.h"
#include "ItemsMgr.h"
#include "Export.h"
#include "ExportAcks.h"
#include "ExpImpDialogs.h"
#include "..\Misc\StrProc.h"
#include "DialogSequnce.h"
#include <atldbsch.h>
////////////////////////////////////////////////////////////////////////////////////////////////////
const int DIALOG_STRING_FOLDER = 99;
const int DIALOG_SOUND_FOLDER = 361;
////////////////////////////////////////////////////////////////////////////////////////////////////
extern HRESULT InitDB( const CString &szConnection, CSession &session, CDBPropSet &propset );
extern SDBConnection dbConnection;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExcelDialogAccessor
{
public:
	TCHAR m_szWho[255];
	TCHAR m_szPhrase[10240];
	TCHAR m_szIntonation[255];

	BEGIN_ACCESSOR_MAP( CExcelDialogAccessor, 1 )
		BEGIN_ACCESSOR( 0, true )
		COLUMN_ENTRY(1, m_szWho)
		COLUMN_ENTRY(2, m_szPhrase)
		COLUMN_ENTRY(3, m_szIntonation)
	END_ACCESSOR()
	END_ACCESSOR_MAP()
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CExcelDialogDB: public CBaseDBCmd<CAccessor<CExcelDialogAccessor> >
{
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void EraseSeq( const string &szDlg, const vector<SDialogSeqEntry> &sequence, int iSeq )
{
	CDialogSeqDB db;
	db.SetConnection( &dbConnection );
	vector<SAckOperation> ops;

	for ( int i = iSeq; i < sequence.size(); ++i )
	{
		SAckOperation op;
		op.szCondition = szDlg;
		if ( db.DeleteEntry( &op, sequence[i].nSeqID ) )
			ops.push_back( op );
	}
	LogAckOperation( ops, "DialogAcksDeleted" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool GetPers( int *pnPers, int *pnSide, bool *pbHero, const string &szPersCode )
{
	static const SResTree tree = *theApp.GetResTree( IDC_DIALOG_PERS_TREE );
	static const SResTree rpgtree = *theApp.GetResTree( IDC_RPG_PERS_TREE );
	int nID = tree.pItemsTree->GetItemByProp( "Code", szPersCode );
	if ( nID < 0 )
	{
		string sz = "Dialog pers\"";
		sz += szPersCode + "\" not found";
		MessageBox( 0, sz.c_str(), "Warning", MB_OK | MB_ICONWARNING );
		return false;
	}
	const CPropMap *pProps = tree.pItemsTree->GetPropList( nID );
	if ( !pProps )
	{
		ASSERT(0);
		return false;
	}
	CPropMap::const_iterator i = pProps->find( "PersID" );
	CPropMap::const_iterator iHero = pProps->find( "Hero" );
	if ( i == pProps->end() || iHero == pProps->end() )
	{
		ASSERT(0);
		return false;
	}
	*pnPers = i->second->GetValue();
	*pbHero = iHero->second->GetValue();
	tree.pItemsTree->ReleasePropList( pProps );

	pProps = rpgtree.pItemsTree->GetPropList( *pnPers );
	if ( !pProps )
		return false;
	CPropMap::const_iterator iSide = pProps->find( "SideID" );
	if ( iSide == pProps->end() )
	{
		ASSERT(0);
		return false;
	}
	*pnSide = iSide->second->GetValue();
	rpgtree.pItemsTree->ReleasePropList( pProps );

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SPers
{
	int nPersID;
	int nVoice;

	SPers( int nID ): nPersID(nID), nVoice(100) {}
};
bool VoiceCompare( const SPers &a, const SPers &b )
{
	return a.nVoice < b.nVoice;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool GetDefHeroPers( int nSide, vector<int> *pPers )
{
	static const SResTree persTree = *theApp.GetResTree( IDC_RPG_PERS_TREE );
	static const SResTree sideTree = *theApp.GetResTree( IDC_SIDES_TREE );
	//
	const CPropMap *pSP = sideTree.pItemsTree->GetPropList( nSide );
	if ( !pSP )
	{
		MessageBox( 0, "Hero side is unknown", "Error", MB_OK | MB_ICONWARNING );
		return false;
	}
	CPropMap::const_iterator im1 = pSP->find( "Nationality1Male" );
	CPropMap::const_iterator im2 = pSP->find( "Nationality2Male" );
	CPropMap::const_iterator im3 = pSP->find( "Nationality3Male" );
	CPropMap::const_iterator if1 = pSP->find( "Nationality1Female" );
	CPropMap::const_iterator if2 = pSP->find( "Nationality2Female" );
	CPropMap::const_iterator if3 = pSP->find( "Nationality3Female" );
	CPropMap::const_iterator e = pSP->end();

	if ( e == im1 || e == im2 || e == im3 || e == if1 || e == if2 || e == if3 )
	{
		ASSERT(0);
		return false;
	}
	vector<SPers> pers;
	pers.push_back( SPers( im1->second->GetValue() ) );
	pers.push_back( SPers( im2->second->GetValue() ) );
	pers.push_back( SPers( im3->second->GetValue() ) );
	pers.push_back( SPers( if1->second->GetValue() ) );
	pers.push_back( SPers( if2->second->GetValue() ) );
	pers.push_back( SPers( if3->second->GetValue() ) );
	sideTree.pItemsTree->ReleasePropList( pSP );
	for ( vector<SPers>::iterator i = pers.begin(); i != pers.end(); ++i )
	{
		const CPropMap *pPP = persTree.pItemsTree->GetPropList( i->nPersID );
		if ( !pPP )
			continue;
		CPropMap::const_iterator iv = pPP->find( "Voice" );
		if ( iv != pPP->end() )
			i->nVoice = iv->second->GetValue();
		persTree.pItemsTree->ReleasePropList( pPP );
	}
	sort( pers.begin(), pers.end(), VoiceCompare );
	for ( vector<SPers>::iterator i = pers.begin(); i != pers.end(); ++i )
		pPers->push_back( i->nPersID );
	//
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
string CreateAckName( int nPers )
{
	static const SResTree persTree = *theApp.GetResTree( IDC_RPG_PERS_TREE );
	const CPropMap *pProps = persTree.pItemsTree->GetPropList( nPers );
	if ( !pProps )
		return "";
	CPropMap::const_iterator iMaxAck = pProps->find( "MaxAck" );
	if ( iMaxAck == pProps->end() )
		return "";
	int nAck = iMaxAck->second->GetValue();
	iMaxAck->second->SetValue( nAck + 1 );
	string szPers = GetAckName( nPers );
	if ( szPers == "" )
		return "";
	return szPers + IToA( nAck );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void ExportDialog( const string &szDlgName, int nDialogID, const string &szSrc )
{
	if ( szSrc == "" )
		return;
	CString szConnect = "Provider=MSDASQL.1;Persist Security Info=False;Extended Properties=\"DSN=Excel Files;DBQ=";
	szConnect += szSrc.c_str();
	szConnect += ";DriverId=790;MaxBufferSize=2048;PageTimeout=5;\"";

	SDBConnection connection;
	HRESULT hr = InitDB( szConnect, connection.session, connection.propset );
	if ( FAILED( hr ) )
		return;
	CExcelDialogDB db;
	CDialogSeqDB dbSeq;
	CAckInfoDB dbInfo;
	db.SetConnection( &connection );
	//
	const SResTree *pPersTree = theApp.GetResTree( IDC_RPG_PERS_TREE );
	const SResTree *pSidesTree = theApp.GetResTree( IDC_SIDES_TREE );
	if ( !pPersTree || !pSidesTree )
		return;
	CFolderHash strFolderHash;
	CFolderHash sndFolderHash;

	vector<SDialogSeqEntry> seq;
	CDialogSeqDB dbSeq1;
	dbSeq1.GetSequence( &seq, nDialogID );
	int iSeq = seq.empty() ? -1 : 0;
	//
	CTables tables;
	if (tables.Open( connection.session ) != S_OK )
		return;
	string szTable;
	while( tables.MoveNext() == S_OK)
	{
		if ( string("") != tables.m_szName )
		{
			szTable = tables.m_szName;
			break;
		}
	}
	if ( szTable == "" )
		return;
	//
	string szOpen = "SELECT * FROM `";
	szOpen += szTable + '`';
	hr = db.Open( szOpen );
	if ( FAILED( hr ) )
		return;
	vector<SAckOperation> ops;
	while ( db.MoveNext() == S_OK )
	{
		if ( string("") == db.m_szWho || string("") == db.m_szPhrase )
			continue;
		int nPers, nSide;
		bool bHero;

		if ( !GetPers( &nPers, &nSide, &bHero, db.m_szWho ) )
			continue;
		if ( iSeq >= 0 )
		{
			const SDialogSeqEntry &de = seq[iSeq];
			if ( nPers == de.nPersID && de.szText == db.m_szPhrase && de.szIntonation == db.m_szIntonation )
			{
				if ( ++iSeq == seq.size() )
					iSeq = -1;
				//---------------------------------------------
				// CRAP переписывание звука для героев
				/*
				if ( bHero && dbInfo.DeleteSounds( de.nAckInfoID ) )
				{
					vector<int> pers;
					vector<int> sounds;
					if ( GetDefHeroPers( nSide, &pers ) )
					{
						const int nSize = Min( 6, (int)pers.size() );
						for ( int i = 0; i < nSize; ++i )
						{
							string szUserName = CreateAckName( pers[i] );
							if ( szUserName == "" )
								continue;
							int nSndFolder = GetFolder( IDC_SOUNDS_TREE, DIALOG_SOUND_FOLDER, szUserName, sndFolderHash );
							vector<string> vszString;
							NStr::SplitString( szUserName, vszString, '\\' );
							if ( vszString.empty() )
								continue;
							string szWav = string( "Sounds\\Dialogs\\" ) + szUserName + ".wav";
							int nSndID = AddSound( nSndFolder, vszString.back(), szWav );
							if ( nSndID > 0 )
								sounds.push_back( nSndID );
						}
						bool bRet = dbInfo.SetSounds( de.nAckInfoID, 
							sounds.size() > 0 ? sounds[0] : 0, 
							sounds.size() > 1 ? sounds[1] : 0, 
							sounds.size() > 2 ? sounds[2] : 0, 
							sounds.size() > 3 ? sounds[3] : 0, 
							sounds.size() > 4 ? sounds[4] : 0, 
							sounds.size() > 5 ? sounds[5] : 0 );
						ASSERT( bRet );
					}
				}
				*/
				//---------------------------------------------
				continue;
			}
			EraseSeq( szDlgName, seq, iSeq );
			iSeq = -1;
		}
		//
		string szUserName = CreateAckName( nPers );
		if ( szUserName == "" )
			continue;
		int nFolder = GetFolder( IDC_STRINGS_TREE, DIALOG_STRING_FOLDER, szUserName, strFolderHash );
		int nSndFolder = GetFolder( IDC_SOUNDS_TREE, 361, szUserName, sndFolderHash );
		vector<string> vszString;
		NStr::SplitString( szUserName, vszString, '\\' );
		if ( vszString.empty() )
		{
			ASSERT(0);
			continue;
		}
		int nStrID = AddString( nFolder, vszString.back(), db.m_szPhrase );
		if ( nStrID <= 0 )
			continue;
		SAckOperation op;
		op.szCondition = szDlgName;
		op.szText = db.m_szPhrase;
		op.szIntonation = db.m_szIntonation;
		//
		if ( !bHero )
		{
			string szWav = string( "Sounds\\Dialogs\\" ) + szUserName + ".wav";
			int nSndID = AddSound( nSndFolder, vszString.back(), szWav );
			if ( nStrID <= 0 )
				continue;
			op.szFile = szWav;
			int nInfoID = dbInfo.Insert( szUserName, nStrID, nSndID, nPers, db.m_szIntonation );
			dbSeq.InsertEntry( nDialogID, nInfoID );
		}
		else
		{
			vector<int> pers;
			vector<int> sounds;
			if ( !GetDefHeroPers( nSide, &pers ) )
				continue;
			op.b6Voices = true;
			const int nSize = Min( 6, (int)pers.size() );
			for ( int i = 0; i < nSize; ++i )
			{
				string szUserName = CreateAckName( pers[i] );
				if ( szUserName == "" )
					continue;
				int nSndFolder = GetFolder( IDC_SOUNDS_TREE, 361, szUserName, sndFolderHash );
				vector<string> vszString;
				NStr::SplitString( szUserName, vszString, '\\' );
				if ( vszString.empty() )
					continue;
				string szWav = string( "Sounds\\Dialogs\\" ) + szUserName + ".wav";
				int nSndID = AddSound( nSndFolder, vszString.back(), szWav );
				op.szSndFiles[i] = szWav;
				if ( nSndID > 0 )
					sounds.push_back( nSndID );
			}
			int nInfoID = dbInfo.InsertHero( szUserName, nStrID, 
				sounds.size() > 0 ? sounds[0] : 0, 
				sounds.size() > 1 ? sounds[1] : 0, 
				sounds.size() > 2 ? sounds[2] : 0, 
				sounds.size() > 3 ? sounds[3] : 0, 
				sounds.size() > 4 ? sounds[4] : 0, 
				sounds.size() > 5 ? sounds[5] : 0, 
				nPers, db.m_szIntonation );
			dbSeq.InsertEntry( nDialogID, nInfoID );
		}
		ops.push_back( op );
	}
	if ( iSeq >= 0 && !seq.empty() )
		EraseSeq( szDlgName, seq, iSeq );
	LogAckOperation( ops, "DialogAcksInserted" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void ExportDialogs( CItemsMgr *pItems, vector<int> nItemIDs, bool bForceExport )
{
	for ( int i = 0; i < nItemIDs.size(); ++i )
	{
		const CPropMap *pProps = pItems->GetPropList( nItemIDs[i] );
		if ( !pProps )
			continue;
		CPropMap::const_iterator it = pProps->find( "SrcName" );
		if ( it != pProps->end() )
		{
			string szSrc = GetExportSrcDir() + (string)it->second->GetValue();
			ExportDialog( pItems->GetItemName( nItemIDs[i] ), nItemIDs[i], szSrc );
			vector<string> vszPath;
			NStr::SplitString( szSrc, vszPath, '\\' );
			if ( !vszPath.empty() )
			{
				vector<string> vszStr;
				NStr::SplitString( vszPath.back(), vszStr, '.' );
				if ( vszStr.size() == 2 )
				{
					CPropMap::const_iterator icode = pProps->find( "Code" );
					if ( icode != pProps->end() )
						icode->second->SetValue( vszStr.front() );
				}
				else
					ASSERT(0);
			}
		}
		pItems->ReleasePropList( pProps );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CheckAckSeqGarbage()
{
}
void CheckAckInfoGarbage()
{
}
void CheckStringGarbage()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////