#ifndef __SOUNDEFFECT_H_
#define __SOUNDEFFECT_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NSound
{
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSoundInstance: public CFuncBase<bool>
{
	OBJECT_BASIC_METHODS(CSoundInstance);
protected:
	virtual bool NeedUpdate() { return pTime.Refresh() | pPlacement.Refresh(); }
	virtual void Recalc();
private:
	ZDATA
	STime tStartTime; // elapsed seek offset, v1.2/Sentinels +0x18
	STime stBeginTime;
	STime tLastSound;
	CDBPtr<NDb::CSoundInstance> pInstance;
	CDGPtr< CFuncBase<STime> > pTime;
	CDGPtr< CFuncBase<CVec3> > pPlacement;
	CObj<NFMSound::CSound3D> pSound;
	vector<int> flags;
public:
	// Retail v1.2 0x709c20 / Sentinels 0x409370: original tags, not a save migration.
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&tStartTime); f.Add(3,&stBeginTime); f.Add(4,&tLastSound); f.Add(5,&pInstance); f.Add(6,&pTime); f.Add(7,&pPlacement); f.Add(8,&pSound); f.Add(9,&flags); return 0; }

	CSoundInstance() : tStartTime(0), stBeginTime(0), tLastSound(0) { value = false; }
	CSoundInstance( NDb::CSoundInstance *_pInstance, STime t, CFuncBase<STime> *_pTime, CFuncBase<CVec3> *pPos, const vector<int> &flags );

	void Pause( bool bPause );	// retail @0x308eb0 leg: freeze/resume the instance's channel
};
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSoundEffect: public CObjectBase
{
	OBJECT_BASIC_METHODS(CSoundEffect);
	ZDATA
	vector<CDGPtr<CFuncBase<bool> > > instances;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&instances); return 0; }

public:
	CSoundEffect() {}
	CSoundEffect( NDb::CSoundEffect *pEff, STime stBeginTime, CFuncBase<STime> *pTime, CFuncBase<CVec3> *pPos, const vector<int> &flags );

	bool Update();
	void Pause( bool bPause );	// retail @0x308eb0: forward to every live CSoundInstance
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __SOUNDEFFECT_H_
