/*
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "battlemechanics.h"

#include "../game/gameitem.h"
#include "../game/gamelimits.h"
#include "../game/healthcomponent.h"

#include "../xegame/chitbag.h"
#include "../xegame/chit.h"
#include "../xegame/spatialcomponent.h"
#include "../xegame/rendercomponent.h"
#include "../xegame/itemcomponent.h"
#include "../xegame/inventorycomponent.h"

#include "../grinliz/glvector.h"
#include "../grinliz/glgeometry.h"

#include "../engine/engine.h"
#include "../engine/camera.h"
#include "../engine/particle.h"

using namespace grinliz;


/*static*/ int BattleMechanics::PrimaryTeam( Chit* src )
{
	int primaryTeam = -1;
	if ( src->GetItemComponent() ) {
		primaryTeam = src->GetItemComponent()->item.primaryTeam;
	}
	return primaryTeam;
}


/*static*/ float BattleMechanics::MeleeRange( Chit* src, Chit* target )
{
	RenderComponent* srcRender = src->GetRenderComponent();
	RenderComponent* targetRender = target->GetRenderComponent();

	if ( srcRender && targetRender ) {
		float meleeRange = srcRender->RadiusOfBase()*1.5f + targetRender->RadiusOfBase();
		return meleeRange;
	}
	return 0;
}


bool BattleMechanics::InMeleeZone(	Engine* engine,
									Chit* src,
									Chit* target )
{
	ComponentSet srcComp(   src,     Chit::SPATIAL_BIT | Chit::RENDER_BIT | ComponentSet::IS_ALIVE );
	ComponentSet targetComp( target, Chit::SPATIAL_BIT | Chit::RENDER_BIT | ComponentSet::IS_ALIVE );

	if ( !srcComp.okay || !targetComp.okay )
		return false;

	// Check range up front and early out.
	const float meleeRange = MeleeRange( src, target );
	const float range = ( targetComp.spatial->GetPosition2D() - srcComp.spatial->GetPosition2D() ).Length();
	if ( range > meleeRange )
		return false;

	int test = IntersectRayCircle( targetComp.spatial->GetPosition2D(),
								   targetComp.render->RadiusOfBase(),
								   srcComp.spatial->GetPosition2D(),
								   srcComp.spatial->GetHeading2D() );

	bool intersect = ( test == INTERSECT || test == INSIDE );
	return intersect;
}


void BattleMechanics::MeleeAttack( Engine* engine, Chit* src, IMeleeWeaponItem* weapon )
{
	GLASSERT( engine && src && weapon );
	ChitBag* chitBag = src->GetChitBag();
	GLASSERT( chitBag );

	U32 absTime = chitBag->AbsTime();
	if( !weapon->Ready()) {
		return;
	}
	weapon->Use();
	
	int primaryTeam = PrimaryTeam( src );

	// Get origin and direction of melee attack,
	// then send messages to everyone hit. Everything
	// with a spatial component is tracked by the 
	// chitBag, so it's a very handy query.

	Vector2F srcPos = src->GetSpatialComponent()->GetPosition2D();
	Rectangle2F b;
	b.min = srcPos; b.max = srcPos;
	b.Outset( MELEE_RANGE + MAX_BASE_RADIUS );
	chitBag->QuerySpatialHash( &hashQuery, b, src, 0 );

	for( int i=0; i<hashQuery.Size(); ++i ) {
		Chit* target = hashQuery[i];

		// Melee damage is chaos. Don't hit your own team.
		if ( PrimaryTeam( target ) == primaryTeam ) {
			continue;
		}

		if ( InMeleeZone( engine, src, target )) {
			// FIXME: account for armor, shields, etc. etc.
			// FIXME: account for knockback (physics move), catching fire, etc.
			HealthComponent* targetHealth = GET_COMPONENT( target, HealthComponent );
			ComponentSet targetComp( target, Chit::ITEM_BIT | ComponentSet::IS_ALIVE );

			if ( targetHealth && targetComp.okay ) {
				target->SetTickNeeded();	// Fire up tick to handle health effects over time
				DamageDesc dd;
				CalcMeleeDamage( src, weapon, &dd );

				GLLOG(( "Chit %3d '%s' using '%s' hit %3d '%s'\n", 
						src->ID(), src->GetItemComponent()->GetItem()->Name(),
						weapon->GetItem()->Name(),
						target->ID(), targetComp.item->Name() ));

				target->SendMessage( ChitMsg( ChitMsg::CHIT_DAMAGE, 0, &dd ), 0 );
			}
		}
	}
}



void BattleMechanics::CalcMeleeDamage( Chit* src, IMeleeWeaponItem* weapon, DamageDesc* dd )
{
	GameItem* item = weapon->GetItem();

	InventoryComponent* inv = src->GetInventoryComponent();
	GLASSERT( inv && item );
	if ( !inv ) return;

	grinliz::CArray< GameItem*, 4 > chain;
	inv->GetChain( item, &chain );
	GLASSERT( !chain.Empty() );
	GLASSERT( chain.Size() == 2 || chain.Size() == 3 );

	DamageDesc::Vector vec = item->meleeDamage.components;
	DamageDesc::Vector handVec;
	if ( chain.Size() == 3 ) {
		handVec = chain[1]->meleeDamage.components;
	}
	// The parent item doesn't do damage. But
	// give it credit for FLAME, etc.
	GameItem* parentItem = chain[chain.Size()-1];
	DamageDesc::Vector parentVec = chain[chain.Size()-1]->meleeDamage.components;
	if ( parentItem->flags & GameItem::EFFECT_FIRE ) {
		parentVec[DamageDesc::FIRE] = 1;
	}
	
	static const float CHAIN_FRACTION = 0.5f;

	// Compute the multiplier. It is the maximum of
	// the item itself, and a percentage of the value 
	// of the chain items. So a creature of fire
	// will do some fire damage, even when using a
	// normal sword.
	for( int i=0; i<DamageDesc::NUM_COMPONENTS; ++i ) {
		vec[i] = Max( vec[i], CHAIN_FRACTION*handVec[i], CHAIN_FRACTION*parentVec[i] );
	}

	// That was the multiplier; the actual damage is based on the mass.
	// How many strikes does it take a unit of equal
	// mass to destroy a unit of the same mass?
	static const float STRIKE_RATIO = 5.0f;

	for( int i=0; i<DamageDesc::NUM_COMPONENTS; ++i ) {
		dd->components[i] = vec[i] * parentItem->mass / STRIKE_RATIO;
	}
}


void BattleMechanics::Shoot( ChitBag* bag, Chit* src, Chit* target, IRangedWeaponItem* weapon, const Vector3F& pos )
{
	GLASSERT( weapon->Ready() );
	bool okay = weapon->Use();
	if ( !okay ) {
		return;
	}
	Bolt* bolt = bag->NewBolt();
	
	Vector3F t; 
	target->GetRenderComponent()->GetMetaData( "target", &t );
	Vector3F dir = t - pos;
	dir.Normalize();

	bolt->head = pos + dir;		// FIXME: use team ignore, not offset
	bolt->len = 1.0f;
	bolt->dir = dir;
	bolt->color.Set( 1, 0, 0, 1 );	// FIXME: real color based on item
	bolt->chitID = src->ID();
	bolt->damage = weapon->GetItem()->rangedDamage.components;
}

