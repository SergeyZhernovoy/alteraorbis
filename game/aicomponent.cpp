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

#include "aicomponent.h"
#include "worldmap.h"
#include "gamelimits.h"
#include "pathmovecomponent.h"
#include "gameitem.h"
#include "lumoschitbag.h"
#include "mapspatialcomponent.h"
#include "gridmovecomponent.h"
#include "sectorport.h"
#include "workqueue.h"
#include "team.h"
#include "circuitsim.h"
#include "reservebank.h"
#include "sim.h"	// FIXME: where shourd the Web live??

// move to tasklist file
#include "lumoschitbag.h"
#include "lumosgame.h"

#include "../scenes/characterscene.h"
#include "../scenes/forgescene.h"

#include "../script/battlemechanics.h"
#include "../script/plantscript.h"
#include "../script/worldgen.h"
#include "../script/corescript.h"
#include "../script/itemscript.h"
#include "../script/buildscript.h"

#include "../engine/engine.h"
#include "../engine/particle.h"

#include "../audio/xenoaudio.h"

#include "../xegame/chitbag.h"
#include "../xegame/spatialcomponent.h"
#include "../xegame/rendercomponent.h"
#include "../xegame/itemcomponent.h"
#include "../xegame/istringconst.h"

#include "../grinliz/glrectangle.h"
#include "../grinliz/glarrayutil.h"

#include "../Shiny/include/Shiny.h"
#include <climits>

#include "../ai/tasklist.h"
#include "../ai/marketai.h"
#include "../ai/domainai.h"

using namespace grinliz;
using namespace ai;

static const float	NORMAL_AWARENESS			= 10.0f;
static const float	LOOSE_AWARENESS = LONGEST_WEAPON_RANGE;
static const float	SHOOT_ANGLE					= 10.0f;	// variation from heading that we can shoot
static const float	SHOOT_ANGLE_DOT				=  0.985f;	// same number, as dot product.
static const float	WANDER_RADIUS				=  5.0f;
static const float	EAT_HP_PER_SEC				=  2.0f;
static const float	EAT_HP_HEAL_MULT			=  5.0f;	// eating really tears up plants. heal the critter more than damage the plant.
static const float  CORE_HP_PER_SEC				=  8.0f;
static const int	WANDER_ODDS					=100;		// as in 1 in WANDER_ODDS
static const int	GREATER_WANDER_ODDS			=  5;		// as in 1 in WANDER_ODDS
static const float	PLANT_AWARE					=  3;
static const float	GOLD_AWARE					=  5.0f;
static const float	FRUIT_AWARE					=  5.0f;
static const float  EAT_WILD_FRUIT				= 0.70f;
static const int	FORCE_COUNT_STUCK			=  8;
static const int	STAND_TIME_WHEN_WANDERING	= 1500;
static const int	RAMPAGE_THRESHOLD			= 40;		// how many times a destination must be blocked before rampage
static const int	GUARD_RANGE					= 1;
static const int	GUARD_TIME					= 10*1000;
static const double	NEED_CRITICAL				= 0.1;
static const int	BUILD_TIME					= 1000;
static const int	REPAIR_TIME					= 4000;

const char* AIComponent::MODE_NAMES[NUM_MODES]     = { "normal", "rampage", "battle" };
const char* AIComponent::ACTION_NAMES[NUM_ACTIONS] = { "none", "move", "melee", "shoot", "wander", "stand" };

Vector2I ToWG(int id) {
	Vector2I v = { 0, 0 };
	if (id < 0) {
		int i = -id;
		v.y   = i / MAX_MAP_SIZE;
		v.x   = i - v.y*MAX_MAP_SIZE;
	}
	return v;
}

inline int ToWG(const grinliz::Vector2I& v) {
	return -(v.y * MAX_MAP_SIZE + v.x);
}


template<int EXCLUDED>
bool FEFilter(Chit* parentChit, int id) {
		const ChitContext* context = parentChit->Context();
		if (id < 0) {
			Vector2I p = ToWG(id);
			const WorldGrid& wg = context->worldMap->GetWorldGrid(p);
			return wg.Plant() || wg.RockHeight();
		}
		Chit* chit = context->chitBag->GetChit(id);
		if (!chit) return false;
		if (chit->Destroyed()) return false;

		SpatialComponent* sc = chit->GetSpatialComponent();
		SpatialComponent* parentSC = parentChit->GetSpatialComponent();
		GLASSERT(sc && parentSC);
		float range2 = (sc->GetPosition() - parentSC->GetPosition()).LengthSquared();

		return (chit != parentChit) 
			&& (range2 < LOOSE_AWARENESS * LOOSE_AWARENESS)
			&& (sc->GetSector() == parentSC->GetSector()) 
			&& (Team::GetRelationship(chit, parentChit) != EXCLUDED);
}


AIComponent::AIComponent() : feTicker( 750 ), needsTicker( 1000 )
{
	currentAction = 0;
	focus = 0;
	aiMode = NORMAL_MODE;
	wanderTime = 0;
	rethink = 0;
	fullSectorAware = false;
	debugLog = false;
	visitorIndex = -1;
	rampageTarget = 0;
	destinationBlocked = 0;
	lastTargetID = 0;
}


AIComponent::~AIComponent()
{
}


void AIComponent::Serialize( XStream* xs )
{
	this->BeginSerialize( xs, Name() );
	XARC_SER( xs, aiMode );
	XARC_SER( xs, currentAction );
	XARC_SER(xs, lastTargetID);
//	XARC_SER_DEF( xs, targetDesc.id, 0 );
//	XARC_SER( xs, targetDesc.mapPos );
	XARC_SER_DEF( xs, focus, 0 );
	XARC_SER_DEF( xs, wanderTime, 0 );
	XARC_SER( xs, rethink );
	XARC_SER_DEF( xs, fullSectorAware, false );
	XARC_SER_DEF( xs, visitorIndex, -1 );
	XARC_SER_DEF( xs, rampageTarget, 0 );
	XARC_SER_DEF( xs, destinationBlocked, 0 );
	XARC_SER( xs, lastGrid );
	XARC_SER_VAL_CARRAY(xs, friendList2);
	XARC_SER_VAL_CARRAY(xs, enemyList2);
	feTicker.Serialize( xs, "feTicker" );
	needsTicker.Serialize( xs, "needsTicker" );
	needs.Serialize( xs );
	this->EndSerialize( xs );
}


void AIComponent::OnAdd( Chit* chit, bool init )
{
	super::OnAdd( chit, init );
	if (init) {
		feTicker.SetPeriod(750 + (chit->ID() & 128));
	}
	const ChitContext* context = Context();
	taskList.Init( context );
}


void AIComponent::OnRemove()
{
	super::OnRemove();
}


bool AIComponent::LineOfSight( const ComponentSet& thisComp, Chit* t, const RangedWeapon* weapon )
{
	Vector3F origin, dest;
	GLASSERT( weapon );

	ComponentSet target( t, Chit::SPATIAL_BIT | Chit::RENDER_BIT | ComponentSet::IS_ALIVE );
	if ( !target.okay ) {
		return false;
	}
	thisComp.render->GetMetaData( HARDPOINT_TRIGGER, &origin );
	//target.render->GetMetaData( META_TARGET, &dest );
	dest = EnemyPos(target.chit->ID(), true);

	Vector3F dir = dest - origin;
	float length = dir.Length() + 0.01f;	// a little extra just in case
	CArray<const Model*, RenderComponent::NUM_MODELS+1> ignore, targetModels;
	thisComp.render->GetModelList( &ignore );

	const ChitContext* context = Context();
	// FIXME: was TEST_TRI, which is less accurate. But also fundamentally incorrect, since
	// the TEST_TRI doesn't account for bone xforms. Deep bug - but surprisingly hard to see
	// in the game. Switched to the faster TEST_HIT_AABB, but it would be nice to clean
	// up TEST_TRI and just make it fast.
	ModelVoxel mv = context->engine->IntersectModelVoxel( origin, dir, length, TEST_HIT_AABB, 0, 0, ignore.Mem() );
	if ( mv.model ) {
		target.render->GetModelList( &targetModels );
		return targetModels.Find( mv.model ) >= 0;
	}
	return false;
}


bool AIComponent::LineOfSight( const ComponentSet& thisComp, const grinliz::Vector2I& mapPos )
{
	RenderComponent* rc = ParentChit()->GetRenderComponent();
	GLASSERT(rc);
	if (!rc) return false;

	Vector3F origin;
	rc->CalcTrigger(&origin, 0);

	CArray<const Model*, RenderComponent::NUM_MODELS+1> ignore;
	thisComp.render->GetModelList( &ignore );

	Vector3F dest = { (float)mapPos.x+0.5f, 0.5f, (float)mapPos.y+0.5f };
	Vector3F dir = dest - origin;
	float length = dir.Length() + 0.01f;	// a little extra just in case

	const ChitContext* context = Context();
	ModelVoxel mv = context->engine->IntersectModelVoxel( origin, dir, length, TEST_TRI, 0, 0, ignore.Mem() );

	// A little tricky; we hit the 'mapPos' if nothing is hit (which gets to the center)
	// or if voxel at that pos is hit.
	if ( !mv.Hit() || ( mv.Hit() && mv.Voxel2() == mapPos )) {
		return true;
	}
	return false;
}


void AIComponent::MakeAware( const int* enemyIDs, int n )
{
	for( int i=0; i<n && enemyList2.HasCap(); ++i ) {
		int id = enemyIDs[i];
		// Be careful to not duplicate list entries:
		if (enemyList2.Find(id) >= 0) continue;

		Chit* chit = Context()->chitBag->GetChit( enemyIDs[i] );
		if ( chit ) {
			int status = Team::GetRelationship( chit, parentChit );
			if ( status == RELATE_ENEMY ) {
				if (FEFilter<RELATE_FRIEND>(parentChit, id)) {
					enemyList2.Push( id );
				}
			}
		}
	}
}


Vector3F AIComponent::EnemyPos(int id, bool target)
{
	Vector3F pos = { 0, 0, 0 };
	if (id > 0) {
		Chit* chit = Context()->chitBag->GetChit(id);
		if (chit) {
			if (target) {
				RenderComponent* rc = chit->GetRenderComponent();
				GLASSERT(rc);
				if (rc) {
					// The GetMetaData is expensive, because
					// of the animation xform. Cheat a bit;
					// use the base xform.
					const ModelMetaData* metaData = rc->MainResource()->GetMetaData(META_TARGET);
					pos = rc->MainModel()->XForm() * metaData->pos;
					return pos;
				}
			}
			else {
				return chit->GetSpatialComponent()->GetPosition();
			}
		}
	}
	else if (id < 0) {
		Vector2I v2i = ToWG(id);
		if (target)
			pos.Set((float)v2i.x + 0.5f, 0.5f, (float)v2i.y + 0.5f);
		else
			pos.Set((float)v2i.x + 0.5f, 0.0f, (float)v2i.y + 0.5f);
		return pos;
	}
	return pos;
}


void AIComponent::ProcessFriendEnemyLists(bool tick)
{
	SpatialComponent* sc = parentChit->GetSpatialComponent();
	if ( !sc ) return;
	Vector2F center = sc->GetPosition2D();
	Vector2I sector = ToSector(center);

	// Clean the lists we have.
	// Enemy list: not sure the best polity. Right now keeps.
	// Friend list: clear and focus on near.
	int target = enemyList2.Empty() ? -1 : enemyList2[0];
	if (tick) friendList2.Clear();

	enemyList2.Filter(parentChit, [](Chit* parentChit, int id) {
		return FEFilter<RELATE_FRIEND>(parentChit, id);
	});

	friendList2.Filter(parentChit, [](Chit* parentChit, int id) {
		return FEFilter<RELATE_ENEMY>(parentChit, id);
	});

	// Did we lose our focused target?
	if (focus == FOCUS_TARGET) {
		if (enemyList2.Empty() || (focus != enemyList2[0])) {
			focus = FOCUS_NONE;
		}
	}

	// Compute the area for the query.
	Rectangle2F zone;
	zone.min = zone.max = center;
	zone.Outset( fullSectorAware ? SECTOR_SIZE : NORMAL_AWARENESS );

	if ( Context()->worldMap->UsingSectors() ) {
		Rectangle2F rf = ToWorld( SectorData::InnerSectorBounds( center.x, center.y ));
		zone.DoIntersection( rf );
	}

	if (tick) {
		CChitArray chitArr;

		MOBIshFilter mobFilter;
		BuildingFilter buildingFilter;
		ItemNameFilter coreFilter(ISC::core, IChitAccept::MAP);
		// Order matters: prioritize mobs, then a core, then buildings.
		IChitAccept* filters[3] = { &mobFilter, &coreFilter, &buildingFilter };

		for (int pass = 0; pass < 3; ++pass) {
			// Add extra friends or enemies into the list.
			Context()->chitBag->QuerySpatialHash(&chitArr, zone, parentChit, filters[pass]);
			for (int i = 0; i < chitArr.Size(); ++i) {
				int status = Team::GetRelationship(parentChit, chitArr[i]);
				int id = chitArr[i]->ID();

				if (status == RELATE_ENEMY  && enemyList2.HasCap() && (enemyList2.Find(id) < 0))  {
					if (   fullSectorAware 
						|| Context()->worldMap->HasStraightPath(center, chitArr[i]->GetSpatialComponent()->GetPosition2D())) 
					{
						if (FEFilter<RELATE_FRIEND>(parentChit, id)) {
							enemyList2.Push(id);
						}
					}
				}
				else if (pass == 0 && status == RELATE_FRIEND && friendList2.HasCap() && (friendList2.Find(id) < 0)) {
					if (FEFilter<RELATE_ENEMY>(parentChit, id)) {
						friendList2.Push(id);
					}
				}
			}
		}
	}
}


class ChitDistanceCompare
{
public:
	ChitDistanceCompare( const Vector3F& _origin ) : origin(_origin) {}
	bool Less( Chit* v0, Chit* v1 ) const
	{
#if 0
		// This has a nasty rounding bug
		// where 2 "close" things would always evaluate "less than" and
		// the sort flips endlessly.
		return ( v0->GetSpatialComponent()->GetPosition() - origin ).LengthSquared() <
			   ( v1->GetSpatialComponent()->GetPosition() - origin ).LengthSquared();
#endif
		Vector3F p0 = v0->GetSpatialComponent()->GetPosition() - origin;
		Vector3F p1 = v1->GetSpatialComponent()->GetPosition() - origin;
		float len0 = p0.LengthSquared();
		float len1 = p1.LengthSquared();
		return len0 < len1;
	}

private:
	Vector3F origin;
};

