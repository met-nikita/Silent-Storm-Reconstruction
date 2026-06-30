#ifndef __2DARRAY_H_
#define __2DARRAY_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
////////////////////////////////////////////////////////////////////////////////////////////////////
template < class T > 
struct CBoundCheck
{
	T *data;
	int nSize;
	CBoundCheck( T *d, int nS ) { data = d; nSize = nS; }
	T& operator[]( int i ) const { ASSERT( i>=0 && i<nSize ); return data[i]; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
template < class T >
class CArray2D
{
	typedef T *PT;
	T *data;
	T **pData;
	int nXSize, nYSize;
	void Copy( const CArray2D &a ) { nXSize = a.nXSize; nYSize = a.nYSize; Create(); for(int i = 0; i < nXSize*nYSize; i++ ) data[i] = a.data[i]; }
	void Destroy() { delete[] data; delete[] pData; }
	void Create() { data = new T[ nXSize*nYSize ]; pData = new PT[ nYSize ]; for(int i=0;i<nYSize;i++) pData[i] = data+i*nXSize; }
public:
	CArray2D( int xsize = 1, int ysize = 1 ) { nXSize = xsize; nYSize = ysize; Create(); }
	CArray2D( const CArray2D &a ) { Copy(a); }
	CArray2D& operator=( const CArray2D &a ) { Destroy(); Copy(a); return *this; }
	~CArray2D() { Destroy(); }
	void SetSizes( int xsize, int ysize ) { if ( nXSize == xsize && nYSize == ysize ) return; Destroy(); nXSize = xsize; nYSize = ysize; Create(); }
	void Clear() { SetSizes( 1,1 ); }
#ifdef _DEBUG
	CBoundCheck<T> operator[]( int i ) const { ASSERT( i>=0 && i<nYSize ); return CBoundCheck<T>( pData[i], nXSize ); }
#else
	T* operator[]( int i ) const { ASSERT( i>=0 && i<nYSize ); return pData[i]; }
#endif
	int GetXSize() const { return nXSize; }
	int GetYSize() const { return nYSize; }
	void FillZero() { memset( data, 0, sizeof( T ) * nXSize * nYSize ); }
	void FillEvery( const T &a ) { for ( int i = 0; i < nXSize * nYSize; i++ ) data[i] = a; }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> class CArray3D;
template<class T> 
class CArray3DFetcher
{
	T *pData;
	CArray3D<T> &a;
public:
	CArray3DFetcher( T *_pData, CArray3D<T> &_a ): pData(_pData), a(_a) {}
#ifdef _DEBUG
	CBoundCheck<T> operator[]( int i ) const { ASSERT( i>=0 && i<a.GetYSize() ); return CBoundCheck<T>( &pData[i*a.GetXSize()], a.GetXSize() ); }
#else
	T* operator[]( int i ) const { ASSERT( i>=0 && i<a.GetYSize() ); return &pData[i*a.GetXSize()]; }
#endif
};
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> 
class CArray3D
{
	ZDATA
		vector<T> data;
	int nXSize, nYSize, nZSize, nXYSize;
public:
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&data); f.Add(3,&nXSize); f.Add(4,&nYSize); f.Add(5,&nZSize); f.Add(6,&nXYSize); return 0; }
	CArray3D(): nXSize(0), nYSize(0), nZSize(0), nXYSize(0){}
	int GetXSize() const { return nXSize; }
	int GetYSize() const { return nYSize; }
	int GetZSize() const { return nZSize; }
	void SetSizes( int nX, int nY, int nZ ) { nXSize = nX; nYSize = nY; nZSize = nZ; nXYSize = nXSize * nYSize; data.resize( nZSize * nXYSize ); }
	void FillEvery( const T &a ) { fill( data.begin(), data.end(), a ); }
#ifdef _DEBUG
	CArray3DFetcher<T> operator[]( int i ) { ASSERT( i>=0 && i<nZSize ); return CArray3DFetcher<T>( &data[i*nXYSize], *this ); }
#else
	CArray3DFetcher<T> operator[]( int i ) { ASSERT( i>=0 && i<nZSize ); return CArray3DFetcher<T>( &data[i*nXYSize], *this ); }
#endif
};
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
