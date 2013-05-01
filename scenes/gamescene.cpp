#include "gamescene.h"

#include "../xegame/chit.h"
#include "../xegame/spatialcomponent.h"
#include "../xegame/cameracomponent.h"
#include "../xegame/rendercomponent.h"
#include "../xegame/itemcomponent.h"

#include "../game/lumosgame.h"
#include "../game/lumoschitbag.h"
#include "../game/sim.h"
#include "../game/pathmovecomponent.h"
#include "../game/worldmap.h"
#include "../game/worldinfo.h"
#include "../game/aicomponent.h"

#include "../engine/engine.h"
#include "../engine/text.h"

#include "../script/procedural.h"

using namespace grinliz;
using namespace gamui;

static const float DEBUG_SCALE = 1.0f;
static const float MINI_MAP_SIZE = 150.0f*DEBUG_SCALE;
static const float MARK_SIZE = 6.0f*DEBUG_SCALE;

GameScene::GameScene( LumosGame* game ) : Scene( game )
{
	fastMode = false;
	simTimer = 0;
	simPS = 1.0f;
	simCount = 0;
	targetChit = 0;
	possibleChit = 0;
	infoID = 0;
	lumosGame = game;
	game->InitStd( &gamui2D, &okay, 0 );
	sim = new Sim( lumosGame );

	Load();

	Vector3F delta = { 14.0f, 14.0f, 14.0f };
	Vector3F target = { (float)sim->GetWorldMap()->Width() *0.5f, 0.0f, (float)sim->GetWorldMap()->Height() * 0.5f };
	if ( sim->GetPlayerChit() ) {
		target = sim->GetPlayerChit()->GetSpatialComponent()->GetPosition();
		CameraComponent* cc = sim->GetChitBag()->GetCamera( sim->GetEngine() );
		cc->SetTrack( sim->GetPlayerChit()->ID() );
	}
	sim->GetEngine()->CameraLookAt( target + delta, target );
	
	RenderAtom atom;
	minimap.Init( &gamui2D, atom, false );
	minimap.SetSize( MINI_MAP_SIZE, MINI_MAP_SIZE );
	minimap.SetCapturesTap( true );

	atom = lumosGame->CalcPaletteAtom( PAL_TANGERINE*2, PAL_ZERO );
	playerMark.Init( &gamui2D, atom, true );
	playerMark.SetSize( MARK_SIZE, MARK_SIZE );

	static const char* serialText[NUM_SERIAL_BUTTONS] = { "Save", "Load", "Cycle" }; 
	for( int i=0; i<NUM_SERIAL_BUTTONS; ++i ) {
		serialButton[i].Init( &gamui2D, game->GetButtonLook(0) );
		serialButton[i].SetText( serialText[i] );
	}
	static const char* camModeText[NUM_CAM_MODES] = { "Track", "Teleport" };
	for( int i=0; i<NUM_CAM_MODES; ++i ) {
		camModeButton[i].Init( &gamui2D, game->GetButtonLook(0) );
		camModeButton[i].SetText( camModeText[i] );
		camModeButton[0].AddToToggleGroup( &camModeButton[i] );
	}
	allRockButton.Init( &gamui2D, game->GetButtonLook(0) );
	allRockButton.SetText( "All Rock" );

	for( int i=0; i<NUM_NEWS_BUTTONS; ++i ) {
		newsButton[i].Init( &gamui2D, game->GetButtonLook(0) );
		newsButton[i].SetSize( NEWS_BUTTON_WIDTH, NEWS_BUTTON_HEIGHT );
		newsButton[i].SetText( "news" );
	}
	clearButton.Init( &gamui2D, game->GetButtonLook(0) );
	clearButton.SetSize( NEWS_BUTTON_WIDTH, NEWS_BUTTON_HEIGHT );
	clearButton.SetText( "Clear" );

	RenderAtom nullAtom;
	faceImage.Init( &gamui2D, nullAtom, true );
	faceImage.SetSize( 100, 100 );
	SetFace();

	RenderAtom green = LumosGame::CalcPaletteAtom( 1, 3 );	
	RenderAtom grey  = LumosGame::CalcPaletteAtom( 0, 6 );
	RenderAtom blue   = LumosGame::CalcPaletteAtom( 8, 0 );	

	healthBar.Init( &gamui2D, 10, green, grey );
	ammoBar.Init( &gamui2D, 10, blue, grey );
	shieldBar.Init( &gamui2D, 10, blue, grey );

	dateLabel.Init( &gamui2D );
	goldLabel.Init( &gamui2D );
}


