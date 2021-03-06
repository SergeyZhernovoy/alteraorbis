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

#ifndef XENOENGINE_COMPONENT_INCLUDED
#define XENOENGINE_COMPONENT_INCLUDED

#include "../grinliz/gltypes.h"
#include "../grinliz/gldebug.h"
#include "../grinliz/glstringutil.h"
#include "../grinliz/glvector.h"
#include "../xarchive/glstreamer.h"
#include "xegamelimits.h"

// The primary components:
class SpatialComponent;
class RenderComponent;
class MoveComponent;
class ItemComponent;
class ScriptComponent;
class AIComponent;
class HealthComponent;
class Chit;
class ChitBag;
class ChitEvent;
class ChitMsg;
class LumosChitBag;
class ChitContext;

class Component
{
public:
	Component() : next(0), parentChit( 0 ), id( idPool++ ), willSerialize(true)	{}
	virtual ~Component()			{
		GLASSERT( parentChit == 0 );	// if this fires, this is being deleted without being Removed()
	}

	int ID() const { return id; }

	virtual void OnAdd( Chit* chit, bool initialize )	{	parentChit = chit; }
	virtual void OnRemove()								{	parentChit = 0;    }
	virtual void Serialize( XStream* xs ) = 0;

	virtual const char* Name() const = 0;

	// Utility
	Chit* ParentChit() const { return parentChit; }
	Chit* GetChit( int id );

	bool WillSerialize() const							{ return willSerialize; }
	void SetSerialize( bool s )							{ willSerialize = s; }

	virtual int DoTick( U32 delta )						{ return VERY_LONG_TICK; }
	virtual void DoUpdate()								{}
	virtual void DebugStr(grinliz::GLString* str)		{
		str->AppendFormat("[%s] ", Name());
	}

	virtual void OnChitEvent( const ChitEvent& event )			{}
	virtual void OnChitMsg( Chit* chit, const ChitMsg& msg );

	// Top level:
	virtual SpatialComponent*	ToSpatialComponent()		{ return 0; }
	virtual ItemComponent*		ToItemComponent()			{ return 0; }
	virtual RenderComponent*	ToRenderComponent()			{ return 0; }
	virtual MoveComponent*		ToMoveComponent()			{ return 0; }
	virtual ScriptComponent*	ToScriptComponent()			{ return 0; }
	virtual AIComponent*		ToAIComponent()				{ return 0; }
	virtual HealthComponent*	ToHealthComponent()			{ return 0; }

	// "private" - used by the Chit to manage a linked list of its components.
	Component* next;

protected:
	void BeginSerialize( XStream* xs, const char* name );
	void EndSerialize( XStream* sx );

	const ChitContext*	Context();
	Chit* parentChit;

private:
	int id;
	bool willSerialize;

	static int idPool;
};

class PhysicsMoveComponent;
class PathMoveComponent;
class TrackingMoveComponent;
class GridMoveComponent;

// Abstract at the XenoEngine level.
class MoveComponent : public Component
{
private:
	typedef Component super;
public:
	virtual MoveComponent*		ToMove()		{ return this; }

	virtual const char* Name() const { return "MoveComponent"; }

	virtual void Serialize( XStream* xs );
	virtual MoveComponent* ToMoveComponent()					{ return this; }
	virtual PhysicsMoveComponent*	ToPhysicsMoveComponent()	{ return 0; }
	virtual PathMoveComponent*		ToPathMoveComponent()		{ return 0; }
	virtual TrackingMoveComponent*	ToTrackingMoveComponent()	{ return 0; }
	virtual GridMoveComponent*		ToGridMoveComponent()		{ return 0; }

	// approximate, may lag, etc. useful for AI
	virtual grinliz::Vector3F Velocity() = 0;
	virtual bool IsMoving()						{ return Velocity().LengthSquared() > 0; }
	virtual bool ShouldAvoid() const			{ return true; }

protected:
};


#endif // XENOENGINE_COMPONENT_INCLUDED