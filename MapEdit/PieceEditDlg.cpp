// PieceEditDlg.cpp : implementation file
//

#include "stdafx.h"
#include "MapEdit.h"
#include "PieceEditDlg.h"


// CPieceEditDlg dialog

IMPLEMENT_DYNAMIC(CPieceEditDlg, CDialog)
CPieceEditDlg::CPieceEditDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CPieceEditDlg::IDD, pParent)
	, m_nPartX(0)
	, m_nPartY(0)
	, m_nPartZ(0)
	, m_nX(0)
	, m_nY(0)
	, m_nZ(0)
	, m_bx(FALSE)
	, m_by(FALSE)
	, m_bz(FALSE)
	, m_b_x(FALSE)
	, m_b_y(FALSE)
	, m_b_z(FALSE)
{
}

CPieceEditDlg::~CPieceEditDlg()
{
}

void CPieceEditDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_PART_X, m_nPartX);
	DDX_Text(pDX, IDC_PART_Y, m_nPartY);
	DDX_Text(pDX, IDC_PART_Z, m_nPartZ);
	DDX_Text(pDX, IDC_PIECE_X, m_nX);
	DDX_Text(pDX, IDC_PIECE_Y, m_nY);
	DDX_Text(pDX, IDC_PIECE_Z, m_nZ);
	DDX_Check(pDX, IDC_X, m_bx);
	DDX_Check(pDX, IDC_Y, m_by);
	DDX_Check(pDX, IDC_Z, m_bz);
	DDX_Check(pDX, IDC_X2, m_b_x);
	DDX_Check(pDX, IDC_Y2, m_b_y);
	DDX_Check(pDX, IDC_Z2, m_b_z);
}


BEGIN_MESSAGE_MAP(CPieceEditDlg, CDialog)
END_MESSAGE_MAP()


// CPieceEditDlg message handlers
