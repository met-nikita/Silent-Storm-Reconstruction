#pragma once

namespace NWorld
{
class CUnitServer;
void CheckTooMuchCorpses( const CVec3 &ptMin, const CVec3 &ptMax,
	const list< CObj<CUnitServer> > &units );
}
