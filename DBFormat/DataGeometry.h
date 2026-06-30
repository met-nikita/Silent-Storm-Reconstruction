#ifndef __DATAGEOMETRY_H_
#define __DATAGEOMETRY_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\ADOImport\BasicDB.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
enum ETrafic
{
	TR_DAMAGE = 1,
	TR_COVER = 2,
	TR_PASS = 4,
	TR_VISION = 8,
	TR_ITEM_BLOCKER = 16
};
inline bool IsNoDamage( ETrafic trafic ) { return ( trafic & TR_DAMAGE ) == 0; }
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SPieceLinks
{
	ZDATA
	int nPieceHashID;
	vector<CVec3> links;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nPieceHashID); f.Add(3,&links); return 0; }
};
typedef unordered_map<int, SPieceLinks> SPieceLinksHash;
void String2PieceLinks( SPieceLinksHash *pLinks, const string &szStr );
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAIGeometry: public CDBRecord
{
	OBJECT_BASIC_METHODS(CAIGeometry);
public:
	ZDATA_(CDBRecord)
	ETrafic traficability;
	float fVolume;
	float fSolidPart;
	SPieceLinksHash additionalLinks;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&traficability); f.Add(3,&fVolume); f.Add(4,&fSolidPart); f.Add(5,&additionalLinks); return 0; }

	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CGeometry: public CDBRecord
{
	OBJECT_BASIC_METHODS(CGeometry);
public:
	CPtr<CAIGeometry> pAIGeometry, pAIGeometry2;
	CVec3 boundSize;
	CVec3 boundCenter;
	virtual void Import();
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CAIGeometry* GetAIGeometry( int nID );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __DATAGEOMETRY_H_