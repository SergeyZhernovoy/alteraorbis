#include "rebuildai.h"

#include "../xegame/chitbag.h"
#include "../xegame/spatialcomponent.h"

#include "../game/lumoschitbag.h"
#include "../game/gameitem.h"
#include "../game/reservebank.h"
#include "../game/workqueue.h"

#include "../script/corescript.h"
#include "../script/buildscript.h"

using namespace grinliz;

RebuildAIComponent::RebuildAIComponent() : ticker(2000)
{
}


RebuildAIComponent::~RebuildAIComponent()
{
}


void RebuildAIComponent::OnAdd(Chit* chit, bool initialize)
{
	super::OnAdd(chit, initialize);
	// Broadcast of destroyed messages is how we 
	// know what to repair.
	Context()->chitBag->AddListener(this);
	ticker.Randomize(ParentChit()->random.Rand());
}


void RebuildAIComponent::OnRemove()
{
	Context()->chitBag->RemoveListener(this);
	super::OnRemove();
}


void RebuildAIComponent::Serialize(XStream* xs)
{
	BeginSerialize(xs, Name());

	EndSerialize(xs);
}


void RebuildAIComponent::OnChitMsg(Chit* chit, const ChitMsg& msg)
{
	if (   msg.ID() == ChitMsg::CHIT_DESTROYED 
		&& (chit != ParentChit())
		&& InSameSector(chit, ParentChit())
		&& chit->Team() == this->ParentChit()->Team())
	{
		BuildingFilter buildingFilter;
		if (buildingFilter.Accept(chit))
		{
			GameItem* gi = chit->GetItem();
			WorkItem* wi = workItems.PushArr(1);
			wi->structure = gi->IName();
			wi->pos = chit->GetSpatialComponent()->Bounds().min;
			wi->rot = LRint(chit->GetSpatialComponent()->GetYRotation());
			GLOUTPUT(("ReBuild: Structure %s at %d,%d r=%d to rebuild queue.\n", wi->structure.safe_str(), wi->pos.x, wi->pos.y, int(wi->rot)));
		}
	}
	super::OnChitMsg(chit, msg);
}


int RebuildAIComponent::DoTick(U32 delta)
{
	SpatialComponent* spatial = parentChit->GetSpatialComponent();
	if (!spatial) return ticker.Next();

	GameItem* mainItem = parentChit->GetItem();
	Vector2I sector = spatial->GetSector();
	CoreScript* cs = CoreScript::GetCore(sector);

	if (ticker.Delta(delta) && mainItem && spatial && cs) {

		// Create workers, if needed.
		Rectangle2F b = ToWorld(InnerSectorBounds(sector));
		CChitArray arr;
		ItemNameFilter workerFilter(ISC::worker, IChitAccept::MOB);
		Context()->chitBag->QuerySpatialHash(&arr, b, 0, &workerFilter);
		if (arr.Empty()) {
			if (mainItem->wallet.Gold() >= WORKER_BOT_COST) {
				ReserveBank::GetWallet()->Deposit(&mainItem->wallet, WORKER_BOT_COST);
				Context()->chitBag->NewWorkerChit(cs->ParentChit()->GetSpatialComponent()->GetPosition(), parentChit->Team());
			}
		}

		// Pull a task off the queue and send a worker out.
		if (workItems.Size()) {
			WorkQueue* workQueue = cs->GetWorkQueue();
			if (workQueue) {
				WorkItem wi = workItems.Pop();
				BuildScript buildScript;
				int id = 0;
				buildScript.GetDataFromStructure(wi.structure, &id);
				if (workQueue->AddAction(wi.pos, id, float(wi.rot), 0)) {
					GLOUTPUT(("ReBuild: Structure %s at %d,%d r=%d POP to work queue.\n", wi.structure.safe_str(), wi.pos.x, wi.pos.y, int(wi.rot)));
				}
				else {
					GLOUTPUT(("ReBuild: Re-Push structure %s at %d,%d to work queue.\n", wi.structure.safe_str(), wi.pos.x, wi.pos.y));
					workItems.Push(wi);
				}
			}
		}
	}
	return ticker.Next();
}
