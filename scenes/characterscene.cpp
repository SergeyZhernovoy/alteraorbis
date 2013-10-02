#include "characterscene.h"
#include "../game/lumosgame.h"
#include "../engine/engine.h"
#include "../xegame/itemcomponent.h"
#include "../script/procedural.h"
#include "../script/battlemechanics.h"

using namespace gamui;
using namespace grinliz;

static const float NEAR = 0.1f;
static const float FAR  = 10.0f;

CharacterScene::CharacterScene( LumosGame* game, CharacterSceneData* csd ) : Scene( game ), screenport( game->GetScreenport() )
{
	this->lumosGame = game;
	this->itemComponent = csd->itemComponent;
	model = 0;

	screenport.SetNearFar( NEAR, FAR );
	engine = new Engine( &screenport, lumosGame->GetDatabase(), 0 );
	engine->SetGlow( true );
	engine->LoadConfigFiles( "./res/particles.xml", "./res/lighting.xml" );

	game->InitStd( &gamui2D, &okay, 0 );
	dropButton.Init( &gamui2D, lumosGame->GetButtonLook(0));
	dropButton.SetText( "Drop" );

	for( int i=0; i<NUM_ITEM_BUTTONS; ++i ) {
		itemButton[i].Init( &gamui2D, lumosGame->GetButtonLook(0) );
		itemButton[0].AddToToggleGroup( &itemButton[i] );
	}

	for( int i=0; i<NUM_TEXT_KV; ++i ) {
		textKey[i].Init( &gamui2D );
		textVal[i].Init( &gamui2D );
	}

	float size = gamui2D.GetTextHeight() * 2.0f;
	for( int i=0; i<NUM_CRYSTAL_TYPES+1; ++i ) {
		moneyText[i].Init( &gamui2D );

		static const char* NAME[NUM_CRYSTAL_TYPES+1] = {
			"au",
			"crystalGreen",
			"crystalRed",
			"crystalBlue",
			"crystalViolet"
		};
		RenderAtom atom = LumosGame::CalcUIIconAtom( NAME[i], true );
		moneyImage[i].Init( &gamui2D, atom, true );
		moneyImage[i].SetSize( size, size );
	}
	CStr<64> str;
	str.Format( "%d", itemComponent->GetWallet().gold );
	moneyText[0].SetText( str.c_str() );
	for( int i=0; i<NUM_CRYSTAL_TYPES; ++i ) {
		str.Format( "%d", itemComponent->GetWallet().crystal[i] );
		moneyText[i+1].SetText( str.c_str() );
	}

	engine->lighting.direction.Set( 0, 1, 1 );
	engine->lighting.direction.Normalize();
	
	engine->camera.SetPosWC( 2, 2, 2 );
	static const Vector3F out = { 1, 0, 0 };
	engine->camera.SetDir( out, V3F_UP );

	itemButton[0].SetDown();
	SetButtonText();
}


CharacterScene::~CharacterScene()
{
	engine->FreeModel( model );
	delete engine;
}


void CharacterScene::Resize()
{
	lumosGame->PositionStd( &okay, 0 );

	LayoutCalculator layout = lumosGame->DefaultLayout();
	layout.PosAbs( &itemButton[0], 0, 0 );

	float space = gamui2D.GetTextHeight() * 2.0f;
	float dy = 0.5f * ( moneyImage[0].Height() - moneyText[0].Height() );

	layout.PosAbs( &moneyImage[0], 2, 0, false );
	moneyText[0].SetPos( moneyImage[0].X() + moneyImage[0].Width(), moneyImage[0].Y()+dy );

	for( int i=0; i<NUM_CRYSTAL_TYPES; ++i ) {
		moneyImage[i+1].SetPos( moneyText[i].X() + moneyText[i].Width() + space, moneyImage[0].Y() );
		moneyText[i+1].SetPos(  moneyImage[i+1].X() + moneyImage[i+1].Width(),	 moneyText[0].Y() );
	}

	int col=0;
	int row=0;
	for( int i=1; i<NUM_ITEM_BUTTONS; ++i ) {
		layout.PosAbs( &itemButton[i], col, row+1 );
		++col;
		if ( col == 3 ) {
			++row;
			col = 0;
		}
	}
	layout.PosAbs( &dropButton, 0, row+1 );

	layout.SetSize( layout.Width(), layout.Height()*0.5f );
	for( int i=0; i<NUM_TEXT_KV; ++i ) {
		layout.PosAbs( &textKey[i], -4, i );
		layout.PosAbs( &textVal[i], -2, i );
	}
}


void CharacterScene::SetItemInfo( const GameItem* item, const GameItem* user )
{
	for( int i=0; i<NUM_TEXT_KV; ++i ) {
		textKey[i].SetText( "" );
		textVal[i].SetText( "" );
	}

	if ( !item )
		return;

	CStr< 32 > str;

	textVal[KV_NAME].SetText( item->ProperName() ? item->ProperName() : item->Name() );
	textKey[KV_NAME].SetText( "Name" );

	str.Format( "%d", item->id );
	textVal[KV_ID].SetText( str.c_str() );
	textKey[KV_ID].SetText( "ID" );

	str.Format( "%d", item->traits.Level() );
	textVal[KV_LEVEL].SetText( str.c_str() );
	textKey[KV_LEVEL].SetText( "Level" );

	str.Format( "%d", item->traits.Experience() );
	textVal[KV_XP].SetText( str.c_str() );
	textKey[KV_XP].SetText( "XP" );

	/*			Ranged		Melee	Shield
		STR		Dam			Dam		Capacity
		WILL	EffRange	D/TU	Reload
		CHR		Clip/Reload	x		x
		INT		Ranged D/TU x		x
		DEX		Melee D/TU	x		x
	*/

	int i = KV_STR;
	if ( item->ToRangedWeapon() ) {
		textKey[i].SetText( "Ranged Damage" );
		str.Format( "%.1f", item->traits.Damage() * item->rangedDamage );
		textVal[i++].SetText( str.c_str() );

		textKey[i].SetText( "Effective Range" );
		float radAt1 = BattleMechanics::ComputeRadAt1( user, item->ToRangedWeapon(), false, false );
		str.Format( "%.1f", BattleMechanics::EffectiveRange( radAt1 ));
		textVal[i++].SetText( str.c_str() );

		textKey[i].SetText( "Clip/Reload" );
		str.Format( "%d / %.1f", item->clipCap, 0.001f * (float)item->reload.Threshold() );
		textVal[i++].SetText( str.c_str() );

		textKey[i].SetText( "Ranged D/TU" );
		str.Format( "%.1f", BattleMechanics::RangedDPTU( item->ToRangedWeapon(), false ));
		textVal[i++].SetText( str.c_str() );
	}
	if ( item->ToMeleeWeapon() ) {
		textKey[i].SetText( "Melee Damage" );
		DamageDesc dd;
		BattleMechanics::CalcMeleeDamage( user, item->ToMeleeWeapon(), &dd );
		str.Format( "%.1f", dd.damage );
		textVal[i++].SetText( str.c_str() );

		textKey[i].SetText( "Melee D/TU" );
		str.Format( "%.1f", BattleMechanics::MeleeDPTU( user, item->ToMeleeWeapon() ));
		textVal[i++].SetText( str.c_str() );

		float boost = BattleMechanics::ComputeShieldBoost( item->ToMeleeWeapon() );
		if ( boost > 1.0f ) {
			textKey[i].SetText( "Shield Boost" );
			str.Format( "%.1f", boost );
			textVal[i++].SetText( str.c_str() );
		}
	}
	if ( item->ToShield() ) {
		textKey[i].SetText( "Capacity" );
		str.Format( "%d", item->clipCap );
		if ( i<NUM_TEXT_KV ) textVal[i++].SetText( str.c_str() );

		textKey[i].SetText( "Reload" );
		str.Format( "%.1f", 0.001f * (float)item->reload.Threshold() );
		if ( i<NUM_TEXT_KV ) textVal[i++].SetText( str.c_str() );
	}

	if ( !(item->ToMeleeWeapon() || item->ToShield() || item->ToRangedWeapon() )) {
		str.Format( "%d", item->traits.Strength() );
		textKey[KV_STR].SetText( "Strength" );
		textVal[KV_STR].SetText( str.c_str() );

		str.Format( "%d", item->traits.Will() );
		textKey[KV_WILL].SetText( "Will" );
		textVal[KV_WILL].SetText( str.c_str() );

		str.Format( "%d", item->traits.Charisma() );
		textKey[KV_CHR].SetText( "Charisma" );
		textVal[KV_CHR].SetText( str.c_str() );

		str.Format( "%d", item->traits.Intelligence() );
		textKey[KV_INT].SetText( "Intelligence" );
		textVal[KV_INT].SetText( str.c_str() );

		str.Format( "%d", item->traits.Dexterity() );
		textKey[KV_DEX].SetText( "Dexterity" );
		textVal[KV_DEX].SetText( str.c_str() );
		i = KV_DEX+1;
	}

	MicroDBIterator it( item->microdb );
	for( ; !it.Done() && i < NUM_TEXT_KV; it.Next() ) {
		
		const char* key = it.Key();
		if ( it.NumSub() == 1 && it.SubType(0) == 'd' ) {
			textKey[i].SetText( key );
			str.Format( "%d", it.Int(0) );
			textVal[i++].SetText( str.c_str() );
		}
	}
}


