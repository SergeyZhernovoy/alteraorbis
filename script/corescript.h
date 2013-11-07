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

#ifndef CORESCRIPT_INCLUDED
#define CORESCRIPT_INCLUDED

#include "scriptcomponent.h"
#include "../xegame/cticker.h"

class WorldMap;
class WorkQueue;
class LumosChitBag;

class CoreScript : public IScript
{
public:
	CoreScript( WorldMap* map, LumosChitBag* chitBag, Engine* engine );
	virtual ~CoreScript();

	virtual void Init();
	virtual void Serialize( XStream* xs );
	virtual void OnAdd();
	virtual void OnRemove();

	virtual int DoTick( U32 delta, U32 since );
	virtual const char* ScriptName() { return "CoreScript"; }
	virtual CoreScript* ToCoreScript() { return this; }

	// Once attached, can't detach. There for good.
	// Can however move around again if the Chit 'ejects'.
	// In this case the chit is attached, but !standing
	bool AttachToCore( Chit* chit );
	Chit* GetAttached( bool* standing );

	WorkQueue* GetWorkQueue()	{ return workQueue; }
	void SetDefaultSpawn( grinliz::IString s ) { defaultSpawn = s; }
	int GetTechLevel() const	{ return 2; }		// FIXME

private:
	WorldMap*	worldMap;
	WorkQueue*	workQueue;
	CTicker		spawnTick;
	int			boundID;
	grinliz::IString defaultSpawn;
};

#endif // CORESCRIPT_INCLUDED
