#ifndef __AIMOVEACTION_H_
#define __AIMOVEACTION_H_
//
#include "aiJob.h"
//
namespace NAI
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class IAIUnit;
struct SPathPlace;
struct SPosition;
struct SUnitPosition;
class IPathNetwork;
struct SPlaceWithAP;
class CAIAction;
////////////////////////////////////////////////////////////////////////////////////////////////////
SPosition GetPos( const SPathPlace &place, IPathNetwork *pPathNetwork );
SUnitPosition GetUnitPos( const SPathPlace &place, IPathNetwork *pPathNetwork );
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIFindGoodPlacesJob
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIFindGoodPlacesJob: public CAIJob
{
	OBJECT_BASIC_METHODS( CAIFindGoodPlacesJob );
	ZDATA
	ZPARENT( CAIJob );
	int nMaxAP;
	bool bPathFound;
	CPtr<IAIUnit> pUnit;
	CPtr<IAIUnit> pEnemy;
	int nCurrentPlace;
	int nBestPlace;
	int nBestCover;
	vector<SPlaceWithAP> places;
	vector<SPlaceWithAP> goodPlaces;
private:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAIJob *)this); f.Add(3,&nMaxAP); f.Add(4,&bPathFound); f.Add(5,&pUnit); f.Add(6,&nCurrentPlace); f.Add(7,&nBestPlace); f.Add(8,&nBestCover); f.Add(9,&places); f.Add(10,&goodPlaces); return 0; }
	//
	void PreparePlaces();
	//
public:
	CAIFindGoodPlacesJob() {}
	CAIFindGoodPlacesJob( IAIUnit *_pUnit, IAIUnit *_pEnemy, int _nMaxAP, CAIJob *pParentJob );
	//
	virtual void DoJob();
	//
	const vector<SPlaceWithAP>& GetPlaces() const { return goodPlaces; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CAIChoosePlaceForActionsJob
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIChoosePlaceForActionsJob: public CAIJob
{
	OBJECT_BASIC_METHODS( CAIChoosePlaceForActionsJob );
	ZDATA
	ZPARENT( CAIJob );
	int nCurPlace, nCurAction;
	vector< CPtr<CAIAction> > actions;
	vector<SPlaceWithAP> places;
	unordered_map< CPtr<CAIAction>, int, SPtrHash > actionToPlace;
	unordered_map< CPtr<CAIAction>, bool, SPtrHash > actionToChosen;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,(CAIJob *)this); f.Add(3,&nCurPlace); f.Add(4,&nCurAction); f.Add(5,&actions); f.Add(6,&places); f.Add(7,&actionToPlace); f.Add(8,&actionToChosen); return 0; }
	//
public:
	CAIChoosePlaceForActionsJob() {}
	CAIChoosePlaceForActionsJob( const vector< CPtr<CAIAction> > &_actions, CAIJob *pParentJob );
	//
	virtual void DoJob();
	//
	bool IsPlaceChosen( CAIAction *pAction );
	void GetPlace( CAIAction *pAction, SPlaceWithAP *pPlace );
	void SetPlaces( const vector<SPlaceWithAP> &_places ) { places = _places; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif