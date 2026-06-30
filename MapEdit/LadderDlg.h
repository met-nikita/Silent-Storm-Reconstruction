#pragma once

namespace NBuilding
{
	struct SLadder;
}
// CLadderDlg dialog
////////////////////////////////////////////////////////////////////////////////////////////////////
class CLadderDlg : public CDialog
{
	DECLARE_DYNAMIC(CLadderDlg)

public:
	CLadderDlg(NBuilding::SLadder *pLadder, CWnd* pParent = NULL);   // standard constructor
	virtual ~CLadderDlg();

// Dialog Data
	enum { IDD = IDD_LADDER };

protected:
	void OnOK();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()
public:
	int m_nLadderSteps;

private:
	NBuilding::SLadder *pLadder;
};
////////////////////////////////////////////////////////////////////////////////////////////////////