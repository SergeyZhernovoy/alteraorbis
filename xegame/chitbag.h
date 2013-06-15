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

#ifndef XENOENGINE_CHITBAG_INCLUDED
#define XENOENGINE_CHITBAG_INCLUDED

#include "../grinliz/gldebug.h"
#include "../grinliz/gltypes.h"
#include "../grinliz/glrandom.h"

#include "../engine/enginelimits.h"
#include "../engine/bolt.h"

#include "chit.h"
#include "xegamelimits.h"
#include "chitevent.h"

class Engine;
class ComponentFactory;
class XStream;
class CameraComponent;

#define CChitArray grinliz::CArray<Chit*, 32 >

class ChitBag : public IBoltImpactHandler
{
public:
	ChitBag();
	virtual ~ChitBag();

	// Chit creation/query
	void DeleteAll();
	Chit* NewChit( int id=0 );
	void  DeleteChit( Chit* );
	Chit* GetChit( int id );

	void Serialize( const ComponentFactory* factory, XStream* xs );

	// Bolts are a special kind of chit. Just easier
	// and faster to treat them as a 2nd stage.
	Bolt* NewBolt();
	const Bolt* BoltMem() const { return bolts.Mem(); }
	int NumBolts() const { return bolts.Size(); }

	// Calls every chit that has a tick.
	void DoTick( U32 delta, Engine* engine );	
	U32 AbsTime() const { return bagTime; }

	int NumChits() const { return chitID.NumValues(); }
	int NumTicked() const { return nTicked; }

	// Due to events, changes, etc. a chit may need an update, possibily in addition to, the tick.
	// Normally called automatically.
	void QueueDelete( Chit* chit );
	void QueueRemoveAndDeleteComponent( Component* comp );
	void DeferredDelete( Component* comp );

	// passes ownership
	void QueueEvent( const ChitEvent& event )			{ events.Push( event ); }

	void AddNews( const NewsEvent& event );
	const NewsEvent* News() const { return news.Mem(); }
	int NumNews() const { return news.Size(); }
	void SetNewsProcessed();

	// Hashes based on integer coordinates. No need to call
	// if they don't change.
	void AddToSpatialHash( Chit*, int x, int y );
	void RemoveFromSpatialHash( Chit*, int x, int y );
	void UpdateSpatialHash( Chit*, int x0, int y0, int x1, int y1 );

	static bool AcceptAll( Chit* ) {
		return true;
	}
	static bool HasMoveComponentFilter( Chit* chit );
	static bool HasAIComponentFilter( Chit* chit );

	void QuerySpatialHash(	grinliz::CDynArray<Chit*>* array, 
							const grinliz::Rectangle2F& r, 
		                    const Chit* ignoreMe,
							bool (*accept)(Chit*) = AcceptAll );	// function ptr: see NoFilter

	// Use with caution: the array returned can change if a sub-function calls this.
	void QuerySpatialHash(	CChitArray* arr,
							const grinliz::Rectangle2F& r, 
							const Chit* ignoreMe,
							bool (*accept)(Chit*) = AcceptAll );

	void QuerySpatialHash(	grinliz::CDynArray<Chit*>* array,
							const grinliz::Vector2F& origin, 
							float rad,
							const Chit* ignoreMe,
							bool (*accept)(Chit*) = AcceptAll )
	{
		grinliz::Rectangle2F r;
		r.Set( origin.x-rad, origin.y-rad, origin.x+rad, origin.y+rad );
		QuerySpatialHash( array, r, ignoreMe, accept );
	}
	// Use with caution: the array returned can change if a sub-function calls this.
	void QuerySpatialHash(	CChitArray* array,
							const grinliz::Vector2F& origin, 
							float rad,
							const Chit* ignoreMe,
							bool (*accept)(Chit*) = AcceptAll )
	{
		grinliz::Rectangle2F r;
		r.Set( origin.x-rad, origin.y-rad, origin.x+rad, origin.y+rad );
		QuerySpatialHash( array, r, ignoreMe, accept );
	}

	// IBoltImpactHandler
	virtual void HandleBolt( const Bolt& bolt, const ModelVoxel& mv );

	// There can only be one camera actually in use:
	CameraComponent* GetCamera( Engine* engine );
	int GetCameraChitID() const { return activeCamera; }

	virtual LumosChitBag* ToLumos() { return 0; }

protected:
	void DeleteChits() { chitID.RemoveAll(); }

private:
	enum {
		SHIFT = 2,	// a little tuning done; seems reasonable
		SIZE = MAX_MAP_SIZE >> SHIFT
	};
	U32 HashIndex( U32 x, U32 y ) const {
		return ( (y>>SHIFT)*SIZE + (x>>SHIFT) );
	}

	int idPool;
	U32 bagTime;
	int nTicked;
	int activeCamera;

	struct CompID { 
		int chitID;
		int compID;
	};

	enum { BLOCK_SIZE = 1000 };
	Chit* memRoot;
	// Use a memory pool so the chits don't move on re-allocation. I
	// keep wanting to use a DynArray, but that opens up issues of
	// Chit copying (which they aren't set up for.) It keeps going 
	// squirrely. If inclined, I think a custom array based on realloc
	// is probably the correct solution, but the current approach 
	// may be fine.
	grinliz::CDynArray< Chit* >		  blocks;
	grinliz::HashTable< int, Chit* >  chitID;
	grinliz::CDynArray<int>			deleteList;	
	grinliz::CDynArray<CompID>		compDeleteList;		// delete and remove list
	grinliz::CDynArray<Component*>	zombieDeleteList;	// just delete
	grinliz::CDynArray<Chit*>		hashQuery;			// local data, cached at class level
	grinliz::CDynArray<Chit*>		cachedQuery;		// local data, cached at class level
	grinliz::CDynArray<ChitEvent>	events;
	grinliz::CDynArray<NewsEvent>	news;
	
	grinliz::CDynArray<Bolt>		bolts;
	
	Chit* spatialHash[SIZE*SIZE];
};


#endif // XENOENGINE_CHITBAG_INCLUDED