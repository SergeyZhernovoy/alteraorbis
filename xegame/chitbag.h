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
#include "../grinliz/glspatialhash.h"

#include "cticker.h"

#include "../engine/enginelimits.h"
#include "../engine/bolt.h"

#include "chit.h"
#include "xegamelimits.h"
#include "../game/news.h"
#include "chitevent.h"

class Engine;
class ComponentFactory;
class XStream;
class CameraComponent;

#define USE_SPACIAL_HASH

#define CChitArray grinliz::CArray<Chit*, 32 >

// Controls tuning of spatial hash. Helps, but 
// still spending way too much time finding out 
// what is around a given chit.
#define SPATIAL_VAR

// This ticks per-component instead of per-chit.
// Rather expected this to make a difference
// (cache use) but doesn't.
//#define OUTER_TICK

class IChitAccept
{
public:
	virtual bool Accept( Chit* chit ) = 0;
};


class ChitAcceptAll : public IChitAccept
{
public:
	virtual bool Accept( Chit* chit )	{ return true; }
};

class ChitHasMoveComponent : public IChitAccept
{
public:
	virtual bool Accept( Chit* chit );
};

class ChitHasAIComponent : public IChitAccept
{
public:
	virtual bool Accept( Chit* chit );
};

class MultiFilter : public IChitAccept
{
public:
	enum {
		MATCH_ANY,
		MATCH_ALL
	};
	MultiFilter( int anyAllMatch ) : anyAll(anyAllMatch) {}
	virtual bool Accept( Chit* chit );

	// Remember: Up to 4 filters causes no memory allocation.
	grinliz::CDynArray< IChitAccept* > filters;
private:
	int anyAll;
};


class WorldMap;
class LumosGame;
class CircuitSim;

class ChitBag : public IBoltImpactHandler
{
public:
	ChitBag( const ChitContext& );
	virtual ~ChitBag();

	// Chit creation/query
	void  DeleteAll();
	Chit* NewChit( int id=0 );
	void  DeleteChit( Chit* );
	Chit* GetChit( int id ) const;

	virtual void Serialize( XStream* xs );

	void SetAreaOfInterest(const grinliz::Rectangle3F& aoi) { areaOfInterest = aoi; }

	// Bolts are a special kind of chit. Just easier
	// and faster to treat them as a 2nd stage.
	Bolt* NewBolt();
	const Bolt* BoltMem() const { return bolts.Mem(); }
	int NumBolts() const { return bolts.Size(); }

	// Calls every chit that has a tick.
	virtual void DoTick( U32 delta );	
	U32 AbsTime() const { return bagTime; }

	int NumChits() const { return chitID.Size(); }
	int NumTicked() const { return nTicked; }

	// Due to events, changes, etc. a chit may need an update, possibily in addition to, the tick.
	// Normally called automatically.
	void QueueDelete( Chit* chit );
	void QueueRemoveAndDeleteComponent( Component* comp );
	void DeferredDelete( Component* comp );
	bool IsQueuedForDelete(Chit* chit);

	// passes ownership
	void QueueEvent( const ChitEvent& event )			{ events.Push( event ); }

	// Hashes based on integer coordinates. No need to call
	// if they don't change.
	void AddToSpatialHash( Chit*, int x, int y );
	void RemoveFromSpatialHash( Chit*, int x, int y );
	void UpdateSpatialHash( Chit*, int x0, int y0, int x1, int y1 );

	void QuerySpatialHash(	grinliz::CDynArray<Chit*>* array, 
							const grinliz::Rectangle2F& r, 
		                    const Chit* ignoreMe,
							IChitAccept* filter );

	void QuerySpatialHash(	CChitArray* arr,
							const grinliz::Rectangle2F& r, 
							const Chit* ignoreMe,
							IChitAccept* filter );

	void QuerySpatialHash(	grinliz::CDynArray<Chit*>* array, 
							const grinliz::Rectangle2I& r, 
							const Chit* ignoreMe,
							IChitAccept* filter );

	void QuerySpatialHash(	CChitArray* arr,
							const grinliz::Rectangle2I& r, 
							const Chit* ignoreMe,
							IChitAccept* filter );

