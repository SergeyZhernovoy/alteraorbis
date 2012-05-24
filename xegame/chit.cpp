#include "chit.h"
#include "chitbag.h"
#include "component.h"
#include "spatialcomponent.h"
#include "rendercomponent.h"

#include "../grinliz/glstringutil.h"

using namespace grinliz;

Chit::Chit( int _id, ChitBag* bag ) :	chitBag( bag ), id( _id ), nTickers( 0 )
{
	next = 0;
	ticking = false;
	for( int i=0; i<NUM_SLOTS; ++i ) {
		slot[i] = 0;
	}
}


Chit::~Chit()
{
	for( int i=0; i<NUM_SLOTS; ++i ) {
		if ( slot[i] ) {
			slot[i]->OnRemove();
			delete slot[i];
		}
	}
}

	
void Chit::Add( Component* c )
{
	if ( c->ToSpatial() ) {
		GLASSERT( !slot[SPATIAL] );
		slot[SPATIAL] = c;
		GLASSERT( spatialComponent == c );
	}
	else if ( c->ToRender() ) {
		GLASSERT( !slot[RENDER] );
		slot[RENDER] = c;
		GLASSERT( renderComponent == c );
	}
	else if ( c->ToMove() ) {
		GLASSERT( !slot[MOVE] );
		slot[MOVE] = c;
		GLASSERT( moveComponent == c );
	}
	else {
		int i;
		for( i=GENERAL_START; i<GENERAL_END; ++i ) {
			if ( slot[i] == 0 ) {
				slot[i] = c;
				break;
			}
		}
		GLASSERT( i < NUM_SLOTS );
	}
	c->OnAdd( this );
	if ( c->NeedsTick() ) {
		++nTickers;
	}
}


void Chit::Remove( Component* c )
{
	if ( c->NeedsTick() )
		--nTickers;

	for( int i=0; i<NUM_SLOTS; ++i ) {
		if ( slot[i] == c ) {
			c->OnRemove();
			slot[i] = 0;
			return;
		}
	}
	GLASSERT( 0 );	// not found
}


Component* Chit::GetComponent( const char* name )
{
	for( int i=0; i<NUM_SLOTS; ++i ) {
		if ( slot[i] && slot[i]->ToComponent( name )) {
			return slot[i];
		}
	}
	return 0;
}


void Chit::RequestUpdate()
{
	if ( chitBag ) {
		chitBag->RequestUpdate( this );
	}
}



void Chit::DoTick( U32 delta )
{
	GLASSERT( NeedsTick() );
	ticking = true;
	for( int i=0; i<NUM_SLOTS; ++i ) {
		if ( slot[i] ) 
			slot[i]->DoTick( delta );
	}
	ticking = false;
}


void Chit::DoUpdate()
{
	for( int i=0; i<NUM_SLOTS; ++i ) {
		if ( slot[i] ) 
			slot[i]->DoUpdate();
	}
}


void Chit::SendMessage( int msgID )
{
	for( int i=0; i<listeners.Size(); ++i ) {
		listeners[i]->OnChitMsg( this, msgID );
	}

	int i=0;
	while ( i<cListeners.Size() ) {
		Chit* target = GetChitBag()->GetChit( cListeners[i].chitID );
		if ( target == 0 ) {
			cListeners.SwapRemove(i);
			continue;
		}
		bool okay = target->CarryMsg( cListeners[i].componentID, this, msgID );
		if ( !okay ) {
			// dead link
			cListeners.SwapRemove(i);
			continue;
		}
		++i;
	}
}


bool Chit::CarryMsg( int componentID, Chit* src, int msgID )
{
	for( int i=0; i<NUM_SLOTS; ++i ) {
		if ( slot[i] && slot[i]->ID() == componentID ) {
			slot[i]->OnChitMsg( src, msgID );
			return true;
		}
	}
	return false;
}


void Chit::AddListener( IChitListener* listener )
{
	GLASSERT( listeners.Find( listener ) < 0 );
	listeners.Push( listener );
}


void Chit::RemoveListener( IChitListener* listener )
{
	int i = listeners.Find( listener );
	GLASSERT( i >= 0 );
	listeners.SwapRemove( i );
}


void Chit::AddListener( Component* c )
{
	CList data;
	data.chitID = c->ParentChit()->ID();
	data.componentID = c->ID();

	cListeners.Push( data );
}


void Chit::RemoveListener( Component* c ) 
{
	CList data;
	data.chitID = c->ParentChit()->ID();
	data.componentID = c->ID();

	for( int i=0; i<cListeners.Size(); ++i ) {
		if ( cListeners[i].chitID == data.chitID && cListeners[i].componentID == data.componentID ) {
			cListeners.SwapRemove( i );
			return;
		}
	}
	GLASSERT( 0 );
}


void Chit::DebugStr( GLString* str )
{
	str->Format( "Chit=%x ", this );

	for( int i=0; i<NUM_SLOTS; ++i ) {
		if ( slot[i] ) {
			unsigned size = str->size();
			slot[i]->DebugStr( str );
			if ( size == str->size() ) {
				str->Format( "[] " );
			}
		}
	}
}

