#ifndef FORGE_SCENE_INCLUDE
#define FORGE_SCENE_INCLUDE

#include "../xegame/scene.h"
#include "../gamui/gamui.h"
#include "../engine/screenport.h"
#include "../widget/itemdescwidget.h"
#include "../widget/moneywidget.h"
#include "../game/wallet.h"

class GameItem;
class LumosGame;
class Engine;
class Model;


class ForgeSceneData : public SceneData
{
public:
	ForgeSceneData() : SceneData(), item(0), tech(3), level(4) {}
	virtual ~ForgeSceneData();
	GameItem*	item;		// item that was forged. (out) WARNING: be sure to null so it doesn't get deleted!
	int			tech;		// available tech (in)
	int			level;		// skill of the smith (in)
	Wallet		wallet;		// in/out: available and used
	Wallet		itemWallet;	// out: the wallet that should become part of the new ItemComponent
};


class ForgeScene : public Scene
{
public:
	ForgeScene( LumosGame* game, ForgeSceneData* data );
	~ForgeScene();

	virtual void Resize();

	virtual void Tap( int action, const grinliz::Vector2F& screen, const grinliz::Ray& world )				
	{
		ProcessTap( action, screen, world );
	}
	virtual void ItemTapped( const gamui::UIItem* item );

	virtual void Draw3D( U32 delatTime );

private:
	void SetModel();

	LumosGame*			lumosGame;
	Engine*				engine;
	Screenport			screenport;
	Model*				model;
	bool				itemBuilt;

	gamui::PushButton	okay;

	enum {
		RING,
		GUN,
		SHIELD,
		NUM_ITEM_TYPES
	};

	enum {
		PISTOL,
		BLASTER,
		PULSE,
		BEAMGUN,
		NUM_GUN_TYPES
	};

	enum {
		GUN_BODY,		// on
		GUN_CELL,		// boosts clip
		GUN_DRIVER,		// boosts power/damage
		GUN_SCOPE,		// boosts accuracy
		NUM_GUN_PARTS
	};

	enum {
		RING_MAIN,		// on
		RING_GUARD,		// shield coupler
		RING_TRIAD,		// 
		RING_BLADE,		// 
		NUM_RING_PARTS
	};

	enum {
		EFFECT_FIRE,
		EFFECT_SHOCK,
		EFFECT_EXPLOSIVE,
		NUM_EFFECTS
	};

	ForgeSceneData*		forgeData;
	int					techRequired;
	Wallet				crystalRequired;

	gamui::ToggleButton	itemType[NUM_ITEM_TYPES];
	gamui::ToggleButton gunType[NUM_GUN_TYPES];
	gamui::ToggleButton gunParts[NUM_GUN_PARTS];
	gamui::ToggleButton ringParts[NUM_RING_PARTS];
	gamui::ToggleButton effects[NUM_EFFECTS];
	gamui::TextLabel	requiredLabel,  techRequiredLabel;
	gamui::TextLabel	availableLabel, techAvailLabel;
	gamui::PushButton	buildButton;
	MoneyWidget			crystalRequiredWidget;
	MoneyWidget			crystalAvailWidget;
	ItemDescWidget		itemDescWidget;
};


#endif // FORGE_SCENE_INCLUDE