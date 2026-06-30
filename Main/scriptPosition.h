#ifndef __SCRIPTPOSITION_H_
#define __SCRIPTPOSITION_H_
//
namespace NScript
{
////////////////////////////////////////////////////////////////////////////////////////////////////
DECLARE_SCRIPT_COMMAND( GetPos );
DECLARE_SCRIPT_COMMAND( GetWaypointPos );
DECLARE_SCRIPT_COMMAND( GetDistance );
////////////////////////////////////////////////////////////////////////////////////////////////////
class CLUAObjectPosition: public CObjectBase
{
	OBJECT_BASIC_METHODS( CLUAObjectPosition )
	ZDATA
public:
	CVec3 ptPos;
	ZEND int operator&( CStructureSaver &f ) { f.Add(2,&ptPos); return 0; }
	//
	CLUAObjectPosition( CVec3 _ptPos = VNULL3 ): ptPos( _ptPos ) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
//
#endif __SCRIPTPOSITION_H_