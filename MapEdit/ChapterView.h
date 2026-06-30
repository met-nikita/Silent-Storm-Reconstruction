#if !defined(AFX_CHAPTERVIEW_H__B8F1298E_5C73_4CA7_AC4A_6F382682014F__INCLUDED_)
#define AFX_CHAPTERVIEW_H__B8F1298E_5C73_4CA7_AC4A_6F382682014F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ChapterView.h : header file
//
#include "TemplateView.h"
#include "2DEditor.h"
#include "PropMap.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
class CHeightsLayer;
class CChapterInfoLoader;
class CChapterInfo;
class CSectorCtrl;
namespace NGfx
{
	struct SPixel8888;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CChapterView view
class CChapterView : public CScrollView
{
// Attributes
public:

// Operations
public:
	CChapterView();
	virtual ~CChapterView();

	void SetChapter( int nID );
	void SetModified() { bModified = true; }

	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CChapterView)
	protected:
	virtual void OnDraw(CDC* pDC);      // overridden to draw this view
	virtual void OnInitialUpdate();     // first time after construct
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual void PostNcDestroy();
	//}}AFX_VIRTUAL

// Implementation
private:
	CObj<CChapterInfo> pChapter;
	int nChapterID;
	CVec2 vStartPoint;
	void UpdateChapterData();
	bool SerializeChapter( int nChapterID, CChapterInfo *pChapter );

protected:
	bool bModified;
	CMenu m_popup;
	vector<CObj<CSectorCtrl> > sectors;
	CPoint ptRightClick;
	CPtr<CSectorCtrl> pDrag;
	int nPointID;
	bool bDrag;
	CPropMap props;
	CBitmap bmpBackground;
	int nBackgroundID;
	bool bDrawStartPoint;

	void Paint( const CPoint &pt );
	bool RegionHitTest( const CVec2 &pt, int *pSectorID, int *pPointID );
	bool StartPointHitTest( const CVec2 &pt );
	void SetBackground();
	void SetBackground( const CArray2D<NGfx::SPixel8888> &screen );
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
	//{{AFX_MSG(CChapterView)
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnImport();
	afx_msg void OnNewContour();
	afx_msg void OnDelContour();
	virtual afx_msg void OnContourProps();
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
const int CHAPTER_SIZE_X = 1024;
const int CHAPTER_SIZE_Y = 768;
const int SEGMENT_LEN  = 20;
const int HIT_ACCURACY = 25;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CChapterProp: public CProp
{
	OBJECT_BASIC_METHODS(CChapterProp)
private:
	CVariant     value;
	CVariant     defValue;
	CChapterView *pChapterView;
  
public:
	CChapterProp() {ASSERT(0);}
  CChapterProp( const string &szName, int nID, int nType, int nViewType, CChapterView *pView, CVariant defValue = CVariant(), bool bReadOnly = false );
  
  const CVariant& GetValue() const { return value; }
	const CVariant GetDefValue() const { return defValue; }
  void SetValue( const CVariant &value, bool bModified = true ) const;
	CProp* Clone() const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CHAPTERVIEW_H__B8F1298E_5C73_4CA7_AC4A_6F382682014F__INCLUDED_)
