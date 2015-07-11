/*
Copyright (c) 2000-2003 Lee Thomason (www.grinninglizard.com)

Grinning Lizard Utilities. Note that software that uses the 
utility package (including Lilith3D and Kyra) have more restrictive
licences which applies to code outside of the utility package.


This software is provided 'as-is', without any express or implied 
warranty. In no event will the authors be held liable for any 
damages arising from the use of this software.

Permission is granted to anyone to use this software for any 
purpose, including commercial applications, and to alter it and 
redistribute it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must 
not claim that you wrote the original software. If you use this 
software in a product, an acknowledgment in the product documentation 
would be appreciated but is not required.

2. Altered source versions must be plainly marked as such, and 
must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any source 
distribution.
*/

#include "glrandom.h"

#include <math.h>
#include <string.h>
#include <time.h>

#include "glcontainer.h"
	
using namespace grinliz;

#if 0
//computes the 2D lookup.
#include "glmath.h"
int main( const char* argv, int argc )
{
	const int COUNT=256;
	for( int i=0; i<COUNT; ++i ) {
		float radian = ((float)i/(float)COUNT)*TWO_PI;
		float x = sinf( radian );
		float y = cosf( radian );
		printf( "%ff, %ff, ", x, y );
		if ( i%4 == 3 )
			printf( "\n" );
	}
}
#endif 

#if 0
//computes the 3D lookup.
#include "glmath.h"
#include "glgeometry.h"

int main( const char* argv, int argc )
{
	// 510 -> 258

	std::vector< Vector3F > vertex;
	std::vector< U32 > index;
	TessellateSphere( 3, 1.0f, false, &vertex, &index );
	printf( "vertex size=%d\n", vertex.size() );

	std::vector< Vector3F > unique;
	for( unsigned i=0; i<vertex.size(); ++i ) {
		unsigned k=0;
		for( ; k<unique.size(); ++k ) {
			if ( unique[k] == vertex[i] ) {
				break;
			}
		}
		if ( k == unique.size() ) {
			unique.push_back( vertex[i] );
		}
	}
	printf( "unique size=%d\n", unique.size() );

	for( unsigned i=0; i<unique.size(); ++i ) {
		const Vector3F& v = unique[i];
		GLASSERT( Equal( v.Length(), 1.0f, 0.001f ) );
		printf( "%ff, %ff, %ff,  \t", v.x, v.y, v.z );
		if ( i%4 == 3 )
			printf( "\n" );
	}
}
#endif

// a true random number sequence from Random.org
U8 Random::series[256] = {
	135,  187,  239,   59,   20,  162,  194,  182,  169,  139,  170,  138,  108,  208,  181,  140,
	 12,   52,  220,    4,  221,  245,  195,  163,   84,  152,   99,   83,  171,   11,  119,   13,
	 23,  151,   24,   49,   80,  112,  134,  136,  252,  132,  241,   21,  117,   67,  243,  141,
	106,    8,   28,    3,  120,   85,  189,  100,  167,  122,  129,  125,   86,   97,  242,  116,
	133,   40,  177,   72,  144,  255,   42,   79,   22,   32,  213,  231,   96,   27,   48,  197,
	  0,   17,  212,  203,  127,  229,  156,    6,  188,  191,  224,  223,  128,  227,  103,  232,
	 41,  118,   81,   77,  173,  238,  178,   76,  209,   18,  153,  249,   73,   31,  228,  160,
	 71,  131,  222,  230,   94,  186,   38,  180,   82,   29,   66,   50,  150,  137,  218,  158,
	147,   74,  113,  114,  105,  157,  115,   47,  154,   34,  165,  240,   56,  237,  146,   88,
	 55,   75,  176,   95,  244,   63,   68,    7,  217,   39,  161,  226,   26,  102,  159,   14,
	207,   51,  109,   35,  107,  174,   46,  251,  198,    5,  201,   62,   53,   58,  210,   37,
	143,  104,  145,   98,   61,  250,   90,   57,    1,  233,   16,   45,  216,  190,  179,  205,
	 91,  175,  234,   33,   60,  164,   65,    9,   25,   89,  110,  168,  185,  202,  246,   87,
	236,  101,  206,   30,   92,  184,  172,  155,   70,   64,   43,  149,  126,  235,  199,  192,
	211,  254,  111,   15,  253,  247,  200,  204,    2,  123,  166,   69,  142,   54,  193,  121,
	219,   93,  183,   78,  124,   10,  248,  148,   36,  214,  225,  196,   19,   44,  130,  215
};


