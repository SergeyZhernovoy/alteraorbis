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

#ifndef AI_COMPONENT_INCLUDED
#define AI_COMPONENT_INCLUDED

#include "../xegame/component.h"
#include "../xegame/cticker.h"
#include "../grinliz/glcontainer.h"
#include "../grinliz/glrectangle.h"
#include "../script/battlemechanics.h"

class WorldMap;
class Engine;
struct ComponentSet;

inline float UtilityCubic( float y0, float y1, float x ) {
	x = grinliz::Clamp( x, 0.0f, 1.0f );
	float a = grinliz::Lerp( y0, y1, x*x*x );
	return a;
}

inline float UtilityFade( float y0, float y1, float x ) {
	x = grinliz::Clamp( x, 0.0f, 1.0f );
	float a = grinliz::Lerp( y0, y1, grinliz::Fade5( x ) );
	return a;
}

inline float UtilityLinear( float y0, float y1, float x ) {
	x = grinliz::Clamp( x, 0.0f, 1.0f );
	float a = grinliz::Lerp( y0, y1, x );
	return a;
}

// Highest at x=0.5
inline float UtilityParabolic( float y0, float maxY, float y1, float x ) {
	x = grinliz::Clamp( x, 0.0f, 1.0f );
	float xp = x - 0.5f;
	float a = 1.0f - ( xp*xp )*4.f;
	return a;
}


// Combat AI: needs refactoring
class AIComponent : public Component
{
private:
	typedef Component super;

public:
	AIComponent( Engine* _engine, WorldMap* _worldMap );
	virtual ~AIComponent();

	virtual const char* Name() const { return "AIComponent"; }

	virtual void Serialize( XStream* xs );

	virtual int  DoTick( U32 delta, U32 timeSince );
	virtual void DebugStr( grinliz::GLString* str );
	virtual void OnChitMsg( Chit* chit, const ChitMsg& msg );
	virtual void OnChitEvent( const ChitEvent& event );

	// Approximate. enemyList may not be flushed.
	bool AwareOfEnemy() const { return enemyList.Size() > 0; }

	// Top level AI modes.
	enum {
		NORMAL_MODE,
		BATTLE_MODE
	};

private:
	enum {
		FRIENDLY,
		ENEMY,
		NEUTRAL,
		
		MAX_TRACK = 8,
	};

	void GetFriendEnemyLists( const grinliz::Rectangle2F* area );
	int GetTeamStatus( Chit* other );
	bool LineOfSight( const ComponentSet& thisComp, Chit* target );

	void Think( const ComponentSet& thisComp );	// Choose a new action.
	void ThinkWander( const ComponentSet& thisComp );
	void ThinkBattle( const ComponentSet& thisComp );

	Engine*		engine;
	WorldMap*	map;

	enum {
		/*
			move,			get to shot, closer to friends, further from friends, close with enemy
			move & shoot	
			move & reload
			move & melee	going in for the hit (ignore friends)
			stand & shoot	good shot, good range, want aim bonus
			stand & reload	
			stand			stuck?
		*/

		// Possible actions:
		NO_ACTION,
		MELEE,			// Go to the target and hit it. The basic combat action.
						// Possibly run-and-gun on the way in.
		SHOOT,			// Stand ground and shoot.
		MOVE,			// Move to a better location. Possibly reload.
		NUM_ACTIONS,
	};

	int aiMode;
	int currentAction;
	int currentTarget;
	bool focusOnTarget;
	CTicker		thinkTicker;

	void DoMelee( const ComponentSet& thisComp );
	void DoShoot( const ComponentSet& thisComp );
	void DoMove( const ComponentSet& thisComp );

	grinliz::CArray<int, MAX_TRACK> friendList;
	grinliz::CArray<int, MAX_TRACK> enemyList;
	BattleMechanics battleMechanics;
};


#endif // AI_COMPONENT_INCLUDED