#include "StdAfx.h"
#include "MEUserSettings.h"
#include "MEParams.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
string GetMEParamName( int nParamID )
{
	switch ( nParamID )
	{
		case ME_SUBTEMPLATE_ALPHA:
			return "SubtemplateAlpha";
		case ME_SWITCH_SYSCOLORS:
			return "SwitchSysColors";
		case ME_OBJECTS_SHOWGROUND:
			return "ObjectsShowGround";
		case ME_SHOW_AISPHERES:
			return "ShowAISpheres";
		case ME_DEF_ANIM_MODEL:
			return "DefAnimModel";
		case ME_INSTANT_TERRAIN:
			return "InstantTerrainPreview";
		case ME_GRASS_DENSITY:
			return "GrassDensity";
		case ME_GRID_INTERVAL:
			return "GridInterval";
		case ME_LOCK_SELECTION:
			return "LockSelection";
		case ME_KBDMOVE_SPEED:
			return "KmdMoveSpeed";
		case ME_GRID_SNAP_2FLOOR:
			return "GridSnap";
		case ME_SEQUENCE_MODE:
			return "SequenceMode";
		case ME_ANIM_SHOWFLAGS:
			return "AnimShowFlags";
		case ME_TERRAIN_SHOWHOLES:
			return "ShowHoles";
		case ME_PERS_FACEGENCAMERA:
			return "FaceGenCamera";
		case ME_SCRIPT_SYNAXCOLORING:
			return "SyntaxColouring";
		case ME_SHOW_OBJ_BROWSER:
			return "ShowObjBr";
		case ME_SHOW_CUSTOMFRAME:
			return "ShowCustomFrame";
		case ME_CUSTOMFRAME_HEIGHT:
			return "CFHeight";
		case ME_CUSTOMFRAME_WIDTH:
			return "CFWidth";
	}
	return "";
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float GetMEParamDefValue( int nParamID )
{
	switch ( nParamID )
	{
		case ME_SUBTEMPLATE_ALPHA:
			return 0.2f;
		case ME_SWITCH_SYSCOLORS:
			return 0;
		case ME_OBJECTS_SHOWGROUND:
			return 0;
		case ME_SHOW_AISPHERES:
			return 1;
		case ME_DEF_ANIM_MODEL:
			return -1;
		case ME_INSTANT_TERRAIN:
			return 0;
		case ME_GRASS_DENSITY:
			return 0;
		case ME_GRID_INTERVAL:
			return 1;
		case ME_LOCK_SELECTION:
			return 0;
		case ME_KBDMOVE_SPEED:
			return 1;
		case ME_GRID_SNAP_2FLOOR:
			return false;
		case ME_SEQUENCE_MODE:
			return false;
		case ME_ANIM_SHOWFLAGS:
			return true;
		case ME_TERRAIN_SHOWHOLES:
			return false;
		case ME_PERS_FACEGENCAMERA:
			return false;
		case ME_SCRIPT_SYNAXCOLORING:
			return true;
		case ME_SHOW_OBJ_BROWSER:
			return false;
		case ME_SHOW_CUSTOMFRAME:
			return false;
		case ME_CUSTOMFRAME_WIDTH:
			return 100;
		case ME_CUSTOMFRAME_HEIGHT:
			return 100;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
