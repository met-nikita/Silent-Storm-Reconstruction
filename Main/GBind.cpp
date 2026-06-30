#include "StdAfx.h"
#include "..\Misc\BasicShare.h"
#include "GAnimFormat.h"
#include "GBind.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NGScene
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CFileBind
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFileBind::Recalc()
{
	pValue = new CFileBindInfo;
	try
	{
		NGScene::CResourceOpener file( "Binds", GetKey() );
		file->Add( 4, &pValue->invBindPoses );
	}
	catch(...)
	{
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CFileAIBind
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFileAIBind::Recalc()
{
	pValue = new CFileBindInfo;
	try
	{
		NGScene::CResourceOpener file( "AIBinds", GetKey() );
		file->Add( 4, &pValue->invBindPoses );
	}
	catch(...)
	{
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CBind
////////////////////////////////////////////////////////////////////////////////////////////////////
void CBind::Recalc()
{
	SHMatrix m;
	const NAnimation::SSkeletonPose &pose = pAnimation->GetValue();
	int nBinds = pBinds->GetValue()->invBindPoses.size();
	ASSERT( pose.size() >= nBinds );
	if ( pose.size() < nBinds )
	{
		// FUCK
		value.resize( nBinds );
		for ( int i = 0; i < nBinds; ++i )		
			Identity( &value[i] );
		Updated();
		return;
	}
	if ( invBindsWithScale.size() != nBinds || pSkeleton->GetValue()->bScale )
	{
		invBindsWithScale.resize( nBinds );
		Identity( &m );
		for ( int i = 0; i < nBinds; ++i )
		{
			CVec3 scale;
			if ( pSkeleton->GetValue()->bScale )
				scale = pose[i].scale;
			else
				scale = pSkeleton->GetValue()->bones[i].scale;
			m._11 = scale.x;
			m._22 = scale.y;
			m._33 = scale.z;
			Multiply( &invBindsWithScale[i], m, pBinds->GetValue()->invBindPoses[i] );
		}
	}
	value.resize( nBinds );
	SSkeletonMatrices global;
	global.resize( nBinds );

	for ( int i = 0; i < nBinds; ++i )
	{
		if ( pose[i].nParent >= 0 )
		{
			MakeMatrix( &m, pose[i].pos, pose[i].rot );
			Multiply( &global[i], global[ pose[i].nParent ], m );
		}
		else
			MakeMatrix( &global[i], pose[i].pos, pose[i].rot );
		Multiply( &value[i], global[i], invBindsWithScale[i] );
	}

	Updated();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CBind::operator&( CStructureSaver &f )
{
	f.Add( 1, &pAnimation );
	f.Add( 2, &pBinds );
	f.Add( 3, &pSkeleton );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
using namespace NGScene;
REGISTER_SAVELOAD_CLASS( 0x12041160, CFileBind );
REGISTER_SAVELOAD_CLASS( 0x12041161, CFileAIBind );
REGISTER_SAVELOAD_CLASS( 0x12041162, CBind );