Chit* AIComponent::Closest( const ComponentSet& thisComp, Chit* arr[], int n, Vector2F* outPos, float* distance )
{
	float best = FLT_MAX;
	Chit* chit = 0;
	Vector3F pos = thisComp.spatial->GetPosition();

	for( int i=0; i<n; ++i ) {
		Chit* c = arr[i];
		SpatialComponent* sc = c->GetSpatialComponent();
		if ( sc ) {
			float len2 = (sc->GetPosition() - pos).LengthSquared();
			if ( len2 < best ) {
				best = len2;
				chit = c;
			}
		}
	}
	if ( distance ) {
		*distance = chit ? sqrtf( best ) : 0;
	}
	if ( outPos ) {
		if ( chit ) {
			*outPos = chit->GetSpatialComponent()->GetPosition2D();
		}
		else {
			outPos->Zero();
		}
	}
	return chit;
}


void AIComponent::DoMove( const ComponentSet& thisComp )
{
	PathMoveComponent* pmc = GET_SUB_COMPONENT( parentChit, MoveComponent, PathMoveComponent );
	if ( !pmc || pmc->ForceCountHigh() || pmc->Stopped() ) {
		currentAction = NO_ACTION;
		return;
	}

	// Generally speaking, moving is done by the PathMoveComponent. When
	// not in battle, this is essentially "do nothing." If in battle mode,
	// we look for opportunity fire and such.
	if ( aiMode != BATTLE_MODE ) {
		// Check for motion done, stuck, etc.
		if ( pmc->Stopped() || pmc->ForceCountHigh() ) {
			currentAction = NO_ACTION;
			return;
		}
		if (aiMode == RAMPAGE_MODE && RampageDone(thisComp)) {
			aiMode = NORMAL_MODE;
			currentAction = NO_ACTION;
			return;
		}
		// Reloading is always a good background task.
		RangedWeapon* rangedWeapon = thisComp.itemComponent->GetRangedWeapon( 0 );
		if ( rangedWeapon && rangedWeapon->CanReload() ) {
			rangedWeapon->Reload( parentChit );
		}
	}
	else {
		// Battle mode! Run & Gun
		float utilityRunAndGun = 0.0f;
		Chit* targetRunAndGun = 0;

		RangedWeapon* rangedWeapon = thisComp.itemComponent->GetRangedWeapon( 0 );
		if ( rangedWeapon ) {
			Vector3F heading = thisComp.spatial->GetHeading();
			bool explosive = (rangedWeapon->flags & GameItem::EFFECT_EXPLOSIVE) != 0;

			if ( rangedWeapon->CanShoot() ) {
				float radAt1 = BattleMechanics::ComputeRadAt1( thisComp.chit->GetItem(), 
															   rangedWeapon,
															   true,
															   true );	// Doesn't matter to utility.

				for( int k=0; k<enemyList2.Size(); ++k ) {
					ComponentSet enemy( Context()->chitBag->GetChit(enemyList2[k]), Chit::SPATIAL_BIT | Chit::ITEM_BIT | ComponentSet::IS_ALIVE );
					if ( !enemy.okay ) {
						continue;
					}
					Vector3F p0;
					Vector3F p1 = BattleMechanics::ComputeLeadingShot( thisComp.chit, enemy.chit, rangedWeapon->BoltSpeed(), &p0 );
					Vector3F normal = p1 - p0;
					normal.Normalize();
					float range = (p0-p1).Length();

					if ( explosive && range < EXPLOSIVE_RANGE*3.0f ) {
						// Don't run & gun explosives if too close.
						continue;
					}

					if ( DotProduct( normal, heading ) >= SHOOT_ANGLE_DOT ) {
						// Wow - can take the shot!
						float u = BattleMechanics::ChanceToHit( range, radAt1 );

						if ( u > utilityRunAndGun ) {
							utilityRunAndGun = u;
							targetRunAndGun = enemy.chit;
						}
					}
				}
			}
			float utilityReload = 0.0f;
			if ( rangedWeapon->CanReload() ) {
				utilityReload = 1.0f - rangedWeapon->RoundsFraction();
			}
			if ( utilityReload > 0 || utilityRunAndGun > 0 ) {
				if ( debugLog ) {
					GLOUTPUT(( "ID=%d Move: RunAndGun=%.2f Reload=%.2f ", thisComp.chit->ID(), utilityRunAndGun, utilityReload ));
				}
				if ( utilityRunAndGun > utilityReload ) {
					GLASSERT( targetRunAndGun );
					Vector3F leading = BattleMechanics::ComputeLeadingShot( thisComp.chit, targetRunAndGun, rangedWeapon->BoltSpeed(), 0 );
					BattleMechanics::Shoot(	Context()->chitBag, 
											thisComp.chit, 
											leading, 
											targetRunAndGun->GetMoveComponent() ? targetRunAndGun->GetMoveComponent()->IsMoving() : false,
											rangedWeapon );
					if ( debugLog ) {
						GLOUTPUT(( "->RunAndGun\n" ));
					}
				}
				else {
					rangedWeapon->Reload( parentChit );
					if ( debugLog ) {
						GLOUTPUT(( "->Reload\n" ));
					}
				}
			}
		}
	}
}


void AIComponent::DoShoot( const ComponentSet& thisComp )
{
	bool pointed = false;
	Vector3F leading = { 0, 0, 0 };
	bool isMoving = false;
	RangedWeapon* weapon = thisComp.itemComponent->GetRangedWeapon( 0 );
	// FIXME: real bug here, although maybe minor. serialization in 
	// ItemComponent() calls UseBestItems(), which can re-order weapons,
	// which (I think) causes the ranged weapon to be !current.
	// It will eventually reset. But annoying and may be occasionally visible.
	//GLASSERT(weapon);
	if (!weapon) return;
	if (enemyList2.Empty()) return;	// no target

	int targetID = enemyList2[0];
	GLASSERT(targetID != 0);
	if (targetID == 0) return;

	if ( targetID > 0 ) {
		Chit* targetChit = Context()->chitBag->GetChit(targetID);
		GLASSERT(targetChit);
		if (!targetChit) return;

		leading = BattleMechanics::ComputeLeadingShot( thisComp.chit, targetChit, weapon->BoltSpeed(), 0 );
		isMoving = targetChit->GetMoveComponent() ? targetChit->GetMoveComponent()->IsMoving() : false;
	}
	else {
		Vector2I p = ToWG(targetID);
		leading.Set( (float)p.x + 0.5f, 0.5f, (float)p.y + 0.5f );
	}

	Vector2F leading2D = { leading.x, leading.z };
	// Rotate to target.
	Vector2F heading = thisComp.spatial->GetHeading2D();
	Vector2F normalToTarget = leading2D - thisComp.spatial->GetPosition2D();
	float distanceToTarget = normalToTarget.Length();
	normalToTarget.Normalize();
	float dot = DotProduct( heading, normalToTarget );

	if ( dot >= SHOOT_ANGLE_DOT ) {
		// all good.
	}
	else {
		// Rotate to target.
		PathMoveComponent* pmc = GET_SUB_COMPONENT( parentChit, MoveComponent, PathMoveComponent );
		if ( pmc ) {
			//float angle = RotationXZDegrees( normalToTarget.x, normalToTarget.y );
			pmc->QueueDest( thisComp.spatial->GetPosition2D(), &normalToTarget );
		}
		return;
	}

	if ( weapon ) {
		if ( weapon->HasRound() ) {
			// Has round. May be in cooldown.
			if ( weapon->CanShoot() ) {
				BattleMechanics::Shoot(	Context()->chitBag, 
										parentChit, 
										leading,
										isMoving,
										weapon );
			}
		}
		else {
			weapon->Reload( parentChit );
			// Out of ammo - do something else.
			currentAction = NO_ACTION;
		}
	}
}


void AIComponent::DoMelee( const ComponentSet& thisComp )
{
	if (enemyList2.Empty()) return;
	int targetID = enemyList2[0];

	MeleeWeapon* weapon = thisComp.itemComponent->GetMeleeWeapon();
	Chit* targetChit = Context()->chitBag->GetChit(targetID);
	Vector2I mapPos = ToWG(targetID);

	const ChitContext* context = Context();
	if ( !weapon ) {
		currentAction = NO_ACTION;
		return;
	}
	PathMoveComponent* pmc = GET_SUB_COMPONENT( parentChit, MoveComponent, PathMoveComponent );

	// Are we close enough to hit? Then swing. Else move to target.
	if ( targetChit && BattleMechanics::InMeleeZone( context->engine, parentChit, targetChit )) {
		GLASSERT( parentChit->GetRenderComponent()->AnimationReady() );
		parentChit->GetRenderComponent()->PlayAnimation( ANIM_MELEE );
		IString sound = weapon->keyValues.GetIString(ISC::sound);
		if (!sound.empty() && XenoAudio::Instance()) {
			XenoAudio::Instance()->PlayVariation(sound, weapon->ID(), &thisComp.spatial->GetPosition());
		}

		Vector2F pos2 = thisComp.spatial->GetPosition2D();
		Vector2F heading = targetChit->GetSpatialComponent()->GetPosition2D() - pos2;
		heading.Normalize();

		float angle = RotationXZDegrees(heading.x, heading.y);
		// The PMC seems like a good idea...BUT the PMC sends back destination
		// reached messages. Which is a good thing, but causes the logic
		// to reset. Go for the expedient solution: insta-turn for melee.
		//if ( pmc ) pmc->QueueDest( pos2, &heading );
		thisComp.spatial->SetYRotation(angle);
		if (pmc) pmc->Stop();
	}
	else if ( targetID < 0 && BattleMechanics::InMeleeZone( context->engine, parentChit, mapPos )) {
		GLASSERT( parentChit->GetRenderComponent()->AnimationReady() );
		parentChit->GetRenderComponent()->PlayAnimation( ANIM_MELEE );
		IString sound = weapon->keyValues.GetIString(ISC::sound);
		if (!sound.empty() && XenoAudio::Instance()) {
			XenoAudio::Instance()->PlayVariation(sound, weapon->ID(), &thisComp.spatial->GetPosition());
		}

		Vector2F pos2 = thisComp.spatial->GetPosition2D();
		Vector2F heading = ToWorld2F( mapPos ) - pos2;
		heading.Normalize();

		float angle = RotationXZDegrees(heading.x, heading.y);
		// The PMC seems like a good idea...BUT the PMC sends back destination
		// reached messages. Which is a good thing, but causes the logic
		// to reset. Go for the expedient solution: insta-turn for melee.
		//if ( pmc ) pmc->QueueDest( pos2, &heading );
		thisComp.spatial->SetYRotation(angle);
		if ( pmc ) pmc->Stop();
	}
	else {
		// Move to target.
		if ( pmc ) {
			Vector2F targetPos = ToWorld2F(EnemyPos(targetID, false));

			Vector2F pos = thisComp.spatial->GetPosition2D();
			Vector2F dest = { -1, -1 };
			pmc->QueuedDest( &dest );

			float delta = ( dest-targetPos ).Length();
			float distanceToTarget = (targetPos - pos).Length();

			// If the error is greater than distance to, then re-path.
			if ( delta > distanceToTarget * 0.25f ) {
				pmc->QueueDest( targetPos );
			}
		}
		else {
			currentAction = NO_ACTION;
			return;
		}
	}
}


bool AIComponent::DoStand( const ComponentSet& thisComp, U32 time )
{
	const GameItem* item	= parentChit->GetItem();
	if (!item) {
		return false;	// defensive - code analysis.
	}

	int itemFlags			= item->flags;
	double totalHP			= double(item->TotalHP());
	int tick = 400;
	const ChitContext* context = Context();

#if 0
	// Plant eater
	if (	!thisComp.move->IsMoving()    
		 && (item->hp < totalHP ))  
	{
		if ( itemFlags & GameItem::AI_EAT_PLANTS ) {
			// Are we on a plant?
			Vector2I pos2i = thisComp.spatial->GetPosition2DI();
			const WorldGrid& wg = Context()->worldMap->GetWorldGrid(pos2i);
			// Note that currently only support eating stage 0-1 plants.
			if (wg.Plant() && !wg.BlockingPlant()) {
				// We are standing on a plant.
				float hp = Travel( EAT_HP_PER_SEC, time );
				ChitMsg heal( ChitMsg::CHIT_HEAL );
				heal.dataF = hp * EAT_HP_HEAL_MULT;

				if ( debugLog ) {
					GLOUTPUT(( "ID=%d Eating plants itemHp=%.1f total=%.1f hp=%.1f\n", thisComp.chit->ID(),
						item->hp, item->TotalHP(), hp ));
				}

				DamageDesc dd( hp, 0 );
				Context()->worldMap->VoxelHit(pos2i, dd);
				parentChit->SendMessage( heal, this );
				return true;
			}
		}
		if ( itemFlags & GameItem::AI_HEAL_AT_CORE ) {
			// Are we on a core?
			Vector2I pos2i = thisComp.spatial->GetPosition2DI();
			Vector2I sector = { pos2i.x/SECTOR_SIZE, pos2i.y/SECTOR_SIZE };
			const SectorData& sd = context->worldMap->GetSectorData( sector );
			if ( sd.core == pos2i ) {
				ChitMsg heal( ChitMsg::CHIT_HEAL );
				heal.dataF = Travel( CORE_HP_PER_SEC, time );
				parentChit->SendMessage( heal, this );
				return true;
			}
		}
	}
#endif	
	if (visitorIndex >= 0 && !thisComp.move->IsMoving())
	{
		// Visitors at a kiosk.
		Vector2I pos2i = thisComp.spatial->GetPosition2DI();
		Vector2I sector = ToSector(pos2i);
		Chit* chit = this->Context()->chitBag->QueryPorch(pos2i, 0);
		CoreScript* cs = CoreScript::GetCore(sector);

		VisitorData* vd = &Visitors::Instance()->visitorData[visitorIndex];

		if (chit && chit->GetItem()->IName() == ISC::kiosk) {
			vd->kioskTime += time;
			if (vd->kioskTime > VisitorData::KIOSK_TIME) {
				vd->visited.Push(sector);
				cs->AddTech();
				thisComp.render->AddDeco("techxfer", STD_DECO);
				vd->kioskTime = 0;
				currentAction = NO_ACTION;	// done here - move on!
				return false;
			}
			// else keep standing.
			return true;
		}
		else {
			// Oops...
			currentAction = NO_ACTION;
			return false;
		}
	}
	// FIXME: there are 2 stand functions. The AIComponent one and
	// the TaskList one. Need to sort that out. But don't call
	// DoStanding here.
	//	return taskList.DoStanding( thisComp, time );
	return false;
}


