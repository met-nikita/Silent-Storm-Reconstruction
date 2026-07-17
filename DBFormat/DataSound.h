#ifndef __DATASOUND_H_
#define __DATASOUND_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\ADOImport\BasicDB.h"
#include "DataConst.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NDb
{
////////////////////////////////////////////////////////////////////////////////////////////////////
enum ESoundType
{
	ST_PERMANENT,
	ST_RANDOM,
	ST_REALTIME,
	ST_WIND
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSound: public CDBRecord
{
	OBJECT_BASIC_METHODS( CSound );
public:
	ZDATA_(CDBRecord)
	bool bLoop;
	float fMinDistance;
	float fMaxDistance;
	int nPriority;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&bLoop); f.Add(3,&fMinDistance); f.Add(4,&fMaxDistance); f.Add(5,&nPriority); f.Add(6,&nEndingSamples); f.Add(7,&nStartSamples); return 0; }
	int nEndingSamples = 0;
	int nStartSamples = 0;
	//
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTSound;
class CSoundVariant: public CDBRecord
{
	OBJECT_BASIC_METHODS( CSoundVariant );
public:
	ZDATA_(CDBRecord)
	CPtr<CSound>  pSound;
	CPtr<CTSound> pTemplate;
	vector<SVariantFlags> flags;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&pSound); f.Add(3,&pTemplate); f.Add(4,&flags); return 0; } // retail @0x41d460: flags@4 (dev's old tag 5 left them empty on retail-db reads)
	//
	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTSound: public CRndPtr<CSoundVariant>
{
	OBJECT_BASIC_METHODS(CTSound);
public:
	CSoundVariant* GetSound( SRand *pRand ) const;
	CSoundVariant* GetSound( SRand *pRand, const vector<int> &params ) const;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
enum EMusicType : int	// fixed underlying type so Sound.h can forward-declare it (opaque enum)
{
	MT_AMBIENT,
	MT_PRECOMBAT,
	MT_COMBAT
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CMusic -- retail record (serializer @0x41d4d0, byte-exact tag table):
//   1=CDBRecord base, 2=szFileName, 3=flags (vector<SVariantFlags>), 4=nFadeIn, 5=nFadeOut,
//   6=nPlayTime, 7=nRndPlayTime, 8=nSilence, 9=nRndSilence.  All times are MILLISECONDS.
// The retail music machine (NSound::CSoundScene) is data-driven off these:
//   nFadeIn      -- fade-in / crossfade-in sec*1000 when the track starts (retail data: 0)
//   nFadeOut     -- FadeOutMusic fade length (@0x304c20 reads +0x2c; retail data: 20000 = 20s)
//   nPlayTime  + rnd(nRndPlayTime)  -- play window before the track is faded (@0x304ae0)
//   nSilence   + rnd(nRndSilence)   -- silence window before the next track starts (@0x304b80)
// Retail Steam game.db carries these columns (FadeIn/FadeOut/PlayTime/RndPlayTime/Silence/
// RndSilence, e.g. Music 1 "Ambient" = 0/20000/180000/120000/60000/120000); the member defaults
// below are those retail-typical values for a (dev) db lacking the columns.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CMusic: public CDBRecord
{
  OBJECT_BASIC_METHODS(CMusic);
public:
	ZDATA_(CDBRecord)
	string szFileName;
	EMusicType eType;
	vector<SVariantFlags> flags;
	int nFadeIn = 0;
	int nFadeOut = 20000;
	int nPlayTime = 180000;
	int nRndPlayTime = 120000;
	int nSilence = 60000;
	int nRndSilence = 120000;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&szFileName); f.Add(3,&flags); f.Add(4,&nFadeIn); f.Add(5,&nFadeOut); f.Add(6,&nPlayTime); f.Add(7,&nRndPlayTime); f.Add(8,&nSilence); f.Add(9,&nRndSilence); f.Add(10,&eType); return 0; }	// tags 1-9 = retail @0x41d4d0. eType is a dev field with no retail tag -> parked at 10 (retail uses only 1-9) so retail-read defaults it to MT_AMBIENT instead of garbage.

  virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// CTMusic - weighted random pool of CMusic variants (release table 0x7d MusicTemplates). Serialization
// is inherited from CRndPtr; GetMusic mirrors the release accessors (return 0 on an empty pool).
////////////////////////////////////////////////////////////////////////////////////////////////////
class CTMusic: public CRndPtr<CMusic>
{
	OBJECT_BASIC_METHODS( CTMusic );
public:
	CMusic* GetMusic( SRand *pRand ) const
	{
		if ( variants.empty() )
			return 0;
		return variants[ roulette.GetRandomSector( pRand ) ];
	}
	CMusic* GetMusic( SRand *pRand, const vector<int> & /*params*/ ) const
	{
		// CMusic carries no per-variant flags, so there is nothing to filter on -> plain roulette pick.
		return GetMusic( pRand );
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSoundEffect;
class CSoundInstance: public CDBRecord
{
	OBJECT_BASIC_METHODS(CSoundInstance);
public:
	ZDATA_(CDBRecord)
	CPtr<CTSound> pSound;
	int nStartTime;
	int nCycleCount;
	bool bFadeIn;
	bool bFadeOut;
	int  nFadeSamples;
	ESoundType eSoundType;
	float fSoundAvgInterval;
	CPtr<CSoundEffect> pEffect;
	int nVolume;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&pSound); f.Add(3,&nStartTime); f.Add(4,&nCycleCount); f.Add(5,&bFadeIn); f.Add(6,&bFadeOut); f.Add(7,&nFadeSamples); f.Add(8,&eSoundType); f.Add(9,&fSoundAvgInterval); f.Add(10,&pEffect); f.Add(11,&nVolume); return 0; }

	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSoundEffect: public CDBRecord
{
	OBJECT_BASIC_METHODS(CSoundEffect);
public:
	ZDATA_(CDBRecord)
	vector<CPtr<CSoundInstance> > instances;
	ZEND int operator&( CStructureSaver &f ) { f.Add(1,(CDBRecord*)this); f.Add(2,&instances); return 0; }

	virtual void Import();
};
////////////////////////////////////////////////////////////////////////////////////////////////////
CTSound* GetTSound( int nTSoundID );
CSound* GetSound( int nID );
CMusic* GetMusic( int nID );
CTMusic* GetTMusic( int nID );	// retail @0x3f98c0 -- the MusicTemplates pool; mission ambient/combat music resolve THROUGH this table
CSoundEffect* GetSoundEffect( int nID );
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __DATASOUND_H_