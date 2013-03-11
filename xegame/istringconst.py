# read a set of text files, and create an include file for c++ 
# example: incgen shader.inc shader.frag shader.vert

import sys;

fileRead  = open( "istringconst.h", "r" )

match = "static grinliz::IString k"
values = []

for line in fileRead:
	line = line.lstrip().rstrip()
	if ( line[0:len(match)] == match ):
		values.append( line[len(match):-1])

#for v in values :
#	print( v )

fileWrite = open( "istringconst.cpp", 'w' )

fileWrite.write( "// machine generated by istringconst.py\n" )
fileWrite.write( '#include "istringconst.h"\nusing namespace grinliz;\n\n' )

for v in values:
	fileWrite.write( "IString IStringConst::k" + v + ";\n")

fileWrite.write( "\nvoid IStringConst::Init()\n{\n" )

for v in values:
	fileWrite.write( '\tk' + v + ' = StringPool::Intern( "' + v + '", true );\n' )

fileWrite.write( "}\n")

