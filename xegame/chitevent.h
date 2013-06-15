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

#ifndef CHIT_EVENT_INCLUDED
#define CHIT_EVENT_INCLUDED

#include "../grinliz/gldebug.h"
#include "../grinliz/gltypes.h"
#include "../grinliz/glrectangle.h"
#include "../grinliz/glvector.h"

// Asynchronous! 
// All data is copied, sliced, all that fun stuff. Can't be subclassed (in a way that adds data.)
class ChitEvent
{
public:
	enum { 
		EFFECT					// data: EFFECT_xyz, factor: strength of effect at AOE center
	};

	int		data;				// if there is obvious data
	int		team;
	float	factor;				

	ChitEvent( int p_id, const grinliz::Rectangle2F& areaOfEffect, int filter=0 )
		: id(p_id), aoe( areaOfEffect ), itemFilter(filter), data(0), team(0) {}

	const grinliz::Rectangle2F& AreaOfEffect() const	{ return aoe; }
	//int ItemFilter() const								{ return itemFilter; }
	int ID() const										{ return id; }

	// Utility:
	static ChitEvent EffectEvent( const grinliz::Vector2F origin, float radius, int effect, float strength ) {
		grinliz::Rectangle2F r;
		r.min = r.max = origin;
		r.Outset( radius );
		ChitEvent event( ChitEvent::EFFECT, r );
		event.data   = effect;
		event.factor = strength;
		return event;
	}


private:
	grinliz::Rectangle2F aoe;	// how broadly this event is broadcast
	int id;
	int itemFilter;				// used to filter the spatial query
};


struct NewsEvent {
	NewsEvent() : priority(0), chitID(0), age(0), processed(false) {
		pos.Zero();
	}
	NewsEvent( int p_priority, const grinliz::Vector2F& p_pos, grinliz::IString p_name, int p_chitID=0 ) :
		priority(p_priority), pos(p_pos), name(p_name), chitID(p_chitID), age(0), processed(false)
	{}

	// News priority:
	enum {
		PONY,
		UNICORN,
		PEGASUS
	};

	int					priority;
	grinliz::Vector2F	pos;
	grinliz::IString	name;
	int					chitID;			// if there is one
	U32					age;			// set by the Add
	bool				processed;
};


#endif // CHIT_EVENT_INCLUDED
