#ifndef __TEMPLATEVIEW_H_
#define __TEMPLATEVIEW_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

const float SELECTION_ACCURACY = 0.45f; // (в тайлах) также определяет размер кружка юнита
enum EEditMode;

class ITemplateView
{
public:
	enum EPaintMode
	{
		PM_PEN_W1, // размер кисти
		PM_PEN_W2,
		PM_PEN_W3,
	};
		
	virtual CDC* GetPaintDC() = 0;

	virtual CWnd* GetWnd() = 0;
	virtual int  GetSpacing() const = 0;
	virtual EEditMode GetPaintMode( int *pExtra = 0 ) const = 0;
	virtual void ScreenToTemplate( CPoint *pPtTempl, const CPoint &pt ) const = 0;
  virtual void ScreenToTemplate( const CPoint &pt, float *pX, float *pY ) const = 0;
  virtual void TemplateToScreen( CPoint *pPtScreen, const CPoint &pt ) const = 0;
	virtual void TemplateToScreen( CPoint *pPtScreen, float x, float y ) const = 0;
	virtual void Repaint() = 0;
	virtual void SetModifiedFlag() = 0;
	virtual bool IsZooming() const = 0;
	virtual void SelectedItem( int nResTreeID, int nItemID, int nObjID ) = 0;
};

#endif // __TEMPLATEVIEW_H_
