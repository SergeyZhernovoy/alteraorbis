/*
Copyright (c) 2000-2012 Lee Thomason (www.grinninglizard.com)
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


#ifndef GRINLIZ_CONTAINER_INCLUDED
#define GRINLIZ_CONTAINER_INCLUDED

#include <new>
#include <cstring>

#include "gldebug.h"
#include "gltypes.h"
#include "glutil.h"

namespace grinliz
{

void TestContainers();

class CompValueBase {
public:
	// Hash table:
	template <class T>
	static U32 Hash( const T& v)					{ return (U32)(v); }
	template <class T>
	static bool Equal( const T& v0, const T& v1 )	{ return v0 == v1; }
};


class CompValue : public CompValueBase {
public:
	// Sort, FindMax:
	template <class T>
	static bool Less( const T& v0, const T& v1 )	{ return v0 < v1; }
};


class CompValueDescending : public CompValueBase {
public:
	// Sort, FindMax:
	template <class T>
	static bool Less( const T& v0, const T& v1 )	{ return v0 > v1; }
};


// I'm completely intriqued by CombSort. QuickSort is
// fast, but an intricate algorithm that is easy to
// implement an "almost working" solution. CombSort
// is very simple, and almost as fast.
// good description: http://yagni.com/combsort/

inline int CombSortGap( int gap ) 
{
	GLASSERT( (gap & 0xff000000) == 0 );
	// shrink: 1.247
	// 9 or 10 go to 11 (awesome)
	//							   0  1  2  3  4  5  6  7  8  9 10 11  12  13  14  15
	static const int table[16] = { 1, 1, 1, 2, 3, 4, 4, 5, 6, 7, 8, 8, 11, 11, 11, 12 };
	if ( gap < 16 ) {
		gap = table[gap];
	}
	else {
		gap = (gap * 103)>>7;
	}
	GLASSERT( gap  > 0 );
	return gap;
}

/*
	3 top level sorts:

	::Sort(T* mem, int size);						uses operator <
	::Sort(T* mem, int size, LessFunc func );		uses lambda function
	::SortContext(T* mem, int size, C& compare);	uses compare::Less(), where compare can be an *instance* with member vars.
*/

template <class T, typename LessFunc >
inline void Sort( T* mem, int size, LessFunc lessfunc  )
{
	int gap = size;
	for (;;) {
		gap = CombSortGap(gap);
		bool swapped = false;
		const int end = size - gap;
		for (int i = 0; i < end; i++) {
			int j = i + gap;
			if ( lessfunc(mem[j],mem[i]) ) {
				Swap(mem+i, mem+j);
				swapped = true;
			}
		}
		if (gap == 1 && !swapped) {
			break;
		}
	}
}
	

template <typename T>
inline void Sort(T* mem, int size) {
	Sort(mem, size, [](const T&a, const T&b) {
		return a < b;
	});
}

template <typename T, class COMP>
inline void SortContext( T* mem, int size, const COMP& compare )
{
	int gap = size;
	for (;;) {
		gap = CombSortGap(gap);
		bool swapped = false;
		const int end = size - gap;
		for (int i = 0; i < end; i++) {
			int j = i + gap;
			if ( compare.Less( mem[j], mem[i]) ) {
				Swap(mem+i, mem+j);
				swapped = true;
			}
		}
		if (gap == 1 && !swapped) {
			break;
		}
	}
}


template <typename T, typename Context, typename Func >
inline int ArrayFind(T* mem, int size, Context context, Func func)
{
	for (int i = 0; i < size; ++i) {
		if (func(context, mem[i])) {
			return i;
		}
	}
	return -1;
}

template <typename T, typename Context, typename Func >
inline int ArrayFindMax( T* mem, int size, Context context, Func func)
{
	int r = 0;
	auto best = func(context, mem[0]);

	for( int i=1; i<size; ++i ) {
		auto score = func(context, mem[i]);
		if (score > best) {
			best = score;
			r = i;
		}
	}
	return r;
}


class ValueSem {
public:
	template <class T>
	static void DoRemove(T& v) {}
};


class OwnedPtrSem {
public:
	template <class T>
	static void DoRemove(T& v) { delete v; }
};
 

