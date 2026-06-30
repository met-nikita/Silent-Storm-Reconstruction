#ifndef __FILEPACKAGE_H_
#define __FILEPACKAGE_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "Streams.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
typedef int FILE_ID;
////////////////////////////////////////////////////////////////////////////////////////////////////
// read only stream
class IFilesPackage;
class CPackageStream: public CBufferedStream
{
	CPtr<IFilesPackage> pPack;
	int nOffset;
	//
	virtual unsigned int DoRead( unsigned int nPos, void *pDest, unsigned int nSize );
	virtual unsigned int DoWrite( unsigned int nPos, const void *pSrc, unsigned int nSize );
	CPackageStream( const CPackageStream &a ) { ASSERT(0); }
	CPackageStream& operator=( const CPackageStream &a ) { ASSERT(0); return *this;}
public:
	CPackageStream( IFilesPackage *_pPack, FILE_ID nFileID );
};
////////////////////////////////////////////////////////////////////////////////////////////////////
IFilesPackage* OpenFilesPackage( const char *pszFileName );
IFilesPackage* OpenCachedFilesPackage( const char *pszFileName );
bool DoesFileExist( IFilesPackage *pPack, FILE_ID nFileID );
bool UpdateFilesPackage( const char *pszFileName, const char *pszDir );
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif