#include "sim.h"

#include "../engine/engine.h"

#include "../xegame/xegamelimits.h"
#include "../xegame/spatialcomponent.h"
#include "../xegame/itemcomponent.h"
#include "../xegame/rendercomponent.h"
#include "../xegame/chitbag.h"
#include "../xegame/inventorycomponent.h"
#include "../xegame/componentfactory.h"

#include "../script/itemscript.h"
#include "../script/scriptcomponent.h"
#include "../script/volcanoscript.h"
#include "../script/plantscript.h"

#include "pathmovecomponent.h"
#include "debugstatecomponent.h"
#include "healthcomponent.h"

#include "lumosgame.h"
#include "worldmap.h"
#include "lumoschitbag.h"
#include "weather.h"
#include "mapspatialcomponent.h"

using namespace grinliz;
using namespace tinyxml2;

Sim::Sim( LumosGame* g )
{
	lumosGame = g;
	Screenport* port = lumosGame->GetScreenportMutable();
	const gamedb::Reader* database = lumosGame->GetDatabase();

	worldMap = new WorldMap( MAX_MAP_SIZE, MAX_MAP_SIZE );
	engine = new Engine( port, database, worldMap );
	worldMap->AttachEngine( engine );
	weather = new Weather( MAX_MAP_SIZE, MAX_MAP_SIZE );

	engine->SetGlow( true );
	engine->LoadConfigFiles( "./res/particles.xml", "./res/lighting.xml" );

	chitBag = new LumosChitBag();
	playerID = 0;
	minuteClock = 0;
	timeInMinutes = 0;
	volcTimer = 0;
	secondClock = 1000;

	random.SetSeedFromTime();
}


Sim::~Sim()
{
	delete weather;
	worldMap->AttachEngine( 0 );
	delete chitBag;
	delete engine;
	delete worldMap;
}


void Sim::Load( const char* mapDAT, const char* mapXML, const char* gameXML )
{
	chitBag->DeleteAll();
	worldMap->Load( mapDAT, mapXML );

	if ( !gameXML ) {
		Vector2I v = worldMap->FindEmbark();
		CreatePlayer( v, "humanFemale" );
	}
	else {
		ComponentFactory factory( this, engine, worldMap, weather, lumosGame );
		XMLDocument doc;
		doc.LoadFile( gameXML );
		GLASSERT( !doc.Error() );
		if ( !doc.Error() ) {
			const XMLElement* root = doc.FirstChildElement( "Sim" );
			playerID = 0;
			Archive( 0, root );
			engine->camera.Load( root );
			chitBag->Load( &factory, root );
		}
	}
}


void Sim::Archive( tinyxml2::XMLPrinter* prn, const tinyxml2::XMLElement* ele )
{
	XE_ARCHIVE( playerID );
	XE_ARCHIVE( minuteClock );
	XE_ARCHIVE( timeInMinutes );
}


void Sim::Save( const char* mapDAT, const char* mapXML, const char* gameXML )
{
	worldMap->Save( mapDAT, mapXML );

	FILE* fp = fopen( gameXML, "w" );
	GLASSERT( fp );
	if ( fp ) {
		XMLPrinter printer( fp );
		printer.OpenElement( "Sim" );
		Archive( &printer, 0 );
		engine->camera.Save( &printer );
		chitBag->Save( &printer );
		printer.CloseElement();
		fclose( fp );
	}
}


void Sim::CreatePlayer( const grinliz::Vector2I& pos, const char* assetName )
{
	ItemDefDB* itemDefDB = ItemDefDB::Instance();
	ItemDefDB::GameItemArr itemDefArr;
	itemDefDB->Get( assetName, &itemDefArr );
	GLASSERT( itemDefArr.Size() > 0 );

	Chit* chit = chitBag->NewChit();
	playerID = chit->ID();

	chit->Add( new SpatialComponent());
	chit->Add( new RenderComponent( engine, assetName ));
	chit->Add( new PathMoveComponent( worldMap ));
	chit->Add( new DebugStateComponent( worldMap ));

	GameItem item( *(itemDefArr[0]));
	item.primaryTeam = 1;
	item.stats.SetExpFromLevel( 4 );
	item.InitState();
	chit->Add( new ItemComponent( item ));

	chit->Add( new HealthComponent());
	chit->Add( new InventoryComponent());

	chit->GetSpatialComponent()->SetPosYRot( (float)pos.x+0.5f, 0, (float)pos.y+0.5f, 0 );
}


Chit* Sim::GetPlayerChit()
{
	return chitBag->GetChit( playerID );
}


