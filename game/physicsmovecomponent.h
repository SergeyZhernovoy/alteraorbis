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

#ifndef PHYSICS_MOVE_COMPONENT_INCLUDED
#define PHYSICS_MOVE_COMPONENT_INCLUDED

#include "gamemovecomponent.h"

class WorldMap;

class PhysicsMoveComponent : public GameMoveComponent
{
private:
	typedef GameMoveComponent super;
public:
	PhysicsMoveComponent();
	virtual ~PhysicsMoveComponent() {}

	virtual const char* Name() const { return "PhysicsMoveComponent"; }
	virtual PhysicsMoveComponent* ToPhysicsMoveComponent() { return this; }
	virtual void DebugStr( grinliz::GLString* str );

	virtual void Serialize( XStream* xs );

	virtual void OnAdd( Chit* chit, bool init );
	virtual void OnRemove();
	virtual int DoTick( U32 delta );
	virtual bool IsMoving() const;
	virtual grinliz::Vector3F Velocity( grinliz::Vector3F* v )	{ return velocity; }

	void Set( const grinliz::Vector3F& _velocity, float _rotation );
	void Add( const grinliz::Vector3F& _velocity, float _rotation );

private:
	grinliz::Vector3F velocity;
	float rotation;
};


class TrackingMoveComponent : public GameMoveComponent
{
private:
	typedef GameMoveComponent super;
public:
	TrackingMoveComponent();
	virtual ~TrackingMoveComponent();

	virtual const char* Name() const { return "TrackingMoveComponent"; }
	virtual TrackingMoveComponent* ToTrackingMoveComponent() { return this; }
	virtual void DebugStr( grinliz::GLString* str ) {
		str->AppendFormat( "[TrackingMove] target=%d ", target );
	}

	virtual void Serialize( XStream* xs );
	virtual int  DoTick( U32 since );
	virtual bool IsMoving();
	virtual grinliz::Vector3F Velocity();

	void SetTarget( int chitID )				{ target = chitID; }
	virtual bool ShouldAvoid() const			{ return false; }

private:
	int target;
};


#endif // PHYSICS_MOVE_COMPONENT_INCLUDED