GameScene::~GameScene()
{
	delete sim;
}


void GameScene::Resize()
{
	lumosGame->PositionStd( &okay, 0 );
	
	LayoutCalculator layout = static_cast<LumosGame*>(game)->DefaultLayout();
	for( int i=0; i<NUM_SERIAL_BUTTONS; ++i ) {
		layout.PosAbs( &serialButton[i], i, -2 );
	}
	for( int i=0; i<NUM_CAM_MODES; ++i ) {
		layout.PosAbs( &camModeButton[i], i, -3 );
	}
	layout.PosAbs( &allRockButton, 0, 1 );

	const Screenport& port = lumosGame->GetScreenport();
	minimap.SetPos( port.UIWidth()-MINI_MAP_SIZE, 0 );

	faceImage.SetPos( minimap.X()-faceImage.Width(), 0 );

	layout.SetSize( faceImage.Width(), 10.0f );
	layout.SetSpacing( 5.0f );
	layout.SetOffset( faceImage.X(), faceImage.Y()+faceImage.Height() );
	layout.SetGutter( 0, 5.0f );

	layout.PosAbs( &healthBar,	0, 0 );
	layout.PosAbs( &ammoBar,	0, 1 );
	layout.PosAbs( &shieldBar,  0, 2 );

	dateLabel.SetPos( faceImage.X()-faceImage.Width(), 0 );
	goldLabel.SetPos( dateLabel.X(), dateLabel.Y() + gamui2D.GetTextHeight() );

	for( int i=0; i<NUM_NEWS_BUTTONS; ++i ) {
		newsButton[i].SetPos( port.UIWidth()- (NEWS_BUTTON_WIDTH), MINI_MAP_SIZE + (NEWS_BUTTON_HEIGHT+2)*i );
	}
	clearButton.SetPos( newsButton[0].X() - clearButton.Width(), newsButton[0].Y() );

	bool visible = game->DebugUIEnabled();
	allRockButton.SetVisible( visible );
	serialButton[CYCLE].SetVisible( visible );
}


void GameScene::SetFace()
{
	Chit* chit = sim->GetPlayerChit();
	if ( chit ) {
		int id = chit->ID();
		GLASSERT( chit->GetItem() );
		const GameItem& item = *chit->GetItem();
		
		ProcRenderInfo info;
		FaceGen faceGen( true );
		faceGen.Render( id, &info );

		RenderAtom procAtom( (const void*) (UIRenderer::RENDERSTATE_UI_CLIP_XFORM_MAP), 
							 info.texture,
							 info.te.uv.x, info.te.uv.y, info.te.uv.z, info.te.uv.w );
		faceImage.SetAtom( procAtom );
		faceImage.SetVisible( true );

		uiRenderer.uv[0]			= info.te.uv;
		uiRenderer.uvClip[0]		= info.te.clip;
		uiRenderer.colorXForm[0]	= info.color;
	}
	else {
		faceImage.SetVisible( false );
	}

#if 0 
	faceImage.SetVisible( true );
	RenderAtom atom = LumosGame::CalcPaletteAtom( 0, 0 );
	faceImage.SetAtom( atom );
#endif
}


void GameScene::SetBars()
{
	RenderAtom orange = LumosGame::CalcPaletteAtom( 4, 0 );
	RenderAtom grey   = LumosGame::CalcPaletteAtom( 0, 6 );
	RenderAtom blue   = LumosGame::CalcPaletteAtom( 8, 0 );	

	Chit* chit = sim->GetPlayerChit();
	if ( chit && chit->GetItem() ) {
		const GameItem* item = chit->GetItem();
		healthBar.SetRange( item->HPFraction() );

		IShield* ishield			= 0;
		IRangedWeaponItem* iweapon	= 0;

		ItemComponent* itemComp = chit->GetItemComponent();
		if ( itemComp ) {
			ishield	= itemComp->GetShield();
			iweapon	= itemComp->GetRangedWeapon(0);
		}

		if ( iweapon ) {
			float r = 0;
			if ( iweapon->GetItem()->Reloading() ) {
				r = iweapon->GetItem()->ReloadFraction();
				ammoBar.SetLowerAtom( orange );
			}
			else {
				r = iweapon->GetItem()->RoundsFraction();
				ammoBar.SetLowerAtom( blue );
			}
			ammoBar.SetRange( Clamp( r, 0.f, 1.f ) );
		}
		else {
			ammoBar.SetRange( 0 );
		}

		if ( ishield ) {
			float r = ishield->GetItem()->RoundsFraction();
			shieldBar.SetRange( Clamp( r, 0.f, 1.0f ));
		}
		else {
			shieldBar.SetRange( 0 );
		}
	}

	healthBar.SetVisible( chit != 0 );
	ammoBar.SetVisible( chit != 0 );
	shieldBar.SetVisible( chit != 0 );
}

void GameScene::Save()
{
	const char* datPath = game->GamePath( "map", 0, "dat" );
	const char* gamePath = game->GamePath( "game", 0, "dat" );
	sim->Save( datPath, gamePath );
}


void GameScene::Load()
{
	const char* datPath  = lumosGame->GamePath( "map", 0, "dat" );
	const char* gamePath = lumosGame->GamePath( "game", 0, "dat" );

	if ( lumosGame->HasFile( gamePath )) {
		sim->Load( datPath, gamePath );
	}
	else {
		sim->Load( datPath, 0 );
	}
	SetFace();
}


void GameScene::Zoom( int style, float delta )
{
	Engine* engine = sim->GetEngine();
	if ( style == GAME_ZOOM_PINCH )
		engine->SetZoom( engine->GetZoom() *( 1.0f+delta) );
	else
		engine->SetZoom( engine->GetZoom() + delta );

}

void GameScene::Rotate( float degrees )
{
	sim->GetEngine()->camera.Orbit( degrees );
}


void GameScene::MouseMove( const grinliz::Vector2F& view, const grinliz::Ray& world )
{
	Matrix4 mvpi;
	Ray ray;
	Vector3F at, atModel;
	game->GetScreenport().ViewProjectionInverse3D( &mvpi );
	sim->GetEngine()->RayFromViewToYPlane( view, mvpi, &ray, &at );
	Model* model = sim->GetEngine()->IntersectModel( ray.origin, ray.direction, 10000.0f, TEST_HIT_AABB, 0, 0, 0, &atModel );
	MoveModel( model ? model->userData : 0 );
	if ( model && model->userData ) {
		infoID = model->userData->ID();
	}
}


void GameScene::ClearTargetFlags()
{
	Chit* target = sim->GetChitBag()->GetChit( targetChit );
	if ( target && target->GetRenderComponent() ) {
		target->GetRenderComponent()->Deco( 0, 0, 0 );
	}
	target = sim->GetChitBag()->GetChit( possibleChit );
	if ( target && target->GetRenderComponent() ) {
		target->GetRenderComponent()->Deco( 0, 0, 0 );
	}
	targetChit = possibleChit = 0;
}


void GameScene::MoveModel( Chit* target )
{
	Chit* player = sim->GetPlayerChit();
	if ( !player ) {
		ClearTargetFlags();
		return;
	}

	Chit* oldTarget = sim->GetChitBag()->GetChit( possibleChit );
	if ( oldTarget ) {
		RenderComponent* rc = oldTarget->GetRenderComponent();
		if ( rc ) {
			rc->Deco( 0, 0, 0 );
		}
	}
	Chit* focusedTarget = sim->GetChitBag()->GetChit( targetChit );
	if ( target && target != focusedTarget ) {
		AIComponent* ai = GET_COMPONENT( player, AIComponent );
		if ( ai && ai->GetTeamStatus( target ) == AIComponent::ENEMY ) {
			possibleChit = 0;
			RenderComponent* rc = target->GetRenderComponent();
			if ( rc ) {
				rc->Deco( "possibleTarget", RenderComponent::DECO_FOOT, INT_MAX );
				possibleChit = target->ID();
			}
		}
	}
}


void GameScene::TapModel( Chit* target )
{
	if ( !target ) {
		return;
	}
	Chit* player = sim->GetPlayerChit();
	if ( !player ) {
		ClearTargetFlags();

		AIComponent* ai = GET_COMPONENT( target, AIComponent );
		if ( ai ) {
			ai->EnableDebug( true );
		}
		return;
	}
	AIComponent* ai = GET_COMPONENT( player, AIComponent );
	if ( ai && ai->GetTeamStatus( target ) == AIComponent::ENEMY ) {
		ai->FocusedTarget( target );
		ClearTargetFlags();

		RenderComponent* rc = target->GetRenderComponent();
		if ( rc ) {
			rc->Deco( "target", RenderComponent::DECO_FOOT, INT_MAX );
			targetChit = target->ID();
		}
	}
}


