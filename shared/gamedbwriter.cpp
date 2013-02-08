/*
 Copyright (c) 2010 Lee Thomason

 This software is provided 'as-is', without any express or implied
 warranty. In no event will the authors be held liable for any damages
 arising from the use of this software.

 Permission is granted to anyone to use this software for any purpose,
 including commercial applications, and to alter it and redistribute it
 freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not
    claim that you wrote the original software. If you use this software
    in a product, an acknowledgment in the product documentation would be
    appreciated but is not required.

    2. Altered source versions must be plainly marked as such, and must not be
    misrepresented as being the original software.

    3. This notice may not be removed or altered from any source
    distribution.
*/

#include "gamedbwriter.h"
#include "../grinliz/glstringutil.h"
#include "../zlib/zlib.h"
#include "gamedb.h"

#include <algorithm>


#pragma warning ( disable : 4996 )
using namespace gamedb;
using namespace grinliz;

void WriteU32( FILE* fp, U32 val )
{
	fwrite( &val, sizeof(U32), 1, fp );
}


Writer::Writer()
{
	stringPool = new StringPool();
	root = new WItem( "root", 0, this );
}

Writer::~Writer()
{
	delete root;
	delete stringPool;
}


void Writer::Save( const char* filename )
{
	FILE* fp = fopen( filename, "wb" );
	GLASSERT( fp );
	if ( !fp ) {
		return;
	}

	// Get all the strings used for anything to build a string pool.
	CDynArray< IString > poolVec;
	stringPool->GetAllStrings( &poolVec );

	// Sort them.
	Sort< IString, CompValue>( poolVec.Mem(), poolVec.Size() );

	// --- Header --- //
	HeaderStruct headerStruct;	// come back later and fill in.
	fwrite( &headerStruct, sizeof(HeaderStruct), 1, fp );

	// --- String Pool --- //
	headerStruct.nString = poolVec.Size();
	U32 mark = ftell( fp ) + 4*poolVec.Size();		// position of first string.
	for( int i=0; i<poolVec.Size(); ++i ) {
		WriteU32( fp, mark );
		mark += poolVec[i].size() + 1;				// size of string and null terminator.
	}
	for( int i=0; i<poolVec.Size(); ++i ) {
		fwrite( poolVec[i].c_str(), poolVec[i].size()+1, 1, fp );
	}
	// Move to a 32-bit boundary.
	while( ftell(fp)%4 ) {
		fputc( 0, fp );
	}

	// --- Items --- //
	headerStruct.offsetToItems = ftell( fp );
	grinliz::CDynArray< WItem::MemSize > dataPool;
	grinliz::CDynArray< U8 > buffer;

	root->Save( fp, poolVec, &dataPool );

	// --- Data Description --- //
	headerStruct.offsetToDataDesc = ftell( fp );

	// reserve space for offsets
	grinliz::CDynArray< DataDescStruct > ddsVec;
	ddsVec.PushArr( dataPool.Size() );
	fwrite( &ddsVec[0], sizeof(DataDescStruct)*ddsVec.Size(), 1, fp );

	// --- Data --- //
	headerStruct.offsetToData = ftell( fp );
	headerStruct.nData = dataPool.Size();

	for( int i=0; i<dataPool.Size(); ++i ) {
		WItem::MemSize* m = &dataPool[i];

		ddsVec[i].offset = ftell( fp );
		ddsVec[i].size = m->size;

		int compressed = 0;
		uLongf compressedSize = 0;

		if ( m->size > 20 && dataPool[i].compressData ) {
			buffer.Clear();
			buffer.PushArr( m->size*8 / 10 );
			compressedSize = buffer.Size();

			int result = compress( (Bytef*)&buffer[0], &compressedSize, (const Bytef*)m->mem, m->size );
			if ( result == Z_OK && compressedSize < (uLongf)m->size )
				compressed = 1;
		}

		if ( compressed ) {
			fwrite( &buffer[0], compressedSize, 1, fp );
			ddsVec[i].compressedSize = compressedSize;
		}
		else {
			fwrite( m->mem, m->size, 1, fp );
			ddsVec[i].compressedSize = m->size;
		}
	}
	unsigned totalSize = ftell( fp );

	// --- Data Description, revisited --- //
	fseek( fp, headerStruct.offsetToDataDesc, SEEK_SET );
	fwrite( &ddsVec[0], sizeof(DataDescStruct)*ddsVec.Size(), 1, fp );

	// Go back and patch header:
	fseek( fp, 0, SEEK_SET );
	fwrite( &headerStruct, sizeof(headerStruct), 1, fp );
	fclose( fp );

	GLOUTPUT(( "Database write complete. size=%dk stringPool=%dk tree=%dk data=%dk\n",
			   totalSize / 1024,
			   headerStruct.offsetToItems / 1024,
			   ( headerStruct.offsetToData - headerStruct.offsetToItems ) / 1024,
			   ( totalSize - headerStruct.offsetToData ) / 1024 ));
}


WItem::WItem( const char* name, const WItem* parent, Writer* p_writer )
{
	GLASSERT( name && *name );
	itemName = p_writer->stringPool->Get( name );
	this->parent = parent;
	writer = p_writer;
	offset = 0;
	attrib = 0;
}


WItem::~WItem()
{
	{
		std::map<IString, WItem*>::iterator itr;
		for(itr = child.begin(); itr != child.end(); ++itr) {
			delete (*itr).second;
		}
	}
	{
		// Free the copy of the memory.
		while ( attrib ) {
			Attrib* a = attrib->next;
			writer->attribMem.Delete( attrib );
			attrib = a;
		}
	}
	// Everything else cleaned by destructors.
}


