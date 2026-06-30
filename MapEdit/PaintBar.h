#ifndef __PAINTBAR_H__
#define __PAINTBAR_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CPaintBar : public SECCustomToolBar
{
public:
	CPaintBar() {};
	~CPaintBar() {};

	//{{AFX_VIRTUAL(CPaintBar)
	protected:
	//}}AFX_VIRTUAL

protected:
	//{{AFX_MSG(CPaintBar)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnChildActivate();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()					
};

#endif // __PAINTBAR_H__