void Random::SetSeed( const char* str )
{
	int len = strlen( str );
	U32 seed = Hash( str, len );
	SetSeed( seed );
}


void Random::SetSeedFromTime()
{
	U32 seed = (U32)time( 0 ) + (U32)clock();
	SetSeed( seed );
}


// Very clever post by: George Marsaglia
// http://www.bobwheeler.com/statistics/Password/MarsagliaPost.txt
// And also the paper:
// http://tbf.coe.wayne.edu/jmasm/vol2_no1.pdf
// and post:
// http://oldmill.uchicago.edu/~wilder/Code/random/Papers/Marsaglia_2003.html
// Simple, elegant, effective!

U32 Random::Rand()
{
	U64 t;
	U64 a=698769069;

	x = 69069*x + 12345;
	y ^= (y<<13);  
	y ^= (y>>17); 
	y ^= (y<<5);

	t = a*(U64)z + (U64)c; 
	c = (U32)(t>>32);
	z = (U32)t;

	// Take the low bits and xor in a series, just
	// to give a little extra random noise in the 
	// low bits.
	return (x+y+z) ^ series[(lowCount++)&0xff];
}


#if 0
void Random::NormalVector( float* v, int dim )
{
	GLASSERT( dim > 0 );

	float len2 = 0.0f;
	for( int i=0; i<dim; ++i ) {
		v[i] = -1.0f + 2.0f*Uniform();
		len2 += v[i]*v[i];
	}
	// exceedingly unlikely error case:
	if ( len2 == 0.0f ) {
		v[0] = 1.0;
		len2 = 1.0;
	}
	float lenInv = 1.0f / sqrtf( len2 );
	for( int i=0; i<dim; ++i ) {
		v[i] *= lenInv;
	}
}
#endif

// Another great work:
// http://www.azillionmonkeys.com/qed/hash.html
// by Paul Hsieh
//
// But this is based on:
// FNV-1a
// http://isthe.com/chongo/tech/comp/fnv/
// public domain.
//

/*static*/ U32 Random::Hash( const void* data, U32 len )
{
	static const U32 NULLT = U32(-1);
	const unsigned char *p = (const unsigned char *)(data);
	unsigned int h = 2166136261U;

	if ( len != NULLT ) {
		for( U32 i=0; i<len; ++i, ++p ) {
			h ^= *p;
			h *= 16777619;
		}
	}
	else {
		for( ; *p; ++p ) {
			h ^= *p;
			h *= 16777619;
		}
	}
	return h;
}


/*static*/ U8 Random::Hash8( U32 data )
{
	return series[ (data ^ (data>>8) ^ (data>>16) ^ (data>>24))&255 ];
}


