#ifndef __OIRELEDIT_H__
#define __OIRELEDIT_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "OIBrowEdit.h"

class COIRelEdit : public COIBrowseEdit
{
public:
	COIRelEdit();

	void SetTableItemIDs( int nTableID, int nItemID );
	int  GetItemID();
	bool HasNewValue() { return bNewValue; }
	virtual void OnBrowse();
	void SetReadOnly( bool bReadOnly );
	
protected:
	int nTableID;
	int nItemID;
	bool bNewValue;
	bool bReadOnly;
	//{{AFX_MSG(COIRelEdit)  
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	//}}AFX_MSG
	
  DECLARE_MESSAGE_MAP()
		
};

#endif // __OIRELEDIT_H__