void CharacterScene::SetButtonText()
{
	int count=0;
	const GameItem* down = 0;
	const GameItem* mainItem = itemComponent->GetItem(0);
	const IRangedWeaponItem* ranged = itemComponent->GetRangedWeapon(0);
	const IMeleeWeaponItem*  melee  = itemComponent->GetMeleeWeapon();
	const IShield*           shield = itemComponent->GetShield();
	const GameItem* rangedItem = ranged ? ranged->GetItem() : 0;
	const GameItem* meleeItem  = melee  ? melee->GetItem() : 0;
	const GameItem* shieldItem = shield ? shield->GetItem() : 0;

	RenderAtom nullAtom;
	RenderAtom iconAtom = LumosGame::CalcUIIconAtom( "okay" );
	memset( itemButtonIndex, 0, sizeof(itemButtonIndex[0])*NUM_ITEM_BUTTONS );

	for( int i=0; i<NUM_ITEM_BUTTONS; ++i ) {
		const GameItem* item = itemComponent->GetItem(i);
		if ( item && !item->Intrinsic() ) {

			// Set the text to the proper name, if we have it.
			// Then an icon for what it is, and a check
			// mark if the object is in use.
			lumosGame->ItemToButton( item, &itemButton[count] );
			itemButtonIndex[count] = i;

			// Set the "active" icons.
			if ( item == rangedItem || item == meleeItem || item == shieldItem ) {
				itemButton[count].SetIcon( iconAtom, iconAtom );
			}
			else {
				itemButton[count].SetIcon( nullAtom, nullAtom );
			}

			if ( itemButton[count].Down() ) {
				down = item;
			}
			++count;
		}
	}
	for( ; count<NUM_ITEM_BUTTONS; ++count ) {
		itemButton[count].SetText( "" );
		itemButton[count].SetIcon( nullAtom, nullAtom );
		itemButton[count].SetDeco( nullAtom, nullAtom );
	}

	if ( down ) {
		if ( model && !StrEqual( model->GetResource()->Name(), down->ResourceName() )) {
			engine->FreeModel( model );
			model = 0;
		}
		if ( !model ) {
			model = engine->AllocModel( down->ResourceName() );
			model->SetPos( 0,0,0 );

			Rectangle3F aabb = model->AABB();
			float size = Max( aabb.SizeX(), Max( aabb.SizeY(), aabb.SizeZ() ));
			float d = 1.5f + size;
			engine->camera.SetPosWC( d, d, d );
			engine->CameraLookAt( engine->camera.PosWC(), aabb.Center() );

			if ( !down->GetValue( "procedural" ).empty() ) {
				ProcRenderInfo info;
				AssignProcedural( down->GetValue( "procedural" ).c_str(), strstr( down->Name(), "emale" )!=0, down->id, down->primaryTeam, false, down->flags & GameItem::EFFECT_MASK, &info );
				model->SetTextureXForm( info.te.uvXForm );
				model->SetColorMap( true, info.color );
				model->SetBoneFilter( info.filterName, info.filter );
			}
		}
	}

	if ( down != itemComponent->GetItem(0)) {
		SetItemInfo( down, itemComponent->GetItem(0) );
	}
	else {
		SetItemInfo( down, 0 );
	}
}


void CharacterScene::ItemTapped( const gamui::UIItem* item )
{
	if ( item == &okay ) {
		lumosGame->PopScene();
	}
	for( int i=0; i<NUM_ITEM_BUTTONS; ++i ) {
		if ( item == &itemButton[i] ) {
			SetButtonText();
		}
	}
}

void CharacterScene::DoTick( U32 deltaTime )
{
}
	
void CharacterScene::Draw3D( U32 deltaTime )
{
	// we use our own screenport
	screenport.SetPerspective();
	engine->Draw( deltaTime, 0, 0 );
	screenport.SetUI();
}


gamui::RenderAtom CharacterScene::DragStart( const gamui::UIItem* item )
{
	RenderAtom atom, nullAtom;
	for( int i=0; i<NUM_ITEM_BUTTONS; ++i ) {
		if ( &itemButton[i] == item ) {

			itemButton[i].GetDeco( &atom, 0 );
			if ( !atom.Equal( nullAtom ) ) {
				itemButton[i].SetDeco( nullAtom, nullAtom );
			}
			return atom;
		}
	}
	return nullAtom;
}


void CharacterScene::DragEnd( const gamui::UIItem* start, const gamui::UIItem* end )
{
	int startIndex = 0;
	int endIndex   = 0;
	for( int i=0; i<NUM_ITEM_BUTTONS; ++i ) {
		if ( start == &itemButton[i] ) {
			startIndex = itemButtonIndex[i];
			break;
		}
	}
	for( int i=0; i<NUM_ITEM_BUTTONS; ++i ) {
		if ( end == &itemButton[i] ) {
			endIndex = itemButtonIndex[i];
			break;
		}
	}

	if ( startIndex && endIndex ) {
		itemComponent->Swap( startIndex, endIndex );
	}
	if ( start && startIndex && end == &dropButton ) {
		itemComponent->Drop( itemComponent->GetItem( startIndex ));
	}

	SetButtonText();
}