/*	A dynamic array class that supports C++ classes.
	Carefully manages construct / destruct.
	NOTE: The array may move around in memory; classes that contain
	      pointers to their own memory will be corrupted.
*/
template < class T, class SEM=ValueSem, class KCOMPARE=CompValue >
class CDynArray
{
	enum { CACHE = 4 };
public:
	typedef T ElementType;

	CDynArray() : size( 0 ), capacity( CACHE ), nAlloc(0) {
		mem = reinterpret_cast<T*>(cache);
		GLASSERT( CACHE_SIZE*sizeof(int) >= CACHE*sizeof(T) );
	}

	~CDynArray() {
		Clear();
		if ( mem != reinterpret_cast<T*>(cache) ) {
			Free( mem );
		}
		GLASSERT( nAlloc == 0 );
	}

	T* begin() { return mem; }
	const T* begin() const { return mem; }
	T* end() { return mem + size; }
	const T* end() const { return mem + size; }

	T& operator[]( int i )				{ GLASSERT( i>=0 && i<(int)size ); return mem[i]; }
	const T& operator[]( int i ) const	{ GLASSERT( i>=0 && i<(int)size ); return mem[i]; }

	bool operator==(const CDynArray<T, SEM, KCOMPARE>& rhs) const {
		bool match = false;
		if (this->Size() == rhs.Size()) {
			match = true;
			for (int i = 0; i < Size(); ++i) {
				if ((*this)[i] == rhs[i]) {
					// all good
				}
				else {
					match = false;
					break;
				}
			}
		}
		return match;
	}

	const T& First() const { GLASSERT(size); return mem[0]; }
	const T& Last() const { GLASSERT(size); return mem[size - 1]; }

	void Push( const T& t ) {
		EnsureCap( size+1 );
#if defined( _MSC_VER )
#pragma warning ( push )
#pragma warning ( disable : 4345 )	// PODs will get constructors generated for them. Yes I know.
#endif

		new (mem+size) T( t );	// placement new copy constructor.
		
#if defined( _MSC_VER )
#pragma warning ( pop )
#endif

		++size;
		++nAlloc;
	}


	void PushFront( const T& t ) {
		EnsureCap( size+1 );
		for( int i=size; i>0; --i ) {
			mem[i] = mem[i-1];
		}
		mem[0] = t;
		++size;
		++nAlloc;
	}

	T* PushArr( int count ) {
		EnsureCap( size+count );
		T* result = &mem[size];
		for( int i=0; i<count; ++i ) {
#if defined( _MSC_VER )
#pragma warning ( push )
#pragma warning ( disable : 4345 )	// PODs will get constructors generated for them. Yes I know.
#endif

			new (result+i) T();	// placement new constructor

#if defined( _MSC_VER )
#pragma warning ( pop )
#endif
		}
		size += count;
		nAlloc += count;
		return result;
	}

	T Pop() {
		GLASSERT( size > 0 );
		--size;
		--nAlloc;

		T temp = mem[size];
		SEM::DoRemove( mem[size] );
		mem[size].~T();
		return temp;
	}

	T PopFront() {
		GLASSERT( size > 0 );
		T temp = mem[0];
		Remove( 0 );
		return temp;
	}

	void Remove( int i ) {
		GLASSERT( i < (int)size );
		// Copy down.
		for( int j=i; j<size-1; ++j ) {
			mem[j] = mem[j+1];
		}
		// Get rid of the end:
		Pop();
	}

	void SwapRemove( int i ) {
		if (i < 0) {
			GLASSERT(i == -1);
			return;
		}
		GLASSERT( i<(int)size );
		GLASSERT( size > 0 );
		
		mem[i] = mem[size-1];
		Pop();
	}

	void Reverse() {
		for (int i = 0; i < size / 2; ++i) {
			Swap(&mem[i], &mem[size - 1 - i]);
		}
	}

	int Find( const T& t ) const {
		for( int i=0; i<size; ++i ) {
			if ( mem[i] == t )
				return i;
		}
		return -1;
	}

	template<typename Context, typename Func>
	int Find(Context context, Func func) {
		return ArrayFind(mem, size, context, func);
	}

