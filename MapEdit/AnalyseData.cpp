// AnalyseData.cpp : implementation file
//

#include "stdafx.h"
#include "mapedit.h"
#include "AnalyseData.h"
#include "dbDefs.h"
#include "ItemsMgr.h"
#include "Export.h"
#include "..\DBFormat\DataFormat.h"
#include "..\Misc\StrProc.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
typedef void (CAnalyseDataDlg::*P_ANALYSE_PROC)();
struct SAnalyseEntry
{
	string szName;
	P_ANALYSE_PROC pProc;

	SAnalyseEntry( const string &szStr, P_ANALYSE_PROC pTest ) : szName( szStr ), pProc( pTest ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
SAnalyseEntry analyseProcsTbl[] =
{
	SAnalyseEntry( "Anim src existence", &CAnalyseDataDlg::AnimSrcFilesExistence ),
//	SAnalyseEntry( "Geometries & AIGeometries", &CAnalyseDataDlg::GeometriesAndAIGeometriesTest ),
	SAnalyseEntry( "Geometry src existence", &CAnalyseDataDlg::GeometrySrcFilesExistence ),
	SAnalyseEntry( "AIGeometry src existence", &CAnalyseDataDlg::AIGeometrySrcFilesExistence ),
	SAnalyseEntry( "Texture src existence", &CAnalyseDataDlg::TextureSrcFilesExistence ),
	SAnalyseEntry( "Effects src existence", &CAnalyseDataDlg::EffectsSrcFilesExistence ),
	SAnalyseEntry( "Material textures", &CAnalyseDataDlg::MaterialTexsValidity ),
	SAnalyseEntry( "Particle textures", &CAnalyseDataDlg::ParticleTexsValidity ),
	SAnalyseEntry( "UI textures", &CAnalyseDataDlg::UITexsTexsValidity ),
	SAnalyseEntry( "Acks sound src existence", &CAnalyseDataDlg::AckSoundSrcFilesExistence ),
	SAnalyseEntry( "Texture garbage check", &CAnalyseDataDlg::TextureGarbageCheck ),
	SAnalyseEntry( "Sound garbage check", &CAnalyseDataDlg::SoundGarbageCheck ),
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAnalyseDataDlg dialog


CAnalyseDataDlg::CAnalyseDataDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CAnalyseDataDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CAnalyseDataDlg)
	m_log = _T("");
	//}}AFX_DATA_INIT
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnalyseDataDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAnalyseDataDlg)
	DDX_Control(pDX, IDC_ANALYSEPROC_LIST, m_list);
	DDX_Text(pDX, IDC_ANALYSE_LOG, m_log);
	//}}AFX_DATA_MAP
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnalyseDataDlg::GeometriesAndAIGeometriesTest()
{
	m_log.Empty();
	UpdateData( false );
	UpdateWindow();
	const SResTree *pGeomTree = theApp.GetResTree( IDC_GEOMETRIES_TREE );
	const SResTree *pAIGeomTree = theApp.GetResTree( IDC_AIGEOMETRIES_TREE );
	if ( !pGeomTree || !pAIGeomTree || !pGeomTree->pItemsTree || !pAIGeomTree->pItemsTree )
		return;
	CItemsMgr *pGeoms = pGeomTree->pItemsTree;
	CItemsMgr *pAIGeoms = pAIGeomTree->pItemsTree;
	pGeoms->MoveFirstItem();
	//
	while ( pGeoms->MoveNextItem() )
	{
		const CPropMap *pGProps = pGeoms->GetPropList( pGeoms->GetItemID() );
		if ( !pGProps )
			continue;
		CPropMap::const_iterator isk = pGProps->find( "SkeletonID" );
		CPropMap::const_iterator iai = pGProps->find( "AIGeometryID" );
		if ( pGProps->end() == isk || pGProps->end() == iai )
		{
			m_log += "Cannot find SkeletonID or AIGeometryID in Geometries\nTestFailed\r\n";
			UpdateData( false );
			return;
		}
		// если пустая AIGeometry, то дальше не проверяем
		if ( CVariant::VT_NULL == iai->second->GetValue().GetType() )
			continue;
		const CPropMap *pAIProps = pAIGeoms->GetPropList( iai->second->GetValue() );
		if ( !pAIProps )
		{
			m_log += CString( "AIGeometry not found for GeometryID = " ) + IToA( pGeoms->GetItemID() ).c_str()
							+ " (\"" + pGeoms->GetItemName() + "\")\r\n"; 
			UpdateData( false );
			continue;
		}
		CPropMap::const_iterator iaisk = pAIProps->find( "SkeletonID" );
		if ( pAIProps->end() == iaisk )
		{
			m_log += "Cannot find SkeletonID in AIGeometries\nTestFailed\r\n";
			UpdateData( false );
			return;
		}
		// проверяем соответсвие скелета геометрии и скелета AI геометрии
		if ( int( isk->second->GetValue() ) != int( iaisk->second->GetValue() ) )
		{
			m_log += CString( "Error: SkeletonID & AIGeometry->SkeletonID mismatch for GeometryID = " ) + IToA( pGeoms->GetItemID() ).c_str()
						+ " (\"" + pGeoms->GetItemName() + "\")\r\n"; 
			UpdateData( false );
			UpdateWindow();
		}
		pGeoms->ReleasePropList( pGProps );
		pAIGeoms->ReleasePropList( pAIProps );
	}
	//
	m_log += "\r\nGeometry test completed\r\n";
	UpdateData( false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CAnalyseDataDlg, CDialog)
	//{{AFX_MSG_MAP(CAnalyseDataDlg)
	ON_BN_CLICKED(IDC_ANALYSE, OnAnalyse)
	ON_LBN_DBLCLK(IDC_ANALYSEPROC_LIST, OnDblclkAnalyseprocList)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CAnalyseDataDlg message handlers
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CAnalyseDataDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	const int nTests = ARRAY_SIZE( analyseProcsTbl );
	for ( int i = 0; i < nTests; ++i )
	{
		int ind = m_list.AddString( analyseProcsTbl[i].szName.c_str() );
		m_list.SetItemData( ind, i );
	}
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnalyseDataDlg::OnAnalyse() 
{
	int ind = m_list.GetCurSel();
	if ( LB_ERR == ind )
		return;
	int iTest = m_list.GetItemData( ind );
	if ( !analyseProcsTbl[iTest].pProc )
		return;
	BeginWaitCursor();
	(this->*(analyseProcsTbl[iTest].pProc))();
	EndWaitCursor();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnalyseDataDlg::OnDblclkAnalyseprocList() 
{
	OnAnalyse();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnalyseDataDlg::SrcExistence( string szPrefix, const SResTree *pResTree, int nFolder )
{
	UpdateData( false );
	UpdateWindow();
	if ( !pResTree )
		return;
	CItemsMgr *pTree = pResTree->pItemsTree;
	pTree->MoveFirstItem();
	string szSrcDir = GetExportSrcDir() + szPrefix;
	//
	while ( pTree->MoveNextItem() )
	{
		if ( nFolder != -1 && pTree->GetItemFolderID( pTree->GetItemID() ) != nFolder )
			continue;
		const CPropMap *pProps = pTree->GetPropList( pTree->GetItemID() );
		if ( !pProps )
			continue;
		CPropMap::const_iterator it = pProps->find( "SrcName" );
		if ( pProps->end() == it )
		{
			m_log += "SrcName not found\r\n";
			UpdateData( false );
			return;
		}
		string szSrc = szSrcDir + (string)it->second->GetValue();
		HANDLE hf = CreateFile( szSrc.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0 );

		if ( INVALID_HANDLE_VALUE == hf )
		{
			m_log += CString( "Error: Source does not exist for ID = " ) + IToA( pTree->GetItemID() ).c_str()
				+ " (\"" + pTree->GetItemPath( pTree->GetItemID() ).c_str() + "\")\r\n"; 
			UpdateData( false );
			UpdateWindow();
		}
		else
			CloseHandle( hf );
		pTree->ReleasePropList( pProps );
	}
	//
	m_log += "\r\nTest completed\r\n";
	UpdateData( false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnalyseDataDlg::AnimSrcFilesExistence()
{
	m_log.Empty();
	m_log = "Animation sources:\r\n";
	SrcExistence( "Models\\", theApp.GetResTree( IDC_ANIMATIONS_TREE ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnalyseDataDlg::GeometrySrcFilesExistence()
{
	m_log.Empty();
	m_log = "Geometry sources:\r\n";
		SrcExistence( "Models\\", theApp.GetResTree( IDC_GEOMETRIES_TREE ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnalyseDataDlg::AIGeometrySrcFilesExistence()
{
	m_log.Empty();
	m_log = "AIGeometry sources:\r\n";
		SrcExistence( "Models\\", theApp.GetResTree( IDC_AIGEOMETRIES_TREE ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnalyseDataDlg::TextureSrcFilesExistence()
{
	m_log.Empty();
	m_log = "Texture sources:\r\n";
		SrcExistence( "Textures\\", theApp.GetResTree( IDC_TEXTURES_TREE ) );
}
void CAnalyseDataDlg::AckSoundSrcFilesExistence()
{
	m_log.Empty();
	m_log = "Acks sound sources:\r\n";
	SrcExistence( "", theApp.GetResTree( IDC_SOUNDS_TREE ), 83 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnalyseDataDlg::EffectsSrcFilesExistence()
{
	m_log.Empty();
	m_log = "Effects sources:\r\n";
		SrcExistence( "", theApp.GetResTree( IDC_PARTICLES_TREE ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// возвр. true, если тип текстуры совпадает с одним из szTypes
static bool CheckTextureType( CItemsMgr *pItems, int nTex, const vector<string> &vszTypes )
{
	const CPropMap *props = pItems->GetPropList( nTex );
	if ( !props )
		return false;
	bool bRet = false;
	CPropMap::const_iterator it = props->find( "Type" );
	const string szType = it->second->GetValue();
	for ( int i = 0; i < vszTypes.size(); ++i )
		if ( szType == vszTypes[i] )
		{
			bRet = true;
			break;
		}
	pItems->ReleasePropList( props );
	return bRet;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnalyseDataDlg::MaterialTexsValidity()
{
	const SResTree *pMTree = theApp.GetResTree( IDC_MATERIALS_TREE );
	const SResTree *pTTree = theApp.GetResTree( IDC_TEXTURES_TREE );
	if ( !pMTree || !pTTree )
		return;
	m_log = "Material textures test\r\n\r\n";
	UpdateData( false );
	UpdateWindow();
	CItemsMgr *pM = pMTree->pItemsTree;
	CItemsMgr *pT = pTTree->pItemsTree;
	pM->MoveFirstItem();
	while ( pM->MoveNextItem() )
	{
		const int nID = pM->GetItemID();
		vector<int> vars;
		if ( !pM->GetItemVariants( nID, &vars ) )
		{
			m_log += "\r\nResource format error\r\n";
			return;
		}
		CString szName = pM->GetItemPath( nID ).c_str();
		for ( int i = 0; i < vars.size(); ++i )
		{
			const CPropMap *pProps = pM->GetPropList( nID, vars[i] );
			if ( pProps )
			{
				CPropMap::const_iterator iTex = pProps->find( "TextureID" );
				CPropMap::const_iterator iBump = pProps->find( "BumpID" );
				CPropMap::const_iterator iGloss = pProps->find( "GlossID" );
				CPropMap::const_iterator iMirr = pProps->find( "MirrorID" );
				CPropMap::const_iterator e = pProps->end();
				if ( iTex != e && iBump != e && iGloss != e && iMirr != e )
				{
					vector<string> szTypes;
					szTypes.push_back( "Transparent" );
					szTypes.push_back( "TransparentAdd" );
					CString szStart = szName + "(" + IToA(nID).c_str() + "," + IToA(vars[i]).c_str() + ") : bad texture ";
					int nTex = iTex->second->GetValue();
					int nBump = iBump->second->GetValue();
					int nGloss = iGloss->second->GetValue();
					int nMirr = iMirr->second->GetValue();
					if ( pT->IsExist( nTex ) && CheckTextureType( pT, nTex, szTypes ) )   m_log += szStart + iTex->first.c_str() + "\r\n";
					if ( pT->IsExist( nBump ) && CheckTextureType( pT, nBump, szTypes ) )  m_log += szStart + iBump->first.c_str() + "\r\n";
					if ( pT->IsExist( nGloss ) && CheckTextureType( pT, nGloss, szTypes ) ) m_log += szStart + iGloss->first.c_str() + "\r\n";
					if ( pT->IsExist( nMirr ) && CheckTextureType( pT, nMirr, szTypes ) )  m_log += szStart + iMirr->first.c_str() + "\r\n";
					UpdateData( false );
					UpdateWindow();
				}
				pM->ReleasePropList( pProps );
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnalyseDataDlg::ParticleTexsValidity()
{
	const SResTree *pPTree = theApp.GetResTree( IDC_PARTICLEINSTANCES_TREE );
	const SResTree *pTTree = theApp.GetResTree( IDC_TEXTURES_TREE );
	if ( !pPTree || !pTTree )
		return;
	m_log = "ParticleInstances textures test\r\n\r\n";
	UpdateData( false );
	UpdateWindow();
	CItemsMgr *pP = pPTree->pItemsTree;
	CItemsMgr *pT = pTTree->pItemsTree;
	pP->MoveFirstItem();
	while ( pP->MoveNextItem() )
	{
		const int nID = pP->GetItemID();
		CString szName = pP->GetItemPath( nID ).c_str();
		const CPropMap *pProps = pP->GetPropList( nID );
		if ( pProps )
		{
			for ( int i = 0; i < NDb::N_PARTICLE_TEXTURES; ++i )
			{
				CPropMap::const_iterator iTex = pProps->find( string( "Texture" ) + IToA(i) );
				if ( iTex != pProps->end() )
				{
					vector<string> szTypes;
					szTypes.push_back( "Transparent" );
					szTypes.push_back( "TransparentAdd" );
					CString szStart = szName + "(" + IToA(nID).c_str() + ",-1) : bad texture ";
					if ( pT->IsExist( iTex->second->GetValue() ) && !CheckTextureType( pT, iTex->second->GetValue(), szTypes ) )
					{
						m_log += szName + "(" + IToA(nID).c_str() + ",-1) : bad texture " + iTex->first.c_str() + "\r\n";
						UpdateData( false );
						UpdateWindow();
					}
				}
			}
			pP->ReleasePropList( pProps );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnalyseDataDlg::UITexsTexsValidity()
{
	const SResTree *pUTree = theApp.GetResTree( IDC_UITEXTURES_TREE );
	const SResTree *pTTree = theApp.GetResTree( IDC_TEXTURES_TREE );
	if ( !pUTree || !pTTree )
		return;
	m_log = "UI textures test\r\n\r\n";
	UpdateData( false );
	UpdateWindow();
	CItemsMgr *pU = pUTree->pItemsTree;
	CItemsMgr *pT = pTTree->pItemsTree;
	pU->MoveFirstItem();
	while ( pU->MoveNextItem() )
	{
		const int nID = pU->GetItemID();
		CString szName = pU->GetItemPath( nID ).c_str();
		const CPropMap *pProps = pU->GetPropList( nID );
		if ( pProps )
		{
			CPropMap::const_iterator i1 = pProps->find( "R_800x600" );
			CPropMap::const_iterator i2 = pProps->find( "R_1024x768" );
			CPropMap::const_iterator i3 = pProps->find( "R_1280x960" );
			CPropMap::const_iterator i4 = pProps->find( "R_1600x1200" );
			CPropMap::const_iterator e = pProps->end();
			if ( i1 != e && i2 != e && i3 != e && i4 != e )
			{
				vector<string> szTypes;
				szTypes.push_back( "2D" );
				CString szStart = szName + "(" + IToA(nID).c_str() + ",-1) : bad texture ";
				int n1 = i1->second->GetValue();
				int n2 = i2->second->GetValue();
				int n3 = i3->second->GetValue();
				int n4 = i4->second->GetValue();
				if ( pT->IsExist( n1 ) && !CheckTextureType( pT, n1, szTypes ) ) m_log += szStart + i1->first.c_str() + "\r\n";
				if ( pT->IsExist( n2 ) && !CheckTextureType( pT, n2, szTypes ) ) m_log += szStart + i2->first.c_str() + "\r\n";
				if ( pT->IsExist( n3 ) && !CheckTextureType( pT, n3, szTypes ) ) m_log += szStart + i3->first.c_str() + "\r\n";
				if ( pT->IsExist( n4 ) && !CheckTextureType( pT, n4, szTypes ) ) m_log += szStart + i4->first.c_str() + "\r\n";
				UpdateData( false );
				UpdateWindow();
			}
			pU->ReleasePropList( pProps );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFileIterator
{
	string szDir;
	HANDLE hHandle;
	CItemsMgr *pMgr;
	WIN32_FIND_DATA data;

public:
	CFileIterator( int nTree, const string &_szDir ): szDir(_szDir), hHandle(INVALID_HANDLE_VALUE), pMgr(0)
	{
		const SResTree *pTree = theApp.GetResTree( nTree );
		if ( pTree )
			pMgr = pTree->pItemsTree;
	}

	~CFileIterator() 
	{
		if ( hHandle != INVALID_HANDLE_VALUE )
			FindClose( hHandle );
	}

	bool MoveNext()
	{
		if ( !pMgr )
			return false;
		if ( hHandle == INVALID_HANDLE_VALUE )
		{
			hHandle = FindFirstFile( (szDir + "\\*").c_str(), &data );
			return hHandle != INVALID_HANDLE_VALUE;
		}
		return FindNextFile( hHandle, &data );
	}
	string GetCurrentFile() { return szDir + '\\' + data.cFileName; }
	int GetCurrentFileID()
	{
		string sz = data.cFileName;
		vector<string> vsz;
		NStr::SplitString( sz, vsz, '\\' );
		if ( vsz.empty() )
			return 0;
		return atoi( vsz.back().c_str() );
	}
	bool IsValid( int nID )
	{
		if ( nID <= 0 )
			return true;
		const CPropMap *p = pMgr->GetPropList( nID );
		pMgr->ReleasePropList( p );
		return p;
	}
	string GetCurrentLogString()
	{
		if ( !IsValid( GetCurrentFileID() ) )
			return GetCurrentFile();
		return "";
	}
	int GetCurrentFileSize()
	{
		return data.nFileSizeLow;
	}
	void DeleteCurrentFile()
	{
		DeleteFile( GetCurrentFile().c_str() );
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool DeleteFiles( HWND hWnd )
{
	return IDYES == MessageBox( hWnd, "Delete garbage?", "MapEdit", MB_YESNO | MB_ICONQUESTION );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnalyseDataDlg::TextureGarbageCheck()
{
	CFileIterator fi( IDC_TEXTURES_TREE, GetExportDstDir() + "Textures" );
	int nFileSize = 0;
	bool bDelete = DeleteFiles( m_hWnd );

	m_log = "Texture garbage check\r\n\r\n";
	UpdateData( false );
	UpdateWindow();

	while ( fi.MoveNext() )
	{
		string szLog = fi.GetCurrentLogString();
		if ( szLog != "" && !fi.IsValid( fi.GetCurrentFileID() & ~0x01000000 ) )
		{
			m_log += szLog.c_str();
			m_log += "\r\n";
			UpdateData( false );
			UpdateWindow();
			nFileSize += fi.GetCurrentFileSize();
			if ( bDelete )
				fi.DeleteCurrentFile();
		}
	}
	m_log += "\r\ngarbage size = ";
	m_log += IToA( nFileSize ).c_str();
	UpdateData( false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CAnalyseDataDlg::SoundGarbageCheck()
{
	CFileIterator fi( IDC_SOUNDS_TREE, GetExportDstDir() + "Sounds" );
	int nFileSize = 0;
	bool bDelete = DeleteFiles( m_hWnd );

	m_log = "Sound garbage check\r\n\r\n";
	UpdateData( false );
	UpdateWindow();

	while ( fi.MoveNext() )
	{
		string szLog = fi.GetCurrentLogString();
		if ( szLog != "" )
		{
			m_log += szLog.c_str();
			m_log += "\r\n";
			UpdateData( false );
			UpdateWindow();
			nFileSize += fi.GetCurrentFileSize();
			if ( bDelete )
				fi.DeleteCurrentFile();
		}
	}
	m_log += "\r\ngarbage size = ";
	m_log += IToA( nFileSize ).c_str();
	UpdateData( false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
