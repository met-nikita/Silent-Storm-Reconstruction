#include "stdafx.h"

namespace NVSOptimize
{
struct SArgument
{
	string szName;
	string szPrefix, szSuffix;

	bool IsEmpty() const { return szName == ""; }
	bool HasSame( const SArgument &arg ) const 
	{
		if ( szName != arg.szName )
			return false;
		for ( int k = 0; k < arg.szSuffix.size(); ++k )
		{
			if ( szSuffix.find( arg.szSuffix[k] ) != string::npos )
				return true;
		}
		return false;
	}
};
struct SVSCmd
{
	string szOp;
	SArgument res, arg1, arg2, arg3;

	bool IsReading( const SArgument &szReg ) const { if ( szReg.IsEmpty() ) return false;  return arg1.HasSame( szReg ) || arg2.HasSame( szReg ) || arg3.HasSame( szReg ); }
	bool IsWriting( const SArgument &szReg ) const { if ( szReg.IsEmpty() ) return false; return res.HasSame( szReg ); }
	bool IsTouching( const SArgument &szReg ) const { return IsReading( szReg ) || IsWriting( szReg ); }
};

struct SProgram
{
	vector<SVSCmd> cmds;
};

struct SCmdDependInfo
{
	vector<int> dependOn;

	/*bool IsDependent( int n ) const
	{ 
		for ( int k = 0; k < dependOn.size(); ++k ) if ( dependOn[k] == n ) return true; 
		return false; 
	}*/
};
struct SProgramDependInfo
{
	vector<SCmdDependInfo> deps;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// dependencies tracker
////////////////////////////////////////////////////////////////////////////////////////////////////
// return true if nCmd depends somehow on nCheck
static bool IsDependentTransitionally( const SProgramDependInfo &dep, int nCmd, int nCheck )
{
	const SCmdDependInfo &cmd = dep.deps[nCmd];
	if ( nCmd == nCheck )
		return true;
	for ( int k = 0; k < cmd.dependOn.size(); ++k )
	{
		if ( IsDependentTransitionally( dep, cmd.dependOn[k], nCheck ) )
			return true;
	}
	return false;
}

static void AddWriteCmd( const SProgram &src, int nLastCmd, const SArgument &szReg, vector<int> *pRes )
{
	if ( szReg.IsEmpty() )
		return;
	for ( int k = nLastCmd - 1; k >= 0; --k )
	{
		if ( find( pRes->begin(), pRes->end(), k ) != pRes->end() )
			continue;
		if ( src.cmds[k].IsWriting( szReg ) )
			pRes->push_back( k );
	}
}

// detect RW, WR, WW dependencies
static void BuildDependencies( SProgramDependInfo *pRes, const SProgram &src )
{
	pRes->deps.resize( src.cmds.size() );
	for ( int k = 0; k < src.cmds.size(); ++k )
	{
		const SVSCmd &srcCmd = src.cmds[k];
		SCmdDependInfo &cmd = pRes->deps[k];
		// find WR && WW dependencies
		for ( int i = k - 1; i >= 0; --i )
		{
			if ( src.cmds[i].IsTouching( srcCmd.res ) )
				cmd.dependOn.push_back( i );
		}
		// find RW dependencies
		AddWriteCmd( src, k, srcCmd.arg1, &cmd.dependOn );
		AddWriteCmd( src, k, srcCmd.arg2, &cmd.dependOn );
		AddWriteCmd( src, k, srcCmd.arg3, &cmd.dependOn );
		// eliminate Transitional dependencies
		for ( int k = 0; k < cmd.dependOn.size(); ++k )
		{
			int nTest = cmd.dependOn[k];
			for ( int i = 0; i < cmd.dependOn.size(); ++i )
			{
				if ( i != k && IsDependentTransitionally( *pRes, cmd.dependOn[i], nTest ) )
				{
					// can erase nTest since cmd.dependOn[i]-th command depends on its results
					cmd.dependOn.erase( cmd.dependOn.begin() + k );
					--k;
					break;
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// optimizer
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool IsRWDependent( const SVSCmd &first, const SVSCmd &next )
{
	return first.IsWriting( next.arg1 ) || first.IsWriting( next.arg2 ) || first.IsWriting( next.arg3 );
}

static int CountClocks( const SProgram &src )
{
	int nClocks = src.cmds.size();
	for ( int k = 1; k < src.cmds.size(); ++k )
		nClocks += IsRWDependent( src.cmds[k-1], src.cmds[k] );
	return nClocks;
}

static void RankTouch( vector<int> *pRanks, const SProgramDependInfo &dep, int nCmd, int nDepth )
{
	if ( (*pRanks)[nCmd] >= nDepth )
		return;
	(*pRanks)[nCmd] = nDepth;
	const SCmdDependInfo &d = dep.deps[ nCmd ];
	for ( int i = 0; i < d.dependOn.size(); ++i )
		RankTouch( pRanks, dep, d.dependOn[i], nDepth + 1 );
}

static void CalcInstructionRanks( vector<int> *pRanks, const SProgramDependInfo &dep, const SProgram &src )
{
	pRanks->resize( src.cmds.size(), 0 );
	for ( int k = 0; k < src.cmds.size(); ++k )
		RankTouch( pRanks, dep, k, 1 );
}

struct SProgramState
{
	vector<int> issued;

	void Init( int nCmds ) { issued.resize(0); issued.resize( nCmds, 0 ); }
	bool WasIssued( int n ) const { return issued[n]; }
	void Issue( int n ) { issued[n] = 1; }
	bool CanIssue( const SProgramDependInfo &dep, int nCmd )
	{
		const SCmdDependInfo &d = dep.deps[ nCmd ];
		for ( int k = 0; k < d.dependOn.size(); ++k )
		{
			if ( !WasIssued( d.dependOn[k] ) )
				return false;
		}
		return true;
	}
};
static void OutputOptimizedSequence( SProgram *pRes, const SProgramDependInfo &dep, const SProgram &src )
{
	vector<int> ranks;
	CalcInstructionRanks( &ranks, dep, src );
	vector<int> newOrder;
	// determine new order, select instructions one by one
	SProgramState state;
	state.Init( src.cmds.size() );
	SVSCmd prevCmd;
	for ( int k = 0; k < src.cmds.size(); ++k )
	{
		int nBestRank = -1, nBestIdx = 0;
		for ( int i = 0; i < src.cmds.size(); ++i )
		{
			if ( state.WasIssued( i ) )
				continue;
			if ( state.CanIssue( dep, i ) && ranks[i] > nBestRank && !IsRWDependent( prevCmd, src.cmds[i] ) )
			{
				nBestRank = ranks[i];
				nBestIdx = i;
			}
		}
		if ( nBestRank == -1 )
		{
			// have to issue dependent instruction
			for ( int i = 0; i < src.cmds.size(); ++i )
			{
				if ( state.WasIssued( i ) )
					continue;
				if ( state.CanIssue( dep, i ) && ranks[i] > nBestRank )
				{
					nBestRank = ranks[i];
					nBestIdx = i;
				}
			}
			ASSERT( nBestRank != -1 ); // dependencies are wrong
		}
		state.Issue( nBestIdx );
		newOrder.push_back( nBestIdx );
		prevCmd = src.cmds[nBestIdx];
	}
	// output result
	pRes->cmds.clear();
	for ( int k = 0; k < newOrder.size(); ++k )
		pRes->cmds.push_back( src.cmds[ newOrder[k] ]  );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// parser
////////////////////////////////////////////////////////////////////////////////////////////////////
static void SkipSpaces( const char **pszText )
{
	for( ;; ++(*pszText) )
	{
		char c = **pszText;
		if ( c != ' ' && c != 9 )
			break;
	}
}

static string GetWord( const char **pszText )
{
	string szRes;
	for(;; ++(*pszText) )
	{
		char c = **pszText;
		if ( !isalnum(c) )
			break;
		szRes += c;
	}
	return szRes;
}

static SArgument GetArgument( const char **pszText )
{
	SArgument res;
	if ( **pszText == '-' )
	{
		res.szPrefix = '-';
		++(*pszText);
	}
	SkipSpaces( pszText );
	res.szName = GetWord( pszText );
	SkipSpaces( pszText );
	if ( **pszText == '.' )
	{
		++(*pszText);
		SkipSpaces( pszText );
		res.szSuffix = GetWord( pszText );
	}
	else
		res.szSuffix = "xyzw";
	return res;
}

static void FixMask( SArgument *p, int n )
{
	if ( p->szName.empty() )
		p->szSuffix = "";
	else
	{
		ASSERT( !p->szSuffix.empty() );
		if ( p->szSuffix.size() > n )
			p->szSuffix.resize( n );
		else
		{
			while ( p->szSuffix.size() < n )
				p->szSuffix += p->szSuffix[p->szSuffix.size() - 1];
		}
	}
}

static void AddLine( SProgram *pRes, const string &szLine )
{
	if ( szLine == "" )
		return;
	if ( strncmp( szLine.c_str(), "vs.", 3 ) == 0 )
		return;
	const char *pszLine = szLine.c_str();
	SkipSpaces( &pszLine );
	string szCmd = GetWord( &pszLine );
	if ( szCmd == "" )
		return; // not a command
	SkipSpaces( &pszLine );
	SArgument target = GetArgument( &pszLine ), arg1, arg2, arg3;
	SkipSpaces( &pszLine );
	if ( *pszLine == ',' )
	{
		++pszLine;
		SkipSpaces( &pszLine );
		arg1 = GetArgument( &pszLine );
		SkipSpaces( &pszLine );
		if ( *pszLine == ',' )
		{
			++pszLine;
			SkipSpaces( &pszLine );
			arg2 = GetArgument( &pszLine );
			SkipSpaces( &pszLine );
			if ( *pszLine == ',' )
			{
				++pszLine;
				SkipSpaces( &pszLine );
				arg3 = GetArgument( &pszLine );
			}
		}
	}
	// skip not a command with result!
	if ( target.IsEmpty() )
	{
		ASSERT(0);
		return;
	}
	// fix number of components for all arguments
	int nComponents = target.szSuffix.size();
	if ( szCmd == "dp3" || szCmd == "dp4" || szCmd == "add" || szCmd == "mad" || szCmd == "max"
		|| szCmd == "min" || szCmd == "mov" || szCmd == "mul" || szCmd == "sge" || szCmd == "slt"
		|| szCmd == "sub" || szCmd == "m4x4" )
	{
		if ( szCmd == "dp3" )
			nComponents = 3;
		if ( szCmd == "dp4" )
			nComponents = 4;
		if ( szCmd == "m4x4" )
			nComponents = 4;
		FixMask( &arg1, nComponents );
		FixMask( &arg2, nComponents );
		FixMask( &arg3, nComponents );
	}
	if ( szCmd == "m4x4" )
	{
		ASSERT( arg3.szName == "" );
		// special case - split into individual dp4`s
		ASSERT( target.szSuffix.size() == 4 );
		int nReg = atoi( arg2.szName.c_str() + 1 );
		for ( int k = 0; k < target.szSuffix.size(); ++k )
		{
			SVSCmd &resCmd = *pRes->cmds.insert( pRes->cmds.end(), SVSCmd());
			resCmd.szOp = "dp4";
			resCmd.res = target;
			resCmd.arg1 = arg1;
			resCmd.arg2 = arg2;
			resCmd.arg3 = arg3;
			char szRegister[10];
			memset( szRegister, 0, sizeof(szRegister) );
			szRegister[0] = arg2.szName[0];
			_itoa( nReg + k, szRegister + 1, 10 );
			resCmd.arg2.szName = szRegister;
			resCmd.res.szSuffix = target.szSuffix.substr( k, 1 );
		}
		return;
	}
	SVSCmd &resCmd = *pRes->cmds.insert( pRes->cmds.end(), SVSCmd());
	resCmd.szOp = szCmd;
	resCmd.res = target;
	resCmd.arg1 = arg1;
	resCmd.arg2 = arg2;
	resCmd.arg3 = arg3;
}

static void BuildProgram( SProgram *pRes, const string &src )
{
	pRes->cmds.clear();
	const char *pszText = src.c_str();
	string szLine;
	for ( ; pszText[0]; ++pszText )
	{
		if ( pszText[0] == '\n' )
		{
			AddLine( pRes, szLine );
			szLine = "";
		}
		else
			szLine += *pszText;
	}
	AddLine( pRes, szLine );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// code gen
////////////////////////////////////////////////////////////////////////////////////////////////////
static void GenerateShaderText( string *pRes, const SProgram &src )
{
	*pRes = "vs.1.0\n";
	for ( int k = 0; k < src.cmds.size(); ++k )
	{
		const SVSCmd &cmd = src.cmds[k];
		*pRes += cmd.szOp + " " + cmd.res.szPrefix + cmd.res.szName + "." + cmd.res.szSuffix;
		if ( !cmd.arg1.IsEmpty() )
			*pRes += string(",") + cmd.arg1.szPrefix + cmd.arg1.szName + "." + cmd.arg1.szSuffix;
		if ( !cmd.arg2.IsEmpty() )
			*pRes += string(",") + cmd.arg2.szPrefix + cmd.arg2.szName + "." + cmd.arg2.szSuffix;
		if ( !cmd.arg3.IsEmpty() )
			*pRes += string(",") + cmd.arg3.szPrefix + cmd.arg3.szName + "." + cmd.arg3.szSuffix;
		*pRes += "\n";
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// main function
////////////////////////////////////////////////////////////////////////////////////////////////////
void OptimizeVertexShader( string *pRes, const string &szShader )
{
	int nBefore, nAfter; 
	SProgram program, res;
	SProgramDependInfo dep;
	BuildProgram( &program, szShader );
	nBefore = CountClocks( program );
	BuildDependencies( &dep, program );
	OutputOptimizedSequence( &res, dep, program );
	nAfter = CountClocks( res );
	char szBuf[1024];
	sprintf( szBuf, "%.3g%% faster after optimize\n", 100 - nAfter * 100.0f / nBefore );
	OutputDebugString( szBuf );
	GenerateShaderText( pRes, res );
}
}