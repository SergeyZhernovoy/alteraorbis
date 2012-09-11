#ifndef LUMOS_BOLT_INCLUDED
#define LUMOS_BOLT_INCLUDED

#include "../grinliz/gltypes.h"
#include "../grinliz/gldebug.h"
#include "../grinliz/glvector.h"
#include "../grinliz/glcolor.h"
#include "../grinliz/glcontainer.h"

#include "vertex.h"

class Engine;
struct Bolt;
class Model;

class IBoltImpactHandler
{
public:
	virtual void HandleBolt( const Bolt& bolt, Model* m, const grinliz::Vector3F& at ) = 0;
};


struct Bolt {
	Bolt() {
		head.Set( 0, 0, 0 );
		len = 0;
		dir.Set( 1, 0, 0 );
		impact = false;
		color.Set( 1, 1, 1, 1 );

		chitID = 0;
	} 

	grinliz::Vector3F	head;
	float				len;
	grinliz::Vector3F	dir;	// normal vector
	bool				impact;	// 'head' is the impact location
	grinliz::Vector4F	color;

	// Userdata follows
	int								chitID;		// who fired this bolt
	grinliz::MathVector<float,4>	damage;		// damageDesc. Don't worry about he '4', won't compile if mis-match

	static void TickAll( grinliz::CDynArray<Bolt>* bolts, U32 delta, Engine* engine, IBoltImpactHandler* handler );
};


class BoltRenderer
{
public:
	BoltRenderer();
	~BoltRenderer()	{}

	void DrawAll( const Bolt* bolts, int nBolts, Engine* engine );

private:
	enum { MAX_BOLTS = 400 };
	int nBolts;

	U16			index[MAX_BOLTS*6];
	PTCVertex	vertex[MAX_BOLTS*4];
};

#endif // LUMOS_BOLT_INCLUDED
