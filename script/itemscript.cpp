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

#include "itemscript.h"
#include "battlemechanics.h"
#include "../engine/model.h"
#include "../engine/animation.h"
#include "../tinyxml2/tinyxml2.h"
#include "../xegame/istringconst.h"
#include "../game/lumosmath.h"
#include "../game/news.h"
#include "../xarchive/glstreamer.h"
#include "../xegame/cgame.h"

using namespace tinyxml2;
using namespace grinliz;

ItemDefDB* StackedSingleton< ItemDefDB >::instance = 0;

void ItemDefDB::Load( const char* path )
{
	XMLDocument doc;
	doc.LoadFile( path );
	GLASSERT( !doc.Error() );
	if ( !doc.Error() ) {
		const XMLElement* itemsEle = doc.RootElement();
		GLASSERT( itemsEle );
		for( const XMLElement* itemEle = itemsEle->FirstChildElement( "item" );
			 itemEle;
			 itemEle = itemEle->NextSiblingElement( "item" ) )
		{
			GameItem* item = new GameItem();
			item->Load( itemEle );
			item->key = item->IName();
			GLASSERT( !map.Query( item->key.c_str(), 0 ));
			map.Add( item->key.c_str(), item );
			topNames.Push( item->IName() );

			IString mob = item->keyValues.GetIString( IStringConst::mob );
			if ( mob == IStringConst::greater ) {
				greaterMOBs.Push( item->IName() );
			}
			else if ( mob == IStringConst::lesser ) {
				lesserMOBs.Push( item->IName() );
			}

			const XMLElement* intrinsicEle = itemEle->FirstChildElement( "intrinsics" );
			if ( intrinsicEle ) {
				int nSub = 0;
				for( const XMLElement* subItemEle = intrinsicEle->FirstChildElement( "item" );
					 subItemEle;
					 subItemEle = subItemEle->NextSiblingElement( "item" ), ++nSub )
				{
					GameItem* subItem = new GameItem();
					subItem->Load( subItemEle );

					// Patch the name to make a sub-item
					GLString str;
					str.Format( "%s.%d", item->Name(), nSub );
					subItem->key = StringPool::Intern( str.c_str() );
					GLASSERT( !map.Query( subItem->key.c_str(), 0 ));
					map.Add( subItem->key.c_str(), subItem );

					item->Apply( subItem );
				}
			}
		}
	}
}


const GameItem& ItemDefDB::Get( const char* name, int intrinsic )
{
	GameItem* item=0;
	if ( intrinsic >= 0 ) {
		grinliz::CStr< MAX_ITEM_NAME > n;
		n.Format( "%s.%d", name, intrinsic );
		map.Query( n.c_str(), &item );
	}
	else {
		map.Query( name, &item );
	}
	if ( item ) {
		return *item;
	}
	GLASSERT( 0 );
	return nullItem;
}


/*static*/ void ItemDefDB::GetProperty( const char* name, const char* prop, int* value )
{
	GameItemArr arr;
	Instance()->Get( name, &arr );
	GLASSERT( arr.Size() > 0 );
	
	float v = 0;
	arr[0]->keyValues.Get( prop, value );
}


bool ItemDefDB::Has( const char* name )
{
	return map.Query( name, 0 );
}


void ItemDefDB::Get( const char* name, GameItemArr* arr )
{
	GameItem* item=0;
	map.Query( name, &item );
	GLASSERT( item );
	arr->Push( item );

	for( int i=0; true; ++i ) {
		grinliz::CStr< MAX_ITEM_NAME > n;
		n.Format( "%s.%d", name, i );
		item = 0;
		map.Query( n.c_str(), &item );

		if ( !item ) 
			break;
		arr->Push( item );
	}
}


