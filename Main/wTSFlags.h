#ifndef __wTSFlags_H_
#define __wTSFlags_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
namespace NWorld
{
////////////////////////////////////////////////////////////////////////////////////////////////////
enum ETraceSet
{
	//TS_OBJECTS  = 1,
	TS_UNITS    = 2,
	TS_TERRAINS = 4,
	TS_VIRTUAL = 8,        // some flag just not to be zero
	//TS_GO_OVER  = 8,
	//TS_OPTIMIZED = 0x10,
	TS_FRAGMENTED = 0x10,  // takes damage
	TS_VISION = 0x20,
	TS_PICK = 0x80, // traceable objects
	TS_PASS_BLOCKER = 0x100,
	TS_COVER = 0x200,
	TS_WEAPON_BLOCKER = 0x400,
	//TS_LADDER_PART = 0x800,
	TS_STATE_OPEN = 0x1000,
	TS_STATE_CLOSED = 0x2000,
	TS_DOOR_HULL_VALID = 0x4000,
	TS_ITEM_BLOCKER = 0x8000,
	TS_VISION_SOLID = 0x10000,
	TS_ALL = (0xffffffff & ~TS_FRAGMENTED & ~TS_ITEM_BLOCKER & ~TS_VIRTUAL), // все и желательно по одному разу
};
////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif
