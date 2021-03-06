#ifndef START_GAME_WIDGET
#define START_GAME_WIDGET

#include "../gamui/gamui.h"
#include "../grinliz/glcontainer.h"
#include "../game/gamelimits.h"

class SectorData;
class Engine;
class ChitBag;
class Chit;
class Scene;
class WorldMap;

class StartGameWidget : public gamui::IWidget
{
public:
	StartGameWidget();
	void Init(gamui::Gamui* gamui, const gamui::ButtonLook& look, const gamui::LayoutCalculator& calculator );

	~StartGameWidget();

	const char* Name() const { return "StartGameWidget"; }

	virtual float X() const				{ return background.X(); }
	virtual float Y() const				{ return background.Y(); }
	virtual float Width() const			{ return background.Width(); }
	virtual float Height() const		{ return background.Height(); }

	virtual void SetPos(float x, float y);
	virtual void SetSize(float width, float h);
	virtual bool Visible() const				{ return background.Visible(); }
	virtual void SetVisible(bool visible) ;

	void ItemTapped(const gamui::UIItem* item);

	void SetSectorData(const SectorData** sdArr, int nData, Engine* engine, ChitBag* chitBag, Scene* callback, WorldMap* map );

private:
	void SetBodyText();

	float				textHeight;
	int					currentSector;
	Engine*				engine;
	ChitBag*			chitBag;
	Scene*				iScene;
	WorldMap*			worldMap;

	gamui::LayoutCalculator calculator;
	gamui::Image		background;
	gamui::TextLabel	topLabel, bodyLabel, countLabel;
	gamui::PushButton	prevDomain, nextDomain;
	gamui::PushButton	okay;

	grinliz::CArray<const SectorData*, START_SECTOR_CHOICES> sectors;
	grinliz::CDynArray<Chit*> queryArr;
};


#endif // START_GAME_WIDGET