void AIComponent::OnChitEvent( const ChitEvent& event )
{
	ComponentSet thisComp( parentChit, Chit::RENDER_BIT | 
		                               Chit::SPATIAL_BIT |
									   ComponentSet::IS_ALIVE |
									   ComponentSet::NOT_IN_IMPACT );
	if ( !thisComp.okay ) {
		return;
	}

	super::OnChitEvent( event );
}


void AIComponent::Think(const ComponentSet& thisComp)
{
	if (visitorIndex >= 0) {
		ThinkVisitor(thisComp);
		return;
	}

	switch (aiMode) {
		case NORMAL_MODE:		ThinkNormal(thisComp);	break;
		case BATTLE_MODE:		ThinkBattle(thisComp);	break;
		case RAMPAGE_MODE:		ThinkRampage(thisComp);	break;
	}
}


bool AIComponent::TravelHome(const ComponentSet& thisComp, bool focus)
{
	CoreScript* cs = CoreScript::GetCoreFromTeam(parentChit->Team());
	if (!cs) return false;

	Vector2I dstSector = cs->ParentChit()->GetSpatialComponent()->GetSector();
	const SectorData& dstSD = Context()->worldMap->GetSectorData(dstSector);

	SectorPort dstSP;
	dstSP.sector = dstSector;
	dstSP.port = dstSD.NearestPort(cs->ParentChit()->GetSpatialComponent()->GetPosition2D());

	return Move(dstSP, focus);
}


bool AIComponent::Move( const SectorPort& sp, bool focused )
{
	PathMoveComponent* pmc    = GET_SUB_COMPONENT( parentChit, MoveComponent, PathMoveComponent );
	SpatialComponent*  sc	  = parentChit->GetSpatialComponent();
	const ChitContext* context = Context();
	if ( pmc && sc ) {
		// Read our destination port information:
		const SectorData& sd = context->worldMap->GetSectorData( sp.sector );
				
		// Read our local get-on-the-grid info
		SectorPort local = context->worldMap->NearestPort( sc->GetPosition2D() );
		// Completely possible this chit can't actually path anywhere.
		if ( local.IsValid() ) {
			const SectorData& localSD = context->worldMap->GetSectorData( local.sector );
			// Local path to remote dst
			Vector2F dest2 = SectorData::PortPos( localSD.GetPortLoc(local.port), parentChit->ID() );
			pmc->QueueDest( dest2, 0, &sp );
			currentAction = MOVE;
			focus = focused ? FOCUS_MOVE : 0;
			rethink = 0;
			return true;
		}
	}
	return false;
}


void AIComponent::Stand()
{
	if ( aiMode != BATTLE_MODE ) {
		PathMoveComponent* pmc    = GET_SUB_COMPONENT( parentChit, MoveComponent, PathMoveComponent );
		if ( pmc ) {
			pmc->Stop();
		}
		currentAction = STAND;
		rethink = 0;
	}
}


void AIComponent::Pickup( Chit* item )
{
	taskList.Push( Task::MoveTask( item->GetSpatialComponent()->GetPosition2D() ));
	taskList.Push( Task::PickupTask( item->ID() ));
}


void AIComponent::Move( const grinliz::Vector2F& dest, bool focused, const Vector2F* normal )
{
	PathMoveComponent* pmc    = GET_SUB_COMPONENT( parentChit, MoveComponent, PathMoveComponent );
	if ( pmc ) {
		pmc->QueueDest( dest, normal );
		currentAction = MOVE;
		focus = focused ? FOCUS_MOVE : 0;
		rethink = 0;
	}
}


bool AIComponent::TargetAdjacent(const grinliz::Vector2I& pos, bool focused)
{
	static const Vector2I DELTA[4] = { { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 } };
	for (int i = 0; i < 4; ++i) {
		Vector2I v = pos + DELTA[i];
		const WorldGrid& wg = Context()->worldMap->GetWorldGrid(v);
		if (wg.BlockingPlant() || wg.RockHeight()) {
			Target(v, focused);
			return true;
		}
	}
	return false;
}


void AIComponent::Target(Chit* chit, bool focused) 
{
	Target(chit->ID(), focused);
}


void AIComponent::Target(const Vector2I& pos, bool focused) 
{
	Target(ToWG(pos), focused);
}


void AIComponent::Target( int id, bool focused )
{
	if ( aiMode != BATTLE_MODE ) {
		aiMode = BATTLE_MODE;
		if ( parentChit->GetRenderComponent() ) {
			parentChit->GetRenderComponent()->AddDeco( "attention", STD_DECO );
		}
	}
	int idx = enemyList2.Find(id);
	if (idx >= 0) {
		Swap(&enemyList2[idx], &enemyList2[0]);
	}
	else {
		enemyList2.Insert(0, id);
	}
	enemyList2.Insert(0, id);
	focus = focused ? FOCUS_TARGET : 0;
}


Chit* AIComponent::GetTarget()
{
	if (!enemyList2.Empty()) {
		return Context()->chitBag->GetChit(enemyList2[0]);
	}
	return 0;
}


WorkQueue* AIComponent::GetWorkQueue()
{
	SpatialComponent* sc = parentChit->GetSpatialComponent();
	if ( !sc )
		return 0;

	Vector2I sector = sc->GetSector();
	CoreScript* coreScript = CoreScript::GetCore(sector);
	if ( !coreScript )
		return 0;

	// Again, bitten: workers aren't citizens. Grr.
	if ((parentChit->GetItem()->flags & GameItem::AI_DOES_WORK)
		|| coreScript->IsCitizen(parentChit->ID()))
	{
		WorkQueue* workQueue = coreScript->GetWorkQueue();
		return workQueue;
	}
	return 0;
}


void AIComponent::Rampage( int dest ) 
{ 
	rampageTarget = dest;

	ComponentSet thisComp(parentChit, Chit::RENDER_BIT |
		Chit::SPATIAL_BIT |
		Chit::ITEM_BIT |
		ComponentSet::IS_ALIVE |
		ComponentSet::NOT_IN_IMPACT);
	GLASSERT(thisComp.okay && !RampageDone(thisComp));

	aiMode = RAMPAGE_MODE; 
	currentAction = NO_ACTION;

	NewsEvent news( NewsEvent::RAMPAGE, parentChit->GetSpatialComponent()->GetPosition2D(), 
				    parentChit->GetItemID(), 0, parentChit->Team() );
	Context()->chitBag->GetNewsHistory()->Add( news );	
}


// Function to compute "should we rampage." Rampage logic is in ThinkRampage()
bool AIComponent::ThinkDoRampage( const ComponentSet& thisComp )
{
	if ( destinationBlocked < RAMPAGE_THRESHOLD ) 
		return false;
	const ChitContext* context = Context();

	// Need a melee weapon to rampage. Ranged is never used..
	const MeleeWeapon* melee = thisComp.itemComponent->QuerySelectMelee();
	if (!melee)	return false;

	// Workers teleport. Rampaging was annoying.
	if (thisComp.item->flags & GameItem::AI_DOES_WORK) {
		Vector2I pos2i = thisComp.spatial->GetPosition2DI();
		CoreScript* cs = CoreScript::GetCore(ToSector(pos2i));
		if (cs) {
			Vector2I csPos2i = cs->ParentChit()->GetSpatialComponent()->GetPosition2DI();
			if (csPos2i != pos2i) {
				thisComp.spatial->Teleport(ToWorld3F(csPos2i));
			}
			if (csPos2i == pos2i) {
				destinationBlocked = 0;
			}
		}
		// Done whether we can teleport or not.
		return true;
	}

	// Go for a rampage: remember, if the path is clear,
	// it's essentially just a random walk.
	destinationBlocked = 0;
	const SectorData& sd = context->worldMap->GetSectorData( ToSector( thisComp.spatial->GetPosition2DI() ));
	Vector2I pos2i = thisComp.spatial->GetPosition2DI();

	CArray< int, 5 > targetArr;

	if (pos2i != sd.core) {
		targetArr.Push(0);	// always core.
	}
	for( int i=0; i<4; ++i ) {
		int port = 1 << i;
		if ((sd.ports & port) && !sd.GetPortLoc(port).Contains(pos2i)) {
			targetArr.Push( i+1 );	// push the port, if it has it.
		}
	}
	GLASSERT(targetArr.Size());

	parentChit->random.ShuffleArray( targetArr.Mem(), targetArr.Size() );
	this->Rampage( targetArr[0] );
	return true;
}


bool AIComponent::RampageDone(const ComponentSet& thisComp)
{
	const ChitContext* context = Context();
	Vector2I pos2i = thisComp.spatial->GetPosition2DI();
	const SectorData& sd = context->worldMap->GetSectorData(ToSector(pos2i));
	Rectangle2I dest;

	switch (rampageTarget) {
	case WorldGrid::PS_CORE:	dest.min = dest.max = sd.core;				break;
	case WorldGrid::PS_POS_X:	dest = sd.GetPortLoc(SectorData::POS_X);	break;
	case WorldGrid::PS_POS_Y:	dest = sd.GetPortLoc(SectorData::POS_Y);	break;
	case WorldGrid::PS_NEG_X:	dest = sd.GetPortLoc(SectorData::NEG_X);	break;
	case WorldGrid::PS_NEG_Y:	dest = sd.GetPortLoc(SectorData::NEG_Y);	break;
	default: GLASSERT(0);	break;
	}

	if (dest.Contains(pos2i)) {
		return true;
	}
	return false;
}


void AIComponent::ThinkRampage( const ComponentSet& thisComp )
{
	if ( thisComp.move->IsMoving() )
		return;

	// Where are we, and where to next?
	const ChitContext* context = Context();
	Vector2I pos2i			= thisComp.spatial->GetPosition2DI();
	const WorldGrid& wg0	= context->worldMap->GetWorldGrid( pos2i.x, pos2i.y );
	Vector2I next			= pos2i + wg0.Path( rampageTarget );
	const WorldGrid& wg1	= context->worldMap->GetWorldGrid( next.x, next.y );
	const SectorData& sd	= context->worldMap->GetSectorData( ToSector( pos2i ));

	if ( RampageDone(thisComp)) {
		aiMode = NORMAL_MODE;
		currentAction = NO_ACTION;
		return;
	}

	if ( wg1.RockHeight() || wg1.BlockingPlant() ) {
		this->Target(next, false);
		currentAction = MELEE;

		const GameItem* melee = thisComp.itemComponent->SelectWeapon(ItemComponent::SELECT_MELEE);
		GLASSERT(melee);
	} 
	else if ( wg1.IsLand() ) {
		this->Move( ToWorld2F( next ), false );
	}
	else {
		aiMode = NORMAL_MODE;
		currentAction = NO_ACTION;
	}
}


Vector2F AIComponent::GetWanderOrigin( const ComponentSet& thisComp )
{
	Vector2F pos = thisComp.spatial->GetPosition2D();
	Vector2I m = { (int)pos.x/SECTOR_SIZE, (int)pos.y/SECTOR_SIZE };
	const ChitContext* context = Context();
	const SectorData& sd = context->worldMap->GetWorldInfo().GetSector( m );
	Vector2F center = { (float)(sd.x+SECTOR_SIZE/2), (float)(sd.y+SECTOR_SIZE/2) };
	if ( sd.HasCore() )	{
		center.Set( (float)sd.core.x + 0.5f, (float)sd.core.y + 0.5f );
	}
	return center;
}


Vector2F AIComponent::ThinkWanderCircle( const ComponentSet& thisComp )
{
	// In a circle?
	// This turns out to be creepy. Worth keeping for something that is,
	// in fact, creepy.
	Vector2F dest = GetWanderOrigin( thisComp );
	Random random( thisComp.chit->ID() );

	float angleUniform = random.Uniform();
	float lenUniform   = 0.25f + 0.75f*random.Uniform();

	static const U32 PERIOD = 40*1000;
	U32 t = wanderTime % PERIOD;
	float timeUniform = (float)t / (float)PERIOD;

	angleUniform += timeUniform;
	float angle = angleUniform * 2.0f * PI;
	Vector2F v = { cosf(angle), sinf(angle) };
		
	v = v * (lenUniform * WANDER_RADIUS);

	dest = GetWanderOrigin( thisComp ) + v;
	return dest;
}


Vector2F AIComponent::ThinkWanderRandom( const ComponentSet& thisComp )
{
	Vector2I pos2i = thisComp.spatial->GetPosition2DI();
	Vector2I sector = ToSector(pos2i);
	CoreScript* cs = CoreScript::GetCore(sector);

	// See also the EnterGrid(), where it takes over a neutral core.
	// Workers stay close to home, as 
	// do denizens trying to find a new home core.
	if (   (thisComp.item->flags & GameItem::AI_DOES_WORK )														
		|| (thisComp.item->MOB() == ISC::denizen && Team::IsRogue(parentChit->Team()) && cs && !cs->InUse()))
	{
		Vector2F dest = GetWanderOrigin(thisComp);
		dest.x += parentChit->random.Uniform() * WANDER_RADIUS * parentChit->random.Sign();
		dest.y += parentChit->random.Uniform() * WANDER_RADIUS * parentChit->random.Sign();
		return dest;
	}

	Vector2F dest = { 0, 0 };
	dest.x = (float)(sector.x*SECTOR_SIZE + 1 + parentChit->random.Rand( SECTOR_SIZE-2 )) + 0.5f;
	dest.y = (float)(sector.y*SECTOR_SIZE + 1 + parentChit->random.Rand( SECTOR_SIZE-2 )) + 0.5f;
	return dest;
}


Vector2F AIComponent::ThinkWanderFlock( const ComponentSet& thisComp )
{
	Vector2F origin = thisComp.spatial->GetPosition2D();

	static const int NPLANTS = 4;
	static const float TOO_CLOSE = 2.0f;

	// +1 for origin, +4 for plants
	CArray<Vector2F, MAX_TRACK+1+NPLANTS> pos;
	for( int i=0; i<friendList2.Size(); ++i ) {
		Chit* c = parentChit->Context()->chitBag->GetChit( friendList2[i] );
		if ( c && c->GetSpatialComponent() ) {
			Vector2F v = c->GetSpatialComponent()->GetPosition2D();
			pos.Push( v );
		}
	}
	// Only consider the wander origin for workers.
	if ( thisComp.item->flags & GameItem::AI_DOES_WORK ) {
		pos.Push( GetWanderOrigin( thisComp ) );	// the origin is a friend.
	}

	// And plants are friends.
	Rectangle2I r;
	r.min = r.max = ToWorld2I(origin);
	r.Outset(int(PLANT_AWARE));
	r.DoIntersection(Context()->worldMap->Bounds());

	int nPlants = 0;
	for (Rectangle2IIterator it(r); !it.Done() && nPlants < NPLANTS; it.Next()) {
		const WorldGrid& wg = Context()->worldMap->GetWorldGrid(it.Pos());
		if (wg.Plant() && !wg.BlockingPlant()) {
			++nPlants;
			pos.Push(ToWorld2F(it.Pos()));
		}
	}

	Vector2F mean = origin;
	// Get close to friends.
	for( int i=0; i<pos.Size(); ++i ) {
		mean = mean + pos[i];
	}
	Vector2F dest = mean * (1.0f/(float)(1+pos.Size()));
	Vector2F heading = thisComp.spatial->GetHeading2D();

	// But not too close.
	for( int i=0; i<pos.Size(); ++i ) {
		if ( (pos[i] - dest).LengthSquared() < TOO_CLOSE*TOO_CLOSE ) {
			dest += heading * TOO_CLOSE;
		}
	}
	return dest;
}


