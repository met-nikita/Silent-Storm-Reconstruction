#ifndef __GANIMFORMAT_H_
#define __GANIMFORMAT_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "GResource.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NAnimation
{
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SBone
{
	string szName; // bone name
	int nParent; // parent index in array of bones (default)
	CVec3 pos; // default pose position (pivot)
	CQuat rot; // default pose orientation
	CVec3 scale; // scale for this bone

	int operator&( CStructureSaver &f )
	{
		f.Add( 1, &szName );
		f.Add( 2, &nParent );
		f.Add( 3, &pos );
		f.Add( 4, &rot );
		f.Add( 5, &scale );
		return 0;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SAnimationHeader
{
	vector< int > indices; // bone indices in skeleton
	float fLength; // animation length in keys
	float fFrameRate; // frame rate
	int nRoots; // number of (pos,rot) bones
	int nBones; // number of (rot) bones
	int nAddBones; // number of (parent,pos,rot) bones;
	bool bScale; // if true, then (nRoots + nBones) is number of (pos,rot,scale) bones
	
	SAnimationHeader() { nRoots = nBones = nAddBones = 0; fLength = 0; fFrameRate = 30; bScale = false; }
	int operator&( CStructureSaver &f )
	{
		f.Add( 1, &indices );
		f.Add( 2, &fLength );
		f.Add( 3, &fFrameRate );
		f.Add( 4, &nRoots );
		f.Add( 5, &nBones );
		f.Add( 6, &nAddBones );
		f.Add( 7, &bScale );
		return 0;
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SRootAnimKey
{
	CVec3 pos;
	CQuat rot;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SBoneAnimKey
{
	CQuat rot;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SAddBoneAnimKey
{
	int nParent;
	CVec3 pos;
	CQuat rot;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SMSRAnimKey
{
	CVec3 pos;
	CQuat rot;
	CVec3 scale;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFileSkeletonInfo : public CObjectBase
{
	OBJECT_BASIC_METHODS(CFileSkeletonInfo);
public:
	vector< SBone > bones;
	bool bScale;

	CFileSkeletonInfo() {}
	const int GetBoneIndex( const char *pszName );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFileAnimationInfo : public CObjectBase
{
	OBJECT_BASIC_METHODS(CFileAnimationInfo);
public:
	SAnimationHeader hdr;
	vector< SRootAnimKey > keysRoots;
	vector< SBoneAnimKey > keysBones;
	vector< SAddBoneAnimKey > keysAddBones;
	vector< SMSRAnimKey > keysMSR;

	CFileAnimationInfo() {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFileSkeleton: public NGScene::CResourceLoader<int, CFileSkeletonInfo>
{
	OBJECT_BASIC_METHODS(CFileSkeleton);
protected:
	virtual void Recalc();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFileAnimation: public NGScene::CResourceLoader<int, CFileAnimationInfo>
{
	OBJECT_BASIC_METHODS(CFileAnimation);
protected:
	virtual void Recalc();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CFileLocators: public NGScene::CResourceLoader<int, CFileSkeletonInfo>
{
	OBJECT_BASIC_METHODS(CFileLocators);
protected:
	virtual void Recalc();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif