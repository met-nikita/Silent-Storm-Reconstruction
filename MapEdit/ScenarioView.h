#ifndef __SCENARIOVIEW_H_
#define __SCENARIOVIEW_H_
////////////////////////////////////////////////////////////////////////////////////////////////////
class CScenarioView : public CScrollView
{
	// Attributes
public:

	// Operations
public:
	CScenarioView();
	virtual ~CScenarioView();

	void SetScenario( int nID );

protected:
	virtual void OnDraw(CDC* pDC);      // overridden to draw this view
	virtual void OnInitialUpdate();     // first time after construct
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual void PostNcDestroy();

  	// Implementation
protected:
	int nSizeX;
	int nSizeY;

	DECLARE_MESSAGE_MAP()
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __SCENARIOVIEW_H_