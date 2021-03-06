#ifndef LUMOS_DB_HELPER
#define LUMOS_DB_HELPER

#include "gamedbreader.h"
#include "gamedbwriter.h"
#include "../grinliz/glrectangle.h"
#include "../grinliz/glvector.h"
#include "../grinliz/glgeometry.h"
#include "../grinliz/glstringutil.h"
#include "../grinliz/glmatrix.h"

struct DBItem
{
	DBItem() : witem( 0 ), item( 0 ) {}
	DBItem( gamedb::WItem* w ) : witem( w ), item( 0 ) {}
	DBItem( const gamedb::Item* i ) : witem( 0 ), item( i ) {}

	bool Loading() const { return item != 0; }
	bool Saving() const  { return witem != 0; }

	gamedb::WItem* witem;
	const gamedb::Item* item;
};

inline DBItem DBChild( DBItem parent, const char* child ) {
	DBItem r;
	if ( parent.item ) {
		r.item = parent.item->Child( child );
	}
	else {
		r.witem = parent.witem->FetchChild( child );
	}
	return r;
}

inline DBItem DBChild( DBItem parent, int i ) {
	DBItem r;
	if ( parent.item ) {
		r.item = parent.item->Child( i );
	}
	else {
		r.witem = parent.witem->FetchChild( i );
	}
	return r;
}

inline void DBSet( gamedb::WItem* item, const char* key, grinliz::IString str ) {
	if ( str.empty() ) {
		item->SetString( key, "<empty>" );
	}
	else {
		item->SetString( key, str.c_str() );
	}
}

inline void DBRead( const gamedb::Item* item, const char* key, grinliz::IString& str ) {
	const char* s = item->GetString( key );
	if ( s && *s ) {
		str = grinliz::StringPool::Intern( s );
	}
	else {
		str = grinliz::IString();
	}
}

inline void DBSet( gamedb::WItem* item, const char* key, int value )						{ item->SetInt( key, value ); }
inline void DBSet( gamedb::WItem* item, const char* key, unsigned value )					{ item->SetInt( key, (int)value ); }
inline void DBSet( gamedb::WItem* item, const char* key, float value )						{ item->SetFloat( key, value ); }
inline void DBSet( gamedb::WItem* item, const char* key, bool value )						{ item->SetBool( key, value ); }
inline void DBSet( gamedb::WItem* item, const char* key, const grinliz::Vector2I& v )		{ item->SetIntArray( key, &v.x, 2 );}
inline void DBSet( gamedb::WItem* item, const char* key, const grinliz::Vector2F& v )		{ item->SetFloatArray( key, &v.x, 2 );}
inline void DBSet( gamedb::WItem* item, const char* key, const grinliz::Vector3F& v )		{ item->SetFloatArray( key, &v.x, 3 );}
inline void DBSet( gamedb::WItem* item, const char* key, const grinliz::Vector4F& v )		{ item->SetFloatArray( key, &v.x, 4 );}
inline void DBSet( gamedb::WItem* item, const char* key, const grinliz::Quaternion& q )		{ item->SetFloatArray( key, &q.x, 4 );}
inline void DBSet( gamedb::WItem* item, const char* key, const grinliz::Rectangle2I& rect ) { item->SetIntArray( key, &rect.min.x, 4 );}
inline void DBSet( gamedb::WItem* item, const char* key, const grinliz::Rectangle3F& rect ) { item->SetFloatArray( key, &rect.min.x, 6 );}
inline void DBSet( gamedb::WItem* item, const char* key, const grinliz::Matrix4& mat )		{ item->SetFloatArray( key, mat.Mem(), 16 ); }

inline void DBRead( const gamedb::Item* item, const char* key, int &value )					{ value = item->GetInt( key ); }
inline void DBRead( const gamedb::Item* item, const char* key, unsigned &value )			{ value = (unsigned)item->GetInt( key ); }
inline void DBRead( const gamedb::Item* item, const char* key, float &value )				{ value = item->GetFloat( key ); }
inline void DBRead( const gamedb::Item* item, const char* key, bool &value )				{ value = item->GetBool( key ); }
inline void DBRead( const gamedb::Item* item, const char* key, grinliz::Vector2I& v )		{ item->GetIntArray( key, 2, &v.x );}
inline void DBRead( const gamedb::Item* item, const char* key, grinliz::Vector2F& v )		{ item->GetFloatArray( key, 2, &v.x );}
inline void DBRead( const gamedb::Item* item, const char* key, grinliz::Vector3F& v )		{ item->GetFloatArray( key, 3, &v.x );}
inline void DBRead( const gamedb::Item* item, const char* key, grinliz::Vector4F& v )		{ item->GetFloatArray( key, 4, &v.x );}
inline void DBRead( const gamedb::Item* item, const char* key, grinliz::Quaternion& q )		{ item->GetFloatArray( key, 4, &q.x );}
inline void DBRead( const gamedb::Item* item, const char* key, grinliz::Rectangle2I& rect ) { item->GetIntArray( key, 4, &rect.min.x );}
inline void DBRead( const gamedb::Item* item, const char* key, grinliz::Rectangle3F& rect ) { item->GetFloatArray( key, 6, &rect.min.x );}
inline void DBRead( const gamedb::Item* item, const char* key, grinliz::Matrix4& mat )		{ item->GetFloatArray( key, 16, mat.Mem() ); }

#define DB_SET( item, name ) DBSet( item, #name, name );
#define DB_READ( witem, name ) DBRead( witem, #name, name );

#define DB_SERIAL( it, name ) {					\
	if ( it.witem )								\
		DBSet( it.witem, #name, name );			\
	else										\
		DBRead( it.item, #name, name ); }

#endif // LUMOS_DB_HELPER