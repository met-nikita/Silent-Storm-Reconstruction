#ifndef __OICOLOREDIT_H__
#define __OICOLOREDIT_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "OIBrowEdit.h"

class COIColorEdit : public COIBrowseEdit
{
public:
	virtual void OnBrowse();

protected:
	//{{AFX_MSG(COIColorEdit)  
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	//}}AFX_MSG
	
  DECLARE_MESSAGE_MAP()
		
};

#endif // __OICOLOREDIT_H__
