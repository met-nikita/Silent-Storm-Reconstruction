#include "StdAfx.h"
#include "GAnimFormat.h"
#include "GAnimTerrain.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAnimation
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTestTerrain
////////////////////////////////////////////////////////////////////////////////////////////////////
float CTestTerrain::GetHeight( float fX, float fY )
{
	//return 0;
	//return fX / SQRT_3;
	//fX / SQRT_3 / 2;
	return sin(fX/SQRT_3*10)/10 + 1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CTestTerrain::GetNormal( float fX, float fY )
{
	//CVec3 n( 0, 0, 1 );
	//CVec3 n( -1 / SQRT_3, 0, 1 );
	//CVec3 n( -1 / SQRT_3 / 2, 0, 1 );
	CVec3 n( -cos(fX/SQRT_3*10)/SQRT_3, 0, 1 );
	Normalize(&n);
	return n;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CATerrain
////////////////////////////////////////////////////////////////////////////////////////////////////
void CATerrain::GetFrame( STime t, SSkeletonPose *pPose )
{
	//pInput->GetFrame( t, pPose );
	//(*pPose)[0].pos.z += pTerrain->GetHeight( (*pPose)[0].pos.x, (*pPose)[0].pos.y );
	//return;
	
	pSkeleton.Refresh();
	int i;
	vector<int> nBones;
	nBones.push_back( pSkeleton->GetValue()->GetBoneIndex( "R_Thigh" ) );
	nBones.push_back( pSkeleton->GetValue()->GetBoneIndex( "R_Leg" ) );
	nBones.push_back( pSkeleton->GetValue()->GetBoneIndex( "R_Foot" ) );
	nBones.push_back( pSkeleton->GetValue()->GetBoneIndex( "L_Thigh" ) );
	nBones.push_back( pSkeleton->GetValue()->GetBoneIndex( "L_Leg" ) );
	nBones.push_back( pSkeleton->GetValue()->GetBoneIndex( "L_Foot" ) );

	pInput->GetFrame( t, pPose );
	SSkeletonPose &pose = *pPose;
	SSkeletonPose globals;

	for ( i=0; i<nBones.size(); ++i )
	{
		globals.push_back( pose[nBones[i]] );
		globals[i].MakeGlobal( pose );
	}

	float fRightLen = fabs( pose[nBones[1]].pos ) + fabs( pose[nBones[2]].pos );
	float fLeftLen = fabs( pose[nBones[4]].pos ) + fabs( pose[nBones[5]].pos );
	fRightLen *= 0.995f; // to prevent leg being full straight
	fLeftLen *= 0.995f;
	CVec3 right = globals[0].pos - globals[2].pos;
	right.z = 0;
	CVec3 left = globals[3].pos - globals[5].pos;
	left.z = 0;
	float fRightMaxH = sqrt(fRightLen * fRightLen - fabs2(right));
	float fLeftMaxH = sqrt(fLeftLen * fLeftLen - fabs2(left));
	fRightMaxH += pose[0].pos.z - globals[0].pos.z;
	fLeftMaxH += pose[0].pos.z - globals[3].pos.z;

	right = globals[2].pos;
	fRightMaxH += pTerrain->GetHeight( right.x, right.y );
	left = globals[5].pos;
	fLeftMaxH += pTerrain->GetHeight( left.x, left.y );

	fRightMaxH += right.z / pTerrain->GetNormal( right.x, right.y ).z;
	fLeftMaxH += left.z / pTerrain->GetNormal( left.x, left.y ).z;

	CVec3 hip = pose[0].pos;
	float fOldHipZ = hip.z;
	hip.z += pTerrain->GetHeight( hip.x, hip.y );
	if ( hip.z > fRightMaxH )
		hip.z = fRightMaxH;
	if ( hip.z > fLeftMaxH )
		hip.z = fLeftMaxH;
	float fHipShift = hip.z - fOldHipZ;
	pose[0].pos = hip;

	for ( int nLeg = 0; nLeg < 2; ++nLeg )
	{
		int nBone1 = nBones[nLeg * 3];
		int nBone2 = nBones[nLeg * 3 + 1];
		int nBone3 = nBones[nLeg * 3 + 2];

		SBonePose &bone1 = pose[nBone1], &bone2 = pose[nBone2], &bone3 = pose[nBone3], eff, bone3global;
		eff = bone3;
		eff.MakeGlobal( pose );
		CVec3 foot = eff.pos;
		eff.pos.z -= fHipShift;
		eff.pos.z /= pTerrain->GetNormal( foot.x, foot.y ).z;
		eff.pos.z += pTerrain->GetHeight( foot.x, foot.y );
		bone3global = eff;
		eff.MakeLocal( pose, nBone1 );

		CVec3 vec2 = bone2.rot.Rotate( bone3.pos );
		CVec3 vec1 = bone2.pos;

		float fLen1 = fabs(vec1);
		ASSERT( fLen1 > FP_EPSILON );
		float fLen2 = fabs(vec2);
		ASSERT( fLen2 > FP_EPSILON );
		float fDist = fabs(eff.pos);

		float fKneeAngle = FP_PI;
		if ( fDist < fLen1 + fLen2 )
		{
			if ( fDist < fLen1 - fLen2 )
				fKneeAngle = 0;
			else
				fKneeAngle = acos( (fLen1 * fLen1 + fLen2 * fLen2 - fDist * fDist) * 0.5f / fLen1 / fLen2 );
		}

		CVec3 n1, n2, axis;
		float fAngle = 0;

		n1 = -vec1;
		n2 = vec2;
		Normalize(&n1);
		Normalize(&n2);
		axis = n1 ^ n2;
		if ( fabs(axis) > 1e-6f )
		{
			fAngle = acos( Clamp( n1 * n2, -1.0f, 1.0f ) );
			CQuat q2( fKneeAngle - fAngle, axis, true );
			bone2.rot = q2 * bone2.rot;
			vec2 = bone2.rot.Rotate( bone3.pos );
		}
		float fDiffAngle = fAngle - fKneeAngle;
		if ( fDiffAngle < 0 )
			fDiffAngle = 0;

		n1 = vec1 + vec2;
		n2 = eff.pos;
		Normalize(&n1);
		Normalize(&n2);
		axis = n1 ^ n2;
		if ( fabs(axis) > 1e-6f )
		{
			fAngle = acos( Clamp( n1 * n2, -1.0f, 1.0f ) );
			CQuat q1( fAngle, axis, true );
			bone1.rot = bone1.rot * q1;
			q1.UnitInverse();
			n2 = q1.Rotate( n2 );
		}
		CQuat q4;
		if ( nLeg == 0 )
			q4.FromAngleAxis( fDiffAngle / 3, n2 );
		else
			q4.FromAngleAxis( -fDiffAngle / 3, n2 );
		bone1.rot = bone1.rot * q4;

		bone3 = bone3global;

		n1 = CVec3(0,0,1);
		n2 = pTerrain->GetNormal( foot.x, foot.y );
		Normalize(&n1);
		Normalize(&n2);
		axis = n1 ^ n2;
		CQuat q3 = QNULL, q5;
		fAngle = 0;
		if ( fabs(axis) > 1e-6f )
		{
			fAngle = acos( Clamp( n1 * n2, -1.0f, 1.0f ) );
			q3.FromAngleAxis( fAngle, axis, true );
		}
		if ( nLeg == 0 )
			q5.FromAngleAxis( -fAngle / 2, n1 );
		else
			q5.FromAngleAxis( fAngle / 2, n1 );
		bone3.rot = q5 * bone3.rot;
		bone3.rot = q3 * bone3.rot;
		bone3.MakeLocal( pose, nBone2 );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CATerrain::operator&( CStructureSaver &f )
{
	f.Add( 1, (CAnimator*)this );
	f.Add( 2, &pInput );
	f.Add( 3, &pTerrain );
	return 0;	
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CATerrainCrawl
////////////////////////////////////////////////////////////////////////////////////////////////////
void CATerrainCrawl::Init()
{
	const char *pszParticleBoneNames[] = {
		"L_UArm",
		"R_UArm",
		"L_Thigh",
		"R_Thigh",
		"L_LArm",
		"L_Hand",
		"R_LArm",
		"R_Hand",
		"L_Leg",
		"L_Foot",
		"R_Leg",
		"R_Foot",
	};
	pSkeleton.Refresh();
	int i;
	for ( i = 0; i < 12; ++i )
		nParticleBones.push_back( pSkeleton->GetValue()->GetBoneIndex( pszParticleBoneNames[i] ) );

	for ( i = 0; i < pSkeleton->GetValue()->bones.size(); ++i )
	{
		string pszBoneName = pSkeleton->GetValue()->bones[i].szName;
		if ( pszBoneName == "Hip" )
			restore.push_back( SRestoreBone(0, 1, 2) );
		else if ( pszBoneName == "L_UArm" )
			restore.push_back( SRestoreBone(0, 4) );
		else if ( pszBoneName == "L_LArm" )
			restore.push_back( SRestoreBone(4, 5) );
		else if ( pszBoneName == "R_UArm" )
			restore.push_back( SRestoreBone(1, 6) );
		else if ( pszBoneName == "R_LArm" )
			restore.push_back( SRestoreBone(6, 7) );
		else if ( pszBoneName == "L_Thigh" )
			restore.push_back( SRestoreBone(2, 8, 9) );
		else if ( pszBoneName == "L_Leg" )
			restore.push_back( SRestoreBone(8, 9, 2) );
		else if ( pszBoneName == "R_Thigh" )
			restore.push_back( SRestoreBone(3, 10, 11) );
		else if ( pszBoneName == "R_Leg" )
			restore.push_back( SRestoreBone(10, 11, 3) );
		else
			restore.push_back( SRestoreBone() );
	}
	const int pairs[] = {
		0, 1,
		0, 2,
		0, 3,
		1, 2,
		1, 3,
		2, 3,
		0, 4,
		4, 5,
		1, 6,
		6, 7,
		2, 8,
		8, 9,
		3, 10,
		10, 11,
	};
	InitSticks( pairs, 14 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CATerrainCrawl::GetFrame( STime t, SSkeletonPose *pPose )
{
	int i;
	pInput->GetFrame( t, pPose );
	particles.clear();
	SBonePose l_hand_mem, r_hand_mem;
	for ( i=0; i<nParticleBones.size(); ++i )
	{
		SBonePose p = (*pPose)[ nParticleBones[i] ];
		p.MakeGlobal( *pPose );
		particles.push_back( p.pos );
		if ( i == 5 )
			l_hand_mem = p;
		if ( i == 7 )
			r_hand_mem = p;
	}
	CVec3 line_before = particles[7] - particles[5];
	for ( int nS = 0; nS < sticks.size(); ++nS )
		sticks[nS].fRest = fabs( particles[ sticks[nS].nP2 ] - particles[ sticks[nS].nP1 ] );
	// up due to terrain
	for ( i=0; i<particles.size(); ++i )
	{
		CVec3 pos = particles[i];
		particles[i].z += pTerrain->GetHeight( pos.x, pos.y );
	}
	// take pose
	for ( int nI = 0; nI < 10; ++nI )
	{
		for ( i=0; i<particles.size(); ++i )
		{
			CVec3 pos = particles[i];
			float fH = pTerrain->GetHeight( pos.x, pos.y );// + 0.08f;
			if ( particles[i].z < fH )
				particles[i].z = fH;
		}
		// cheat constraints
		{
			CVec3 &p1 = particles[3], &p2 = particles[2], &p3 = particles[8], &p4 = particles[9];
			SPlane plane;
			plane.Set( p1, p2, p3 );
			float d = plane.GetDistanceToPoint( p4 );
			if ( d < 0.1f )
				p4 -= (d - 0.1f) * plane.n;
		}
		{
			CVec3 &p1 = particles[10], &p2 = particles[3], &p3 = particles[2], &p4 = particles[11];
			SPlane plane;
			plane.Set( p1, p2, p3 );
			float d = plane.GetDistanceToPoint( p4 );
			if ( d < 0.1f )
				p4 -= (d - 0.1f) * plane.n;
		}
		IterateSticks();
	}
	RestoreAll( particles, pPose );

	CVec3 line_after = particles[7] - particles[5];
	bool bNormal = ( Normalize( &line_after ) && Normalize( &line_before ) );
	CVec3 axis = line_before ^ line_after;
	if ( bNormal && fabs(axis) > 1e-6f )
	{
		float fAngle = acos( Clamp( line_before * line_after, -1.0f, 1.0f ) );
		SBonePose &l_hand = (*pPose)[ nParticleBones[5] ];
		int nLParent = l_hand.nParent;
		SBonePose &r_hand = (*pPose)[ nParticleBones[7] ];
		int nRParent = r_hand.nParent;
		CQuat q( fAngle, axis, true );
		l_hand.MakeGlobal( *pPose );
		l_hand.rot = q * l_hand_mem.rot;
		l_hand.MakeLocal( *pPose, nLParent );
		r_hand.MakeGlobal( *pPose );
		r_hand.rot = q * r_hand_mem.rot;
		r_hand.MakeLocal( *pPose, nRParent );
	}
	particles.clear();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CATerrainCrawl::operator&( CStructureSaver &f )
{
	f.Add( 1, (CAParticle*)this );
	f.Add( 2, &pInput );
	f.Add( 3, &pTerrain );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CATerrainSimple::GetFrame( STime t, SSkeletonPose *pPose )
{
	pInput->GetFrame( t, pPose );
	(*pPose)[0].pos.z += pTerrain->GetHeight( (*pPose)[0].pos.x, (*pPose)[0].pos.y );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CATerrainSimple::operator&( CStructureSaver &f )
{
	f.Add( 1, (CAnimator*)this );
	f.Add( 2, &pInput );
	f.Add( 3, &pTerrain );
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
using namespace NAnimation;
REGISTER_SAVELOAD_CLASS( 0x11651140, CATerrain );
REGISTER_SAVELOAD_CLASS( 0x11651141, CTestTerrain );
REGISTER_SAVELOAD_CLASS( 0x11651142, CATerrainCrawl );
REGISTER_SAVELOAD_CLASS( 0x12881190, CATerrainSimple );
