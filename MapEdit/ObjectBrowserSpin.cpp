#include "StdAfx.h"
#include "ObjectBrowserSpin.h"
#include "ObjBrowserConstants.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
CSpin::CSpin( CWnd *pParent, int _nID )
{
	nID = _nID;
	CRect r(0,0,0,0);
	s.pSpin->Create( WS_VISIBLE|WS_CHILD|UDS_HORZ, r, pParent, nID );
	s.pSpin->ShowWindow( SW_SHOW );
	s.pStatic->Create( "", WS_CHILD | WS_DISABLED, r, s.pSpin, 444 );
	nActiveVar = -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSpin::Move( int nColumn, int nRow )
{
	CRect r;

	r.left = nColumn * COLUMN_WIDTH;
	r.right = r.left + SPIN_WIDTH;
	r.top = nRow * SPIN_HEIGHT;
	r.bottom = r.top + SPIN_HEIGHT;
	s.pSpin->MoveWindow( &r );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSpin::SetVariants( const vector<int> &vars )
{
	variants = vars;
	s.pSpin->SetRange( 0, vars.size() - 1 );
	if ( !vars.empty() )
		nActiveVar = 0;
	SetPos( 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSpin::SetActiveVariant( int nID )
{
	for ( int i = 0; i < variants.size(); ++i )
		if ( variants[i] == nID )
		{
			nActiveVar = i;
			SetPos( nActiveVar );
			return true;
		}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSpin::NextVar()
{
	if ( nActiveVar < variants.size() - 1 )
	{
		++nActiveVar;
		SetPos( nActiveVar );
		return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSpin::PrevVar()
{
	if ( nActiveVar > 0 )
	{
		--nActiveVar;
		SetPos( nActiveVar );
		return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CSpin::GetActiveVariant() const
{
	if ( nActiveVar >= 0 && nActiveVar < variants.size() )
		return variants[nActiveVar];
	return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSpin::SetActiveVariantIndex( int nIndex )
{
	if ( nIndex >= 0 && nIndex < variants.size() )
	{
		nActiveVar = nIndex;
		SetPos( nActiveVar );
		return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSpin::SetPos( int nPos )
{
	int nLo, nHi;
	CRect r;

	s.pSpin->GetRange( nLo, nHi );
	s.pSpin->SetPos( nPos );

	if ( nPos <= nLo )
	{
		s.pSpin->GetClientRect( &r );
		r.right /= 2;
		s.pStatic->MoveWindow( &r );
		s.pStatic->ShowWindow( SW_SHOW );
	}
	else if ( nPos >= nHi )
	{
		s.pSpin->GetClientRect( &r );
		r.left = r.right / 2;
		s.pStatic->MoveWindow( &r );
		s.pStatic->ShowWindow( SW_SHOW );
	}
	else
		s.pStatic->ShowWindow( SW_HIDE );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