WItem* WItem::FetchChild( const char* name )
{
	GLASSERT( name && *name );
	IString iname = writer->stringPool->Get( name );
	std::map<IString, WItem*>::iterator itr = child.find( iname );
	if ( itr == child.end() )
		return CreateChild( name );
	return (*itr).second;
}


WItem* WItem::CreateChild( const char* name )
{
	GLASSERT( name && *name );
	IString iname = writer->stringPool->Get( name );
	GLASSERT( child.find( iname ) == child.end() );
	
	WItem* witem = new WItem( name, this, writer );
	child[ iname ] = witem;
	return witem;
}


WItem* WItem::CreateChild( int id )
{
	char buffer[32];
	grinliz::SNPrintf( buffer, 32, "%d", id );
	return CreateChild( buffer );
}


void WItem::Attrib::Free()
{
	if ( type == ATTRIBUTE_DATA ) {
		free( data );
	}
	Clear();
}


void WItem::SetData( const char* name, const void* d, int nData, bool useCompression )
{
	GLASSERT( name && *name );
	GLASSERT( d && nData );

	void* mem = malloc( nData );
	memcpy( mem, d, nData );

	Attrib* a = writer->attribMem.New();
	a->Clear();
	a->name = writer->stringPool->Get( name );
	a->type = ATTRIBUTE_DATA;
	a->data = mem;
	a->dataSize = nData;
	a->compressData = useCompression;

	a->next = attrib;
	attrib = a;
}


void WItem::SetInt( const char* name, int value )
{
	GLASSERT( name && *name );

	Attrib* a = writer->attribMem.New();
	a->Clear();
	a->name = writer->stringPool->Get( name );
	a->type = ATTRIBUTE_INT;
	a->intVal = value;

	a->next = attrib;
	attrib = a;
}


void WItem::SetFloat( const char* name, float value )
{
	GLASSERT( name && *name );

	Attrib* a = writer->attribMem.New();
	a->Clear();
	a->name = writer->stringPool->Get( name );
	a->type = ATTRIBUTE_FLOAT;
	a->floatVal = value;

	a->next = attrib;
	attrib = a;
}


void WItem::SetString( const char* name, const char* str )
{
	GLASSERT( name && *name );

	Attrib *a = writer->attribMem.New();
	a->Clear();
	a->name = writer->stringPool->Get( name );
	a->type = ATTRIBUTE_STRING;
	a->stringVal = writer->stringPool->Get( str );

	a->next = attrib;
	attrib = a;
}


void WItem::SetBool( const char* name, bool value )
{
	GLASSERT( name && *name );
	
	Attrib* a = writer->attribMem.New();
	a->Clear();
	a->name = writer->stringPool->Get( name );
	a->type = ATTRIBUTE_BOOL;
	a->intVal = value ? 1 : 0;

	a->next = attrib;
	attrib = a;
}


int WItem::FindString( const IString& str, const CDynArray< IString >& stringPool )
{
	int index = stringPool.BSearch( str );
	GLASSERT( index >= 0 );	// should always succeed!
	return index;
}


void WItem::Save(	FILE* fp, 
					const CDynArray< IString >& stringPool, 
					CDynArray< MemSize >* dataPool )
{
	offset = ftell( fp );

	// Pull out the linked list and sort it.
	CDynArray< Attrib* >& attribArr = writer->attribArr;
	attribArr.Clear();
	for( Attrib* a = attrib; a; a=a->next ) {
		attribArr.Push( a );
	}
	Sort< Attrib*, CompAttribPtr >( attribArr.Mem(), attribArr.Size() );

	ItemStruct itemStruct;
	itemStruct.nameID = FindString( itemName, stringPool );
	itemStruct.offsetToParent = Parent() ? Parent()->offset : 0;
	itemStruct.nAttrib = attribArr.Size();
	itemStruct.nChildren = child.size();

	fwrite( &itemStruct, sizeof(itemStruct), 1, fp );

	// Reserve the child locations.
	int markChildren = ftell( fp );
	for( unsigned i=0; i<child.size(); ++i )
		WriteU32( fp, 0 );
	
	// And now write the attributes:
	AttribStruct aStruct;

	for( int i=0; i<attribArr.Size(); ++i ) {
		Attrib* a = attribArr[i];
		aStruct.SetKeyType( FindString( a->name, stringPool ), a->type );

		switch ( a->type ) {
			case ATTRIBUTE_DATA:
				{
					int id = dataPool->Size();
					MemSize m = { a->data, a->dataSize, a->compressData };
					dataPool->Push( m );
					aStruct.dataID = id;
				}
				break;

			case ATTRIBUTE_INT:
				aStruct.intVal = a->intVal;
				break;

			case ATTRIBUTE_FLOAT:
				aStruct.floatVal = a->floatVal;
				break;

			case ATTRIBUTE_STRING:
				aStruct.stringID = FindString( a->stringVal, stringPool );
				break;

			case ATTRIBUTE_BOOL:
				aStruct.intVal = a->intVal;
				break;

			default:
				GLASSERT( 0 );
		}
		fwrite( &aStruct, sizeof(aStruct), 1, fp );
	}

	// Save the children
	std::map<IString, WItem*>::iterator itr;
	int n = 0;

	for(itr = child.begin(); itr != child.end(); ++itr, ++n ) {
		// Back up, write the current position to the child offset.
		int current = ftell(fp);
		fseek( fp, markChildren+n*4, SEEK_SET );
		WriteU32( fp, current );
		fseek( fp, current, SEEK_SET );

		// save
		itr->second->Save( fp, stringPool, dataPool );
	}
}
