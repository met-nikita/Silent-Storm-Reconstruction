#include "StdAfx.h"
#include "TerrainProp.h"
#include "..\Main\TerrainInfo.h"
#include "..\Main\METerrain.h"
#include "MESerialize.h"
#include "..\Main\Grid.h"
#include "..\Misc\BasicShare.h"
#include "CtrlObjectInspector.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
extern CBasicShare<int, CMETerrainLoader> shareTerrains;
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTerrainProp: public CProp
{
	int nTerrainID;
protected:
	CMETerrainInfo* GetTerrain() const
	{
		CDGPtr< CPtrFuncBase<CMETerrainInfo> > pLoader = shareTerrains.Get( nTerrainID );
		pLoader.Refresh();
		return pLoader->GetValue();
	}
	bool Save( CMETerrainInfo *pTerr )
	{
		return SerializeTerrain( pTerr, nTerrainID );
	}

public:
	CTerrainProp() {}
	CTerrainProp( int _nTerrainID, const string &szName, int nPropertyID, int nType, int nViewType, bool bReadOnly )
		:CProp( szName, nPropertyID, nType, nViewType, bReadOnly ), nTerrainID(_nTerrainID)
	{
	}
	virtual const CVariant GetDefValue() const { return CVariant(); }
	virtual CProp* Clone() const { return 0; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CHoleProp: public CTerrainProp
{
	OBJECT_BASIC_METHODS(CHoleProp)
	int nHoleID;
	CVariant value;
	STerrainHole* GetHole() const
	{
		CMETerrainInfo *pTerr = GetTerrain();
		if ( !pTerr )
			return 0;
		if ( nHoleID < 0 || nHoleID >= pTerr->info.holes.size() )
			return 0;
		return &pTerr->info.holes[nHoleID];
	}

public:
	CHoleProp() {}
	CHoleProp( int nTerrainID, int _nHoleID, const string &szName, int nPropertyID, int nType, int nViewType, bool bReadOnly = false )
		:CTerrainProp( nTerrainID, szName, nPropertyID, nType, nViewType, bReadOnly ), nHoleID(_nHoleID)
	{
		STerrainHole *pHole = GetHole();
		if ( !pHole )
			return;
		if ( GetName() == "Height" )
			value = pHole->nHeight * FP_TERRAIN_H_SCALE;
	}
	virtual const CVariant& GetValue() const { return value; }
	void SetValue( const CVariant &v, bool bModified = true ) const 
	{
		if ( !bModified )
			const_cast<CVariant&>( value ) = v;
		STerrainHole *pHole = GetHole();
		if ( !pHole )
			return;
		if ( GetName() == "Height" )
			pHole->nHeight = (float)v / FP_TERRAIN_H_SCALE;
		const_cast<CVariant&>( value ) = (float)v;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
bool GetHoleProperties( CPropMap *pProps, int nTemplateID, int nTerrainID, int nHoleID )
{
	CProp *p = new CHoleProp( nTerrainID, nHoleID, "Height", 2111, CVariant::VT_FLOAT, DT_DEC );
	p->SetOwner( SOwner( nTemplateID, nTerrainID ) );
	(*pProps)["Height"] = p;

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