void ItemDefDB::AssignWeaponStats( const int* roll, const GameItem& base, GameItem* item )
{
	// Accuracy and Damage are effected by traits + level.
	// Speed, ClipCap, Reload by traits. (And only at creation.)
	// Accuracy:	DEX, level		
	// Damage:		STR, level, rangedDamage
	//
	// Fixed at creation:
	// Cooldown:	WILL
	// ClipCap:		CHR		
	// Reload:		INT

	*item = base;
	for( int i=0; i<GameTrait::NUM_TRAITS; ++i ) {
		item->GetTraitsMutable()->Set( i, roll[i] );
	}

	float cool = (float)item->cooldown.Threshold();
	cool *= Dice3D6ToMult( item->Traits().Get( GameTrait::ALT_COOL ));
	item->cooldown.SetThreshold( Clamp( (int)cool, 100, 3000 ));

	float clipCap = (float)base.clipCap;
	clipCap *= Dice3D6ToMult( item->Traits().Get( GameTrait::ALT_CAPACITY ));
	item->clipCap = Clamp( (int)clipCap, 1, 100 );

	float reload = (float)base.reload.Threshold();
	reload /= Dice3D6ToMult( item->Traits().Get( GameTrait::ALT_RELOAD ));
	item->reload.SetThreshold( Clamp( (int)reload, 100, 3000 ));
}


void ItemDefDB::DumpWeaponStats()
{
	GameItem humanMale = this->Get( "humanMale" );

	GLString path;
	GetSystemPath(GAME_SAVE_DIR, "weapons.txt", &path);
	FILE* fp = fopen( path.c_str(), "w" );
	GLASSERT( fp );

	fprintf( fp, "%-25s %-5s %-5s %-5s %-5s\n", "Name", "Dam", "DPS", "C-DPS", "EffR" );
	for( int i=0; i<topNames.Size(); ++i ) {
		const char* name = topNames[i].c_str();
		GameItemArr arr;
		Get( name, &arr );

		for( int j=0; j<arr.Size(); ++j ) {
			if ( arr[j]->ToMeleeWeapon() ) {

				DamageDesc dd;
				if ( j>0 ) {
					fprintf( fp, "%-12s:%-12s ", arr[0]->Name(), arr[j]->Name() );
					BattleMechanics::CalcMeleeDamage( arr[0], arr[j]->ToMeleeWeapon(), &dd );
				}
				else {			
					fprintf( fp, "%-25s ", arr[j]->Name() );
					BattleMechanics::CalcMeleeDamage( &humanMale, arr[j]->ToMeleeWeapon(), &dd );
				}
				fprintf( fp, "%5.1f %5.1f ", dd.damage, BattleMechanics::MeleeDPTU( &humanMale, arr[j]->ToMeleeWeapon()));
				fprintf( fp, "\n" ); 
			}
			if ( arr[j]->ToRangedWeapon() ) {
				float radAt1 = 1;

				if ( j>0 ) {
					fprintf( fp, "%-12s:%-12s ", arr[0]->Name(), arr[j]->Name() );
					radAt1 = BattleMechanics::ComputeRadAt1( arr[0], arr[j]->ToRangedWeapon(), false, false );
				}
				else {
					fprintf( fp, "%-25s ", arr[j]->Name() );
					radAt1 = BattleMechanics::ComputeRadAt1( &humanMale, arr[j]->ToRangedWeapon(), false, false );
				}

				float effectiveRange = BattleMechanics::EffectiveRange( radAt1 );

				float dps  = BattleMechanics::RangedDPTU( arr[j]->ToRangedWeapon(), false );
				float cdps = BattleMechanics::RangedDPTU( arr[j]->ToRangedWeapon(), true );

				fprintf( fp, "%5.1f %5.1f %5.1f %5.1f", arr[j]->rangedDamage, dps, cdps, effectiveRange );
				fprintf( fp, "\n" ); 
			}
		}
	}
	fclose( fp );
}


