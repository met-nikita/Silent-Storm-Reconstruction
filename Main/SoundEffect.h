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
	STime stBeginTime;
	STime tLastSound;
	CDBPtr<NDb::CSoundInstance> pInstance;
	CDGPtr< CFuncBase<STime> > pTime;
	CDGPtr< CFuncBase<CVec3> > pPlacement;
	CObj<NFMSound::CSound3D> pSound;
	vector<int> flags;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&stBeginTime); f.Add(3,&tLastSound); f.Add(4,&pInstance); f.Add(5,&pTime); f.Add(6,&pPlacement); f.Add(7,&pSound); f.Add(8,&flags); return 0; }

	CSoundInstance() {}
	CSoundInstance( NDb::CSoundInstance *_pInstance, STime t, CFuncBase<STime> *_pTime, CFuncBase<CVec3> *pPos, const vector<int> &flags );
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
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __SOUNDEFFECT_H_