void AIComponent::GoSectorHerd(bool focus)
{
	ComponentSet thisComp( parentChit, Chit::RENDER_BIT | 
		                               Chit::SPATIAL_BIT |
									   Chit::ITEM_BIT |
									   ComponentSet::IS_ALIVE |
									   ComponentSet::NOT_IN_IMPACT );
	if (!thisComp.okay) return;

	SectorHerd( thisComp, focus );
}


bool AIComponent::SectorHerd(const ComponentSet& thisComp, bool focus)
{
	/*
		Depending on the MOB and tech level,
		avoid or be attracted to core
		locations. Big travel in is implemented
		be the CoreScript

		The current rules are in corescript.cpp
		*/
	static const int NDELTA = 8;
	static const Vector2I initDelta[NDELTA] = {
		{ -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 },
		{ -1, -1 }, { 1, -1 }, { -1, 1 }, { 1, 1 }
	};
	IString mob = thisComp.item->keyValues.GetIString("mob");
	CArray<Vector2I, NDELTA * 2> delta, rinit;
	for (int i = 0; i < NDELTA; ++i) {
		rinit.Push(initDelta[i]);
	}
	if (mob == ISC::greater) {
		for (int i = 0; i < NDELTA; ++i) {
			rinit.Push(initDelta[i]*2);	// greaters have a larger move range
		}
	}
	parentChit->random.ShuffleArray(rinit.Mem(), rinit.Size());

	const ChitContext* context = Context();
	const Vector2F pos = thisComp.spatial->GetPosition2D();
	const SectorData& sd = context->worldMap->GetWorldInfo().GetSector(ToSector(pos));
	const SectorPort start = context->worldMap->NearestPort(pos);
	//Sometimes we can't path to any port. Hopefully rampage cuts in.
	//GLASSERT(start.IsValid());
	if (!start.IsValid()) {
		return false;
	}

	Vector2I sector = ToSector(ToWorld2I(pos));

	if (thisComp.item->IName() == ISC::troll) {
		// Visit Truulga every now and again. And if leaving truuga...go far.
		Chit* truulga = Context()->chitBag->GetDeity(LumosChitBag::DEITY_TRUULGA);
		if (truulga && truulga->GetSpatialComponent()) {
			Vector2I truulgaSector = truulga->GetSpatialComponent()->GetSector();
			if (thisComp.spatial->GetSector() == truulgaSector) {
				// At Truulga - try to go far.
				Vector2I destSector = { parentChit->random.Rand(NUM_SECTORS), parentChit->random.Rand(NUM_SECTORS) };
				if (DoSectorHerd(thisComp, focus, destSector))
					return true;
				// Else drop out and use code below to go to a neighbor.
			}
			else {
				// Should we visit Truulga? Check for a little gold, too.
				// FIXME: needs tuning!
				if (thisComp.item->wallet.Gold() > 15 && parentChit->random.Rand(15) == 0) {
					return DoSectorHerd(thisComp, focus, truulgaSector );
				}
			}
		}
	}

	// First pass: filter on attract / repel choices.
	// This is game difficulty logic!
	Rectangle2I sectorBounds;
	sectorBounds.Set(0, 0, NUM_SECTORS - 1, NUM_SECTORS - 1);

	for (int i = 0; i < rinit.Size(); ++i) {
		Vector2I destSector = start.sector + rinit[i];

		if (sectorBounds.Contains(destSector)) {
			CoreScript* cs = CoreScript::GetCore(destSector);

			// Check repelled / attracted.
			if (cs && cs->ParentChit()->Team()) {
				int relate = Team::GetRelationship(cs->ParentChit(), thisComp.chit);
				int nTemples = cs->NumTemples();
				float tech = cs->GetTech();

				// For enemies, apply rules to make the gameplay smoother.
				if (relate == RELATE_ENEMY) {
					if (mob == ISC::lesser) {
						if (nTemples <= TEMPLES_REPELS_LESSER) {
							if (parentChit->random.Rand(2) == 0) {
								delta.Push(rinit[i]);
							}
						}
						else {
							delta.Push(rinit[i]);
						}
					}
					else if (mob == ISC::greater) {
						if (tech >= TECH_ATTRACTS_GREATER) 
							delta.Insert(0, rinit[i]);
						else if (nTemples > TEMPLES_REPELS_GREATER)
							delta.Push(rinit[i]);
						else {
							// else push nothing
						}
					}
					else {
						delta.Push(rinit[i]);
					}
				}
				else {
					delta.Push(rinit[i]);
				}
			}
			else if (cs) {
				// FIXME: is the cs check needed?
				// But we don't want the MOBs herding to the outland
				delta.Push(rinit[i]);
			}
		}
	}

	// 2nd pass: look for 1st match
	if (start.IsValid()) {
		for (int i = 0; i < delta.Size(); ++i) {
			if (DoSectorHerd(thisComp, focus, start.sector + delta[i])) {
				return true;
			}
		}
	}
	return false;
}


bool AIComponent::DoSectorHerd(const ComponentSet& thisComp, bool focus, const grinliz::Vector2I& sector)
{
	SectorPort dest;
	dest.sector = sector;
	const SectorData& destSD = Context()->worldMap->GetSectorData(dest.sector);
	GLASSERT(thisComp.spatial);
	dest.port = destSD.NearestPort(thisComp.spatial->GetPosition2D());
	return DoSectorHerd(thisComp, focus, dest);
}


bool AIComponent::DoSectorHerd(const ComponentSet& thisComp, bool focus, const SectorPort& dest)
{
	if (dest.IsValid()) {
		const ChitContext* context = Context();
		GLASSERT(dest.port);

		RenderComponent* rc = parentChit->GetRenderComponent();
		if ( rc ) {
			rc->AddDeco( "horn", 10*1000 );
		}

		// Trolls herd *all the time*
		if ( thisComp.item->IName() != ISC::troll ) {
			NewsEvent news( NewsEvent::SECTOR_HERD, thisComp.spatial->GetPosition2D(), 
						   parentChit->GetItemID(), 0, parentChit->Team() );
			Context()->chitBag->GetNewsHistory()->Add( news );
		}

		ChitMsg msg( ChitMsg::CHIT_SECTOR_HERD, focus ? 1:0, &dest );
		for( int i=0; i<friendList2.Size(); ++i ) {
			Chit* c = Context()->chitBag->GetChit( friendList2[i] );
			if ( c ) {
				c->SendMessage( msg );
			}
		}
		parentChit->SendMessage( msg );
		return true;
	}
	return false;
}


void AIComponent::ThinkVisitor( const ComponentSet& thisComp )
{
	// Visitors can:
	// - go to kiosks, stand, then move on to a different domain
	// - disconnect
	// - grid travel to a new domain

	if ( !thisComp.okay ) return;

	PathMoveComponent* pmc = GET_SUB_COMPONENT( parentChit, MoveComponent, PathMoveComponent );
	if ( !pmc ) return;											// strange state,  can't do anything
	if ( !pmc->Stopped() && !pmc->ForceCountHigh() ) return;	// everything is okay. move along.

	bool disconnect = false;
	const ChitContext* context = Context();

	Vector2I pos2i = thisComp.spatial->GetPosition2DI();
	Vector2I sector = { pos2i.x/SECTOR_SIZE, pos2i.y/SECTOR_SIZE };
	CoreScript* coreScript = CoreScript::GetCore(sector);
	VisitorData* vd = Visitors::Get( visitorIndex );
	Chit* kiosk = Context()->chitBag->ToLumos()->QueryPorch( pos2i, 0 );
	if ( kiosk && kiosk->GetItem()->IName() == ISC::kiosk ) {
		// all good
	}
	else {
		kiosk = 0;
	}

	if ( vd->visited.Find(sector) >= 0) {
		// This sector has been added to "visited", so we are done here.
		// Head out!
		LumosChitBag* chitBag = Context()->chitBag;
		const Web& web = chitBag->GetSim()->GetCachedWeb();
		SectorPort sp = Visitors::Instance()->ChooseDestination(visitorIndex, web, chitBag, Context()->worldMap);

		if (sp.IsValid()) {
			this->Move(sp, true);
		}
		else {
			disconnect = true;
		}
	}
	else if ( pmc->Stopped() && kiosk ) {
		currentAction = STAND;
	}
	else {
		// Find a kiosk.
		Chit* kiosk = Context()->chitBag->FindBuilding(	ISC::kiosk,
														sector,
														&thisComp.spatial->GetPosition2D(),
														LumosChitBag::RANDOM_NEAR, 0, 0 );

		if ( !kiosk ) {
			// Done here.
			vd->visited.Push(sector);
		}
		else {
			MapSpatialComponent* msc = GET_SUB_COMPONENT( kiosk, SpatialComponent, MapSpatialComponent );
			GLASSERT( msc );
			Rectangle2I porch = msc->PorchPos();

			// The porch is a rectangle; go to a particular point based on the ID()
			if ( context->worldMap->CalcPath( thisComp.spatial->GetPosition2D(), ToWorld2F(porch.min), 0, 0 ) ) {
				this->Move(ToWorld2F(porch.min), false);
			}
			else {
				// Can't path!
				vd->visited.Push(sector);
			}
		}
	}
	if ( disconnect ) {
		parentChit->GetItem()->hp = 0;
		parentChit->SetTickNeeded();
	}
}


bool AIComponent::ThinkWanderEat(const ComponentSet& thisComp)
{
	GLASSERT(thisComp.item);
	// Plant eater
	if (thisComp.item->HPFraction() < EAT_WILD_FRUIT) {
		Vector2I pos2i = thisComp.spatial->GetPosition2DI();
		Vector2F pos2 = thisComp.spatial->GetPosition2D();

		// Are we near fruit?
		CChitArray arr;
		FruitElixirFilter fruitFilter;
		Context()->chitBag->QuerySpatialHash(&arr, pos2, PLANT_AWARE, 0, &fruitFilter);

		for (int i = 0; i < arr.Size(); ++i) {
			Vector2I plantPos = arr[i]->GetSpatialComponent()->GetPosition2DI();
			if (Context()->worldMap->HasStraightPath(pos2, ToWorld2F(plantPos))) {
				this->Move(ToWorld2F(plantPos), false);
				return true;
			}
		}
	}
	return false;
}


#if 0
bool AIComponent::ThinkWanderHealAtCore( const ComponentSet& thisComp )
{
	// Core healer
	if (    (thisComp.item->flags & GameItem::AI_HEAL_AT_CORE) 
		 && (thisComp.item->hp < double(thisComp.item->TotalHP()) * 0.8 )) 
	{
		const ChitContext* context = Context();

		Vector2I sector = ToSector( thisComp.spatial->GetPosition2DI() );
		const SectorData& sd = context->worldMap->GetSectorData( sector );
		if ( sd.core != thisComp.spatial->GetPosition2DI() ) {
			if ( context->worldMap->CalcPath( thisComp.spatial->GetPosition2D(), ToWorld2F( sd.core ), 0, 0 )) {
				this->Move( ToWorld2F( sd.core ), false );
				return true;
			}
		}
	}
	return false;
}
#endif


Vector2I AIComponent::RandomPosInRect( const grinliz::Rectangle2I& rect, bool excludeCenter )
{
	Vector2I v = { 0, 0 };
	while( true ) {
		 v.Set( rect.min.x + parentChit->random.Rand( rect.Width() ),
				rect.min.y + parentChit->random.Rand( rect.Height() ));
		if ( excludeCenter && v == rect.Center() )
			continue;
		break;
	}
	return v;
}


bool AIComponent::ThinkGuard( const ComponentSet& thisComp )
{
	if (    !(thisComp.item->flags & GameItem::AI_USES_BUILDINGS )
		 || !(thisComp.item->flags & GameItem::HAS_NEEDS )
		 || parentChit->PlayerControlled() 
		 || !AtHomeCore() )
	{
		return false;
	}

	if ( thisComp.item->GetPersonality().Guarding() == Personality::DISLIKES ) {
		return false;
	}

	Vector2I pos2i = thisComp.spatial->GetPosition2DI();
	Vector2I sector = ToSector( pos2i );
	Rectangle2I bounds = InnerSectorBounds( sector );

	CoreScript* coreScript = CoreScript::GetCore( sector );

	if ( !coreScript ) return false;

	ItemNameFilter filter(ISC::guardpost, IChitAccept::MAP);
	Context()->chitBag->FindBuilding( IString(), sector, 0, 0, &chitArr, &filter );

	if ( chitArr.Empty() ) return false;

	// Are we already guarding??
	for( int i=0; i<chitArr.Size(); ++i ) {
		Rectangle2I guardBounds;
		guardBounds.min = guardBounds.max = chitArr[i]->GetSpatialComponent()->GetPosition2DI();
		guardBounds.Outset( GUARD_RANGE );
		guardBounds.DoIntersection( bounds );

		if ( guardBounds.Contains( pos2i )) {
			taskList.Push( Task::MoveTask( ToWorld2F( RandomPosInRect( guardBounds, true ))));
			taskList.Push( Task::StandTask( GUARD_TIME ));
			return true;
		}
	}

	int post = thisComp.chit->random.Rand( chitArr.Size() );
	Rectangle2I guardBounds;
	guardBounds.min = guardBounds.max = chitArr[post]->GetSpatialComponent()->GetPosition2DI();
	guardBounds.Outset( GUARD_RANGE );
	guardBounds.DoIntersection( bounds );

	taskList.Push( Task::MoveTask( ToWorld2F( RandomPosInRect( guardBounds, true ))));
	taskList.Push( Task::StandTask( GUARD_TIME ));
	return true;
}


