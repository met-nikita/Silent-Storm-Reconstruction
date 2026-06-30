#ifndef __WYSIWYGCLIPBOARD_H_
#define __WYSIWYGCLIPBOARD_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NBuilding
{
	struct SBuildFragment;
	struct SProjectedSpot;
	struct SLadder;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void AddObjectToClipboardBuffer( int nObjectID );
void AddFragmentToClipboardBuffer( const NBuilding::SBuildFragment &fr );
void AddTerrSpotToClipboardBuffer( const NBuilding::SProjectedSpot &spot );
void AddLadderToClipboardBuffer( const NBuilding::SLadder &ladder );
void AddUnitToClipboardBuffer( int nDBUnitID );
void AddSubTemplateToClipboardBuffer( int nDBRectID );
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SForceSelection
{
	int nWorldID;
	vector<int> fragsUserIDs;
	vector<int> objectIDs;
	vector<int> terrSpotsIDs;
	vector<int> wallSpotsIDs;
	vector<int> ladderIDs;
	vector<int> subtemplateIDs;
	vector<int> unitIDs;
	vector<int> waypointIDs;

	bool operator==(const SForceSelection &op )
	{
		bool bRet = nWorldID == op.nWorldID && fragsUserIDs == op.fragsUserIDs && objectIDs == op.objectIDs
			&& terrSpotsIDs == op.terrSpotsIDs && wallSpotsIDs == op.wallSpotsIDs && ladderIDs == op.ladderIDs
			&& subtemplateIDs == op.subtemplateIDs && unitIDs == op.unitIDs && waypointIDs == op.waypointIDs;
		return bRet;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetClipboardData( int nActiveFloor );
void PasteClipboard( SForceSelection *SelectionBuf, int *pnSelectionMinFloor, int nWorldID, int nActiveFloor );
void CheckLayerExistence( int nLayerID );
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __WYSIWYGCLIPBOARD_H_
