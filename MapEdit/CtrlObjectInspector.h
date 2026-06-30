#if !defined(AFX_CTRLOBJECTINSPECTOR_H__A6751B03_5DCC_4993_8D98_89E650E73626__INCLUDED_)
#define AFX_CTRLOBJECTINSPECTOR_H__A6751B03_5DCC_4993_8D98_89E650E73626__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// CtrlObjectInspector.h : header file
//
//#include <unordered_map>
#include <map>
#include <list>
#include <vector>

#include "Variant.h"

typedef std::list<string> CStlStringList;

typedef int PropID;
typedef int DomenID;
typedef int GroupID;

const PropID PropIDEmpty = -1;
const GroupID GroupIDEmpty = -1;
const GroupID GroupDefault = -2;

const int N_BORDER = 2;
const int N_TEXT_BORDER = 3;

const UINT WM_USER_LOST_FOCUS = WM_USER + 2;

// Domen type ID
enum
{
	DT_ERROR = 0,
	DT_DEC,
	DT_HEX,
	DT_STR,
	DT_BOOL,
  DT_BROWSE,
  DT_COMBO,
	DT_COLOR,
	DT_RELATION,
	DT_SUBPARTS,
	DT_RELLIST,
	DT_PARAMS,
	DT_GEOMETRYPIECES,
	DT_CUSTOM
};

class CListProp;
struct SCOIProperties
{
  PropID  idProp;
	DomenID idDomen;
	GroupID idGroup;
	int nRelation;
	CVariant varValue;
	CVariant varShadowVal;
	string  strName;
  bool    bReadOnly;
  string  szPrefix;
	string  szToolTip;
  vector<string> szStrs;  // пол€ комбо-бокса
	CPtr<CListProp> pListProps; // списочное св-во
};
typedef std::map<PropID, SCOIProperties> CCOIPropMap; // хранить все свойства по ID
typedef std::list<SCOIProperties*> CCOIPropPtrs;

struct SCOIGroup
{
	bool    isExpand;
	bool    isVisible;
  bool    bRadioGroup;
  PropID  iActiveProp;  // если это радио-группа, то здесь сохран€етс€ текущий активный элемент
	CCOIPropPtrs aPorops;
//	GroupID idGroup;
	string strGroupName;
	SCOIGroup() : isExpand(true), isVisible(true), strGroupName("Unnamed") {}
};
typedef std::map<GroupID, SCOIGroup> CCOIGpoupMap;

struct SCOIPaintElem
{
	SCOIProperties *pProp;
	SCOIGroup *pGroup;
	SCOIPaintElem() : pProp(0), pGroup(0) {}
};

typedef std::vector<SCOIPaintElem> CCOIPaintElemVector;

////////////////////////////////////////////////////////////////////////////////////////////////////

struct SCOICustomListDomen
{
	CStlStringList aValueSet;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// CCtrlObjectInspector window

class COIEdit;
class COICombo;
class COIBrowseEdit;
class COIColorEdit;
class COIRelEdit;
class COISubPartsEdit;
class COIRelList;
class COIExEdit;
class COIParamsEdit;
class COIGeometryPiecesEdit;

class CCtrlObjectInspector : public CWnd
{
private:
	// Draw data
	bool		m_haveFocus;
	CSize		m_sizeClient;
	int			m_nLineHeight;
	CFont		m_fntDef, m_fntDefBold;
	COIExEdit	*const m_pEdit;
	COICombo	*const m_pCombo;
  COIBrowseEdit *const m_pBEdit;
	COIColorEdit  *const m_pCEdit;
	COIRelEdit    *const m_pREdit;
	COISubPartsEdit *const m_pSEdit;
	COIRelList *const m_pRellist;
	COIParamsEdit *const m_pParams;
	COIGeometryPiecesEdit *const m_pPiecesEdit;
	CButton		m_button;
	int			m_nSplitterPos;
  CWnd    *pActiveWnd;
	CToolTipCtrl m_ToolTips;

	// Logic data
	CCOIPropMap		m_mapProps;
	CCOIGpoupMap	m_mapGroups;	// все группы
	CCOIPaintElemVector m_aPaintElems;

	int m_nFirstElem;	// персва€ строчка котора€ видна на экране из m_aPaintElems
	int	m_nCurVirtualLine; // текущий выбранный елемент из m_aPaintElems
	int m_nCurGroup;
	int nItemID;
	int nOwnerID;
	bool bDrawZeroGroup;

	// Private methods
	void Init();
	void MakePaintList();
	void UpdateScrollers( int nFirstVirtualLine = -1 );