	template<typename Context, typename Func>
	int FindMax(Context context, Func func) {
		return ArrayFindMax(mem, size, context, func);
	}

	int Size() const		{ return size; }
	
	void Clear()			{ 
		while( !Empty() ) 
			Pop();
		GLASSERT( nAlloc == 0 );
		GLASSERT( size == 0 );
	}
	bool Empty() const		{ return size==0; }
	const T* Mem() const	{ return mem; }
	T* Mem()				{ return mem; }
	const T* End() const	{ return mem + size; }	// mem never 0, because of cache
	const T& Front() const	{ GLASSERT(size);  return mem[0]; }
	const T& Back() const	{ GLASSERT(size);  return mem[size-1]; }

	void Reserve(int n) { EnsureCap(n); }

	void EnsureCap( int count ) {
		if ( count > capacity ) {
			capacity = Max( CeilPowerOf2( count ), (U32) 16 );
			if ( mem == reinterpret_cast<T*>(cache) ) {
				mem = (T*)Malloc( capacity*sizeof(T) );
				memcpy( mem, cache, size*sizeof(T) );
			}
			else {
				mem = (T*)Realloc( mem, capacity*sizeof(T) );
			}
		}
	}

	void Sort() { 
		grinliz::Sort(mem, size, [](const T& a, const T& b){
			return CompValue::Less(a, b);
		});
	}
	// LessFunc returns a < b
	template<typename LessFunc>
	void Sort(LessFunc func) { grinliz::Sort<T, LessFunc>(mem, size, func); }

	// Binary Search: array must be sorted!
	int BSearch( const T& t ) const {
		int low = 0;
		int high = Size();

		while (low < high) {
			int mid = low + (high - low) / 2;
			if ( KCOMPARE::Less(mem[mid],t ))
				low = mid + 1; 
			else
				high = mid; 
		}
		if ((low < Size()) && KCOMPARE::Equal(mem[low], t)) {
			// if multiple matches, return the first.
			while (low && KCOMPARE::Equal(mem[low - 1], t)) {
				--low;
			}
			return low;
		}

		return -1;
	}

	template<typename Context, typename Func>
	void Filter(Context context, Func func) {
		for (int i = 0; i < size; ++i) {
			if (!func(context, mem[i])) {
				SwapRemove(i);
				--i;
			}
		}
	}

protected:
	CDynArray( const CDynArray<T>& );	// not allowed. Add a missing '&' in the code.
	void operator=( const CDynArray< T, SEM >& rhs );	// hard to implement with ownership semantics

	T* mem;
	int size;
	int capacity;
	int nAlloc;
	enum { 
		CACHE_SIZE = (CACHE*sizeof(T)+sizeof(int)-1)/sizeof(int)
	};
	int cache[CACHE_SIZE];
};


/*
	Sorted Array
	Does support repeated keys. (Unlike hash table.)
*/
template < class T, class SEM=ValueSem, class KCOMPARE=CompValue >
class SortedDynArray : public CDynArray< T, SEM, KCOMPARE >
{
public:
	void Add( const T& t ) {
		this->EnsureCap( this->size+1 );

		int i = this->size;
		while ( (i>0) && ( KCOMPARE::Less( t, this->mem[i-1] ))) {
			this->mem[i] = this->mem[i-1];
			--i;
		}
		this->mem[i] = t;
		++(this->size);
		++(this->nAlloc);

#ifdef DEBUG
		for (int i = 1; i < this->size; ++i) {
			// Use the !, swap the operators, becomes
			// 'greater than or equal to'
			GLASSERT(!KCOMPARE::Less(this->mem[i], this->mem[i-1]));
		}
#endif
	}
};


/* A fixed array class for any type.
   Supports copy construction, proper destruction, etc.
   Does keep the objects around, until entire CArray is destroyed,

 */
template < class T, int CAPACITY, int INIT_SIZE=0 >
class CArray
{
public:
	typedef T ElementType;

	// construction
	CArray() : size(INIT_SIZE)	{ GLASSERT(INIT_SIZE <= CAPACITY); }
	~CArray()				{}

	// operations
	T& operator[]( int i )				{ GLASSERT( i>=0 && i<(int)size ); return mem[i]; }
	const T& operator[]( int i ) const	{ GLASSERT( i>=0 && i<(int)size ); return mem[i]; }

