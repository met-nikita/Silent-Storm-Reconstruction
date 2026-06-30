#include "StdAfx.h"
#include "dbDefs.h"
#include "ObjBrowserDescription.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
static unordered_map<int, CObj<IObjectBrowser> > browsers;
IObjectBrowser* GetBrowser( EObjectBrowser nID )
{
	unordered_map<int, CObj<IObjectBrowser> >::const_iterator i = browsers.find( nID );
	if ( i != browsers.end() )
		return i->second;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CreateBrowsers()
{
	{
		IObjectBrowser *p = IObjectBrowser::Create( IDC_CONTAINERS_TREE );
		p->AddProperty( 0, "ParticleID" );
		p->AddProperty( 0, "SoundEffectID" );
		p->AddProperty( 0, "DestructionSoundID" );
		p->AddProperty( 1, "ModelID" );
		p->AddProperty( 1, "ModelID\\RPGArmorID" );
		p->AddProperty( 1, "ModelID\\RPGArmorID\\RPGMaterialID" );
		p->AddProperty( 2, "ModelID\\GeometryID" );
		p->AddProperty( 3, "ModelID\\GeometryID\\AIGeometryID" );
		p->AddProperty( 3, "ModelID\\GeometryID\\AIGeometryID\\SolidPart" );
		p->AddProperty( 3, "ModelID\\GeometryID\\AIGeometryID\\Damage" );
		p->AddProperty( 3, "ModelID\\GeometryID\\AIGeometryID\\Cover" );
		p->AddProperty( 3, "ModelID\\GeometryID\\AIGeometryID\\Vision" );
		p->AddProperty( 3, "ModelID\\GeometryID\\AIGeometryID\\Passability" );
		p->AddProperty( 3, "ModelID\\GeometryID\\AIGeometryID\\ItemBlocker" );
		p->AddProperty( 4, "ModelID\\GeometryID\\AIGeometry2ID" );
		p->AddProperty( 4, "ModelID\\GeometryID\\AIGeometry2ID\\Damage" );
		p->AddProperty( 4, "ModelID\\GeometryID\\AIGeometry2ID\\Cover" );
		p->AddProperty( 4, "ModelID\\GeometryID\\AIGeometry2ID\\Vision" );
		p->AddProperty( 4, "ModelID\\GeometryID\\AIGeometry2ID\\Passability" );
		p->AddProperty( 4, "ModelID\\GeometryID\\AIGeometry2ID\\ItemBlocker" );
		browsers[OB_CONTAINERS] = p;
	}
	{
		IObjectBrowser *p = IObjectBrowser::Create( IDC_OBJECTS_TREE );
		p->AddProperty( 0, "Model0", GetBrowser( OB_CONTAINERS ) );
		browsers[OB_OBJECTS_MODEL0] = p;
	}
	{
		IObjectBrowser *p = IObjectBrowser::Create( IDC_OBJECTS_TREE );
		p->AddProperty( 0, "Model1", GetBrowser( OB_CONTAINERS ) );
		browsers[OB_OBJECTS_MODEL1] = p;
	}
	{
		IObjectBrowser *p = IObjectBrowser::Create( IDC_OBJECTS_TREE );
		p->AddProperty( 0, "Model2", GetBrowser( OB_CONTAINERS ) );
		browsers[OB_OBJECTS_MODEL2] = p;
	}
	{
		IObjectBrowser *p = IObjectBrowser::Create( IDC_OBJECTS_TREE );
		p->AddProperty( 0, "Model3", GetBrowser( OB_CONTAINERS ) );
		browsers[OB_OBJECTS_MODEL3] = p;
	}
	{
		IObjectBrowser *p = IObjectBrowser::Create( IDC_OBJECTS_TREE );
		p->AddProperty( 0, "Model4", GetBrowser( OB_CONTAINERS ) );
		browsers[OB_OBJECTS_MODEL4] = p;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////