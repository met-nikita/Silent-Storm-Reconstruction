#pragma once


// CAnimFlagsDlg dialog

class CAnimFlagsDlg : public CDialog
{
	DECLARE_DYNAMIC(CAnimFlagsDlg)

public:
	CAnimFlagsDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CAnimFlagsDlg();

	void SetAnimationID( int nID );
// Dialog Data
	enum { IDD = IDD_ANIM_FLAGS };

protected:
	int nAnimationID;

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	void UpdateItem( int nID );
	DECLARE_MESSAGE_MAP()
public:
	BOOL bPoseStand;
	BOOL bPoseCrouch;
	BOOL bPoseCrawl;
	BOOL bWeaponNoWeapon;
	BOOL bWeaponItem;
	BOOL bWeaponPistol;
	BOOL bWeaponRifle;
	BOOL bWeaponSubMachineGun;
	BOOL bWeaponMachineGun;
	BOOL bWeaponRLauncher;
	BOOL bWeaponKnife;
	BOOL bWeaponMachete;
	BOOL bWeaponKatana;
	BOOL bWeaponMDetector;
	BOOL bWeaponPKShooter;
	BOOL bWeaponPKSlasher;
	BOOL bWeaponPKRepairer;
	BOOL bMale;
	BOOL bFemale;
	BOOL bCombat;
	BOOL bRealtime;
	BOOL bClassEngineer;
	BOOL bClassGrenadier;
	BOOL bClassMedic;
	BOOL bClassScout;
	BOOL bClassSniper;
	BOOL bClassSoldier;
	BOOL bClassEnemy;
	afx_msg void OnBnClickedPoseStand();
	afx_msg void OnBnClickedPoseCrouch();
	afx_msg void OnBnClickedPoseCrawl();
	afx_msg void OnBnClickedWeaponNoweapon();
	afx_msg void OnBnClickedWeaponItem();
	afx_msg void OnBnClickedWeaponPistol();
	afx_msg void OnBnClickedWeaponRifle();
	afx_msg void OnBnClickedWeaponSubmachinegun();
	afx_msg void OnBnClickedWeaponMachinegun();
	afx_msg void OnBnClickedWeaponRlauncher();
	afx_msg void OnBnClickedWeaponKnife();
	afx_msg void OnBnClickedWeaponMachete();
	afx_msg void OnBnClickedWeaponKatana();
	afx_msg void OnBnClickedWeaponMdetector();
	afx_msg void OnBnClickedWeaponPkshooter();
	afx_msg void OnBnClickedWeaponPkslasher();
	afx_msg void OnBnClickedWeaponPkrepairer();
	afx_msg void OnBnClickedMale();
	afx_msg void OnBnClickedFemale();
	afx_msg void OnBnClickedCombat();
	afx_msg void OnBnClickedRealtime();
	afx_msg void OnBnClickedClassEngineer();
	afx_msg void OnBnClickedClassGrenadier();
	afx_msg void OnBnClickedClassMedic();
	afx_msg void OnBnClickedClassScout();
	afx_msg void OnBnClickedClassSniper();
	afx_msg void OnBnClickedClassSoldier();
	afx_msg void OnBnClickedClassEnemy();
	BOOL bWeaponPlazmaGun;
	BOOL bWeaponPKPlazmaGun;
	afx_msg void OnBnClickedWeaponPlazmagun();
	afx_msg void OnBnClickedWeaponPkPlazmagun();
};