	CRect GetPaintColPartRect( int nPaintLine, int nCol );
	CRect GetTextColPartRect( int nPaintLine, int nCol );
	CRect GetPaintLineRect( int nPaintLine );
	void ProcessKeyInput( UINT nChar );
	void SelectRow( int nVirtualLine, bool needHide = false );
	void ExpandGroup( bool needInverse = false, bool isExpand = true );
	void LooseFocus();
	void DrawPlus( CDC* pDC, int nLine, bool isPlus );

	CRect	GetPlusRect( int nPaintLine ) const	{ int nSideSize = ( m_nLineHeight / 4 ); return CRect( nSideSize, nSideSize + nPaintLine * m_nLineHeight, m_nLineHeight - nSideSize, (nPaintLine+1) * m_nLineHeight - nSideSize ); }
//	int		GetPaintLineCount() const			{ return ( m_sizeClient.cy - N_BORDER * 2 ) / m_nLineHeight + 1; }	//  оличество линий которое умещаетс€ в окне
	int		GetPaintLine( const CPoint &point ) const { return ( point.y / m_nLineHeight ); } // 0 - if then click on caption
	int		GetCol( const CPoint &point ) const { return point.x > m_nSplitterPos; }
	int		GetLineCount() const				{ return m_sizeClient.cy / m_nLineHeight - 1; }
	int		PaintLineToVirtual( int nPaintLine ) const { return m_nFirstElem + nPaintLine - 1; }
	int		VirtualToPaintLine(int nVirtualLine) const { return nVirtualLine - m_nFirstElem + 1; }
	SCOIPaintElem *GetVirtualElem( int nElem )		{ return ( nElem > -1 && nElem < m_aPaintElems.size() ) ? &m_aPaintElems[nElem] : 0; }

// Construction
public:
	CCtrlObjectInspector();

// Attributes
public:

// Operations
public:
	// General operations
	void ClearAll();
	
	void SetItemID( int nID ) { nItemID = nID; }
	int  GetItemID() const { return nItemID; }
	void SetOwnerID( int _nOwnerID ) { nOwnerID = nOwnerID; }
	int GetOwnerID() const { return nOwnerID; }
	// Domen operation
	// nNewDomenID must DT_CUSTOM + Number
	void AddCustomDomen( PropID idNewDomen, int eBaseDomenType );
	void AddCustomListDomen( PropID idNewDomen, int eBaseDomenType, CStlStringList );
	bool IsValidDomen( DomenID idDomen );

	// Properties operation
	void SetGroup( GroupID idGroup, const string strName, bool bRadioGroup = false );
	bool AddPropertiesValue( PropID idProp, DomenID idDomen, const string &strName, const CVariant &var, 
                           GroupID idGroup = GroupDefault, const string &szToolTip = "", bool bReadOnly = false, 
													 const char *szDirPrefix = 0, CVariant varShadowVal = CVariant(), bool bRepaint = true );
	void SetListProperties( PropID idProp, CListProp *pProp );
	void Update();
  void AddPropertyString( PropID idProp, const string &szStr );
	void SetPropertyRelation( PropID idProp, int nRelationID );
  CVariant GetPropertyValue( PropID idProp );
  string GetPropertyName( PropID idProp );
	string GetGroupName( GroupID idGroup ) const;
	bool SetPropertiesValue( PropID idProp, const CVariant &var, const CVariant shadowVal = CVariant() );
	void SelectProperties( PropID nID );
	PropID HitTest( CPoint ptClient );	// PropIDEmpty if not click in properties
  PropID GetActiveProp( int nGroupID );
	void SetActiveProp( PropID nID );
	void CollapseGroup( GroupID idGroup );
	int  GetNumProps( GroupID idGroup ) const;
	void DrawZeroGroupName( bool bDraw );
	int  GetPropertyLine( PropID idProp );
	
	// Events
	virtual void OnPropertiesChangeOK( PropID idProp, const CVariant &var ) {}
	virtual void OnEditCustomDomen( DomenID idDomen, CVariant &var ) {}

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CCtrlObjectInspector)
	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CCtrlObjectInspector();

	// Generated message map functions
protected:
	//{{AFX_MSG(CCtrlObjectInspector)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnPaint();
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnKillFocus(CWnd* pNewWnd);
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnClose();
	afx_msg void OnNcLButtonDown(UINT nFlags, CPoint point);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

////////////////////////////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CTRLOBJECTINSPECTOR_H__A6751B03_5DCC_4993_8D98_89E650E73626__INCLUDED_)
