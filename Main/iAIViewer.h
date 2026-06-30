#ifndef __IAIVIEWER_H_
#define __IAIVIEWER_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "iMain.h"
#include "Camera.h"
namespace NAI
{
	class IAIMap;
	class IPathNetwork;
}
namespace NRPG
{
	class IVisionTracker;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class CICAIView: public NMainLoop::CInterfaceCommand
{
	OBJECT_BASIC_METHODS(CICAIView);
	CPtr<NAI::IAIMap> pMap;
	CPtr<NAI::IPathNetwork> pNet;
	CPtr<NRPG::IVisionTracker> pVision;
	ICamera::SCameraPos cameraPos;
public:
	CICAIView() {}
	CICAIView( NAI::IAIMap *_pMap, NAI::IPathNetwork *_pNetwork, NRPG::IVisionTracker *_pVision, const ICamera::SCameraPos &_cameraPos );
	virtual void Exec();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif