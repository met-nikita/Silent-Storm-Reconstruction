#include "StdAfx.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
#include <dinput.h>
#include "..\Misc\basic2.h"
#include "..\Misc\StrProc.h"
#include "..\MiscDll\Commands.h"
#include "..\MiscDll\LogStream.h"
#include "..\Misc\Win32Helper.h"
#include "..\FileIO\Streams.h"
#include "..\FileIO\BasicChunk1.h"
#include "..\Input\Input.h"
#include "..\Input\Bind.h"
#include "..\Input\BindInternal.h"
using namespace NStr;
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NInput
{
////////////////////////////////////////////////////////////////////////////////////////////////////
// Global vars
typedef unordered_map<int, SActionInfo> TActionsMap;
typedef unordered_map<string, SCommand> TCommandsMap;
typedef unordered_map<string, SBindCommand> TBindCommandsMap;

static string szSection;
static list<SEvent> events;

TActionsMap& GetActions()
{
	static TActionsMap actionsMap;
	return actionsMap;
}
TCommandsMap& GetCommands()
{
	static TCommandsMap commandsMap;
	return commandsMap;
}
TBindCommandsMap& GetBindCommands()
{
	static TBindCommandsMap bindCommandsMap;
	return bindCommandsMap;
}

static void Update( DWORD dwTime );
static void ProcessMessage( const NInput::SMessage &mMsg );
static bool ProcessCommandMessage( const NInput::SMessage &mMsg, SCommand &cCommand );
static bool IsMappingBSubsetA( const SMapping &mSetA, const SMapping &mSetB );
static bool IsSameMappingExist( const SCommand &sCommand, const SMapping &sMapping );
static void CrossMappings( const SMapping &sBaseSet, SMapping *pSubSet );
////////////////////////////////////////////////////////////////////////////////////////////////////
// Public
////////////////////////////////////////////////////////////////////////////////////////////////////
const string& GetSection()
{
	return szSection;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetSection( const string &_szSection, bool bUpdate )
{
	if ( szSection == _szSection )
		return;

	szSection = _szSection;
	if ( bUpdate )
		UpdateBinds();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void Bind( EMappingType eType, const string &sCmd, const string &szSection, const vector<string> &szControlsSet )
{
	SMapping sMapping;
	TActionsMap &sActions = GetActions();
	TCommandsMap &sCommandsMap = GetCommands();

	SCommand &sCommand = sCommandsMap[sCmd];
	
	if ( szControlsSet.empty() )
		return;

	sMapping.mType = eType;
	if ( eType == MTYPE_EVENT_UP )
		sMapping.mType = MTYPE_EVENT;
	if ( eType == MTYPE_SLIDER_MINUS )
		sMapping.mType = MTYPE_SLIDER;

	sMapping.nPower = 100;
	if ( eType == MTYPE_EVENT_UP )
		sMapping.bActive = true;
	if ( ( eType == MTYPE_EVENT_UP ) || ( eType == MTYPE_SLIDER_MINUS ) )
		sMapping.nPower = -sMapping.nPower;

	sMapping.szSection = szSection;
	sMapping.actionsSet.resize( szControlsSet.size() );
	sMapping.fullActionsSet.resize( szControlsSet.size() );
	for ( int nTemp = 0; nTemp < szControlsSet.size(); nTemp++ )
	{
		int nAction = GetControlID( szControlsSet[nTemp] );
		sMapping.actionsSet[nTemp] = nAction;
		sMapping.fullActionsSet[nTemp] = nAction;

		SActionInfo &sInfo = sActions[nAction];
		if ( sInfo.eState != STATE_INITIALIZED )
		{
			if ( sInfo.eState != STATE_CONFIGPRESET )
				sInfo.fCoeff = 1.0f;;

			sInfo.eState = STATE_INITIALIZED;
			sInfo.szName = szControlsSet[nTemp];
			GetControlInfo( nAction, &sInfo.eType, &sInfo.fGranularity );
		}
	}

	if ( IsSameMappingExist( sCommand, sMapping ) )
		return;

	sCommand.mappingsList.push_back( sMapping );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void Unbind( const string &szCmd )
{
	TCommandsMap &sCommandsMap = GetCommands();

	TCommandsMap::iterator iTemp = sCommandsMap.find( szCmd );
	if ( iTemp == sCommandsMap.end() )
		return;

	SCommand &sCommand = iTemp->second;
	for ( list<SMapping>::iterator iMapping = sCommand.mappingsList.begin(); iMapping != sCommand.mappingsList.end(); )
	{
		if ( !szSection.empty() && ( iMapping->szSection != szSection ) )
			continue;

		if ( iMapping->mType != MTYPE_UNKNOWN )
			iMapping = sCommand.mappingsList.erase( iMapping );
		else
			iMapping++;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void UnbindAll()
{
	TCommandsMap &sCommandsMap = GetCommands();

	for ( TCommandsMap::iterator iTemp = sCommandsMap.begin(); iTemp != sCommandsMap.end(); iTemp++ )
		Unbind( iTemp->first );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void GetBind( const string &szCmd, list<SBind> *pRes )
{
	TActionsMap &sActions = GetActions();
	TCommandsMap &sCommandsMap = GetCommands();

	pRes->clear();

	if ( sCommandsMap.find( szCmd ) == sCommandsMap.end() )
		return;

	SCommand &sCommand = sCommandsMap[szCmd];
	for ( list<SMapping>::iterator iTempMapping = sCommand.mappingsList.begin(); iTempMapping != sCommand.mappingsList.end(); ++iTempMapping )
	{
		SBind &sBind = *( pRes->insert( pRes->end(), SBind()) );
		sBind.eType = iTempMapping->mType;
		sBind.szSection = iTempMapping->szSection;

		for( int nTemp = 0; nTemp < iTempMapping->actionsSet.size(); nTemp++ )
		{
			const SActionInfo &sTempInfo = sActions[iTempMapping->actionsSet[nTemp]];
			sBind.controlsSet.push_back( sTempInfo.szName );
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void UpdateBinds()
{
	TCommandsMap &sCommandsMap = GetCommands();

	for ( unordered_map<string, SCommand>::iterator iTempCommand = sCommandsMap.begin(); iTempCommand != sCommandsMap.end(); ++iTempCommand )
	{
		SCommand &sCommand = iTempCommand->second;
		for ( list<SMapping>::iterator iTempMapping = sCommand.mappingsList.begin(); iTempMapping != sCommand.mappingsList.end(); ++iTempMapping )
			iTempMapping->blockingGroupsSet.clear();
	}

	for ( unordered_map<string, SCommand>::iterator iTempCommand = sCommandsMap.begin(); iTempCommand != sCommandsMap.end(); ++iTempCommand )
	{
		SCommand &sCommand = iTempCommand->second;

		for ( list<SMapping>::iterator iTempMapping = sCommand.mappingsList.begin(); iTempMapping != sCommand.mappingsList.end(); ++iTempMapping )
		{
			iTempMapping->bDisabled = false;

			if ( !iTempMapping->szSection.empty() && ( iTempMapping->szSection != szSection ) )
				iTempMapping->bDisabled = true;
		}
	}

	for ( unordered_map<string, SCommand>::iterator iTempCommand1 = sCommandsMap.begin(); iTempCommand1 != sCommandsMap.end(); ++iTempCommand1 )
	{
		SCommand &sCommand1 = iTempCommand1->second;
		unordered_map<string, SCommand>::iterator iNext = iTempCommand1;
		iNext++;
		for ( unordered_map<string, SCommand>::iterator iTempCommand2 = iNext; iTempCommand2 != sCommandsMap.end(); ++iTempCommand2 )
		{
			SCommand &sCommand2 = iTempCommand2->second;

			for ( list<SMapping>::iterator iTempMapping1 = sCommand1.mappingsList.begin(); iTempMapping1 != sCommand1.mappingsList.end(); ++iTempMapping1 )
			{
				for ( list<SMapping>::iterator iTempMapping2 = sCommand2.mappingsList.begin(); iTempMapping2 != sCommand2.mappingsList.end(); ++iTempMapping2 )
				{
					if ( iTempMapping1->bDisabled || iTempMapping2->bDisabled )
						continue;

					if ( IsMappingBSubsetA( *iTempMapping1, *iTempMapping2 ) )
						CrossMappings( *iTempMapping1, &(*iTempMapping2) );
					if ( IsMappingBSubsetA( *iTempMapping2, *iTempMapping1 ) )
						CrossMappings( *iTempMapping2, &(*iTempMapping1) );
				}
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float GetControlCoeff( const string &szControl )
{
	TActionsMap &sActions = GetActions();

	int nAction = GetControlID( szControl );
	if ( nAction == -1 )
		return 1.0f;

	TActionsMap::iterator iTemp = sActions.find( nAction );
	if ( iTemp == sActions.end() )
		return 1.0f;

	return iTemp->second.fCoeff;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetControlCoeff( const string &szControl, float fCoeff )
{
	TActionsMap &sActions = GetActions();

	int nAction = GetControlID( szControl );
	if ( nAction == -1 )
		return;

	SActionInfo &sInfo = sActions[nAction];
	if ( sInfo.eState == STATE_DEFAULT )
		sInfo.eState = STATE_CONFIGPRESET;

	sInfo.fCoeff = fCoeff;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// SetCommandCoeff @0x3d0880 -- set a bound COMMAND's coeff (retail writes SCommand::fCoeff at +0x20). The
// camera sensitivity/invert options drive this; the coeff scales the command's fDelta in ProcessCommand
// (fDelta = nValue * fCoeff / 100000) -> magnitude = sensitivity, negative = inverted axis.
void SetCommandCoeff( const string &szCommand, float fCoeff )
{
	TCommandsMap &sCommandsMap = GetCommands();
	TCommandsMap::iterator i = sCommandsMap.find( szCommand );
	if ( i != sCommandsMap.end() )
		i->second.fCoeff = fCoeff;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool GetEvent( SEvent *psEvent )
{
	NInput::SMessage msg;
	if ( events.empty() )
	{
		if ( !NInput::GetMessage( &msg ) )
		{
			Update( msg.tTime );
			psEvent->commands.clear();
			psEvent->mMessage = msg;
			return false;
		}
		NInput::ProcessMessage( msg );
		return GetEvent( psEvent );
	}
	*psEvent = events.front();
	events.pop_front();
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void PostEvent( const string &szEvent )
{
	SEvent eEvent;

	TBindCommandsMap::iterator iTemp = GetBindCommands().find( szEvent );
	if ( iTemp != GetBindCommands().end() )
		eEvent.commands.push_back( &(iTemp->second) );

	eEvent.mMessage.cType = CT_UNKNOWN;
	eEvent.mMessage.tTime = GetTickCount();
	eEvent.mMessage.bState = false;
	eEvent.mMessage.nAction = -1;

	events.push_back( eEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CBind
////////////////////////////////////////////////////////////////////////////////////////////////////
CBind::CBind( const string &sCmd ): fDelta( 0 )
{
	TCommandsMap &sCommands = GetCommands();
	TBindCommandsMap &sBindCommands = GetBindCommands();

	pBindCommand = &sBindCommands[sCmd];
	pBindCommand->pCommand = &sCommands[sCmd];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBind::IsActive()
{
	return pBindCommand->bActive;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CBind::GetDelta()
{
	float fRes = fDelta;
	fDelta = 0;
	return fRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CBind::GetSpeed()
{
	return pBindCommand->fSpeed;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBind::ProcessEvent( const NInput::SEvent &eEvent )
{
	if ( eEvent.mMessage.cType == CT_TIME )
	{
		fDelta += pBindCommand->fDelta;
		return false;
	}
	
	if ( find( eEvent.commands.begin(), eEvent.commands.end(), pBindCommand ) == eEvent.commands.end() )
		return false;

	if ( ( eEvent.mMessage.cType == CT_UNKNOWN ) && ( eEvent.mMessage.nAction == -1 ) )
		return true;
	
	return ProcessCommandMessage( eEvent.mMessage, *pBindCommand->pCommand );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Internal functions
//
////////////////////////////////////////////////////////////////////////////////////////////////////
//! Update all commands
static void Update( DWORD dwTime )
{
	TCommandsMap &sCommands = GetCommands();
	TBindCommandsMap &sBindCommands = GetBindCommands();
	
	for ( TCommandsMap::iterator iTempCommand = sCommands.begin(); iTempCommand != sCommands.end(); ++iTempCommand )
	{
		SCommand &sCommand = iTempCommand->second;
		SBindCommand &sBindCommand = sBindCommands[iTempCommand->first];
		
		bool bActive = false;
		int64 nValue = 0;
		for( list<SMapping>::iterator iTempMapping = sCommand.mappingsList.begin(); iTempMapping != sCommand.mappingsList.end(); ++iTempMapping )
		{
			if ( iTempMapping->bDisabled )
				continue;

			nValue += iTempMapping->sAccumulator.Sample( dwTime );
			bActive |= iTempMapping->bActive;
		}

		sBindCommand.bActive = bActive;
		sBindCommand.fDelta = nValue * sCommand.fCoeff / 100000.0f;
		sBindCommand.fSpeed = sBindCommand.fDelta / ( ( dwTime - sBindCommand.dwTime ) * ( 1 / 1024.0f ) );
		sBindCommand.dwTime = dwTime;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool IsBindActive( const SMapping &sMapping )
{
	TActionsMap &sActions = GetActions();

	int nCount = 0;
	bool bSetActive = true;
	for( int nTemp = 0; nTemp < sMapping.actionsSet.size(); nTemp++ )
	{
		const SActionInfo &sTempInfo = sActions[sMapping.actionsSet[nTemp]];
		if ( sTempInfo.bActive )
			nCount++;
	}

	if ( nCount != sMapping.actionsSet.size() )
		bSetActive = false;

	if ( bSetActive )
	{
		for( list<vector<int> >::const_iterator iTempSet = sMapping.blockingGroupsSet.begin(); iTempSet != sMapping.blockingGroupsSet.end(); iTempSet++ )
		{
			int nBlockingCount = 0;
			const vector<int> &sBlockingSet = *iTempSet;

			for( int nTemp = 0; nTemp < sBlockingSet.size(); nTemp++ )
			{
				const SActionInfo &sTempInfo = sActions[sBlockingSet[nTemp]];
				
				if ( sTempInfo.bActive )
					nBlockingCount++;
				else if ( ( sTempInfo.eType == CT_AXIS ) || ( sTempInfo.eType == CT_LIMAXIS ) )
					nBlockingCount++;
			}

			if ( nBlockingCount == sBlockingSet.size() )
			{
				bSetActive = false;
				break;
			}
		}
	}

	if ( ( sMapping.mType == MTYPE_EVENT ) && ( sMapping.nPower < 0 ) )
		bSetActive = !bSetActive;

	return bSetActive;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//! Handle message and generate prediction
static void ProcessMessage( const NInput::SMessage &mMsg )
{
	SEvent sEvent;
	TActionsMap &sActions = GetActions();
	TCommandsMap &sCommands = GetCommands();
	TBindCommandsMap &sBindCommands = GetBindCommands();
	
	sEvent.commands.clear();
	sEvent.mMessage = mMsg;
		
	for ( unordered_map<string, SCommand>::iterator iTempCommand = sCommands.begin(); iTempCommand != sCommands.end(); ++iTempCommand )
	{
		SCommand &sCommand = iTempCommand->second;
		SBindCommand &sBindCommand = sBindCommands[iTempCommand->first];

		for ( list<SMapping>::iterator iTempMapping = sCommand.mappingsList.begin(); iTempMapping != sCommand.mappingsList.end(); ++iTempMapping )
		{
			if ( iTempMapping->bDisabled || iTempMapping->actionsSet.empty() )
				continue;
			if ( find( iTempMapping->fullActionsSet.begin(), iTempMapping->fullActionsSet.end(), mMsg.nAction ) == iTempMapping->fullActionsSet.end() )
				continue;

			SActionInfo &sActionInfo = sActions[mMsg.nAction];
			sActionInfo.bActive = mMsg.bState;

			bool bSetActive = IsBindActive( *iTempMapping );

			if ( ( mMsg.cType == CT_AXIS ) || ( mMsg.cType == CT_LIMAXIS ) )
				sActionInfo.bActive = false;

			if ( !bSetActive )
			{
				iTempMapping->bActive = false;
				iTempMapping->sAccumulator.sKeyAccumulator.Deactivate( mMsg.tTime );
				iTempMapping->sAccumulator.sPOVAccumulator.Deactivate( mMsg.tTime );
				iTempMapping->sAccumulator.sLimAxisAccumulator.Deactivate( mMsg.tTime );
				continue;
			}
			
			if ( iTempMapping->bActive && ( iTempMapping->mType == MTYPE_EVENT ) )
				continue;

			iTempMapping->bActive = true;
			sEvent.commands.push_back( &sBindCommands[iTempCommand->first] );
			continue;
		}
	}
	
	events.push_back( sEvent );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//! Handle messages for command
static bool ProcessCommandMessage( const NInput::SMessage &mMsg, SCommand &sCommand )
{
	TActionsMap &sActions = GetActions();

	for( list<SMapping>::iterator iTempMapping = sCommand.mappingsList.begin(); iTempMapping != sCommand.mappingsList.end(); ++iTempMapping )
	{
		if ( iTempMapping->bDisabled || iTempMapping->actionsSet.empty() )
			continue;
		if ( find( iTempMapping->fullActionsSet.begin(), iTempMapping->fullActionsSet.end(), mMsg.nAction ) == iTempMapping->fullActionsSet.end() )
			continue;
		
		SActionInfo &sActionInfo = sActions[mMsg.nAction];
		sActionInfo.bActive = mMsg.bState;
		
		bool bSetActive = IsBindActive( *iTempMapping );
		
		if ( ( mMsg.cType == CT_AXIS ) || ( mMsg.cType == CT_LIMAXIS ) )
			sActionInfo.bActive = false;

		if ( !bSetActive )
			continue;

		if ( ( iTempMapping->mType == MTYPE_SLIDER ) && ( mMsg.cType == CT_KEY ) )
			iTempMapping->sAccumulator.sKeyAccumulator.Activate( iTempMapping->nPower * sActionInfo.fCoeff, mMsg.tTime );
		else if ( ( iTempMapping->mType == MTYPE_SLIDER ) && ( mMsg.cType == CT_AXIS ) )
			iTempMapping->sAccumulator.sAxisAccumulator.Add( iTempMapping->nPower * sActionInfo.fCoeff * mMsg.nParam / sActionInfo.fGranularity, mMsg.tTime );
		else if ( ( iTempMapping->mType == MTYPE_SLIDER ) && ( mMsg.cType == CT_LIMAXIS ) )
			iTempMapping->sAccumulator.sLimAxisAccumulator.Add( iTempMapping->nPower * sActionInfo.fCoeff * mMsg.nParam / sActionInfo.fGranularity, mMsg.tTime );
		else if ( ( iTempMapping->mType == MTYPE_SLIDER ) && ( mMsg.cType == CT_POV ) )
		{
			if ( mMsg.nParam != -1 )
				iTempMapping->sAccumulator.sPOVAccumulator.Activate( iTempMapping->nPower * sActionInfo.fCoeff * mMsg.nParam / sActionInfo.fGranularity, mMsg.tTime );
			else 
				iTempMapping->sAccumulator.sPOVAccumulator.Deactivate( mMsg.tTime );
		}
		else if ( ( iTempMapping->mType == MTYPE_EVENT ) && ( mMsg.cType == CT_KEY ) )
		{
			int nPower = iTempMapping->nPower * ( mMsg.bState ? 1 : -1 );
			if ( nPower < 0 )
				return false;

			iTempMapping->sAccumulator.sKeyAccumulator.Activate( 0, mMsg.tTime );
			return true;
		}
		
		return true;
	}
	
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//! Crossover detection
static bool IsMappingBSubsetA( const SMapping &mSetA, const SMapping &mSetB )
{
	for ( vector<int>::const_iterator iTempB = mSetB.actionsSet.begin(); iTempB != mSetB.actionsSet.end(); ++iTempB )
	{
		bool bInSet = false;
		
		for ( vector<int>::const_iterator iTempA = mSetA.actionsSet.begin(); iTempA != mSetA.actionsSet.end(); ++iTempA )
		{
			if ( *iTempB == *iTempA )
				bInSet = true;
		}

		if ( !bInSet )
			return false;
	}

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool IsSameMappingExist( const SCommand &sCommand, const SMapping &sMapping )
{
	for ( list<SMapping>::const_iterator iTempMapping = sCommand.mappingsList.begin(); iTempMapping != sCommand.mappingsList.end(); ++iTempMapping )
	{
		if ( ( sMapping.mType != iTempMapping->mType ) || ( sMapping.nPower != iTempMapping->nPower ) || ( sMapping.bActive != iTempMapping->bActive ) || ( sMapping.szSection != iTempMapping->szSection ) )
			continue;
		if ( iTempMapping->actionsSet.size() != sMapping.actionsSet.size() )
			continue;

		int nSameActionsCount = 0;
		for ( vector<int>::const_iterator iTempAction = iTempMapping->actionsSet.begin(); iTempAction != iTempMapping->actionsSet.end(); iTempAction++ )
		{
			if ( find( sMapping.actionsSet.begin(), sMapping.actionsSet.end(), *iTempAction ) != sMapping.actionsSet.end() )
				nSameActionsCount++;
		}

		if ( nSameActionsCount == sMapping.actionsSet.size() )
			return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CrossMappings( const SMapping &sBaseSet, SMapping *pSubSet )
{
	vector<int> blockingGroupSet;
	TActionsMap &sActions = GetActions();

	for ( int nBaseTemp = 0; nBaseTemp < sBaseSet.actionsSet.size(); nBaseTemp++ )
	{
		int nAction = sBaseSet.actionsSet[nBaseTemp];
		bool bCrossover = false;
		
		for ( int nSubTemp = 0; nSubTemp < pSubSet->actionsSet.size(); nSubTemp++ )
		{
			if ( pSubSet->actionsSet[nSubTemp] == nAction )
				bCrossover = true;
		}
		
		const SActionInfo &sTempInfo = sActions[nAction];
		if ( !bCrossover && ( sTempInfo.eType == CT_KEY ) )
		{
			blockingGroupSet.push_back( nAction );
			pSubSet->fullActionsSet.push_back( nAction );
		}
	}

	if ( !blockingGroupSet.empty() )
		pSubSet->blockingGroupsSet.push_back( blockingGroupSet );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}; // NAMESPACE
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandBind( const string &szID, const vector<wstring> &_paramsSet, void *pContext )
{
	vector<wstring> paramsSet( _paramsSet );

	if ( paramsSet.size() < 2 )
	{
		csSystem << "usage: " << szID << "[+,-,!]'bind-id' 'KEY' + ..." << endl;
		csSystem << "'+' - slider plus, '-' - slider minus, '!' - on release" << endl;
		return;
	}

	vector<wstring>::const_iterator iTemp = paramsSet.begin();

	string szCommand( NStr::ToAscii( *paramsSet.begin() ) );
	paramsSet.erase( paramsSet.begin() );
	NStr::TrimBoth( szCommand, "\t\n\r\'" );

	NInput::EMappingType eType = NInput::MTYPE_UNKNOWN;
	switch ( szCommand[0] )
	{
	case '+':
		eType = NInput::MTYPE_SLIDER;
		NStr::TrimLeft( szCommand, '+' );
		break;
	case '-':
		eType = NInput::MTYPE_SLIDER_MINUS;
		NStr::TrimLeft( szCommand, '-' );
		break;
	case '!':
		eType = NInput::MTYPE_EVENT_UP;
		NStr::TrimLeft( szCommand, '!' );
		break;
	default:
		eType = NInput::MTYPE_EVENT;
		break;
	}

	vector<string> controlsSet;
	controlsSet.reserve( paramsSet.size() );
	for ( vector<wstring>::const_iterator iTemp = paramsSet.begin(); iTemp != paramsSet.end(); iTemp++ )
	{
		if ( iTemp->compare( L"+" ) == 0 )
			continue;

		string szWord( NStr::ToAscii( *iTemp ) );
		NStr::TrimBoth( szWord, "\t\n\r\'" );
		controlsSet.push_back( szWord );
	}

	NInput::Bind( eType, szCommand, NInput::GetSection(), controlsSet );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandUnbind( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( paramsSet.size() < 1 )
	{
		csSystem << "usage: " << szID << "'bind-id'" << endl;
		return;
	}

	NInput::Unbind( NStr::ToAscii( paramsSet.front() ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandUnbindAll( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	NInput::UnbindAll();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandBindSection( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	string szSection;

	if ( paramsSet.size() >= 1 )
		szSection = NStr::ToAscii( paramsSet.front() );

	NInput::SetSection( szSection, false );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandBindConfigure( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( paramsSet.size() < 2 )
	{
		csSystem << "usage: " << szID << "'bind-id' #coef" << endl;
		return;
	}

	NInput::SetControlCoeff( NStr::ToAscii( paramsSet[0] ), _wtof( paramsSet[1].c_str() ) );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandBindUpdate( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	NInput::UpdateBinds();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void CommandShowBind( const string &szID, const vector<wstring> &paramsSet, void *pContext )
{
	if ( paramsSet.size() < 1 )
	{
		csSystem << "usage: " << szID << "'bind-id'" << endl;
		return;
	}

	list<NInput::SBind> bindsList;
	NInput::GetBind( NStr::ToAscii( paramsSet.front() ), &bindsList );

	csSystem << "Bind '" << paramsSet.front() << "' :" << endl;
	for ( list<NInput::SBind>::const_iterator iTemp = bindsList.begin(); iTemp != bindsList.end(); iTemp++ )
	{
		for ( int nTemp = 0; nTemp < iTemp->controlsSet.size(); nTemp++ )
			csSystem << iTemp->controlsSet[nTemp] << " ";
		csSystem << endl;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
START_REGISTER(InputBind)
	REGISTER_CMD( "bind", CommandBind )
	REGISTER_CMD( "unbind", CommandUnbind )
	REGISTER_CMD( "unbindall", CommandUnbindAll )
	REGISTER_CMD( "bindsection", CommandBindSection )
	REGISTER_CMD( "bindconfigure", CommandBindConfigure )
	REGISTER_CMD( "bind_update", CommandBindUpdate )
	REGISTER_CMD( "showbind", CommandShowBind )
FINISH_REGISTER

