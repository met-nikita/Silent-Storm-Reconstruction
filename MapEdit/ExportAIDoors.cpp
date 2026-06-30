#include "StdAfx.h"
#include "MapEdit.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataObject.h"
#include "..\DBFormat\DataGeometry.h"
#include "..\DBFormat\DataAnimation.h"
#include "..\DBFormat\DataMap.h"
#include "ItemsMgr.h"
#include "Export.h"
#include "..\Misc\basicShare.h"
#include "..\Main\BSPtree.h"
#include "..\Main\GAnimFormat.h"
#include "..\Main\GAnimation.h"
#include "..\Main\GBind.h"
#include "..\Main\aiObject.h"
#include "..\Main\aiObjectLoader.h"
namespace NAnimation
{
	class CFileSkeleton;
	extern CBasicShare<int, CFileSkeleton> shareSkeletons;
}
namespace NAI
{
	extern CBasicShare<int, NGScene::CFileAIBind> shareAIBinds;
	extern CBasicShare<int, CFileSkinPointsLoad> shareSkinPoints;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static NAnimation::CSkeletonAnimator *MakeFirstFrameAnimator( NDb::CSkeleton *pSkeleton, NDb::CAnimation::EType type )
{
	NAnimation::CSkeletonAnimator *pAn = new NAnimation::CSkeletonAnimator( pSkeleton );
	STime current = (STime)0;
	pAn->pTime = new CCTime( 0 );
	NAnimation::CAnimation *pAnim;
	pAnim = pAn->CreateAnimation(
		pSkeleton->GetAnimation( type, 0 ), 0 );
	if ( pAnim )
	{
		pAnim->SetStand( 0, CVec3(0,0,0), 0 );
		pAn->AddAnimator( 0, pAnim );
	}
	return pAn;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void MakeFirstFrameGeom( NDb::CSkeleton *pSkeleton, NDb::CAnimation::EType type, 
	NDb::CAIGeometry *pAIGeom, NAI::CBSPPieces *pTrees )
{
	CPtr<NAnimation::CSkeletonAnimator> pAn = MakeFirstFrameAnimator( pSkeleton, type );
	CPtr<NGScene::CBind> pBind = new NGScene::CBind;
	pBind->pBinds = NAI::shareAIBinds.Get( pAIGeom->GetRecordID() );
	pBind->pAnimation = pAn;
	pBind->pSkeleton = NAnimation::shareSkeletons.Get( pSkeleton->GetRecordID() );
	CPtr<NAI::CSkinner> pSkin = new NAI::CSkinner( 
		NAI::shareSkinPoints.Get( pAIGeom->GetRecordID() ),
		pBind );
	pSkin->CreateBSPTrees();
	CDGPtr<CPtrFuncBase<NAI::CGeometryInfo> > pGeometryCalcer( pSkin );
	pGeometryCalcer.Refresh();
	NAI::CGeometryInfo *pGeom = pGeometryCalcer->GetValue();
	if ( pGeom->pieces.empty() )
		__debugbreak();
	// store geometries:
	NAI::CBSPPieces &pieces = *pTrees;
	pieces.clear();
	NAI::CGeometryInfo::CPieceMap::iterator it;
	for ( it = pGeom->pieces.begin(); it != pGeom->pieces.end(); ++it )
	{
		if ( !it->second.trees.empty() )
			pieces[ it->first ] = it->second.trees[0];
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void DoorGetOnePositionBSPTrees( int nID, NDb::CAnimation::EType type, vector<NAI::CBSPPieces> *pTrees )
{
	vector<NAI::CBSPPieces> &pieces = *pTrees;
	pieces.clear();
	pieces.resize( NDb::N_DESTROY_STAGES );
	NDb::CDoor *pDoor = NDb::GetDoorWindow( nID );
	NDb::CTRndObject *pRndObj = pDoor->pObject;
	SRand rand;
	NDb::CObject *pObj = pRndObj->CreateObject( &rand );
	for ( int nDestroyStage = 0; nDestroyStage < NDb::N_DESTROY_STAGES; ++nDestroyStage )
	{
		NDb::CContainerModel *pCont = pObj->pModels[nDestroyStage];
		if ( !pCont )
			continue;
		NDb::CModel *pModel = pCont->pModel;
		bool bAllValid = pModel && pModel->pGeometry && pModel->pGeometry->pAIGeometry;
		if ( !bAllValid )
			continue;
		NDb::CSkeleton *pSkeleton = pModel->pSkeleton;
		if ( !pSkeleton )
			continue;
		NDb::CAIGeometry *pAIGeom = pModel->pGeometry->pAIGeometry;
		vector<NAI::CBSPPieces> &trees = *pTrees;
		MakeFirstFrameGeom( pSkeleton, type, pAIGeom, &trees[ nDestroyStage ] );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void DoorsUpdateDBData( CItemsMgr *pItems, const vector<int> &nItemIDs )
{
	for ( int i = 0; i < nItemIDs.size(); ++i )
	{
		int nID = nItemIDs[ i ];
		vector<NAI::CBSPPieces> treesOpen, treesClosed;
		DoorGetOnePositionBSPTrees( nID, NDb::CAnimation::ACTIVATE, &treesOpen );
		DoorGetOnePositionBSPTrees( nID, NDb::CAnimation::DEACTIVATE, &treesClosed );
		try
		{
			char szPath[MAX_PATH];
			sprintf( szPath, "%sAIBSPTrees\\%d", GetExportDstDir().c_str(), nID );
			CFileStream fp;
			fp.OpenWrite( szPath );
			{
				CStructureSaver file( fp, CStructureSaver::WRITE );
				file.Add( 1, &treesOpen );
				file.Add( 2, &treesClosed );
			}
		}
		catch ( ... )	{}
	}	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
