#include "StdAfx.h"
#include "DataCamera.h"
//
namespace NDb
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDBCamera
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDBCamera::Import()
{
	NDatabase::ImportField( "Yaw", &fYaw );
	NDatabase::ImportField( "Pitch", &fPitch );
	NDatabase::ImportField( "Roll", &fRoll );
	NDatabase::ImportField( "Distance", &fDistance );
	NDatabase::ImportField( "AnchorX", &vAnchor.x );
	NDatabase::ImportField( "AnchorY", &vAnchor.y );
	NDatabase::ImportField( "AnchorZ", &vAnchor.z );
	NDatabase::ImportField( "FOV", &fFOV );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
using namespace NDb;
//
REGISTER_SAVELOAD_CLASS( 0x50992190, CDBCamera );
