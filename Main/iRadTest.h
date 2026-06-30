#ifndef __IRADTEST_H_
#define __IRADTEST_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "iMain.h"
#include "Camera.h"
namespace NGScene
{
	class IGameView;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
class CICRadTest: public NMainLoop::CInterfaceCommand
{
	OBJECT_BASIC_METHODS(CICRadTest);
	CPtr<NGScene::IGameView> pScene;
	ICamera::SCameraPos cameraPos;
public:
	CICRadTest() {}
	CICRadTest( NGScene::IGameView *_pScene, const ICamera::SCameraPos &_cameraPos );
	virtual void Exec();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif