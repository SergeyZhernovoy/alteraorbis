#ifndef ROCKGEN_INCLUDED
#define ROCKGEN_INCLUDED

#include "../grinliz/gldebug.h"
#include "../grinliz/gltypes.h"
#include "../grinliz/glnoise.h"


// Creates a sub-region of rocks.
class RockGen
{
public:
	RockGen( int size );
	~RockGen();

	// HEIGHT style
	enum {
		NOISE_HEIGHT,		// height from separate generator
		NOISE_HIGH_HEIGHT,	// noise from separate generator, but high
		KEEP_HEIGHT,		// height from noise generator
	};
	int		heightStyle;

	// ROCK style
	enum {
		BOULDERY,			// plasma noise
		CAVEY
	};

	// Do basic computation.
	void StartCalc( int seed );
	void DoCalc( int y );
	void EndCalc();

	// Seperate flat land from rock.
	void DoThreshold( int seed, float fractionLand, int heightStyle );

	const U8* Height() const { return heightMap; }

private:
	float Fractal( int x, int y, float octave0, float octave1, float octave1Amount );

	grinliz::PerlinNoise* noise0;
	grinliz::PerlinNoise* noise1;

	int size;
	U8*	heightMap;
};


#endif // ROCKGEN_INCLUDED
