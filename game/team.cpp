#include "team.h"
#include "visitorweb.h"
#include "../script/corescript.h"
#include "../grinliz/glutil.h"
#include "../xegame/istringconst.h"
#include "../xegame/chit.h"
#include "../xegame/itemcomponent.h"
#include "../xarchive/glstreamer.h"

using namespace grinliz;

Team* Team::instance = 0;

Team::Team()
{
	GLASSERT(!instance);
	instance = this;
	idPool = 1;	// id=0 is rogue.
}

Team::~Team()
{
	GLASSERT(instance == this);
	instance = 0;
}

void Team::Serialize(XStream* xs)
{
	XarcOpen(xs,"Team");
	XARC_SER(xs, idPool);

	XarcOpen(xs, "attitude");
	if (xs->Saving()) {
		int size = hashTable.Size();
		XARC_SER_KEY(xs, "size", size);
		for (int i = 0; i < size; ++i) {
			XarcOpen(xs, "teamkey");
			const TeamKey& tk = hashTable.GetKey(i);
			int t0 = tk.T0();
			int t1 = tk.T1();
			int a = hashTable.GetValue(i);
			XARC_SER_KEY(xs, "t0", t0);
			XARC_SER_KEY(xs, "t1", t1);
			XARC_SER_KEY(xs, "a", a);
			XarcClose(xs);
		}
	}
	else {
		hashTable.Clear();
		int size = 0;
		XARC_SER_KEY(xs, "size", size);
		for (int i = 0; i < size; ++i) {
			XarcOpen(xs, "teamkey");
			int t0, t1, a;
			XARC_SER_KEY(xs, "t0", t0);
			XARC_SER_KEY(xs, "t1", t1);
			XARC_SER_KEY(xs, "a", a);
			TeamKey tk(t0, t1);
			hashTable.Add(tk, a);
			XarcClose(xs);
		}
	}
	XarcClose(xs);	// attitude
	XarcClose(xs);	// team
}

grinliz::IString Team::TeamName(int team)
{
	IString name;
	CStr<64> str;
	int group = 0, id = 0;
	SplitID(team, &group, &id);

	switch (group) {
		case TEAM_HOUSE:
		if (id)
			str.Format("House-%x", id);
		else
			str = "House";
		name = StringPool::Intern(str.c_str());
		break;

		case TEAM_TROLL:
		// Since Trolls can't build anything,
		// any troll core is by definition
		// Truulga. (At least at this point.)
		name = ISC::Truulga;
		break;

		case TEAM_GOB:
		if (id)
			str.Format("Gobmen-%x", id);
		else
			str = "Gobmen";
		name = StringPool::Intern(str.c_str());
		break;

		case TEAM_KAMAKIRI:
		if (id)
			str.Format("Kamakiri-%x", id);
		else
			str = "Kamakiri";
		name = StringPool::Intern(str.c_str());
		break;

		case TEAM_DEITY:
		switch (id) {
			case 0: name = ISC::deity; break;
			case DEITY_MOTHER_CORE:	name = ISC::MotherCore; break;
			default: GLASSERT(0);
		}
		break;

		case TEAM_CHAOS:
		case TEAM_NEUTRAL:
		break;

		default:
		GLASSERT(0);
		break;
	}

	return name;
}


int Team::GetTeam( const grinliz::IString& itemName )
{
	if (itemName == ISC::trilobyte) {
		return TEAM_RAT;
	}
	else if ( itemName == ISC::mantis ) {
		return TEAM_GREEN_MANTIS;
	}
	else if ( itemName == ISC::redMantis ) {
		return TEAM_RED_MANTIS;
	}
	else if ( itemName == ISC::troll ) {
		return TEAM_TROLL;
	}
	else if (itemName == ISC::gobman) {
		return TEAM_GOB;
	}
	else if (itemName == ISC::kamakiri) {
		return TEAM_KAMAKIRI;
	}
	else if (itemName == ISC::deity || itemName == ISC::MotherCore){
		return TEAM_DEITY;
	}
	else if (    itemName == ISC::cyclops
		      || itemName == ISC::fireCyclops
		      || itemName == ISC::shockCyclops )
	{
		return TEAM_CHAOS;
	}
	else if (    itemName == ISC::human) 
	{
		return TEAM_HOUSE;
	}
	GLASSERT(0);
	return TEAM_NEUTRAL;
}


bool Team::IsDefault(const IString& str, int team)
{
	if (team == TEAM_NEUTRAL || team == TEAM_CHAOS) return true;
	return GetTeam(str) == Group(team);
}


