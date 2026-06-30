#ifndef __GLOBALMAPVIEW_H_
#define __GLOBALMAPVIEW_H_

#include "ChapterView.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
class CGlobalInfo;
class CGlobalMapView: public CChapterView
{
protected:
	int nGlobalMapID;
	CObj<CGlobalInfo> pInfo;
	void UpdateGlobalMapInfo();
	bool SerializeGlobalMap( int nMapID, CGlobalInfo *pInfo );
	virtual void OnContourProps();

public:
	CGlobalMapView();

	void SetGlobalMap( int nGlobalMapID );

	void PostNcDestroy();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __GLOBALMAPVIEW_H_