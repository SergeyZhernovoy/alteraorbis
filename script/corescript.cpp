#include "corescript.h"

#include "../grinliz/glvector.h"

#include "../game/census.h"
#include "../game/lumoschitbag.h"
#include "../game/mapspatialcomponent.h"
#include "../game/worldmap.h"
#include "../game/aicomponent.h"

#include "../xegame/chit.h"
#include "../xegame/spatialcomponent.h"

using namespace grinliz;

CoreScript::CoreScript( WorldMap* map ) : worldMap( map ), spawnTick( 10*1000 )
{
}


CoreScript::~CoreScript()
{
}


void CoreScript::Init( const ScriptContext& ctx )
{
	ctx.census->cores.Push( ctx.chit->ID() );
	spawnTick.Randomize( ctx.chit->ID() );
}


void CoreScript::Serialize( const ScriptContext& ctx, XStream* xs )
{
	XarcOpen( xs, ScriptName() );
	spawnTick.Serialize( xs, "spawn" );
	XarcClose( xs );
}


void CoreScript::OnAdd( const ScriptContext& ctx )
{
	ctx.census->cores.Push( ctx.chit->ID() );
	// Cores are indestructable. They don't get removed.
}


static bool Accept( Chit* c ) 
{
	AIComponent* ai = GET_COMPONENT( c, AIComponent );
	return ai != 0;
}

int CoreScript::DoTick( const ScriptContext& ctx, U32 delta, U32 since )
{
	static const int RADIUS = 4;

	if ( spawnTick.Delta( since )) {
		// spawn stuff.
		MapSpatialComponent* ms = ( GET_COMPONENT( ctx.chit, MapSpatialComponent ));
		GLASSERT( ms );
		Vector2I pos = ms->MapPosition();
		pos.x += -RADIUS + ctx.chit->random.Rand( RADIUS*2 );
		pos.y += -RADIUS + ctx.chit->random.Rand( RADIUS*2 );

		if ( worldMap->Bounds().Contains( pos ) && worldMap->IsPassable( pos.x, pos.y )) {
			Rectangle2F r;
			r.Set( (float)pos.x, (float)(pos.y), (float)(pos.x+1), (float)(pos.y+1) );
			const CDynArray<Chit*>& arr = ctx.chit->GetChitBag()->QuerySpatialHash( r, 0, Accept );
			if ( arr.Empty() ) {
				Vector3F pf = { (float)pos.x+0.5f, 0, (float)pos.y+0.5f };
				// FIXME: proper team
				// FIXME safe downcast
				((LumosChitBag*)(ctx.chit->GetChitBag()))->NewMonsterChit( pf, "mantis", 100 );
			}
		}
	}
	return spawnTick.Next();
}


