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

#ifndef RENDER_COMPONENT_INCLUDED
#define RENDER_COMPONENT_INCLUDED

#include "component.h"

#include "../engine/enginelimits.h"
#include "../engine/animation.h"

#include "../xegame/xegamelimits.h"

#include "../grinliz/glvector.h"
#include "../grinliz/glmatrix.h"
#include "../grinliz/glcolor.h"


class Engine;
class ModelResource;
class Model;
struct ProcRenderInfo;

class RenderComponent : public Component
{
private:
	typedef Component super;
public:
	enum {	NUM_MODELS	= EL_NUM_METADATA,	// slot[0] is the main model (and META_TARGET). Others are hardpoint attach.

			DECO_FOOT	= 0,
			DECO_HEAD,
			NUM_DECO };

	// spacetree probably  sufficient, but 'engine' easier to keep track of
	// asset can be null if followed by a Load()
	RenderComponent( Engine* engine, const char* asset );
	virtual ~RenderComponent();

	// ------ Component functionality: -------
	virtual const char* Name() const { return "RenderComponent"; }
	virtual RenderComponent* ToRenderComponent() { return this; }
	virtual void DebugStr( grinliz::GLString* str );

	virtual void Serialize( XStream* xs );

	virtual void OnAdd( Chit* chit );
	virtual void OnRemove();

	virtual int DoTick( U32 deltaTime, U32 since );
	virtual void OnChitMsg( Chit* chit, const ChitMsg& msg );

	// ------ Additional --------
	// Radius of the "base" the model stands on. Used to position
	// the model so it doesn't walk into walls or other models.
	float	RadiusOfBase();

	// --- MetaData -- //
	bool HasMetaData( int id );
	bool GetMetaData( int id, grinliz::Matrix4* xform );
	bool GetMetaData( int id, grinliz::Vector3F* pos );	
	
	// Useful metadata access.
	bool CalcTarget( grinliz::Vector3F* pos );	// manufacture a target if there isn't metadata
	bool CalcTrigger( grinliz::Vector3F* pos, grinliz::Matrix4* xform );	// either can be null
	
	// --- Model Info --- //
	void GetModelList( grinliz::CArray<const Model*, NUM_MODELS+1> *ignore  );
	const Model* MainModel() const				{ return model[0]; }	// used to map back from world to chits
	const ModelResource* MainResource() const;


	// --- Animation -- //
	// Is the animation ready to change?
	bool	AnimationReady() const;
	// Play the special animations: MELEE, IMPACT, etc.
	// Walk, stand, etc. are played automatically.
	bool	PlayAnimation( int type );
	int		CurrentAnimation() const;

	// --- Hardpoint control -- //
	// A render component has one primary, animated model. Additional
	// assets (guns, shields, etc.) can be Attached and Detatched
	// to "metadata hardpoints".
	bool HasHardpoint( int hardpoint );

	const ModelResource* ResourceAtHardpoint( int hardpoint );
	// Carrying is a little special: you need both a hardpoint AND the animation.
	bool CarryHardpointAvailable();

	bool Attach( int hardpoint, const char* asset );
	void SetColor( int hardpoint, const grinliz::Vector4F& colorMult );
	void SetProcedural( int hardpoint, const ProcRenderInfo& info );
	void SetSaturation( float s );
	void Detach( int hardpoint );

	// --- Decoration --- //
	void Deco( const char* asset, int slot, int duration );

private:
	int CalcAnimation() const;
	SpatialComponent* SyncToSpatial();	// this a scary function: location is stored in both the model and the spatialcomponent

	Engine*					engine;
	float					radiusOfBase;
	grinliz::IString		mainAsset;

	Model*					model[ NUM_MODELS ];
	Model*					deco[ NUM_DECO ];
	int						decoDuration[ NUM_DECO ];
};

#endif // RENDER_COMPONENT_INCLUDED