const float Random::normal2D[COUNT_2D*2] = {
	0.000000f, 1.000000f, 0.024541f, 0.999699f, 0.049068f, 0.998795f, 0.073565f, 0.997290f, 
	0.098017f, 0.995185f, 0.122411f, 0.992480f, 0.146730f, 0.989177f, 0.170962f, 0.985278f, 
	0.195090f, 0.980785f, 0.219101f, 0.975702f, 0.242980f, 0.970031f, 0.266713f, 0.963776f, 
	0.290285f, 0.956940f, 0.313682f, 0.949528f, 0.336890f, 0.941544f, 0.359895f, 0.932993f, 
	0.382683f, 0.923880f, 0.405241f, 0.914210f, 0.427555f, 0.903989f, 0.449611f, 0.893224f, 
	0.471397f, 0.881921f, 0.492898f, 0.870087f, 0.514103f, 0.857729f, 0.534998f, 0.844854f, 
	0.555570f, 0.831470f, 0.575808f, 0.817585f, 0.595699f, 0.803208f, 0.615232f, 0.788346f, 
	0.634393f, 0.773010f, 0.653173f, 0.757209f, 0.671559f, 0.740951f, 0.689541f, 0.724247f, 
	0.707107f, 0.707107f, 0.724247f, 0.689541f, 0.740951f, 0.671559f, 0.757209f, 0.653173f, 
	0.773010f, 0.634393f, 0.788346f, 0.615232f, 0.803208f, 0.595699f, 0.817585f, 0.575808f, 
	0.831470f, 0.555570f, 0.844854f, 0.534998f, 0.857729f, 0.514103f, 0.870087f, 0.492898f, 
	0.881921f, 0.471397f, 0.893224f, 0.449611f, 0.903989f, 0.427555f, 0.914210f, 0.405241f, 
	0.923880f, 0.382683f, 0.932993f, 0.359895f, 0.941544f, 0.336890f, 0.949528f, 0.313682f, 
	0.956940f, 0.290285f, 0.963776f, 0.266713f, 0.970031f, 0.242980f, 0.975702f, 0.219101f, 
	0.980785f, 0.195090f, 0.985278f, 0.170962f, 0.989177f, 0.146730f, 0.992480f, 0.122411f, 
	0.995185f, 0.098017f, 0.997290f, 0.073564f, 0.998795f, 0.049068f, 0.999699f, 0.024541f, 
	1.000000f, -0.000000f, 0.999699f, -0.024541f, 0.998795f, -0.049068f, 0.997290f, -0.073565f, 
	0.995185f, -0.098017f, 0.992480f, -0.122411f, 0.989177f, -0.146731f, 0.985278f, -0.170962f, 
	0.980785f, -0.195090f, 0.975702f, -0.219101f, 0.970031f, -0.242980f, 0.963776f, -0.266713f, 
	0.956940f, -0.290285f, 0.949528f, -0.313682f, 0.941544f, -0.336890f, 0.932993f, -0.359895f, 
	0.923880f, -0.382684f, 0.914210f, -0.405241f, 0.903989f, -0.427555f, 0.893224f, -0.449611f, 
	0.881921f, -0.471397f, 0.870087f, -0.492898f, 0.857729f, -0.514103f, 0.844854f, -0.534998f, 
	0.831470f, -0.555570f, 0.817585f, -0.575808f, 0.803208f, -0.595699f, 0.788346f, -0.615232f, 
	0.773010f, -0.634393f, 0.757209f, -0.653173f, 0.740951f, -0.671559f, 0.724247f, -0.689541f, 
	0.707107f, -0.707107f, 0.689541f, -0.724247f, 0.671559f, -0.740951f, 0.653173f, -0.757209f, 
	0.634393f, -0.773010f, 0.615232f, -0.788346f, 0.595699f, -0.803208f, 0.575808f, -0.817585f, 
	0.555570f, -0.831470f, 0.534997f, -0.844854f, 0.514103f, -0.857729f, 0.492898f, -0.870087f, 
	0.471397f, -0.881921f, 0.449611f, -0.893224f, 0.427555f, -0.903989f, 0.405241f, -0.914210f, 
	0.382683f, -0.923880f, 0.359895f, -0.932993f, 0.336890f, -0.941544f, 0.313682f, -0.949528f, 
	0.290285f, -0.956940f, 0.266713f, -0.963776f, 0.242980f, -0.970031f, 0.219101f, -0.975702f, 
	0.195090f, -0.980785f, 0.170962f, -0.985278f, 0.146730f, -0.989177f, 0.122411f, -0.992480f, 
	0.098017f, -0.995185f, 0.073564f, -0.997290f, 0.049067f, -0.998795f, 0.024541f, -0.999699f, 
	-0.000000f, -1.000000f, -0.024541f, -0.999699f, -0.049068f, -0.998795f, -0.073565f, -0.997290f, 
	-0.098017f, -0.995185f, -0.122411f, -0.992480f, -0.146730f, -0.989177f, -0.170962f, -0.985278f, 
	-0.195090f, -0.980785f, -0.219101f, -0.975702f, -0.242980f, -0.970031f, -0.266713f, -0.963776f, 
	-0.290285f, -0.956940f, -0.313682f, -0.949528f, -0.336890f, -0.941544f, -0.359895f, -0.932993f, 
	-0.382683f, -0.923880f, -0.405241f, -0.914210f, -0.427555f, -0.903989f, -0.449612f, -0.893224f, 
	-0.471397f, -0.881921f, -0.492898f, -0.870087f, -0.514103f, -0.857729f, -0.534998f, -0.844854f, 
	-0.555570f, -0.831470f, -0.575808f, -0.817585f, -0.595699f, -0.803208f, -0.615232f, -0.788346f, 
	-0.634393f, -0.773010f, -0.653173f, -0.757209f, -0.671559f, -0.740951f, -0.689541f, -0.724247f, 
	-0.707107f, -0.707107f, -0.724247f, -0.689541f, -0.740951f, -0.671559f, -0.757209f, -0.653173f, 
	-0.773010f, -0.634393f, -0.788346f, -0.615232f, -0.803208f, -0.595699f, -0.817585f, -0.575808f, 
	-0.831470f, -0.555570f, -0.844854f, -0.534997f, -0.857729f, -0.514103f, -0.870087f, -0.492898f, 
	-0.881921f, -0.471397f, -0.893224f, -0.449611f, -0.903989f, -0.427555f, -0.914210f, -0.405241f, 
	-0.923880f, -0.382683f, -0.932993f, -0.359895f, -0.941544f, -0.336890f, -0.949528f, -0.313682f, 
	-0.956940f, -0.290285f, -0.963776f, -0.266713f, -0.970031f, -0.242980f, -0.975702f, -0.219101f, 
	-0.980785f, -0.195090f, -0.985278f, -0.170962f, -0.989177f, -0.146730f, -0.992480f, -0.122411f, 
	-0.995185f, -0.098017f, -0.997290f, -0.073564f, -0.998795f, -0.049067f, -0.999699f, -0.024541f, 
	-1.000000f, 0.000000f, -0.999699f, 0.024541f, -0.998795f, 0.049068f, -0.997290f, 0.073565f, 
	-0.995185f, 0.098017f, -0.992480f, 0.122411f, -0.989177f, 0.146730f, -0.985278f, 0.170962f, 
	-0.980785f, 0.195090f, -0.975702f, 0.219101f, -0.970031f, 0.242980f, -0.963776f, 0.266713f, 
	-0.956940f, 0.290285f, -0.949528f, 0.313682f, -0.941544f, 0.336890f, -0.932993f, 0.359895f, 
	-0.923879f, 0.382684f, -0.914210f, 0.405242f, -0.903989f, 0.427555f, -0.893224f, 0.449612f, 
	-0.881921f, 0.471397f, -0.870087f, 0.492898f, -0.857729f, 0.514103f, -0.844853f, 0.534998f, 
	-0.831469f, 0.555570f, -0.817585f, 0.575808f, -0.803208f, 0.595699f, -0.788346f, 0.615232f, 
	-0.773010f, 0.634393f, -0.757209f, 0.653173f, -0.740951f, 0.671559f, -0.724247f, 0.689541f, 
	-0.707107f, 0.707107f, -0.689541f, 0.724247f, -0.671559f, 0.740951f, -0.653173f, 0.757209f, 
	-0.634393f, 0.773011f, -0.615231f, 0.788347f, -0.595699f, 0.803208f, -0.575808f, 0.817585f, 
	-0.555570f, 0.831470f, -0.534998f, 0.844854f, -0.514103f, 0.857729f, -0.492898f, 0.870087f, 
	-0.471397f, 0.881921f, -0.449611f, 0.893224f, -0.427555f, 0.903989f, -0.405241f, 0.914210f, 
	-0.382683f, 0.923880f, -0.359895f, 0.932993f, -0.336890f, 0.941544f, -0.313682f, 0.949528f, 
	-0.290284f, 0.956940f, -0.266712f, 0.963776f, -0.242980f, 0.970031f, -0.219101f, 0.975702f, 
	-0.195090f, 0.980785f, -0.170962f, 0.985278f, -0.146730f, 0.989177f, -0.122410f, 0.992480f, 
	-0.098017f, 0.995185f, -0.073565f, 0.997290f, -0.049068f, 0.998795f, -0.024541f, 0.999699f
};


