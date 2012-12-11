#ifndef LUMOS_WORLDGEN_INCLUDED
#define LUMOS_WORLDGEN_INCLUDED

#include "../grinliz/gltypes.h"
#include "../grinliz/gldebug.h"
#include "../grinliz/glrectangle.h"
#include "../grinliz/glcontainer.h"
#include "../xegame/xegamelimits.h"

namespace grinliz {
class PerlinNoise;
};

struct WorldFeature {
	int  id;						// unique id
	bool land;						// true: continent, island, false: ocean
	grinliz::Rectangle2I bounds;	// bounds
	int area;						// actual area
};


class WorldGen
{
public:
	WorldGen();
	~WorldGen();

	void StartLandAndWater( U32 seed0, U32 seed1 );
	void DoLandAndWater( int y );
	bool EndLandAndWater( float fractionLand );

	bool CalColor();

	// This works, but doesn't produce good result.
	// Early optimizing. Same problem can be solved with roads.
	bool Split( int maxSize, int radius );

	// SIZExSIZE
	const U8* Land() const						{ return land; }
	const U8* Color() const						{ return color; }
	const WorldFeature* WorldFeatures() const	{ return featureArr.Mem(); }
	int	  NumWorldFeatures() const				{ return featureArr.Size(); }

	enum {
		SIZE = MAX_MAP_SIZE
	};

private:
	int CountFlixelsAboveCutoff( const float* flixels, float cutoff );
	void DrawCanal( grinliz::Vector2I v, int radius, int dx, int dy, const grinliz::Rectangle2I& wf );

	class CompareWF
	{
	public:
		static bool Less( const WorldFeature& s0, const WorldFeature& s1 ) { return s0.area > s1.area; }
	};

	float* flixels;
	grinliz::PerlinNoise* noise0;
	grinliz::PerlinNoise* noise1;

	U8* land;	// 0: water, 1: last
	U8* color;	// 0: not processed, >0 color
	grinliz::CDynArray< WorldFeature > featureArr;
};

#endif // LUMOS_WORLDGEN_INCLUDED