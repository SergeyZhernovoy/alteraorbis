#ifndef LUMOS_GAME_SCENE_INCLUDED
#define LUMOS_GAME_SCENE_INCLUDED

#include "../xegame/scene.h"
#include "../xegame/chitevent.h"

class LumosGame;
class Sim;
struct NewsEvent;
class Chit;

class GameScene : public Scene
{
	typedef Scene super;
public:
	GameScene( LumosGame* game );
	~GameScene();

	virtual void DoTick( U32 deltaTime );

	virtual void Resize();
	void Zoom( int style, float delta );
	void Rotate( float degrees );

	virtual void Tap( int action, const grinliz::Vector2F& screen, const grinliz::Ray& world );
	virtual void ItemTapped( const gamui::UIItem* item );
	virtual void HandleHotKey( int mask );
	virtual void MouseMove( const grinliz::Vector2F& view, const grinliz::Ray& world );

	virtual void Draw3D( U32 deltaTime );
	virtual void DrawDebugText();

private:
	void Save();
	void Load();
	void SetFace();
	void DoDestTapped( const grinliz::Vector2F& grid );

	void TapModel( Chit* chit );
	void MoveModel( Chit* chit );
	void ClearTargetFlags();

	enum {
		SAVE,
		LOAD,
		CYCLE,
		NUM_SERIAL_BUTTONS,
		
		NUM_NEWS_BUTTONS = 16,
		NEWS_BUTTON_WIDTH  = 60,
		NEWS_BUTTON_HEIGHT = 25
	};
	enum {
		TRACK,
		TELEPORT,
		NUM_CAM_MODES
	};

	LumosGame*	lumosGame;
	Sim*		sim;
	bool		fastMode;
	int			simTimer;		// used to count sim ticks/second
	int			simCount;
	float		simPS;

	int targetChit;
	int possibleChit;

	gamui::PushButton	okay;
	gamui::PushButton	serialButton[NUM_SERIAL_BUTTONS];
	gamui::ToggleButton camModeButton[NUM_CAM_MODES];
	gamui::PushButton	allRockButton;
	gamui::PushButton	coreButton;
	gamui::PushButton	newsButton[NUM_NEWS_BUTTONS];
	gamui::Image		minimap;
	gamui::Image		playerMark;
	gamui::Image		faceImage;
	gamui::TextLabel	dateLabel;
};


#endif // LUMOS_GAME_SCENE_INCLUDED