void GameScene::Tap( int action, const grinliz::Vector2F& view, const grinliz::Ray& world )
{
	bool uiHasTap = ProcessTap( action, view, world );
	Engine* engine = sim->GetEngine();
	enable3DDragging = (sim->GetPlayerChit() == 0);

	if ( !uiHasTap ) {
		bool tap = Process3DTap( action, view, world, sim->GetEngine() );

		if ( tap ) {
			Matrix4 mvpi;
			Ray ray;
			Vector3F at, atModel;
			game->GetScreenport().ViewProjectionInverse3D( &mvpi );
			sim->GetEngine()->RayFromViewToYPlane( view, mvpi, &ray, &at );
			Model* model = sim->GetEngine()->IntersectModel( ray.origin, ray.direction, 10000.0f, TEST_HIT_AABB, 0, 0, 0, &atModel );
			
			// FIXME: need a generic solution here. How to handle stuff that isn't tappable?
			if ( model && model->userData && LumosChitBag::GoldFilter( model->userData )) {
				model = 0;	// don't tap on gold.
			}

			int tapMod = lumosGame->GetTapMod();

			if ( tapMod == 0 ) {
				Chit* chit = sim->GetPlayerChit();
				if ( model ) {
					TapModel( model->userData );
				}
				else if ( chit ) {
					Vector2F dest = { at.x, at.z };
					DoDestTapped( dest );
				}
				else {
					sim->GetEngine()->CameraLookAt( at.x, at.z );
				}
			}
			else if ( tapMod == GAME_TAP_MOD_CTRL ) {

				Vector2I v = { (int)at.x, (int)at.z };
				sim->CreatePlayer( v, 0 ); 
				SetFace();
#if 0
				sim->CreateVolcano( (int)at.x, (int)at.z, 6 );
				sim->CreatePlant( (int)at.x, (int)at.z, -1 );
#endif
			}
			else if ( tapMod == GAME_TAP_MOD_SHIFT ) {
				for( int i=0; i<NUM_PLANT_TYPES; ++i ) {
					sim->CreatePlant( (int)at.x+i, (int)at.z, i );
				}
			}
		}
	}
}


void GameScene::ItemTapped( const gamui::UIItem* item )
{
	Vector2F dest = { -1, -1 };

	if ( item == &okay ) {
		game->PopScene();
	}
	else if ( item == &minimap ) {
		float x=0, y=0;
		gamui2D.GetRelativeTap( &x, &y );
		GLOUTPUT(( "minimap tapped nx=%.1f ny=%.1f\n", x, y ));

		Engine* engine = sim->GetEngine();
		dest.x = x*(float)engine->GetMap()->Width();
		dest.y = y*(float)engine->GetMap()->Height();
	}
	else if ( item == &serialButton[SAVE] ) {
		Save();
	}
	else if ( item == &serialButton[LOAD] ) {
		delete sim;
		sim = new Sim( lumosGame );
		Load();
	}
	else if ( item == &serialButton[CYCLE] ) {
		Save();
		delete sim;
		sim = new Sim( lumosGame );
		Load();
	}
	else if ( item == &allRockButton ) {
		sim->SetAllRock();
	}

	for( int i=0; i<NUM_NEWS_BUTTONS; ++i ) {
		if ( item == &newsButton[i] ) {

			const NewsEvent* ne = sim->GetChitBag()->News() + i;
			if ( !sim->GetPlayerChit() && ne->chitID ) {
				CameraComponent* cc = sim->GetChitBag()->GetCamera( sim->GetEngine() );
				if ( cc ) {
					cc->SetTrack( ne->chitID );
				}
			}
			else {
				GLASSERT( i < sim->GetChitBag()->NumNews() );
				dest = ne->pos;
			}
		}
	}
	if ( item == &clearButton ) {
		if ( !sim->GetPlayerChit() ) {
			CameraComponent* cc = sim->GetChitBag()->GetCamera( sim->GetEngine() );
			if ( cc  ) {
				cc->SetTrack( 0 );
			}
		}
	}

	if ( dest.x >= 0 ) {
		DoDestTapped( dest );
	}
}


