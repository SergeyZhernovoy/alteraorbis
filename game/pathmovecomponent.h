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

#ifndef PATH_MOVE_COMPONENT_INCLUDED
#define PATH_MOVE_COMPONENT_INCLUDED

#include "../grinliz/glvector.h"
#include "../grinliz/glcontainer.h"
#include "gamelimits.h"
#include "gamemovecomponent.h"
#include "sectorport.h"

class WorldMap;

/*	Move along a path.
	Future note: Pathing should be async and done by message passing.
*/
class PathMoveComponent : public GameMoveComponent
{
private:
	typedef GameMoveComponent super;
public:

	PathMoveComponent(	WorldMap* _map )				// required; used to avoids blocks when moving. 
		: GameMoveComponent( _map ), nPathPos( 0 ), pathPos( 0 ), pathDebugging( false ), forceCount( 0 )
	{ 
		queued.Clear();
		dest.Clear();
		SetNoPath(); 
	}
	virtual ~PathMoveComponent() {}

	virtual const char* Name() const { return "PathMoveComponent"; }
	virtual PathMoveComponent* ToPathMoveComponent() { return this; }

	virtual void DebugStr( grinliz::GLString* str );

	virtual void Serialize( XStream* xs );

	virtual void OnAdd( Chit* chit );
	virtual void OnRemove();
	virtual int DoTick( U32 delta );
	virtual void OnChitMsg( Chit* chit, const ChitMsg& msg );

	virtual void CalcVelocity( grinliz::Vector3F* v );

	void QueueDest( grinliz::Vector2F dest,
					float rotation = -1.f,			// if specified, the rotation we wish to get to
					const SectorPort* sector=0 );	// if specified, the sector travel target
	void QueueDest( Chit* targetChit );
	bool QueuedDest( grinliz::Vector2F* dest ) const {
		if ( queued.pos.x >= 0 ) { *dest = queued.pos; return true; }
		return false;
	}
	const grinliz::Vector2F& DestPos() const { return dest.pos; }

	void Stop()				{ SetNoPath(); }
	void Clear()			{ Stop(); queued.Clear(); }
	bool Stopped() const	{ return !HasPath() && queued.pos.x < 0; }

	void SetPathDebugging( bool d )	{ pathDebugging = d; }

	// Status info
	int BlockForceApplied() const	{ return blockForceApplied; }
	bool IsAvoiding() const			{ return avoidForceApplied; }

	int ForceCount() const			{ return forceCount; }
	bool ForceCountHigh() const		{ return forceCount > FORCE_COUNT_HIGH; }
	// Mainly an animation query. Since it responds to what happened - not 
	// the actual state - should not be used for AI queries.
	virtual bool IsMoving() const	{ return isMoving; }
	
private:
	enum {
		FORCE_COUNT_HIGH = 10,
		FORCE_COUNT_EXCESSIVE = 40
	};

	// Commit the 'queued' to the 'dest', if possible. 
	void ComputeDest();
	bool NeedComputeDest();
	
	void GetPosRot( grinliz::Vector2F* pos, float* rot );
	void SetPosRot( grinliz::Vector2F pos, float rot );
	float GetDistToNext2( const grinliz::Vector2F& currentPos );

	void SetNoPath() {
		nPathPos = pathPos = forceCount = 0;
		dest.Clear();
	}
	bool HasPath() const {
		return dest.pos.x >= 0 && nPathPos > 0;
	}

	// Rotate, then move in that direction.
	void RotationFirst( U32 delta );
	// Try to avoid walking through others.
	// Returns 'true' if the destination is being squatted.
	bool AvoidOthers( U32 delta );

	int nPathPos;				// size of path
	int pathPos;				// index of where we are on path

	struct Dest {
		void Clear() { pos.Set( -1, -1 ); rotation = -1.f; sectorPort.Zero(); }

		grinliz::Vector2F	pos;
		float				rotation;	// <0 means ignore
		SectorPort			sectorPort;
	};

	Dest queued;	// queued up, 
	Dest dest;		// in use

	grinliz::Vector2F pos2;		// only valid during tick!
	float    rot;				// only valid during tick!

	bool pathDebugging;

	bool blockForceApplied;		
	bool avoidForceApplied;
	bool isMoving;
	int  forceCount;

	grinliz::CDynArray< Chit* > chitArr;
	grinliz::Vector2F path[MAX_MOVE_PATH];
};

#endif // PATH_MOVE_COMPONENT_INCLUDED
