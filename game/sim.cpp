#include "sim.h"

#include "../engine/engine.h"

#include "../xegame/xegamelimits.h"
#include "../xegame/spatialcomponent.h"
#include "../xegame/itemcomponent.h"
#include "../xegame/rendercomponent.h"
#include "../xegame/chitbag.h"
#include "../xegame/inventorycomponent.h"
#include "../xegame/componentfactory.h"
#include "../xegame/cameracomponent.h"

#include "../script/itemscript.h"
#include "../script/scriptcomponent.h"
#include "../script/volcanoscript.h"
#include "../script/plantscript.h"
#include "../script/corescript.h"

#include "pathmovecomponent.h"
#include "debugstatecomponent.h"
#include "healthcomponent.h"
#include "lumosgame.h"
#include "worldmap.h"
#include "lumoschitbag.h"
#include "weather.h"
#include "mapspatialcomponent.h"

#include "../xarchive/glstreamer.h"

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
	chitBag->SetContext( engine, worldMap );
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


void Sim::Load( const char* mapDAT, const char* gameDAT )
{
	chitBag->DeleteAll();
	worldMap->Load( mapDAT );

	if ( !gameDAT ) {
		// Fresh start 
		CreatePlayer();
		CreateCores();
	}
	else {
		QuickProfile qp( "Sim::Load" );
		ComponentFactory factory( this, &chitBag->census, engine, worldMap, weather, lumosGame );

		FILE* fp = fopen( gameDAT, "rb" );
		GLASSERT( fp );
		if ( fp ) {
			StreamReader reader( fp );
			XarcOpen( &reader, "Sim" );
			XARC_SER( &reader, playerID );
			XARC_SER( &reader, minuteClock );
			XARC_SER( &reader, timeInMinutes );

			engine->camera.Serialize( &reader );
			chitBag->Serialize( &factory, &reader );

			XarcClose( &reader );

			fclose( fp );
		}
	}
}


void Sim::Save( const char* mapDAT, const char* gameDAT )
{
	worldMap->Save( mapDAT );

	{
		QuickProfile qp( "Sim::SaveXarc" );

		ComponentFactory factory( this, &chitBag->census, engine, worldMap, weather, lumosGame );

		FILE* fp = fopen( gameDAT, "wb" );
		if ( fp ) {
			StreamWriter writer( fp );
			XarcOpen( &writer, "Sim" );
			XARC_SER( &writer, playerID );
			XARC_SER( &writer, minuteClock );
			XARC_SER( &writer, timeInMinutes );

			engine->camera.Serialize( &writer );
			chitBag->Serialize( &factory, &writer );

			XarcClose( &writer );
			fclose( fp );
		}
	}
}


void Sim::CreateCores()
{
	ItemDefDB* itemDefDB = ItemDefDB::Instance();
	ItemDefDB::GameItemArr itemDefArr;
	itemDefDB->Get( "core", &itemDefArr );
	GLASSERT( itemDefArr.Size() > 0 );
	const GameItem* gameItem = itemDefArr[0];
	const char* asset = gameItem->resource.c_str();

	// FIXME: use zones
	for( int i=0; i<MAX_CORES; ++i ) {
		int x = random.Rand( worldMap->Width() );
		int y = random.Rand( worldMap->Height() );

		const WorldGrid& wg = worldMap->GetWorldGrid( x, y );
		if ( wg.IsLand() && !wg.InUse() && wg.IsPassable() ) {
			Chit* chit = chitBag->NewChit();

			MapSpatialComponent* ms = new MapSpatialComponent( worldMap );
			ms->SetMapPosition( x, y );
			ms->SetMode( MapSpatialComponent::BLOCKS_GRID ); 
			chit->Add( ms );
			GLASSERT( wg.InUse() );

			chit->Add( new ScriptComponent( new CoreScript( worldMap ), &chitBag->census ));
			chit->Add( new ItemComponent( engine, *gameItem ));
			chit->Add( new RenderComponent( engine, asset ));
		}
	}
}


grinliz::Vector2F Sim::FindCore( const grinliz::Vector2F& pos )
{
	Census* census = &chitBag->census;

	Chit* best = 0;
	float close = FLT_MAX;

	for( int i=0; i<census->cores.Size(); ++i ) {
		int id = census->cores[i];
		Chit* chit = chitBag->GetChit( id );
		if ( chit ) {
			float len2 = ( pos - chit->GetSpatialComponent()->GetPosition2D() ).LengthSquared();
			if ( len2 < close ) {
				close = len2;
				best = chit;
			}
		}
	}
	Vector2F r = { pos.x, pos.y };
	if ( best ) {
		r = best->GetSpatialComponent()->GetPosition2D();
	}
	return r;
}


void Sim::CreatePlayer()
{
	Vector2I v = worldMap->FindEmbark();
	CreatePlayer( v, "humanFemale" );
}


void Sim::CreatePlayer( const grinliz::Vector2I& pos, const char* assetName )
{
	if ( !assetName ) {
		assetName = "humanFemale";
	}

	ItemDefDB* itemDefDB = ItemDefDB::Instance();
	ItemDefDB::GameItemArr itemDefArr;
	itemDefDB->Get( assetName, &itemDefArr );
	GLASSERT( itemDefArr.Size() > 0 );

	Chit* chit = chitBag->NewChit();
	playerID = chit->ID();
	chitBag->GetCamera( engine )->SetTrack( playerID );

	chit->Add( new SpatialComponent());
	chit->Add( new RenderComponent( engine, assetName ));
	chit->Add( new PathMoveComponent( worldMap ));
	chit->Add( new DebugStateComponent( worldMap ));

	GameItem item( *(itemDefArr[0]));
	item.primaryTeam = 1;
	item.stats.SetExpFromLevel( 4 );
	item.InitState();
	chit->Add( new ItemComponent( engine, item ));

	chit->Add( new HealthComponent( engine ));
	chit->Add( new InventoryComponent( engine ));

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
	int age = timeInMinutes / MINUTES_IN_AGE;
	volcTimer += delta;

	// Age of Fire. Needs lots of volcanoes to seed the world.
	static const int VOLC_RAD = 9;
	static const int VOLC_DIAM = VOLC_RAD*2+1;
	static const int NUM_VOLC = MAX_MAP_SIZE*MAX_MAP_SIZE / (VOLC_DIAM*VOLC_DIAM);
	int MSEC_TO_VOLC = AGE / NUM_VOLC;

	// NOT Age of Fire:
	if ( age > 0 ) {
		MSEC_TO_VOLC *= 10;
	}

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

//	if ( secondTick ) {
		CreatePlant( random.Rand(worldMap->Width()), random.Rand(worldMap->Height()), -1 );
//	}
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
	chit->Add( new ScriptComponent( new VolcanoScript( worldMap, size ), &chitBag->census ));

	chit->GetSpatialComponent()->SetPosition( (float)x+0.5f, 0.0f, (float)y+0.5f );
}


void Sim::CreatePlant( int x, int y, int type )
{
	if ( !worldMap->Bounds().Contains( x, y ) ) {
		return;
	}

	// About 50,000 plants seems about right.
	int count = chitBag->census.CountPlants();
	if ( count > worldMap->Bounds().Area() / 20 ) {
		return;
	}

	// And space them out a little.
	Random r( x+y*1024 );
	if ( r.Rand(4) == 0 )
		return;

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
						float weight = (float)((stage+1)*(stage+1));
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

			chit->Add( new HealthComponent( engine ) );
			chit->Add( new ScriptComponent( new PlantScript( this, engine, worldMap, weather, type ), &chitBag->census ));
		}
	}
}

