/*
Copyright (c) 2000-2003 Lee Thomason (www.grinninglizard.com)
Grinning Lizard Utilities.

This software is provided 'as-is', without any express or implied 
warranty. In no event will the authors be held liable for any 
damages arising from the use of this software.

Permission is granted to anyone to use this software for any 
purpose, including commercial applications, and to alter it and 
redistribute it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must 
not claim that you wrote the original software. If you use this 
software in a product, an acknowledgment in the product documentation 
would be appreciated but is not required.

2. Altered source versions must be plainly marked as such, and 
must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any source 
distribution.
*/


#ifndef GRINLIZ_MEMORY_POOL
#define GRINLIZ_MEMORY_POOL

#include "gldebug.h"
#include <new>

namespace grinliz {

/*	
	A memory pool that will dynamically grow as memory is needed.
*/
class MemoryPool
{
  public:
	MemoryPool( const char* _name, unsigned objectSize, unsigned blockSize = 16*1024, bool warnOnGrowth=false );
	~MemoryPool();

	void* Alloc();
	void Free( void* mem );

	void FreePool();

	unsigned Blocks() const				{ return numBlocks; }
	unsigned NumObjects() const			{ return numObjects; }
	unsigned ObjectWatermark() const	{ return numObjectsWatermark; }

	unsigned MemoryInUse() const			{ return numObjects * objectSize; }
	unsigned MemoryInUseWatermark()	const 	{ return numObjectsWatermark * objectSize; }
	unsigned MemoryAllocated() const		{ return numBlocks * blockSize; }

	bool Empty() const						{ return numObjects == 0; }
	bool MemoryInPool( void* mem ) const;

  private:
	struct Block
	{
		Block* nextBlock;
	};
	struct Object
	{
		Object* next;
	};

	void NewBlock();

	void* MemStart( Block* block ) const
	{ 
		return block+1; 
	}
	void* MemEnd( Block* block ) const		
	{ 
		return (U8*)MemStart( block ) + objectsPerBlock*objectSize;
	}
	Object* GetObject( Block* block, unsigned i )	
	{	
		GLASSERT( i<objectsPerBlock );
		U8* mem = (U8*)MemStart( block ) + i*objectSize;
		return (Object*)mem;
	}

	unsigned objectSize;		// size of chunk in bytes
	unsigned blockSize;			// size of block in bytes
	unsigned objectsPerBlock;
	
	unsigned numBlocks;
	unsigned numObjects;
	unsigned numObjectsWatermark;
	
	Block* rootBlock;
	Object* head;

	const char* name;
	bool warn;
};

template < class T >
class MemoryPoolT
{
public:
	MemoryPoolT( const char* name ) : pool( name, sizeof(T) ) 	{}
	~MemoryPoolT() {}

	T* New() {
		void* mem = pool.Alloc();
		return new (mem) T();
	}
	void Delete( T* mem ) {
		if ( mem ) {
			mem->~T();
			pool.Free( mem );
		}
	}

	bool Empty() const	{ return pool.Empty(); }
	void FreePool()		{ pool.FreePool(); }

private:
	MemoryPool pool;
};


};	// grinliz

#endif
