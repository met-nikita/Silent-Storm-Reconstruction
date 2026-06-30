#include "StdAfx.h"
#include "weInterface.h"
#include "..\Main\wTurnBased.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataGeometry.h"
#include "..\DBFormat\DataRPG.h"
#include "..\DBFormat\DataObject.h"
#include "..\DBFormat\DataAnimation.h"
#include "iWysiwyg.h"
#include "..\Main\MELayers.h"
#include "MEUserSettings.h"
#include "MEParams.h"
#include "..\Main\Grid.h"
#include "..\Main\Transform.h"
#include "..\Main\aiMap.h"
#include "..\Main\MemObject.h"
#include "..\Main\gAnimation.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
const int CLUE_SLOT_ID = 188;
const int EXPLOSION_ID = 189;
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CEditorObject: public IEditorObject
{
	OBJECT_NOCOPY_METHODS(CEditorObject)
	ZDATA
	CSyncSrcBind<IVisObj> bindGlobal;
	CPtr<IEditorWorld> pWorld;
	CDBPtr<NDb::CFinalElement> pFin;
	CDGPtr<CFuncBase<STime> > pTime;
	int nCutFloor;
	CObj<NDb::CObject> pObj;
	CObj<NDb::CModel> pModel;
	vector< CObj<NDb::CModel> > children;
	CObj<CMemObject> pMemObject;
	CVec4 memColor;
	int nUserID;
	int nObjectID;
	bool bIsOpen;
	int nCurrentPhase;
	//
	CDBPtr<NDb::CSkeleton> pSkeleton;
	CObj<NAnimation::CSkeletonAnimator> pAnimator;
	CObj<CActionCounter> pAction;
	STime tEnd;
	ZEND

	void CreateModels();
	void AddAISphers( IRenderVisitor *p );
	SFBTransform MakeTransform();
	bool IsObjValid() const;
	void OpenClose( bool bOpen );
	bool IsAnimObject();

public:
	CEditorObject() {}
	CEditorObject( IEditorWorld *pWorld, NDb::CFinalElement *pFin, int nCutFloor, CFuncBase<STime> *pTime );

	static IEditorObject* Create( IEditorWorld *pWorld, NDb::CFinalElement *pFin, const SFBTransform &pos, int nCutFloor );

	virtual NDb::CFinalElement* GetDBElement() const { if ( IsObjValid() ) return pFin; return 0; }
	virtual NDb::CModel* GetModel() const { return pModel; }
	virtual void Update( int nCutFloor );
	virtual void UpdateCutFloor( int nCutFloor );

	virtual void Visit( IRenderVisitor *p );
	virtual void Visit( IAIVisitor *p );

	virtual void ToggleOpen();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CEditorObject::CEditorObject( IEditorWorld *_pWorld, NDb::CFinalElement *_pFin, int _nCutFloor, CFuncBase<STime> *_pTime )
: pWorld(_pWorld), pFin(_pFin), nCutFloor(_nCutFloor), pTime(_pTime)
{
	bindGlobal.Link( pWorld->GetActive(), this );
	pTime = _pTime;
	CreateModels();
	if ( IsValid( pFin->pObject ) )
		nObjectID = pFin->pObject->GetRecordID();
	bIsOpen = false;
	nCurrentPhase = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorObject::Update( int _nCutFloor )
{
	nCutFloor = _nCutFloor;
	nUserID = NWysiwyg::MakeUserID( BT_OBJECT, pFin->GetRecordID() );
	bindGlobal.Update();
	if ( IsValid( pFin->pObject ) && nObjectID != pFin->pObject->GetRecordID() || pFin->nObjectPhase != nCurrentPhase )
	{
		nObjectID = pFin->pObject->GetRecordID();
		CreateModels();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorObject::UpdateCutFloor( int _nCutFloor )
{
	bool bOldVis = pFin->nFloor <= nCutFloor;
	bool bNewVis = pFin->nFloor <= _nCutFloor;
	if ( bNewVis != bOldVis )
		Update( _nCutFloor );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CEditorObject::IsObjValid() const
{
	return ::IsValid( pFin ) && IsValid( pFin->pVariant );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorObject::Visit( IRenderVisitor *p )
{
	if ( !IsObjValid() || !IsLayerVisible( NBuilding::MakeFragmentID( LID_OBJECTS, 0 ) ) || pFin->nFloor > nCutFloor )
		return;

	SFBTransform pos = MakeTransform();
	if ( IsValid( pModel ) )
	{
		if ( !IsAnimObject() )
			p->AddMesh( pModel, pos, 0, pFin->nFloor );
		else
		{
			vector<IRenderVisitor::SBoundMesh> boundMeshes;
			p->AddMesh( pModel, pAnimator, 0, boundMeshes, 0, pFin->nFloor );
			OpenClose( bIsOpen );
		}
		for ( int i = 0; i < children.size(); ++i )
				p->AddMesh( children[i], pos, 0, pFin->nFloor );
	}
	if ( IsValid( pMemObject ) )
		p->AddMesh( pMemObject, memColor, pos );
	if ( GetUserSettings().GetParam( ME_SHOW_AISPHERES ) )
		AddAISphers( p );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorObject::Visit( IAIVisitor *p )
{
	if ( !IsObjValid() || !IsLayerVisible( NBuilding::MakeFragmentID( LID_OBJECTS, 0 ) ) || pFin->nFloor > nCutFloor )
		return;

	SFBTransform pos = MakeTransform();

	if ( IsValid( pModel ) && IsValid( pModel->pGeometry ) && IsValid( pModel->pGeometry->pAIGeometry ) )
		p->AddHull( pModel->pGeometry->pAIGeometry, pos, 0, pFin->nFloor, TS_PICK );
	//
	for ( int i = 0; i < children.size(); ++i )
		if ( IsValid( children[i]->pGeometry ) && IsValid( children[i]->pGeometry->pAIGeometry ) )
			p->AddHull( children[i]->pGeometry->pAIGeometry, pos, 0, pFin->nFloor, TS_PICK );
	//
	if ( IsValid( pMemObject ) )
		p->AddHull(	pMemObject, pos, 0, pFin->nFloor, TS_PICK, nUserID );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
SFBTransform CEditorObject::MakeTransform()
{
	CVec3 pt( FP_GRID_STEP * pFin->ptPos, pFin->nFloor * NBuilding::WALL_HEIGHT + pFin->fDZ );
	SFBTransform tr;
	MakeMatrix( &tr, pFin->ptScale, pt, ToRadian( pFin->fRotation ) );
	return tr * pWorld->GetTerrainTransform( pt.x, pt.y );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CreateChild( vector< CObj<NDb::CModel> > *pModels, NDb::CObject *pObj, int nPhase )
{
	if ( pObj->pChild )
	{
		if ( pObj->pChild->pModels[nPhase] && pObj->pChild->pModels[nPhase]->pModel )
			pModels->push_back( pObj->pChild->pModels[nPhase]->pModel.GetPtr() );
		CreateChild( pModels, pObj->pChild, nPhase );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorObject::CreateModels()
{
	if ( !IsValid( pFin ) || !IsValid( pFin->pObject ) )
		return;
	bool bClueSlot = false;
	bool bExplosion = false;
	static SRand rand;
	if ( IsValid( pFin->pObject->pObject ) )
	{
		nCurrentPhase = pFin->nObjectPhase;
		pObj = pFin->pObject->pObject->CreateObject( &rand );
		if ( IsValid( pObj->pModels[pFin->nObjectPhase] ) && IsValid( pObj->pModels[pFin->nObjectPhase]->pModel ) )
			pModel = pObj->pModels[pFin->nObjectPhase]->pModel;
		CreateChild( &children, pObj, pFin->nObjectPhase );
	}
	else if ( IsValid( pFin->pObject->pRPGItem ) && IsValid( pFin->pObject->pRPGItem->pModel ) )
	{
		pModel = pFin->pObject->pRPGItem->pModel->CreateModel( &rand );
	}
	else if ( IsValid( pFin->pObject->pRPGItem ) )
	{
		if ( pFin->pObject->pRPGItem->GetRecordID() == CLUE_SLOT_ID )
			bClueSlot = true;
		if ( pFin->pObject->pRPGItem->GetRecordID() == EXPLOSION_ID )
			bExplosion = true;
	}
	//
	if ( !IsValid( pModel ) )
	{
		pMemObject = new ::CMemObject;
		pMemObject->CreateSphere( VNULL3, 0.5f * FP_GRID_STEP, 4 );
		memColor = CVec4( 0.7f, 0.7f, 0.9f, 1.0f );
		if ( bClueSlot )
			memColor = CVec4( 1.0f, 0.5f, 0.6f, 1.0f );
		if ( bExplosion )
			memColor = CVec4( 0.77f, 0.55f, 0.1f, 1.0f );
	}
	else
		pMemObject = 0;
	//
	if ( IsAnimObject() )
	{
		pSkeleton = pModel->pSkeleton;
		pAnimator = new NAnimation::CSkeletonAnimator( pSkeleton );
		pAnimator->pTime = pTime;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorObject::AddAISphers( IRenderVisitor *p )
{
	if ( !IsValid( pModel ) || !IsValid( pModel->pGeometry ) || !IsValid( pModel->pGeometry->pAIGeometry ) )
		return;

	list<NAI::SObjectInfo> objs;
	vector<SMassSphere> spheres;
	bool bClosed;
	SFBTransform pos = MakeTransform();
	NAI::GetGeometry( &objs, &spheres, pModel->pGeometry->pAIGeometry->GetRecordID(), &bClosed );
	for ( int i = 0; i < spheres.size(); ++i )
	{
		CPtr<CMemObject> pSph = new CMemObject;
		pSph->CreateSphere( spheres[i].ptCenter, spheres[i].fRadius, 2 );
		CVec4 color( 1, 0.7f, 0.7f, 0.7f );
		p->AddMesh( pSph, color, pos );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CEditorObject::IsAnimObject()
{
	return IsValid( pModel ) && IsValid( pModel->pSkeleton ) && IsValid( pObj->pDoor ) &&
		IsValid( pModel->pSkeleton->GetAnimation( NDb::CAnimation::ACTIVATE, 0 ) ) &&
		IsValid( pModel->pSkeleton->GetAnimation( NDb::CAnimation::DEACTIVATE, 0 ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorObject::OpenClose( bool bOpen )
{
	if ( !IsAnimObject() )
		return;
	bIsOpen = bOpen;
	pTime.Refresh();
	tEnd = pTime->GetValue();
	CPtr<NAnimation::CAnimation> pAnim;
	SFBTransform pos = MakeTransform();
	CVec3 ptPos = VNULL3;
	pos.forward.RotateHVector( &ptPos, ptPos );
	//
	if ( bOpen )
	{
		//pWorld->MakeSound( ptPos, pObj->pDoor->pOpenSound );
		pAnim = pAnimator->CreateAnimation(	pSkeleton->GetAnimation( NDb::CAnimation::ACTIVATE, 0 ), tEnd );
	}
	else
	{
		//pWorld->MakeSound( ptPos, pObj->pDoor->pCloseSound );
		pAnim = pAnimator->CreateAnimation(	pSkeleton->GetAnimation( NDb::CAnimation::DEACTIVATE, 0 ), tEnd );
	}
	if ( pAnim )
	{
		pAnim->SetStand( tEnd, ptPos, ToRadian( pFin->fRotation ) );
		pAnimator->AddAnimator( tEnd, pAnim );
		tEnd += pAnim->GetTime();
		pAnimator->AddMemorizer( tEnd );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CEditorObject::ToggleOpen()
{
	OpenClose( !bIsOpen );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IEditorObject* IEditorObject::Create( IEditorWorld *pWorld, NDb::CFinalElement *pFin, int nCutFloor, CFuncBase<STime> *pTime )
{
	return new CEditorObject( pWorld, pFin, nCutFloor, pTime );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