	T* begin() { return mem; }
	const T* begin() const { return mem; }
	T* end() { return mem + size; }
	const T* end() const { return mem + size; }

	const T& Front() const { return (*this)[0]; }
	const T& Back() const { return (*this)[this->Size() - 1]; }

	// Push on
	void Push( const T& t ) {
		GLASSERT( size < CAPACITY );
		if (size < CAPACITY) 
			mem[size++] = t;
	}

	bool PushIfCap(const T& t) {
		if (size < CAPACITY) {
			mem[size++] = t;
			return true;
		}
		return false;
	}

	void Insert(int index, const T& t) {
		GLASSERT(size <= CAPACITY);
		if (size == CAPACITY) --size;
		for (int i = size; i > index; --i) {
			mem[i] = mem[i - 1];
		}
		mem[index] = t;
		++size;
	}

	// Returns space to uninitialized objects.
	T* PushArr( int n ) {
		GLASSERT( size+n <= CAPACITY );
		T* rst = &mem[size];
		size += n;
		return rst;
	}

	T Pop() {
		GLASSERT( size > 0 );
		return mem[--size];
	}

	T PopFront() {
		GLASSERT( size > 0 );
		T temp = mem[0];
		for( int i=0; i<size-1; ++i ) {
			mem[i] = mem[i+1];
		}
		--size;
		return temp;
	}

	void Crop(int maxSize) {
		GLASSERT(maxSize >= 0);
		if (size > maxSize) {
			size = maxSize;
		}
	}

	int Find( const T& key ) const { 
		for( int i=0; i<size; ++i ) {
			if ( mem[i] == key )
				return i;
		}
		return -1;
	}

	template<typename Context, typename Func>
	int Find(Context context, Func func) {
		return ArrayFind(mem, size, context, func);
	}

	template<typename Context, typename Func>
	int FindMax(Context context, Func func) {
		return ArrayFindMax(mem, size, context, func);
	}

	int Size() const		{ return size; }
	int Capacity() const	{ return CAPACITY; }
	bool HasCap() const		{ return size < CAPACITY; }
	
	void Clear()	{ 
		size = 0; 
	}
	bool Empty() const		{ return size==0; }
	T*		 Mem() 			{ return mem; }
	const T* Mem() const	{ return mem; }

	const T* End() 			{ return mem + size; }
	const T* End() const	{ return mem + size; }

	void SwapRemove( int i ) {
		GLASSERT( i >= 0 && i < (int)size );
		mem[i] = mem[size-1];
		Pop();
	}

	void Reverse() {
		for (int i = 0; i < size / 2; ++i) {
			Swap(&mem[i], &mem[size - 1 - i]);
		}
	}

	template<typename Context, typename Func>
	void Filter(Context context, Func func) {
		for (int i = 0; i < size; ++i) {
			if (!func(context, mem[i])) {
				SwapRemove(i);
				--i;
			}
		}
	}

	void Sort() { grinliz::Sort<T>(mem, size); }

	// LessFunc returns a < b
	template<typename LessFunc>
	void Sort(LessFunc func) { grinliz::Sort<T, LessFunc>(mem, size, func); }

private:
	T mem[CAPACITY];
	int size;
};


class CompCharPtr {
public:
	template <class T>
	static U32 Hash( T& _p) {
		const unsigned char* p = (const unsigned char*)_p;
		U32 hash = 2166136261UL;
		for( ; *p; ++p ) {
			hash ^= *p;
			hash *= 16777619;
		}
		return hash;
	}
	template <class T>
	static bool Equal( T& v0, T& v1 ) { return strcmp( v0, v1 ) == 0; }
};


template <class K, class V, class KCOMPARE=CompValue, class SEM=ValueSem >
class HashTable
{
public:
	HashTable() : nBuckets(0), nItems(0), nDeleted(0), reallocating(false), buckets(0) {}
	~HashTable() 
	{ 
		Clear();
		delete [] buckets;
	}

