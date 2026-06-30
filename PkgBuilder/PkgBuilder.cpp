#include "stdafx.h"
#include "..\FileIO\FilesPackage.h"
#include <iostream>
////////////////////////////////////////////////////////////////////////////////////////////////////
bool UpdateFilesPackage( CDataStream *pErr, const char *pszFileName, const char *pszDir );
int __cdecl main(int argc, char* argv[])
{
	if ( argc != 3 )
	{
		cout << "USAGE: pkgBuilder pkgFile srcDir" << endl;
		return 0;
	}
	cout << "updating " << argv[1] << endl;
	CMemoryStream m;
	bool bOk = UpdateFilesPackage( &m, argv[1], argv[2] );
	m << (char)0;
	cout << m.GetBuffer();
	//if ( bOk )
		//cout << "Ok" << endl;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
