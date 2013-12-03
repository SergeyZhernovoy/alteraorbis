#include "tasklist.h"
#include "marketai.h"

#include "../game/lumosmath.h"
#include "../game/pathmovecomponent.h"
#include "../game/workqueue.h"
#include "../game/aicomponent.h"
#include "../game/worldmap.h"
#include "../game/worldgrid.h"
#include "../game/lumoschitbag.h"
#include "../game/lumosgame.h"

#include "../xegame/chit.h"
#include "../xegame/spatialcomponent.h"
#include "../xegame/itemcomponent.h"
#include "../xegame/istringconst.h"

#include "../scenes/characterscene.h"
#include "../scenes/forgescene.h"

#include "../script/corescript.h"
#include "../script/buildscript.h"

#include "../engine/particle.h"

using namespace grinliz;
using namespace ai;

Task Task::RemoveTask( const grinliz::Vector2I& pos2i, int taskID )
{
	return Task::BuildTask( pos2i, BuildScript::CLEAR, taskID );
}


Vector2I TaskList::Pos2I() const
{
	Vector2I v = { 0, 0 };
	if ( !taskList.Empty() ) {
		v = taskList[0].pos2i;
	}
	return v;
}


void TaskList::Push( const Task& task )
{
	taskList.Push( task );
}


void TaskList::DoStanding( int time )
{
	if ( Standing() ) {
		taskList[0].timer -= time;
		if ( taskList[0].timer <= 0 ) {
			taskList.Remove(0);
		}
	}
}


void TaskList::DoTasks( Chit* chit, WorkQueue* workQueue, U32 delta, U32 since )
{
	if ( taskList.Empty() ) return;

	ComponentSet thisComp( chit, Chit::MOVE_BIT | Chit::SPATIAL_BIT | Chit::AI_BIT | Chit::ITEM_BIT | ComponentSet::IS_ALIVE );
	PathMoveComponent* pmc		= GET_SUB_COMPONENT( chit, MoveComponent, PathMoveComponent );

	if ( !pmc || !thisComp.okay ) {
		taskList.Clear();
		return;
	}

	LumosChitBag* chitBag = chit->GetLumosChitBag();
	Task* task			= &taskList[0];
	Vector2I pos2i		= thisComp.spatial->GetPosition2DI();
	Vector2F taskPos2	= { (float)task->pos2i.x + 0.5f, (float)task->pos2i.y + 0.5f };
	Vector3F taskPos3	= { taskPos2.x, 0, taskPos2.y };
	Vector2I sector		= ToSector( pos2i );
	CoreScript* coreScript = chitBag->GetCore( sector );
	Chit* controller = coreScript->ParentChit();

	// If this is a task associated with a work item, make
	// sure that work item still exists.
	if ( task->taskID ) {
		const WorkQueue::QueueItem* queueItem = 0;
		if ( workQueue ) {
			queueItem = workQueue->GetJobByTaskID( task->taskID );
		}
		if ( !queueItem ) {
			// This task it attached to a work item that expired.
			taskList.Clear();
			return;
		}
	}

	switch ( task->action ) {
	case Task::TASK_MOVE:
		if ( pmc->Stopped() ) {
			if ( pos2i == task->pos2i ) {
				// arrived!
				taskList.Remove(0);
			}
			else {
				Vector2F dest = { (float)task->pos2i.x + 0.5f, (float)task->pos2i.y + 0.5f };
				thisComp.ai->Move( dest, false );
			}
		}
		break;

	case Task::TASK_STAND:
		if ( pmc->Stopped() ) {
			task->timer -= (int)since;
			if ( task->timer <= 0 ) {
				taskList.Remove(0);
			}
			if ( taskList.Size() >= 2 ) {
				int action = taskList[1].action;
				if ( action == Task::TASK_BUILD ) {
					Vector3F pos = ToWorld3F( taskList[1].pos2i );
					pos.y = 0.5f;
					engine->particleSystem->EmitPD( "construction", pos, V3F_UP, 30 );	// FIXME: standard delta constant					
				}
			}
		}
		break;

	case Task::TASK_BUILD:
		{
			// "BUILD" just refers to a BuildScript task. Do some special case handling
			// up front for rocks. (Which aren't handled by the general code.)
			// Although a small kludge, way better than things used to be.

			const WorldGrid& wg = worldMap->GetWorldGrid( task->pos2i.x, task->pos2i.y );

			if ( task->buildScriptID == BuildScript::CLEAR ) {
				if ( wg.RockHeight() ) {
					DamageDesc dd( 10000, 0 );	// FIXME need constant
					Vector3I voxel = { task->pos2i.x, 0, task->pos2i.y };
					worldMap->VoxelHit( voxel, dd );
				}
				else {
					Chit* found = chitBag->QueryRemovable( task->pos2i );
					if ( found ) {
						GLASSERT( found->GetItem() );
						found->GetItem()->hp = 0;
						found->SetTickNeeded();
					}
				}
				if ( wg.Pave() ) {
					worldMap->SetPave( task->pos2i.x, task->pos2i.y, 0 );
				}
				taskList.Remove(0);
			}
			else if ( controller && controller->GetItem() ) {
				BuildScript buildScript;
				const BuildData& buildData	= buildScript.GetData( task->buildScriptID );

				if ( WorkQueue::TaskCanComplete( worldMap, chitBag, task->pos2i,
					task->buildScriptID,
					controller->GetItem()->wallet ))
				{
					if ( task->buildScriptID == BuildScript::ICE ) {
						worldMap->SetRock( task->pos2i.x, task->pos2i.y, 1, false, WorldGrid::ICE );
					}
					else if ( task->buildScriptID == BuildScript::PAVE ) {
						worldMap->SetPave( task->pos2i.x, task->pos2i.y, chit->PrimaryTeam()%3+1 );
					}
					else {
						// Move the build cost to the building. The gold is held there until the
						// building is destroyed
						GameItem* controllerItem = controller->GetItem();
						controllerItem->wallet.gold -= buildData.cost; 
						Chit* building = chitBag->NewBuilding( task->pos2i, buildData.cStructure, chit->PrimaryTeam() );
						building->GetItem()->wallet.AddGold( buildData.cost );
					}
					taskList.Remove(0);
				}
				else {
					taskList.Clear();
				}
			}
			else {
				// Plan has gone bust:
				taskList.Clear();
			}

			// No matter if successful or not, we need to clear the work item.
			// The work is done or can't be done.
			if ( workQueue ) {
				workQueue->Remove( pos2i );
			}
		}
		break;

	case Task::TASK_PICKUP:
		{
			int chitID = task->data;
			Chit* itemChit = chitBag->GetChit( chitID );
			if (    itemChit 
				 && itemChit->GetSpatialComponent()
				 && itemChit->GetSpatialComponent()->GetPosition2DI() == thisComp.spatial->GetPosition2DI()
				 && itemChit->GetItemComponent()->NumItems() == 1 )	// doesn't have sub-items / intrinsic
			{
				if ( thisComp.itemComponent->CanAddToInventory() ) {
					ItemComponent* ic = itemChit->GetItemComponent();
					itemChit->Remove( ic );
					chit->GetItemComponent()->AddToInventory( ic );
					chitBag->DeleteChit( itemChit );
				}
			}
			taskList.Remove(0);
		}
		break;

	case Task::TASK_USE_BUILDING:
		{
			Chit* building	= chitBag->QueryPorch( pos2i );
			if ( building ) {
				IString buildingName = building->GetItem()->name;

				if ( chit->PlayerControlled() ) {
					// Not sure how this happened. But do nothing.
				}
				else {
					UseBuilding( thisComp, building, buildingName );
				}
			}
			taskList.Remove(0);
		}
		break;

	default:
		GLASSERT( 0 );
		break;

	}
}


void TaskList::UseBuilding( const ComponentSet& thisComp, Chit* building, const grinliz::IString& buildingName )
{
	LumosChitBag* chitBag	= thisComp.chit->GetLumosChitBag();
	Vector2I pos2i			= thisComp.spatial->GetPosition2DI();
	Vector2I sector			= ToSector( pos2i );
	CoreScript* coreScript	= chitBag->GetCore( sector );
	Chit* controller		= coreScript->ParentChit();

	// Workers:
	if ( thisComp.item->flags & GameItem::AI_DOES_WORK ) {
		if ( buildingName == IStringConst::vault ) {
			GameItem* vaultItem = building->GetItem();
			GLASSERT( vaultItem );
			Wallet w = vaultItem->wallet.EmptyWallet();
			vaultItem->wallet.Add( w );

			// Put everything in the vault.
			ItemComponent* vaultIC = building->GetItemComponent();
			for( int i=1; i<thisComp.itemComponent->NumItems(); ++i ) {
				if ( vaultIC->CanAddToInventory() ) {
					GameItem* item = thisComp.itemComponent->RemoveFromInventory(i);
					if ( item ) {
						vaultIC->AddToInventory( item );
					}
				}
			}

			// Move gold & crystal to the owner?
			if ( controller && controller->GetItemComponent() ) {
				Wallet w = vaultItem->wallet.EmptyWallet();
				controller->GetItem()->wallet.Add( w );
			}
		}
		return;
	}
	if ( thisComp.item->flags & GameItem::AI_USES_BUILDINGS ) {
		BuildScript buildScript;
		const BuildData* bd = buildScript.GetDataFromStructure( buildingName );
		GLASSERT( bd );
		double needScale = 0;

		if ( buildingName == IStringConst::market ) {
			GoShopping( thisComp, building );
			needScale = 1;
		}
		else if ( buildingName == IStringConst::factory ) {
			// FIXME: use factory
			// FIXME: check for successfully building something
			needScale = 1;
		}
		else if ( buildingName == IStringConst::bed ) {
			needScale = 1;	// should be a delay here?
		}
		else if ( buildingName == IStringConst::bar ) {
			// FIXME: check social
			// FIXME: check for food
			needScale = 1;
		}
		else {
			GLASSERT( 0 );
		}
		thisComp.ai->GetNeedsMutable()->Add( bd->needs, needScale );
	}
}