void Random::NormalVector2D( float* v )
{
	// Take 0-255 from 3 bits up.
	U32 index = ((Rand()>>3)&0xff)*2;
	v[0] = normal2D[index+0];
	v[1] = normal2D[index+1];
}


const int COUNT_3D = 258;
const float gNormal3D[COUNT_3D*3] =
{
	1.000000f, 0.000000f, 0.000000f,  	0.000000f, 1.000000f, 0.000000f,  	-1.000000f, 0.000000f, 0.000000f,  	0.000000f, -1.000000f, 0.000000f,  	
	0.000000f, 0.000000f, 1.000000f,  	0.000000f, 0.000000f, -1.000000f,  	0.707107f, 0.707107f, 0.000000f,  	0.000000f, 0.707107f, 0.707107f,  	
	0.707107f, 0.000000f, 0.707107f,  	-0.707107f, 0.707107f, 0.000000f,  	-0.707107f, 0.000000f, 0.707107f,  	-0.707107f, -0.707107f, 0.000000f,  	
	0.000000f, -0.707107f, 0.707107f,  	0.707107f, -0.707107f, 0.000000f,  	0.707107f, 0.000000f, -0.707107f,  	0.000000f, 0.707107f, -0.707107f,  	
	-0.707107f, 0.000000f, -0.707107f,  	0.000000f, -0.707107f, -0.707107f,  	0.923880f, 0.382683f, 0.000000f,  	0.816497f, 0.408248f, 0.408248f,  	
	0.923880f, 0.000000f, 0.382683f,  	0.382683f, 0.923880f, 0.000000f,  	0.000000f, 0.923880f, 0.382683f,  	0.408248f, 0.816497f, 0.408248f,  	
	0.408248f, 0.408248f, 0.816497f,  	0.000000f, 0.382683f, 0.923880f,  	0.382683f, 0.000000f, 0.923880f,  	-0.382683f, 0.923880f, 0.000000f,  	
	-0.408248f, 0.816497f, 0.408248f,  	-0.923880f, 0.382683f, 0.000000f,  	-0.923880f, 0.000000f, 0.382683f,  	-0.816497f, 0.408248f, 0.408248f,  	
	-0.408248f, 0.408248f, 0.816497f,  	-0.382683f, 0.000000f, 0.923880f,  	-0.923880f, -0.382683f, 0.000000f,  	-0.816497f, -0.408248f, 0.408248f,  	
	-0.382683f, -0.923880f, 0.000000f,  	0.000000f, -0.923880f, 0.382683f,  	-0.408248f, -0.816497f, 0.408248f,  	-0.408248f, -0.408248f, 0.816497f,  	
	0.000000f, -0.382683f, 0.923880f,  	0.382683f, -0.923880f, 0.000000f,  	0.408248f, -0.816497f, 0.408248f,  	0.923880f, -0.382683f, 0.000000f,  	
	0.816497f, -0.408248f, 0.408248f,  	0.408248f, -0.408248f, 0.816497f,  	0.408248f, 0.816497f, -0.408248f,  	0.000000f, 0.923880f, -0.382683f,  	
	0.923880f, 0.000000f, -0.382683f,  	0.816497f, 0.408248f, -0.408248f,  	0.408248f, 0.408248f, -0.816497f,  	0.382683f, 0.000000f, -0.923880f,  	
	0.000000f, 0.382683f, -0.923880f,  	-0.816497f, 0.408248f, -0.408248f,  	-0.923880f, 0.000000f, -0.382683f,  	-0.408248f, 0.816497f, -0.408248f,  	
	-0.408248f, 0.408248f, -0.816497f,  	-0.382683f, 0.000000f, -0.923880f,  	-0.408248f, -0.816497f, -0.408248f,  	0.000000f, -0.923880f, -0.382683f,  	
	-0.816497f, -0.408248f, -0.408248f,  	-0.408248f, -0.408248f, -0.816497f,  	0.000000f, -0.382683f, -0.923880f,  	0.816497f, -0.408248f, -0.408248f,  	
	0.408248f, -0.816497f, -0.408248f,  	0.408248f, -0.408248f, -0.816497f,  	0.980785f, 0.195090f, 0.000000f,  	0.959683f, 0.198757f, 0.198757f,  	
	0.980785f, 0.000000f, 0.195090f,  	0.831470f, 0.555570f, 0.000000f,  	0.788675f, 0.577350f, 0.211325f,  	0.890320f, 0.404615f, 0.208847f,  	
	0.890320f, 0.208847f, 0.404615f,  	0.788675f, 0.211325f, 0.577350f,  	0.831470f, 0.000000f, 0.555570f,  	0.555570f, 0.831470f, 0.000000f,  	
	0.404615f, 0.890320f, 0.208847f,  	0.577350f, 0.788675f, 0.211325f,  	0.195090f, 0.980785f, 0.000000f,  	0.000000f, 0.980785f, 0.195090f,  	
	0.198757f, 0.959683f, 0.198757f,  	0.208847f, 0.890320f, 0.404615f,  	0.000000f, 0.831470f, 0.555570f,  	0.211325f, 0.788675f, 0.577350f,  	
	0.639602f, 0.639602f, 0.426401f,  	0.211325f, 0.577350f, 0.788675f,  	0.426401f, 0.639602f, 0.639602f,  	0.639602f, 0.426401f, 0.639602f,  	
	0.577350f, 0.211325f, 0.788675f,  	0.404615f, 0.208847f, 0.890320f,  	0.555570f, 0.000000f, 0.831470f,  	0.000000f, 0.555570f, 0.831470f,  	
	0.208847f, 0.404615f, 0.890320f,  	0.198757f, 0.198757f, 0.959683f,  	0.000000f, 0.195090f, 0.980785f,  	0.195090f, 0.000000f, 0.980785f,  	
	-0.195090f, 0.980785f, 0.000000f,  	-0.198757f, 0.959683f, 0.198757f,  	-0.555570f, 0.831470f, 0.000000f,  	-0.577350f, 0.788675f, 0.211325f,  	
-0.404615f, 0.890320f, 0.208847f,  	-0.208847f, 0.890320f, 0.404615f,  	-0.211325f, 0.788675f, 0.577350f,  	-0.831470f, 0.555570f, 0.000000f,  	
	-0.890320f, 0.404615f, 0.208847f,  	-0.788675f, 0.577350f, 0.211325f,  	-0.980785f, 0.195090f, 0.000000f,  	-0.980785f, 0.000000f, 0.195090f,  	
	-0.959683f, 0.198757f, 0.198757f,  	-0.890320f, 0.208847f, 0.404615f,  	-0.831470f, 0.000000f, 0.555570f,  	-0.788675f, 0.211325f, 0.577350f,  	
	-0.639602f, 0.639602f, 0.426401f,  	-0.577350f, 0.211325f, 0.788675f,  	-0.639602f, 0.426401f, 0.639602f,  	-0.426401f, 0.639602f, 0.639602f,  	
	-0.211325f, 0.577350f, 0.788675f,  	-0.208847f, 0.404615f, 0.890320f,  	-0.555570f, 0.000000f, 0.831470f,  	-0.404615f, 0.208847f, 0.890320f,  	
	-0.198757f, 0.198757f, 0.959683f,  	-0.195090f, 0.000000f, 0.980785f,  	-0.980785f, -0.195090f, 0.000000f,  	-0.959683f, -0.198757f, 0.198757f,  	
	-0.831470f, -0.555570f, 0.000000f,  	-0.788675f, -0.577350f, 0.211325f,  	-0.890320f, -0.404615f, 0.208847f,  	-0.890320f, -0.208847f, 0.404615f,  	
	-0.788675f, -0.211325f, 0.577350f,  	-0.555570f, -0.831470f, 0.000000f,  	-0.404615f, -0.890320f, 0.208847f,  	-0.577350f, -0.788675f, 0.211325f,  	
	-0.195090f, -0.980785f, 0.000000f,  	0.000000f, -0.980785f, 0.195090f,  	-0.198757f, -0.959683f, 0.198757f,  	-0.208847f, -0.890320f, 0.404615f,  	
	0.000000f, -0.831470f, 0.555570f,  	-0.211325f, -0.788675f, 0.577350f,  	-0.639602f, -0.639602f, 0.426401f,  	-0.211325f, -0.577350f, 0.788675f,  	
	-0.426401f, -0.639602f, 0.639602f,  	-0.639602f, -0.426401f, 0.639602f,  	-0.577350f, -0.211325f, 0.788675f,  	-0.404615f, -0.208847f, 0.890320f,  	
	0.000000f, -0.555570f, 0.831470f,  	-0.208847f, -0.404615f, 0.890320f,  	-0.198757f, -0.198757f, 0.959683f,  	0.000000f, -0.195090f, 0.980785f,  	
	0.195090f, -0.980785f, 0.000000f,  	0.198757f, -0.959683f, 0.198757f,  	0.555570f, -0.831470f, 0.000000f,  	0.577350f, -0.788675f, 0.211325f,  	
	0.404615f, -0.890320f, 0.208847f,  	0.208847f, -0.890320f, 0.404615f,  	0.211325f, -0.788675f, 0.577350f,  	0.831470f, -0.555570f, 0.000000f,  	
	0.890320f, -0.404615f, 0.208847f,  	0.788675f, -0.577350f, 0.211325f,  	0.980785f, -0.195090f, 0.000000f,  	0.959683f, -0.198757f, 0.198757f,  	
	0.890320f, -0.208847f, 0.404615f,  	0.788675f, -0.211325f, 0.577350f,  	0.639602f, -0.639602f, 0.426401f,  	0.577350f, -0.211325f, 0.788675f,  	
	0.639602f, -0.426401f, 0.639602f,  	0.426401f, -0.639602f, 0.639602f,  	0.211325f, -0.577350f, 0.788675f,  	0.208847f, -0.404615f, 0.890320f,  	
	0.404615f, -0.208847f, 0.890320f,  	0.198757f, -0.198757f, 0.959683f,  	0.198757f, 0.959683f, -0.198757f,  	0.000000f, 0.980785f, -0.195090f,  	
	0.577350f, 0.788675f, -0.211325f,  	0.404615f, 0.890320f, -0.208847f,  	0.208847f, 0.890320f, -0.404615f,  	0.211325f, 0.788675f, -0.577350f,  	
	0.000000f, 0.831470f, -0.555570f,  	0.890320f, 0.404615f, -0.208847f,  	0.788675f, 0.577350f, -0.211325f,  	0.980785f, 0.000000f, -0.195090f,  	
	0.959683f, 0.198757f, -0.198757f,  	0.890320f, 0.208847f, -0.404615f,  	0.831470f, 0.000000f, -0.555570f,  	0.788675f, 0.211325f, -0.577350f,  	
	0.639602f, 0.639602f, -0.426401f,  	0.577350f, 0.211325f, -0.788675f,  	0.639602f, 0.426401f, -0.639602f,  	0.426401f, 0.639602f, -0.639602f,  	
	0.211325f, 0.577350f, -0.788675f,  	0.208847f, 0.404615f, -0.890320f,  	0.000000f, 0.555570f, -0.831470f,  	0.555570f, 0.000000f, -0.831470f,  	
	0.404615f, 0.208847f, -0.890320f,  	0.198757f, 0.198757f, -0.959683f,  	0.195090f, 0.000000f, -0.980785f,  	0.000000f, 0.195090f, -0.980785f,  	
	-0.959683f, 0.198757f, -0.198757f,  	-0.980785f, 0.000000f, -0.195090f,  	-0.788675f, 0.577350f, -0.211325f,  	-0.890320f, 0.404615f, -0.208847f,  	
	-0.890320f, 0.208847f, -0.404615f,  	-0.788675f, 0.211325f, -0.577350f,  	-0.831470f, 0.000000f, -0.555570f,  	-0.404615f, 0.890320f, -0.208847f,  	
	-0.577350f, 0.788675f, -0.211325f,  	-0.198757f, 0.959683f, -0.198757f,  	-0.208847f, 0.890320f, -0.404615f,  	-0.211325f, 0.788675f, -0.577350f,  	
	-0.639602f, 0.639602f, -0.426401f,  	-0.211325f, 0.577350f, -0.788675f,  	-0.426401f, 0.639602f, -0.639602f,  	-0.639602f, 0.426401f, -0.639602f,  	
	-0.577350f, 0.211325f, -0.788675f,  	-0.404615f, 0.208847f, -0.890320f,  	-0.555570f, 0.000000f, -0.831470f,  	-0.208847f, 0.404615f, -0.890320f,  	
	-0.198757f, 0.198757f, -0.959683f,  	-0.195090f, 0.000000f, -0.980785f,  	-0.198757f, -0.959683f, -0.198757f,  	0.000000f, -0.980785f, -0.195090f,  	
	-0.577350f, -0.788675f, -0.211325f,  	-0.404615f, -0.890320f, -0.208847f,  	-0.208847f, -0.890320f, -0.404615f,  	-0.211325f, -0.788675f, -0.577350f,  	
	0.000000f, -0.831470f, -0.555570f,  	-0.890320f, -0.404615f, -0.208847f,  	-0.788675f, -0.577350f, -0.211325f,  	-0.959683f, -0.198757f, -0.198757f,  	
	-0.890320f, -0.208847f, -0.404615f,  	-0.788675f, -0.211325f, -0.577350f,  	-0.639602f, -0.639602f, -0.426401f,  	-0.577350f, -0.211325f, -0.788675f,  	
	-0.639602f, -0.426401f, -0.639602f,  	-0.426401f, -0.639602f, -0.639602f,  	-0.211325f, -0.577350f, -0.788675f,  	-0.208847f, -0.404615f, -0.890320f,  	
	0.000000f, -0.555570f, -0.831470f,  	-0.404615f, -0.208847f, -0.890320f,  	-0.198757f, -0.198757f, -0.959683f,  	0.000000f, -0.195090f, -0.980785f,  	
	0.959683f, -0.198757f, -0.198757f,  	0.788675f, -0.577350f, -0.211325f,  	0.890320f, -0.404615f, -0.208847f,  	0.890320f, -0.208847f, -0.404615f,  	
	0.788675f, -0.211325f, -0.577350f,  	0.404615f, -0.890320f, -0.208847f,  	0.577350f, -0.788675f, -0.211325f,  	0.198757f, -0.959683f, -0.198757f,  	
	0.208847f, -0.890320f, -0.404615f,  	0.211325f, -0.788675f, -0.577350f,  	0.639602f, -0.639602f, -0.426401f,  	0.211325f, -0.577350f, -0.788675f,  	
	0.426401f, -0.639602f, -0.639602f,  	0.639602f, -0.426401f, -0.639602f,  	0.577350f, -0.211325f, -0.788675f,  	0.404615f, -0.208847f, -0.890320f,  	
	0.208847f, -0.404615f, -0.890320f,  	0.198757f, -0.198757f, -0.959683f	
};


