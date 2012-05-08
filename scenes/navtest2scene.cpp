#include "navtest2scene.h"

#include "../game/lumosgame.h"
#include "../game/worldmap.h"
#include "../game/mapspatialcomponent.h"
#include "../game/gamelimits.h"
#include "../game/pathmovecomponent.h"
#include "../game/debugpathcomponent.h"

#include "../xegame/chit.h"
#include "../xegame/rendercomponent.h"

#include "../engine/engine.h"


using namespace grinliz;
using namespace gamui;

//#define DEBUG_PMC


NavTest2Scene::NavTest2Scene( LumosGame* game ) : Scene( game )
{
	nChits = 0;
	creationTick = 0;
	game->InitStd( &gamui2D, &okay, 0 );
	engine = 0;

	LoadMap();
}


NavTest2Scene::~NavTest2Scene()
{
	chitBag.DeleteAll();
	delete engine;
	delete map;
}


void NavTest2Scene::Resize()
{
	LumosGame* lumosGame = static_cast<LumosGame*>( game );
	lumosGame->PositionStd( &okay, 0 );
}


void NavTest2Scene::LoadMap()
{
	map = new WorldMap( 32, 32 );

	delete engine;
	engine = new Engine( game->GetScreenportMutable(), game->GetDatabase(), map );	
	engine->CameraLookAt( (float)map->Width()*0.5f, (float)map->Height()*0.5f, 40 );

	grinliz::CDynArray<Vector2I> blocks;
	map->InitPNG( "./res/testnav.png", &blocks, &waypoints );

	for ( int i=0; i<blocks.Size(); ++i ) {
		Chit* chit = chitBag.NewChit();
		const Vector2I& v = blocks[i];
		MapSpatialComponent* msc = new MapSpatialComponent( 1, 1, map );
		chit->Add( msc );
		chit->Add( new RenderComponent( engine, "unitCube", 0 ));

		GET_COMPONENT( chit, MapSpatialComponent )->SetMapPosition( v.x, v.y, 0 );
	}
	for( int i=0; i<waypoints.Size(); ++i ) {
		CreateChit( waypoints[i] );
	}
}


void NavTest2Scene::CreateChit( const Vector2I& p )
{
	Chit* chit = chitBag.NewChit();
	chit->Add( new SpatialComponent() );

	const char* asset = "humanFemale";
	if ( random.Rand( 4 ) == 0 ) {
		asset = "hornet";
	}

	chit->Add( new RenderComponent( engine, asset, MODEL_USER_AVOIDS ));
	chit->Add( new PathMoveComponent( map, engine->GetSpaceTree() ));
#ifdef DEBUG_PMC
	chit->Add( new DebugPathComponent( engine, map, static_cast<LumosGame*>(game) ));
#endif

	chit->GetSpatialComponent()->SetPosition( (float)p.x+0.5f, 0, (float)p.y+0.5f );
	chit->AddListener( this );
	OnChitMsg( chit, "PathMoveComponent", PathMoveComponent::MSG_DESTINATION_REACHED );
	++nChits;
}


void NavTest2Scene::OnChitMsg( Chit* chit, const char* componentName, int id )
{
	if ( StrEqual( componentName, "PathMoveComponent" ) ) {
		const Vector2I& dest = waypoints[random.Rand(waypoints.Size())];
		GET_COMPONENT( chit, PathMoveComponent )->SetDest( (float)dest.x+0.5f, (float)dest.y+0.5f ); 
	}
}


void NavTest2Scene::Zoom( int style, float delta )
{
	if ( style == GAME_ZOOM_PINCH )
		engine->SetZoom( engine->GetZoom() *( 1.0f+delta) );
	else
		engine->SetZoom( engine->GetZoom() + delta );
}


void NavTest2Scene::Rotate( float degrees ) 
{
	engine->camera.Orbit( degrees );
}


void NavTest2Scene::Tap( int action, const grinliz::Vector2F& view, const grinliz::Ray& world )				
{
	bool uiHasTap = ProcessTap( action, view, world );
	if ( !uiHasTap ) {
		int tap = Process3DTap( action, view, world, engine );
	}
}


void NavTest2Scene::ItemTapped( const gamui::UIItem* item )
{
	if ( item == &okay ) {
		game->PopScene();
	}
}


void NavTest2Scene::DoTick( U32 deltaTime )
{
	chitBag.DoTick( deltaTime );
	++creationTick;
	if ( creationTick == 5 && nChits < 100 ) {
		CreateChit( waypoints[random.Rand(waypoints.Size()) ] );
		creationTick = 0;
	}
}


void NavTest2Scene::Draw3D( U32 deltaTime )
{
	engine->Draw( deltaTime );
}