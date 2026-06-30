#include "stdafx.h"
#include "..\Misc\Geom.h"
#include "..\FileIO\BasicChunk1.h"
#include "..\Image\ImageOperation.h"
#include "..\Image\ImageTGA.h"
#include <LifeStudioHeadAPIGDP.h>
#include <LifeStudioHeadAPI.h>
#include <LifeStudioHeadAPIInit.h>
#include <LifeStudioHeadAPIMMTS.h>
////////////////////////////////////////////////////////////////////////////////////////////////////
const int N_TEXTURE_MAPS = 7;
struct STextureMap
{
	float fSize;
	float fUShift;
	float fVShift;
};
STextureMap textureMaps[N_TEXTURE_MAPS] = {
	{ 0.5f, 0, 0 },
	{ 0.25f, 0, 0.5f },
	{ 0.25f, 0, 0.75f },
	{ 0.25f, 0.25f, 0.5f },
	{ 0.5f, 0.5f, 0 },
	{ 0.5f, 0.5f, 0.5f },
	{ 0.25f, 0.25f, 0.75f } // for case :)
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void OutputGDPObject( LifeStudioHeadAPI::IGDPObject *pGDPObject, int nLevel )
{
	char *s = new char[nLevel+1];
	memset( s, ' ', nLevel );
	s[nLevel] = 0;

	if ( !pGDPObject )
	{
		printf( "%sERROR: Zero object!!!\n\n", s );
		delete [] s;
		return;
	}

	printf( "%sVertices: %d\n", s, pGDPObject->VerticesCount() );
	for ( int i = 0; i < pGDPObject->MaterialsCount(); ++i )
	{
		LifeStudioHeadAPI::ObjectMaterial mat;
		if ( pGDPObject->Material( i, mat ) )
		{
			printf( "%sMaterial: \"%s\"\n", s, mat.name );
			bool bDouble = false;
			if ( mat.flags & LIFESTUDIOHEADAPI_MATERIAL_DOUBLESIDED)
				bDouble = true;
			printf( "%s Faces: %d, TextureName: \"%s\"%s\n", s, pGDPObject->TrianglesCount(i), mat.textureName, bDouble ? ", double-sided" : "" );
		}
	}
	printf("\n");
	LifeStudioHeadAPI::IGDPObject *pGDPSubObject = 0;
	for ( int nSubObj = 0; nSubObj < pGDPObject->SubObjectsCount(); ++nSubObj )
	{
		printf( "%s    SubObject: \"%s\" of type \"%s\"\n", s, pGDPObject->SubObjectName(nSubObj), pGDPObject->SubObjectType(nSubObj) );
		pGDPSubObject = pGDPObject->SubObject(nSubObj);
		OutputGDPObject( pGDPSubObject, nLevel + 4 );
		if ( pGDPSubObject )
			pGDPSubObject->Destroy();
	}

	delete [] s;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void OutputGDPInfo( const char *pszInput )
{
	LifeStudioHeadAPI::IGDPFile *pGDPFile = 0;
	LifeStudioHeadAPI::IGDPObject *pGDPObject = 0;

	pGDPFile = LifeStudioHeadAPI::IGDPFile::Create( pszInput );
	if ( !pGDPFile )
	{
		printf( "ERROR: Opening GDP File!!!\n" );
		return;
	}
	printf( "\nGDP File: %s\n\n", pszInput );
	for ( int nObj = 0; nObj < pGDPFile->ObjectsCount(); ++nObj )
	{
		printf( "Object: \"%s\"\n", pGDPFile->ObjectName(nObj) );
		pGDPObject = pGDPFile->Object(nObj);
		OutputGDPObject( pGDPObject, 0 );
		if ( pGDPObject )
			pGDPObject->Destroy();
	}
	pGDPFile->Destroy();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SConvertedData
{
	vector< CMemoryStream > animatorStreams;
	vector< int > numberVertices;
	vector< CVec2 > UVs;
	vector< CTPoint<int> > copys;
	vector< int > normalsIndices;
	vector< WORD > normalsData;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
void OutputInGameFormat( SConvertedData &data, const char *pszOutput )
{
	CFileStream out;
	out.OpenWrite( pszOutput );
	{
		CStructureSaver file( out, CStructureSaver::WRITE );
		file.Add( 1, &data.animatorStreams );
		file.Add( 2, &data.numberVertices );
		file.Add( 3, &data.copys );
		file.Add( 4, &data.UVs );
		file.Add( 5, &data.normalsData );
		file.Add( 6, &data.normalsIndices );
	}
	out.CloseFile(); 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void OutputInOBJFormat( SConvertedData &data, const char *pszOutput )
{
	vector<CVec3> vertexBuffer;
	vector<CVec3> normalBuffer;
	vertexBuffer.resize( data.UVs.size() );
	normalBuffer.resize( data.UVs.size() );
	memset( &normalBuffer[0], 0, sizeof(CVec3) * normalBuffer.size() );

	// vertices
	LifeStudioHeadAPI::IMMTree *pLSTree = LifeStudioHeadAPI::IMMTree::Create();
	if ( !pLSTree || !pLSTree->Load( "tree.mma" ) )
		return;
	int nVert = 0;
	for ( int i = 0; i < data.animatorStreams.size(); ++i )
	{
		LifeStudioHeadAPI::IAnimator *pLSAnimator = LifeStudioHeadAPI::IAnimator::Create();
		if ( !pLSAnimator || !pLSAnimator->Load( (const char *)data.animatorStreams[i].GetBuffer(), data.animatorStreams[i].GetSize() ) )
		{
			pLSAnimator->Destroy();
			return;
		}
		pLSAnimator->RegisterMacroMuscle( pLSTree->RootMacroMuscle() );
		pLSAnimator->ClearAllMacroMuscles();
		pLSAnimator->ComputePhysics();
		pLSAnimator->FillUnused( true );
		pLSAnimator->Process( &(vertexBuffer[nVert].x), 3 );
		pLSAnimator->Destroy();
		nVert += data.numberVertices[i];
	}
	pLSTree->Destroy();
	for ( int i = 0; i < data.copys.size(); ++i )
		vertexBuffer[ data.copys[i].y ] = vertexBuffer[ data.copys[i].x ];

	SHMatrix transform;
	CQuat q;
	q.FromAngleAxis( -FP_PI2, CVec3(0,0,1) );
	MakeMatrix( &transform, CVec3( 0, -0.035f, 0 ), q, CVec3( 0.014f, 0.014f, 0.014f ) );

	FILE *out = fopen( pszOutput, "wt" );
	for ( int i = 0; i < vertexBuffer.size(); ++i )
	{
		CVec3 res;
		transform.RotateHVector( &res, vertexBuffer[i] );
		fprintf( out, "v %f %f %f\n", res.x, res.y, res.z );
	}
	// UVs
	for ( int i = 0; i < data.UVs.size(); ++i )
		fprintf( out, "vt %f %f\n", data.UVs[i].u, data.UVs[i].v );

	// normals
	int nFrom = 0;
	for ( int i = 0; i < data.normalsIndices.size(); ++i )
	{
		int nTo = data.normalsIndices[i];
		CVec3 &v1 = vertexBuffer[ data.normalsData[nFrom] ];
		CVec3 &v2 = vertexBuffer[ data.normalsData[nFrom+1] ];
		CVec3 &v3 = vertexBuffer[ data.normalsData[nFrom+2] ];
		CVec3 normal = (v2 - v1) ^ (v3 - v1);
		Normalize(&normal);
		for ( int j = nFrom; j < nTo; ++j )
			normalBuffer[ data.normalsData[j] ] += normal;
		nFrom = nTo;
	}

	for ( int i = 0; i < normalBuffer.size(); ++i )
	{
		CVec3 res;
		transform.RotateVector( &res, normalBuffer[i] );
		Normalize( &res );
		fprintf( out, "vn %f %f %f\n", res.x, res.y, res.z );
	}

	// faces
	nFrom = 0;
	for ( int i = 0; i < data.normalsIndices.size(); ++i )
	{
		int nTo = data.normalsIndices[i];
		WORD i1 = data.normalsData[nFrom] + 1;
		WORD i2 = data.normalsData[nFrom+1] + 1;
		WORD i3 = data.normalsData[nFrom+2] + 1;
		fprintf( out, "f %d/%d/%d %d/%d/%d %d/%d/%d\n", i1, i1, i1, i2, i2, i2, i3, i3, i3 );
		nFrom = nTo;
	}

	fclose(out);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void ConvertGDPFile( const char *pszInput, const char *pszOutput, const char *pszTexPath, int nTextureSize, bool bObjFormat )
{
	try
	{

	char pszName[256];
	int nTotalVertices = 0;
	int nTotalMaterials = 0;
	LifeStudioHeadAPI::IGDPFile *pGDPFile = LifeStudioHeadAPI::IGDPFile::Create( pszInput );
	if ( !pGDPFile )
	{
		printf( "Error: Cannot open GDP file.\n" );
		return;
	}
	vector<LifeStudioHeadAPI::IGDPObject*> objects;
	for ( int nObj = 0; nObj < pGDPFile->ObjectsCount(); ++nObj )
	{
		LifeStudioHeadAPI::IGDPObject *pGDPObject = pGDPFile->Object(nObj);
		if ( !pGDPObject )
		{
			printf( "Error: Zero object.\n" );
			continue;
		}
		if ( pGDPObject->IsTransformable() )
		{
			printf( "Error: Transformable object.\n" );
			continue;
		}

		objects.push_back( pGDPObject );
		nTotalVertices += pGDPObject->VerticesCount();
		nTotalMaterials += pGDPObject->MaterialsCount();
		for ( int nSubObj = 0; nSubObj < pGDPObject->SubObjectsCount(); ++nSubObj )
		{
			LifeStudioHeadAPI::IGDPObject *pGDPSubObject = pGDPObject->SubObject(nSubObj);
			if ( !pGDPSubObject )
			{
				printf( "Error: Zero subobject.\n" );
				continue;
			}
			if ( pGDPSubObject->IsTransformable() )
			{
				printf( "Error: Transformable subobject.\n" );
				continue;
			}
			objects.push_back( pGDPSubObject );
			nTotalVertices += pGDPSubObject->VerticesCount();
			nTotalMaterials += pGDPSubObject->MaterialsCount();
		}
	}

	NImage::CImage picture;
	picture.SetSizes( nTextureSize, nTextureSize );
	picture.FillZero();

	SConvertedData resultData;
	vector< CMemoryStream > &animatorStreams = resultData.animatorStreams;
	vector< int > &numberVertices = resultData.numberVertices;
	vector< CVec2 > &UVs = resultData.UVs;
	vector< CTPoint<int> > &copys = resultData.copys;
	vector< int > &normalsIndices = resultData.normalsIndices;
	vector< WORD > &normalsData = resultData.normalsData;

	vector< WORD > indices;
	vector< int > verticesToMaterials;

	animatorStreams.resize( objects.size() );
	numberVertices.resize( objects.size() );
	UVs.resize( nTotalVertices );
	verticesToMaterials.resize( nTotalVertices );
	for ( int i = 0; i < nTotalVertices; ++i )
		verticesToMaterials[i] = -1;

	int nVert = 0;
	int nMaterial = 0;
	int nNormalIndex = 0;

	for ( int nObj = 0; nObj < objects.size(); ++nObj )
	{
		LifeStudioHeadAPI::IGDPObject *pGDPObject = objects[nObj];

		int nLength = pGDPObject->DefaultAnimatorDataSize();
		char *pBuf = new char[nLength];
		pGDPObject->DefaultAnimatorData( pBuf );
		animatorStreams[nObj].Write( pBuf, nLength );
		delete [] pBuf;

		int nObjVerts = pGDPObject->VerticesCount();
		int nObjMaterials = pGDPObject->MaterialsCount();
		const float *pUVs = pGDPObject->UV();

		int nObjIndex = indices.size();
		for ( int nMat = 0; nMat < nObjMaterials; ++nMat, ++nMaterial )
		{
			if ( nMaterial >= N_TEXTURE_MAPS )
				continue;
			int nTris = pGDPObject->TrianglesCount(nMat);
			if ( !nTris )
				continue;
			LifeStudioHeadAPI::ObjectMaterial material;
			if ( !pGDPObject->Material( nMat, material ) )
				continue;

			STextureMap &textureMap = textureMaps[nMaterial];

			bool bDoubleSided = false;
			if ( material.flags & LIFESTUDIOHEADAPI_MATERIAL_DOUBLESIDED)
				bDoubleSided = true;
			unordered_map< int, int > doubleVerts;

			int nMatIndex = indices.size();
			indices.resize( nMatIndex + nTris * (bDoubleSided ? 6 : 3) );
			const WORD *pTris = pGDPObject->Triangulation( nMat );
			for ( int j = 0; j < nTris * 3; ++j )
			{
				CVec2 uv;
				int nV = pTris[j];
				float u = pUVs[ nV * 2 ];
				float v = pUVs[ nV * 2 + 1 ] + 1;
				//if ( u < 0 || u > 1 )
				//	printf( "%d: U = %f --- ", nV, u );
				//if ( v < 0 || v > 1 )
				//	printf( "%d: V = %f --- ", nV, v );
				uv.u = textureMap.fUShift + textureMap.fSize * u;
				uv.v = 1 - textureMap.fVShift - textureMap.fSize * v;
				nV += nVert;
				if ( verticesToMaterials[nV] == -1 )
					verticesToMaterials[nV] = (nMaterial << 16) + nV;
				if ( (verticesToMaterials[nV] >> 16) != nMaterial )
				{
					int nNewV = UVs.size();
					UVs.resize( UVs.size() + 1 );
					copys.push_back( CTPoint<int>( nV, nNewV ) );
					//printf( "Vertex copy: %d -> %d\n", nV, nNewV );
					verticesToMaterials[nV] = (nMaterial << 16) + nNewV;
				}
				nV = verticesToMaterials[nV] & 0xFFFF;
				indices[ nMatIndex + j ] = nV;
				UVs[nV] = uv;
				doubleVerts[nV] = 0;
			}

			if ( bDoubleSided )
			{
				int nNewV = UVs.size();
				UVs.resize( UVs.size() + doubleVerts.size() );
				//printf( "Double vertices: %d\n", doubleVerts.size() );
				for ( unordered_map<int,int>::iterator it = doubleVerts.begin(); it != doubleVerts.end(); ++it, ++nNewV )
				{
					copys.push_back( CTPoint<int>( it->first, nNewV ) );
					it->second = nNewV;
					UVs[nNewV] = UVs[it->first];
				}
				for ( int j = 0; j < nTris * 3; ++j )
					indices[ nMatIndex + nTris * 3 + j ] = doubleVerts[ indices[ nMatIndex + j ] ];
			}

			// reverse order
			for ( int j = 0; j < nTris * 3; j += 3 )
			{
				WORD tmp = indices[ nMatIndex + j + 1 ];
				indices[ nMatIndex + j + 1 ] = indices[ nMatIndex + j + 2 ];
				indices[ nMatIndex + j + 2 ] = tmp;
			}

			if ( bObjFormat )
				continue;

			NImage::CImage texture;
			bool bSuccess;
			// get texture
			int nTexSize = pGDPObject->PNGTextureSize( material.textureName );
			if ( bSuccess = (nTexSize > 0) )
			{
				char *pBuf = new char[nTexSize];
				bSuccess = pGDPObject->PNGTexture( material.textureName, pBuf );
				if ( bSuccess )
				{
					CMemoryStream mem;
					mem.Write( pBuf, nTexSize );
					mem.Seek(0);
					delete [] pBuf;
					bSuccess = NImage::LoadImage( &texture, &mem );
				}
			}
			int nSizeNeeded = (int)(picture.GetXSize() * textureMap.fSize);
			if ( !bSuccess )
			{
				// create fake texture
				texture.SetSizes( nSizeNeeded, nSizeNeeded );
				for ( int y = 0; y < texture.GetYSize(); ++y )
				for ( int x = 0; x < texture.GetXSize(); ++x )
					texture[y][x] = CVec4( 1, 1, 1, 1 );
			}
/*
			sprintf( pszName, "%stemp%d.tga", pszTexPath, nCurMat );
			CFileStream pic;
			pic.OpenWrite( pszName );
			NImage::SaveImageAsTGA( &pic, texture );
			pic.CloseFile();
*/
			//printf( "Ambient %d: %f %f %f %f\n", nCurMat, material.ambient[0], material.ambient[1], material.ambient[2], material.ambient[3] );

			for ( int y = 0; y < texture.GetYSize(); ++y )
			for ( int x = 0; x < texture.GetXSize(); ++x )
			{
				CVec4 &pixel = texture[y][x];
				pixel.r *= material.ambient[0];
				pixel.g *= material.ambient[1];
				pixel.b *= material.ambient[2];
				pixel.a *= material.ambient[3];
			}
			if ( texture.GetXSize() != texture.GetYSize() || GetNextPow2( texture.GetXSize() ) != texture.GetXSize() )
				continue;
			while ( texture.GetXSize() > nSizeNeeded )
			{
				NImage::CImage tempPic;
				NImage::GenerateMipLevelPoint( &tempPic, texture );
				texture = tempPic;
			}
			if ( texture.GetXSize() != nSizeNeeded )
				continue;
			int nXFrom = (int)(picture.GetXSize() * textureMap.fUShift);
			int nYFrom = (int)(picture.GetXSize() * textureMap.fVShift);
			for ( int y = 0; y < texture.GetYSize(); ++y )
			for ( int x = 0; x < texture.GetXSize(); ++x )
				picture[y + nYFrom][x + nXFrom] = texture[y][x];
		}

		// additional normals data
		vector< vector<int> > groups;
		unordered_map< int, int > groupsMap;
		int nNormDataSize = pGDPObject->AdditionalNormalsDataSize();
		if ( nNormDataSize > 0 )
		{
			char *pBuf = new char[nNormDataSize];
			bool bRes = pGDPObject->AdditionalNormalsData( pBuf );
			if ( bRes )
			{
				char *p = pBuf + sizeof(int);
				int nGroups = *(int*)p;
				p += sizeof(int);
				groups.resize( nGroups );
				for ( int j = 0; j < nGroups; ++j )
				{
					int nGroupVerts = *(int*)p;
					p += sizeof(int);
					for ( int n = 0; n < nGroupVerts; ++n )
					{
						int nV = *(int*)p + nVert;
						p += sizeof(int);
						groups[j].push_back( nV );
						groupsMap[nV] = j;
					}
				}
			}
			delete [] pBuf;
		}

		for ( int i = nObjIndex; i < indices.size(); i += 3 )
		{
			int a = indices[ i ];
			int b = indices[ i + 1 ];
			int c = indices[ i + 2 ];
			normalsData.push_back( a );
			normalsData.push_back( b );
			normalsData.push_back( c );
			nNormalIndex += 3;
			if ( groupsMap.find(a) != groupsMap.end() )
			{
				vector<int> &group = groups[ groupsMap[a] ];
				for ( int j = 0; j < group.size(); ++j )
					if ( group[j] != a )
					{
						normalsData.push_back( group[j] );
						++nNormalIndex;
					}
			}
			if ( groupsMap.find(b) != groupsMap.end() )
			{
				vector<int> &group = groups[ groupsMap[b] ];
				for ( int j = 0; j < group.size(); ++j )
					if ( group[j] != b )
					{
						normalsData.push_back( group[j] );
						++nNormalIndex;
					}
			}
			if ( groupsMap.find(c) != groupsMap.end() )
			{
				vector<int> &group = groups[ groupsMap[c] ];
				for ( int j = 0; j < group.size(); ++j )
					if ( group[j] != c )
					{
						normalsData.push_back( group[j] );
						++nNormalIndex;
					}
			}
			normalsIndices.push_back( nNormalIndex );
		}

		nVert += nObjVerts;
		numberVertices[nObj] = nObjVerts;
	}

	for ( int nObj = objects.size() - 1; nObj >= 0; --nObj )
		objects[nObj]->Destroy();
	pGDPFile->Destroy();

	if ( bObjFormat )
		OutputInOBJFormat( resultData, pszOutput );
	else
	{
		sprintf( pszName, "%s0.tga", pszTexPath );
		CFileStream pic;
		pic.OpenWrite( pszName );
		NImage::SaveImageAsTGA( &pic, picture );
		pic.CloseFile();

		string szMatProps = "AddressMode=Wrap\nAlpha=alpha_test\n";
		string szTexProps = "AddrType=Wrap";
		sprintf( pszName, "%s0.mat", pszTexPath );
		CFileStream mat;
		mat.OpenWrite( pszName );
		mat.Write( szMatProps.c_str(), szMatProps.size() );
		mat.CloseFile();

		sprintf( pszName, "%s0.tex", pszTexPath );
		CFileStream tex;
		tex.OpenWrite( pszName );
		tex.Write( szTexProps.c_str(), szTexProps.size() );
		tex.CloseFile();

		OutputInGameFormat( resultData, pszOutput );
	}

	}
	catch(...)
	{
		printf( "File IO error.\n" );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void ConvertMMSFile( const char *pszInput, const char *pszOutput )
{
	try
	{

	CMemoryStream mem;
	CFileStream in;
	in.OpenRead( pszInput );
	int nSize = in.GetSize();
	in.ReadTo( mem, nSize );
	in.CloseFile();

	CFileStream out;
	out.OpenWrite( pszOutput );
	{
		CStructureSaver file( out, CStructureSaver::WRITE );
		file.Add( 1, &mem );
	}
	out.CloseFile();

	}
	catch(...)
	{
		printf( "File IO error.\n" );
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void ShowUsage()
{
	printf( "LifeStudio Files Convertor Utility\n" );
	printf( "(C) Nival Interactive, 2002\nUsage:\n" );

	printf( "  LSConverter -<mode> -t<texture> -d<size> <input> <output>\n" );
	printf( "    <mode>   : g(convert GDP file), s(convert MMS file),\n               i(GDP text info), o(GDP to OBJ)\n" );
	printf( "    <texture>: if <mode> == g, file name for texture output\n" );
	printf( "    <size>   : if <mode> == g, size of output texture\n" );
	printf( "    <input>  : if <mode> == g|i|o, input GDP file;\n               if <mode> == s, input MMS file\n" );
	printf( "    <output> : output file name\n" );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int __cdecl main( int argc, char *argv[] )
{
	if ( argc < 2 )
	{
		ShowUsage();
		return 0;
	}
	LifeStudioHeadAPI::Init();
	string szInputName, szOutputName, szTexName;
	int nTextureSize = 256;
	enum EMode
	{
		UNKNOWN,
		GDP,
		SEQ,
		INFO,
		OBJ,
	};
	EMode mode = UNKNOWN;
	for ( int i=1; i<argc; ++i )
	{
		if ( argv[i][0] == '-' )
		{
			if ( argv[i][1] == 'g' )
				mode = GDP;
			else if ( argv[i][1] == 's' )
				mode = SEQ;
			else if ( argv[i][1] == 'i' )
				mode = INFO;
			else if ( argv[i][1] == 'o' )
				mode = OBJ;
			else if ( argv[i][1] == 't' )
				szTexName = argv[i] + 2;
			else if ( argv[i][1] == 'd' )
				nTextureSize = atoi( argv[i] + 2 );
		}
		else if ( szInputName.empty() )
			szInputName = argv[i];
		else if ( szOutputName.empty() )
			szOutputName = argv[i];
		else
			printf( "UNKNOWN parameter \"%s\"\n", argv[i] );
	}
	//
	if ( mode == INFO )
	{
		if ( szInputName.empty() )
		{
			ShowUsage();
			return 0;
		}
	}
	else if ( szInputName.empty() || szOutputName.empty() || mode == UNKNOWN )
	{
		ShowUsage();
		return 0;
	}
	switch( mode )
	{
		case GDP:
		case OBJ:
			ConvertGDPFile( szInputName.c_str(), szOutputName.c_str(), szTexName.c_str(), nTextureSize, mode == OBJ );
			break;
		case SEQ:
			ConvertMMSFile( szInputName.c_str(), szOutputName.c_str() );
			break;
		case INFO:
			OutputGDPInfo( szInputName.c_str() );
			break;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

