#include "stdafx.h"
#include <iostream>
//#include <string.h>
#include <fstream>
#include "Optimizer.h"
#include "..\FileIO\streams.h"
#include <d3dx9.h>

static char* GetNextLine( char *p, int *pnLines )
{
	while ( p[0] != 0 && p[0] != 10 && p[0] != 13 )
		++p;
	while ( p[0] == 10 || p[0] == 13 )
	{
		*pnLines += p[0] == 10;
		++p;
	}
	return p;
}
struct SLexem
{
	string szData;

	SLexem() {}
	SLexem( const char *pStart, const char *pFinish ): szData(pStart, pFinish) {}
};

static void ParseLine( vector<SLexem> *pRes, const char *p, const char *pFinish )
{
	pRes->resize( 0 );
	if ( p == pFinish )
		return;
	// check for comments
	if ( p[0] == ';' )
		return; 
	const char *pStart = p;
	for ( ; p < pFinish; ++p )
	{
		switch ( p[0] )
		{
			case '/':
				if ( p[1] == '/' )
				{
					if ( pStart != p )
						pRes->push_back( SLexem( pStart, p ) );
					return;
				}
				break;
			case 9:
			case 10:
			case 13:
			case ' ':
			case ',':
			case '(':
			case ')':
				if ( pStart != p )
					pRes->push_back( SLexem( pStart, p ) );
				pStart = p + 1;
				break;
		}
	}
}

struct SCommand
{
	string szCmd;
	vector<string> params;
};

struct SProcedure
{
	string szName;
	bool bExtern;
	vector<string> params;
	vector<string> locals;
	vector<SCommand> cmds;

	void Clear()
	{
		params.clear();
		locals.clear();
		cmds.clear();
	}
	int GetLocalVar( const char *p ) const
	{
		for ( int k = 0; k < locals.size(); ++k )
		{
			if ( locals[k] == p )
				return k;
		}
		return -1;
	}
	int GetParam( const char *p ) const
	{
		for ( int k = 0; k < params.size(); ++k )
		{
			if ( params[k] == p )
				return k;
		}
		return -1;
	}
};

const char* pszCmds[] = {
"add", "dp3", "dp4", "dst", "expp", "lit", "logp", "mad", "max", "min", "mov", "mul", "rcp", 
"rsq", "sge", "slt", "sub", "exp", "frc", "log", "m3x2", "m3x3", "m3x4", "m4x3", "m4x4", 0 };

const char* pszRegs[] = {
	"oD0", "oD1", "oT0", "oT1", "oT2", "oT3", "oT4", "oT5", "oT6", "oT7", "oFog", "oPos", "oPts", 0 };

static bool IsOneOf( const char **pList, const char *p )
{
	for ( ; pList[0]; ++pList )
	{
		if ( strcmp( pList[0], p ) == 0 )
			return true;
	}
	return false;
}

static bool IsVSRegister( const char *pTest, const char c )
{
	if ( toupper(c) != toupper( pTest[0] ) )
		return false;
	for ( ++pTest; pTest[0]; ++pTest )
	{
		if ( pTest[0] < '0' || pTest[0] > '9' )
			return false;
	}
	return true;
}

string StripAfterDot( const char *p, string *pSuffix = 0, string *pPrefix = 0 )
{
	if ( pSuffix )
		*pSuffix = "";
	if ( pPrefix )
		*pPrefix = "";
	string res( p );
	if ( res.size() > 0 && res[0] == '-' )
	{
		res = res.substr( 1 );
		if ( pPrefix )
			*pPrefix = "-";
	}
	int n = res.find( '.' );
	if ( n != string::npos )
	{
		if ( pSuffix )
			*pSuffix = res.substr( n );
		res = res.substr( 0, n );
	}
	return res;
}

string GetNumber( int n )
{
	char szBuf[8] = {0,0,0,0,0,0,0,0};
	itoa( n, szBuf, 10 );
	return szBuf;
}

struct SProgram
{
	vector<SProcedure> procs;