bool AIComponent::AtHomeCore()
{
	SpatialComponent* sc = parentChit->GetSpatialComponent();
	if ( !sc ) return false;

	Vector2I sector = ToSector( sc->GetPosition2DI());
	CoreScript* coreScript = CoreScript::GetCore(sector);

	if ( !coreScript ) return false;
	return coreScript->IsCitizen( parentChit->ID() );
}


bool AIComponent::AtFriendlyOrNeutralCore()
{
	SpatialComponent* sc = parentChit->GetSpatialComponent();
	if ( !sc ) return false;

	Vector2I sector = ToSector( sc->GetPosition2DI());
	CoreScript* coreScript = CoreScript::GetCore( sector );
	if (coreScript) {
		return Team::GetRelationship(parentChit, coreScript->ParentChit()) != RELATE_ENEMY;
	}
	return false;
}


void AIComponent::FindFruit( const Vector2F& pos2, Vector2F* dest, CChitArray* arr, bool* nearPath )
{
	// Be careful of performance. Allow one full pathing, use
	// direct pathing for the rest.
	// Also, kind of tricky. Fruit is usually blocked, so it
	// can be picked up on its grid, or the closest neighbor
	// grid. And check near the mob and near farms.

	const ChitContext* context	= this->Context();
	LumosChitBag* chitBag		= this->Context()->chitBag;
	const Vector2I sector		= ToSector(pos2);
	CoreScript* cs				= CoreScript::GetCore(sector);

	FruitElixirFilter filter;
	*nearPath = false;

	// Check local. For local, use direct path.
	CChitArray chitArr;
	chitBag->QuerySpatialHash( &chitArr, pos2, FRUIT_AWARE, 0, &filter );
	for (int i = 0; i < chitArr.Size(); ++i) {
		Chit* chit = chitArr[i];
		Vector2F fruitPos = chit->GetSpatialComponent()->GetPosition2D();
		if (context->worldMap->HasStraightPath(pos2, fruitPos)) {
			*dest = fruitPos;
			arr->Push(chit);
			// Push other fruit at this same location.
			for (int k = i + 1; k < chitArr.Size(); ++k) {
				if (chitArr[k]->GetSpatialComponent()->GetPosition2DI() == chit->GetSpatialComponent()->GetPosition2DI()) {
					arr->Push(chitArr[k]);
				}
			}
			*nearPath = true;
			return;
		}
	}

	// Okay, now check the farms and distilleries. 
	// Now we need to use full pathing for meaningful results.
	// Only need to check the farm porch - all fruit goes there.
	for (int pass = 0; pass < 2; ++pass) {
		if (pass == 1) {
			int debug = 1;
		}
		CChitArray buildingArr;
		chitBag->FindBuildingCC(pass == 0 ? ISC::farm : ISC::distillery,
								ToSector(ToWorld2I(pos2)),
								0, 0, &buildingArr, 0);

		for (int i = 0; i < buildingArr.Size(); ++i) {
			Chit* farmChit = buildingArr[i];
			MapSpatialComponent* farmMSC = GET_SUB_COMPONENT(farmChit, SpatialComponent, MapSpatialComponent);
			GLASSERT(farmMSC);
			if (farmMSC) {
				Rectangle2I porch = farmMSC->PorchPos();
				Vector2F farmLoc = ToWorld(porch).Center();
				chitBag->QuerySpatialHash(&chitArr, farmLoc, 1.0f, 0, &filter);

				for (int j = 0; j < chitArr.Size(); ++j) {
					Vector2F fruitLoc = chitArr[j]->GetSpatialComponent()->GetPosition2D();
					Vector2I fruitLoc2i = ToWorld2I(fruitLoc);
					// Skip if someone probably heading there.
					if (cs && cs->HasTask(fruitLoc2i)) {
						continue;
					}

					if (context->worldMap->CalcPath(pos2, fruitLoc, 0, 0, 0)) {
						*dest = fruitLoc;
						Chit* chit = chitArr[j];
						arr->Push(chit);
						// Push other fruit at this same location.
						for (int k = j + 1; k < chitArr.Size(); ++k) {
							if (chitArr[k]->GetSpatialComponent()->GetPosition2DI() == chit->GetSpatialComponent()->GetPosition2DI()) {
								arr->Push(chitArr[k]);
							}
						}
						return;
					}
				}
			}
		}
	}
	dest->Zero();
	return;
}



bool AIComponent::ThinkFruitCollect( const ComponentSet& thisComp )
{
	// Some buildings provide food - that happens in ThinkNeeds.
	// This is for picking up food on the group.

	if ( parentChit->PlayerControlled() ) {
		return false;
	}

	double need = 1;
	if ( thisComp.item->flags & GameItem::HAS_NEEDS ) {
		const ai::Needs& needs = GetNeeds();
		need = needs.Value( Needs::FOOD );
	}

	// workers carrying fruit seemed weird for some reason.
	//bool worker = (thisComp.item->flags & GameItem::AI_DOES_WORK) != 0;

	// It is the duty of every citizen to take fruit to the distillery.
	if ( AtHomeCore() ) {
		if (    thisComp.item->GetPersonality().Botany() != Personality::DISLIKES
			 || need < NEED_CRITICAL * 2.0 )
		{
			// Can we go pick something up?
			if ( thisComp.itemComponent->CanAddToInventory() ) {
				Vector2F fruitPos = { 0, 0 };
				CChitArray fruit;
				bool nearPath = false;
				FindFruit( thisComp.spatial->GetPosition2D(), &fruitPos, &fruit, &nearPath );
				if ( fruit.Size() ) {
					//GameItem* gi = fruit[0]->GetItem();
					Vector2I fruitPos2i = ToWorld2I(fruitPos);
					Vector2I sector = ToSector(fruitPos2i);
					CoreScript* cs = CoreScript::GetCore(sector);
					if (!nearPath && cs && cs->HasTask(fruitPos2i)) {
						// Do nothing; assume it is this task.
					}
					else {
						taskList.Push(Task::MoveTask(fruitPos));
						for (int i = 0; i < fruit.Size(); ++i) {
							taskList.Push(Task::PickupTask(fruit[i]->ID()));
						}
						return true;
					}
				}
			}
		}
	}
	return false;
}


bool AIComponent::ThinkFlag(const ComponentSet& thisComp)
{
	if (parentChit->PlayerControlled()) {
		return false;
	}
	bool usesBuilding = (thisComp.item->flags & GameItem::AI_USES_BUILDINGS) != 0;
	if (!usesBuilding) {
		return false;
	}

	// Flags are done by denizens, NOT workers.
	// This handles flags in the same domain.
	CoreScript* homeCoreScript = CoreScript::GetCoreFromTeam(thisComp.chit->Team());
	if (!homeCoreScript)
		return false;

	Vector2I homeCoreSector = homeCoreScript->ParentChit()->GetSpatialComponent()->GetSector();
	Vector2I sector = thisComp.spatial->GetSector();

	if (   homeCoreScript->IsCitizen(parentChit->ID())
		&& ( homeCoreSector == sector))
	{
		Vector2I flag = homeCoreScript->GetFlag();
		if (!flag.IsZero()) {
			if (!homeCoreScript->HasTask(flag)) {
				taskList.Push(Task::MoveTask(flag));
				taskList.Push(Task::FlagTask());
				return true;
			}
		}
	}
	return false;
}


bool AIComponent::ThinkHungry(const ComponentSet& thisComp)
{
	if (parentChit->PlayerControlled()) {
		return false;
	}

	const GameItem* fruit = thisComp.itemComponent->FindItem(ISC::fruit);
	const GameItem* elixir = thisComp.itemComponent->FindItem(ISC::elixir);
	bool carrying = fruit || elixir;

	// Are we carrying fruit?? If so, eat if hungry!
	if (carrying && (thisComp.item->flags & GameItem::HAS_NEEDS)) {
		const ai::Needs& needs = GetNeeds();
		if (needs.Value(Needs::FOOD) < NEED_CRITICAL) {
			if (elixir) {
				GameItem* item = thisComp.itemComponent->RemoveFromInventory(elixir);
				delete item;
				this->GetNeedsMutable()->Set(Needs::FOOD, 1);
			}
			else if (fruit) {
				GameItem* item = thisComp.itemComponent->RemoveFromInventory(fruit);
				delete item;
				this->GetNeedsMutable()->Set(Needs::FOOD, 1);
			}
			return true;	// did something...?
		}
	}
	return false;
}


bool AIComponent::ThinkDelivery( const ComponentSet& thisComp )
{
	if ( parentChit->PlayerControlled() ) {
		return false;
	}

	bool usesBuilding = (thisComp.item->flags & GameItem::AI_USES_BUILDINGS) != 0;
	bool worker       = (thisComp.item->flags & GameItem::AI_DOES_WORK) != 0;

	if ( worker ) {
		bool needVaultRun = false;

		if ( !thisComp.itemComponent->GetItem(0)->wallet.IsEmpty() ) {
			needVaultRun = true;
		}

		if ( needVaultRun == false ) {
			for( int i=1; i<thisComp.itemComponent->NumItems(); ++i ) {
				const GameItem* item = thisComp.itemComponent->GetItem( i );
				if ( item->Intrinsic() )
					continue;
				if ( item->ToRangedWeapon() || item->ToMeleeWeapon() || item->ToShield() )
				{
					needVaultRun = true;
					break;
				}
			}
		}
		if ( needVaultRun ) {
			Vector2I sector = thisComp.spatial->GetSector();
			Chit* vault = Context()->chitBag->FindBuilding(	ISC::vault, 
															sector, 
															&thisComp.spatial->GetPosition2D(), 
															LumosChitBag::RANDOM_NEAR, 0, 0 );
			if ( vault && vault->GetItemComponent() && vault->GetItemComponent()->CanAddToInventory() ) {
				MapSpatialComponent* msc = GET_SUB_COMPONENT( vault, SpatialComponent, MapSpatialComponent );
				GLASSERT( msc );
				CoreScript* coreScript = CoreScript::GetCore(ToSector(msc->MapPosition()));
				if ( msc && coreScript ) {
					Rectangle2I porch = msc->PorchPos();
					for (Rectangle2IIterator it(porch); !it.Done(); it.Next()) {
						if (!coreScript->HasTask(it.Pos())) {
							taskList.Push(Task::MoveTask(it.Pos()));
							taskList.Push(Task::UseBuildingTask());
							return true;
						}
					}
				}
			}
		}
	}

	if (worker || usesBuilding)
	{
		for (int pass = 0; pass < 2; ++pass) {
			const IString iItem     = (pass == 0) ? ISC::elixir : ISC::fruit;
			const IString iBuilding = (pass == 0) ? ISC::bar    : ISC::distillery;

			const GameItem* item = thisComp.itemComponent->FindItem(iItem);
			if (item) {
				Vector2I sector = thisComp.spatial->GetSector();
				Chit* building = Context()->chitBag->FindBuilding(iBuilding, sector,
																  &thisComp.spatial->GetPosition2D(),
																  LumosChitBag::RANDOM_NEAR, 0, 0);
				if (building && building->GetItemComponent() && building->GetItemComponent()->CanAddToInventory()) {
					MapSpatialComponent* msc = GET_SUB_COMPONENT(building, SpatialComponent, MapSpatialComponent);
					GLASSERT(msc);
					CoreScript* coreScript = CoreScript::GetCore(sector);
					if (msc && coreScript) {
						Rectangle2I porch = msc->PorchPos();
						for (Rectangle2IIterator it(porch); !it.Done(); it.Next()) {
							if (!coreScript->HasTask(it.Pos())) {
								taskList.Push(Task::MoveTask(it.Pos()));
								taskList.Push(Task::UseBuildingTask());
								return true;
							}
						}
					}
				}
			}
		}
	}
	return false;
}


bool AIComponent::ThinkRepair(const ComponentSet& thisComp)
{
	if (parentChit->PlayerControlled()) {
		return false;
	}

	bool worker = (thisComp.item->flags & GameItem::AI_DOES_WORK) != 0;
	if (!worker) return false;

	BuildingRepairFilter filter;
	Vector2I sector = thisComp.spatial->GetSector();

	Chit* building = Context()->chitBag->FindBuilding(IString(),
		sector,
		&thisComp.spatial->GetPosition2D(),
		LumosChitBag::RANDOM_NEAR, 0, &filter);

	if (!building) return false;

	MapSpatialComponent* msc = building->GetSpatialComponent()->ToMapSpatialComponent();
	GLASSERT(msc);
	Rectangle2I repair;
	repair.Set(0, 0, 0, 0);
	if (msc) {
		if (msc->HasPorch()) {
			repair = msc->PorchPos();
		}
		else {
			repair = msc->Bounds();
			repair.Outset(1);
		}
	}
	CoreScript* coreScript = CoreScript::GetCore(ToSector(msc->MapPosition()));
	if (!coreScript) return false;

	WorldMap* worldMap = Context()->worldMap;
	Vector2F pos2 = thisComp.spatial->GetPosition2D();

	for (Rectangle2IIterator it(repair); !it.Done(); it.Next()) {
		if (!coreScript->HasTask(it.Pos()) 
			&& worldMap->CalcPath( pos2, ToWorld2F(it.Pos()), 0, 0, false)) 
		{
			taskList.Push(Task::MoveTask(it.Pos()));
			taskList.Push(Task::StandTask(REPAIR_TIME));
			taskList.Push(Task::RepairTask(building->ID()));
			return true;
		}
	}
	return false;
}