void ItemHistory::Set( const GameItem* gi )
{
	this->itemID		= gi->ID();
	this->titledName	= gi->INameAndTitle();
	this->level			= gi->Traits().Level();
	this->value			= gi->GetValue();

	int k = 0, c = 0, g = 0, s = 0;

	gi->historyDB.Get(ISC::Kills, &k);
	gi->historyDB.Get(ISC::Greater, &g);
	gi->historyDB.Get(ISC::Crafted, &c);
	gi->keyValues.Get(ISC::score, &s);

	this->kills = k;
	this->greater = g;
	this->crafted = c;
	this->score = s;
}

void ItemHistory::Serialize( XStream* xs )
{
	int aLevel = level;	// U16 to int conversion, and back.
	int aValue = value;
	int aKills = kills;
	int aCrafted = crafted;
	int aGreater = greater;
	int aScore = score;

	XarcOpen( xs, "ItemHistory" );
	XARC_SER( xs, itemID );
	XARC_SER( xs, titledName );
	XARC_SER( xs, aLevel );
	XARC_SER( xs, aValue );
	XARC_SER( xs, aKills );
	XARC_SER( xs, aGreater );
	XARC_SER(xs, aCrafted);
	XARC_SER(xs, aScore);
	XarcClose( xs );

	level = aLevel;
	value = aValue;
	kills = aKills;
	greater = aGreater;
	crafted = aCrafted;
	score = aScore;
}


void ItemHistory::AppendDesc( GLString* str, NewsHistory* history )
{
	if ( !itemID ) {
		str->append( "(none)" );
	}
	else {
		GLASSERT( !titledName.empty() );
		str->AppendFormat( "[%d] %s", itemID, titledName.c_str() );
		if ( level )
			str->AppendFormat( " Level=%d", level );
		if ( value )
			str->AppendFormat( " Value=%d", value );
		if ( kills )
			str->AppendFormat( " Kills=%d", kills );
		if ( greater ) 
			str->AppendFormat( " Greater=%d", greater );
		if ( crafted )
			str->AppendFormat( " Crafted=%d", crafted );
		if (score)
			str->AppendFormat(" Score=%d", score);

		if (history) {
			NewsHistory::Data data;
			history->Find(itemID, false, 0, &data);

			if (data.born) {
				if (data.died) {
					str->AppendFormat(" (c%.2f d%.2f)", double(data.born) / double(AGE_IN_MSEC), double(data.died) / double(AGE_IN_MSEC));
				}
				else {
					str->AppendFormat(" (c%.2f)", double(data.born) / double(AGE_IN_MSEC));
				}
			}
		}
	}
}


ItemDB* StackedSingleton< ItemDB >::instance = 0;

void ItemDB::Serialize( XStream* xs ) 
{
	XarcOpen( xs, "ItemDB" );
	// Don't serialize map! It is created/destroyed with GameItems
	XARC_SER_CARRAY( xs, itemHistory );
	XarcClose( xs );
}

void ItemDB::Add( const GameItem* gi )
{
	int id = gi->ID();
	GLASSERT( id >= 0 );

	// History
	if ( gi->Significant() ) {
		ItemHistory h;
		h.Set( gi );
		itemHistory.Add( h );
	}

	// Current
	itemMap.Add( id, gi );
}


void ItemDB::Remove( const GameItem* gi )
{
	int id = gi->ID();
	GLASSERT( id >= 0 );

	// History:
	if ( gi->Significant() ) {
		ItemHistory h;
		h.Set( gi );
		itemHistory.Add( h );
	}

	// Current:
	itemMap.Remove( id );
}


void ItemDB::Update( const GameItem* gi )
{
	int id = gi->ID();
	GLASSERT( id >= 0 );

	if ( gi->Significant() ) {
		ItemHistory h;
		h.Set( gi );
		itemHistory.Add( h );
	}
}


const GameItem*	ItemDB::Find( int id )
{
	const GameItem* gi = 0;
	itemMap.Query( id, &gi );
	return gi;
}


const ItemHistory* ItemDB::History( int id )
{
	ItemHistory key;
	key.itemID = id;

	const ItemHistory* h = 0;
	int index = itemHistory.BSearch( key );
	return index >= 0 ? &itemHistory[index] : 0;
}

