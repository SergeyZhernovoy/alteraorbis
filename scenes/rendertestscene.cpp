#include "rendertestscene.h"
#include "../game/lumosgame.h"
#include "../gamui/gamui.h"
#include "../engine/engine.h"
#include "../xegame/testmap.h"

using namespace gamui;
using namespace tinyxml2;

RenderTestScene::RenderTestScene( LumosGame* game, const RenderTestSceneData* data ) : Scene( game ), lumosGame( game )
{
	engine = new Engine( game->GetScreenportMutable(), game->GetDatabase() );

	for( int i=0; i<NUM_MODELS; ++i )
		model[i] = 0;

	testMap = new TestMap( 8, 8 );
	engine->SetMap( testMap );
	
	switch( data->id ) {
	case 0:
		SetupTest0();
		break;
	case 1:
		SetupTest1();
		break;
	default:
		GLASSERT( 0 );
		break;
	};
	LayoutCalculator layout = lumosGame->DefaultLayout();

	lumosGame->InitStd( &gamui2D, &okay, 0 );
	whiteButton.Init( &gamui2D, game->GetButtonLook( LumosGame::BUTTON_LOOK_STD ) );
	whiteButton.SetSize( layout.Width(), layout.Height() );
	whiteButton.SetText( "white" );

	hemiButton.Init( &gamui2D, game->GetButtonLook( LumosGame::BUTTON_LOOK_STD ));
	hemiButton.SetSize( layout.Width(), layout.Height() );
	hemiButton.SetText( "hemisph" );

	refreshButton.Init( &gamui2D, game->GetButtonLook( LumosGame::BUTTON_LOOK_STD ));
	refreshButton.SetSize( layout.Width(), layout.Height() );
	refreshButton.SetText( "refresh" );

	textBox.Init( &gamui2D );
	textBox.SetSize( 400, 100 );

	RenderAtom nullAtom;
	rtImage.Init( &gamui2D, nullAtom, true );
	rtImage.SetSize( 400.f, 200.f );
}


RenderTestScene::~RenderTestScene()
{
	for( int i=0; i<NUM_MODELS; ++i ) {
		engine->FreeModel( model[i] );
	}
	delete testMap;
	delete engine;
}


void RenderTestScene::Resize()
{
	lumosGame->PositionStd( &okay, 0 );
	
	LayoutCalculator layout = lumosGame->DefaultLayout();
	layout.PosAbs( &whiteButton, 1, -1 );
	layout.PosAbs( &hemiButton, 2, -1 );
	layout.PosAbs( &refreshButton, 3, -1 );

	textBox.SetPos( okay.X(), okay.Y()-100 );
}



void RenderTestScene::SetupTest0()
{
	const ModelResource* res0 = ModelResourceManager::Instance()->GetModelResource( "humanFemale" );
	const ModelResource* res1 = ModelResourceManager::Instance()->GetModelResource( "humanMale" );
	for( int i=0; i<NUM_MODELS; ++i ) {
		model[i] = engine->AllocModel( i<3 ? res0 : res1 );
		model[i]->SetPos( 1, 0, (float)i );
		model[i]->SetRotation( (float)(-i*10) );
	}
	engine->CameraLookAt( 0, (float)(NUM_MODELS/2), 12 );

	textBox.SetText( "DC = ( 2fem + 2male ) * ( 1color + 1shadow) + 2map = 10. 'u' disable ui" ); 
}


void RenderTestScene::SetupTest1()
{
	const ModelResource* testStruct0Res = ModelResourceManager::Instance()->GetModelResource( "testStruct0" );
	const ModelResource* testStruct1Res = ModelResourceManager::Instance()->GetModelResource( "testStruct1" );

	model[0] = engine->AllocModel( testStruct0Res );
	model[0]->SetPos( 0.5f, 0.0f, 1.0f );

	model[1] = engine->AllocModel( testStruct1Res );
	model[1]->SetPos( 0.5f, 0.0f, 3.5f );

	engine->CameraLookAt( 0, 3, 8, -45.f, -30.f );
}


void RenderTestScene::ItemTapped( const gamui::UIItem* item )
{
	if ( item == &okay ) {
		game->PopScene();
	}
	else if ( item == &whiteButton ) {
		Texture* white = TextureManager::Instance()->GetTexture( "white" );
		for( int i=0; i<NUM_MODELS; ++i ) {
			if ( model[i] ) {
				model[i]->SetTexture( white );
			}
		}
	}
	else if ( item == &hemiButton ) {
		GLOUTPUT(( "Set light model: %s\n", hemiButton.Down() ? "down" : "up" ));
		engine->lighting.hemispheric = hemiButton.Down();
		LoadLighting();
	}
	else if ( item == &refreshButton ) {
		LoadLighting();
	}
}


void RenderTestScene::LoadLighting()
{
	XMLDocument doc;
	doc.LoadFile( "./resin/lighting.xml" );

	if ( engine->lighting.hemispheric ) {
		const XMLElement* ele = doc.FirstChildElement( "lighting" )->FirstChildElement( "hemispherical" );
		engine->lighting.Load( ele );
	}
	else {
		const XMLElement* ele = doc.FirstChildElement( "lighting" )->FirstChildElement( "lambert" );
		engine->lighting.Load( ele );
	}
}


void RenderTestScene::Zoom( int style, float delta )
{
	if ( style == GAME_ZOOM_PINCH )
		engine->SetZoom( engine->GetZoom() *( 1.0f+delta) );
	else
		engine->SetZoom( engine->GetZoom() + delta );
}


void RenderTestScene::Rotate( float degrees ) 
{
	engine->camera.Orbit( degrees );
}


void RenderTestScene::HandleHotKeyMask( int mask )
{
	switch( mask ) {
	case GAME_HK_TOGGLE_UI:
		{
			bool visible = !okay.Visible();
			okay.SetVisible( visible );
			whiteButton.SetVisible( visible );
			textBox.SetVisible( visible );
		}
		break;
	}
}


void RenderTestScene::Draw3D( U32 deltaTime )
{
	engine->Draw( deltaTime );
	
	RenderAtom atom( (const void*)UIRenderer::RENDERSTATE_UI_NORMAL_OPAQUE, (const void*)engine->GetRenderTargetTexture(), 0, 0, 1, 1 );
	rtImage.SetAtom( atom );
}