bool AIComponent::ThinkNeeds(const ComponentSet& thisComp)
{
	if (!(thisComp.item->flags & GameItem::AI_USES_BUILDINGS)
		|| !(thisComp.item->flags & GameItem::HAS_NEEDS)
		|| parentChit->PlayerControlled())
	{
		return false;
	}

	Vector2I pos2i = thisComp.spatial->GetPosition2DI();
	Vector2I sector = ToSector(pos2i);
	CoreScript* coreScript = CoreScript::GetCore(sector);

	if (!coreScript) return false;
	if (Team::GetRelationship(parentChit, coreScript->ParentChit()) == RELATE_ENEMY) return false;

	BuildingFilter filter;
	Context()->chitBag->FindBuilding(IString(), sector, 0, 0, &chitArr, &filter);

	BuildScript			buildScript;
	int					bestIndex = -1;
	double				bestScore = 0;
	const BuildData*	bestBD = 0;
	Vector2I			bestPorch = { 0, 0 };

	Vector3<double> myNeeds = thisComp.ai->GetNeeds().GetOneMinus();
	double myMorale = thisComp.ai->GetNeeds().Morale();

	// Don't eat when not hungry; depletes food.
	if (myNeeds.X(Needs::FOOD) < 0.5) {
		myNeeds.X(Needs::FOOD) = 0;
	}

	/*
	bool logNeeds = thisComp.item->IProperName() == "Tria";
	if (logNeeds) {
	GLOUTPUT(("Denizen %d eval:\n", thisComp.chit->ID()));
	}
	*/
	
	bool debugBuildingOutput[BuildScript::NUM_TOTAL_OPTIONS] = { false };

	for (int i = 0; i < chitArr.Size(); ++i) {
		Chit* building = chitArr[i];
		GLASSERT(building->GetItem());
		int buildDataID = 0;
		const BuildData* bd = buildScript.GetDataFromStructure(building->GetItem()->IName(), &buildDataID);
		if (!bd || bd->needs.IsZero()) continue;

		MapSpatialComponent* msc = GET_SUB_COMPONENT(building, SpatialComponent, MapSpatialComponent);
		GLASSERT(msc);

		Vector2I porch = { 0, 0 };
		Rectangle2I porchRect = msc->PorchPos();
		for (Rectangle2IIterator it(porchRect); !it.Done(); it.Next()) {
			if (!coreScript->HasTask(it.Pos())) {
				porch = it.Pos();
				break;
			}
		}
		if (porch.IsZero())	continue;

		bool functional = false;
		Vector3<double> buildingNeeds = ai::Needs::CalcNeedsFullfilledByBuilding(building, thisComp.chit, &functional);
		if (buildingNeeds.IsZero()) continue;

		// The needs match.
		double score = DotProduct(buildingNeeds, myNeeds);
		// Small wiggle to use different markets, sleep tubes, etc.
		static const double INV = 1.0 / 255.0;
		score += 0.05 * double(Random::Hash8(building->ID() ^ thisComp.chit->ID())) * INV;
		// Variation - is this the last building visited?
		// This is here to break up the bar-sleep loop. But only if we have full morale.
		if (myMorale > 0.99) {
			for (int k = 0; k < TaskList::NUM_BUILDING_USED; ++k) {
				if (bd->structure == taskList.BuildingsUsed()[k]) {
					// Tricky number to get right; note the bestScore >= SOMETHING filter below.
					score *= 0.6 + 0.1 * double(k);
				}
			}
		}

		if (debugLog) {
			if (!debugBuildingOutput[buildDataID]) {
				GLASSERT(buildDataID >= 0 && buildDataID < GL_C_ARRAY_SIZE(debugBuildingOutput));
				debugBuildingOutput[buildDataID] = true;
				GLOUTPUT(("  %.2f %s %s\n", score, building->GetItem()->Name(), functional ? "func" : "norm" ));
			}
		}

		if ((score > 0.4 || (score > 0.1 && functional))
			&& score > bestScore)
		{
			bestScore = score;
			bestIndex = i;
			bestBD = bd;
			bestPorch = porch;
		}
	}

	if (bestScore >0) {
		GLASSERT(bestPorch.x > 0);
		if (debugLog) {
			GLOUTPUT(("  --> %s\n", bestBD->structure.c_str()));
		}

		taskList.Push(Task::MoveTask(bestPorch));
		taskList.Push(Task::StandTask(bestBD->standTime));
		taskList.Push(Task::UseBuildingTask());
		return true;
	}
	return false;
}


bool AIComponent::ThinkLoot( const ComponentSet& thisComp )
{
	// Is there stuff around to pick up?

	int flags = thisComp.item->flags;
	const ChitContext* context = Context();

	if(    !parentChit->PlayerControlled() 
		&& ( flags & (GameItem::GOLD_PICKUP | GameItem::ITEM_PICKUP))) 
	{
		// Which filter to use?
		GoldCrystalFilter	gold;
		LootFilter			loot;

		MultiFilter filter( MultiFilter::MATCH_ANY );
		bool canAdd	= thisComp.itemComponent->CanAddToInventory();

		if ( canAdd && ( flags & GameItem::ITEM_PICKUP)) {
			filter.filters.Push( &loot );
		}
		if ( flags & GameItem::GOLD_PICKUP ) {
			filter.filters.Push( &gold );
		}

		Vector2F pos2 = thisComp.spatial->GetPosition2D();
		CChitArray chitArr;
		parentChit->Context()->chitBag->QuerySpatialHash( &chitArr, pos2, GOLD_AWARE, 0, &filter );

		ChitDistanceCompare compare( thisComp.spatial->GetPosition() );
		SortContext( chitArr.Mem(), chitArr.Size(), compare );

		for( int i=0; i<chitArr.Size(); ++i ) {
			Vector2F goldPos = chitArr[i]->GetSpatialComponent()->GetPosition2D();
			if ( context->worldMap->HasStraightPath( goldPos, pos2 )) {
				// Pickup and gold use different techniques. (Because of player UI. 
				// Always want gold - not all items.)
				if ( loot.Accept( chitArr[i] )) {
					taskList.Push(Task::MoveTask(goldPos));
					taskList.Push( Task::PickupTask( chitArr[i]->ID()));
					return true;
				}
				else {
					this->Move( goldPos, false );
					return true;
				}
			}
		}
	}
	return false;
}


void AIComponent::ThinkNormal( const ComponentSet& thisComp )
{
	// Wander in some sort of directed fashion.
	// - get close to friends
	// - but not too close
	// - given a choice, get close to plants.
	// - occasionally randomly wander about

	Vector2F dest = { 0, 0 };
	const GameItem* item	= parentChit->GetItem();
	int itemFlags			= item ? item->flags : 0;
	int wanderFlags			= itemFlags & GameItem::AI_WANDER_MASK;
	Vector2F pos2 = thisComp.spatial->GetPosition2D();
	Vector2I pos2i = { (int)pos2.x, (int)pos2.y };
	const ChitContext* context = Context();

	RangedWeapon* ranged = thisComp.itemComponent->GetRangedWeapon( 0 );
	if ( ranged && ranged->CanReload() ) {
		ranged->Reload( parentChit );
	}

	if (ThinkWaypoints(thisComp))
		return;

	if (ThinkWanderEat(thisComp))
		return;
	if (ThinkLoot(thisComp))
		return;
	if (ThinkHungry(thisComp))
		return;

	// Be sure to deliver before collecting!
	if (ThinkDelivery(thisComp))
		return;
	if (ThinkFruitCollect(thisComp))
		return;
	if (ThinkFlag(thisComp))
		return;
	if (ThinkNeeds(thisComp))
		return;
	if (ThinkRepair(thisComp))
		return;
	if (ThinkGuard(thisComp))
		return;
	if (ThinkDoRampage(thisComp))
		return;

	// Wander....
	if ( dest.IsZero() ) {
		if ( parentChit->PlayerControlled() ) {
			currentAction = WANDER;
			return;
		}
		PathMoveComponent* pmc = GET_SUB_COMPONENT( parentChit, MoveComponent, PathMoveComponent );
		int r = parentChit->random.Rand(4);

		// FIXME: the greater logic doesn't even seem to get used.
		// Denizens DO sector herd until they are members of a core.
		bool sectorHerd = pmc
							&& itemFlags & GameItem::AI_SECTOR_HERD
							&& (friendList2.Size() >= (MAX_TRACK * 3 / 4) || pmc->ForceCount() > FORCE_COUNT_STUCK)
							&& (thisComp.chit->random.Rand(WANDER_ODDS) == 0)
							&& (CoreScript::GetCoreFromTeam(thisComp.chit->Team()) == 0);
		bool sectorWander =		pmc
							&& itemFlags & GameItem::AI_SECTOR_WANDER
							&& thisComp.item
							&& thisComp.item->HPFraction() > 0.80f
							&& (thisComp.chit->random.Rand( GREATER_WANDER_ODDS ) == 0)
							&& (CoreScript::GetCoreFromTeam(thisComp.chit->Team()) == 0);

		if ( sectorHerd || sectorWander ) 
		{
			if ( SectorHerd( thisComp, false ) )
				return;
		}
		else if ( wanderFlags == 0 || r == 0 ) {
			dest = ThinkWanderRandom( thisComp );
		}
		else if ( wanderFlags == GameItem::AI_WANDER_HERD ) {
			dest = ThinkWanderFlock( thisComp );
		}
		else if ( wanderFlags == GameItem::AI_WANDER_CIRCLE ) {
			dest = ThinkWanderCircle( thisComp );
		}
	}
	if ( !dest.IsZero() ) {
		Vector2I dest2i = { (int)dest.x, (int)dest.y };
		// If the move is very near (happens if friendList empty)
		// don't do the move to avoid jerk.
		if (dest2i != thisComp.spatial->GetPosition2DI()) {
			taskList.Push(Task::MoveTask(dest2i));
		}
		taskList.Push( Task::StandTask( STAND_TIME_WHEN_WANDERING ));
	}
}


void AIComponent::ThinkBattle( const ComponentSet& thisComp )
{
	PathMoveComponent* pmc = GET_SUB_COMPONENT( parentChit, MoveComponent, PathMoveComponent );
	if ( !pmc ) {
		currentAction = NO_ACTION;
		return;
	}
	const Vector3F& pos = thisComp.spatial->GetPosition();
	Vector2F pos2 = { pos.x, pos.z };
	Vector2I sector = ToSector( ToWorld2I( pos2 ));
	
	// Use the current or reserve - switch out later if we need to.
	const RangedWeapon* rangedWeapon = thisComp.itemComponent->QuerySelectRanged();
	const MeleeWeapon*  meleeWeapon  = thisComp.itemComponent->QuerySelectMelee();

	enum {
		OPTION_NONE,			// Stand around
		OPTION_MOVE_TO_RANGE,	// Move to shooting range or striking range
		OPTION_MELEE,			// Focused melee attack
		OPTION_SHOOT,			// Stand and shoot. (Don't pay run-n-gun accuracy penalty.)
		NUM_OPTIONS
	};

	float utility[NUM_OPTIONS] = { 0,0,0,0 };
	int   target[NUM_OPTIONS]  = { 0,0,0,0 };

	// Moves are always to the target location, since intermediate location could
	// cause very strange pathing. Time is set to when to "rethink". If we are moving
	// to melee for example, the time is set to when we should actually switch to
	// melee. Same applies to effective range. If it isn't a straight path, we will
	// rethink too soon, which is okay.
	//float    moveToTime = 1.0f;	// Seconds to the desired location is reached.

	// Consider flocking. This wasn't really working in a combat situation.
	// May reconsider later, or just use for spreading out.
	//static  float FLOCK_MOVE_BIAS = 0.2f;
	Vector2F heading = thisComp.spatial->GetHeading2D();
	utility[OPTION_NONE] = 0;

	int nRangedEnemies = 0;	// number of enemies that could be shooting at me
	int nMeleeEnemies = 0;	// number of enemies that could be pounding at me

	// Count melee and ranged enemies.
	for( int k=0; k<enemyList2.Size(); ++k ) {
		Chit* chit = Context()->chitBag->GetChit( enemyList2[k] );
		if ( chit ) {
			ItemComponent* ic = chit->GetItemComponent();
			if ( ic ) {
				if ( ic->GetRangedWeapon(0) ) {
					++nRangedEnemies;
				}
				static const float MELEE_ZONE = (MELEE_RANGE + 0.5f) * (MELEE_RANGE + 0.5f);
				if ( ic->GetMeleeWeapon() && chit->GetSpatialComponent() &&
					( pos2 - chit->GetSpatialComponent()->GetPosition2D() ).LengthSquared() <= MELEE_ZONE )
				{
					++nMeleeEnemies;
				}
			}
		}
	}

	BuildingFilter buildingFilter;

	for( int k=0; k<enemyList2.Size(); ++k ) {
		const ChitContext* context = Context();
		int targetID = enemyList2[k];

		Chit* enemyChit = context->chitBag->GetChit(targetID);	// null if there isn't a chit
		Vector2I voxelTarget = ToWG(targetID);					// zero if there isn't a voxel target

		const Vector3F	enemyPos = EnemyPos(targetID, true);
		const Vector2F	enemyPos2		= { enemyPos.x, enemyPos.z };
		const Vector2I  enemyPos2I		= ToWorld2I(enemyPos2);
		float			range			= (enemyPos - pos).Length();
		Vector3F		toEnemy			= (enemyPos - pos);
		Vector2F		normalToEnemy	= { toEnemy.x, toEnemy.z };
		bool			enemyMoving = (enemyChit && enemyChit->GetMoveComponent()) ? enemyChit->GetMoveComponent()->IsMoving() : false;

		GLASSERT(FEFilter<RELATE_FRIEND>(parentChit, targetID) == true);

		normalToEnemy.Normalize();
		float dot = DotProduct( normalToEnemy, heading );

		// Prefer targets we are pointed at.
		static const float DOT_BIAS = 0.25f;
		float q = 1.0f + dot * DOT_BIAS;

		// Prefer the current target & focused target.
		if (targetID == lastTargetID) q *= 2.0f;
		if (k == 0 && focus == FOCUS_TARGET) q *= 20;
		// Prefer MOBs over buildings
		if (!enemyChit) q *= 0.5f;
		
		if (debugLog) {
			GLOUTPUT(("  id=%d rng=%.1f ", targetID, range));
		}

		// Consider ranged weapon options: OPTION_SHOOT, OPTION_MOVE_TO_RANGE
		if ( rangedWeapon ) {
			float radAt1 = BattleMechanics::ComputeRadAt1(	thisComp.chit->GetItem(),
															rangedWeapon,
															false,	// SHOOT implies stopping.
															enemyMoving );
			
			float effectiveRange = BattleMechanics::EffectiveRange( radAt1 );

			// 1.5f gives spacing for bolt to start.
			// The HasRound() && !Reloading() is really important: if the gun
			// is in cooldown, don't give up on shooting and do something else!
			if (    range > 1.5f 
				 && (    ( rangedWeapon->HasRound() && !rangedWeapon->Reloading() )		// we have ammod
				      || ( nRangedEnemies == 0 && range > 2.0f )))	// we have a gun and they don't
			{
				float u = 1.0f - (range - effectiveRange) / effectiveRange; 
				u = Clamp(u, 0.0f, 2.0f);	// just to keep the point blank shooting down.
				u *= q;

				// If we have melee targets, focus in on those.
				if ( nMeleeEnemies) {
					u *= 0.1f;
				}
				// This needs tuning.
				// If the unit has been shooting, time to do something else.
				// Stand around and shoot battles are boring and look crazy.
				if ( currentAction == SHOOT ) {
					u *= 0.8f;	// 0.5f;
				} 

				// Don't blow ourself up:
				if ( rangedWeapon->flags & GameItem::EFFECT_EXPLOSIVE ) {
					if ( range < EXPLOSIVE_RANGE * 2.0f ) {
						u *= 0.1f;	// blowing ourseves up is bad.
					}
				}

				bool lineOfSight = false;
				if (enemyChit) lineOfSight = LineOfSight(thisComp, enemyChit, rangedWeapon);
				else if (!enemyPos2I.IsZero()) lineOfSight = LineOfSight(thisComp, enemyPos2I);

				if (debugLog) {
					GLOUTPUT(("r=%.1f ", u));
				}
						
				if (u > utility[OPTION_SHOOT] && lineOfSight) {
					utility[OPTION_SHOOT] = u;
					target[OPTION_SHOOT] = targetID;
				}
			}
			// Move to the effective range?
			float u = ( range - effectiveRange ) / effectiveRange;
			u *= q;
			if ( !rangedWeapon->CanShoot() ) {
				// Moving to effective range is less interesting if the gun isn't ready.
				u *= 0.5f;
			}
			if (debugLog) {
				GLOUTPUT(("mtr(r)=%.1f ", u));
			}
			if ( u > utility[OPTION_MOVE_TO_RANGE] ) {
				utility[OPTION_MOVE_TO_RANGE] = u;
				target[OPTION_MOVE_TO_RANGE] = targetID;
			}
		}

		// Consider Melee
		if (meleeWeapon) {
			if (range < 0.001f) {
				range = 0.001f;
			}
			float u = MELEE_RANGE / range;
			u *= q;
			if (debugLog) {
				GLOUTPUT(("m=%.1f ", u));
			}
			if (u > utility[OPTION_MELEE]) {
				utility[OPTION_MELEE] = u;
				target[OPTION_MELEE] = targetID;
			}
		}
		if (debugLog) {
			GLOUTPUT(("\n"));
		}
	}

	int index = ArrayFindMax(utility, NUM_OPTIONS, (int)0, [](int, float f) { return f; });

	// Translate to action system:
	switch ( index ) {
		case OPTION_NONE:
		{
			pmc->Stop();
		}
		break;

		case OPTION_MOVE_TO_RANGE:
		{
			GLASSERT(target[OPTION_MOVE_TO_RANGE]);
			int idx = enemyList2.Find(target[OPTION_MOVE_TO_RANGE]);
			GLASSERT(idx >= 0);
			if (idx >= 0) {
				Swap(&enemyList2[0], &enemyList2[idx]);	// move target to 1st slot.
				lastTargetID = enemyList2[0];
				this->Move(ToWorld2F(EnemyPos(enemyList2[0], false)), false);
			}
		}
		break;

		case OPTION_MELEE:
		{
			currentAction = MELEE;
			int idx = enemyList2.Find(target[OPTION_MELEE]);
			GLASSERT(idx >= 0);
			if (idx >= 0) {
				Swap(&enemyList2[0], &enemyList2[idx]);
				lastTargetID = enemyList2[0];
			}
		}
		break;
		
		case OPTION_SHOOT:
		{
			pmc->Stop();
			currentAction = SHOOT;
			int idx = enemyList2.Find(target[OPTION_SHOOT]);
			GLASSERT(idx >= 0);
			if (idx >= 0) {
				Swap(&enemyList2[0], &enemyList2[idx]);
				lastTargetID = enemyList2[0];
			}
		}
		break;

		default:
			GLASSERT( 0 );
	};

	if (debugLog) {
		static const char* optionName[NUM_OPTIONS] = { "none", "mtr", "melee", "shoot" };
		GLOUTPUT(("ID=%d BTL nEne=%d (m=%d r=%d) mtr=%.2f melee=%.2f shoot=%.2f -> %s [%d]\n",
			parentChit->ID(),
			enemyList2.Size(),
			nMeleeEnemies,
			nRangedEnemies,
			utility[OPTION_MOVE_TO_RANGE], utility[OPTION_MELEE], utility[OPTION_SHOOT],
			optionName[index],
			enemyList2[0]));
	}

	// And finally, do a swap if needed!
	// This logic is minimal: what about the other states?
	if (currentAction == MELEE) {
		thisComp.itemComponent->SelectWeapon(ItemComponent::SELECT_MELEE);
	}
	else if (currentAction == SHOOT) {
		thisComp.itemComponent->SelectWeapon(ItemComponent::SELECT_RANGED);
	}
}


