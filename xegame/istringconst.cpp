// machine generated by istringconst.py
#include "istringconst.h"
using namespace grinliz;

IString IStringConst::ktrigger;
IString IStringConst::ktarget;
IString IStringConst::kalthand;
IString IStringConst::khead;
IString IStringConst::kshield;

void IStringConst::Init()
{
	ktrigger = StringPool::Intern( "trigger", true );
	ktarget = StringPool::Intern( "target", true );
	kalthand = StringPool::Intern( "althand", true );
	khead = StringPool::Intern( "head", true );
	kshield = StringPool::Intern( "shield", true );
}