	void QuerySpatialHash(	grinliz::CDynArray<Chit*>* array,
							const grinliz::Vector2F& origin, 
							float rad,
							const Chit* ignoreMe,
							IChitAccept* accept )
	{
		grinliz::Rectangle2F r;
		r.Set( origin.x-rad, origin.y-rad, origin.x+rad, origin.y+rad );
		QuerySpatialHash( array, r, ignoreMe, accept );
	}

	void QuerySpatialHash(	CChitArray* array,
							const grinliz::Vector2F& origin, 
							float rad,
							const Chit* ignoreMe,
							IChitAccept* accept )
	{
		grinliz::Rectangle2F r;
		r.Set( origin.x-rad, origin.y-rad, origin.x+rad, origin.y+rad );
		QuerySpatialHash( array, r, ignoreMe, accept );
	}


	// Tell all the Chits in an area to tick; event has happened
	// and they need to look around.
	void SetTickNeeded(const grinliz::Rectangle2F& bounds);
	void SetTickNeeded(const grinliz::Vector2F& origin, float rad) {
		grinliz::Rectangle2F r;
		r.min = r.max = origin;
		r.Outset(rad);
		SetTickNeeded(r);
	}

	// IBoltImpactHandler
	virtual void HandleBolt( const Bolt& bolt, const ModelVoxel& mv );

	// There can only be one camera actually in use:
//	CameraComponent* GetCamera( Engine* engine );
//	int GetCameraChitID() const { return activeCamera; }
	
	void  SetNamedChit(const grinliz::IString& name, Chit* chit);
	Chit* GetNamedChit(const grinliz::IString& name) const;

	virtual LumosChitBag* ToLumos() { return 0; }
	const ChitContext* Context() const { return &chitContext; }

	NewsHistory* GetNewsHistory() { return newsHistory;  }

	struct CurrentNews {
		grinliz::IString name;
		grinliz::Vector2F pos;
		int chitID;
	};
	const grinliz::CDynArray<CurrentNews>& GetCurrentNews() const { return currentNews; }
	void PushCurrentNews(const CurrentNews& news);

	// Slow iteration: (for census)
	int NumBlocks() const;
	void GetBlockPtrs( int block, grinliz::CDynArray<Chit*>* arr ) const;

	// VERY wide broadcast - be cautious of performance.
	// WARNING: not serialized.
	void AddListener(IChitListener* handler) {
		GLASSERT(listeners.Find(handler) < 0);
		listeners.Push(handler);
	}

	void RemoveListener(IChitListener* handler) {
		int i = listeners.Find(handler);
		GLASSERT(i >= 0);
		if (i >= 0) listeners.SwapRemove(i);
	}

	void SendMessage(Chit* chit, const ChitMsg& msg) {
		for (int i = 0; i < listeners.Size(); ++i) {
			listeners[i]->OnChitMsg(chit, msg);
		}
	}

	static void Test();

protected:
	const ChitContext& chitContext;

private:
	void InnerQuerySpatialHash(grinliz::CDynArray<Chit*>* array,
							   const grinliz::Rectangle2I& queryBounds,
							   const grinliz::Rectangle2F& searchBounds,
							   const Chit* ignoreMe,
							   IChitAccept* accept);

	void ProcessDeleteList();

	grinliz::CDynArray< IChitListener* > listeners;

	enum {
		SIZE = 256,
		SIZE2 = SIZE*SIZE,
		SHIFT = 2
	};

	U32 HashIndex( U32 x, U32 y ) const {
		return (y >> SHIFT)*SIZE + (x >> SHIFT);
	}

	int idPool;
	U32 bagTime;
	int nTicked;
	int frame;
//	int activeCamera;
	NewsHistory* newsHistory;
	grinliz::Rectangle3F areaOfInterest;

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
	grinliz::CDynArray<Bolt>		bolts;
	grinliz::CDynArray<CurrentNews> currentNews;
	grinliz::HashTable<grinliz::IString, int, grinliz::CompValueString>	namedChits;	// not serialized; generated by OnAdd

#ifdef USE_SPACIAL_HASH
	grinliz::SpatialHash<Chit*> spatialHash;
	CTicker debugTick;
#else
	Chit* spatialHash[SIZE2];
#endif
};


#endif // XENOENGINE_CHITBAG_INCLUDED