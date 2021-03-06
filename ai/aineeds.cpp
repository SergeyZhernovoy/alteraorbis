#include "aineeds.h"
#include "../game/personality.h"
#include "../game/lumosmath.h"
#include "../grinliz/glutil.h"
#include "../xarchive/glstreamer.h"

#include "../game/aicomponent.h"
#include "../game/gameitem.h"
#include "../game/reservebank.h"
#include "../xegame/chit.h"
#include "../xegame/itemcomponent.h"
#include "../script/evalbuildingscript.h"
#include "marketai.h"

using namespace ai;
using namespace grinliz;

static const double DECAY_TIME = 200.0;	// in seconds

double Needs::DecayTime() { return DECAY_TIME; }


/* static */ const char* Needs::Name( int i )
{
	GLASSERT( i >= 0 && i < NUM_NEEDS );

	static const char* name[NUM_NEEDS] = { "food", "energy", "fun" };
	GLASSERT( GL_C_ARRAY_SIZE( name ) == NUM_NEEDS );
	return name[i];
}


Needs::Needs()  
{ 
	for( int i=0; i<NUM_NEEDS; ++i ) {
		need[i] = 1; 
	}
	morale = 1;
}


void Needs::DoTravelTick(U32 delta)
{
	double dMorale= double(delta) * 0.001 * 0.05 / DECAY_TIME;
	morale -= dMorale;
	morale = Clamp( morale, 0.0, 1.0 );
}


void Needs::DoTick( U32 delta, bool inBattle, bool lowerDifficulty, const Personality* personality )
{
	double dNeed = double(delta) * 0.001 / DECAY_TIME;
	if (lowerDifficulty)
		dNeed *= 0.5f;

	if (!personality) {
		for (int i = 0; i < NUM_NEEDS; ++i) {
			need[i] -= dNeed;
		}
		if (inBattle) {
			need[FUN] += dNeed * 10.0;
		}
	}
	else {
		need[FOOD]   -= dNeed;
		need[ENERGY] -= dNeed;
		need[FUN]	 -= dNeed;

		if (inBattle) {
			if (personality->Fighting() == Personality::LIKES)
				need[FUN] += dNeed * 10.0;
			else if (personality->Fighting() == Personality::INDIFFERENT)
				need[FUN] += dNeed * 2.0;
		}
	}

	ClampNeeds();

	double min = need[0];
	double max = need[0];
	for( int i=1; i<NUM_NEEDS; ++i ) {
		min = Min( need[i], min );
		max = Max( need[i], max );
	}

	if ( min < 0.001 ) {
		morale -= dNeed;
	}
	if ( min > 0.5 ) {
		morale += dNeed;
	}

	morale = Clamp( morale, 0.0, 1.0 );
}


void Needs::ClampNeeds()
{
	for( int i=0; i<NUM_NEEDS; ++i ) {
		need[i] = Clamp( need[i], 0.0, 1.0 );
	}
}


void Needs::Add( const Needs& other )
{
	for( int i=0; i<NUM_NEEDS; ++i ) {
		need[i] += other.need[i];
	}
	ClampNeeds();
}


void Needs::Add(const Vector3<double>& other)
{
	for (int i = 0; i<NUM_NEEDS; ++i) {
		need[i] += other.X(i);
	}
	ClampNeeds();
}


void Needs::Serialize(XStream* xs)
{
	bool allOne = (morale == 1);
	for (int i = 0; i < NUM_NEEDS; ++i) {
		if (need[i] != 1) {
			allOne = false;
			break;
		}
	}

	XarcOpen( xs, "Needs" );
	if (xs->Loading() || (xs->Saving() && !allOne)) {
		XARC_SER(xs, morale);
		XARC_SER_ARR(xs, need, NUM_NEEDS);
	}
	XarcClose( xs );
}


grinliz::Vector3<double> Needs::CalcNeedsFullfilledByBuilding(Chit* building, Chit* visitor)
{
	static const Vector3<double> ZERO = { 0, 0, 0 };
	static const double LIKE = 1.5;
	static const double DISLIKE = 0.7;

	GLASSERT(Needs::NUM_NEEDS == 3);		// use a Vector3
	const GameItem* buildingItem = building->GetItem();
	GLASSERT(buildingItem);
	const GameItem* visitorItem = visitor->GetItem();
	GLASSERT(visitorItem);
	if (!visitorItem || !buildingItem) return ZERO;

	GLASSERT(Needs::FOOD == 0 && Needs::ENERGY == 1 && Needs::FUN == 2);
	Vector3<double> needs = { 0, 0, 0 };

	int likesCrafting = visitorItem->GetPersonality().Crafting();
	const IString& buildingName = buildingItem->IName();

	if (buildingName == ISC::factory) {
		if (visitorItem->wallet.Crystal(0) == 0)					needs.Zero();	// can't use.
		else if (likesCrafting == Personality::LIKES)				needs.X(Needs::FUN) = LIKE;
		else if (likesCrafting == Personality::DISLIKES)			needs.X(Needs::FUN) = DISLIKE;
	}
	else if (buildingName == ISC::bed) {
		// Beds always work for energy. Up importance if wounded.
		needs.X(Needs::ENERGY) = 1.0;
	}
	else if (buildingName == ISC::market) {
		// Can always sell; if we have something to sell a market is interesting.
		needs.X(Needs::FUN) = 1.0;
		const GameItem* sell = visitor->GetItemComponent()->ItemToSell();
		if (!sell) {
			if (visitorItem->wallet.Gold() == 0)						needs.Zero();	// broke
			if (building->GetItemComponent()->NumCarriedItems() == 0)	needs.Zero();	// market empty
		}
	}
	else if (buildingName == ISC::exchange) {
		if (likesCrafting != Personality::DISLIKES) {
			// Likes crafting, wants to BUY crystal
			int cheapest = -1;
			for (int i = 0; i < NUM_CRYSTAL_TYPES; ++i) {
				if (building->GetWallet()->Crystal(i)) {
					cheapest = i;
					break;
				}
			}
			int cost = INT_MAX;
			if (cheapest >= 0) {
				cost = ReserveBank::Instance()->CrystalValue(cheapest);
			}
			if (visitorItem->wallet.Gold() >= cost) {
				needs.X(Needs::FUN) = likesCrafting ? LIKE : 1.0;
			}
		}
		else {
			// DISLIKES. only uses to sell.
			if (visitorItem->wallet.NumCrystals()) {
				needs.X(Needs::FUN) = 1.0;
			}
		}
	}
	else if (buildingName == ISC::bar) {
		if (building->GetItemComponent()->FindItem(ISC::elixir)) {
			needs.X(Needs::FOOD) = 1.0;
			needs.X(Needs::FUN)  = 0.2;
		}
	}
	else if (buildingName == ISC::academy) {
		if (visitor->GetWallet()->Gold() > ACADEMY_COST_PER_XP * 2) {
			needs.X(Needs::FUN) = 1.0;
		}
	}
	else if (buildingName == ISC::kiosk) {
		// randomly on porch?
		needs.Zero();
	}
	else{
		GLASSERT(0);	// missing some usable item.
	}
	return needs;
}
