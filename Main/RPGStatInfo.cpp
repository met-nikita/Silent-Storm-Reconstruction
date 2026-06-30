#include "StdAfx.h"
#include "..\DBFormat\DataRPG.h"
#include "RPGUnit.h"
#include "RPGUnitInfo.h"
#include "RPGCritical.h"
#include "aiPosition.h"
#include "..\MiscDll\LogStream.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
static string szCriticalNames[NDb::N_CRIT_TYPES];
static string szStatNames[NDb::SKILL_TYPE_NUMBERS];
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NRPG
{
////////////////////////////////////////////////////////////////////////////////////////////////////
static void Initialize()
{
	if ( szCriticalNames[0] == "" )
	{
		szCriticalNames[NDb::C_VP] = "AP Penalty (VP base)";
		szCriticalNames[NDb::C_DEATH] = "Death";
		szCriticalNames[NDb::C_AP_REDUCTION] = "AP reduction";
		szCriticalNames[NDb::C_BLIND] = "Blind";
		szCriticalNames[NDb::C_DEAF] = "Deaf";
		szCriticalNames[NDb::C_WEAPONSKILL_REDUCTION] = "Weapon skill reduction";
		szCriticalNames[NDb::C_MOTIONLESS] = "Motionless/AP red.";
		szCriticalNames[NDb::C_ENCUMBRANCE] = "Encumbrance";
		szCriticalNames[NDb::C_ACCIDENTAL_SHOT] = "Accidental shot";
		szCriticalNames[NDb::C_STUN] = "Stun";
		szCriticalNames[NDb::C_LOST_WEAPON] = "Lost weapon";
		szCriticalNames[NDb::C_IDLE_HAND] = "Idle hand";
		szCriticalNames[NDb::C_DAMAGE_WEAPON] = "Weapon damaged";
		szCriticalNames[NDb::C_PATIENT] = "Patient";
		//
		szStatNames[NDb::ST_MELEE] = "Mel ";
		szStatNames[NDb::ST_SHOOTING] = "Shot";
		szStatNames[NDb::ST_THROWING] = "Thr ";
		szStatNames[NDb::ST_BURST] = "Bur ";
		szStatNames[NDb::ST_SNIPE] = "Snip";
		szStatNames[NDb::ST_STEALTH] = "Stlz";
		szStatNames[NDb::ST_SPOT] = "Spot";
		szStatNames[NDb::ST_MEDICINE] = "Med ";
		szStatNames[NDb::ST_ENGINEERING] = "Eng ";
		szStatNames[NDb::ST_VP] = "VP  ";
		szStatNames[NDb::ST_AP] = "AP  ";
		szStatNames[NDb::ST_IC] = "IC  ";
		szStatNames[NDb::ST_INTERRUPT] = "Intr";
		szStatNames[NDb::ST_LEVEL] = "Lev ";
		szStatNames[NDb::ST_STR] = "Str ";
		szStatNames[NDb::ST_DEX] = "Dex ";
		szStatNames[NDb::ST_INT] = "Int ";
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static string szCritLocations[] =
{
	"Head",
	"Torso",
	"Arms",
	"Legs",
	"ASSERT(0)"
};
////////////////////////////////////////////////////////////////////////////////////////////////////
const string& GetCLName( NDb::ECriticalLocation cl )
{
	return szCritLocations[cl];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static string szHitLocations[] =
{
	"Body",
	"Head",
	"RHand",
	"LHand",
	"RLeg",
	"LLeg",
	"ASSERT(0)",
};
////////////////////////////////////////////////////////////////////////////////////////////////////
const string& GetHLName( NAI::EHitLocation cl )
{
	static string szAny = "Any";
	static string szAssert = "ASSERT(0)";
	if ( NAI::HL_ANY == cl )
		return szAny;
	else if ( cl >= NAI::N_HL )
		return szAssert;
	else
		return szHitLocations[cl];
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void DumpCritical( CCritical *p )
{
	if ( !p )
		return;
	Initialize();
	const SCritical &c = p->GetCritical();
	csRPG << "<font size=16pt>";
	csRPG << "<color=red>" << "\tCritical: " << "<color=orange>"  << "Type=" << "<color=grey>" << "\"" << szCriticalNames[c.eCritical] << "\";  ";
	csRPG << "<color=orange>"  << "\tLocation=" << "<color=grey>" << GetCLName( p->GetCritical().eCl );
	csRPG << ";  \t" << "<color=orange>"  << "Modifier=" << "<color=grey>" << p->GetModifier();
	csRPG << ";  \t" << "<color=orange>" << "Remainig time=" << "<color=grey>" << p->GetRemainingTime() << ";  \t";
	csRPG << "<color=orange>"  << "DC=" << "<color=grey>" << c.nDC << "\n";
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void DumpStats( CUnit *pUnit, int nHealedVP )
{
	if ( !pUnit )
	{
		ASSERT(0);
		return;
	}
	Initialize();
	const int nWords = 9;
	for ( int l = 0, i = 0; i < NDb::SKILL_TYPE_NUMBERS; ++i )
	{
		CDynamicSkill &sk = pUnit->Skills( i );
		string szPad1 = int(sk / 10) == 0 ? " /" : "/";
		string szPad2 = int(sk.GetMaxValue() / 10) == 0 ? "  " : " ";
		csRPG << "<font size=16pt>" << "<color=yellow> \t" << szStatNames[i] << "<color=grey>" << "=" << "<color=white>" << sk << szPad1 << sk.GetMaxValue() << szPad2;
		if ( ++l == nWords )
		{
			csRPG << "\n";
			l = 0;
		}
	}
	csRPG << CC_YELLOW << " \tHealedVP" << "<color=grey>" << "=" << CC_GREEN << nHealedVP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////////////////////////
