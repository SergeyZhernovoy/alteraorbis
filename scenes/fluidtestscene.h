#ifndef FLUID_TEST_SCENE_INCLUDED
#define FLUID_TEST_SCENE_INCLUDED

#include "../gamui/gamui.h"
#include "../xegame/scene.h"
#include "../xegame/cticker.h"
#include "../xegame/chitbag.h"
#include "../xegame/chitcontext.h"

class LumosGame;
class WorldMap;
class LumosChitBag;
class CircuitSim;

class FluidTestScene : public Scene
{
	typedef Scene super;
public:
	FluidTestScene(LumosGame* game);
	virtual ~FluidTestScene();

	virtual void Resize();
	void Zoom(int style, float delta);
	void Rotate(float degrees);
	void MoveCamera(float dx, float dy);

	virtual bool Tap(int action, const grinliz::Vector2F& screen, const grinliz::Ray& world);
	virtual void ItemTapped(const gamui::UIItem* item);
	virtual void HandleHotKey(int mask) { super::HandleHotKey(mask); }

	virtual void Draw3D(U32 deltaTime);
	virtual void DrawDebugText();

	virtual void DoTick(U32 delta);
	virtual void MouseMove(const grinliz::Vector2F& view, const grinliz::Ray& world);

private:
	void Tap3D(const grinliz::Vector2F& view, const grinliz::Ray& world);

	enum {
		BUTTON_ROCK0,
		BUTTON_ROCK1,
		BUTTON_ROCK2,
		BUTTON_ROCK3,
		BUTTON_EMITTER,
		BUTTON_LAVA_EMITTER,

		BUTTON_TEMPLE,

		BUTTON_DETECTOR,
		BUTTON_SWITCH_ON,
		BUTTON_SWITCH_OFF,

		BUTTON_GATE,
		BUTTON_TIMED_GATE,
		BUTTON_TURRET,

		BUTTON_DELETE,
		BUTTON_ROTATE,
		NUM_BUTTONS
	};

	ChitContext	context;
	grinliz::Vector2I hover;
	grinliz::Vector2I dragStart;

	CTicker fluidTicker;

	gamui::PushButton okay;
	gamui::ToggleButton buildButton[NUM_BUTTONS];
	gamui::PushButton saveButton, loadButton;

};


#endif // FLUID_TEST_SCENE_INCLUDED
