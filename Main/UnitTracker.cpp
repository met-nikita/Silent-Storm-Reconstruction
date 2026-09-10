#include "StdAfx.h"
#include "GView.h"
#include "Grid.h"
#include "Transform.h"
#include "GSceneUtils.h"
#include "iMain.h"
#include "wInterface.h"
#include "RPGUnitInfo.h"
#include "RWGame.h"
#include "RWSound.h"
#include "aiPath.h"
#include "Interface.h"
#include "UIWrap.h"
#include "iMission.h"
#include "iMissionUI.h"
#include "iCommonUI.h"
#include "MemObject.h"
#include "UnitTracker.h"
#include "iGameStates.h"	// GetSelectionColor (v1.2 selection palette)
#include "..\Misc\StrProc.h"
#include "..\DBFormat\DataMap.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataGeometry.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGame
{
////////////////////////////////////////////////////////////////////////////////////////////////////
const float
	F_PATHPOINT_DIST = 0.5f;
// (the old local SELECT/HILIGHT greens became the shared palette entries 0/1 in v1.2 --
// GetSelectionColor in iGameStates.cpp, dispatcher v1.2 @0x5d6130)
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool IsSamePlace( const NAI::SPathPlace &s1, const NAI::SPathPlace &s2 )
{
	if ( s1.GetX() != s2.GetX() )
		return false;
	if ( s1.GetY() != s2.GetY() )
		return false;
	if ( s1.GetLayer() != s2.GetLayer() )
		return false;

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnitTracker
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnitTracker::CUnitTracker( IMission *_pMission, NWorld::CUnit *_pUnit ):
	pMission( _pMission ), pUnit( _pUnit ), nFloor( 0 ), bSelected( false ), bHilighted( false ), bPathVisible( false ), bHilightTarget( false ), bPathDigitsVisible( false ),
	bTrackRealTime( false )
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CUnitTracker::GetSkillChanges @0x32c110: (current - baseline) for nSkill; the FIRST probe
// of a skill records its maximum value as the baseline and reports 0.
// v1.2 0x72c616 calls IUnitMissionInfo::GetSkillMaxValue (vtable +0x40).
int CUnitTracker::GetSkillChanges( int nSkill )
{
	int nCur = 0;
	if ( IsValid( pUnit ) && pUnit->GetRPG() )
		nCur = pUnit->GetRPG()->GetSkillMaxValue( (NDb::ESkillType)nSkill );

	unordered_map<int,int>::iterator pos = skillsChanges.find( nSkill );
	if ( pos != skillsChanges.end() )
		return nCur - pos->second;

	skillsChanges[nSkill] = nCur;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail CUnitTracker::SyncAllSkills @0x32c180: re-baseline every watched skill to its CURRENT value
// (after this GetSkillChanges reports 0 for each until the unit's skills move again).
void CUnitTracker::SyncAllSkills()
{
	for ( unordered_map<int,int>::iterator pos = skillsChanges.begin(); pos != skillsChanges.end(); ++pos )
	{
		int nCur = 0;
		if ( IsValid( pUnit ) && pUnit->GetRPG() )
			nCur = pUnit->GetRPG()->GetSkillMaxValue( (NDb::ESkillType)pos->first );
		pos->second = nCur;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// retail @0x32b670 (oracle s2_unittracker.h:888): build the command, gate it on the unit's own
// CanDo -- for a CCmdPath that is the synchronous FindPath preview (wUnitServer.cpp:550) whose
// UCR_PATH_NOT_FOUND is the click-time "Path not found" -- and submit ONLY on UCR_OK /
// UCR_NOT_ENOUGH_AP (real-time: AP accrues). A rejected command is never queued and sTarget is
// left untouched. The Jan03 IsRealTime()/bInstantly split survives only as Command's submit
// flavor (retail forces it off for CCmdContinue @0x72b797). NOTE CanDo destroys a zero-ref
// command, hence the CPtr holder. (Retail's extra bStandUp leg -- WALK-pose cancel + wish-pose,
// v1.2 PK/corpse guards -- is a separate unported parity item.)
NWorld::EUnitCommandResult CUnitTracker::SetTargetPosition( const NAI::SPosition &sPos, bool bInstantly )
{
	CObj<NAI::CPath> pCurrentPath( pUnit->GetCurrentPath() );
	const bool bContinue = IsValid( pCurrentPath ) && IsSamePlace( sTarget.p, sPos.p );

	CPtr<NWorld::CCmd> pCmd;
	if ( bContinue )
		pCmd = new NWorld::CCmdContinue;
	else
		pCmd = new NWorld::CCmdPath( sPos, NAI::PF_DEFAULT );

	NWorld::EUnitCommandResult eResult = pUnit->CanDo( pCmd );
	if ( eResult != NWorld::UCR_OK && eResult != NWorld::UCR_NOT_ENOUGH_AP )
		return eResult;

	const bool bSubmitInstantly = !bContinue && ( pMission->IsRealTime() || bInstantly );
	pMission->Command( pUnit, pCmd, bSubmitInstantly );
	sTarget = sPos;
	return NWorld::UCR_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NAI::SPosition CUnitTracker::GetTargetPosition() const
{
	CObj<NAI::CPath> pCurrentPath( pUnit->GetCurrentPath() );

	if ( IsValid( pCurrentPath ) )
	{
		NAI::SPosition sPos;
		sPos.p = pCurrentPath->points.back();
		sPos.SetNetwork( pMission->GetWorld()->GetPathNetwork() );
		return sPos;
	}

	return pUnit->GetPosition().pos;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitTracker::IsPathComplete() const
{
	return pUnit->GetCurrentPath() == 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitTracker::CancelPath()
{
	pMission->Command( new NWorld::CCmdCancel( pUnit ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitTracker::ShowPath()
{
	bool bNeedUpdate = true;
	CObj<NAI::CPath> pNewPath = pUnit->GetCurrentPath();
	if ( pPath && pNewPath )
	{
		if ( pPath->IsEqual( (*pNewPath) ) )
			bNeedUpdate = false;
	}
	else
	{
		if ( !pPath && !pNewPath )
			bNeedUpdate = false;
	}

	if ( pNewPath )
	{
		bool bNewHilightTarget = false;
		NAI::SPosition sPosition;
		if ( pMission->GetTracePosition( &sPosition ) && ( sPosition == sTarget ) )
			bNewHilightTarget = true;

		if ( bNewHilightTarget != bHilightTarget )
		{
			bNeedUpdate = true;
			bHilightTarget = bNewHilightTarget;
		}
	}

	if ( bNeedUpdate )
	{
		pPath = pNewPath;
		HidePath();
		HidePathDigits();
	}

	if ( pPath && !bPathVisible )
	{
		CObj<NWorld::IPathViewer> pPathViewer = pUnit->CreatePathViewer();
		vector<NWorld::SPathPoint> points;
		pPathViewer->SetPath( pPath );
		pPathViewer->GetPoints( &points );

		if ( points.empty() )
			return;

		nTargetAP = points.back().nAP;
		vTargetPos = points.back().vPoint;
		sTarget.p = pPath->points.back();
		sTarget.SetNetwork( pMission->GetWorld()->GetPathNetwork() );

		NGScene::IGameView *pScene = pMission->GetScene();
		CPtr<NRPG::IUnitMissionInfo> pRPG = pUnit->GetRPG();

		vector<NWorld::SPathPoint> resPoints;
		SmoothPathLine( points, &resPoints );

		int nAttackAP = 0;
		SActionInfo sAction;
		pMission->GetActionInfo( UA_ATTACK, &sAction );
		// retail CUnitTracker::ShowPath @0x32c7b0: the "can still attack after moving" AP is the
		// selection's folded MINIMUM, adopted when any unit reported one (nMinAP != -1)
		if ( sAction.nMinAP != -1 )
			nAttackAP = sAction.nMinAP;

		if ( !resPoints.empty() )
		{
			for ( int nTemp = 1; nTemp < resPoints.size() - 1; nTemp++ )
			{
				NWorld::SPathPoint sPoint = resPoints[nTemp];
				sPoint.vPoint.z += 0.1f;

				SFBTransform sTransform;
				MakeMatrix( &sTransform, 0, 0, 0, sPoint.vPoint );

				CPtr<NDb::CModel> pPoint = NDb::GetModel( 1558 );
				if ( !pMission->IsRealTime() )
				{
					if ( !pRPG->CanSpendAP( sPoint.nAP ) )
						pPoint = NDb::GetModel( 1570 );
					else if ( !pRPG->CanSpendAP( sPoint.nAP + nAttackAP ) )
						pPoint = NDb::GetModel( 1569 );
				}

				NGScene::CLightGroup *pGroup = pScene->CreateLightGroup();
				groupsSet.push_back( pGroup );
				nodesSet.push_back( pScene->CreateMesh( pPoint, sTransform, NGScene::SFullRoomInfo( pGroup, sPoint.nFloor ) ) );
			}
		}

		SFBTransform sTransform;
		MakeMatrix( &sTransform, 0, 0, 0, vTargetPos + CVec3( 0, 0, 0.1f ) );

		CPtr<NDb::CModel> pCross = NDb::GetModel( 1578 );
		if ( !pMission->IsRealTime() )
		{
			if ( bHilightTarget )
				pCross = NDb::GetModel( 1559 );
			else if ( !pRPG->CanSpendAP( points.back().nAP ) )
				pCross = NDb::GetModel( 1634 );
			else if ( !pRPG->CanSpendAP( points.back().nAP + nAttackAP ) )
				pCross = NDb::GetModel( 1635 );
		}

		NGScene::CLightGroup *pGroup = pScene->CreateLightGroup();
		groupsSet.push_back( pGroup );
		nodesSet.push_back( pScene->CreateMesh( pCross, sTransform, NGScene::SFullRoomInfo( pGroup ) ) );

		bPathVisible = true;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitTracker::HidePath()
{
	bPathVisible = false;
	groupsSet.clear();
	nodesSet.clear();

	HidePathDigits();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitTracker::ShowPathDigits()
{
	if ( !bPathVisible )
		return;
	if ( bPathDigitsVisible )
		return;

	HidePathDigits();

	NGScene::IGameView *pScene = pMission->GetScene();

	CObj<NWorld::IPathViewer> pPathViewer = pUnit->CreatePathViewer();
	vector<NWorld::SPathPoint> points;
	pPathViewer->SetPath( pPath );
	pPathViewer->GetPoints( &points );

	if ( points.empty() )
		return;

	CPtr<NRPG::IUnitMissionInfo> pRPG = pUnit->GetRPG();

	int nAttackAP = 0;
	SActionInfo sAction;
	pMission->GetActionInfo( UA_ATTACK, &sAction );
	// retail CUnitTracker::ShowPathDigits @0x32cf70: same nMinAP != -1 adoption as ShowPath
	if ( sAction.nMinAP != -1 )
		nAttackAP = sAction.nMinAP;

	bPathDigitsVisible = true;

	CVec3 vDir = pMission->GetCamera()->GetForwardDir();
	vDir.z = 0;
	Normalize( &vDir );

	CVec3 vAPDir( 0, 0, 1 );
	if ( points.size() > 1 )
		vAPDir = vTargetPos - points[points.size() - 2].vPoint;
	else
		vAPDir = vTargetPos - pUnit->GetPosition().GetCP();

	vAPDir.z = 0;
	Normalize( &vAPDir );

	int nValue = nTargetAP;
	CVec2 vSize( 0, 0 );
	do
	{
		int nDigit = nValue % 10;
		nValue /= 10;

		CPtr<NDb::CModel> pDigit = NDb::GetModel( 1585 + nDigit );
		if ( !IsValid( pDigit->pGeometry ) )
			continue;

		vSize.x += pDigit->pGeometry->boundSize.x;
		vSize.y = Max( vSize.y, pDigit->pGeometry->boundSize.y );
	} while( nValue > 0 );

	nValue = nTargetAP;
	CVec2 vShift = vSize / 2;
	do
	{
		int nDigit = nValue % 10;
		nValue /= 10;

		CPtr<NDb::CModel> pDigit = NDb::GetModel( 1585 + nDigit );
		if ( !pMission->IsRealTime() )
		{
			if ( !pRPG->CanSpendAP( points.back().nAP ) )
				pDigit = NDb::GetModel( 3454 + nDigit );
			else if ( !pRPG->CanSpendAP( points.back().nAP + nAttackAP ) )
				pDigit = NDb::GetModel( 3444 + nDigit );
		}

		if ( !IsValid( pDigit->pGeometry ) )
			continue;

		const CVec3 &vBounds = pDigit->pGeometry->boundSize;

		SHMatrix sTranslate, sRotate;
		Identity( &sTranslate );
		sTranslate._14 = vShift.x - vBounds.x;
		sTranslate._24 = vShift.y;
		sTranslate._34 = 0.20f;
		MakeMatrix( &sRotate, CVec3( 0, 0, 0 ), vDir );

		SFBTransform sTransform;
		Multiply( &sTransform.forward, sRotate, sTranslate );
		sTransform.forward._14 += vTargetPos.x;
		sTransform.forward._24 += vTargetPos.y;
		sTransform.forward._34 += vTargetPos.z;
		InvertMatrix( &sTransform.backward, sTransform.forward );

		NGScene::CLightGroup *pGroup = pScene->CreateLightGroup();
		digidGroupsSet.push_back( pGroup );
		digidNodesSet.push_back( pScene->CreateMesh( pDigit, sTransform, NGScene::SFullRoomInfo( pGroup ) ) );

		vShift.x -= vBounds.x;
	} while( nValue > 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitTracker::HidePathDigits()
{
	bPathDigitsVisible = false;
	digidGroupsSet.clear();
	digidNodesSet.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitTracker::IsActive() const
{
	return bActive;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitTracker::SetActive( bool bState )
{
	bActive = bState;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitTracker::IsSelected() const
{
	return bSelected;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitTracker::SetSelected( bool bState )
{
	bSelected = bState;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnitTracker::IsHilighted() const
{
	return bHilighted;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitTracker::SetHilighted( bool bState )
{
	bHilighted = bState;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitTracker::ShowSelection()
{
	// v1.2 @0x72bf40: selected -> palette(0), hilighted -> palette(1) (game_selectionmode-aware)
	if ( bSelected )
		pSelection = pMission->GetRenderGame()->Select( pUnit, GetSelectionColor( 0 ) );
	else if ( bHilighted )
		pSelection = pMission->GetRenderGame()->Select( pUnit, GetSelectionColor( 1 ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitTracker::HideSelection()
{
	pSelection = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitTracker::Update()
{
	// retail Update @0x32d520 step 1: across a turn-based/real-time switch, clear the stale path
	// overlay and re-latch bTrackRealTime (serialized tag 12).
	if ( bTrackRealTime != pMission->IsRealTime() )
	{
		HidePath();
		bTrackRealTime = pMission->IsRealTime();
	}

	// retail CUnitTracker::Update @0x32d520 HIDEs the path/selection while a scripted sequence runs (mission
	// vtbl+0x44 IsSequence) -- so the script-assigned move path of your character is not shown during a sequence.
	if ( pMission->IsSequence() || pMission->IsInterfaceHidden() || ( !IsSelected() && !IsHilighted() ) || !pMission->IsRealTime() && ( pUnit->GetPlayer() != pMission->GetActivePlayer()->GetPlayer() ) )
	{
		HidePath();
		HideSelection();
		HidePathDigits();
	}
	else
	{
		ShowPath();
		ShowSelection();

		if ( !pMission->IsRealTime() )
			ShowPathDigits();
		else
			HidePathDigits();
	}

	enemiesList.clear();

	vector<CPtr<NWorld::CUnit> > visibleUnits;
	pUnit->GetVisible( &visibleUnits );
	for ( int nTemp = 0; nTemp < visibleUnits.size(); nTemp++ )
	{
		if ( GetUnitDiplomacy( visibleUnits[nTemp] ) != NDb::DS_ENEMY )
			continue;

		enemiesList.push_back( visibleUnits[nTemp] );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::EDiplomacyState CUnitTracker::GetUnitDiplomacy( NWorld::CUnit *pTestUnit ) const
{
	// retail @0x32b640: MY unit's stance toward the TEST unit's player -- NOT the test unit's
	// stance toward me. The variant diplomacy matrix is asymmetric (e.g. civilians view the
	// player as ENEMY so their fear-AI flees, while the player views them NEUTRAL); the old
	// reversed query painted such civilians as enemies.
	return pMission->GetWorld()->GetDiplomacyState( pUnit, pTestUnit->GetPlayer() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitTracker::GetVisibleEnemiesList( list<CPtr<NWorld::CUnit> > *pEnemies ) const
{
	*pEnemies = enemiesList;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NWorld::CUnit* CUnitTracker::GetNextVisibleEnemy()
{
	if ( enemiesList.empty() )
	{
		pLastEnemyUnit = 0;
		return 0;
	}

	list<CPtr<NWorld::CUnit> >::iterator iTemp = enemiesList.begin();
	if ( IsValid( pLastEnemyUnit ) )
	{
		list<CPtr<NWorld::CUnit> >::iterator iSearch = find( enemiesList.begin(), enemiesList.end(), pLastEnemyUnit );
		if ( iSearch != enemiesList.end() )
		{
			iTemp = iSearch;
			iTemp++;
		}
	}

	for ( int nTemp = 0; nTemp < enemiesList.size(); nTemp++ )
	{
		if ( iTemp == enemiesList.end() )
			iTemp = enemiesList.begin();

		if ( !(*iTemp)->IsDead() )
		{
			pLastEnemyUnit = (*iTemp);
			return (*iTemp);
		}

		iTemp++;
	}

	pLastEnemyUnit = 0;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitTracker::SmoothPathLine( const vector<NWorld::SPathPoint> &points, vector<NWorld::SPathPoint> *pRes )
{
	if ( points.size() < 2 )
		return;

	vector<NWorld::SPathPoint> smoothedPoints( points.size() );
	for ( int nTemp = 0; nTemp < points.size(); nTemp++ )
		smoothedPoints[nTemp] = points[points.size() - 1 - nTemp];

	while ( SmoothIteration( &smoothedPoints ) > 0.05f );

	float fDistance = 0;
	pRes->push_back( smoothedPoints.front() );
	for( int nTemp = 0; nTemp < smoothedPoints.size() - 1; nTemp++ )
	{
		const NWorld::SPathPoint &sTemp = smoothedPoints[nTemp];
		const NWorld::SPathPoint &sNext = smoothedPoints[nTemp + 1];

		fDistance += fabs( sNext.vPoint - sTemp.vPoint );
		if ( fDistance > F_PATHPOINT_DIST )
		{
			pRes->push_back( sNext );
			fDistance = 0;
		}
	}
	pRes->push_back( smoothedPoints.back() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CUnitTracker::SmoothIteration( vector<NWorld::SPathPoint> *pRes )
{
	vector<NWorld::SPathPoint> &points = (*pRes);

	vector<NWorld::SPathPoint> newPoints;
	newPoints.reserve( points.size() * 2 + 1 );
	for ( int nTemp = 0; nTemp < points.size() - 1; ++nTemp )
	{
		const NWorld::SPathPoint &sTemp = points[nTemp];
		const NWorld::SPathPoint &sNext = points[nTemp + 1];

		NWorld::SPathPoint sPoint;
		sPoint.nAP = Max( sTemp.nAP, sNext.nAP );
		sPoint.nFloor = Min( sTemp.nFloor, sNext.nFloor );
		sPoint.vPoint.x = ( sTemp.vPoint.x + sNext.vPoint.x) / 2;
		sPoint.vPoint.y = ( sTemp.vPoint.y + sNext.vPoint.y) / 2;
		sPoint.vPoint.z = ( sTemp.vPoint.z + sNext.vPoint.z) / 2;
		newPoints.push_back( sTemp );
		newPoints.push_back( sPoint );
	}
	newPoints.push_back( points.back() );

	for ( int nTemp = 1; nTemp < points.size() - 1; ++nTemp )
	{
		const NWorld::SPathPoint &sTemp = points[nTemp];
		const NWorld::SPathPoint &sNext = points[nTemp + 1];
		const NWorld::SPathPoint &sPrev = points[nTemp - 1];

		newPoints[nTemp * 2].vPoint.x = ( 4.0f * sTemp.vPoint.x + sPrev.vPoint.x + sNext.vPoint.x) / 6.0f;
		newPoints[nTemp * 2].vPoint.y = ( 4.0f * sTemp.vPoint.y + sPrev.vPoint.y + sNext.vPoint.y) / 6.0f;
		newPoints[nTemp * 2].vPoint.z = ( 4.0f * sTemp.vPoint.z + sPrev.vPoint.z + sNext.vPoint.z) / 6.0f;
	}
	points = newPoints;

	float fMax = 0;
	for ( int nTemp = 0; nTemp < points.size() - 1; ++nTemp )
		fMax = Max( fMax, fabs( points[nTemp + 1].vPoint - points[nTemp].vPoint ) );

	return fMax;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NGame;
REGISTER_SAVELOAD_CLASS( 0x12221220, CUnitTracker )
