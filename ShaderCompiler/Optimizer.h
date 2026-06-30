#ifndef __VSCOMPILER_OPTIMIZER_H_
#define __VSCOMPILER_OPTIMIZER_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

namespace NVSOptimize
{
	void OptimizeVertexShader( string *pRes, const string &szShader );
}

#endif