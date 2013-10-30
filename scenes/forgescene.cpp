#include "forgescene.h"
#include "../game/lumosgame.h"
#include "../engine/engine.h"
#include "../script/procedural.h"

using namespace gamui;
using namespace grinliz;

static const float NEAR = 0.1f;
static const float FAR  = 10.0f;


ForgeScene::ForgeScene( LumosGame* game, ForgeSceneData* data ) : Scene( game ), lumosGame( game ), screenport( game->GetScreenport())
{
	screenport.SetNearFar( NEAR, FAR );
	engine = new Engine( &screenport, lumosGame->GetDatabase(), 0 );
	engine->SetGlow( true );
	engine->LoadConfigFiles( "./res/particles.xml", "./res/lighting.xml" );

	engine->lighting.direction.Set( 2, 1, 1 );
	engine->lighting.direction.Normalize();
	
	model = 0;

	static const float D = 3; //1.5f;
	static const Vector3F out = { 1, 0, 0 };
	engine->camera.SetDir( out, V3F_UP );

	engine->camera.SetPosWC( D, 0, 0 );
	engine->CameraLookAt( engine->camera.PosWC(), V3F_ZERO );

	lumosGame->InitStd( &gamui2D, &okay, 0 );

	for( int i=0; i<NUM_ITEM_TYPES; ++i ) {
		static const char* name[NUM_ITEM_TYPES] = { "Ring", "Gun", "Shield" };
		itemType[i].Init( &gamui2D, game->GetButtonLook(0));
		itemType[i].SetText( name[i] );
		itemType[0].AddToToggleGroup( &itemType[i] );
	}
	for( int i=0; i<NUM_GUN_TYPES; ++i ) {
		static const char* name[NUM_GUN_TYPES] = { "Pistol", "Blaster", "Pulse", "Beamgun" };
		gunType[i].Init( &gamui2D, game->GetButtonLook(0));
		gunType[i].SetText( name[i] );
		gunType[0].AddToToggleGroup( &gunType[i] );
	}
	for( int i=0; i<NUM_GUN_PARTS; ++i ) {
		static const char* name[NUM_GUN_PARTS] = { "Body", "Cell", "Driver", "Scope" };
		gunParts[i].Init( &gamui2D, game->GetButtonLook(0));
		gunParts[i].SetText( name[i] );
	}

	SetModel();
}


ForgeScene::~ForgeScene()
{
	engine->FreeModel( model );
	delete engine;
}


void ForgeScene::Resize()
{
	lumosGame->PositionStd( &okay, 0 );
	LayoutCalculator layout = lumosGame->DefaultLayout();

	for( int i=0; i<NUM_ITEM_TYPES; ++i ) {
		layout.PosAbs( &itemType[i], 0, i );		
	}
	for( int i=0; i<NUM_GUN_TYPES; ++i ) {
		layout.PosAbs( &gunType[i], 1, i );		
	}
	gunParts[0].SetVisible( false );
	for( int i=1; i<NUM_GUN_PARTS; ++i ) {
		layout.PosAbs( &gunParts[i], 2, i-1 );
	}
}


void ForgeScene::SetModel()
{
	const char* gType = "pistol";
	if ( gunType[BLASTER].Down() )		gType = "blaster";
	else if ( gunType[PULSE].Down() )	gType = "pulse";
	else if ( gunType[BEAMGUN].Down() ) gType = "beamgun";

	int features = 0;
	if ( gunParts[GUN_CELL].Down() )   features |= WeaponGen::GUN_CELL;
	if ( gunParts[GUN_DRIVER].Down() ) features |= WeaponGen::GUN_DRIVER;
	if ( gunParts[GUN_SCOPE].Down() )  features |= WeaponGen::GUN_SCOPE;

	engine->FreeModel( model );
	model = engine->AllocModel( gType );
	model->SetPos( 0, 0, 0 );

	WeaponGen gen( 0, 0, features );
	ProcRenderInfo info;
	gen.AssignGun( &info );

	model->SetTextureXForm( info.te.uvXForm.x, info.te.uvXForm.y, info.te.uvXForm.z, info.te.uvXForm.w );
	model->SetTextureClip( info.te.clip.x, info.te.clip.y, info.te.clip.z, info.te.clip.w );
	model->SetColorMap( true, info.color );
	model->SetBoneFilter( info.filterName, info.filter );
}


void ForgeScene::ItemTapped( const gamui::UIItem* item )
{
	if ( item == &okay ) {
		lumosGame->PopScene();
	}
	else {
		SetModel();
	}
}


void ForgeScene::Draw3D( U32 deltaTime )
{
	// we use our own screenport
	screenport.SetPerspective();
	engine->Draw( deltaTime, 0, 0 );
	screenport.SetUI();
}




