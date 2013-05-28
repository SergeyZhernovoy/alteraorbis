#include "glstreamer.h"

using namespace grinliz;


XStream::XStream()
{
}


XStream::~XStream()
{
}


StreamWriter::StreamWriter( FILE* p_fp ) : XStream(), fp( p_fp ), depth( 0 ) 
{
	// Write the header:
	int version = 1;
	idPool = 0;
	WriteInt( version );
}


StreamWriter::~StreamWriter()
{
}

void StreamWriter::WriteInt( int value )
{
	// Very simple compression. Lead 4 bits
	// saves the sign & #bytes needed.
	//
	// 1: sign (1:positive, 0:negative)
	// 3: 0-4 number of bytes that follow
	U8 lead=0;
	if ( value >= 0 ) {
		lead |= (1<<7);
	}
	else {
		value *= -1;
	}

	int nBits = LogBase2(value)+1;
	int nBytes = (nBits+3)/8;
	lead |= nBytes << 4;

	lead |= value & 0xf;
	fputc( lead, fp );
	value >>= 4;

	for( int i=0; i<nBytes; ++i ) {
		fputc( value & 0xff, fp );
		value >>= 8;
	}
}


int StreamReader::ReadInt()
{
	int value = 0;
	int lead = getc( fp );
	int sign = (lead & (1<<7)) ? 1 : -1;
	int nBytes = (lead & 0x70)>>4;

	value = lead & 0xf;
	for( int i=0; i<nBytes; ++i ) {
		value |= (getc(fp) << (4+i*8));
	}
	return value * sign;
}


int StreamReader::PeekByte()
{
	int value = 0;
	int lead = getc( fp );
	int sign = (lead & (1<<7)) ? 1 : -1;
	int nBytes = (lead & 0x70)>>4;
	GLASSERT( nBytes == 0 );

	value = lead & 0xf;
	ungetc( lead, fp );
	return value * sign;
}


void StreamWriter::WriteString( const char* str )
{
	// Write the name.
	// Has it been written before? Re-use, positive index.
	// Else, negative index, then add string.

	// Special case empty string.
	if ( str == 0 || *str == 0 ) {
		WriteInt( 0 );
		return;
	}

	int index=0;
	if ( strToIndex.Query( str, &index )) {
		WriteInt( index );
	}
	else {
		idPool++;
		WriteInt( -idPool );
		int len = strlen( str );
		WriteInt( len );
		fwrite( str, len, 1, fp );

		IString istr = StringPool::Intern( str );
		indexToStr.Add( idPool, istr.c_str() );
		strToIndex.Add( istr.c_str(), idPool );
	}
}


const char* StreamReader::ReadString()
{
	int id = ReadInt();

	if ( id == 0 ) {
		return "";
	}

	const char* r = 0;

	if ( id < 0 ) {
		int len = ReadInt();
		strBuf.Clear();
		char* buf = strBuf.PushArr( len );
		fread( buf, len, 1, fp );
		strBuf.Push(0);

		IString istr = StringPool::Intern( buf );
		indexToStr.Add( -id, istr.c_str() );
		r = istr.c_str();
	}
	else {
		r = indexToStr.Get( id );
	}
	return r;
}


void StreamWriter::WriteFloat( float value )
{
	fwrite( &value, sizeof(value), 1, fp );
}


float StreamReader::ReadFloat()
{
	float v = 0;
	fread( &v, sizeof(v), 1, fp );
	return v;
}


void StreamWriter::WriteDouble( double value )
{
	fwrite( &value, sizeof(value), 1, fp );
}


double StreamReader::ReadDouble()
{
	double v=0;
	fread( &v, sizeof(v), 1, fp );
	return v;
}


void StreamWriter::OpenElement( const char* str ) 
{
	++depth;
	WriteInt( BEGIN_ELEMENT );
	WriteString( str );
}


void StreamWriter::CloseElement()
{
	GLASSERT( depth > 0 );
	--depth;
	WriteInt( END_ELEMENT );
}


void StreamWriter::SetArr( const char* key, const char* value[], int n )
{
	WriteInt( ATTRIB_STRING );
	WriteString( key );
	WriteInt( n );
	for( int i=0; i<n; ++i ) {
		WriteString( value[i] );
	}
}




void StreamWriter::SetArr( const char* key, const U8* value, int n )
{
	WriteInt( ATTRIB_INT );
	WriteString( key );
	WriteInt( n );
	for( int i=0; i<n; ++i ) {
		WriteInt( value[i] );
	}
}


void StreamWriter::SetArr( const char* key, const int* value, int n )
{
	WriteInt( ATTRIB_INT );
	WriteString( key );
	WriteInt( n );
	for( int i=0; i<n; ++i ) {
		WriteInt( value[i] );
	}
}


void StreamWriter::SetArr( const char* key, const float* value, int n )
{
	WriteInt( ATTRIB_FLOAT );
	WriteString( key );
	WriteInt( n );
	for( int i=0; i<n; ++i ) {
		WriteFloat( value[i] );
	}
}


void StreamWriter::SetArr( const char* key, const double* value, int n )
{
	WriteInt( ATTRIB_DOUBLE );
	WriteString( key );
	WriteInt( n );
	for( int i=0; i<n; ++i ) {
		WriteDouble( value[i] );
	}
}

StreamReader::StreamReader( FILE* p_fp ) : XStream(), fp( p_fp ), depth(0), version(0)
{
	version = ReadInt();
}


