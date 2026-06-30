#include "StdAfx.h"
#include "SGData.h"
#import "msxml3.dll"
#include <fstream>
///////////////////////////////////////////////////////////////////////////////////////////////////////////
//inline int MSVCMustDie_tolower( int a ) { return tolower(a); }
//inline void ToLower( std::string &szString ) { std::transform( szString.begin(), szString.end(), szString.begin(), std::ptr_fun(MSVCMustDie_tolower) ); }
const string strDot = "dot.exe";
const int N_NOWAY_ZONE = 0xFFFF;
int nHoldrand = 0x11112222;
inline int GRandom( int nMax ) { return (((nHoldrand = nHoldrand * 214013L + 2531011L) >> 16) & 0x7fff) % nMax; }
void RemoveChar( string &str, char ch )
{
	int i;
	while( (i = int(str.find(ch))) != -1 )
		str.erase( i, 1 );
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CZone::CanPlaceClue( CClue* clue )
{
	return clue->isPermanent || GetPlaceNumByType(clue->type) > 0;
}
int *CZone::GetPlaceNum( string type )
{
	if ( type == "Item" )
		return &nItemSlot;
	else if ( type == "Person" )
		return &nPersonSlot;
	else
		return 0;
}
int CZone::GetPlaceNumByType( string type )
{
	int *pNum = GetPlaceNum(type);
	if ( pNum )
		return *pNum;
	return 0;
}
string CZone::GetLabel()
{
	char buff[1024];
	sprintf( buff, "'%s' #%i", code.c_str(), nDistance );
	if ( nDistance != N_NOWAY_ZONE )
		return string(buff);
	return code;
}
bool CScenGraph::PlaceClue( CZone &zone, CClue* clue )
{
	int *pNum = zone.GetPlaceNum( clue->type );
	if ( !clue->isPermanent )
	{
		if ( !pNum || *pNum < 1 )
			return false;
		--(*pNum);
	}
	zone.placed_clues.push_back(clue->code);
	clue->isPlaced = true;
	for ( unsigned k = 0; k < clue->objectives.size(); ++k )
	{
		CObjective &obj = clue->objectives[k];
		if ( !obj.cluesToOpen.empty() )
			for ( unsigned t = 0; t < obj.cluesToOpen.size(); ++t )
			{
				CClue *pCl = GC(obj.cluesToOpen[t]);
				pCl->nTmpDerClues--;
			}
	}
	return true;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenGraph::GenerateFullGraph()
{
	for ( unordered_map<string,CClue>::iterator k = clues.begin(); k != clues.end(); ++k )
	{
		CClue &clue = k->second;
		clue.isPlaced = true;
		clue.nTmpDerClues = 0;
		clue.nMinClueToOpen = -1;
		for ( unsigned nZone = 0; nZone < clue.zone2place.size(); ++nZone )
			GZ(clue.zone2place[nZone])->placed_clues.push_back(clue.code);
	}
	Check();
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////
CZone &CScenGraph::GetZone( CZoneP zoneName )
{
	return zones.find( zoneName )->second;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////
CClue &CScenGraph::GetClue( CClueP clueName )
{
	return clues.find( clueName )->second;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenGraph::FindChildZones( CZoneP zoneName, list<CZoneP> *childZones, bool bUseProcessed )
{
	CZone &zone = GetZone( zoneName );
	for ( vector<CClueP>::iterator clue = zone.placed_clues.begin(); clue != zone.placed_clues.end(); ++clue )
		FindZonesOpenedByClue( *clue, childZones, zone.code, bUseProcessed );
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenGraph::FindZonesOpenedByClue( CClueP clueName, list<CZoneP> *OpenedZones, CZoneP parent, bool bUseProcessed )
{
	if ( clueName == "" )
		return;
	//
	CClue &clue = GetClue( clueName );
	for ( vector<CObjective>::iterator objective = clue.objectives.begin(); objective != clue.objectives.end(); ++objective )
	{
		// заносим зоны
		for ( vector<CZoneP>::iterator zone = (*objective).zone2open.begin(); zone != (*objective).zone2open.end(); ++zone )
		{
			CZone &childZone = GetZone( *zone );
			if ( !childZone.bProcessed || !bUseProcessed )
			{
				if ( parent != "" )
					childZone.pComeFrom = parent;
				childZone.bProcessed = true;
				OpenedZones->push_back( childZone.code );
			}
		}
		// перебираем улики
		if ( !objective->cluesToOpen.empty() )
			for ( unsigned t = 0; t < objective->cluesToOpen.size(); ++t )
				FindZonesOpenedByClue( objective->cluesToOpen[t], OpenedZones, parent );
	}
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenGraph::GetPath( CZoneP start, list<CZoneP> *path, bool (* IsFinal)( CZone zone ) )
{
	path->clear();
	if ( IsFinal( GetZone( start ) ) )
		return;
	//
	CZoneP finish;
	// иниацилизация
	for ( unordered_map<string,CZone>::iterator i = zones.begin(); i != zones.end(); ++i )
	{
		i->second.bProcessed = false;
		i->second.pComeFrom = "";
	}
	// Пускаем волну из начальной точки
	list<CZoneP> WaveFront;
	WaveFront.push_back( start );
	GetZone( start ).bProcessed = true;
	//
	bool bFinishReached = false;
	while ( !bFinishReached )
	{
		list<CZoneP> NewWaveFront;
		// заносим каждого потомка в новый фронт волны
		for ( list<CZoneP>::iterator zone = WaveFront.begin(); zone != WaveFront.end(); ++zone )
			FindChildZones( *zone, &NewWaveFront );
		//
		WaveFront.clear();
		WaveFront = NewWaveFront;
		NewWaveFront.clear();
		//
		for ( list<CZoneP>::iterator zone = WaveFront.begin(); zone != WaveFront.end(); ++zone )
			if ( IsFinal( GetZone( *zone ) ) )
			{
				bFinishReached = true;
				finish = *zone;
				WaveFront.clear();
				break;
			}
	}
	// запоминаем кратчайший путь
	CZoneP current = finish;
	path->push_front( current );
	while ( current != start )
	{
		current = GetZone( GetZone( current ).pComeFrom ).code;
		path->push_front( current );
	}
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScenGraph::FindUnprocessedBranch( CZoneP *start, CZoneP *finish )
{
	for ( int i = 0; i < 16; ++i )
	{
		for ( unordered_map<string,CZone>::iterator zone = zones.begin(); zone != zones.end(); ++zone )
			if ( zone->second.bDifCalculated && zone->second.nDifficulty == i )
			{
				list<CZoneP> childZones;
				FindChildZones( zone->second.code, &childZones, false );
				for ( list<CZoneP>::iterator childZone = childZones.begin(); childZone != childZones.end(); ++childZone )
					if ( !GetZone( *childZone ).bDifCalculated )
					{
						*start = zone->second.code;
						*finish = *childZone;
						return true;
					}
				childZones.clear();
			}
	}
	//
	return false;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenGraph::SetPathDifficulties( const list<CZoneP> &path )
{
	if ( path.size() < 2 )
		return;
	//
	float fBegin = max ( 0.f, (float)GetZone( path.front() ).nDifficulty );
	float fEnd = max ( 0.f, (float)GetZone( path.back() ).nDifficulty );
	float fStep = ( fEnd - fBegin ) / float( path.size() - 1 );
	float fCurrent = fBegin;
	for ( list<CZoneP>::const_iterator zone = path.begin(); zone != path.end(); ++zone )
	{
		GetZone( *zone ).nDifficulty = (int)fCurrent;
		GetZone( *zone ).bDifCalculated = true;
		fCurrent += fStep;
	}
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScenGraph::IsBeginZone( CZoneP zoneName )
{
	for ( unordered_map<string,CClue>::iterator clue = clues.begin(); clue != clues.end(); ++clue )
	{
		if ( !clue->second.isPlaced )
			continue;
		//
		list<CZoneP> childZones;
		FindZonesOpenedByClue( clue->second.code, &childZones, "", false );
		for ( list<CZoneP>::iterator childZone = childZones.begin(); childZone != childZones.end(); ++childZone )
			if ( *childZone == zoneName )
				return false;
		childZones.clear();
	}
	//
	return true;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenGraph::InitDifficulties()
{
	for ( unordered_map<string,CZone>::iterator zone = zones.begin(); zone != zones.end(); ++zone )
	{
		if ( IsBeginZone( zone->second.code ) )
		{
			zone->second.nDifficulty = 0;
			zone->second.bDifCalculated = true;
		}
		else if ( zone->second.placed_clues.size() == 0 )
		{
			zone->second.nDifficulty = 13;
			zone->second.bDifCalculated = true;
		}
		else
		{
			zone->second.nDifficulty = 0;
			zone->second.bDifCalculated = false;
		}
	}
	//
	GetZone( "Base" ).nDifficulty = 0;
	GetZone( "FFight" ).nDifficulty = 15;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////
CZone globalFinish;
bool IsFinal_Finish( CZone zone )
{
	return zone.code == globalFinish.code;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsFinal_Processed( CZone zone )
{
	return zone.bDifCalculated;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenGraph::CalculateDifficulties()
{
	list<CZoneP> path;
	globalFinish = GetZone( "FFight" );
	InitDifficulties();
	// заполняем сложности
	GetPath( "Base", &path, IsFinal_Finish );
	SetPathDifficulties( path );
	//
	CZoneP start;
	CZoneP finish;
	while ( FindUnprocessedBranch( &start, &finish ) )
	{
		GetPath( finish, &path, IsFinal_Processed );
		path.push_front( start );
		SetPathDifficulties( path );
	}
	//
	GetZone( "FFight" ).nDifficulty = 15;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenGraph::GenerateCorrectGraph()
{
	// Reset clues
	for ( unordered_map<string,CClue>::iterator k = clues.begin(); k != clues.end(); ++k )
		k->second.isPlaced = false;
	unordered_map<string,CZone> saved_zones(zones);
	unordered_map<string,CClue> saved_clues(clues);
	int nIter = 1;
	for( ; nIter < 100; ++nIter )
	{
		zones.clear();
		clues.clear();
		zones = saved_zones;
		clues = saved_clues;
		GenerateGraph();
		if ( Check() )
			break;
	}
	//
	printf("Graph generated in %i iteration.\n", nIter );
}
extern int nHoldrand;
void CScenGraph::GenerateGraph()
{
	nRandomSeed = nHoldrand = GetTickCount(); // 20615562
	for ( unordered_map<string,CClue>::iterator i = clues.begin(); i != clues.end(); ++i )
		i->second.ResetTmpDerClues();
	for ( bool bCanPlace = true; bCanPlace; )
	{
		int nRndClue = GRandom( int( clues_index.size() ) );
		for ( unsigned j = nRndClue; ; )
		{
			CClue &clue = *GC(clues_index[j]);
			if ( !clue.isPlaced )
			{
				if ( clue.zone2place.empty() )
				{
					clue.isPlaced = true;
					break;
				}
				int nZone = GRandom( int(clue.zone2place.size()) );
				for( int nFirstZone = nZone; !GZ(clue.zone2place[nZone])->CanPlaceClue(&clue); )
				{
					if ( ++nZone >= int(clue.zone2place.size()) )
						nZone = 0;
					if ( nFirstZone == nZone )
						break;
				}
				if ( GZ(clue.zone2place[nZone])->CanPlaceClue(&clue) )
				{
					PlaceClue( *GZ(clue.zone2place[nZone]), &clue );
					break;
				}
			}
			if ( ++j >= clues_index.size() )
				j = 0;
			if ( j == nRndClue )
			{
				bCanPlace = false;
				break;
			}
		}
	}
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////
void WriteActionColor( fstream &file, CObjective &obj )
{
	if ( obj.action == "Capture" )
		file << " edge[color=blue,style=filled];\n";
	else if ( obj.action == "Destroy" )
		file << " edge[color=red,style=dotted];\n";
	else
		file << " edge[color=black,style=filled];\n";
}
void WriteZoneColor( fstream &file, CZone &zone )
{
	if ( zone.country == "England" )
		file << " node[color=red];\n";
	else if ( zone.country == "Russia" )
		file << " node[color=green];\n";
	else if ( zone.country == "Germany" )
		file << " node[color=blue];\n";
	else
		file << " node[color=black];\n";
	if ( zone.bUnachievable )
		file << " node[style=dotted];\n";
	else
	{
		if ( zone.isInShortPath )
			file << " node[style=bold];\n";
		else
			file << " node[style=solid];\n";
	}
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////
void CScenGraph::SaveOneZone( fstream &file, CZone &zone, const char chStrat )
{
	if ( zone.code.empty() )
	{
		file << "error::Skiping bag zone" << endl;
		return;
	}
	WriteZoneColor( file, zone );
	file << chStrat << zone.code << "[fontcolor = \"#" << hex << int( zone.nDifficulty / 15.f * 255 ) << dec << "0000\", label = \"";
	if ( !zone.placed_clues.empty() && bNeedItems )
	{
		file << "{" << zone.GetLabel() << "|{";
		for ( unsigned j = 0; j < zone.placed_clues.size(); ++j )
		{
			CClue &clue = *GC(zone.placed_clues[j]);
			file << '<' << clue.code << '>' << clue.code;
			if ( j+1 != zone.placed_clues.size() )
				file << '|';
		}
		file << "}}";
	}
	else
		file << zone.GetLabel();

	file << ", Dif = " << zone.nDifficulty; // << ", ParentName= " << zone.pRef << ", Ref = " << zone.nRef;
	file << "\"];" << endl;
}
void CScenGraph::SaveZones( fstream &file )
{
	file << " concentrate=true;\n"
		 << " nodesep=" << strSep << ";\n"
		 << " ranksep=" << strSep << ";\n"
		 << " node[shape=record];\n";
	if ( bUniteZones )
	{
		for ( unordered_map< string, vector<CZoneP> >::iterator l = byLocation.begin(); l != byLocation.end(); ++l )
		{
			file << " subgraph cluster" << l->first << "\n\t{\n";
			file << "\tlabel = \"" << l->first << "\";" << endl;
			file << "\tcolor = green;" << endl;
			for ( vector<CZoneP>::iterator i = l->second.begin(); i != l->second.end(); ++i )
				SaveOneZone( file, *GZ(*i), '\t' );
			file << "\t}" << endl;
		}
	}
	else
	{
		for ( unordered_map<string,CZone>::iterator i = zones.begin(); i != zones.end(); ++i )
			SaveOneZone( file, i->second );
		printf( "Saved %i zones\n", zones.size() );
	}
}
void CScenGraph::SaveCheck( fstream &file )
{
	file << "label=\"Graph is ";
	if ( Check() )
		file << "correct";
	else
		file << "incorrect";
	file << "\"\n";
}
void CScenGraph::SaveSeparateGraph( string fileName )
{
	fstream file;
	file.open( (fileName + ".dot").c_str(), ios_base::out | ios_base::trunc );
	file << "digraph g\n{ //" << nRandomSeed << "\n";
	SaveZones(file);
	file << " node[shape=ellipse,color=black, style=solid];\n";
	file << " edge[color=black];\n";
	for ( unordered_map<string,CZone>::iterator i = zones.begin(); i != zones.end(); ++i )
	{
		CZone &zone = i->second;
//		WriteZoneColor( file, zone );
		for ( unsigned j = 0; j < zone.placed_clues.size(); ++j )
		{
			CClue &clue = *GC(zone.placed_clues[j]);
			file << zone.code + " -> " << clue.code << ";\n";
		}
	}
	file << " node[shape=ellipse,color=black];\n";
	int nKKK = 0;
	for ( unordered_map<string,CClue>::iterator k = clues.begin(); k != clues.end(); ++k, ++nKKK )
	{
		CClue &clue = k->second;
		if ( clue.IsClueOpened() && ( clue.nDerClues > 0 || clue.isPlaced ) )
		{
			// Перебераем Actions
			for ( unsigned k = 0; k < clue.objectives.size(); ++k )
			{
				CObjective &obj = clue.objectives[k];
				WriteActionColor( file, obj );
				for ( unsigned o = 0; o < obj.zone2open.size(); ++o )
					file << clue.code << " -> "<< obj.zone2open[o] << ';' << endl;

				if ( !obj.cluesToOpen.empty() )
				{
					for ( unsigned t = 0; t < obj.cluesToOpen.size(); ++t )
						if ( GC(obj.cluesToOpen[t])->IsClueOpened() )
						{
							file << clue.code << " -> "<< obj.cluesToOpen[t] << ';' << endl;
						}

				}
			}
		}
	}
	SaveCheck(file);
	file << "}";
	file.close();
	string strRun = strDot + " -Tjpg " + fileName + ".dot -o " + fileName + ".jpg";
	system( strRun.c_str() );
}
void CScenGraph::SaveGraph( string fileName )
{
	fstream file;
	file.open( (fileName + ".dot").c_str(), ios_base::out | ios_base::trunc );
	file << "digraph g\n{\n";
	SaveZones(file);

	for ( unordered_map<string,CZone>::iterator i = zones.begin(); i != zones.end(); ++i )
	{
		CZone &zone = i->second;
		WriteZoneColor( file, zone );
		for ( unsigned j = 0; j < zone.placed_clues.size(); ++j )
		{
			CClue &clue = *GC(zone.placed_clues[j]);
			string strStart = "  " + zone.code + ':' + clue.code + " -> ";
			// Перебераем Actions
			for ( unsigned k = 0; k < clue.objectives.size(); ++k )
			{
				CObjective &obj = clue.objectives[k];
				WriteActionColor( file, obj );
				for ( unsigned o = 0; o < obj.zone2open.size(); ++o )
					if ( obj.zone2open[o] != zone.code )
						file << strStart << obj.zone2open[o] << ';' << endl;
			}
		}
	}
	SaveCheck(file);
	file << "}";
	file.close();
	string strRun = strDot + " -Tjpg " + fileName + ".dot -o " + fileName + ".jpg";
	system( strRun.c_str() );
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CScenGraph::LoadZones( string strFileName )
{
	MSXML2::IXMLDOMDocumentPtr document( "Microsoft.XMLDOM" );

	if( document == 0 )
	{
		printf( "Error loading MS XML Parser. Minimum requirements: Internet Explorer 4.01 SP1" );
		return false;
	}

	// читаем документ
	document->async = false;
	//strFileName += ".xml";
	if( !document->load( strFileName.c_str() ) )
	{
		printf( "Can't open file %s.\n", strFileName.c_str() );
		return false;
	}
	// корневой элемент
	MSXML2::IXMLDOMNodeListPtr nodes = document->selectNodes( "Zones" );
	MSXML2::IXMLDOMNodePtr nodeRoot = nodes->item[0];

	// Зоны
	nodes = nodeRoot->selectNodes( "Zone" );
	for( int i = 0; i < nodes->length; i++ )
	{
		MSXML2::IXMLDOMNodePtr m_node = nodes->item[i];
		CZone zone;
		zone.code = m_node->attributes->getNamedItem("Code")->text;
		RemoveChar( zone.code, ' ' );
		if ( zone.code.length() < N_MIN_CODE )
			printf( "Bad 'code' in zone %i\n", i );
		zone.country = m_node->attributes->getNamedItem("Country")->text;
		zone.location = m_node->attributes->getNamedItem("Location")->text;
		zone.description = m_node->attributes->getNamedItem("Description")->text;
		zone.nItemSlot = atoi( m_node->attributes->getNamedItem("ItemSlots")->text );
		zone.nPersonSlot = atoi( m_node->attributes->getNamedItem("PersonSlots")->text );
		zone.nDifficulty = 0;
		zone.bProcessed = false;
		zones[zone.code] = zone;
		byLocation[zone.country].push_back( zone.code );
	}
	printf( "Loaded %i zones\n", zones.size() );
	// Факты
	nodes = nodeRoot->selectNodes( "Clues" );
	for( int i = 0; i < nodes->length; i++ )
	{
		MSXML2::IXMLDOMNodePtr m_node = nodes->item[i];
		CClue clue;
		string sGroup = "";
//		clue.description = m_node->attributes->getNamedItem("Description")->text;
		clue.code = m_node->attributes->getNamedItem("Code")->text;
		RemoveChar( clue.code, ' ' );
		if ( m_node->attributes->getNamedItem("Description") )
			clue.description = m_node->attributes->getNamedItem("Description")->text;
		clue.type = m_node->attributes->getNamedItem("Type")->text;
		if ( m_node->attributes->getNamedItem("Group") )
			sGroup = m_node->attributes->getNamedItem("Group")->text;
		clue.isPermanent = false;
		if ( m_node->attributes->getNamedItem("Permanent") )
			if ( string(m_node->attributes->getNamedItem("Permanent")->text) == "yes" )
				clue.isPermanent = true;
		clue.nMinClueToOpen = -1;
		if ( m_node->attributes->getNamedItem("MinLinksToOpen") )
			clue.nMinClueToOpen = atoi(m_node->attributes->getNamedItem("MinLinksToOpen")->text);
		clue.isPlaced = false;
		clue.nDerClues = 0;
		clue.nItemID = 0;
		clue.nPersID = 0;
		if ( m_node->attributes->getNamedItem("ItemID") )
			clue.nItemID = atoi(m_node->attributes->getNamedItem("ItemID")->text);
		if ( m_node->attributes->getNamedItem("PersID") )
			clue.nPersID = atoi(m_node->attributes->getNamedItem("PersID")->text);
		// zone to place
		MSXML2::IXMLDOMNodeListPtr nns = m_node->selectNodes( "ZoneToPlace" );
		for( int k = 0; k < nns->length; k++ )
		{
			string str = nns->item[k]->text;
			RemoveChar( str, ' ' );
			if ( str.length() < N_MIN_CODE )
				printf( "Empty 'zone2place' in clue %i\n", i );
			else
			{
				if ( zones.find(str) != zones.end() )
					clue.zone2place.push_back( str );
				else
					printf( "Can't find place zone '%s'\n", str.c_str() );
			}
		}
		// zone 2 open
		nns = m_node->selectNodes( "Obj" );
		for( int j = 0; j < nns->length; j++ )
		{
			MSXML2::IXMLDOMNodePtr o_node = nns->item[j];
			CObjective obj;
			obj.action = o_node->attributes->getNamedItem("Action")->text;
			if ( o_node->attributes->getNamedItem("Description") )
				obj.description = o_node->attributes->getNamedItem("Description")->text;
//			obj.description = o_node->attributes->getNamedItem("Description")->text;
			MSXML2::IXMLDOMNodeListPtr zo = o_node->selectNodes( "ZoneToOpen" );
			for( int k = 0; k < zo->length; k++ )
			{
				string str = zo->item[k]->text;
				RemoveChar( str, ' ' );
				if ( str.length() < N_MIN_CODE )
					printf( "Empty fiels 'zone2open' in clue %i\n", i );
				else
				{
					if ( zones.find(str) != zones.end() )
						obj.zone2open.push_back( str );
					else
						printf( "Can't find zone to open '%s'\n", str.c_str() );
				}
			}
			//
			MSXML2::IXMLDOMNodeListPtr zd = o_node->selectNodes( "ZoneToDestroy" );
			for( int k = 0; k < zd->length; k++ )
			{
				string str = zd->item[k]->text;
				RemoveChar( str, ' ' );
				if ( str.length() < N_MIN_CODE )
					printf( "Empty fiels 'zone2destroy' in clue %i\n", i );
				else
				{
					if ( zones.find(str) != zones.end() )
						obj.zone2destroy.push_back( str );
					else
						printf( "Can't find zone to open '%s'\n", str.c_str() );
				}
			}
			clue.objectives.push_back(obj);
		}
		if ( clues.find(clue.code) != clues.end() )
		{
			printf( "Error: clue %s have dublicate in row %i\n", clue.code.c_str(), i );
			continue;
		}
		clues[clue.code] = clue;
		clues_index.push_back( clue.code );
		byGroup[sGroup].push_back( clue.code );
	}
	// Resolve cross-cluses
	for( int i = 0; i < nodes->length; i++ )
	{
		MSXML2::IXMLDOMNodePtr m_node = nodes->item[i];
		string clue_code;
		clue_code = m_node->attributes->getNamedItem("Code")->text;
		RemoveChar( clue_code, ' ' );
		MSXML2::IXMLDOMNodeListPtr nns = m_node->selectNodes( "Obj" );
		for( int j = 0; j < nns->length; j++ )
		{
			MSXML2::IXMLDOMNodePtr o_node = nns->item[j];
			MSXML2::IXMLDOMNodeListPtr ccns = o_node->selectNodes( "ClueToOpen" );
			CClueP pClueToOpen;
            if ( o_node->attributes->getNamedItem("ClueToOpen") )
			{
				pClueToOpen = o_node->attributes->getNamedItem("ClueToOpen")->text;
				RemoveChar( pClueToOpen, ' ' );
                GC(pClueToOpen)->nDerClues++;
				GC(pClueToOpen)->whoOpenMe.push_back(clue_code);
				clues[clue_code].objectives[j].cluesToOpen.push_back(pClueToOpen);
			}
			else if ( ccns )
			{
				for( int t = 0; t < ccns->length; t++ )
				{
					pClueToOpen = ccns->item[t]->text;
					RemoveChar( pClueToOpen, ' ' );
					GC(pClueToOpen)->nDerClues++;
					GC(pClueToOpen)->whoOpenMe.push_back(clue_code);
					clues[clue_code].objectives[j].cluesToOpen.push_back(pClueToOpen);
				}
			}
		}
	}
	printf( "Loaded %i clues\n", clues.size() );
	return true;
}
/*bool CScenGraph::IsReached( CClue &clue )
{
	int i = 0;
	for( ; i < clue.whoOpenMe.size(); ++i )
	{
		CZone *pZone = ZoneByClue(clue);
		if ( !pZone || !passed[pZone->code] )
			return;
	}
	return i == clue.whoOpenMe.size();
}*/
void PushSubLink( CZone &zoneFrom, CZone *pZone, stack<CZone*> &links, int nDistOver = -1 )
{
	int nNewDist = N_NOWAY_ZONE;
	if ( nDistOver == -1 )
		nNewDist = zoneFrom.nDistance + 1;
	else
		nNewDist = nDistOver;
	if ( nNewDist > pZone->nDistance )
		return;
	pZone->nDistance = nNewDist;
	pZone->pComeFrom = zoneFrom.code;
	links.push( pZone );
}
void CScenGraph::PushClueLinks( CZone &zoneFrom, CClue &clue, stack<CZone*> &links )
{
	clue.nMaxDistToOpen = max( clue.nMaxDistToOpen, zoneFrom.nDistance + 1 );
	if ( clue.nTmpDerClues > 0 )
		clue.nTmpDerClues--;
	if ( clue.nTmpDerClues != 0 )
		return;
	for ( unsigned k = 0; k < clue.objectives.size(); ++k )
	{
		CObjective &obj = clue.objectives[k];
		for ( unsigned o = 0; o < obj.zone2open.size(); ++o )
			PushSubLink( zoneFrom, GZ(obj.zone2open[o]), links, clue.nMaxDistToOpen );
		if ( !obj.cluesToOpen.empty() )
			for ( unsigned t = 0; t < obj.cluesToOpen.size(); ++t )
				PushClueLinks( zoneFrom, *GC(obj.cluesToOpen[t]), links );
	}
}
void CScenGraph::PushZoneLinks( CZone &zone, stack<CZone*> &links )
{
	for ( unsigned j = 0; j < zone.placed_clues.size(); ++j )
	{
		CClue &clue = *GC(zone.placed_clues[j]);
		for ( unsigned k = 0; k < clue.objectives.size(); ++k )
		{
			CObjective &obj = clue.objectives[k];
			for ( unsigned o = 0; o < obj.zone2open.size(); ++o )
				PushSubLink( zone, GZ(obj.zone2open[o]), links );
			if ( !obj.cluesToOpen.empty() )
				for ( unsigned t = 0; t < obj.cluesToOpen.size(); ++t )
					PushClueLinks( zone, *GC(obj.cluesToOpen[t]), links );
		}
	}
}
bool CScenGraph::Check()
{
	for ( unordered_map<string,CZone>::iterator i = zones.begin(); i != zones.end(); ++i )
	{
		i->second.bUnachievable = true;
		i->second.nDistance = N_NOWAY_ZONE;
		i->second.pComeFrom = "";
		i->second.isInShortPath = false;
	}
	for ( unordered_map<string,CClue>::iterator i = clues.begin(); i != clues.end(); ++i )
	{
		i->second.nMaxDistToOpen = 0;
		i->second.ResetTmpDerClues();
	}
	unordered_map<string,CZone>::iterator iStart= zones.find("Base");
	unordered_map<string,CZone>::iterator iFinish= zones.find("FFight");
	if ( iStart == zones.end() || iFinish == zones.end() )
		return false;
	CZone &start = iStart->second;
	CZone &finish = iFinish->second;
	unordered_map<string,int> passed;
	stack<CZone*> st;
	st.push(&start);
	start.nDistance = 0;
	bool bCheck = false;
	for(;;)
	{
		if ( st.empty() )
			break;
		CZone *pCur = st.top();
		st.pop();
		if ( pCur == &finish )
			bCheck = true;
		PushZoneLinks( *pCur, st );
//		passed[pCur->code] = true;
		pCur->bUnachievable = false;
	}
	if ( bCheck )
	{
		for( CZone *pCur = &finish; pCur != &start; )
		{
			pCur->isInShortPath = true;
			pCur = GZ(pCur->pComeFrom);
		}
		start.isInShortPath = true;
	}
	return bCheck;
}