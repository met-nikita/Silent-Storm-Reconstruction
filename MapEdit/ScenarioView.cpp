#include "StdAfx.h"
#include "mapedit.h"
#include "ScenarioView.h"
#include "..\Main\scFlowChart.h"
#include "..\DBFormat\DataScenario.h"
#include <foundation\image\mfc\secjpeg.h>
#include "dbDefs.h"
#include "ItemsMgr.h"

using namespace stingray;
using namespace foundation;
static SECJpeg jpg;

BEGIN_MESSAGE_MAP(CScenarioView, CScrollView)
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioView
CScenarioView::CScenarioView()
{
	nSizeX = 0;
	nSizeY = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CScenarioView::~CScenarioView()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CScenarioView::PreCreateWindow(CREATESTRUCT& cs) 
{
	if (!CScrollView::PreCreateWindow(cs))
		return FALSE;

	//cs.dwExStyle |= WS_EX_CLIENTEDGE;
	cs.style &= ~WS_BORDER;
	cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS|CS_BYTEALIGNCLIENT, 
		::LoadCursor(NULL, IDC_ARROW), /*HBRUSH(COLOR_WINDOW+1)*/0, NULL);

	return TRUE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioView::OnInitialUpdate()
{
	CScrollView::OnInitialUpdate();

	SetScrollSizes(MM_TEXT, CSize( nSizeX, nSizeY ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioView::OnDraw(CDC* pDC)
{
	jpg.StretchDIBits( pDC, 0, 0,	nSizeX,	nSizeY,	0, 0,	nSizeX,	nSizeY,	jpg.m_lpSrcBits, jpg.m_lpBMI,	DIB_RGB_COLORS,	SRCCOPY	);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenarioView::SetScenario( int nID )
{
	nSizeX = 0;
	nSizeY = 0;

	Sleep(10);
	NDatabase::Refresh<NDb::CDBScenarioZone>();
	NDatabase::Refresh<NDb::CDBScenarioState>();
	NDatabase::Refresh<NDb::CDBScenarioObjective>();
	NDatabase::Refresh<NDb::CDBScenarioClue>();
	NDatabase::Refresh<NDb::CDBScenarioObjective2Clue>();

	const SResTree *pTree = theApp.GetResTree( IDC_SCENARIOS_TREE );
	if ( !pTree )
		return;
	const CPropMap *pProps = pTree->pItemsTree->GetPropList( nID );
	if ( !pProps )
		return;
	CPropMap::const_iterator it = pProps->find( "FullGraph" );
	bool bFull = it != pProps->end() ? it->second->GetValue() : true;
	CObj<NScenario::CScenarioFlowChart> pS = NScenario::CreateScenarioFlowChart( nID, bFull );

	char szTempPath[MAX_PATH];
	if ( !GetTempPath( sizeof(szTempPath), szTempPath ) )
		return;
	string szPath = szTempPath;
	szPath += "scenario";
	string szFile = szPath + ".jpg";
	DeleteFile( szFile.c_str() );
	pS->Draw( list<CPtr<NScenario::CScenarioZone> >(), list<CPtr<NScenario::CScenarioClue> >(), szPath.c_str() );


	if ( jpg.LoadImage( szFile.c_str() ) )
	{
		nSizeX = jpg.dwGetWidth();
		nSizeY = jpg.dwGetHeight();
	}
	SetScrollSizes(MM_TEXT, CSize( nSizeX, nSizeY ) );
	Invalidate();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioView message handlers
void CScenarioView::PostNcDestroy() 
{
//	CScrollView::PostNcDestroy();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
