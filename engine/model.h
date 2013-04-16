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

#ifndef UFOATTACK_MODEL_INCLUDED
#define UFOATTACK_MODEL_INCLUDED

#include "../grinliz/gldebug.h"
#include "../grinliz/gltypes.h"
#include "../grinliz/glgeometry.h"
#include "../grinliz/glmemorypool.h"
#include "../grinliz/glcontainer.h"
#include "../grinliz/glstringutil.h"

#include "vertex.h"
#include "enginelimits.h"
#include "serialize.h"
#include "gpustatemanager.h"
#include "animation.h"

class Texture;
class SpaceTree;
class RenderQueue;
class GPUState;
struct GPUStream;
struct GPUStreamData;
class EngineShaders;
class Chit;
class AnimationResource;
class XStream;


/*
	v0 v1 v2 v0 v1 v2									vertex (3 points x 2 instances)
	i0 i1 i2 i0 i0 i0 i0+3 i1+3 i2+3 i0+3 i0+3 i0+3		index (2 triangle x 2 instances)

	vertex size *= nInstance
	index size *= nInstance
	instance size = index size * nInstance
*/

// The smallest draw unit: texture, vertex, index.
struct ModelAtom 
{
	void Init() {
		texture = 0;
		nVertex = nIndex = 0;
		index = 0;
		vertex = 0;
		instances = 0;
	}

	void Free() {
		DeviceLoss();
		delete [] index;
		delete [] vertex;
		Init();
	}

	void DeviceLoss() {
		vertexBuffer.Destroy();
		indexBuffer.Destroy();
	}

	Texture* texture;
#	ifdef EL_USE_VBO
	mutable GPUVertexBuffer vertexBuffer;		// created on demand, hence 'mutable'
	mutable GPUIndexBuffer  indexBuffer;
#	endif

	void Bind( GPUStream* stream, GPUStreamData* data ) const;

	U32 nVertex;
	U32 nIndex;
	U32 instances;

	U16* index;
	InstVertex* vertex;
};


struct ModelMetaData {
	grinliz::IString		name;		// name of the metadata
	grinliz::Vector3F		pos;		// position
	grinliz::Vector3F		axis;		// axis of rotation (if any)
	float					rotation;	// amount of rotation (if any)
	grinliz::IString		boneName;	// name of the bone this metadata is attached to (if any)
};


struct ModelParticleEffect {
	grinliz::IString	metaData;	// name	 of the metadata
	grinliz::IString	name;		// name of the particle effect.
};

struct ModelHeader
{
	// flags
	enum {
		RESOURCE_NO_SHADOW	= 0x08,		// model casts no shadow
	};
	bool NoShadow() const			{ return (flags&RESOURCE_NO_SHADOW) ? true : false; }

	grinliz::IString		name;
	grinliz::IString		animation;			// the name of the animation, if exists, for this model.
	U16						nTotalVertices;		// in all atoms
	U16						nTotalIndices;
	U16						flags;
	U16						nAtoms;
	grinliz::Rectangle3F	bounds;
	ModelMetaData			metaData[EL_MAX_METADATA];
	ModelParticleEffect		effectData[EL_MAX_MODEL_EFFECTS];
	grinliz::IString		boneName[EL_MAX_BONES];

	int BoneNameToOffset( const grinliz::IString& name ) const {
		for ( int i=0; i<EL_MAX_BONES; ++i ) {
			if ( boneName[i] == name )
				return i;
		}
		GLASSERT( 0 );
		return 0;
	}

	void Set( const grinliz::IString& name, int nGroups, int nTotalVertices, int nTotalIndices,
			  const grinliz::Rectangle3F& bounds );

	void Load(	const gamedb::Item* );		// database connection
	void Save(	gamedb::WItem* parent );	// database connection
};


class ModelResource
{
public:
	ModelResource()		{ memset( &header, 0, sizeof( header ) ); }
	~ModelResource()	{ Free(); }

	const grinliz::Rectangle3F& AABB() const	{ return header.bounds; }

	void Free();
	void DeviceLoss();

	// In the coordinate space of the resource! (Object space.)
	int Intersect(	const grinliz::Vector3F& point,
					const grinliz::Vector3F& dir,
					grinliz::Vector3F* intersect ) const;

	const ModelMetaData* GetMetaData( grinliz::IString name ) const;
	const ModelMetaData* GetMetaData( int i ) const { GLASSERT( i>= 0 && i < EL_MAX_METADATA ); return &header.metaData[i]; }

	const char*			Name() const	{ return header.name.c_str(); }
	grinliz::IString	IName() const	{ return header.name; }

	ModelHeader				header;				// loaded
	grinliz::Rectangle3F	hitBounds;			// for picking - a bounds approximation
	grinliz::Rectangle3F	invariantBounds;	// build y rotation in, so that bounds can
												// be generated with simple addition, without
												// causing the call to Model::XForm 
	ModelAtom atom[EL_MAX_MODEL_GROUPS];
};


struct ModelAux {
	BoneData			boneData;
	grinliz::Vector4F	texture0XForm;
	grinliz::Vector4F	texture0Clip;
	grinliz::Vector4F	texture0ColorMap[3];
};


class ModelResourceManager
{
public:
	static ModelResourceManager* Instance()	{ GLASSERT( instance ); return instance; }
	
	void  AddModelResource( ModelResource* res );
	const ModelResource* GetModelResource( const char* name, bool errorIfNotFound=true );
	void DeviceLoss();

	static void Create();
	static void Destroy();

	const ModelResource* const* GetAllResources( int *count ) const {
		*count = modelResArr.Size();
		return modelResArr.Mem();
	}

	grinliz::MemoryPoolT< ModelAux > modelAuxPool;

private:
	enum { 
		MAX_MODELS = 200	// just pointers
	};
	ModelResourceManager();
	~ModelResourceManager();

	static ModelResourceManager* instance;
	grinliz::CArray< ModelResource*, MAX_MODELS > modelResArr;
	grinliz::HashTable<	const char*, ModelResource*, grinliz::CompCharPtr > map;
};


class ModelLoader
{
public:
	ModelLoader() 	{}
	~ModelLoader()	{}

	void Load( const gamedb::Item*, ModelResource* res );

private:
	void LoadAtom( const gamedb::Item* item, int index, ModelResource* res );
	grinliz::CDynArray<Vertex> vBuffer;
	grinliz::CDynArray<U16> iBuffer;
};


class Model
{
public:
	Model();
	~Model();

	void Init( const ModelResource* resource, SpaceTree* tree );
	void Free();

	void Serialize( XStream* xs, SpaceTree* tree );

	void Queue( RenderQueue* queue, EngineShaders* shaders, int requiredShaderFlag, int excludedShaderFlag );

	enum {
		MODEL_SELECTABLE			= (1<<0),
		MODEL_HAS_COLOR				= (1<<1),
		MODEL_HAS_BONE_FILTER		= (1<<2),
		MODEL_NO_SHADOW				= (1<<3),
		MODEL_INVISIBLE				= (1<<4),
		MODEL_TEXTURE0_XFORM		= (1<<5),
		MODEL_TEXTURE0_CLIP			= (1<<6),
		MODEL_TEXTURE0_COLORMAP		= (1<<7),

		MODEL_USER					= (1<<16)		// reserved for user code.
	};

	bool CastsShadow() const		{ return (flags & MODEL_NO_SHADOW) == 0; }
	int IsFlagSet( int f ) const	{ return flags & f; }
	void SetFlag( int f )			{ flags |= f; }
	void ClearFlag( int f )			{ flags &= (~f); }
	int Flags()	const				{ return flags; }

	const grinliz::Vector3F& Pos() const			{ return pos; }
	void SetPos( const grinliz::Vector3F& pos );
	void SetPos( float x, float y, float z )		{ grinliz::Vector3F vec = { x, y, z }; SetPos( vec ); }
	float X() const { return pos.x; }
	float Y() const { return pos.y; }
	float Z() const { return pos.z; }

	void SetRotation( const grinliz::Quaternion& rot );
	void SetYRotation( float yRot ) {
		grinliz::Quaternion q;
		static const grinliz::Vector3F UP = {0,1,0};
		q.FromAxisAngle( UP, yRot );
		SetRotation( q );
	}
	const grinliz::Quaternion& GetRotation() const			{ return rot; }
	void SetPosAndYRotation( const grinliz::Vector3F& pos, float yRot );
	void SetTransform( const grinliz::Matrix4& mat );

	// Get the animation resource, if it exists.
	const AnimationResource* GetAnimationResource() const	{ return animationResource; }
	// Set the current animation, null or "reference" turns off animation.
	// Does nothing if the 'id' is the currently playing animation
	void SetAnimation( int id, U32 crossFade, bool restart );
	int GetAnimation() const						{ return currentAnim.id; }
	bool AnimationDone() const;

	grinliz::IString GetBoneName( int i ) const;
	int GetBoneID( grinliz::IString name ) const;

	// Update the time and animation rendering.
	void DeltaAnimation(	U32 time, 
							grinliz::CArray<int, EL_MAX_METADATA> *metaData,
							bool *done );

	void SetAnimationRate( float rate )						{ animationRate = rate; }
	bool HasAnimation() const								{ return animationResource && (currentAnim.id>=0); }

	// WARNING: not really supported. Just for debug rendering. May break:
	// normals, lighting, bounds, picking, etc. etc.
	void SetScale( float s );
	float GetScale() const							{ return debugScale; }
	
	void SetColor( const grinliz::Vector4F& color ) {
		SetFlag( MODEL_HAS_COLOR );
		this->color = color;
	}
	bool HasColor() const {
		return IsFlagSet( MODEL_HAS_COLOR ) != 0;
	}

	// 4 ids. Null to clear.
	void SetBoneFilter( const int* boneID ) {
		if ( boneID ) {
			boneFilter.Set(  (float)boneID[0], (float)boneID[1], (float)boneID[2], (float)boneID[3] );
			SetFlag( MODEL_HAS_BONE_FILTER );
		}
		else {
			ClearFlag( MODEL_HAS_BONE_FILTER );
		}
	}
	// [4]
	void SetBoneFilter( const grinliz::IString* names, const bool* filter );

	bool HasBoneFilter() const {
		return IsFlagSet( MODEL_HAS_BONE_FILTER ) != 0;
	}

	void SetTextureXForm( float a=1, float d=1, float tx=0, float ty=0 );
	void SetTextureClip( float x0=0, float y0=0, float x1=1, float y1=1 );
	void SetColorMap(	bool enable, 
						const grinliz::Vector4F& red, 
						const grinliz::Vector4F& green, 
						const grinliz::Vector4F& blue );
	void SetColorMap(	bool enable, 
						const grinliz::Color4F& red, 
						const grinliz::Color4F& green, 
						const grinliz::Color4F& blue )
	{
		grinliz::Vector4F r = { red.r, red.g, red.b, red.a };
		grinliz::Vector4F g = { green.r, green.g, green.b, green.a };
		grinliz::Vector4F b = { blue.r, blue.g, blue.b, blue.a };
		SetColorMap( enable, r, g, b );
	}

	const grinliz::Vector4F& Control() const		{ return control; }
	void SetControl( const grinliz::Vector4F& v )	{ control = v; }

	// AABB for user selection (bigger than the true AABB)
	void CalcHitAABB( grinliz::Rectangle3F* aabb ) const;

	// The bounding box
	// Accurate, but can be expensive to call a lot.
	const grinliz::Rectangle3F& AABB() const;

	// Bigger bounding box, cheaper call.
	grinliz::Rectangle3F GetInvariantAABB( float xPerY, float zPerY ) const {
		grinliz::Rectangle3F b = resource->invariantBounds;
		b.min += pos;
		b.max += pos;
		
		if ( CastsShadow() ) {
			if ( xPerY > 0 )
				b.max.x += xPerY * b.max.y;
			else
				b.min.x += xPerY * b.max.y;

			if ( zPerY > 0 )
				b.max.z += zPerY * b.max.y;
			else
				b.min.z += zPerY * b.max.y;
		}
		return b;
	}

	void CalcMetaData( grinliz::IString name, grinliz::Matrix4* xform ) const;
	bool HasParticles() const {  return hasParticles; }
	void EmitParticles( ParticleSystem* system, const grinliz::Vector3F* eyeDir, U32 deltaTime ) const;
	void CalcTargetSize( float* width, float* height ) const;

	// Returns grinliz::INTERSECT or grinliz::REJECT
	int IntersectRay(	const grinliz::Vector3F& origin, 
						const grinliz::Vector3F& dir,
						grinliz::Vector3F* intersect ) const;

	const ModelResource* GetResource() const	{ return resource; }
	bool Sentinel()	const						{ return resource==0 && tree==0; }

	Model* next;			// used by the SpaceTree query
	Model* next0;			// used by the Engine sub-sorting
	Chit*  userData;		// really should be void* - but types are nice.

	const grinliz::Matrix4& XForm() const;

	void Modify() 
	{			
		xformValid = false; 
		invValid = false; 
	}

private:
	const grinliz::Matrix4& InvXForm() const;

	void CalcAnimation( BoneData* boneData ) const;	// compute the animition, accounting for crossfade, etc.
	void CalcAnimation( BoneData::Bone* bone, grinliz::IString boneName ) const;	// compute the animition, accounting for crossfade, etc.
	void CrossFade( float fraction, BoneData::Bone* inOut, const BoneData::Bone& prev ) const;

	SpaceTree* tree;
	const ModelResource* resource;

	grinliz::Vector3F	pos;
	grinliz::Quaternion rot;
	
	float debugScale;
	
	struct AnimationState {
		void Init() { time=0; id=ANIM_OFF; }
		U32				time;
		int				id;
	};
	AnimationState	currentAnim;
	AnimationState	prevAnim;

	float animationRate;
	U32 totalCrossFadeTime;	// how much time the crossfade will use
	U32 crossFadeTime;

	const AnimationResource* animationResource;
	bool hasParticles;
	int flags;

	grinliz::Vector4F	color;
	grinliz::Vector4F	boneFilter;
	grinliz::Vector4F	control;
	ModelAux* aux;

	mutable bool xformValid;
	mutable bool invValid;

	mutable grinliz::Rectangle3F aabb;
	mutable grinliz::Matrix4 _xform;
	mutable grinliz::Matrix4 _invXForm;
};


#endif // UFOATTACK_MODEL_INCLUDED