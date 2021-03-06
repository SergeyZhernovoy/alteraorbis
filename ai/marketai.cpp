#include "marketai.h"
#include "../xegame/chit.h"
#include "../game/gameitem.h" 
#include "../game/news.h"
#include "../game/reservebank.h"
#include "../xegame/itemcomponent.h"
#include "../xegame/spatialcomponent.h"
#include "../xegame/chitcontext.h"
#include "../game/lumoschitbag.h"
#include "../script/corescript.h"

using namespace grinliz;

MarketAI::MarketAI( Chit* c ) : chit(c)
{
	GLASSERT( chit && chit->GetItem() && chit->GetItem()->IName() == ISC::market );
	ic = chit->GetItemComponent();
	GLASSERT( ic );
}


const GameItem* MarketAI::Has( int flag, int maxAuCost, int minAuValue )
{
	for( int i=1; i<ic->NumItems(); ++i ) {
		const GameItem* item = ic->GetItem( i );

		if (    (flag == RANGED && item->ToRangedWeapon())
			 || (flag == MELEE && item->ToMeleeWeapon())
			 || (flag == SHIELD && item->ToShield()))
		{ 
			int value = item->GetValue();
			if ( value > 0 && value >= minAuValue && value <= maxAuCost ) {
				return item;
			}
		}
	}
	return 0;
}


/*static*/ void MarketAI::RecoupAndDeleteOverflowItem(GameItem* item, Chit* market)
{
	GLASSERT(item);
	GLASSERT(market);

	Vector2I sector = ToSector(market->Position());
	CoreScript* cs = CoreScript::GetCore(sector);

	if (cs && cs->InUse()) {
		cs->ParentChit()->GetWallet()->Deposit(&item->wallet, item->wallet);
	}
	else if (ReserveBank::GetWallet()) {
		ReserveBank::GetWallet()->Deposit(&item->wallet, item->wallet);
	}
	delete item;
}


/*	General "transact" method because this is very tricky code to get right.
	There's always another case or bit of logic to account for.
*/
/*static*/ int MarketAI::Transact(const GameItem* item, ItemComponent* buyer, ItemComponent* seller, Wallet* salesTax, bool doTrade)
{
	GLASSERT(!item->Intrinsic());
	int cost = item->GetValue();

	// Can always add to inventory: will be sold to reserve bank, if needed.
	// Will try to send gems to exchange first.
	if (buyer->GetItem()->wallet.Gold() >= cost)
	{
		if (doTrade) {
			buyer->MakeRoomInInventory();
			GLASSERT(buyer->CanAddToInventory());

			GameItem* gi = seller->RemoveFromInventory(item);
			GLASSERT(gi);
			buyer->AddToInventory(gi);
			seller->GetItem()->wallet.Deposit(&buyer->GetItem()->wallet, cost);

			Vector2F pos = ToWorld2F(seller->ParentChit()->Position());
			Vector2I sector = ToSector(seller->ParentChit()->Position());
			GLOUTPUT(("'%s' sold to '%s' the item '%s' for %d Au at sector=%c%d\n",
				seller->ParentChit()->GetItem()->BestName(),
				buyer->ParentChit()->GetItem()->BestName(),
				gi->BestName(),
				cost,
				'A' + sector.x, 1 + sector.y));
			(void)sector;

			if (salesTax && ReserveBank::Instance()) {
				int tax = LRint(float(cost) * SALES_TAX);
				if (tax > 0) {
					salesTax->Deposit(ReserveBank::GetWallet(), tax);
					//GLOUTPUT(("  tax paid: %d\n", tax));
				}
			}

			NewsHistory* history = seller->ParentChit()->Context()->chitBag->GetNewsHistory();
			history->Add(NewsEvent(NewsEvent::PURCHASED, pos, gi->ID(), buyer->ParentChit()->GetItemID()));
		}
		return cost;
	}
	return 0;
}


/*static*/ bool MarketAI::CoreHasMarket(CoreScript* cs)
{
	if (!cs) return false;
	const ChitContext* context = cs->ParentChit()->Context();
	GLASSERT(context);
	Rectangle2I bounds = SectorBounds(ToSector(cs->ParentChit()->Position()));
	return context->chitBag->QueryBuilding(ISC::market, bounds, nullptr) != 0;
}

/*static*/ bool MarketAI::CoreHasMarket(CoreScript* cs, Chit* chit)
{
	if (!cs) return false;
	if (!chit) return false;

	const ChitContext* context = cs->ParentChit()->Context();
	GLASSERT(context);
	Rectangle2I bounds = SectorBounds(ToSector(cs->ParentChit()->Position()));
	if (context->chitBag->QueryBuilding(ISC::market, bounds, nullptr) != 0) {
		return Team::Instance()->GetRelationship(cs->ParentChit(), chit) != ERelate::ENEMY;
	}
	return false;
}
