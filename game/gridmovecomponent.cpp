#include "gridmovecomponent.h"
#include "worldinfo.h"
#include "gamelimits.h"
#include "worldmap.h"
#include "pathmovecomponent.h"

#include "../xegame/spatialcomponent.h"
#include "../xegame/rendercomponent.h"

using namespace grinliz;

static const float GRID_SPEED = MOVE_SPEED * 6.0f;
static const float GRID_ACCEL = 1.0f;

GridMoveComponent::GridMoveComponent( WorldMap* wm ) : GameMoveComponent( wm )
{
	worldMap = wm;
	state = NOT_INIT;
	velocity.Zero();
	speed = MOVE_SPEED;
}


GridMoveComponent::~GridMoveComponent()
{
}


void GridMoveComponent::DebugStr( grinliz::GLString* str )
{
	str->Format( "[GridMove] " );
}


void GridMoveComponent::Serialize( XStream* xs )
{
	this->BeginSerialize( xs, Name() );
	XARC_SER( xs, state );
	XARC_SER( xs, portDest );
	XARC_SER( xs, deleteWhenDone );
	XARC_SER( xs, speed );
	XARC_SER( xs, sectorDest );
	this->EndSerialize( xs );
}


void GridMoveComponent::OnAdd( Chit* chit )
{
	super::OnAdd( chit );
}


void GridMoveComponent::OnRemove()
{
	super::OnRemove();
}


bool GridMoveComponent::IsMoving() const
{
	// This is something of a problem. Are we moving
	// if on transit? Mainly used by the animation system,
	// so the answer is 'no', but not quite what you
	// might expect.
	return false;
}


void GridMoveComponent::SetDest( int sectorX, int sectorY, int port )
{
	GLASSERT( state == NOT_INIT );
	sectorDest.Set( sectorX, sectorY );
	portDest = port;
	state = ON_BOARD;

	// Error check:
	worldMap->GetWorldInfo().GetGridEdge( sectorDest, portDest );
}


int GridMoveComponent::DoTick( U32 delta, U32 since )
{
	if ( state == DONE ) {
		if ( deleteWhenDone ) {
			Chit* chit = parentChit;	// gets nulled in Remove()
			chit->Remove( this );
			chit->Add( new PathMoveComponent( map ));
			delete this;
		}
		return VERY_LONG_TICK;
	}
	if ( state == NOT_INIT ) {
		return VERY_LONG_TICK;
	}

	SpatialComponent* sc = parentChit->GetSpatialComponent();
	if ( !sc ) return VERY_LONG_TICK;

	int tick = 0;
	Vector2F pos = sc->GetPosition2D();

	// Speed up or slow down, depending
	// on on or off board.
	if ( state != OFF_BOARD ) {
		if ( speed < GRID_SPEED ) {
			speed += Travel( GRID_ACCEL, since );
			if ( speed > GRID_SPEED ) speed = GRID_SPEED;
		}
	}
	else {
		speed -= Travel( GRID_ACCEL, since );
		if ( speed < MOVE_SPEED ) speed = MOVE_SPEED;
	}

	Vector2F dest = { 0, 0 };
	int stateIfDestReached = DONE;
	bool goToDest = true;
	const WorldInfo& worldInfo = worldMap->GetWorldInfo();

	switch ( state ) 
	{
	
	case ON_BOARD:
		// Presume we started somewhere rational. Move to grid.
		{
			int port = 0;
			const SectorData& sd = worldInfo.GetSectorInfo( pos.x, pos.y, &port );
			Vector2I sector = { sd.x / SECTOR_SIZE, sd.y / SECTOR_SIZE };
			GridEdge gridEdge = worldInfo.GetGridEdge( sector, port );
			GLASSERT( worldInfo.HasGridEdge( gridEdge ));

			dest = worldInfo.GridEdgeToMapF( gridEdge );
			stateIfDestReached = TRAVELLING;
		}
		break;

	case OFF_BOARD:
		{
			const SectorData& sd = worldInfo.GetSector( sectorDest );
			Rectangle2I portBounds = sd.GetPortLoc( portDest );
			dest = SectorData::PortPos( portBounds, parentChit->ID() );
			stateIfDestReached = DONE;
		}
		break;

	case TRAVELLING:
		{	
			GridEdge destEdge = worldInfo.GetGridEdge( sectorDest, portDest );
			Vector2F destPt   = worldInfo.GridEdgeToMapF( destEdge ); 
			
			Rectangle2F destRect;
			destRect.min = destRect.max = destPt;
			destRect.Outset( 0.2f );

			goToDest = false;
			if ( destRect.Contains( pos )) {
				state = OFF_BOARD;
			}
			else {
				if ( path.Size() == 0 ) {
					GridEdge current = worldMap->GetWorldInfo().MapToGridEdge( (int)pos.x, (int)pos.y );
					int result = worldMap->GetWorldInfoMutable()->Solve( current, destEdge, &path );
					GLASSERT( result == micropather::MicroPather::SOLVED );
				}

				float travel = Travel( speed, since );
				while ( travel > 0 && !path.Empty() ) {
					GridEdge ge   = path[0];
					Vector2F target = worldInfo.GridEdgeToMapF( ge );

					Vector2F delta = target - pos;
					float len = delta.Length();
					if ( len <= travel ) {
						travel -= len;
						pos = target;
						path.Remove( 0 );	// FIXME: potentially expensive - use index
					}
					else {
						Vector2F dir = { Sign( delta.x ), Sign( delta.y ) };
						pos = pos + dir * travel;
						travel = 0;
					}
				}

			}
		}
		break;

	};

	if ( goToDest ) {
		Vector2F dir = dest - pos;
		float distance = dir.Length();
		if ( distance > 0 ) {
			dir.Multiply( 1.0f / distance );	// Normalize.

			float travel = Travel( speed, since );
			velocity.Set( dir.x*speed, 0, dir.y*speed );

			if ( travel > distance ) travel = distance;
			pos = pos + dir * travel;
			if ( travel >= distance ) {
				state = stateIfDestReached;
				pos = dest;
			}
		}
		else {
			state = stateIfDestReached;
		}
	}
	sc->SetPosition( pos.x, 0, pos.y );
	return tick;
}
