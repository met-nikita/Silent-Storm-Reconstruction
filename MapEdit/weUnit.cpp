#include "StdAfx.h"
#include "weInterface.h"
#include "..\dbFormat\DataMap.h"
#include "..\dbFormat\DataRPG.h"
#include "..\dbFormat\DataFormat.h"
#include "..\dbFormat\DataAnimation.h"
#include "..\dbFormat\DataGeometry.h"
#include "iWysiwyg.h"
#include "..\Main\gAnimation.h"
#include "..\Main\Grid.h"
#include "..\Main\Transform.h"
#include "..\Main\gSceneUtils.h"
#include "..\Main\gView.h"
#include "..\Main\aiPosition.h"
#include "..\Main\wUnitCommands.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CEditorUnit: public IEditorUnit, public IVisObj
{
	OBJECT_NOCOPY_METHODS(CEditorUnit)

	CDBPtr<NDb::CUnit> pUnit;
	CPtr<IEditorWorld> pWorld;
	CObj<NDb::CModel> pModel;
	SFBTransform position;
	int nUserID;
	CSyncSrcBind<IVisObj> bindGlobal;
	CObj<NGScene::CCFBTransform> pTransform;
	CObj<NGScene::CLightGroup> pLG;

	CVec3 MakePosition() const;
	SFBTransform MakeTransform() const { return ::MakeTransform( MakePosition(), pUnit->fRotation + 90 ); }
	void  CreateModel();

public:
	CEditorUnit() {}
	CEditorUnit( IEditorWorld *pWorld, NDb::CUnit *pUnit, const SFBTransform &pos );

	virtual void Visit( IRenderVisitor *p );
	virtual void Visit( IAIVisitor *p );

	virtual NDb::CUnit* GetDBUnit() { return pUnit; }
	virtual void Update();

	// NWorld::CUnit implementation
	virtual bool IsMoving() const { return false; }
	virtual bool IsDead() const { return false; }
	virtual bool IsUnconscious() const { return false; }
	virtual bool IsStrafing() const { return false; }
	virtual bool IsCarryingCorpse() const { return false; }
	virtual CUnit* GetCorpseCarrier() const { return 0; }
	virtual NDb::CModel* GetModel() const { return pModel; }
	virtual void GetVisible( vector<CPtr<CUnit> > *pTarget ) const {}
	virtual void GetInfo( NRPG::SUnitInfo *pInfo ) const {}
	virtual IPlayer* GetPlayer() const { return 0; }
	virtual NAI::CPath* GetCurrentPath() { return 0; }
	virtual IPathViewer* CreatePathViewer() { return 0; }
	virtual NRPG::IUnitMissionInfo* GetRPG() const { return 0; }
	virtual const NAI::SUnitPosition& GetPosition() const { static NAI::SUnitPosition pos; return pos; }
	virtual NAI::SUnitPosition GetSetPosePosition() const { return GetPosition(); }   // editor unit never moves
	virtual void AddVisitableChildren( vector<IVisObj*> *pRes ) {}
	virtual bool GetCurrentCommandName( string *pName ) const { return false; }
	virtual CVec3 GetAttackOrigin() const { return VNULL3; }
	virtual CVec3 GetAttackOrigin( const NAI::SUnitPosition &from ) const { return VNULL3; }
	virtual float GetMinClearDistance() const { return 0; }
	virtual const CObjectBase* GetAttackIgnore() const { return false; }
	virtual EUnitCommandResult CanDo( CCmd *p, int *pnStartAP = 0, int *pnFullAP = 0 ) { return UCR_UNAVAILABLE; }
	virtual bool HasEnoughAP() { return false; }
	virtual EState GetState() { return ST_NORMAL_DEFAULT; }
	virtual NDb::CComplexHead* GetDBHead() { return pUnit->pMonster->pHead; }
	virtual bool IsCapPresent() { return false; }
	virtual int GetCarefulShotExtraAP() { return 0; }
	virtual void GetRealPosition( CVec3 *pRes ) {}
	virtual bool IsCheatEnabled( int nCheat ) { return true; }
	virtual bool IsEmptyPK() const { return false; }
	virtual bool CanStrafe() { return true; }
	virtual NDb::CPanzerklein* GetWearingDBPK() { return 0; }
	virtual bool IsUnitVisible( const NWorld::CUnit* ) const { return false; }
	virtual bool IsUnitAudible(const NWorld::CUnit* ) const { return false; }
	virtual bool IsHiding() const { return false; }
	virtual CObjectBase* GetAIMapHull() { return 0; }
	virtual bool CanTalk() const { return false; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CEditorUnit::CEditorUnit( IEditorWorld *_pWorld, NDb::CUnit *_pUnit, const SFBTransform &pos )
: pWorld(_pWorld), pUnit(_pUnit), position(pos)
{
	bindGlobal.Link( pWorld->GetActive(), this );
	nUserID = NWysiwyg::MakeUserID( BT_UNIT, pUnit->GetRecordID() );

	CreateModel();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorUnit::CreateModel()
{
	static SRand rand;
	pModel = IsValid( pUnit->pMonster ) && IsValid( pUnit->pMonster->pModel ) && IsValid( pUnit->pVariant ) ? pUnit->pMonster->pModel->CreateModel( &rand ) : 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorUnit::Visit( IRenderVisitor *p )
{
	if ( !IsValid( pModel ) )
		return;
	SFBTransform pos = MakeTransform();
	pTransform = new NGScene::CCFBTransform( pos );
	pLG = p->MakeGroup();
	p->AddMesh( pModel, pos, 0, pUnit->nFloor );
	//p->AddHead( this, pTransform, NGScene::SRoomInfo( pLG ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorUnit::Visit( IAIVisitor *p )
{
	if ( !IsValid( pModel ) )
		return;
	p->AddHull( pModel->pGeometry->pAIGeometry, MakeTransform(), 0, pUnit->nFloor, TS_UNITS | TS_PICK );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CEditorUnit::MakePosition() const
{
	CVec3 pt( pUnit->ptPos.x, pUnit->ptPos.y, 0 );
	pt *= FP_GRID_STEP;
	pt.z = pUnit->nFloor * NBuilding::WALL_HEIGHT;
	position.forward.RotateHVector( &pt, pt );
	return pt;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorUnit::Update()
{
	CreateModel();
	bindGlobal.Update();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IEditorUnit* IEditorUnit::Create( IEditorWorld *pWorld, NDb::CUnit *pUnit, const SFBTransform &pos )
{
	return new CEditorUnit( pWorld, pUnit, pos );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////