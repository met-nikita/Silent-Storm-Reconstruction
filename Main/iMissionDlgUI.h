#ifndef __A5_MISSIONDLG_UI_H__
#define __A5_MISSIONDLG_UI_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
	class CSound;
	class CSequence;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NUI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMissionUI;
class CAnimUnitView;
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SAckEvent
{
	int nPriority;
	wstring wsText;
	CPtr<NWorld::CUnit> pUnit;
	CDBPtr<NDb::CSound> pSound;
	CDBPtr<NDb::CSequence> pSequence;
	// retail SAckEvent tail: the per-phrase facial-expression sequence (UpdatePhrases @0x206d60
	// resolves it via GetSequenceByExpression from the ack's FaceExpression column; first page only)
	CDBPtr<NDb::CSequence> pExpression;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMissionDlgUI
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMissionDlgUI: public CDesktopWindow
{
	OBJECT_NOCOPY_METHODS(CMissionDlgUI);
private:
	NInput::CBind bindCancel, bindNext, bindPrev;
	////
	CObj<NSound::ISound2D> pSound;

	enum EStage
	{
		START,
		FADEIN,
		MOVEUNITVIEWIN,
		SHOWDIALOG,
		////
		FINISH,
		MOVEUNITVIEWOUT,
		FADEOUT
	};
	ZDATA_(CDesktopWindow)
	CPtr<NGame::IMission> pMission;
	CPtr<CDesktopWindow> pTransition;
	////
	int nPanelsStateSave;
	////
	STime sStageTime;
	EStage eStage;
	////
	int nStage;
	vector<SAckEvent> parsedPhrasesSet;
	vector<CObj<NWorld::CUnit> > unitsSet;
	vector<CPtr<NWorld::CAckEvent> > phrasesSet;
	////
	CPtr<CImage> pTopBackground;
	CPtr<CImage> pBottomBackground;
	////
	string szDialogCode;
	CObj<CMLText> pDialog;
	CObj<CHoverButton> pBack;
	CObj<CHoverButton> pNext;
	CObj<CHoverButton> pExit;
	vector<CObj<CAnimUnitView> > unitViewsSet;
	int nID = -1;	// DialogPlay wait id (AddUICommandWithID); EndDialog posts CCmdInterfaceEvent(nID) so WaitForUI unblocks
	// retail CMissionDlgUI pSequenceHolder (s2_types.h:27225): the view whose head currently plays a
	// lipsync sequence -- cleared before a NEW phrase's sequence starts and on skip/close, so a
	// skipped voiceline stops lipsyncing (SetStage @0x2059c0). Continuation pages (null pSequence)
	// deliberately do NOT clear it: the same speaker keeps talking across subtitle pages.
	CPtr<CAnimUnitView> pSequenceHolder;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDesktopWindow*)this); f.Add(2,&pMission); f.Add(3,&pTransition); f.Add(4,&nPanelsStateSave); f.Add(5,&sStageTime); f.Add(6,&eStage); f.Add(7,&nStage); f.Add(8,&parsedPhrasesSet); f.Add(9,&unitsSet); f.Add(10,&phrasesSet); f.Add(11,&pTopBackground); f.Add(12,&pBottomBackground); f.Add(13,&szDialogCode); f.Add(14,&pDialog); f.Add(15,&pBack); f.Add(16,&pNext); f.Add(17,&pExit); f.Add(18,&unitViewsSet); f.Add(19,&nID); f.Add(20,&pSequenceHolder); return 0; }

protected:
	void SetStage( int nStage );
	void UpdatePhrases( NGScene::I2DGameView *pView );
	void StartDialog();
	void EndDialog();

public:
	CMissionDlgUI();
	CMissionDlgUI( const SWindowInfo &sInfo, NGame::IMission *pMission, CDesktopWindow *pTransition, const string &szDialogCode, const vector<CObj<NWorld::CUnit> > &unitsSet, const vector<CPtr<NWorld::CAckEvent> > &phrasesSet, int nID = -1 );

	void ShowDesktop();
	void HideDesktop();
	void UpdateDesktop( const STime &sTime );

	NGame::CUICmdExec* CreateExecutor( NWorld::CUICmd *pCmd );

	bool ProcessEvent( const NInput::SEvent &sEvent );
	bool ProcessMessage( const SEvent &sEvent );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // Namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