	int GetProc( const char *p ) const
	{
		for ( int k = 0; k < procs.size(); ++k )
		{
			if ( procs[k].szName == p )
				return k;
		}
		return -1;
	}
	bool IsValidCommand( const char *p ) const
	{
		if ( IsOneOf( pszCmds, p ) )
			return true;
		return GetProc( p ) != -1;
	}
};

class CProgramParser
{
	enum EState
	{
		PROCEDURE,
		_ERROR
	};
	SProgram &prog;
	SProcedure cur;
	EState state;
	string szError;

	bool IsValidVarName( const char *p )
	{
		string sz = StripAfterDot( p );
		const char *psz = sz.c_str();
		return IsOneOf( pszRegs, psz )  || IsVSRegister( psz, 'V' ) || IsVSRegister( psz, 'C' ) ||
			cur.GetLocalVar(psz) != -1 || cur.GetParam(psz) != -1;
	}
	void ShitHappened( const string &szInfo )
	{
		szError = szInfo;
		state = _ERROR;
	}
public:
	CProgramParser( SProgram &_p ): prog(_p), state(PROCEDURE) {}
	bool IsError() const { return state == _ERROR; }
	const string& GetError() const { return szError; }
	void Finish()
	{
		if ( !cur.cmds.empty() )
			prog.procs.push_back( cur );
		cur.Clear();
	}
	void AddLine( const char *pszLine, const char *pszLineEnd )
	{
		vector<SLexem> lexems;
		ParseLine( &lexems, pszLine, pszLineEnd );
		if ( lexems.empty() )
			return;
		switch ( state )
		{
			case PROCEDURE:
				if ( lexems[0].szData == "proc" || lexems[0].szData == "func" )
				{
					// if had program output it
					Finish();

					if ( lexems.size() < 2 )
					{
						ShitHappened( "proc name expected" );
						return;
					}
					cur.szName = lexems[1].szData;
					if ( prog.GetProc( cur.szName.c_str() ) != -1 )
					{
						ShitHappened( string("proc ") + cur.szName + " is already defined" );
						return;
					}
					cur.bExtern = lexems[0].szData == "proc";
					for ( int k = 2; k < lexems.size(); ++k )
						cur.params.push_back( lexems[k].szData );
					if ( cur.bExtern && !cur.params.empty() )
					{
						ShitHappened( string("proc ") + cur.szName + " should have no params, only funcs are allowed to have them" );
						return;
					}
				}
				else if ( lexems[0].szData == "locals" )
				{
					for ( int k = 1; k < lexems.size(); ++k )
						cur.locals.push_back( lexems[k].szData );
				}
				else
				{
					SCommand cmd;
					cmd.szCmd = lexems[0].szData;
					if ( !prog.IsValidCommand( cmd.szCmd.c_str() ) )
					{
						ShitHappened( string("unrecognised command ") + cmd.szCmd );
						return;
					}
					for ( int k = 1; k < lexems.size(); ++k )
					{
						if ( !IsValidVarName( lexems[k].szData.c_str() ) )
						{
							ShitHappened( string("invalid command parameter ") + lexems[k].szData );
							return;
						}
						cmd.params.push_back( lexems[k].szData );
					}
					int nCmd = prog.GetProc( cmd.szCmd.c_str() );
					if ( nCmd != -1 )
					{
						if ( prog.procs[nCmd].params.size() != cmd.params.size() )
						{
							ShitHappened( string("invalid number of params of cmd ") + cmd.szCmd );
							return;
						}
					}
					cur.cmds.push_back( cmd );
				}
				break;
		}
	}
};

struct CVSCompiler
{
	const SProgram &prog;
	struct SStkFrame
	{
		const SProcedure *pProc;
		int nIP, nBaseRegister;
		vector<string> args;

