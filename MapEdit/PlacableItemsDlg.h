#ifndef __PLACABLEITEMSDLG_H_
#define __PLACABLEITEMSDLG_H_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
//
#include "TreeSelItemDlg.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
class CPlacableItemsDlg: public CTreeSelItemDlg
{
// Construction
	bool MakeItemID( int *pnTree, int *pnID, int nPlacableID );
public:
	CPlacableItemsDlg( const vector<SResTree> &vResTrees );

	virtual void SetSelectedItemID( int nPlaceTreeID, int nPlaceID );
	virtual void GetSelectedItemID( int *pnTree, int *pnID );
	virtual string GetItemPath( int nTreeID, int nItemID );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // __PLACABLEITEMSDLG_H_
