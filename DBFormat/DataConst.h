#ifndef __DATACONST_H_
#define __DATACONST_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\ADOImport\BasicDB.h"
#include "..\misc\RandomGen.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T> 
class CRndPtr : public CDBRecord
{
public:
	vector<CPtr<T> > variants;
	CRoulette roulette;

	T* GetRnd( SRand *pRand ) const
	{
		ASSERT( pRand );
		if ( variants.empty() )
		{
			ASSERT( false );
			return 0;
		}
		return variants[roulette.GetRandomSector( pRand )];
	}
	T* GetRnd( SRand *pRand, const vector<int> &params ) const
	{
		ASSERT( pRand );
		if ( variants.empty() )
		{
			ASSERT( false );
			return 0;
		}
		if ( params.empty() )	// подходит любой вариант
			return variants[roulette.GetRandomSector( pRand )];

		vector<int> vSuitableIndices;

		CRoulette roll;
		for ( int i = 0; i < variants.size(); ++i )
			if ( IsValid( variants[i] ) && IsSuitableVariant( params, variants[i]->flags ) )
			{
				vSuitableIndices.push_back( i );
				roll.AddSector( roulette.GetSectorValue( i ) );
			}
			if ( vSuitableIndices.empty() )
				return 0;
			return variants[vSuitableIndices[roll.GetRandomSector( pRand )]];
	}
	T* GetVariant( int nVarID ) const
	{
		ASSERT( !variants.empty() );

		for( int i = 0; i < variants.size(); ++i )
			if ( variants[i]->GetRecordID() == nVarID )
				return variants[i];
		return 0;
	}
	virtual void Import() {}
	int operator&( CStructureSaver &f )
	{
		f.Add( 1, (CDBRecord*)this );
		f.Add( 2, &variants );
		f.Add( 3, &roulette );
		return 0;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
const int N_CONSTRUCTION_MATERIALS = 8;
const int N_FIRST_GEOMMATERIALS = 4;
const int N_SECOND_GEOMMATERIALS = N_CONSTRUCTION_MATERIALS - N_FIRST_GEOMMATERIALS;
const int N_MODEL_MATERIALS = 4;

const int N_DEF_MATERIAL_ID = 7;
const int N_CUBE_GEOMETRY_ID = 1;
const int N_SPHERE_GEOMETRY_ID = 6;

const int N_DESTROY_STAGES = 5;
const int N_PARTICLE_TEXTURES = 48;
const int N_HEAD_MESHES = 4;
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SVariantFlags
{
	vector<int> flags; // индексы атрибутов в таблице Attributes
	
	int operator&( CStructureSaver &f );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Слоты для ношения предметов
enum ESlot
{
	SLOT_1 = 0,
	SLOT_2,
	N_SLOTS
};
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif