#ifndef __DBDEFS_H__
#define __DBDEFS_H__

const int MAX_STRING_LEN = 49;
const int MAX_DBSTRING = 252;	// должно быть четное число (для хранения координат или индексов)
//
const int IDC_TEMPLATE_TREE   = 900;
const int IDC_MODELS_TREE     = 901;
const int IDC_GEOMETRIES_TREE = 902;
const int IDC_MATERIALS_TREE  = 903;
const int IDC_TEXTURES_TREE   = 904;
const int IDC_BRDF_TREE       = 905;
const int IDC_WALLS_TREE      = 906;
const int IDC_SKELETONS_TREE  = 907;
const int IDC_ANIMATIONS_TREE = 908;
const int IDC_FONTS_TREE      = 909;
const int IDC_RPG_CLASSES_TREE       = 910;
const int IDC_RPG_MONSTERS_TREE      = 911;
const int IDC_RPG_MONSTERSTATS_TREE  = 912;
const int IDC_RPG_PERS_TREE          = 913;
const int IDC_RPG_WEAPONS_TREE       = 914;
const int IDC_RPG_WEAPONTYPES_TREE   = 915;
const int IDC_AIGEOMETRIES_TREE      = 916;
const int IDC_FLOORS_TREE            = 917;
const int IDC_PARTICLES_TREE         = 918;
const int IDC_SOLIDMODELS_TREE       = 919;
const int IDC_RPG_BASEVALUES         = 920;
const int IDC_TERRAINTILES_TREE      = 921;
const int IDC_AMBIENTLIGHTS_TREE     = 922;
const int IDC_RPG_ARMORS_TREE        = 923;
const int IDC_CONTAINERS_TREE        = 924;
const int IDC_SOUNDS_TREE            = 925;
const int IDC_DEBRIS_TREE            = 926;
const int IDC_DEBRISMATERIALS_TREE   = 927;
const int IDC_OBJECTS_TREE           = 928;
const int IDC_EXPLOSIONS_TREE        = 929;
const int IDC_INTERFACES_TREE        = 930;
const int IDC_STRINGS_TREE           = 931;
const int IDC_CONSTRUCTIONPARTS_TREE = 932;
const int IDC_GRASS_TREE             = 933;
const int IDC_UITEXTURES_TREE        = 934;
const int IDC_RPG_ITEMS_TREE         = 935;
const int IDC_RPG_CRITICALS_TREE     = 936;
const int IDC_CUBEMAPS_TREE          = 937;
const int IDC_CHAPTERS_TREE          = 938;
const int IDC_RNDSOUNDS_TREE         = 939;
const int IDC_RPG_GRENADES_TREE      = 940;
const int IDC_WAYPOINTS_TREE         = 941;
const int IDC_DOORS_TREE             = 942;
const int IDC_GUNS_TREE              = 943;
const int IDC_WAYPOINTNAMES_TREE     = 944;
const int IDC_PARTICLEINSTANCES_TREE = 945;
const int IDC_EFFECTS_TREE           = 946;
const int IDC_NATIONALITY_TREE       = 947;
const int IDC_HEADS_TREE             = 948;
const int IDC_HEADSEQS_TREE          = 949;
const int IDC_ANIMWEAPONTYPES_TREE   = 950;
const int IDC_ACKINFOS               = 951;
const int IDC_LIGHTINSTANCES_TREE    = 952;
const int IDC_LIGHTS_TREE            = 953;
const int IDC_MUSIC_TREE             = 954;
const int IDC_COMPLEXHEADS           = 955;
const int IDC_RPGWEAPONTYPES_TREE    = 956;
const int IDC_RPGAMMOTYPES_TREE      = 957;
const int IDC_PLACABLE_TREE          = 958;
const int IDC_SCRIPTS_TREE           = 959;
const int IDC_UNIFORMS_TREE          = 960;
const int IDC_RPG_CLIPS_TREE         = 961;
const int IDC_RPG_FIRSTAID_TREE      = 962;
const int IDC_RPG_MINEDETECTORS_TREE = 963;
const int IDC_RPG_MELEEWEAPONS_TREE  = 964;
const int IDC_SPOTS_TREE             = 965;
const int IDC_SCENARIOS_TREE         = 966;
const int IDC_PLAYERS_TREE           = 967;
const int IDC_GLOBALMAPS_TREE        = 968;
const int IDC_SCENARIO_ZONES         = 969;
const int IDC_PASSAGE_OBJECTS_TREE   = 970;
const int IDC_PANZERKLEINS_TREE      = 971;
const int IDC_CAMERAS_TREE           = 972;
const int IDC_UNITGROUPS_TREE        = 973;
const int IDC_RPGMATERIALS_TREE      = 974;
const int IDC_SOUNDINSTANCES_TREE    = 975;
const int IDC_SOUNDEFFECTS_TREE      = 976;
const int IDC_SIDES_TREE             = 977;
const int IDC_RPG_STOREITEMS_TREE    = 978;
const int IDC_DIPLOMACY_TREE         = 979;
const int IDC_DIALOGS_TREE           = 980;
const int IDC_DIALOG_PERS_TREE       = 981;
const int IDC_RPG_PERKS_TREE         = 982;
const int IDC_RPG_MINES_TREE         = 983;
const int IDC_RPG_TOOLS_TREE         = 984;
const int IDC_RPG_KEYS_TREE          = 985;

