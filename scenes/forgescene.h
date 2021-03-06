#ifndef FORGE_SCENE_INCLUDE
#define FORGE_SCENE_INCLUDE

#include "../xegame/scene.h"
#include "../gamui/gamui.h"
#include "../engine/screenport.h"
#include "../widget/itemdescwidget.h"
#include "../widget/moneywidget.h"
#include "../game/wallet.h"
#include "../script/forgescript.h"

class GameItem;
class LumosGame;
class Engine;
class Model;
class ItemComponent;

class ForgeSceneData : public SceneData
{
public:
	ForgeSceneData() : SceneData(), tech(3), itemComponent(0) {}

	int				tech;			// available tech (in)
	ItemComponent*	itemComponent;	// Who is operating this forge? Note that this means
									// construction limits (only trolls can use blades, for 
									// example) are enforced by the USER, not the forge.
};


class ForgeScene : public Scene
{
public:
	ForgeScene( LumosGame* game, ForgeSceneData* data );
	~ForgeScene();

	virtual void Resize();

	virtual bool Tap( int action, const grinliz::Vector2F& screen, const grinliz::Ray& world )				
	{
		return ProcessTap( action, screen, world );
	}
	virtual void ItemTapped( const gamui::UIItem* item );

	virtual void Draw3D( U32 delatTime );

private:
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
		RING_TRIAD,		// increases damage
		RING_BLADE,		// increases damage, decreases shield coupling
		NUM_RING_PARTS
	};

	void SetModel( bool randomizeTraits );

	LumosGame*			lumosGame;
	Engine*				engine;
	Screenport			screenport;
	Model*				model;
	grinliz::Random		random;
	gamui::PushButton	okay;

	ForgeSceneData*		forgeData;
	int					techRequired;
	TransactAmt			crystalRequired;
	GameItem*			item;
	grinliz::GLString	logText;

	gamui::ToggleButton	itemType[ForgeScript::NUM_ITEM_TYPES];
	gamui::ToggleButton gunType[ForgeScript::NUM_GUN_TYPES];
	gamui::ToggleButton gunParts[NUM_GUN_PARTS];
	gamui::ToggleButton ringParts[NUM_RING_PARTS];
	gamui::ToggleButton effects[ForgeScript::NUM_EFFECTS];
	gamui::TextLabel	requiredLabel,  techRequiredLabel;
	gamui::TextLabel	availableLabel, techAvailLabel;
	gamui::PushButton	buildButton;
	gamui::TextLabel	log;
	MoneyWidget			crystalRequiredWidget;
	MoneyWidget			crystalAvailWidget;
	ItemDescWidget		itemDescWidget;
};


#endif // FORGE_SCENE_INCLUDE
