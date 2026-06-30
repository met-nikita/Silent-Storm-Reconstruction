#ifndef __METERRAIN_H_
#define __METERRAIN_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "TerrainInfo.h"
#include "gResource.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMETerrainInfo: public CObjectBase
{
	OBJECT_BASIC_METHODS(CMETerrainInfo);
public:
	STerrainInfo info;
	CArray2D<unsigned short> alphaMap;
	float fMinH;
	float fMaxH;

	int operator&( CStructureSaver &f ) 
	{ 
		f.Add( 21, &info );
		f.Add( 6, &alphaMap );
		f.Add( 10, &fMinH );
		f.Add( 11, &fMaxH );
		return 0;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMETerrainLoader: public NGScene::CResourceLoader<int, CMETerrainInfo>
{
	OBJECT_BASIC_METHODS(CMETerrainLoader);
protected:
	virtual void Recalc()
	{
		try
		{
			NGScene::CResourceOpener file( "Terrain", GetKey() );
			pValue = new CMETerrainInfo;
			pValue->operator&( *(file.operator->()) );
		}
		catch(...)
		{
			return;
		}
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __METERRAIN_H_