#include <atldbcli.h>
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SDBConnection
{
  CDBPropSet	propset;	
	CSession	  session;

	SDBConnection() : propset(DBPROPSET_ROWSET) {}
};
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT OpenDBConnection( SDBConnection *pConnection );
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TAccessor> class CBaseDBCmd : public CCommand<TAccessor>
{
protected:
	SDBConnection *pConnection;
	
public:
	CBaseDBCmd() : pConnection(0) {}

	bool HasConnection() { return pConnection; }
  void SetConnection( SDBConnection *pConnection );
  virtual HRESULT Open( const std::string &szQuery );
	
  void Close();  
};
////////////////////////////////////////////////////////////////////////////////////////////////////
//#define DB_NAME         OLESTR( "A5" )
const LPCWSTR GetDBProvider();

#define TEMPL_TBL       "Templates"
#define VARIANTS_TBL    "TemplVariants"
#define RECTS_TBL       "Rects"
#define MODELS_TBL      "Models"
#define FINELEMS_TBL    "FinalElements"
#define UNITS_TBL       "Units"
#define WALLS_TBL       "Walls"
#define MATERIALS_TBL   "Materials"
#define GEOMETRIES_TBL  "Geometries"
#define TEXTURES_TBL    "Textures"
#define FLOORS_TBL      "Floors"
#define SOLIDOBJS_TBL   "SolidObjects"
#define INTERMEDIATE_FLOORS_TBL "IntermediateFloors"
#define INTERMEDIATE_SOLIDS_TBL "IntermediateSolids"
#define ROOMMAP_TBL     "Rooms"
#define ROOMPARAMS_TBL  "RoomParams"
#define TERRMAP_TBL     "TerrainMap"
#define CONTAINERS_TBL	"Containers"
#define EXPLOSIONS_TBL	"Explosions"

#define TYPE_TBL        "TemplateTypes"
#define MODELSTREE_TBL  "ModelsTree"
#define GEOMTREE_TBL    "GeometriesTree"
#define MATTREE_TBL     "MaterialsTree"
#define TEXTREE_TBL     "TexturesTree"
#define ATTRIBUTES_TBL  "Attributes"

#define FOLDER_COL      "FolderID"

#include <string>

using std::string;

void MakeQueryStr( string &szQuery, const string &szTable, vector<int> items );
void DisplayOLEDBErrorRecords( HRESULT hrErr );
string IToA( int n );
extern SDBConnection dbConnection;

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TAccessor> void CBaseDBCmd<TAccessor>::SetConnection( SDBConnection *pCnct )
{
	ASSERT( pCnct );
	pConnection = pCnct;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TAccessor> HRESULT CBaseDBCmd<TAccessor>::Open( const std::string &szQuery )
{
	if ( !pConnection )
		SetConnection( &dbConnection );
	
  Close();  // очищаем запрос
  
  HRESULT hr = CCommand<TAccessor>::Open( pConnection->session,  szQuery.c_str(), &pConnection->propset);
  if ( FAILED(hr) )
  {
    DisplayOLEDBErrorRecords( hr );
    return hr;
  }
  return S_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TAccessor> void CBaseDBCmd<TAccessor>::Close()
{
	if ( !pConnection )
		SetConnection( &dbConnection );
  CCommand<TAccessor>::Close();
  CCommand<TAccessor>::CreateCommand( pConnection->session );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __DBDEFS_H__