void Random::NormalVector3D( float* v )
{
	U32 index = Rand( COUNT_3D )*3;
	v[0] = gNormal3D[index+0];
	v[1] = gNormal3D[index+1];
	v[2] = gNormal3D[index+2];
}


float Random::DiceUniform( U32 nDice, U32 sides )
{
	float dice = (float)Dice( nDice, sides );		// 2,6 -> 2-12
	dice -= nDice;									//     -> 0-10
	float mx = (float)((sides-1)*nDice + 1);		// 11

	return ( dice + Uniform() ) / mx;
}


int Random::Select( const float* scores, int nItems )
{
	float total = 0.0f;
	for( int i=0; i<nItems; ++i ) {
		GLASSERT( scores[i] >= 0.0f );
		total += scores[i];
	}
	float uniform = Uniform() * total;

	total = 0;
	for( int i=0; i<nItems; ++i ) {
		total += scores[i];
		if ( uniform <= total )
			return i;
	}
	GLASSERT( 0 );
	return 0;
}


int Random::WeightedDice(int nDice, int side, int nHigh, int nLow)
{
	static const int MAX = 12;
	int rolls[MAX] = { 0 };
	int totalDice = nDice + nHigh + nLow;

	GLASSERT( totalDice <= MAX);
	if (totalDice > MAX) {
		return Dice(nDice, side);
	}

	for (int i = 0; i < totalDice; ++i) {
		rolls[i] = 1 + Rand(side);
	}
	Sort(rolls, totalDice);

	int result = 0;
	for (int i = nLow; i < nLow + nDice; ++i) {
		result += rolls[i];
	}
	return result;
}