		SStkFrame( const SProcedure *_pProc, int _nBaseRegister ): pProc(_pProc), nBaseRegister(_nBaseRegister), nIP(0) {}
	};
public:
	CVSCompiler( const SProgram &_p ): prog(_p) {}
	void GenerateShader( string *pRes, const char *pszProcName )
	{
		string &res = *pRes;
		res = "vs.1.1\n";
		res += "dcl_position v0;\n";
		res += "dcl_normal v1;\n";
		res += "dcl_texcoord0 v3;\n";
		res += "dcl_tangent0 v4;\n";
		res += "dcl_tangent1 v5;\n";
		res += "dcl_texcoord1 v6;\n";
		int n = prog.GetProc( pszProcName );
		if ( n == -1 )
			return;
		list<SStkFrame> stk;
		stk.push_back( SStkFrame( &prog.procs[n], 0 ) );
		while ( !stk.empty() )
		{
			SStkFrame &f = stk.back();
			if ( f.nIP >= f.pProc->cmds.size() )
				stk.pop_back();
			else
			{
				const SCommand &cmd = f.pProc->cmds[f.nIP++];
				int nProc = prog.GetProc( cmd.szCmd.c_str() );
				if ( nProc != -1 )
				{
					SStkFrame nf( &prog.procs[nProc], f.nBaseRegister + f.pProc->locals.size() );
					nf.args = cmd.params;
					ASSERT( nf.args.size() == prog.procs[nProc].params.size() );
					for ( int k = 0; k < nf.args.size(); ++k )
					{
						string sz = StripAfterDot( nf.args[k].c_str() );
						const char *pszParam = sz.c_str();
						int nLocal = f.pProc->GetLocalVar( pszParam ), nParam = f.pProc->GetParam( pszParam );
						if ( nLocal != -1 )
							nf.args[k] = "r" + GetNumber( nLocal + f.nBaseRegister );
						if ( nParam != -1 )
							nf.args[k] = f.args[nParam];
					}
					stk.push_back( nf );
				}
				else
				{
					res += cmd.szCmd + " ";
					for ( int k = 0; k < cmd.params.size(); ++k )
					{
						string szParam, szSuffix, szPrefix;
						szParam = StripAfterDot( cmd.params[k].c_str(), &szSuffix, &szPrefix );
						const char *pszParam = szParam.c_str();
						int nLocal = f.pProc->GetLocalVar( pszParam ), nParam = f.pProc->GetParam( pszParam );
						if ( nLocal != -1 )
							res += szPrefix + "r" + GetNumber( nLocal + f.nBaseRegister ) + szSuffix;
						else if ( nParam != -1 )
							res += szPrefix + f.args[nParam] + szSuffix;
						else
							res += szPrefix + pszParam + szSuffix;
						if ( k != cmd.params.size() - 1 )
							res += ", ";
					}
					res += "\n";
				}
			}
		}
	}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SVShader
{
	string szName;
	string szShader;
};

struct SRS
{
	string sz1, sz2;
};

struct STSS
{
	string sz1, sz2, sz3;
};

struct SStates
{
	vector<SRS> rs;
	vector<STSS> tss;