Texture* Sim::GetMiniMapTexture()
{
	return engine->GetMiniMapTexture();
}


void Sim::DoTick( U32 delta )
{
	worldMap->DoTick( delta, chitBag );
	chitBag->DoTick( delta, engine );

	bool minuteTick = false;
	bool secondTick = false;

	minuteClock -= (int)delta;
	if ( minuteClock <= 0 ) {
		minuteClock += MINUTE;
		timeInMinutes++;
		minuteTick = true;
	}

	secondClock -= (int)delta;
	if ( secondClock <= 0 ) {
		secondClock += 1000;
		secondTick = true;
	}

	// Logic that will probably need to be broken out.
	// What happens in a given age?
	int age = minuteClock / AGE;
	volcTimer += delta;

	// Age of Fire
	static const int VOLC_RAD = 9;
	static const int VOLC_DIAM = VOLC_RAD*2+1;
	static const int NUM_VOLC = MAX_MAP_SIZE*MAX_MAP_SIZE / (VOLC_DIAM*VOLC_DIAM);
	static const int MSEC_TO_VOLC = AGE / NUM_VOLC;

	switch( age ) {
	case 0:
		// The Age of Fire
		// Essentially want to get the world covered.
		while( volcTimer >= MSEC_TO_VOLC ) {
			volcTimer -= MSEC_TO_VOLC;

			for( int i=0; i<5; ++i ) {
				int x = random.Rand(worldMap->Width());
				int y = random.Rand(worldMap->Height());
				if ( worldMap->IsLand( x, y ) ) {
					CreateVolcano( x, y, VOLC_RAD );
					break;
				}
			}
		}
		break;

	default:
		if ( minuteTick && random.Rand(3)==0 ) {
			CreateVolcano( random.Rand(worldMap->Width()), random.Rand(worldMap->Height()), VOLC_RAD );
		}
		break;
	}

	if ( secondTick ) {
		CreatePlant( random.Rand(worldMap->Width()), random.Rand(worldMap->Height()), -1 );
	}
}


void Sim::Draw3D( U32 deltaTime )
{
	engine->Draw( deltaTime );
}


void Sim::SetAllRock()
{
	for( int j=0; j<worldMap->Height(); ++j ) {
		for( int i=0; i<worldMap->Width(); ++i ) {
			worldMap->SetRock( i, j, -1, 0, false );
		}
	}
}


void Sim::CreateVolcano( int x, int y, int size )
{
	Chit* chit = chitBag->NewChit();
	chit->Add( new SpatialComponent() );
	chit->Add( new ScriptComponent( new VolcanoScript( worldMap, size )));

	chit->GetSpatialComponent()->SetPosition( (float)x+0.5f, 0.0f, (float)y+0.5f );
}


void Sim::CreatePlant( int x, int y, int type )
{
	if ( !worldMap->Bounds().Contains( x, y ) ) {
		return;
	}

	const WorldGrid& wg = worldMap->GetWorldGrid( x, y );

	if ( wg.IsPassable() && !wg.InUse() ) {
		// check for a plant already there.
		// At this point, check for *anything* there. Could relax in future.
		Rectangle2F r;
		r.Set( (float)x, (float)y, (float)(x+1), (float)(y+1) );
		chitBag->QuerySpatialHash( &queryArr, r, 0, 0 );

		if ( queryArr.Empty() ) {

			if ( type < 0 ) {
				// Scan for a good type!
				r.Outset( 3.0f );
				chitBag->QuerySpatialHash( &queryArr, r, 0, 0 );

				float chance[NUM_PLANT_TYPES];
				for( int i=0; i<NUM_PLANT_TYPES; ++i ) {
					chance[i] = 1.0f;
				}

				for( int i=0; i<queryArr.Size(); ++i ) {
					Vector3F pos = { (float)x+0.5f, 0, (float)y+0.5f };
					Chit* c = queryArr[i];

					int stage, type;
					GameItem* item = PlantScript::IsPlant( c, &type, &stage );
					if ( item ) {
						float weight = (float)((stage+1)*(stage+1)) / ( c->GetSpatialComponent()->GetPosition() - pos ).Length();
						chance[type] += weight;
					}
				}
				type = random.Select( chance, NUM_PLANT_TYPES );
			}

			Chit* chit = chitBag->NewChit();
			MapSpatialComponent* ms = new MapSpatialComponent( worldMap );
			ms->SetMapPosition( x, y );
			chit->Add( ms );
			GLASSERT( wg.InUse() );

			chit->Add( new HealthComponent() );
			chit->Add( new ScriptComponent( new PlantScript( this, engine, worldMap, weather, type )));
		}
	}
}
