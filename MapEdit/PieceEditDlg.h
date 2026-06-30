#pragma once


// CPieceEditDlg dialog

class CPieceEditDlg : public CDialog
{
	DECLARE_DYNAMIC(CPieceEditDlg)

public:
	CPieceEditDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CPieceEditDlg();

// Dialog Data
	enum { IDD = IDD_PIECE_EDIT };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	int m_nPartX;
	int m_nPartY;
	int m_nPartZ;
	int m_nX;
	int m_nY;
	int m_nZ;
	BOOL m_bx;
	BOOL m_by;
	BOOL m_bz;
	BOOL m_b_x;
	BOOL m_b_y;
	BOOL m_b_z;
};
