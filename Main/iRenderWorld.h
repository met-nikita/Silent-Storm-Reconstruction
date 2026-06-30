#ifndef __A5_IRENDERWORLD_H_
#define __A5_IRENDERWORLD_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
class ICamera;
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CRenderBaseInterface: public NMainLoop::IInterfaceBase
{
	OBJECT_BASIC_METHODS(CRenderBaseInterface);
private:
	NInput::CBind bindShadows, bindSwitchLighting;

	ZDATA
	//// graphics
	CObj<NGScene::IGameView> pScene;
	CObj<NSound::ISoundScene> pSoundScene;
	CObj<NRender::IRenderGame> pRender;
	CObj<NRender::IRenderSound> pRenderSound;
	//// world
	CObj<NWorld::IWorld> pWorld;
	CPtr<NWorld::IPlayer> pPlayer;
	CPtr<NWorld::CCommander> pCommander;
	//// interface
	CObj<NUI::ICursor> pCursor;
	CObj<NUI::CInterface> pInterface;
	//// camera
	CObj<ICamera> pCamera;
	//// crap
	int nLightMode;
	CObj<CObjectBase> pLightSource;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&pScene); f.Add(3,&pSoundScene); f.Add(4,&pRender); f.Add(5,&pRenderSound); f.Add(6,&pWorld); f.Add(7,&pPlayer); f.Add(8,&pCommander); f.Add(9,&pCursor); f.Add(10,&pInterface); f.Add(11,&pCamera); f.Add(12,&nLightMode); f.Add(13,&pLightSource); return 0; }

public:
	CRenderBaseInterface();

	void Initialize( int nTemplate );

	void Command( NWorld::CCommand *pCmd );

	void SetLightMode( int _nLightMode );

	ICamera* GetCamera() const { return pCamera; }
	NUI::ICursor* GetCursor() const { return pCursor; }
	NUI::CInterface* GetInterface() const { return pInterface; }

	NWorld::IWorld* GetWorld() const { return pWorld; }
	NGScene::IGameView* GetScene() const { return pScene; }
	NRender::IRenderGame* GetRenderGame() const { return pRender; }

	void Step();
	void OnGetFocus();
	bool ProcessEvent( const NInput::SEvent &sEvent );
	void RenderFrame( const STime &sTime, ICamera *pCamera );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif