#include "StdAfx.h"
#include "wDecal.h"
#include "wMain.h"
#include "..\Misc\RandomGen.h"
#include "..\DBFormat\DataFormat.h"
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDecal
////////////////////////////////////////////////////////////////////////////////////////////////////
CDecal::CDecal( CWorld *_pWorld, const CVec3 &_vCenter, const CVec3 &_vNormal, float _fSize, NDb::CMaterial *_pMaterial, CObjectBase *_pTarget )
: pWorld(_pWorld), pMaterial(_pMaterial), r1(this,&CDecal::OnShowBloodUpdated)
{
	targets.push_back( _pTarget );
	info.vCenter = _vCenter;
	info.vNormal = _vNormal;
	info.fRadius = _fSize;
	info.fRotation = random.GetFloat( 0, FP_2PI );
	bindGlobal.Link( pWorld->GetActive(), this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CDecal::CDecal( CWorld *_pWorld, const CVec3 &_vCenter, float _fSize, NDb::CMaterial *_pMaterial, vector<CObjectBase*> &_targets )
: pWorld(_pWorld), pMaterial(_pMaterial), r1(this,&CDecal::OnShowBloodUpdated)
{
	for ( int k = 0; k < _targets.size(); ++k )
		targets.push_back( _targets[k] );
	info.vCenter = _vCenter;
	info.vNormal = CVec3(0,0,0);
	info.fRadius = _fSize;
	info.fRotation = random.GetFloat( 0, FP_2PI );
	bindGlobal.Link( pWorld->GetActive(), this );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
extern bool bShowBlood;
void CDecal::OnShowBloodUpdated( const CShowBloodUpdated &event )
{
	if ( !bShowBlood && pMaterial->GetRecordID() == 3411 )
	{
		CMObj<CDecal> pKill(this);
		return;
	}
	//bindGlobal.Update();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDecal::Visit( IRenderVisitor *p )
{
	if ( !bShowBlood && pMaterial->GetRecordID() == 3411 )
	{
		CMObj<CDecal> pKill(this);
		return;
	}
	if ( !IsValid( pDecalTarget ) )
	{
		vector<CObjectBase*> t;
		for ( int k = 0; k < targets.size(); ++k )
		{
			if ( IsValid( targets[k] ) )
				t.push_back( targets[k] );
		}
		pDecalTarget = p->CreateDecalTarget( t, info );
		targets.clear();
	}
	p->AddDecal( pDecalTarget, pMaterial );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NWorld;
REGISTER_SAVELOAD_CLASS( 0x003c2130, CDecal )
