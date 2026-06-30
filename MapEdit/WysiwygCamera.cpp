#include "StdAfx.h"
#include "WysiwygCamera.h"
#include "..\DBFormat\DataCamera.h"
#include "iWysiwyg.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWysiwyg
{
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SCameraMove
{
	ICamera::SCameraPos pos;
	float fFOV;
	STime sTransitionTime;

	SCameraMove( const ICamera::SCameraPos &_pos, float _fFOV, STime _sTransitionTime ): pos(_pos), sTransitionTime(_sTransitionTime), fFOV(_fFOV){}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CWysiwygCamera: public IWysiwygCamera
{
	OBJECT_BASIC_METHODS(CWysiwygCamera)

	CPtr<ICamera> pCamera;
	vector<SCameraMove> transition;
	int iCurrentPos;
	STime sMorphTime;
	ICamera::SCameraPos sCameraPos;
	float fCameraFOV;

public:
	CWysiwygCamera( ICamera *p=0 );

	virtual void Update( const STime &time );
	virtual void SetTransition( const vector<SDBCamera> &transition );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CWysiwygCamera::CWysiwygCamera( ICamera *p ): pCamera( p ), iCurrentPos(-1)
{ 
	ASSERT( IsValid( pCamera ) ); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWysiwygCamera::Update( const STime &sTime )
{
	pCamera->Update( sTime );
	//
	if ( iCurrentPos < 0 || iCurrentPos >= transition.size() )
	{
		//pCamera->SetFOV( N_FOV );
		return;
	}
	if ( sMorphTime == 0 )
		sMorphTime = sTime;
	
	const SCameraMove &cm = transition[iCurrentPos];

	float fCoeff;
	if ( cm.sTransitionTime <= 0 )
		fCoeff = 1;
	else
	{
		fCoeff = float( sTime - sMorphTime ) / cm.sTransitionTime;
		if ( fCoeff > 1.0f )
			fCoeff = 1.0f;
	}

	ICamera::SCameraPos sNewCameraPos( sCameraPos );
	sNewCameraPos.fRod = cm.pos.fRod * fCoeff + sCameraPos.fRod * ( 1 - fCoeff );
	sNewCameraPos.fYaw = cm.pos.fYaw * fCoeff + sCameraPos.fYaw * ( 1 - fCoeff );
	sNewCameraPos.fRoll = cm.pos.fRoll * fCoeff + sCameraPos.fRoll * ( 1 - fCoeff );
	sNewCameraPos.fPitch = cm.pos.fPitch * fCoeff + sCameraPos.fPitch * ( 1 - fCoeff );
	sNewCameraPos.ptAnchor = cm.pos.ptAnchor * fCoeff + sCameraPos.ptAnchor * ( 1 - fCoeff );
	float fFOV = cm.fFOV * fCoeff + fCameraFOV * ( 1 - fCoeff );
	pCamera->SetPlacement( sNewCameraPos );
	pCamera->SetFOV( fFOV );

	bool bFinished = ( fCoeff == 1.0f );
	if ( bFinished )
	{
		++iCurrentPos;
		sMorphTime = 0;
		sCameraPos = sNewCameraPos;
		fCameraFOV = fFOV;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CWysiwygCamera::SetTransition( const vector<SDBCamera> &_transition )
{
	transition.clear();
	for ( int i = 0; i < _transition.size(); ++i )
	{
		NDb::CDBCamera *pDBCamera = NDb::GetDBCamera( _transition[i].nCameraID );
		if ( !IsValid( pDBCamera ) )
			continue;
		transition.push_back( SCameraMove( ICamera::SCameraPos( pDBCamera->vAnchor, pDBCamera->fDistance, pDBCamera->fPitch, pDBCamera->fYaw, pDBCamera->fRoll ), pDBCamera->fFOV, _transition[i].sTransitionTime ) );
	}
	iCurrentPos = 0;
	sMorphTime = 0;
	pCamera->GetPlacement( &sCameraPos );
	float fCameraFOV = pCamera->GetFOV();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
IWysiwygCamera* CreateWysiwygCamera( ICamera *pCamera )
{
	return new CWysiwygCamera( pCamera );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
