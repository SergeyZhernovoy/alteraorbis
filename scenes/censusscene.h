#ifndef CENSUS_SCENE_INCLUDED
#define CENSUS_SCENE_INCLUDED

#include "../xegame/scene.h"
#include "../gamui/gamui.h"
#include "../script/itemscript.h"

class LumosGame;
class ItemComponent;
class ChitBag;
class GameItem;

struct ItemHistoryScore
{
	// sort descending
	static bool Equal(const ItemHistory& a, const ItemHistory& b) { return a.score == b.score; }
	static bool Less(const ItemHistory& a, const ItemHistory& b) { return a.score > b.score; }
};


struct ItemHistoryKills
{
	// sort descending
	static bool Equal(const ItemHistory& a, const ItemHistory& b) { return a.kills == b.kills; }
	static bool Less(const ItemHistory& a, const ItemHistory& b) { return a.kills > b.kills; }
};


struct ItemHistoryGreaterKills
{
	// sort descending
	static bool Equal(const ItemHistory& a, const ItemHistory& b) { return a.greater == b.greater; }
	static bool Less(const ItemHistory& a, const ItemHistory& b) { return a.greater > b.greater; }
};


struct ItemHistoryCrafting
{
	// sort descending
	static bool Equal(const ItemHistory& a, const ItemHistory& b) { return a.crafted == b.crafted; }
	static bool Less(const ItemHistory& a, const ItemHistory& b) { return a.crafted > b.crafted; }
};


class CensusSceneData : public SceneData
{
public:
	CensusSceneData( ChitBag* _chitBag )				// apply markup, costs
		: SceneData(), chitBag(_chitBag) { GLASSERT(chitBag); }

	ChitBag* chitBag;
};


class CensusScene : public Scene
{
public:
	CensusScene( LumosGame* game, CensusSceneData* data );
	virtual ~CensusScene();

	virtual void Resize();

	virtual void Tap( int action, const grinliz::Vector2F& screen, const grinliz::Ray& world )				
	{
		ProcessTap( action, screen, world );
	}
	virtual void ItemTapped( const gamui::UIItem* item );

private:
	void Scan();
	void ScanItem( ItemComponent* ic, const GameItem* );

	LumosGame*			lumosGame;
	ChitBag*			chitBag;
	gamui::PushButton	okay;
	gamui::TextLabel	text;

	// stuff to scan for:
	Wallet	allWallet, 
			mobWallet;

	enum {
		MOB_DENIZEN,
		MOB_GREATER,
		MOB_LESSER,
		MOB_COUNT
	};

	enum {
		ITEM_PISTOL,
		ITEM_BLASTER,
		ITEM_PULSE,
		ITEM_BEAMGUN,
		ITEM_RING,
		ITEM_SHIELD,
		ITEM_COUNT
	};

	void AddToHistory(const ItemHistory& h);
	grinliz::SortedDynArray<ItemHistory, grinliz::ValueSem, ItemHistoryScore> domains;
	grinliz::SortedDynArray<ItemHistory, grinliz::ValueSem, ItemHistoryKills> kills;
	grinliz::SortedDynArray<ItemHistory, grinliz::ValueSem, ItemHistoryGreaterKills> greaterKills;
	grinliz::SortedDynArray<ItemHistory, grinliz::ValueSem, ItemHistoryCrafting> crafted;

	struct Info {
		const GameItem*			item;
		const ItemComponent*	ic;
	};

	Info mobActive[MOB_COUNT];
	Info itemActive[ITEM_COUNT];
};

#endif // CENSUS_SCENE_INCLUDED