const char* StreamReader::OpenElement()
{
	int node = ReadInt();
	GLASSERT( node == BEGIN_ELEMENT );
	const char* elementName = ReadString();

	attributes.Clear();
	intData.Clear();
	floatData.Clear();
	doubleData.Clear();

	// Attributes:
	while ( true ) {
		int b = PeekByte();
		if ( b >= ATTRIB_START && b < ATTRIB_END ) {
			Attribute a;
			a.type = ReadInt();
			a.key  = ReadString();
			a.n    = ReadInt();

			switch ( a.type ) {
			case ATTRIB_INT:
				a.offset = intData.Size();
				for( int i=0; i<a.n; ++i ) {
					intData.Push( ReadInt() );
				}
				break;

			case ATTRIB_FLOAT:
				a.offset = floatData.Size();
				for( int i=0; i<a.n; ++i ) {
					floatData.Push( ReadFloat() );
				}
				break;

			case ATTRIB_DOUBLE:
				a.offset = doubleData.Size();
				for( int i=0; i<a.n; ++i ) {
					doubleData.Push( ReadDouble() );
				}
				break;

			case ATTRIB_STRING:
				a.offset = stringData.Size();
				for( int i=0; i<a.n; ++i ) {
					stringData.Push( ReadString() );
				}
				break;
		
			default:
				GLASSERT( 0 );
				break;
			}
			attributes.Push( a );
		}
		else {
			break;
		}
	}
	Sort< Attribute, CompValue >( attributes.Mem(), attributes.Size() );
	return elementName;
}


bool StreamReader::HasChild()
{
	int c = PeekByte();
	return c == BEGIN_ELEMENT;
}


void StreamReader::CloseElement()
{
	int node = ReadInt();
	GLASSERT( node == END_ELEMENT );
}


const StreamReader::Attribute* StreamReader::Get( const char* key )
{
	Attribute a = { 0, key };
	int index  = attributes.BSearch( a );
	if ( index >= 0 ) {
		return &attributes[index];
	}
	return 0;
}


void StreamReader::Value( const Attribute* a, int* value, int size, int offset ) const	
{	
	GLASSERT( a->type == ATTRIB_INT );
	for( int i=0; i<size; ++i ) {
		value[i] = intData[a->offset+i+offset];
	}
}


void StreamReader::Value( const Attribute* a, float* value, int size, int offset ) const
{	
	GLASSERT( a->type == ATTRIB_FLOAT );
	for( int i=0; i<size; ++i ) {
		value[i] = floatData[a->offset+i+offset];
	}
}


void StreamReader::Value( const Attribute* a, double* value, int size, int offset ) const
{	
	GLASSERT( a->type == ATTRIB_DOUBLE );
	for( int i=0; i<size; ++i ) {
		value[i] = doubleData[a->offset+i+offset];
	}
}


void StreamReader::Value( const Attribute* a, U8* value, int size, int offset ) const	
{	
	GLASSERT( a->type == ATTRIB_INT );
	for( int i=0; i<size; ++i ) {
		int v = intData[a->offset+i+offset];
		GLASSERT( v >= 0 && v < 256 );
		value[i] = (U8)v;
	}
}



const char* StreamReader::Value( const Attribute* a, int index ) const
{
	GLASSERT( a->type == ATTRIB_STRING );
	GLASSERT( index < a->n );
	return stringData[a->offset+index];
}


void DumpStream( StreamReader* reader, int depth )
{
	const char* name = reader->OpenElement();

	for( int i=0; i<depth; ++i ) 
		printf( "    " );
	printf( "%s [", name );

	const StreamReader::Attribute* attrs = reader->Attributes();
	int nAttr = reader->NumAttributes();

	float fv;
	double dv;
	int iv;

	for( int i=0; i<nAttr; ++i ) {
		const StreamReader::Attribute* attr = attrs + i;
		switch( attr->type ) {
		case XStream::ATTRIB_INT:
			printf( "%s=(i:", attr->key );
			for( int i=0; i<attr->n; ++i ) {
				reader->Value( attr, &iv, 1, i );
				printf( "%d ", iv );
			}
			printf( ") " );
			break;
		
		case XStream::ATTRIB_FLOAT:
			printf( "%s=(f:", attr->key );
			for( int i=0; i<attr->n; ++i ) {
				reader->Value( attr, &fv, 1, i );
				printf( "%f ", fv );
			}
			printf( ") " );
			break;
		
		case XStream::ATTRIB_DOUBLE:
			printf( "%s=(d:", attr->key );
			for( int i=0; i<attr->n; ++i ) {
				reader->Value( attr, &dv, 1, i );
				printf( "%f ", dv );
			}
			printf( ") " );
			break;

		case XStream::ATTRIB_STRING:
			printf( "%s=(s:", attr->key );
			for( int i=0; i<attr->n; ++i ) {
				printf( "%s ", reader->Value( attr, i ));
			}
			printf( ") " );
			break;

		default:
			GLASSERT( 0 );
		}
	}
	printf( "]\n" );

	while ( reader->HasChild() ) {
		DumpStream( reader, depth+1 );
	}

	reader->CloseElement();
}


bool XarcGet( XStream* xs, const char* key, grinliz::IString& i )
{
	GLASSERT( xs->Loading() );
	const StreamReader::Attribute* attr = xs->Loading()->Get( key );
	if ( attr ) {
		i = StringPool::Intern( xs->Loading()->Value( attr, 0 ));
		return true;
	}
	else {
		i = IString();
		return false;
	}
}

void XarcSet( XStream* xs, const char* key, const grinliz::IString& i )
{
	GLASSERT( xs->Saving() );
	xs->Saving()->Set( key, i.empty() ? "" : i.c_str() );
}