void TaskList::GoShopping(  const ComponentSet& thisComp, Chit* market )
{
	GameItem* ranged=0;
	GameItem* melee=0;
	GameItem* shield=0;

	int rangedIndex=0, meleeIndex=0, shieldIndex=0;

	Vector2I sector = ToSector( thisComp.spatial->GetPosition2DI() );

	MarketAI marketAI( market );

	// The inventory is kept in value sorted order.
	// 1. Figure out the good stuff
	// 2. Sell everything that isn't "best of". FIXME: depending on personality, some denizens should keep back-ups
	// 3. Buy better stuff where it makes sense

	for( int i=1; i<thisComp.itemComponent->NumItems(); ++i ) {
		GameItem* gi = thisComp.itemComponent->GetItem( i );
		if ( !gi->Intrinsic() ) {
			if ( !ranged && (gi->flags & GameItem::RANGED_WEAPON)) {
				ranged = gi;
				rangedIndex = i;
			}
			else if ( !melee && !(gi->flags & GameItem::RANGED_WEAPON) && (gi->flags & GameItem::MELEE_WEAPON) ) {
				melee = gi;
				meleeIndex = i;
			}
			else if ( !shield && gi->hardpoint == HARDPOINT_SHIELD ) {
				shield = gi;
				shieldIndex = i;
			}
		}
	}

	for( int i=1; i<thisComp.itemComponent->NumItems(); ++i ) {
		GameItem* gi = thisComp.itemComponent->GetItem( i );
		int value = gi->GetValue();
		if ( value && !gi->Intrinsic() && (gi != ranged) && (gi != melee) && (gi != shield) ) {

			int sold = MarketAI::Transact(	gi, 
											market->GetItemComponent(),	// buyer
											thisComp.itemComponent,		// seller
											true );
			if ( sold ) {
				--i;
			}
		}
	}

	// Basic, critical buys:
	for( int i=0; i<3; ++i ) {
		const GameItem* purchase = 0;
		if ( i == 0 && !ranged ) purchase = marketAI.HasRanged( thisComp.item->wallet.gold, 1 );
		if ( i == 1 && !shield ) purchase = marketAI.HasShield( thisComp.item->wallet.gold, 1 );
		if ( i == 2 && !melee )  purchase = marketAI.HasMelee(  thisComp.item->wallet.gold, 1 );
		if ( purchase ) {
			
			MarketAI::Transact(	purchase,
								thisComp.itemComponent,
								market->GetItemComponent(),
								true );
		}
	}

	// Upgrades! Don't set ranged, shield, etc. because we don't want a critical followed 
	// by a non-critical. Also, personality probably should factor in to purchasing decisions.
	for( int i=0; i<3; ++i ) {
		const GameItem* purchase = 0;
		if ( i == 0 && ranged ) purchase = marketAI.HasRanged( thisComp.item->wallet.gold, ranged->GetValue() * 3 / 2 );
		if ( i == 1 && shield ) purchase = marketAI.HasShield( thisComp.item->wallet.gold, shield->GetValue() * 3 / 2 );
		if ( i == 2 && melee )  purchase = marketAI.HasMelee(  thisComp.item->wallet.gold, melee->GetValue() * 3 / 2 );
		if ( purchase ) {
			
			MarketAI::Transact(	purchase,
								thisComp.itemComponent,
								market->GetItemComponent(),
								true );
		}
	}
}
