#include "buildscript.h"
#include "itemscript.h"

#include "../grinliz/glstringutil.h"

using namespace grinliz;

/* static */
BuildData BuildScript::buildData[NUM_OPTIONS] = {
	{ "", "", 0, 0, 0 },
	// Utility
	{	"Cancel",	"",			0, 0, 0 },
	{	"Clear",	"",			0, 0, 0 },
	{	"Rotate",	"",			0, 0, 0 },
	{	"Pave",		"pave",		0, 0, 0 },
	{	"Ice",		"ice",		0, 0, 0 },
	// Visitor stuff
	{	"News",		"kiosk.n",	1, 0, 0 },
	{	"Media",	"kiosk.m",	1, 0, 0 },
	{	"Commerce",	"kiosk.c",	1, 0, 0 },
	{	"Social",	"kiosk.s",	1, 0, 0 },
	{	"Temple",	"power",	1, 0, 0 },
	// Economy
	{	"Forge",	"factory",	2, 0, 0 },
	{	"Vault",	"vault",	2, 0, 0 },
	{	"SleepTube","bed",		2, 0, 0 },
	{	"Market",	"market",	2, 0, 0 },
	{	"Bar",		"bar",		2, 0, 0 },
	// Defense
	{	"GuardPost","guardpost",3, 0, 0 },
	// Industry
	{	"Solar Farm","farm",		4, 0, 0 },
	{	"Distillery","distillery",4, 0, 0 },
};


const BuildData* BuildScript::GetDataFromStructure( const grinliz::IString& structure )
{
	for( int i=0; i<NUM_OPTIONS; ++i ) {
		if ( structure == buildData[i].cStructure ) {
			return &GetData( i );
		}
	}
	return 0;
}


const BuildData& BuildScript::GetData( int i )
{
	GLASSERT( i >= 0 && i < NUM_OPTIONS );
	if ( buildData[i].size == 0 ) {

		buildData[i].name		= StringPool::Intern( buildData[i].cName, true );
		buildData[i].structure	= StringPool::Intern( buildData[i].cStructure, true );

		bool has = ItemDefDB::Instance()->Has( buildData[i].cStructure );

		// Generate the label.
		CStr<64> str = buildData[i].cName;
		if ( *buildData[i].cName && has ) 
		{
			const GameItem& gi = ItemDefDB::Instance()->Get( buildData[i].cStructure );
			int cost = 0;
			gi.keyValues.Get( ISC::cost, &cost );
			str.Format( "%s\nAu %d", buildData[i].cName, cost );
		}
		buildData[i].label = StringPool::Intern( str.c_str() ) ;
		
		buildData[i].size = 1;
		if ( has ) {
			const GameItem& gi = ItemDefDB::Instance()->Get( buildData[i].cStructure );
			gi.keyValues.Get( ISC::size, &buildData[i].size );
			gi.keyValues.Get( ISC::cost, &buildData[i].cost );

			buildData[i].needs.SetZero();
			
			for( int k=0; k<ai::Needs::NUM_NEEDS; ++k ) {
				CStr<32> str;
				str.Format( "need.%s", ai::Needs::Name(k) );

				float need=0;
				gi.keyValues.Get( str.c_str(), &need );
				if ( need > 0 ) {
					buildData[i].needs.Set( k, need );
				}
			}

			float timeF = 1.0;
			gi.keyValues.Get( "need.time", &timeF );
			buildData[i].standTime = int( timeF * 1000.0f );
		}
	}
	return buildData[i];
}


