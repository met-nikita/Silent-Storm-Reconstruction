#include "StdAfx.h"
#include "SoundFormat.h"
#include "..\FModSound\FMSound.h"
#include "..\DBFormat\DataSound.h"

namespace NSound
{
////////////////////////////////////////////////////////////////////////////////////////////////////
NGScene::CFileRequest* CFileSample3D::CreateRequest()
{
	NDb::CSound *pSound = NDb::GetSound( GetKey() );
	if ( !pSound )
	{
		ASSERT(0);
		return 0;
	}
	return new NGScene::CFileRequest( "Sounds", GetKey() );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFileSample3D::RecalcValue( NGScene::CFileRequest *p )
{
	NDb::CSound *pSound = NDb::GetSound( GetKey() );
	if ( !pSound )
		return;
	NGScene::CFileRequest &file = *p;
	const int nSize = file->GetSize();
	if ( nSize )
	{
		CMemoryStream *pStream = file.GetStream();
		pValue = NFMSound::LoadSample3D( pStream->GetBuffer(), pStream->GetSize(), pSound->fMinDistance, pSound->fMaxDistance, pSound->nPriority );
	}
	else
	{
		pValue = NFMSound::GetDefault3DSound();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CFileSample2D::Recalc()
{
	try
	{
		NGScene::CResourceFileOpener file( "Sounds", GetKey() );

		const int nSize = file->GetSize();
		vector<char> buff( nSize );
		file->Read( &buff[0], nSize );
		pValue = NFMSound::LoadSample2D( &buff[0], nSize );
	}
	catch(...)
	{
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
using namespace NSound;
REGISTER_SAVELOAD_CLASS( 0x03081140, CFileSample3D );
REGISTER_SAVELOAD_CLASS( 0x03081141, CFileSample2D );
