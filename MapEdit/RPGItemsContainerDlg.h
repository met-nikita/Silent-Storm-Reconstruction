#pragma once
#include "itemslistdlg.h"
#include "afxwin.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// CRPGItemsContainerDlg dialog
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRPGItemsContainerDlg : public CPropertySheet
{
	DECLARE_DYNAMIC(CRPGItemsContainerDlg)

public:
	CRPGItemsContainerDlg( int nRPGPersID, CWnd* pParent = NULL);   // standard constructor
	virtual ~CRPGItemsContainerDlg();

// Dialog Data
	enum { IDD = IDD_RPG_ALL_ITEMS };
	void SetRPGPersID( int nPersID );

protected:

	DECLARE_MESSAGE_MAP()
	CRPGItemsListPropPage m_ctrlWeapons;
	CRPGItemsListPropPage m_ctrlClips;
	CRPGItemsListPropPage m_ctrlGrenades;
	CRPGItemsListPropPage m_ctrlFirstAids;
	CRPGItemsListPropPage m_ctrlMineDetectors;
	CRPGItemsListPropPage m_ctrlMelee;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