ERelate Team::BaseRelationship( int _t0, int _t1 )
{
	int t0 = 0, t1 = 0;
	int g0 = 0, g1  =0 ;
	SplitID(_t0, &t0, &g0);
	SplitID(_t1, &t1, &g1);

	// t0 <= t1 to keep the logic simple.
	if ( t0 > t1 ) Swap( &t0, &t1 );

	// Neutral is just neutral. Else Chaos units
	// keep attacking neutral cores. Very annoying.
	if (t0 == TEAM_NEUTRAL || t1 == TEAM_NEUTRAL)
		return ERelate::NEUTRAL;

	// CHAOS hates all - even each other.
	if ( t0 == TEAM_CHAOS || t1 == TEAM_CHAOS)
		return ERelate::ENEMY;

	GLASSERT(t0 >= TEAM_RAT && t0 < NUM_TEAMS);
	GLASSERT(t1 >= TEAM_RAT && t1 < NUM_TEAMS);

	static const int F = int(ERelate::FRIEND);
	static const int E = int(ERelate::ENEMY);
	static const int N = int(ERelate::NEUTRAL);
	static const int OFFSET = TEAM_RAT;
	static const int NUM = NUM_TEAMS - OFFSET;

	static const int relate[NUM][NUM] = {
		{ F, E, E, E, E, E, E, E, N },		// rat
		{ 0, F, E, N, E, E, F, N, N },		// green
		{ 0, 0, F, N, E, E, E, E, N },		// red
		{ 0, 0, 0, F, E, N, N, N, N },		// troll 
		{ 0, 0, 0, 0, F, N, E, F, N },		// house
		{ 0, 0, 0, 0, 0, F, E, F, N },		// gobmen
		{ 0, 0, 0, 0, 0, 0, F, N, N },		// kamakiri
		{ 0, 0, 0, 0, 0, 0, 0, F, F },		// visitor
		{ 0, 0, 0, 0, 0, 0, 0, 0, N },		// deity
	};
	GLASSERT(t0 - OFFSET >= 0 && t0 - OFFSET < NUM);
	GLASSERT(t1 - OFFSET >= 0 && t1 - OFFSET < NUM);
	GLASSERT(t1 >= t0);

	// Special handling for left/right battle scene matchups:
	if (   t0 == t1 
		&& ((g0 == TEAM_ID_LEFT && g1 == TEAM_ID_RIGHT) || (g0 == TEAM_ID_RIGHT && g1 == TEAM_ID_LEFT))) 
	{
		return ERelate::ENEMY;
	}
	return ERelate(relate[t0-OFFSET][t1-OFFSET]);
}


ERelate Team::GetRelationship(Chit* chit0, Chit* chit1)
{
	if (chit0->GetItem() && chit1->GetItem()) {
		ERelate r = GetRelationship(chit0->GetItem()->Team(), chit1->GetItem()->Team());
		// Check symmetry:
		GLASSERT(r == GetRelationship(chit1->GetItem()->Team(), chit0->GetItem()->Team()));
		return r;
	}
	return ERelate::NEUTRAL;
}


ERelate Team::GetRelationship(int t0, int t1)
{
	TeamKey tk0(t0, t1);
	TeamKey tk1(t1, t0);
	int d0 = 0, d1 = 0;

	if (!hashTable.Query(tk0, &d0)) {
		d0 = RelationshipToAttitude(BaseRelationship(t0, t1));
	}
	if (!hashTable.Query(tk1, &d1)) {
		d1 = RelationshipToAttitude(BaseRelationship(t0, t1));
	}
	// Combined relationship is the worse one.
	int d = Min(d0, d1);
	ERelate r = AttitudeToRelationship(d);
	return r;
}


int Team::Attitude(CoreScript* center, CoreScript* eval)
{
	int t0 = center->ParentChit()->Team();
	int t1 = eval->ParentChit()->Team();

	TeamKey tk(t0, t1);
	int d0 = 0;
	if (hashTable.Query(tk, &d0)) {
		return d0;
	}
	ERelate relate = BaseRelationship(t0, t1);
	int d = RelationshipToAttitude(relate);
	return d;
}

int Team::CalcAttitude(CoreScript* center, CoreScript* eval, const Web* web)
{
	GLASSERT(eval != center);

	// Positive: more friendly
	// Negative: more enemy

	// Species 
	int centerTeam = center->ParentChit()->Team();
	int evalTeam = eval->ParentChit()->Team();
	if (Team::IsDeityCore(centerTeam)) return 0;

	const bool ENVIES_WEALTH = (centerTeam == TEAM_GOB);
	const bool ENVIES_TECH = (centerTeam == TEAM_KAMAKIRI);
	const bool WARLIKE = (centerTeam == TEAM_KAMAKIRI);

	ERelate relate = BaseRelationship(centerTeam, evalTeam);
	int d = 0;

	switch (relate) {
		case ERelate::FRIEND:	d = d + 2;	break;
		case ERelate::ENEMY:	d = d - 1;	break;
	}

	// Compete for Visitors
	Vector2I sector = ToSector(center->ParentChit()->Position());
	// There should be a webNode where we are, of course,
	// but that depends on bringing the web cache current.
	const MinSpanTree::Node* webNode = web->FindNode(sector);
	GLASSERT(webNode);
	if (!webNode) return 0;
	float visitorStr = webNode->strength;

	// An an alternate world where 'eval' is gone...what happens?
	Web altWeb;
	Vector2I altSector = ToSector(eval->ParentChit()->Position());
	altWeb.Calc(&altSector);
	const MinSpanTree::Node* altWebNode = altWeb.FindNode(sector);
	float altVisitorStr = altWebNode->strength;

	if (altVisitorStr > visitorStr) {
		d--;		// better off if they are gone...
	}
	else if (altVisitorStr < visitorStr) {
		d += WARLIKE ? 1 : 2;		// better off with them around...
	}

	// Techiness, envy of Kamakiri
	if (ENVIES_TECH && eval->GetTech() > center->GetTech()) {
		d--;
	}

	// Wealth, envy of the Gobmen
	if (ENVIES_WEALTH && ((eval->CoreWealth() * 2 / 3) > center->CoreWealth())) {
		d--;
	}

	TeamKey tk(centerTeam, evalTeam);
	hashTable.Add(tk, d);
	return d;
}