	// Adds a key/value pair. What about duplicates? Duplicate
	// keys aren't allowed. An old value will be deleted and
	// replaced.
	void Add( const K& key, const V& value ) 
	{
		if ( !reallocating ) {
			values.Clear();
			EnsureCap();
		}

		// Existing value?
		int index = FindIndex( key );
		if ( index >= 0 ) {
			// Replace!
			SEM::DoRemove( buckets[index].value );
			buckets[index].value = value;
		}
		else {
			U32 hash = KCOMPARE::Hash(key);
			while( true ) {
				hash = hash & (nBuckets-1);
				int state = buckets[hash].state;
				if ( state == UNUSED || state == DELETED ) {
					if ( state == DELETED ) 
						--nDeleted;
					++nItems;

					buckets[hash].state = IN_USE;
					buckets[hash].key   = key;
					buckets[hash].value = value;
					break;
				}
				++hash;
			}
		}
	}

	V Remove( K key ) {
		int index = FindIndex( key );
		GLASSERT( index >= 0 );
		buckets[index].state = DELETED;
		++nDeleted;
		--nItems;
		SEM::DoRemove( buckets[index].value );
		values.Clear();
		return buckets[index].value;
	}

	void Clear() {
		for( int i=0; i<nBuckets; ++i ) {
			if ( buckets[i].state == IN_USE ) {
				SEM::DoRemove( buckets[i].value );
				--nItems;
			}
			buckets[i].state = UNUSED;
		}
		values.Clear();
		GLASSERT( nItems == 0 );
		nDeleted = 0;
	}


	V Get( const K& key ) const {
		int index = FindIndex( key );
		GLASSERT( index >= 0 );
		return buckets[index].value;
	}

	bool Query( const K& key, V* value ) const {
		int index = FindIndex( key );
		if ( index >= 0 ) {
			if ( value ) {
				*value = buckets[index].value;
			}
			return true;
		}
		return false;
	}

	bool Empty() const		{ return nItems == 0; }
	int Size() const		{ return nItems; }

	const V& GetValue( int i ) {
		// Create a cache of the values, so they can be a true array.
		if ( values.Empty() ) {
			for( int i=0; i<nBuckets; ++i ) {
				if ( buckets[i].state == IN_USE ) {
					values.Push( buckets[i] );
				}
			}	
		}
		GLASSERT( values.Size() == nItems );
		return values[i].value;
	}

	const K& GetKey(int i) {
		if (values.Empty()) {
			GetValue(i);
		}
		return values[i].key;
	}

private:
	void EnsureCap() {
		if ( (nItems + nDeleted) >= nBuckets*2/3 ) {
			GLASSERT( !reallocating );
			reallocating = true;
			if ( nItems ) {
				GetValue( 0 );	// trick to copy the hashtable
			}

			int oldNItems = nItems;
			int n = CeilPowerOf2( nItems*3 );
			if ( n < 16 ) n = 16;
			if (n < oldNItems) n = oldNItems;	// because nDeleted may be the driving number.

			// Don't need to reallacote if n <= nBuckets,
			// but DO need to re-initialize.
			delete [] buckets;
			nBuckets = n;
			buckets = new Bucket[nBuckets];

			nItems = 0;
			nDeleted = 0;
			for( int i=0; i<values.Size(); ++i ) {
				Add( values[i].key, values[i].value );
			}
			values.Clear();
			reallocating = false;
		}
	}

	int FindIndex( const K& key ) const
	{
		if ( nBuckets > 0 ) {
			U32 hash = KCOMPARE::Hash( key );
			while( true ) {
				hash = hash & (nBuckets-1);
				if ( buckets[hash].state == IN_USE ) {
					const K& bkey = buckets[hash].key;
					if ( KCOMPARE::Equal( bkey, key )) {
						return hash;
					}
				}
				else if ( buckets[hash].state == UNUSED ) {
					return -1;
				}
				++hash;
			}
		}
		return -1;
	}

	int nBuckets;
	int nItems;		// in use
	int nDeleted;
	bool reallocating;

	enum {
		UNUSED,
		IN_USE,
		DELETED
	};

	struct Bucket
	{
		Bucket() : state( UNUSED ) {}
		char	state;
		K		key;
		V		value;
	};
	Bucket *buckets;

	CDynArray< Bucket > values;	// essentially a cache of the hash table.
};

}	// namespace grinliz
#endif