void GameScene::DoDestTapped( const Vector2F& _dest )
{
	Vector2F dest = _dest;

	Chit* chit = sim->GetPlayerChit();
	if ( chit ) {
		if ( camModeButton[TRACK].Down() ) {
			AIComponent* ai = GET_COMPONENT( chit, AIComponent );
			GLASSERT( ai );
			if ( ai ) {
				Vector2F pos = chit->GetSpatialComponent()->GetPosition2D();
				// Is this grid travel or normal travel?
				Vector2I currentSector = SectorData::SectorID( pos.x, pos.y );
				Vector2I destSector    = SectorData::SectorID( dest.x, dest.y );
				Vector2I sector = { 0, 0 };
					
				if ( currentSector != destSector ) {
					// Find the nearest port.
					int id = chit->ID();
					Rectangle2I portRect = sim->GetWorldMap()->NearestPort( pos );
					if ( portRect.max.x > 0 && portRect.max.y > 0 ) {
						dest = SectorData::PortPos( portRect, chit->ID() );
						sector = destSector;
					}
				}

				ai->FocusedMove( dest, sector.IsZero() ? 0 : &sector );
			}
		}
		else if ( camModeButton[TELEPORT].Down() ) {
			SpatialComponent* sc = chit->GetSpatialComponent();
			GLASSERT( sc );
			sc->SetPosition( dest.x, 0, dest.y );
		}
	}
	else {
		sim->GetEngine()->CameraLookAt( dest.x, dest.y );
	}
}



void GameScene::HandleHotKey( int mask )
{
	if ( mask == GAME_HK_SPACE ) {
		fastMode = !fastMode;
	}
	else if ( mask == GAME_HK_TOGGLE_PATHING ) {
		sim->GetWorldMap()->ShowRegionOverlay( !sim->GetWorldMap()->IsShowingRegionOverlay() );
	}
	else {
		super::HandleHotKey( mask );
	}
}

/*
class NewsSorter
{
public:
	static bool Less( const NewsEvent& v0, const NewsEvent& v1 )	
	{ 
		int score0 = v0.priority * 30*1000 - (int)v0.time;
		int score1 = v1.priority * 30*1000 - (int)v1.time;
		return score0 < score1; 
	}
};
*/

void GameScene::DoTick( U32 delta )
{
	clock_t startTime = clock();

	sim->DoTick( delta );
	++simCount;

	if ( fastMode ) {
		while( clock() - startTime < 100 ) {
			sim->DoTick( 30 );
			++simCount;
		}
	}

	const NewsEvent* news = sim->GetChitBag()->News();
	int nNews = sim->GetChitBag()->NumNews();

#if 0
	for( int i=0; i<nNews; ++i ) {
		//float h = sim->GetEngine()->camera.PosWC().y;
		if (    news[i].processed == false 
			 && news[i].name == "SectorHerd" 
			 && !sim->GetPlayerChit() ) 
		{
			CameraComponent* cc = sim->GetChitBag()->GetCamera( sim->GetEngine() );
			if ( cc ) {
				cc->SetTrack( 0 );
				cc->SetTrack( news[i].chitID );
			}
		}
	}
#endif
	sim->GetChitBag()->SetNewsProcessed();
	/*
	localNews.Clear();
	for( int i=0; i<nNews; ++i ) {
		localNews.Push( news[i] );
	}
	Sort<NewsEvent, NewsSorter>( localNews.Mem(), localNews.Size() );
	*/
	for( int i=0; i<NUM_NEWS_BUTTONS; ++i ) {
		if ( i < nNews ) {
			//newsButton[i].SetText( localNews[i].name.c_str() );
			newsButton[i].SetText( news[i].name.c_str() );
			newsButton[i].SetEnabled( true );
		}
		else {
			newsButton[i].SetText( "" );
			newsButton[i].SetEnabled( false );
		}
	}

	clock_t endTime = clock();
	simTimer += (int)(endTime-startTime);

	Vector3F lookAt = { 0, 0, 0 };
	sim->GetEngine()->CameraLookingAt( &lookAt );
	Vector2I sector = { (int)lookAt.x / SECTOR_SIZE, (int)lookAt.z / SECTOR_SIZE };
	const SectorData& sd = sim->GetWorldMap()->GetWorldInfo().GetSector( sector );

	CStr<18> str;
	str.Format( "%.2f %s", sim->DateInAge(), sd.name.c_str() );
	dateLabel.SetText( str.c_str() );

	Chit* playerChit = sim->GetPlayerChit();
	str.Clear();
	int gold = 0;
	if ( playerChit && playerChit->GetItemComponent() ) {
		gold = playerChit->GetItemComponent()->Gold();
	}
	str.Format( "Au:%d", gold );
	goldLabel.SetText( str.c_str() );

	SetBars();
}


