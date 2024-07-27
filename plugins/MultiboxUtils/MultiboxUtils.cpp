#include "MultiboxUtils.h"

#include <GWCA/GWCA.h>

#include <GWCA/Utilities/Hooker.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
//#include <GWCA/Managers/CtoSMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/ItemMgr.h>



#define GAME_CMSG_INTERACT_GADGET                   (0x004F) // 79
#define GAME_CMSG_SEND_SIGNPOST_DIALOG              (0x0051) // 81

bool IsFollowing = true;
bool IsLooting = true;
bool LootingRoutineStarted = false;
static auto Last_Activity = clock();


DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static MultiboxUtils instance;
    return &instance;
}

void MultiboxUtils::Initialize(ImGuiContext* ctx, const ImGuiAllocFns fns, const HMODULE toolbox_dll)
{
    ToolboxPlugin::Initialize(ctx, fns, toolbox_dll);

    GW::Initialize();
    
    /*GW::Chat::CreateCommand(L"follow", [](const wchar_t*, int, const LPWSTR*) {
        GW::GameThread::Enqueue([] {
            IsFollowing = !IsFollowing;
            if (IsFollowing) {
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"Follow Activated", L"MultiboxUtils");
            }
            else {
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"Follow Deactivated", L"MultiboxUtils");
            }
        });
    });
    WriteChat(GW::Chat::CHANNEL_GWCA1, L"Use /follow to toggle Follow Party Leader", L"MultiboxUtils");

    GW::Chat::CreateCommand(L"loot", [](const wchar_t*, int, const LPWSTR*) {
        GW::GameThread::Enqueue([] {
            IsLooting = !IsLooting;
            if (IsLooting) {
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"Auto-Loot Activated", L"MultiboxUtils");
            }
            else {
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"Auto-Loot Deactivated", L"MultiboxUtils");
            }
            });
        });
    WriteChat(GW::Chat::CHANNEL_GWCA1, L"Use /loot to toggle Auto-Loot", L"MultiboxUtils");
    */
}

void MultiboxUtils::Update(float)
{
    uint32_t tLeaderid = 0;

    if ((GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) && (GW::PartyMgr::GetIsPartyLoaded())) {
        const auto tSelf = GW::Agents::GetPlayerAsAgentLiving();
       
        const GW::PartyInfo* Party = GW::PartyMgr::GetPartyInfo(); 

        if ((IsLooting) &&
            (tSelf) && 
            (!LootingRoutineStarted) &&
            (Party)
            ) {
            if ((tSelf->GetIsMoving()) || (tSelf->GetIsCasting()) || (tSelf->GetIsAttacking())) {
                Last_Activity = clock();
            }
            else {
                if ((clock() - Last_Activity) > 1000) {
                    LootingRoutineStarted = true;
                       //WriteChat(GW::Chat::CHANNEL_GWCA1, L"Looteando", L"MultiboxUtils");
                       TargetNearestItem();
                    LootingRoutineStarted = false;
                }
            }

        }

        // ----------------  Auto Following Routine --------------------------------
        if ((!LootingRoutineStarted) && (IsFollowing)) {
            if ((tSelf) && ((Party) && (Party->players[0].connected()))) {
                tLeaderid = GW::Agents::GetAgentIdByLoginNumber(Party->players[0].login_number);
                if (tLeaderid) {
                    auto tLeader = GW::Agents::GetAgentByID(tLeaderid);
                    if (tLeader) {
                        const auto tLeaderliving = tLeader->GetAsAgentLiving();
                        if (tLeaderliving) {
                            const auto dist = GW::GetDistance(tSelf->pos, tLeaderliving->pos);
                            if ((!tSelf->GetIsMoving()) &&
                                (!tSelf->GetIsCasting()) &&
                                (tSelf != tLeader)  &&
                                (dist > GW::Constants::Range::Area)) {
                                GW::Agents::InteractAgent(tLeader, false);
                            };
                        };
                    }
                }
            };
        };
        // ---------------- END Auto Following Routine --------------------------------
    };
    
}

void MultiboxUtils::TargetNearestItem()
{
    uint32_t model_id = 0;

    // target nearest agent
    const auto agents = GW::Agents::GetAgentArray();
    
    float distance = GW::Constants::SqrRange::Compass;
    size_t closest = 0;
    static GW::AgentID ItemID;


    for (const GW::Agent* agent : *agents) {
        if ((GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) && (GW::PartyMgr::GetIsPartyLoaded())) {
            if (agent) {
                if (agent->GetIsItemType()) {
                    const auto AgentItem = agent->GetAsAgentItem();
                    if (AgentItem) {
                        const auto ItemID = AgentItem->agent_id;
                        const auto tAssignedTo = AgentItem->owner;
                        const auto tSelf = GW::Agents::GetPlayerAsAgentLiving();
                        if ((ItemID) && (tSelf) && ((tAssignedTo == tSelf->agent_id) || (!tAssignedTo))) {
                            //GW::Agents::ChangeTarget(ItemID);
                            //auto tTarget = GW::Agents::GetTarget();
                            //GW::Agents::PickUpItem(tTarget, false);
                            // GW::Agents::PickUpItem(ItemID, false);
                            //GW::Items::PickupItem(GW::Agents::GetAgentByID(ItemID), false);
                            GW::Agents::InteractAgent(GW::Agents::GetAgentByID(ItemID), false);
                        }
                    }
                }
            }
        }
    }

}

void MultiboxUtils::SignalTerminate() 
{
    GW::Chat::DeleteCommand(L"follow");
    GW::DisableHooks();
}

bool MultiboxUtils::CanTerminate()
{
    return GW::HookBase::GetInHookCount() == 0;
}

void MultiboxUtils::Terminate()
{
    ToolboxPlugin::Terminate();
    GW::Terminate();
}


