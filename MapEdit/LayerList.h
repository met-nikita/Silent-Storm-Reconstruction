#ifndef __LAYERLIST_H_
#define __LAYERLIST_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "..\Main\MELayers.h"
#include "MEUserSettings.h"
#include "UserSettingsSetup.h"
#include "LayerCtrl.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// CLayerListBar dialog
class CLayerListBar : public CDialog
{
	// Construction
public:
	CLayerListBar(CWnd* pParent = NULL);   // standard constructor
	
	// Dialog Data
	//{{AFX_DATA(CLayerListBar)
	enum { IDD = IDD_LAYER_LIST };
	CButton	m_del;
	CButton	m_add;
	CButton	m_up;
	CButton	m_down;
	BOOL	m_floor_1;
	BOOL	m_floor_2;
	BOOL	m_floor0;
	BOOL	m_floor1;
	BOOL	m_floor2;
	BOOL	m_floor3;
	BOOL	m_floor4;
	//}}AFX_DATA
	
	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CLayerListBar)
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL
	
	// Implementation
protected:
	BOOL& Floor( int nFloor );
	// Generated message map functions
	//{{AFX_MSG(CLayerListBar)
	virtual BOOL OnInitDialog();
	afx_msg void OnLayerDown();
	afx_msg void OnLayerUp();
	afx_msg void OnLayerAdd();
	afx_msg void OnLayerDel();
	afx_msg void OnCheckFloor();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
////////////////////////////////////////////////////////////////////////////////////////////////////
//! искусственный класс, чтобы обойти баги при отрисовке контрола слоев
class CLayersPlace : public CWnd
{
	//{{AFX_MSG(CLayersPlace)
	afx_msg void OnPaint();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
////////////////////////////////////////////////////////////////////////////////////////////////////
typedef list<CLayerCtrl*> CLayers;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CLayerList : public SECControlBar, public IUserSettings, public IUserSettingsSetup
{
	// Construction
public:
	CLayerList();			//!< standard constructor
	~CLayerList();

	void AddLayer( CLayerCtrl *pLayer );		//!< ƒобавить слой в список.
	void DeleteLayer( int nLayerID );				//!< ”далить из списка слой nLayerID
	void DeleteAllLayers();									//!< ”далить все слои из списка
	void Paint( ITemplateView *pView );
	void ClearSelection();
	void AddNotifyWnd( CWnd *pWnd );
	CLayerCtrl* GetActiveLayer() const;
	bool HasLayer( CLayerCtrl *pLayer );
	bool HasLayer( int nLayerID );
	int  IsLayerVisible( int nLayerID );
	void GetLayerOrder( vector< pair<int, bool> > *pOrder );
	void SetLayerOrder( const vector< pair<int, bool> > &order );
	void Activate( int nLayerID, bool bActivate = true );
	CLayerCtrl* GetLayer( int nLayerID );
	CLayers::iterator begin();
	CLayers::iterator end();
	void ResetLinkedFloors();
	void GetLinkedFloors( vector<int> *pFloors );
	void SetLinkedFloors( const vector<int> &floors );
	void Setup();

	virtual EEditMode GetMode() const;
	virtual EMoveMode GetMoveMode() const;
	virtual int   GetActiveLayerID() const;
	virtual float GetActiveFloor() const;
	virtual int   GetActiveRotationID() const;
	virtual int   GetSelectedBrushID( int nLayerID ) const;
	virtual bool  GetActiveMaterial( vector<NBuilding::SRawMaterialApply> *pMaterials ) const; // sizeof materials = 1 or N_MATERIALSET_SIZE
	virtual bool  CanEditAnyLayer() const;
	virtual void  GetVisibleLayers( vector<int> *pLayers ) const;
	virtual int   GetSpotMaterialID() const { return nSpotMaterialID; }
	virtual bool  IsCameraReset() const;
	virtual CItemsMgr* GetResourceManager( int nResourceTypeID ) const;
	virtual int   GetBrushSize() const;
	virtual bool IsGridVisible() const;
	virtual int  GetSubTemplateDepth() const;
	virtual bool IsWireSubTemplate() const;
	virtual float GetParam( int nParamID ) const;
	virtual CVec3 GetSelectionCenter() const;

	virtual void SetMode( EEditMode mode );
	virtual void PushMode( EEditMode mode );
	virtual void PopMode();
	virtual void SetMoveMode( EMoveMode mode );
	virtual void SetActiveLayerID( int nID );
	virtual void SetActiveFloor( float fFloor );
	virtual void SetActiveRotationID( int nID );
	virtual void SetSelectedBrushID( int nLayerID, int nID );
	virtual void SetMaterial( EMaterialSet set, const vector<NBuilding::SRawMaterialApply> &materials ); // sizeof materials = 1 or N_MATERIALSET_SIZE
	virtual void SetActiveMaterialSet( EMaterialSet set );
	virtual EMaterialSet GetActiveMaterialSet();
	virtual void GetMaterial( EMaterialSet set, vector<NBuilding::SRawMaterialApply> *pMaterials ) const;
	virtual void SetCanEditAnyLayer( bool bCan );
	virtual void SetVisibleLayers( const vector<int> &layers );
	virtual void SetSpotMaterial( int nMaterialID );
	virtual void SendMessage( SMessage msg );
	virtual void SetCameraInfo( const ICamera::SCameraPos &pos, float fFOV );
	virtual void SetDBCameraInfo( const ICamera::SCameraPos &pos, float fFOV );
	virtual void SetBrushSize( int nSize );
	virtual void SetGridVisible( bool bVisible );
	virtual void SetSubTemplateDepth( int nDepth ) {};
	virtual void SetWireSubTemplates( bool bWire );
	virtual void SetParam( int nParamID, float fValue );
	virtual void SetSelectionCenter( const CVec3 &ptCenter );
	virtual void ShowPropertyBrowser( const int nObjectID );

	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CLayerList)
	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	//}}AFX_VIRTUAL
	
// Implementation
protected:
	// Current user settings
	EEditMode eMode;
	EEditMode ePushedMode;
	EMoveMode eMoveMode;
	EMaterialSet eActiveMaterial;
	int nRotationID;
	vector<NBuilding::SRawMaterialApply> materials[MSET_SIZE];
	bool bCanEditAnyLayer;
	int nSpotMaterialID;
	int nBrushSize;
	bool bGridVisible;
	bool bWireSubTemplate;
	unordered_map<int, float> meparams;
	CVec3 ptSelectionCenter;

	CLayersPlace internalWnd;
	CLayerListBar listBar;
	CLayerCtrl invisLayer;
	CLayers    layers;
	vector<CWnd*> notifyWnds;
	unordered_map<int, int> lid2brush;

	int nItemHeight;
	int nItemWidth;
	bool bDirty;
	CPropMap *pPlacementProps;
	CRect rlistbar;

	CLayers::iterator FindLayer( int nLayerID );
	void ArrangeLayers();
	void SendNotify( WPARAM wParam = 0 );
	void SetupScrolls();
	// Generated message map functions
	//{{AFX_MSG(CLayerList)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
//{{AFX_INSERT_LOCATION}}
#endif //__LAYERLIST_H_
