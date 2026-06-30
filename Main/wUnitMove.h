#ifndef __wUnitMove_H_
#define __wUnitMove_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "wEActiveItem.h"

namespace NAI
{
	class CPath;
	struct SPathPlace;
	enum EFindPathParams;
}
namespace NWorld
{
class CCommandExecute;
class CUnitServer;
enum ENeedActiveItem;
////////////////////////////////////////////////////////////////////////////////////////////////////
class IExecMove
{
public:
	struct SPathPoint
	{
		int nAP;
		int nFloor;
		CVec3 vPoint;

		SPathPoint( int _nAP, int _nFloor, CVec3 _vPoint ): nAP( _nAP ), nFloor( _nFloor ), vPoint( _vPoint ) {}
	};

	virtual void GetSearchFromPosition( NAI::SPathPlace *pRes ) = 0;
	virtual void SetNewPath( NAI::CPath *pPath, NAI::EFindPathParams _eParams, ENeedActiveItem eActive = ITEM_NO_MATTER ) = 0;
	virtual void GetDesiredPlace( NAI::SPathPlace *pRes, NAI::EFindPathParams *pParams ) = 0;
	virtual void GetPathPoints( list<SPathPoint> *pRes ) = 0;
	virtual void FullCancel() = 0;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CCommandExecute* CreateSimpleMoveExecutor( CUnitServer *_pUS, NAI::CPath *pPath, NAI::EFindPathParams _eParams,
	ENeedActiveItem eActive = ITEM_NO_MATTER, bool _bCheckCanRotate = true );
}
#endif