	void Clear() { rs.clear(); tss.clear(); }
};

struct SPShader
{
	string szName;
	string szShader, szShader14;
	SStates states, shader;
};

static vector<SVShader> vertexShaders;
static vector<SPShader> pixelShaders;

string Filter( char *pszSrc )
{
	string szRes;
	for ( ; pszSrc[0]; ++pszSrc )
	{
		if ( isalnum( pszSrc[0] ) )
			szRes += pszSrc[0];
	}
	return szRes;
}

enum EState
{
	NONE,
	VSHADER,
	PSHADER,
	PSHADER14,
	PSHADER_RS,
	PSHADER_TSS
};

EState parseState;
string szError, szName;
SStates rs, tss;
int nLine;
char *pszShader, *pszShader14;

static void FinishState( char *pszFinish )
{
	switch ( parseState )
	{
		case NONE:
			break;
		case VSHADER:
/*			{
				SVShader v;
				v.szName = szName;
				v.szShader = pszShader;
				vertexShaders.push_back( v );
			}*/
			break;
		case PSHADER:
		case PSHADER14:
		case PSHADER_RS:
		case PSHADER_TSS:
			{
				SPShader p;
				p.szName = szName;
				p.szShader = pszShader;
				if ( pszShader14 )
					p.szShader14 = pszShader14;
				p.states = rs;
				p.shader = tss;
				pixelShaders.push_back( p );
			}
			break;
		default:
			ASSERT( 0 );
			break;
	}
}

static void SplitString( const char *pszData, vector<string> *pRes )
{
	pRes->clear();
	const char *pszStart = pszData;;
	for(;;)
	{
		while ( pszStart[0] != 0 && isspace( pszStart[0] ) )
			++pszStart;
		string sz;
		while ( !isspace( pszStart[0] ) && pszStart[0] != 0 )
			sz += *pszStart++;
		if ( !sz.empty() )
			pRes->push_back( sz );
		if ( pszStart[0] == 0 )
			break;
	}
}
static void AddRS( SStates *pRes, const char *pszLine )
{
	vector<string> res;
	SplitString( pszLine, &res );
	if ( res.size() == 2 )
	{
		SRS r;
		r.sz1 = res[0];
		r.sz2 = res[1];
		pRes->rs.push_back( r );
	}
	else if ( res.size() == 3 )
	{
		STSS r;
		r.sz1 = res[0];
		r.sz2 = res[1];
		r.sz3 = res[2];
		pRes->tss.push_back( r );
	}
	else
	{
		cout << "ignoring state in line " << nLine << endl;
	}
}

static void ParseFile( char *pszFile )
{
	char *pszParse = pszFile, *pNextLine;
	int nNextLine;
	nLine = 1;
	parseState = NONE;
	SProgram prog; 
	CProgramParser pparser( prog );
	for ( ; pszParse[0]; pszParse = pNextLine, nLine = nNextLine )
	{
		nNextLine = nLine;
		pNextLine = GetNextLine( pszParse, &nNextLine );
		if ( strlen( pszParse ) > 5 )
		{
			if ( strncmp( pszParse, "[VS]", 4 ) == 0 )
			{
				pszParse[0] = 0;
				FinishState( pszParse );
				pNextLine[-1] = 0;
				//szName = Filter( pszParse + 4 );
				//pszShader = pNextLine;
				parseState = VSHADER;
				continue;
			}
			if ( strncmp( pszParse, "[PS]", 4 ) == 0 )
			{
				pszParse[0] = 0;
				FinishState( pszParse );
				rs.Clear();
				tss.Clear();
				pNextLine[-1] = 0;
				szName = Filter( pszParse + 4 );
				pszShader = pNextLine;
				pszShader14 = 0;
				parseState = PSHADER;
				continue;
			}
			if ( strncmp( pszParse, "[PS14]", 6 ) == 0 )
			{
				pszParse[0] = 0;
				if ( parseState != PSHADER )
				{

					szError = "[PS14] should follow [PS]";
					return;
				}
				pNextLine[-1] = 0;
				pszShader14 = pNextLine;
				parseState = PSHADER14;
				continue;
			}
			if ( strncmp( pszParse, "[RS]", 4 ) == 0 )
			{
				pszParse[0] = 0;
				if ( parseState != PSHADER && parseState != PSHADER14 )
				{
					szError = "[RS] should follow [PS] or [PS14]";
					return;
				}
				parseState = PSHADER_RS;
				continue;
			}
			if ( strncmp( pszParse, "[TSS]", 5 ) == 0 )
			{
				pszParse[0] = 0;
				if ( parseState != PSHADER && parseState != PSHADER_RS )
				{
					szError = "[TSS] should follow either [RS] or [PS]";
					return;
				}
				parseState = PSHADER_TSS;
				continue;
			}
		}
		if ( pszParse[0] == ';' )
			continue;
		switch ( parseState )
		{
			case NONE:
				break;
			case VSHADER:
				pparser.AddLine( pszParse, pNextLine );
				if ( pparser.IsError() )
				{
					szError = pparser.GetError();
					return;
				}
				break;
			case PSHADER:
				break;
			case PSHADER_RS:
				if ( pNextLine[0] )
					pNextLine[-1] = 0;
				AddRS( &rs, pszParse );
				break;
			case PSHADER_TSS:
				if ( pNextLine[0] )
					pNextLine[-1] = 0;
				AddRS( &tss, pszParse );
				break;
		}
	}
	FinishState( pszParse );
	pparser.Finish();
	// generate vertex programs
	for ( int k = 0; k < prog.procs.size(); ++k )
	{
		if ( prog.procs[k].bExtern )
		{
			SVShader v;
			v.szName = prog.procs[k].szName;
			v.szShader = pszShader;
			CVSCompiler vsc( prog );
			vsc.GenerateShader( &v.szShader, prog.procs[k].szName.c_str() );
			// has no effect on nVidia & ATi; looks like drivers already do this
			//NVSOptimize::OptimizeVertexShader( &v.szShader, v.szShader );
			vertexShaders.push_back( v );
			OutputDebugString( v.szName.c_str() );
			OutputDebugString( "\n" );
			OutputDebugString( v.szShader.c_str() );
		}
	}
}

void WriteRS( ofstream &f, const SStates &s, const char *pszName, const char *pszPrefix )
{
	f << "static SRenderState rs" << pszPrefix << pszName << "[] = { " << endl;
	for ( int i = 0; i < s.rs.size(); ++i )
		f << "{ " << s.rs[i].sz1 << ", " << s.rs[i].sz2 << "}, " << endl;
	f << "{(D3DRENDERSTATETYPE)0,0} };" << endl;
	f << "static STextureStageState tss" << pszPrefix << pszName << "[] = { " << endl;
	for ( int i = 0; i < s.tss.size(); ++i )
		f << "{ " << s.tss[i].sz1 << ", " << s.tss[i].sz2 << ", " << s.tss[i].sz3 << "}, " << endl;
	f << "{-1,(D3DTEXTURESTAGESTATETYPE)0,0} };" << endl;
}

static void WriteShader( ofstream &f, DWORD *pRes )
{
	f << "{ 0x";
	f << hex;
	int i;
	for ( i = 0; (pRes[i] & 0xffff ) != 0xffff; ++i )
		f << pRes[i] << ", 0x";
	f << pRes[i] << " };" << dec << endl;
}

LPD3DXBUFFER CompileShader( const string &s, const string &name, const char *pszType )
{
	HRESULT hr;
	LPD3DXBUFFER pCode, pError;
	if ( s == "" )
		return 0;
	hr = D3DXAssembleShader( s.c_str(), s.length(), 0, 0, D3DXSHADER_DEBUG, &pCode, &pError );
	const char *pszError = pError ? (const char*)pError->GetBufferPointer() : 0;
	if ( hr != D3D_OK && pError )
	{
		cout << pszType << " shader " << name << " has an error " << (const char*)pError->GetBufferPointer() << endl;
		if ( pCode )
			pCode->Release();
		pError->Release();
		return 0;
	}
	else
		ASSERT( hr == D3D_OK );
	if ( pError )
		pError->Release();
	return pCode;
}

void WriteResult( const char *pszOutput )
{
	// write .h file
	{
		ofstream fh( (string(pszOutput) + ".h").c_str() );
		fh << "#ifndef __" << pszOutput << "_H__" << endl;
		fh << "#define __" << pszOutput << "_H__" << endl;
		//fh << "#include \"GfxShadersDescr.h\"" << endl;
		//fh << "namespace NGfx" << endl;
		//fh << "{" << endl;
		fh << "struct SVShader;" << endl;
		fh << "struct SPShader;" << endl;
		for ( int k = 0; k < vertexShaders.size(); ++k )
		{
			fh << "extern SVShader vs" << vertexShaders[k].szName << ";" << endl;
		}
		fh << endl << "extern SVShader *vsAllShaders[" << vertexShaders.size() << "];" << endl;
		fh << endl;
		for ( int k = 0; k < pixelShaders.size(); ++k )
		{
			fh << "extern SPShader ps" << pixelShaders[k].szName << ";" << endl;
		}
		fh << endl << "extern SPShader *psAllShaders[" << pixelShaders.size() << "];" << endl;
		//fh << "}" << endl;
		fh << "#endif" << endl;
	}
	// write .cpp file
	{
		ofstream f( (string(pszOutput) + ".cpp").c_str() );
		f << "#include \"StdAfx.h\"" << endl;
		f << "#include \"GfxShadersDescr.h\"" << endl;
		f << "#include \"" << pszOutput << ".h\"" << endl;
		//f << "namespace NGfx" << endl;
		//f << "{" << endl;
		for ( int k = 0; k < vertexShaders.size(); ++k )
		{
			SVShader &v = vertexShaders[k];
			LPD3DXBUFFER pCode = CompileShader( v.szShader, v.szName, "vertex" );
			if ( pCode )
			{
				DWORD *pRes = (DWORD*)pCode->GetBufferPointer();
				f << "static DWORD dwvs" << v.szName << "[] =";
				WriteShader( f, pRes );
				f << "SVShader vs" << v.szName << "( " << k + 1 << ", dwvs" << v.szName << ");" << endl;
			}
			if ( pCode )
				pCode->Release();
		}
		for ( int k = 0; k < pixelShaders.size(); ++k )
		{
			SPShader &v = pixelShaders[k];
			LPD3DXBUFFER pCode = CompileShader( v.szShader, v.szName, "pixel" );
			LPD3DXBUFFER pCode14 = CompileShader( v.szShader14, v.szName, "pixel14" );
			if ( pCode && ((DWORD*)pCode->GetBufferPointer())[0] == 0xffff0104 )
			{
				// 1.4 shader under [ps] section
				if ( pCode14 )
				{
					cout << "WARNING, pixel shader " << v.szName << ", using [PS] shader as [PS14] one" << endl;
					pCode14->Release();
				}
				pCode14 = pCode;
				pCode = 0;
			}
			if ( pCode14 == 0 )
				pCode14 = CompileShader( v.szShader, v.szName, "pixel" );
			if ( pCode || pCode14 )
			{
				if ( pCode )
				{
					DWORD *pRes = (DWORD*)pCode->GetBufferPointer();
					f << "static DWORD dwps" << v.szName << "[] = ";
					WriteShader( f, pRes );
				}
				if ( pCode14 )
				{
					DWORD *pRes = (DWORD*)pCode14->GetBufferPointer();
					f << "static DWORD dwps14" << v.szName << "[] = ";
					WriteShader( f, pRes );
				}
				WriteRS( f, v.shader, v.szName.c_str(), "sha" );
				WriteRS( f, v.states, v.szName.c_str(), "state" );
				f << "SPShader ps" << v.szName << "( " << k + 1;
				if ( pCode )
					f << ", dwps" << v.szName << ", ";
				else
					f << "0, ";
				if ( pCode14 )
					f << "dwps14" << v.szName << ", ";
				else
					f << "0, ";
				f << "rssha" << v.szName << ", " << "tsssha" << v.szName << ", ";
				f << "rsstate" << v.szName << ", " << "tssstate" << v.szName << " );" << endl;
			}
			if ( pCode )
				pCode->Release();
			if ( pCode14 )
				pCode14->Release();
		}

		f << endl << "SVShader *vsAllShaders[" << vertexShaders.size() << "] = { ";
		for ( int k = 0; k < vertexShaders.size(); ++k )
		{
			f << "&vs" << vertexShaders[k].szName;
			if ( k != vertexShaders.size() - 1 )
				f << ", ";
		}
		f << " };" << endl;

		f << endl << "SPShader *psAllShaders[" << pixelShaders.size() << "] = { ";
		for ( int k = 0; k < pixelShaders.size(); ++k )
		{
			f << "&ps" << pixelShaders[k].szName;
			if ( k != pixelShaders.size() - 1 )
				f << ", ";
		}
		f << " };" << endl;
		//f << "}" << endl;
	}
}

void __cdecl main( int argc, char* argv[] )
{
	if ( argc != 3 )
	{
		cout << "Usage: ShaderCompiler srcFile dstFile\n";
		return;
	}
	cout << "Compiling " << argv[1] << " into " << argv[2] << endl;
	CMemoryStream m;
	try
	{
		CFileStream s;
		s.OpenRead( argv[1] );
		m.WriteFrom( s );
		char cZero = 0;
		m.Write( &cZero, 1 );
		ParseFile( (char*) m.GetBufferForWrite() );
		if ( szError != "" )
		{
			cout << argv[1] << "(" << nLine << ") " << szError << endl;
		}
		else
		{
			WriteResult( argv[2] );
		}
	}
	catch(...)
	{
		cout << "failed" << endl;
	}
}