void GameScene::Draw3D( U32 deltaTime )
{
	sim->Draw3D( deltaTime );

	RenderAtom atom( (const void*)UIRenderer::RENDERSTATE_UI_NORMAL_OPAQUE, 
					 (const void*)sim->GetMiniMapTexture(), 
					 0, 0, 1, 1 );
	/* coordinate test. they are correct.
	RenderAtom atom( (const void*)UIRenderer::RENDERSTATE_UI_NORMAL_OPAQUE, 
					 (const void*)TextureManager::Instance()->GetTexture( "palette" ),
					 0, 0, 1, 1 );
	*/

	minimap.SetAtom( atom );
	Vector3F lookAt = { 0, 0, 0 };
	sim->GetEngine()->CameraLookingAt( &lookAt );

	Chit* chit = sim->GetPlayerChit();
	if ( chit && chit->GetSpatialComponent() ) {
		lookAt = chit->GetSpatialComponent()->GetPosition();
	}

	Map* map = sim->GetEngine()->GetMap();
	
	float x = minimap.X() + Lerp( 0.f, minimap.Width(),  lookAt.x / (float)map->Width() );
	float y = minimap.Y() + Lerp( 0.f, minimap.Height(), lookAt.z / (float)map->Height() );

	playerMark.SetCenterPos( x, y );
}


void GameScene::DrawDebugText()
{
	DrawDebugTextDrawCalls( 16, sim->GetEngine() );

	UFOText* ufoText = UFOText::Instance();
	Chit* chit = sim->GetPlayerChit();
	Engine* engine = sim->GetEngine();
	LumosChitBag* chitBag = sim->GetChitBag();
	Vector3F at = { 0, 0, 0 };
	int y = 32;

	if ( chit && chit->GetSpatialComponent() ) {
		const Vector3F& v = chit->GetSpatialComponent()->GetPosition();
		at = v;
		ufoText->Draw( 0, y, "Player: %.1f, %.1f, %.1f  Camera: %.1f %.1f %.1f", 
			           v.x, v.y, v.z,
					   engine->camera.PosWC().x, engine->camera.PosWC().y, engine->camera.PosWC().z );
		y += 16;
	}
	else {
		engine->CameraLookingAt( &at );
	}

	if ( simTimer > 1000 ) {
		simPS = 1000.0f * (float)simCount / (float)simTimer;
		simTimer = 0;
		simCount = 0;
	}

	ufoText->Draw( 0, y,	"Date=%.2f %s mode. Sim/S=%.1f x%.1f ticks=%d/%d", 
							sim->DateInAge(),
							fastMode ? "fast" : "normal", 
							simPS,
							simPS / 30.0f,
							chitBag->NumTicked(), chitBag->NumChits() ); 
	y += 16;

	int typeCount[NUM_PLANT_TYPES];
	for( int i=0; i<NUM_PLANT_TYPES; ++i ) {
		typeCount[i] = 0;
		for( int j=0; j<MAX_PLANT_STAGES; ++j ) {
			typeCount[i] += chitBag->census.plants[i][j];
		}
	}
	int stageCount[MAX_PLANT_STAGES];
	for( int i=0; i<MAX_PLANT_STAGES; ++i ) {
		stageCount[i] = 0;
		for( int j=0; j<NUM_PLANT_TYPES; ++j ) {
			stageCount[i] += chitBag->census.plants[j][i];
		}
	}

	ufoText->Draw( 0, y,	"Plants type: %d %d %d %d %d %d %d %d stage: %d %d %d %d", 
									typeCount[0], typeCount[1], typeCount[2], typeCount[3], typeCount[4], typeCount[5], typeCount[6], typeCount[7],
									stageCount[0], stageCount[1], stageCount[2], stageCount[3] );
	y += 16;

	micropather::CacheData cacheData;
	Vector2I sector = { (int)at.x/SECTOR_SIZE, (int)at.z/SECTOR_SIZE };
	sim->GetWorldMap()->PatherCacheHitMiss( sector, &cacheData );
	ufoText->Draw( 0, y, "Pather(%d,%d) kb=%d/%d %.2f cache h:m=%d:%d %.2f", 
						  sector.x, sector.y,
						  cacheData.nBytesUsed/1024,
						  cacheData.nBytesAllocated/1024,
						  cacheData.memoryFraction,
						  cacheData.hit,
						  cacheData.miss,
						  cacheData.hitFraction );
	y += 16;

	Chit* info = sim->GetChitBag()->GetChit( infoID );
	if ( info ) {
		GLString str;
		info->DebugStr( &str );
		ufoText->Draw( 0, y, "id=%d: %s", infoID, str.c_str() );
		y += 16;
	}
}