void AIComponent::FlushTaskList( const ComponentSet& thisComp, U32 delta )
{
	if ( !taskList.Empty() ) {
		Vector2I pos2i    = thisComp.spatial->GetPosition2DI();
		Vector2I sector   = { pos2i.x/SECTOR_SIZE, pos2i.y/SECTOR_SIZE };

		WorkQueue* workQueue = GetWorkQueue();
		taskList.DoTasks(parentChit, delta);	
	}
}


void AIComponent::WorkQueueToTask(  const ComponentSet& thisComp )
{
	// Is there work to do?		
	Vector2I sector = thisComp.spatial->GetSector();
	CoreScript* coreScript = CoreScript::GetCore(sector);
	const ChitContext* context = Context();

	if ( coreScript ) {
		WorkQueue* workQueue = GetWorkQueue();
		if (!workQueue) return;

		// Get the current job, or find a new one.
		const WorkQueue::QueueItem* item = workQueue->GetJob( parentChit->ID() );
		if ( !item ) {
			item = workQueue->Find( thisComp.spatial->GetPosition2DI() );
			if ( item ) {
				workQueue->Assign( parentChit->ID(), item );
			}
		}
		if ( item ) {
			Vector2F dest = { 0, 0 };
			float cost = 0;

			bool hasPath = context->worldMap->CalcWorkPath(thisComp.spatial->GetPosition2D(), item->Bounds(), &dest, &cost);

			if (hasPath) {
				if (BuildScript::IsClear(item->buildScriptID)) {
					taskList.Push(Task::MoveTask(dest));
					taskList.Push(Task::StandTask(1000));
					taskList.Push(Task::RemoveTask(item->pos));
				}
				else if (BuildScript::IsBuild(item->buildScriptID)) {
					taskList.Push(Task::MoveTask(dest));
					taskList.Push(Task::StandTask(1000));
					taskList.Push(Task::BuildTask(item->pos, item->buildScriptID, LRint(item->rotation)));
				}
				else {
					GLASSERT(0);
				}
			}
		}
	}
}


void AIComponent::DoMoraleZero( const ComponentSet& thisComp )
{
	enum {
		STARVE,
		BLOODRAGE,
		VISION_QUEST,
		NUM_OPTIONS
	};

	const Needs& needs = this->GetNeeds();
	const Personality& personality = thisComp.item->GetPersonality();
	double food = needs.Value( Needs::FOOD );

	float options[NUM_OPTIONS] = {	food < 0.05				? 1.0f : 0.0f,		// starve
									personality.Fighting()	? 0.7f : 0.2f,		// bloodrage
									personality.Spiritual() ? 0.7f : 0.2f };	// quest

	int option = parentChit->random.Select( options, NUM_OPTIONS );
	this->GetNeedsMutable()->SetMorale( 1 );

	Vector2F pos2 = thisComp.spatial->GetPosition2D();
	thisComp.chit->SetTickNeeded();

	switch ( option ) {
	case STARVE:
		thisComp.item->hp = 0;
		Context()->chitBag->GetNewsHistory()->Add(NewsEvent(NewsEvent::STARVATION, pos2, parentChit->GetItemID(), 0, parentChit->Team()));
		break;

	case BLOODRAGE:
		thisComp.item->SetChaos();
		Context()->chitBag->GetNewsHistory()->Add(NewsEvent(NewsEvent::BLOOD_RAGE, pos2, parentChit->GetItemID(), 0, parentChit->Team()));
		break;

	case VISION_QUEST:
		{
			PathMoveComponent* pmc = GET_SUB_COMPONENT( parentChit, MoveComponent, PathMoveComponent );
			const SectorData* sd = 0;
			Vector2I sector = { 0, 0 };
			for( int i=0; i<16; ++i ) {
				sector.Set( parentChit->random.Rand( NUM_SECTORS ), parentChit->random.Rand( NUM_SECTORS ));
				sd = &Context()->worldMap->GetSectorData( sector );
				if ( sd->HasCore() ) {
					break;
				}
			}
			if ( sd && pmc ) {
				SectorPort sectorPort;
				sectorPort.sector = sector;
				for( int k=0; k<4; ++k ) {
					if ( sd->ports & (1<<k)) {
						sectorPort.port = 1<<k;
						break;
					}
				}
				this->Move( sectorPort, true );
				Context()->chitBag->GetNewsHistory()->Add(NewsEvent(NewsEvent::VISION_QUEST, pos2, parentChit->GetItemID(), 0, parentChit->Team()));
				thisComp.item->SetRogue();
			}
		}
		break;
	};
}


void AIComponent::EnterNewGrid( const ComponentSet& thisComp )
{
	// FIXME: this function does way too much.

	Vector2I pos2i = thisComp.spatial->GetPosition2DI();

	// Circuits.
	// FIXME: Not at all clear where this code should be...ItemComponent? MoveComponent?
	const WorldGrid& wg = Context()->worldMap->GetWorldGrid(pos2i);
	if (wg.Circuit() == CIRCUIT_DETECT_ENEMY) {
		CoreScript* cs = CoreScript::GetCore(ToSector(pos2i));
		if (cs && Team::GetRelationship(cs->ParentChit(), parentChit) == RELATE_ENEMY) {
			Context()->circuitSim->TriggerDetector(pos2i);
		}
	}

	// Is there food to eat?
	if (!thisComp.chit->Destroyed() && thisComp.item->HPFraction() < EAT_WILD_FRUIT) {
		FruitElixirFilter fruitFilter;
		Vector2F pos2 = thisComp.spatial->GetPosition2D();
		CChitArray arr;
		Context()->chitBag->QuerySpatialHash(&arr, pos2, 0.7f, 0, &fruitFilter);

		for (int i = 0; i < arr.Size(); ++i) {
			const GameItem* item = arr[i]->GetItem();
			thisComp.item->hp = thisComp.item->TotalHP();
			RenderComponent* rc = parentChit->GetRenderComponent();
			if (rc) {
				parentChit->GetRenderComponent()->AddDeco("fruit", STD_DECO);
			}
			Context()->chitBag->DeleteChit(arr[i]);
			break;
		}
	}

	// Pick up stuff in battle
	if (aiMode == BATTLE_MODE && (thisComp.item->flags & GameItem::AI_USES_BUILDINGS)) {
		if (thisComp.okay) {
			LootFilter filter;
			CChitArray arr;
			Context()->chitBag->QuerySpatialHash(&arr, thisComp.spatial->GetPosition2D(), 0.7f, 0, &filter);
			for (int i = 0; i < arr.Size() && thisComp.itemComponent->CanAddToInventory(); ++i) {
				const GameItem* item = arr[i]->GetItem();
				if (thisComp.itemComponent->IsBetterItem(item)) {
					ItemComponent* ic = arr[i]->GetItemComponent();
					arr[i]->Remove(ic);
					thisComp.itemComponent->AddToInventory(ic);
					Context()->chitBag->DeleteChit(arr[i]);
				}
			}
		}
	}

	// Domain Takeover.
	if (thisComp.item->MOB() == ISC::denizen
		&& Team::IsRogue(thisComp.chit->Team())
		&& Team::IsDefault(thisComp.item->IName(), thisComp.chit->Team()))
	{
		// FIXME: refactor this to somewhere. Part of CoreScript?
		Vector2I sector = ToSector(pos2i);
		CoreScript* cs = CoreScript::GetCore(sector);
		if (cs
			&& !cs->InUse()
			&& cs->ParentChit()->GetSpatialComponent()->GetPosition2DI() == pos2i)
		{
			// Need some team. And some cash.
			Rectangle2I inner = InnerSectorBounds(sector);
			CChitArray arr;
			ItemNameFilter filter(ISC::gobman, IChitAccept::MOB);
			Context()->chitBag->QuerySpatialHash(&arr, ToWorld(inner), parentChit, &filter);

			//GL_ARRAY_FILTER(arr, (ele->GetItem() && Team::IsRogue(ele->GetItem()->Team())));
			arr.Filter(0, [](int, Chit* ele) {
				return Team::IsRogue(ele->Team());
			});

			if (arr.Size() > 3) {
				DomainAI* ai = DomainAI::Factory(thisComp.item->Team());
				if (ai) {
					GLASSERT(Team::IsRogue(thisComp.item->Team()));
					int newCoreTeam = Team::GenTeam(thisComp.item->Team());
					CoreScript* newCS = CoreScript::CreateCore(sector, newCoreTeam, Context());
					newCS->ParentChit()->Add(ai);
					newCS->AddCitizen(thisComp.chit);

					for (int i = 0; i < arr.Size(); ++i) {
						newCS->AddCitizen(arr[i]);
					}

					NewsEvent news(NewsEvent::DOMAIN_CONQUER, newCS->ParentChit()->GetSpatialComponent()->GetPosition2D(),
								   newCS->ParentChit()->GetItemID(), parentChit->GetItemID(), newCS->ParentChit()->Team());
					Context()->chitBag->GetNewsHistory()->Add(news);
				}
			}
		}
	}

	// Check for morale-changing items (tombstones, at this writing.)
	if ( aiMode == NORMAL_MODE ) {
		if ( thisComp.item->flags & GameItem::HAS_NEEDS ) {
			RenderComponent* rc = parentChit->GetRenderComponent();
			Vector2F center = ToWorld2F( thisComp.spatial->GetPosition2DI() );	// center of the grid.
			CChitArray arr;
			const ChitContext* context = this->Context();
			LumosChitBag* chitBag = this->Context()->chitBag;

			// For now, just tombstones.
			ItemNameFilter filter(ISC::tombstone, IChitAccept::MOB);
			chitBag->QuerySpatialHash( &arr, center, 1.1f, parentChit, &filter );
			for( int i=0; i<arr.Size(); ++i ) {
				Chit* chit = arr[i];
				GameItem* item = chit->GetItem();
				GLASSERT( item );
				IString mob = item->keyValues.GetIString( "tomb_mob" );
				GLASSERT( !mob.empty() );
				int team = -1;
				item->keyValues.Get( "tomb_team", &team );
				GLASSERT( team >= 0 );

				double boost = 0;
				if ( mob == ISC::lesser )			boost = 0.05;
				else if ( mob == ISC::greater )	boost = 0.20;
				else if ( mob == ISC::denizen )	boost = 0.10;

				int relate = Team::GetRelationship( thisComp.chit->Team(), team );
				if ( relate == RELATE_ENEMY ) {
					GetNeedsMutable()->AddMorale( boost );
					if ( rc ) {
						rc->AddDeco( "happy", STD_DECO );
					}
				}
				else if ( relate == RELATE_FRIEND ) {
					GetNeedsMutable()->AddMorale( -boost );
					if ( rc ) {
						rc->AddDeco( "sad", STD_DECO );
					}				
				}
			}
		}
	}
}


