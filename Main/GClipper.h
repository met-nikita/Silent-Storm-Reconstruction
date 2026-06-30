#ifndef __CLIPPER_H_
#define __CLIPPER_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "RectLayout.h"
#include "DG.h"

namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CClipper
////////////////////////////////////////////////////////////////////////////////////////////////////
class CClipper: public CFuncBase< CRectLayout >
{
	OBJECT_BASIC_METHODS(CClipper);
	
protected:
	bool NeedUpdate() { return pRects.Refresh() | pWindow.Refresh() | pPosition.Refresh(); }
	void Recalc();
	
public:
	ZDATA
	CDGPtr< CFuncBase< CRectLayout > > pRects;
	CDGPtr< CFuncBase< CTRect<int> > > pWindow;
	CDGPtr< CFuncBase< CTPoint<int> > > pPosition;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pRects); f.Add(3,&pWindow); f.Add(4,&pPosition); return 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void ClipRects( CRectLayout *pRes, const CRectLayout &src, const CTRect<int> &window, const CVec2 &pos );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif