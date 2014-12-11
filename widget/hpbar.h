#ifndef HP_BAR_INCLUDED
#define HP_BAR_INCLUDED

#include "../gamui/gamui.h"

class GameItem;
class Shield;

class HPBar : public gamui::DigitalBar
{
public:
	HPBar()	{}
	~HPBar()	{}

	void Init(gamui::Gamui* gamui);
	void Set(const GameItem* item, const Shield* shield, const char* optionalName);
};

#endif // HP_BAR_INCLUDED