bool AIComponent::AtWaypoint()
{
	CoreScript* cs = CoreScript::GetCoreFromTeam(parentChit->Team());
	if (!cs) 
		return false;

	int squad = cs->SquadID(parentChit->ID());
	if ( squad < 0) 
		return false;

	Vector2I waypoint = cs->GetWaypoint(squad);
	if (waypoint.IsZero()) 
		return false;

	Vector2F dest2 = ToWorld2F(waypoint);

	static const float DEST_RANGE = 1.0f;
	Vector2F pos2 = ParentChit()->GetSpatialComponent()->GetPosition2D();
	float len = (dest2 - pos2).Length();
	return len < DEST_RANGE;
}


bool AIComponent::ThinkWaypoints(const ComponentSet& thisComp)
{
	if (parentChit->PlayerControlled()) return false;

	CoreScript* cs = CoreScript::GetCoreFromTeam(thisComp.chit->Team());
	if (!cs) return false;
	
	int squad = cs->SquadID(parentChit->ID());
	if ( squad < 0) 
		return false;

	Vector2I waypoint = cs->GetWaypoint(squad);
	if (waypoint.IsZero()) 
		return false;

	PathMoveComponent* pmc = GET_SUB_COMPONENT(ParentChit(), MoveComponent, PathMoveComponent);
	if (!pmc) return false;

	if (AtWaypoint()) {
		bool allHere = true;
		pmc->Stop();
		CChitArray squaddies;
		cs->Squaddies(squad, &squaddies);

		for (int i = 0; i < squaddies.Size(); ++i) {
			Chit* traveller = squaddies[i];
			if (traveller && traveller->GetAIComponent() && !traveller->PlayerControlled() && !traveller->GetAIComponent()->Rampaging()) {
				if (!traveller->GetAIComponent()->AtWaypoint()) {
					allHere = false;
					break;
				}
			}
		}
		if (allHere) {
			GLOUTPUT(("Waypoint All Here. squad=%d\n", squad));
			cs->PopWaypoint(squad);
		}
		return true;
	}

	Vector2F dest2 = ToWorld2F(waypoint);
	Vector2F pos2 = thisComp.spatial->GetPosition2D();

	static const float FRIEND_RANGE = 2.0f;

	Vector2I destSector = ToSector(waypoint);
	Vector2I currentSector = thisComp.spatial->GetSector();

	if (destSector == currentSector) {
		this->Move(dest2, false);
	}
	else {
		SectorPort sp;
		const SectorData& destSD = Context()->worldMap->GetSectorData(destSector);
		int destPort = destSD.NearestPort(dest2);

		sp.sector = destSector;
		sp.port = destPort;
		this->Move(sp, false);
	}
	return true;
}




int AIComponent::DoTick( U32 deltaTime )
{
	PROFILE_FUNC();

	ComponentSet thisComp( parentChit, Chit::RENDER_BIT | 
		                               Chit::SPATIAL_BIT |
									   Chit::MOVE_BIT |
									   Chit::ITEM_BIT |
									   Chit::AI_BIT |
									   ComponentSet::IS_ALIVE |
									   ComponentSet::NOT_IN_IMPACT );
	if ( !thisComp.okay ) {
		return ABOUT_1_SEC;
	}

	// If we are in some action, do nothing and return.
	if (    parentChit->GetRenderComponent() 
		 && !parentChit->GetRenderComponent()->AnimationReady() ) 
	{
		return 0;
	}

//	wanderTime += deltaTime;
	int oldAction = currentAction;

	ChitBag* chitBag = this->Context()->chitBag;
	const ChitContext* context = Context();

	if (parentChit->ID() == 405) {
		debugLog = true;
	}

	// Clean up the enemy/friend lists so they are valid for this call stack.
	ProcessFriendEnemyLists(feTicker.Delta(deltaTime) != 0);

	// High level mode switch, in/out of battle?
	if (focus != FOCUS_MOVE &&  !taskList.UsingBuilding()) {
		CoreScript* cs = CoreScript::GetCore(thisComp.spatial->GetSector());
		// Workers only go to battle if the population is low. (Cuts down on continuous worked destruction.)
		bool goesToBattle = (thisComp.item->IName() != ISC::worker)
			|| (cs && cs->Citizens(0) <= 4);

		if (    aiMode != BATTLE_MODE 
			 && goesToBattle
			 && enemyList2.Size() ) 
		{
			aiMode = BATTLE_MODE;
			currentAction = 0;
			taskList.Clear();

			if ( debugLog ) {
				GLOUTPUT(( "ID=%d Mode to Battle\n", thisComp.chit->ID() ));
			}

			if ( parentChit->GetRenderComponent() ) {
				parentChit->GetRenderComponent()->AddDeco( "attention", STD_DECO );
			}
			for( int i=0; i<friendList2.Size(); ++i ) {
				Chit* fr = Context()->chitBag->GetChit( friendList2[i] );
				if ( fr && fr->GetAIComponent() ) {
					fr->GetAIComponent()->MakeAware( enemyList2.Mem(), enemyList2.Size() );
				}
			}
		}
		else if ( aiMode == BATTLE_MODE && enemyList2.Empty() ) {
			aiMode = NORMAL_MODE;
			currentAction = 0;
			if ( debugLog ) {
				GLOUTPUT(( "ID=%d Mode to Normal\n", thisComp.chit->ID() ));
			}
		}
	}

	
	if ( lastGrid != thisComp.spatial->GetPosition2DI() ) {
		lastGrid = thisComp.spatial->GetPosition2DI();
		EnterNewGrid( thisComp );
	}

	if  ( focus == FOCUS_MOVE ) {
		return 0;
	}

	// This is complex; HAS_NEEDS and USES_BUILDINGS aren't orthogonal. See aineeds.h
	// for an explanation.
	// FIXME: remove "lower difficulty" from needs.DoTick()
	if (thisComp.item->flags & (GameItem::HAS_NEEDS | GameItem::AI_USES_BUILDINGS)) {
		if (needsTicker.Delta(deltaTime)) {
			CoreScript* cs = CoreScript::GetCore(thisComp.spatial->GetSector());
			CoreScript* homeCore = CoreScript::GetCoreFromTeam(thisComp.chit->Team());
			bool atHomeCore = cs && (cs == homeCore);

			// Squaddies, on missions, don't have needs. Keeps them 
			// from running off or falling apart in the middle.
			// But for the other denizens:
			if (!homeCore || !homeCore->IsSquaddieOnMission(parentChit->ID())) {
				static const double LOW_MORALE = 0.25;

				if (AtFriendlyOrNeutralCore()) {
					needs.DoTick(needsTicker.Period(), aiMode == BATTLE_MODE, false, &thisComp.item->GetPersonality());

					if (thisComp.chit->PlayerControlled()) {
						needs.SetFull();
					}
					else if (!(thisComp.item->flags & GameItem::HAS_NEEDS)) {
						needs.SetMorale(1.0);	// no needs, so morale doesn't change.
					}
					else if (atHomeCore && needs.Morale() == 0) {
						DoMoraleZero(thisComp);
					}
					else if (!atHomeCore && needs.Morale() < LOW_MORALE) {
						bool okay = TravelHome(thisComp, true);
						if (!okay) needs.SetMorale(1.0);
					}
				}
				else {
					// We are wandering the world.
					if ((thisComp.item->flags & GameItem::HAS_NEEDS) && !thisComp.chit->PlayerControlled()) {
						needs.DoTravelTick(deltaTime);
						if (needs.Morale() == 0) {
							bool okay = TravelHome(thisComp, true);
							if (!okay) needs.SetMorale(1.0);
						}
					}
				}
			}
		}
	}

	// Is there work to do?
	if (    aiMode == NORMAL_MODE 
		 && (thisComp.item->flags & GameItem::AI_DOES_WORK)
		 && taskList.Empty() ) 
	{
		WorkQueueToTask( thisComp );
	}

	int RETHINK = 2000;
	if (aiMode == BATTLE_MODE) {
		RETHINK = 1000;	// so we aren't runnig past targets and such.
		// And also check for losing our target.
		if (enemyList2.Size() && (lastTargetID != enemyList2[0])) {
			if (debugLog) GLOUTPUT(("BTL retarget.\n"));
			rethink += RETHINK;
		}
	}

	if ( aiMode == NORMAL_MODE && !taskList.Empty() ) {
		FlushTaskList( thisComp, deltaTime );
		if ( taskList.Empty() ) {
			rethink = 0;
		}
	}
	else if ( !currentAction || (rethink > RETHINK ) ) {
		Think( thisComp );
		rethink = 0;
	}

	// Are we doing something? Then do that; if not, look for
	// something else to do.
	switch( currentAction ) {
		case MOVE:		
			DoMove( thisComp );
			break;
		case MELEE:		
			DoMelee( thisComp );	
			break;
		case SHOOT:		
			DoShoot( thisComp );
			break;
		case STAND:
			if ( taskList.Empty() ) {				// If there is a tasklist, it will manage standing and re-thinking.
				DoStand( thisComp, deltaTime );		// We aren't doing the tasklist stand, so do the component stand
				rethink += deltaTime;
			}
			break;

		case NO_ACTION:
			break;

		case WANDER:
			{
				PathMoveComponent* pmc = GET_SUB_COMPONENT( parentChit, MoveComponent, PathMoveComponent );
				if ( pmc && !pmc->Stopped() && !pmc->ForceCountHigh() ) {
					// okay
				}
				else {
					// not actually wandering
					currentAction = STAND;
				}
			}
			break;

		default:
			GLASSERT( 0 );
			currentAction = 0;
			break;
	}

	if ( debugLog && (currentAction != oldAction) ) {
		GLOUTPUT(( "ID=%d mode=%s action=%s\n", thisComp.chit->ID(), MODE_NAMES[aiMode], ACTION_NAMES[currentAction] ));
	}

	// Without this rethink ticker melee fighters run past
	// each other and all kinds of other silliness.
	if (aiMode == BATTLE_MODE) {
		rethink += deltaTime;
	}
	return (currentAction != NO_ACTION) ? GetThinkTime() : 0;
}


void AIComponent::DebugStr( grinliz::GLString* str )
{
	str->AppendFormat( "[AI] %s %s ", MODE_NAMES[aiMode], ACTION_NAMES[currentAction] );
}


void AIComponent::OnChitMsg(Chit* chit, const ChitMsg& msg)
{
	/* Remember this is a *synchronous* function.
	   Not safe to reach back and change other components.
	   */
	ComponentSet thisComp(parentChit, Chit::RENDER_BIT |
						  Chit::SPATIAL_BIT |
						  ComponentSet::IS_ALIVE |
						  ComponentSet::NOT_IN_IMPACT);

	if (!thisComp.okay)
		return;

	Vector2I mapPos = thisComp.spatial->GetPosition2DI();
	Vector2I sector = ToSector(mapPos);

	switch (msg.ID()) {
		case ChitMsg::CHIT_DAMAGE:
		{
			// If something hits us, make sure we notice.
			const ChitDamageInfo* info = (const ChitDamageInfo*) msg.Ptr();
			GLASSERT(info);
			this->MakeAware(&info->originID, 1);
		}
		break;

		case ChitMsg::PATHMOVE_DESTINATION_REACHED:
		destinationBlocked = 0;
		focus = 0;
		if (currentAction != WANDER) {
			currentAction = NO_ACTION;
			parentChit->SetTickNeeded();
		}

		break;

		case ChitMsg::PATHMOVE_DESTINATION_BLOCKED:
		destinationBlocked++;

		if (aiMode == BATTLE_MODE) {
			// We are stuck in battle - attack something! Keeps everyone from just
			// standing around wishing they could fight. Try to target anything
			// adjacent. If that fails, walk towards the core.
			if (!TargetAdjacent(mapPos, true)) {
				const WorldGrid& wg = Context()->worldMap->GetWorldGrid(mapPos);
				Vector2I next = mapPos + wg.Path(0);
				this->Move(ToWorld2F(next), true);
			}
		}
		else {
			focus = 0;
			currentAction = NO_ACTION;
			parentChit->SetTickNeeded();


			// Generally not what we expected.
			// Do a re-think.get
			// Never expect move troubles in rampage mode.
			if (aiMode != BATTLE_MODE) {
				aiMode = NORMAL_MODE;
			}
			taskList.Clear();
			{
				PathMoveComponent* pmc = GET_SUB_COMPONENT(parentChit, MoveComponent, PathMoveComponent);
				if (pmc) {
					pmc->Clear();	// make sure to clear the path and the queued path
				}
			}
		}
		break;

		case ChitMsg::PATHMOVE_TO_GRIDMOVE:
		{
			LumosChitBag* chitBag = Context()->chitBag;
			const SectorPort* sectorPort = (const SectorPort*)msg.Ptr();
			Vector2I sector = sectorPort->sector;
			CoreScript* cs = CoreScript::GetCore(sector);

			// Pulled out the old "summoning" system, but left in 
			// the cool message for Greaters attracted to tech.
			if (parentChit->GetItem()
				&& parentChit->GetItem()->MOB() == ISC::greater
				&& cs && (cs->GetTech() >= TECH_ATTRACTS_GREATER))
			{
				Vector2I target = cs->ParentChit()->GetSpatialComponent()->GetPosition2DI();
				Context()->chitBag->GetNewsHistory()->Add(NewsEvent(NewsEvent::GREATER_SUMMON_TECH, ToWorld2F(target), parentChit->GetItemID(), 0, parentChit->Team()));
			}
		}
		break;


		case ChitMsg::CHIT_SECTOR_HERD:
		if (parentChit->GetItem() && (parentChit->GetItem()->flags & (GameItem::AI_SECTOR_HERD | GameItem::AI_SECTOR_WANDER))) {
			// Read our destination port information:
			const SectorPort* sectorPort = (const SectorPort*)msg.Ptr();
			this->Move(*sectorPort, msg.Data() ? true : false);
		}
		break;

		case ChitMsg::WORKQUEUE_UPDATE:
		if (aiMode == NORMAL_MODE && currentAction == WANDER) {
			currentAction = NO_ACTION;
			parentChit->SetTickNeeded();
		}
		break;

		default:
		super::OnChitMsg(chit, msg);
		break;
	}
}


