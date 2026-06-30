#pragma once


const int N_EFFECTORS = 16;
void ReadEffectorPrefs();
void WriteEffectorPrefs();

// CAnimItemsDlg dialog
class CAnimItemsDlg : public CDialog
{
	DECLARE_DYNAMIC(CAnimItemsDlg)

public:
	CAnimItemsDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CAnimItemsDlg();

// Dialog Data
	enum { IDD = IDD_ANIM_ITEMS };

protected:
	vector<pair<string, int> > vAnimItems;
	int nHeadID;
	void SelectModel( const string &szEffector, CButton *pBtn );
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnOK();

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedAnimHead();
	afx_msg void OnBnClickedAnimItem();
	afx_msg void OnBnClickedAnimCap();
	afx_msg void OnBnClickedAnimBackpack();
	afx_msg void OnBnClickedAnimRifle();
	afx_msg void OnBnClickedAnimSmg();
	afx_msg void OnBnClickedAnimMg();
	afx_msg void OnBnClickedAnimRl();
	afx_msg void OnBnClickedSlot1();
	afx_msg void OnBnClickedSlot2();
	afx_msg void OnBnClickedSlot3();
	afx_msg void OnBnClickedSlot4();
	afx_msg void OnBnClickedSlot5();
	afx_msg void OnBnClickedSlot6();
	afx_msg void OnBnClickedSlot7();
	afx_msg void OnBnClickedSlot8();
	afx_msg void OnBnClickedSlot9();
	virtual BOOL OnInitDialog();
	CButton m_head;
	CButton m_item;
	CButton m_cap;
	CButton m_backpack;
	CButton m_rifle;
	CButton m_smg;
	CButton m_mg;
	CButton m_rl;
	CButton m_slot1;
	CButton m_slot2;
	CButton m_slot3;
	CButton m_slot4;
	CButton m_slot5;
	CButton m_slot6;
	CButton m_slot7;
	CButton m_slot8;
	CButton m_slot9;
};
