#include "navtestscene.h"
#include "../game/lumosgame.h"
#include "../engine/engine.h"
#include "../game/worldmap.h"
#include "../grinliz/glgeometry.h"

using namespace grinliz;
using namespace gamui;

// Draw calls as a proxy for world subdivision:
// 20+20 blocks:
// quad division: 249 -> 407 -> 535
// block growing:  79 -> 135 -> 173
//  32x32          41 ->  97 -> 137
// Wow. Sticking with the growing block algorithm.
// Need to tune size on real map. (Note that the 32vs16 is approaching the same value.)

NavTestScene::NavTestScene( LumosGame* game ) : Scene( game )
{
	game->InitStd( &gamui2D, &okay, 0 );

	LayoutCalculator layout = game->DefaultLayout();
	block.Init( &gamui2D, game->GetButtonLook( LumosGame::BUTTON_LOOK_STD ));
	block.SetSize( layout.Width(), layout.Height() );
	block.SetText( "block" );

	block20.Init( &gamui2D, game->GetButtonLook( LumosGame::BUTTON_LOOK_STD ));
	block20.SetSize( layout.Width(), layout.Height() );
	block20.SetText( "block20" );

	engine = new Engine( game->GetScreenportMutable(), game->GetDatabase() );
	
	map = new WorldMap( 32, 32 );
	map->InitCircle();

	engine->SetMap( map );
	engine->CameraLookAt( 10, 10, 40 );
}


NavTestScene::~NavTestScene()
{
	engine->SetMap( 0 );
	delete map;
	delete engine;
}


void NavTestScene::Resize()
{
	LumosGame* lumosGame = static_cast<LumosGame*>( game );
	lumosGame->PositionStd( &okay, 0 );
	LayoutCalculator layout = lumosGame->DefaultLayout();
	layout.PosAbs( &block, 1, -1 );
	layout.PosAbs( &block20, 2, -1 );
}


void NavTestScene::Zoom( int style, float delta )
{
	if ( style == GAME_ZOOM_PINCH )
		engine->SetZoom( engine->GetZoom() *( 1.0f+delta) );
	else
		engine->SetZoom( engine->GetZoom() + delta );
}


void NavTestScene::Rotate( float degrees ) 
{
	engine->camera.Orbit( degrees );
}


void NavTestScene::Tap( int action, const grinliz::Vector2F& view, const grinliz::Ray& world )				
{
	bool uiHasTap = ProcessTap( action, view, world );
	if ( !uiHasTap ) {
		Process3DTap( action, view, world, engine );
	}
}


void NavTestScene::ItemTapped( const gamui::UIItem* item )
{
	int makeBlocks = 0;

	if ( item == &okay ) {
		game->PopScene();
	}
	else if ( item == &block ) {
		makeBlocks = 1;
	}
	else if ( item == &block20 ) {
		makeBlocks = 20;
	}

	while ( makeBlocks ) {
		Rectangle2I b = map->Bounds();
		int x = random.Rand( b.Width() );
		int y = random.Rand( b.Height() );
		if ( map->IsLand( x, y ) && !map->IsBlockSet( x, y ) ) {
			map->SetBlock( x, y );
			--makeBlocks;
		}
	}
}


void NavTestScene::Draw3D( U32 deltaTime )
{
	engine->Draw( deltaTime );
}
