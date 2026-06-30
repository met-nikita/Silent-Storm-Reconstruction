#ifndef __A5_GLOBALINFO_H_
#define __A5_GLOBALINFO_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "DG.h"
#include "GResource.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
	class CUITexture;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SGlobalSector
{
	ZDATA
	int nImageID;
	int nTemplate;
	int nDescriptionID;
	CVec2 vImagePos;
	vector<CVec2> pointsSet;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nImageID); f.Add(3,&nTemplate); f.Add(4,&nDescriptionID); f.Add(5,&vImagePos); f.Add(6,&pointsSet); return 0; }

	SGlobalSector();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CGlobalInfo: public CObjectBase
{
	OBJECT_BASIC_METHODS(CGlobalInfo)
public:
	ZDATA
	int nMapID;
	int nScenarioID;
	vector<SGlobalSector> sectorsSet;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&nMapID); f.Add(3,&nScenarioID); f.Add(4,&sectorsSet); return 0; }

	CGlobalInfo();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CGlobalInfoLoader: public NGScene::CResourceLoader<int, CGlobalInfo>
{
	OBJECT_BASIC_METHODS(CGlobalInfoLoader);
protected:
	virtual void Recalc();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
