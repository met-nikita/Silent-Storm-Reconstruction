// ObjBrowserDlg.cpp : implementation file
//

#include "stdafx.h"
#include "MapEdit.h"
#include "ObjBrowserDlg.h"
#include "OIDlg.h"
#include "ObjBrowserConstants.h"
#include "PropMap.h"
#include "ItemsMgr.h"

// CObjBrowserDlg dialog

IMPLEMENT_DYNAMIC(CObjBrowserDlg, CDialog)
CObjBrowserDlg::CObjBrowserDlg( IObjectBrowser *pOB, CWnd* pParent /*=NULL*/ )
	: CDialog(CObjBrowserDlg::IDD, pParent)
{
	pObjBrowser = pOB;
	nObjectID = -1;
	nVariantID = -1;
	size = CSize(0,0);
}

CObjBrowserDlg::~CObjBrowserDlg()
{
	for ( int i = 0; i < oiCtrls.size(); ++i )
		delete oiCtrls[i];
}

void CObjBrowserDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

void CObjBrowserDlg::Create()
{
	if ( !IsValid( pObjBrowser ) )
		return;
	if ( nObjectID > 0 )
		SetObject( nObjectID, nVariantID );
}

BOOL CObjBrowserDlg::OnInitDialog()
{
	if ( !CDialog::OnInitDialog() )
		return false;
	//Create();
	return true;
}

BEGIN_MESSAGE_MAP(CObjBrowserDlg, CDialog)
	ON_WM_CREATE()
	ON_WM_HSCROLL()
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////////////////////////////
// CObjBrowserDlg message handlers
int CObjBrowserDlg::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CDialog::OnCreate(lpCreateStruct) == -1)
		return -1;

	Create();
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjBrowserDlg::SetObject( int nObjID, int nVarID )
{
	if ( !IsValid( pObjBrowser ) )
		return;
	nObjectID = nObjID;
	nVariantID = nVarID;
	if ( !::IsWindow( m_hWnd ) )
		return;
	for ( vector<COIDlg*>::iterator it = oiCtrls.begin(); it != oiCtrls.end(); ++it )
		(*it)->ShowWindow( SW_HIDE );
	SetObjectInternal();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjBrowserDlg::SetObjectInternal()
{
	unordered_map<int, int> propVariants; // PropID to VariantID
	for ( CSpinMap::const_iterator i = spins.begin(); i != spins.end(); ++i )
		propVariants[i->second->GetID()] = i->second->GetActiveVariant();
	//
	properties.clear();
	spins.clear();

	pObjBrowser->GetProperties( &properties, nObjectID, nVariantID, propVariants );
	ResizeColumns( properties.size() );
	SetWindowPos( 0, 0, 0, COLUMN_WIDTH * oiCtrls.size(), COLUMN_HEIGHT, SWP_NOZORDER | SWP_NOMOVE );

	int i = 0;
	for ( vector<COIDlg*>::iterator it = oiCtrls.begin(); i < properties.size(); ++i, ++it )
	{
		COIDlg *pDlg = *it;
		if ( !::IsWindow( pDlg->m_hWnd ) )
		{
			pDlg->Create( IDD_OBJINSPECTOR, this );
			pDlg->ModifyStyle( 0, WS_BORDER );
		}
		const CPropMap *pProps = &properties[i];
		pDlg->m_wndOI.DrawZeroGroupName( false );
		pDlg->SetPropMap( -1, pProps );
		CRect r( i * COLUMN_WIDTH + SPIN_WIDTH, 0, (i + 1) * COLUMN_WIDTH + 1, COLUMN_HEIGHT );
		::SetWindowPos( pDlg->m_hWnd, NULL, r.left, r.top, r.Width(), r.Height(), SWP_NOZORDER | SWP_NOACTIVATE);
		for ( CPropMap::const_iterator ip = pProps->begin(); ip != pProps->end(); ++ip )
		{
			CProp *p = ip->second;
			int nRel = p->GetRelation();
			if ( nRel < 0 )
				continue;
			const SResTree *pTree = theApp.GetResTree( nRel );
			if ( pTree && pTree->pItemsTree->IsUniTemplate() )
			{
				vector<int> vars;
				pTree->pItemsTree->GetItemVariants( p->GetValue(), &vars );
				if ( vars.size() > 1 )
				{
					CSpin *pSpin = new CSpin( this, p->GetID() );
					spins[p->GetID()] = pSpin;
					spins[p->GetID()]->Move( i, 1 + pDlg->m_wndOI.GetPropertyLine( p->GetID() ) );
					pSpin->SetVariants( vars );
					unordered_map<int, int>::const_iterator iv = propVariants.find( p->GetID() );
					if ( iv != propVariants.end() )
						pSpin->SetActiveVariant( iv->second );
				}
			}
			pDlg->ShowWindow( SW_SHOW );
		}
	}
	size = CSize( COLUMN_WIDTH * properties.size(), COLUMN_HEIGHT );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjBrowserDlg::ResizeColumns( int nSize )
{
	if ( nSize <= oiCtrls.size() )
		return;
	for ( int i = oiCtrls.size(); i < nSize; ++i )
		oiCtrls.push_back( new COIDlg );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CSize CObjBrowserDlg::GetSize()
{
	return size;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CObjBrowserDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	int nID = pScrollBar->GetDlgCtrlID();

	CSpinMap::const_iterator i = spins.find( nID );
	if ( i == spins.end() )
		return;
	bool bRet = false;
	CSpin *pSpin = i->second;
	switch ( nSBCode )
	{
		case SB_LEFT:
			bRet = pSpin->PrevVar();
			break;
		case SB_RIGHT:
			bRet = pSpin->NextVar();
			break;
		case SB_THUMBPOSITION:
			bRet = pSpin->SetActiveVariantIndex( nPos );
			break;
	}
	if ( bRet )
		SetObjectInternal();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CObjBrowserDlg::PreTranslateMessage(MSG* pMsg) 
{
	switch ( pMsg->message )
	{
	case WM_USER + 1:
		SetObjectInternal();
		return true;
	}
	return CDialog::PreTranslateMessage(pMsg);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
