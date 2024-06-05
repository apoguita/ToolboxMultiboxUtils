#include "MultiboxUtils.h"

#include <GWCA/GWCA.h>

#include <GWCA/Utilities/Hooker.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/Managers/SkillbarMgr.h>
//#include <Modules/InventoryManager.h>



#define GAME_CMSG_INTERACT_GADGET                   (0x004F) // 79
#define GAME_CMSG_SEND_SIGNPOST_DIALOG              (0x0051) // 81

uint32_t last_hovered_item_id = 0;


enum eState { Idle, Looting, Following, Scatter };

struct StateMachine {
    eState State;
    clock_t LastActivity;
    bool IsFollowingEnabled;
    bool IsLootingEnabled;
    bool IsCombatEnabled;
    bool IsTargettingEnabled;
    bool IsScatterEnabled;
};

StateMachine Machine;

//static auto Last_Skill_Activity = clock();



enum combatParams {unknown, skilladd, skillremove};
combatParams Combatstate;

bool CombatSkillState[8];

enum combatState { cIdle, InCastingRoutine, InRecharge };

struct CombatStateMachine {
    combatState State;
    clock_t LastActivity;
    clock_t StayAlert;
    uint32_t castingptr =0;
    uint32_t currentCastingSlot;
    bool IsSkillCasting = false;
    float IntervalSkillCasting;
};

CombatStateMachine CombatMachine;

static GW::AgentID oldCalledTarget;
float oldAngle;

float ScatterPos[8] = {0.0f, 45.0f, -45.0f, 90.0f, -90.0f, 135.0f, -135.0f, 180.0f };





void wcs_tolower(wchar_t* s)
{
    for (size_t i = 0; s[i]; i++)
        s[i] = towlower(s[i]);
}


DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static MultiboxUtils instance;
    return &instance;
}

static void CmdCombat(const wchar_t*, const int argc, const LPWSTR* argv) {
    wcs_tolower(*argv);

    if (argc == 1) {
        Machine.IsCombatEnabled = !Machine.IsCombatEnabled;
        if (Machine.IsCombatEnabled) {
            WriteChat(GW::Chat::CHANNEL_GWCA1, L"CombatAI Enabled", L"MultiboxUtils");
        }
        else {
            WriteChat(GW::Chat::CHANNEL_GWCA1, L"CombatAI Disabled", L"MultiboxUtils");
        }
        return;
    }

    if (argc == 2) {
        WriteChat(GW::Chat::CHANNEL_GWCA1, L"CombatAI: bad parameters, need at least 2", L"MultiboxUtils");
        return;
    }

    

    int cIndex = -1;
    const wchar_t first_char_of_first_arg = *argv[1];
    const wchar_t first_char_of_last_arg = *argv[argc - 1];

    Combatstate = unknown; 
    //if (argv[2] == L"add") { Combatstate = skilladd; }
    //if (argv[2] == L"remove") { Combatstate = skillremove; }
    if (first_char_of_first_arg == 'a') { Combatstate = skilladd; }
    if (first_char_of_first_arg == 'r') { Combatstate = skillremove; } 

    switch (Combatstate) {
        case skillremove:

            if (first_char_of_last_arg == '1') { cIndex = 1; }
            if (first_char_of_last_arg == '2') { cIndex = 2; }
            if (first_char_of_last_arg == '3') { cIndex = 3; }
            if (first_char_of_last_arg == '4') { cIndex = 4; }
            if (first_char_of_last_arg == '5') { cIndex = 5; }
            if (first_char_of_last_arg == '6') { cIndex = 6; }
            if (first_char_of_last_arg == '7') { cIndex = 7; }
            if (first_char_of_last_arg == '8') { cIndex = 8; }
            if (first_char_of_last_arg == 'a') { cIndex = 0; }

            if (cIndex < 0) {
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"CombatAI: bad parameter, skill#", L"MultiboxUtils");
                break;
            }

            if (cIndex == 0) {
                for (int i = 0; i < 8; i++) {
                    CombatSkillState[i] = false;
                }
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"CombatAI: skills 1-8 removed from combat loop", L"MultiboxUtils");

            }
            else {
                CombatSkillState[cIndex - 1] = false;
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"CombatAI: skill removed from combat loop", L"MultiboxUtils");

            }

            break;

    
        case skilladd: 
            
            if (first_char_of_last_arg == '1') { cIndex = 1; }
            if (first_char_of_last_arg == '2') { cIndex = 2; }
            if (first_char_of_last_arg == '3') { cIndex = 3; }
            if (first_char_of_last_arg == '4') { cIndex = 4; }
            if (first_char_of_last_arg == '5') { cIndex = 5; }
            if (first_char_of_last_arg == '6') { cIndex = 6; }
            if (first_char_of_last_arg == '7') { cIndex = 7; }
            if (first_char_of_last_arg == '8') { cIndex = 8; }
            if (first_char_of_last_arg == 'a') { cIndex = 0; }

            if (cIndex < 0) {
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"CombatAI: bad parameter, skill#", L"MultiboxUtils");
                break;
            }

            if (cIndex == 0) {
                for (int i = 0; i < 8; i++) {
                    CombatSkillState[i] = true;
                }
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"CombatAI: skills 1-8 added to combat loop", L"MultiboxUtils");

            }
            else {
                CombatSkillState[cIndex-1] = true;
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"CombatAI: skill added to combat loop", L"MultiboxUtils");

            }
           
            break;
        default:
            WriteChat(GW::Chat::CHANNEL_GWCA1, L"CombatAI: bad parameter, use add/remove", L"MultiboxUtils");
        
    }
}


void MultiboxUtils::Initialize(ImGuiContext* ctx, const ImGuiAllocFns fns, const HMODULE toolbox_dll)
{
    ToolboxPlugin::Initialize(ctx, fns, toolbox_dll);

    GW::Initialize();

    Machine.State = Idle; 
    Machine.IsFollowingEnabled = false;
    Machine.IsLootingEnabled = false;


    GW::Chat::CreateCommand(L"autoloot", [](const wchar_t*, int, const LPWSTR*) {
        GW::GameThread::Enqueue([] { 
            Machine.IsLootingEnabled = !Machine.IsLootingEnabled;

            if (Machine.IsLootingEnabled) {
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"LootAI Enabled", L"MultiboxUtils"); 
            }
            else {
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"LootAI Disabled", L"MultiboxUtils"); 
            }
            });
        }); 
     
    WriteChat(GW::Chat::CHANNEL_GWCA1, L"Use /autoloot To toggle LootAI", L"MultiboxUtils");

    GW::Chat::CreateCommand(L"autofollow", [](const wchar_t*, int, const LPWSTR*) {
        GW::GameThread::Enqueue([] {
            Machine.IsFollowingEnabled = !Machine.IsFollowingEnabled;
            
            if (Machine.IsFollowingEnabled) {
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"FollowAI Enabled", L"MultiboxUtils");
            }
            else {
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"FollowAI Disabled", L"MultiboxUtils");
            }
            });
        });
    WriteChat(GW::Chat::CHANNEL_GWCA1, L"Use /autofollow To toggle Follow party Leader", L"MultiboxUtils");

    GW::Chat::CreateCommand(L"dropitem", [](const wchar_t*, int, const LPWSTR*) {
        GW::GameThread::Enqueue([] {
            if ((GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) && (GW::PartyMgr::GetIsPartyLoaded())) {
                const GW::Item* current = GW::Items::GetHoveredItem();
                if (current) {
                    GW::Items::DropItem(current, current->quantity);
                }
            }
            else {
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"/dropitem Command only works in Explorable areas", L"MultiboxUtils");
            }
            });
        });
    WriteChat(GW::Chat::CHANNEL_GWCA1, L"Use /dropitem To drop hovered item to the ground", L"MultiboxUtils");

    GW::Chat::CreateCommand(L"autotarget", [](const wchar_t*, int, const LPWSTR*) {
            GW::GameThread::Enqueue([] {
            Machine.IsTargettingEnabled = !Machine.IsTargettingEnabled;

            if (Machine.IsTargettingEnabled) {
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"TargettingAI Enabled", L"MultiboxUtils");
            }
            else {
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"TargettingAI Disabled", L"MultiboxUtils");
            }
            });
        });
    WriteChat(GW::Chat::CHANNEL_GWCA1, L"Use /autotarget To drop hovered item to the ground", L"MultiboxUtils");

    GW::Chat::CreateCommand(L"scatter", [](const wchar_t*, int, const LPWSTR*) {
        GW::GameThread::Enqueue([] {
            Machine.IsScatterEnabled = !Machine.IsScatterEnabled;

            if (Machine.IsScatterEnabled) {
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"ScatterAI Enabled", L"MultiboxUtils");
            }
            else {
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"ScatterAI Disabled", L"MultiboxUtils");
            }
            });
        });
    WriteChat(GW::Chat::CHANNEL_GWCA1, L"Use /scatter toggle Scatter AI", L"MultiboxUtils");

    GW::Chat::CreateCommand(L"heroai", [](const wchar_t*, int, const LPWSTR*) {
        GW::GameThread::Enqueue([] {
            
            Machine.IsFollowingEnabled = !Machine.IsFollowingEnabled;
            Machine.IsLootingEnabled = !Machine.IsLootingEnabled;
            Machine.IsCombatEnabled = !Machine.IsCombatEnabled;
            Machine.IsTargettingEnabled = !Machine.IsTargettingEnabled;
            Machine.IsScatterEnabled = !Machine.IsScatterEnabled;


            for (int i = 0; i < 8; i++) {
                CombatSkillState[i] = !CombatSkillState[i];
            }

            if (Machine.IsFollowingEnabled) {
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"HeroAI Enabled", L"MultiboxUtils");
            }
            else {
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"HeroAI Disabled", L"MultiboxUtils");
            }
            });
        });
    WriteChat(GW::Chat::CHANNEL_GWCA1, L"Use /heroai to toggle all features at once", L"MultiboxUtils");

    GW::Chat::CreateCommand(L"autocombat", &CmdCombat);

    WriteChat(GW::Chat::CHANNEL_GWCA1, L"Use /autocombat Toggle Auto-combat", L"MultiboxUtils");
    WriteChat(GW::Chat::CHANNEL_GWCA1, L"Use /autocombat add [1-8,all] To add skill to combat queue.", L"MultiboxUtils");
    WriteChat(GW::Chat::CHANNEL_GWCA1, L"Use /autocombat remove [1-8,all] To remove skill from combat queue.", L"MultiboxUtils");

}

bool MultiboxUtils::IsMapExplorable() {
    if ((GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) && (GW::PartyMgr::GetIsPartyLoaded())) 
    { return true;}
    else { return false; }
};

float DegToRad(float degree) {
    // converting degrees to radians
    return ( degree * 3.14159 / 180);
}

void MultiboxUtils::Update(float)
{
    if (!IsMapExplorable()) { return; }
    const auto tSelf = GW::Agents::GetPlayerAsAgentLiving();
    const GW::PartyInfo* Party = GW::PartyMgr::GetPartyInfo();
    if ((!tSelf) || (!Party)) { return; }
    if (!Party->players[0].connected()) { return; }
    const auto tLeaderid = GW::Agents::GetAgentIdByLoginNumber(Party->players[0].login_number);
    if (!tLeaderid) { return; }
    const auto tLeader = GW::Agents::GetAgentByID(tLeaderid);
    if (!tLeader) { return; }
    const auto tLeaderliving = tLeader->GetAsAgentLiving();
    if (!tLeaderliving) { return; }

    const auto dist = GW::GetDistance(tSelf->pos, tLeaderliving->pos);

    if (!TargetNearestEnemyInAggro()) { CombatMachine.castingptr = 0; CombatMachine.State = cIdle;
    }

    /****************** TARGERT MONITOR **********************/
    const auto targetChange = PartyTargetIDChanged(); 
    if ((Machine.IsTargettingEnabled) &&
        (targetChange != 0)) {
        GW::Agents::ChangeTarget(targetChange);
    }
    /****************** END TARGET MONITOR **********************/

    // ENHANCE SCANNING AREA ON COMBAT ACTIVITY 

    if ((tLeaderliving->GetIsAttacking()) ||
        (tLeaderliving->GetIsCasting())) {

        CombatMachine.StayAlert = clock();
    }

    if (tSelf->GetIsMoving()) { return; }
    if (CombatMachine.State == InCastingRoutine) { goto forceCombat; }; //dirty trick to enforce combat check
    
    /**************** LOOTING *****************/
    uint32_t tItem; 

    tItem = TargetNearestItem(); 
    if ((Machine.IsLootingEnabled) &&
        (!TargetNearestEnemyInAggro()) &&
        (ThereIsSpaceInInventory()) &&
        (tItem != 0)
        ) {
        
        GW::Agents::PickUpItem(GW::Agents::GetAgentByID(tItem), false);
        return; 
    }
    /**************** END LOOTING *************/

    /************ FOLLOWING *********************/
    

    if ((Machine.IsFollowingEnabled) && 
        (dist > GW::Constants::Range::Area) && 
        (!tSelf->GetIsCasting()) 
        ) {
        //GW::Agents::InteractAgent(tLeader, false);
        const auto xx = tLeaderliving->x; 
        const auto yy = tLeaderliving->y; 
        const auto heropos = (GetPartyNumber() - 1) + GW::PartyMgr::GetPartyHeroCount() + GW::PartyMgr::GetPartyHenchmanCount(); 
        const auto angle = tLeaderliving->rotation_angle + DegToRad(ScatterPos[heropos]); 
        const auto rotcos = cos(angle);
        const auto rotsin = sin(angle);


        const auto posx = GW::Constants::Range::Adjacent * rotcos;
        const auto posy = GW::Constants::Range::Adjacent * rotsin;

        GW::Agents::Move(xx + posx, yy + posy);
        return;
    }
    /************ END FOLLOWING *********************/

    /*************** SCATTER ****************************/
    if ((Machine.IsScatterEnabled) &&
        (!tSelf->GetIsCasting()) &&
        ((dist <= GW::Constants::Range::Touch) ||
         (AngleChange() && (!TargetNearestEnemyInAggro()))
        )
        //Need to include checks for melee chars
        ) {
        const auto xx = tLeaderliving->x;
        const auto yy = tLeaderliving->y;
        const auto heropos = (GetPartyNumber() - 1) + GW::PartyMgr::GetPartyHeroCount() + GW::PartyMgr::GetPartyHenchmanCount();
        const auto angle = tLeaderliving->rotation_angle + DegToRad(ScatterPos[heropos]);
        const auto rotcos = cos(angle);
        const auto rotsin = sin(angle);


        const auto posx = GW::Constants::Range::Adjacent * rotcos;
        const auto posy = GW::Constants::Range::Adjacent * rotsin;

        GW::Agents::Move(xx + posx, yy + posy);
        return;
        //AngleChange()
    }
    /***************** END SCATTER   ************************/

    /******************COMBAT ROUTINES *********************************/
    forceCombat:

    if ((Machine.IsCombatEnabled) && (TargetNearestEnemyInAggro() != 0)) {
        if (CombatMachine.castingptr >= 8) { CombatMachine.castingptr = 0; }
        switch (CombatMachine.State) {
        case cIdle:

            if (!CombatSkillState[CombatMachine.castingptr]) { CombatMachine.castingptr++; break; }
            if (!IsAllowedCast()) {
                break;
            }

            if (IsSkillready(CombatMachine.castingptr) && (IsAllowedCast())) {

                CombatMachine.State = InCastingRoutine;
                CombatMachine.currentCastingSlot = CombatMachine.castingptr;

                CombatMachine.LastActivity = clock();
                CombatMachine.IntervalSkillCasting = 750.0f;

                SmartSelectTarget();
                CastSkill(CombatMachine.currentCastingSlot);
                break;

            }
            else {
                CombatMachine.castingptr++;
                break;
            }
            break;
        case InCastingRoutine:

            if (!IsAllowedCast()) { break; }
            CombatMachine.State = cIdle;

            CombatMachine.castingptr++;
            break;
        default:
            CombatMachine.State = cIdle;
        }
        
    }
    
}

int MultiboxUtils::GetPartyNumber() {
    const GW::PartyInfo* Party = GW::PartyMgr::GetPartyInfo(); 
    if (!Party) { return 0; } 

    const auto tSelf = GW::Agents::GetPlayerAsAgentLiving();
    const auto tSelfId = tSelf->agent_id;

    for (int i = 0; i < GW::PartyMgr::GetPartySize(); i++)
    {
        const auto playerId = GW::Agents::GetAgentIdByLoginNumber(Party->players[i].login_number);
        if (playerId == tSelfId) {
            return i+1;
        }
    }
    return 0;
}

float MultiboxUtils::AngleChange() {

    const GW::PartyInfo* Party = GW::PartyMgr::GetPartyInfo();
    if (!Party) { return 0.0f; }

    if (!Party->players[0].connected()) { return 0.0f; }
    const auto tLeaderid = GW::Agents::GetAgentIdByLoginNumber(Party->players[0].login_number);
    if (!tLeaderid) { return 0.0f; }
    const auto tLeader = GW::Agents::GetAgentByID(tLeaderid);
    if (!tLeader) { return 0.0f; }

    const auto tLeaderliving = tLeader->GetAsAgentLiving(); 
    if (!tLeaderliving) { return 0.0f; } 

    const auto angle = tLeaderliving->rotation_angle;
    if (!angle) {
        oldCalledTarget = 0.0f;
        return 0.0f;
    }

    if (oldAngle != angle) {
        oldAngle = angle;
        return oldAngle;
    }

    return 0.0f;

}

GW::AgentID MultiboxUtils::PartyTargetIDChanged() {
 
    const GW::PartyInfo* Party = GW::PartyMgr::GetPartyInfo(); 
    if (!Party) { return 0; }
    
    if (!Party->players[0].connected()) { return 0; }
    const auto tLeaderid = GW::Agents::GetAgentIdByLoginNumber(Party->players[0].login_number); 
    if (!tLeaderid) { return 0; } 
    const auto tLeader = GW::Agents::GetAgentByID(tLeaderid);
    if (!tLeader) { return 0; }

    const auto target = Party->players[0].calledTargetId; 
 
    if (!target) {
        oldCalledTarget = 0;
        return 0;
    }

    if (oldCalledTarget != target) {
        oldCalledTarget = target;
        return oldCalledTarget;
    }
 
    return 0;

}

uint32_t  MultiboxUtils::TargetNearestEnemyInAggro() {
    const auto agents = GW::Agents::GetAgentArray();
    const auto tSelf = GW::Agents::GetPlayerAsAgentLiving();

    if (!tSelf) { return 0; }

    float distance = GW::Constants::Range::Earshot;
    float maxdistance = GW::Constants::Range::Earshot;
    if ((clock() - CombatMachine.StayAlert) <= 2500) {

        distance = GW::Constants::Range::Spellcast;
        maxdistance = GW::Constants::Range::Spellcast;
    }

    static GW::AgentID EnemyID;
    static GW::AgentID tStoredEnemyID = 0;

    for (const GW::Agent* agent : *agents)
    {
        if (!IsMapExplorable()) { return 0; }
        if (!agent) { continue; }
        if (!agent->GetIsLivingType()) { continue; }
        const auto tEnemyLiving = agent->GetAsAgentLiving();
        if (!tEnemyLiving) { continue; }
        if (tEnemyLiving->allegiance != GW::Constants::Allegiance::Enemy) { continue; }

        EnemyID = tEnemyLiving->agent_id;

        if (!EnemyID) { continue; }

        const auto tempAgent = GW::Agents::GetAgentByID(EnemyID);

        if (!tempAgent) { continue; }
        if (!tempAgent->GetAsAgentLiving()->GetIsAlive()) { continue; }
        const auto dist = GW::GetDistance(tSelf->pos, tempAgent->pos);

        if (dist < distance) {
            distance = dist;
            tStoredEnemyID = EnemyID;
        }
    }
    if (distance != maxdistance){ return tStoredEnemyID; }
    else {
        return 0;
    }
}

uint32_t  MultiboxUtils::TargetNearestEnemyInBigAggro() {
    const auto agents = GW::Agents::GetAgentArray(); 
    const auto tSelf = GW::Agents::GetPlayerAsAgentLiving(); 

    if (!tSelf) { return 0; } 

    float distance = GW::Constants::Range::Spellcast;
    static GW::AgentID EnemyID; 
    static GW::AgentID tStoredEnemyID = 0; 

    for (const GW::Agent* agent : *agents) 
    {
        if (!IsMapExplorable()) { return 0; }
        if (!agent) { continue; } 
        if (!agent->GetIsLivingType()) { continue; } 
        const auto tEnemyLiving = agent->GetAsAgentLiving(); 
        if (!tEnemyLiving) { continue; }
        if (tEnemyLiving->allegiance != GW::Constants::Allegiance::Enemy) { continue; }

        EnemyID = tEnemyLiving->agent_id;

        if (!EnemyID) { continue; } 

        const auto tempAgent = GW::Agents::GetAgentByID(EnemyID); 

        if (!tempAgent) { continue; }
        if (!tempAgent->GetAsAgentLiving()->GetIsAlive()) { continue; }
        const auto dist = GW::GetDistance(tSelf->pos, tempAgent->pos);

        if (dist < distance) {
            distance = dist;
            tStoredEnemyID = EnemyID;
        }
    }
    if (distance != GW::Constants::Range::Spellcast) { return tStoredEnemyID; }
    else {
        return 0;
    }
}

uint32_t MultiboxUtils::TargetNearestItem()
{
    const auto agents = GW::Agents::GetAgentArray();
    const auto tSelf = GW::Agents::GetPlayerAsAgentLiving();

    if (!tSelf) { return 0; }

    float distance = GW::Constants::Range::Spellcast;
    
    static GW::AgentID ItemID;
    static GW::AgentID tStoredItemID = 0;

    for (const GW::Agent* agent : *agents)
    {
        if (!IsMapExplorable()) { return 0; }
        if (!agent) { continue; }
        if (!agent->GetIsItemType()) { continue; }

        const auto AgentItem = agent->GetAsAgentItem();

        if (!AgentItem) { continue; }
        
        uint32_t ItemID = AgentItem->agent_id;
        const auto tAssignedTo = AgentItem->owner;
        
        if (!ItemID) { continue; }
        if ((tAssignedTo) && (tAssignedTo != tSelf->agent_id)) { continue; }

        const auto tempAgent = GW::Agents::GetAgentByID(ItemID);

        if (!tempAgent) { continue; }

       
        const auto dist = GW::GetDistance(tSelf->pos, tempAgent->pos); 

        if (dist < distance) {
            distance = dist;
            tStoredItemID = ItemID;
        }
    }
    if (distance != GW::Constants::Range::Spellcast) { return tStoredItemID; }
    else {
        return 0;
    }  
}

bool MultiboxUtils::IsSkillready(uint32_t slot) {
    const auto skillbar = GW::SkillbarMgr::GetPlayerSkillbar(); 

    if (!skillbar || !skillbar->IsValid()) { return false; } 

    const GW::SkillbarSkill& skill = skillbar->skills[slot]; 

    if (skill.skill_id == GW::Constants::SkillID::No_Skill) { return false; } 

    const GW::Skill& skilldata = *GW::SkillbarMgr::GetSkillConstantData(skill.skill_id); 

    if (skill.GetRecharge() != 0) { return false; }

    return true;
}

void MultiboxUtils::CastSkill(uint32_t slot) {
    
    const auto tSelf = GW::Agents::GetPlayerAsAgentLiving();  
    if (!tSelf) { return; } 

    if (tSelf->GetIsCasting()) { return; }
    //if (tSelf->GetIsMoving()) { return; } 
  
    const auto skillbar = GW::SkillbarMgr::GetPlayerSkillbar();

    if (!skillbar || !skillbar->IsValid()) { return; }
    const GW::SkillbarSkill& skill = skillbar->skills[slot]; 

    if (skill.skill_id == GW::Constants::SkillID::No_Skill) {  return; }
    
    const GW::Skill& skilldata = *GW::SkillbarMgr::GetSkillConstantData(skill.skill_id); 
    
    if (skill.GetRecharge() != 0 ) {return;}

    const auto current_energy = static_cast<uint32_t>((tSelf->energy * tSelf->max_energy));
    if (current_energy < skilldata.energy_cost) { return; }

    const auto enough_adrenaline =
        (skilldata.adrenaline == 0) || (skilldata.adrenaline > 0 && skill.adrenaline_a >= skilldata.adrenaline);
    
    if (!enough_adrenaline) { return; }

    if ((!tSelf->GetIsCasting()) && (skill.GetRecharge() == 0) && (!CombatMachine.IsSkillCasting)){
        CombatMachine.IsSkillCasting = true;
        CombatMachine.LastActivity = clock();
        CombatMachine.IntervalSkillCasting = skilldata.activation + skilldata.aftercast;
        GW::SkillbarMgr::UseSkill(slot, GW::Agents::GetTargetId()); 
        const auto target = GW::Agents::GetTargetId(); 
        if (target) { 
            GW::Agents::InteractAgent(GW::Agents::GetAgentByID(target), false); 
        }
    }
  
}

bool MultiboxUtils::IsAllowedCast() {
    if (!CombatMachine.IsSkillCasting) { return true; }
    
    if ((clock() - CombatMachine.LastActivity) > CombatMachine.IntervalSkillCasting) {
        CombatMachine.IsSkillCasting = false;
        return true;
    }
    return false;
}

void MultiboxUtils::SmartSelectTarget() {
    const auto tSelf = GW::Agents::GetPlayerAsAgentLiving(); 
    const GW::PartyInfo* Party = GW::PartyMgr::GetPartyInfo(); 
    if ((!tSelf) || (!Party)) { return; } 
    if (!Party->players[0].connected()) { return; } 
    const auto tLeaderid = GW::Agents::GetAgentIdByLoginNumber(Party->players[0].login_number); 
    if (!tLeaderid) { return; } 
    const auto tLeader = GW::Agents::GetAgentByID(tLeaderid); 
    if (!tLeader) { return; }  

      
    if (Machine.IsTargettingEnabled) {
        const auto target = GW::Agents::GetTargetId();
        if (!target) { 
            GW::Agents::ChangeTarget(GW::Agents::GetAgentByID(TargetNearestEnemyInBigAggro())); 
            GW::Agents::InteractAgent(GW::Agents::GetAgentByID(TargetNearestEnemyInBigAggro()), false);  
        }
    }

}

bool MultiboxUtils::ThereIsSpaceInInventory()
{

    GW::Bag** bags = GW::Items::GetBagArray();
    if (!bags) {
        return false;
    }
    size_t end_bag = static_cast<size_t>(GW::Constants::Bag::Bag_2);


    for (size_t bag_idx = static_cast<size_t>(GW::Constants::Bag::Backpack); bag_idx <= end_bag; bag_idx++) {
        GW::Bag* bag = bags[bag_idx];
        if (!bag || !bag->items.valid()) {
            continue;
        }
        for (size_t slot = 0; slot < bag->items.size(); slot++) {
            if (!bag->items[slot]) {
                return true;
            }
        }
    }
    return false;
}

void MultiboxUtils::SignalTerminate() 
{
    GW::Chat::DeleteCommand(L"autoloot");
    GW::Chat::DeleteCommand(L"autofollow");
    GW::Chat::DeleteCommand(L"autoloot");
    GW::Chat::DeleteCommand(L"dropitem");
    GW::Chat::DeleteCommand(L"autotarget");
    GW::Chat::DeleteCommand(L"autocombat");
    GW::Chat::DeleteCommand(L"heroai");
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



void MultiboxUtils::Draw(IDirect3DDevice9*)
{
    auto DialogButton = [](const int x_idx, const int x_qty, const char* text, const char* help, const DWORD dialog) -> void {
        if (x_idx != 0) {
            ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
        }
        const float w = (ImGui::GetWindowWidth() - ImGui::GetStyle().ItemInnerSpacing.x * (x_qty - 1)) / x_qty;
        if (text != nullptr && ImGui::IsItemHovered()) {
            ImGui::SetTooltip(help);
        }
        };

    const auto& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), can_close && show_closebutton ? GetVisiblePtr() : nullptr, GetWinFlags(ImGuiWindowFlags_NoScrollbar))) {
        
        ImGui::BeginTable("maintable", 2);
        ImGui::TableNextRow(); 
        ImGui::TableSetColumnIndex(0);
        ImGui::Checkbox("Auto-Loot", &Machine.IsLootingEnabled);
        ImGui::TableSetColumnIndex(1);
        ImGui::Checkbox("Auto-Follow", &Machine.IsFollowingEnabled);
        ImGui::TableNextRow(); 
        ImGui::TableSetColumnIndex(0);
        ImGui::Checkbox("Auto-Target", &Machine.IsTargettingEnabled);
        ImGui::TableSetColumnIndex(1);
        ImGui::Checkbox("Auto-Combat", &Machine.IsCombatEnabled);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Checkbox("Scatter", &Machine.IsScatterEnabled);
        ImGui::EndTable(); 

        ImGui::Text("Allowed Skills");
        
        ImGui::BeginTable("table1", 8);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Checkbox("1", &CombatSkillState[0]);
        ImGui::TableSetColumnIndex(1);
        ImGui::Checkbox("2", &CombatSkillState[1]);
        ImGui::TableSetColumnIndex(2);
        ImGui::Checkbox("3", &CombatSkillState[2]);
        ImGui::TableSetColumnIndex(3);
        ImGui::Checkbox("4", &CombatSkillState[3]);
        ImGui::TableSetColumnIndex(4);
        ImGui::Checkbox("5", &CombatSkillState[4]);
        ImGui::TableSetColumnIndex(5);
        ImGui::Checkbox("6", &CombatSkillState[5]);
        ImGui::TableSetColumnIndex(6);
        ImGui::Checkbox("7", &CombatSkillState[6]);
        ImGui::TableSetColumnIndex(7);
        ImGui::Checkbox("8", &CombatSkillState[7]);

        ImGui::EndTable();

    }
    ImGui::End();
}

void MultiboxUtils::DrawSettings()
{
    ToolboxUIPlugin::DrawSettings();

}

/**************SKILL BEAHVIOUR DECLARATION ***********************************/

enum SkillNature { Offensive,OffensiveCaster, OffensiveMartial, Healing,Benefical, Neutral, SelfTargetted, Resurrection, Interrupt};
enum SkillTarget { Enemy, EmenyCaster, EnemyMartial,Ally, AllyCaster, AllyMartial,OtherAlly, Self,Corpse, Minion, Spirit, Pet };

struct castConditions {
    bool IsAlive = true;
    bool HasCondition = false;
    //bool HasDeepWound = false;
    //bool HasWeakness = false;
    //bool HasCrippled = false;
    bool HasEnchantment = false;
    bool HasHex = false;
    bool IsCasting = false;
    bool IsNockedDown = false;
    bool IsMoving = false;
    bool IsAttacking = false;
    bool IsHoldingItem = false;
    bool Target25Life = false;
    bool Target50Life = false;
    bool Target75Life = false;
    bool IsPartyWide = false;
    bool UniqueProperty = false;
    
};

struct CustomSkillData {
    GW::Constants::SkillID SkillID;
    GW::Constants::SkillType SkillType;
    SkillTarget  TargetAllegiance;
    SkillNature Nature = Offensive;
    castConditions Conditions;
};

CustomSkillData SpecialSkills[3030];

// Ally_NonAttackable = 0x1, Neutral = 0x2, Enemy = 0x3, Spirit_Pet = 0x4, Minion = 0x5, Npc_Minipet = 0x6

void InitSkillData() {
    //WARRIOR
    //STRENGTH
    int ptr = 0;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::I_Meant_to_Do_That;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Shout;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.IsNockedDown = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::I_Will_Survive;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Shout;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::You_Will_Die;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Shout;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.Target50Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Counterattack;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Attack;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.IsAttacking = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Disarm;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Attack; 
    SpecialSkills[ptr].TargetAllegiance = Enemy; 
    SpecialSkills[ptr].Nature = Interrupt; 
    SpecialSkills[ptr].Conditions.IsCasting = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Lions_Comfort;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Skill;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Protectors_Strike;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Attack;
    SpecialSkills[ptr].TargetAllegiance = Enemy; 
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.IsMoving = true;
    ptr++;   
    //AXE MASTERY
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Disrupting_Chop;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Attack;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Interrupt;
    SpecialSkills[ptr].Conditions.IsCasting = true;
    ptr++;
    //HAMMER MASTERY
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Belly_Smash;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Attack;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive; 
    SpecialSkills[ptr].Conditions.IsNockedDown = true; 
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Counter_Blow; 
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Attack; 
    SpecialSkills[ptr].TargetAllegiance = Enemy; 
    SpecialSkills[ptr].Nature = Offensive; 
    SpecialSkills[ptr].Conditions.IsAttacking = true; 
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Pulverizing_Smash;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Attack;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.IsNockedDown = true;
    ptr++;
    //SWORDMANSHIP
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Knee_Cutter;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Attack;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Savage_Slash;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Attack;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Interrupt;
    SpecialSkills[ptr].Conditions.IsCasting = true;
    ptr++;
    //TACTICS
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Healing_Signet;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Signet;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    //NO ATTRIBUTE
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Coward;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Shout;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.IsMoving = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::On_Your_Knees;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Shout;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.UniqueProperty = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Distracting_Blow;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Attack;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Interrupt;
    SpecialSkills[ptr].Conditions.IsCasting = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Distracting_Strike;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Attack;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Interrupt;
    SpecialSkills[ptr].Conditions.IsCasting = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Skull_Crack;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Attack;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Interrupt;
    SpecialSkills[ptr].Conditions.IsCasting = true;
    ptr++;
    //RANGER
    //EXPERTISE
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Distracting_Shot; 
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Attack; 
    SpecialSkills[ptr].TargetAllegiance = Enemy; 
    SpecialSkills[ptr].Nature = Interrupt; 
    SpecialSkills[ptr].Conditions.IsCasting = true;
    ptr++;
    //BEAST MASTERY
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Comfort_Animal;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Skill;
    SpecialSkills[ptr].TargetAllegiance = Pet;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Disrupting_Lunge;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::PetAttack;
    SpecialSkills[ptr].TargetAllegiance = Enemy; 
    SpecialSkills[ptr].Nature = Interrupt;
    SpecialSkills[ptr].Conditions.IsCasting= true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Heal_as_One;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Skill;
    SpecialSkills[ptr].TargetAllegiance = Pet;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    //MARKSMANSHIP
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Broad_Head_Arrow;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Attack;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Interrupt;
    SpecialSkills[ptr].Conditions.IsCasting = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Concussion_Shot;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Attack;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Interrupt;
    SpecialSkills[ptr].Conditions.IsCasting = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Disrupting_Shot;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Attack;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Interrupt;
    SpecialSkills[ptr].Conditions.IsCasting = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Punishing_Shot;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Attack;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Interrupt;
    SpecialSkills[ptr].Conditions.IsCasting = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Savage_Shot;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Attack;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Interrupt;
    SpecialSkills[ptr].Conditions.IsCasting = true;
    ptr++;
    //WILDERNESS SURVIVAL
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Scavengers_Focus;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Skill;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    ptr++;
    //NO ATTRIBUTE
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Magebane_Shot;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Attack;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Interrupt;
    SpecialSkills[ptr].Conditions.IsCasting = true;
    ptr++;
    //MONK
    //DIVINE FAVOR
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Blessed_Aura;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Blessed_Light;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Boon_Signet; 
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Signet; 
    SpecialSkills[ptr].TargetAllegiance = Ally; 
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Contemplation_of_Purity;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Skill;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Deny_Hexes;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.HasHex = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Divine_Boon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Divine_Healing;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    SpecialSkills[ptr].Conditions.IsPartyWide = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Divine_Intervention;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.Target25Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Divine_Spirit;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Healers_Boon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Heavens_Delight;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    SpecialSkills[ptr].Conditions.IsPartyWide = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Holy_Haste;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Peace_and_Harmony;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    SpecialSkills[ptr].Conditions.HasHex = true;
    SpecialSkills[ptr].Conditions.UniqueProperty = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Release_Enchantments;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    SpecialSkills[ptr].Conditions.IsPartyWide = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Scribes_Insight;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Signet_of_Devotion;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Signet;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Smiters_Boon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Spell_Breaker;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Spell_Shield;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Unyielding_Aura;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.UniqueProperty = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Watchful_Healing;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Watchful_Healing;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Withdraw_Hexes;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.HasHex = true;
    ptr++;
    //HEALING PRAYERS
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Cure_Hex;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.HasHex = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Dwaynas_Sorrow;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.Target25Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Ethereal_Light;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Gift_of_Health;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Glimmer_of_Light;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Heal_Area;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    SpecialSkills[ptr].Conditions.IsPartyWide = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Heal_Other;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly; 
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Heal_Party;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    SpecialSkills[ptr].Conditions.IsPartyWide = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Healers_Covenant;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Healing_Breeze;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Healing_Burst;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Healing_Hands;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Healing_Light;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Healing_Ribbon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Healing_Ring;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    SpecialSkills[ptr].Conditions.IsPartyWide = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Healing_Seed;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Healing_Touch;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Healing_Whisper;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Infuse_Health;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target25Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Jameis_Gaze;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Kareis_Healing_Circle;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    SpecialSkills[ptr].Conditions.IsPartyWide = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Light_of_Deliverance;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    SpecialSkills[ptr].Conditions.IsPartyWide = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Live_Vicariously;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Mending;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Orison_of_Healing;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Patient_Spirit;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Renew_Life;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Resurrection; 
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Restful_Breeze;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Restore_Life;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Resurrection;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Resurrection_Chant;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Resurrection;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Signet_of_Rejuvenation;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Signet;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Spotless_Mind;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.HasHex = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Spotless_Soul;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Supportive_Spirit;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Vigorous_Spirit;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Word_of_Healing;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target50Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Words_of_Comfort;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    //PROTECTION PRAYERS
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Air_of_Disenchantment;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Aura_of_Faith;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Aura_of_Stability;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Convert_Hexes;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.HasHex = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Dismiss_Condition;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Divert_Hexes;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.HasHex = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Draw_Conditions;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Guardian;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Life_Attunement;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Life_Barrier;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Life_Bond;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Life_Sheath;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Mark_of_Protection;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Mend_Ailment;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Mend_Condition;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Mending_Touch;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Pensive_Guardian;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Protective_Bond;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Protective_Spirit;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Purifying_Veil;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Rebirth;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Resurrection;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Restore_Condition;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Reversal_of_Fortune;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Reverse_Hex;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.HasHex = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Shield_of_Absorption;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Shield_of_Deflection;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Shield_of_Regeneration;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Shielding_Hands;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Spirit_Bond;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Vital_Blessing;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Zealous_Benediction;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.Target50Life = true;
    ptr++;
    //SMITING PRAYERS
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Balthazars_Aura;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.UniqueProperty = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Balthazars_Pendulum;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Offensive;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Balthazars_Spirit;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Holy_Wrath;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Offensive;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Judges_Insight;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Offensive;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Judges_Intervention;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Offensive;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Retribution;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Offensive;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Reversal_of_Damage;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Shield_of_Judgment;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Offensive;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Smite_Condition;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Smite_Hex;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.HasHex = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Strength_of_Honor;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Zealots_Fire;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Offensive;
    ptr++;
    //NO ATTRIBUTE
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Empathic_Removal;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    SpecialSkills[ptr].Conditions.HasHex = true;
    SpecialSkills[ptr].Conditions.UniqueProperty = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Essence_Bond;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Holy_Veil;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Light_of_Dwayna;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Resurrection;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Purge_Conditions;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Remove_Hex;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.HasHex = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Resurrect;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Resurrection;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Signet_of_Removal;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Signet;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    SpecialSkills[ptr].Conditions.HasHex = true;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    SpecialSkills[ptr].Conditions.UniqueProperty = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Succor;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Vengeance;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Resurrection;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
    //NECROMANCER
    //SOUL REAPING
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Foul_Feast;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Hexers_Vigor;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Masochism;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Signet_of_Lost_Souls;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Signet;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.Target50Life = false;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Soul_Taker;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    //BLOOD MAGIC
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Awaken_the_Blood;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self; 
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Blood_Renewal;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Blood_Ritual;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.UniqueProperty = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Blood_is_Power;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.UniqueProperty = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Contagion;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Corrupt_Enchantment;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Hex;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Cultists_Fervor;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Dark_Bond;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Dark_Fury; 
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment; 
    SpecialSkills[ptr].TargetAllegiance = AllyMartial;
    SpecialSkills[ptr].Nature = Benefical; 
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Demonic_Flesh;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Offensive;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Jaundiced_Gaze;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Mark_of_Subversion;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Hex;
    SpecialSkills[ptr].TargetAllegiance = EmenyCaster;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.UniqueProperty = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Order_of_Pain;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Order_of_the_Vampire;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Strip_Enchantment;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Vampiric_Spirit;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    ptr++;
    //CURSES
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Corrupt_Enchantment;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Hex;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Defile_Flesh;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Hex;
    SpecialSkills[ptr].TargetAllegiance = EmenyCaster;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.UniqueProperty = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Envenom_Enchantments;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Order_of_Apostasy;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Pain_of_Disenchantment;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Plague_Sending;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    SpecialSkills[ptr].Conditions.UniqueProperty = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Plague_Signet;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Signet;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    SpecialSkills[ptr].Conditions.UniqueProperty = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Plague_Touch;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    SpecialSkills[ptr].Conditions.UniqueProperty = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Rend_Enchantments;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Rip_Enchantment;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    ptr++;
    //DEATH MAGIC
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Animate_Bone_Fiend;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Corpse;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Animate_Bone_Horror;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Corpse;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Animate_Bone_Minions;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Corpse;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Animate_Flesh_Golem;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Corpse;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    SpecialSkills[ptr].Conditions.UniqueProperty = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Animate_Shambling_Horror;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Corpse;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Animate_Vampiric_Horror;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Corpse;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Aura_of_the_Lich;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Corpse;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Consume_Corpse;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Corpse;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Dark_Aura;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Death_Nova;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Minion;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.Target25Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Discord;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    SpecialSkills[ptr].Conditions.HasHex = true;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    SpecialSkills[ptr].Conditions.UniqueProperty = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Infuse_Condition;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Jagged_Bones;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Necrotic_Traversal;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Corpse;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Putrid_Explosion;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Corpse;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Soul_Feast;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Corpse;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Tainted_Flesh;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Taste_of_Pain;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.Target50Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Veratas_Aura;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Corpse;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Veratas_Gaze;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Corpse;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Vile_Miasma;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Hex;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Virulence;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Hex;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Withering_Aura;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = AllyMartial;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    //NO ATTRIBUTE
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Gaze_of_Contempt;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.Target50Life = true;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    ptr++;
    //MESMER
    //FAST CASTING
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Persistence_of_Memory;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Power_Return;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Interrupt;
    SpecialSkills[ptr].Conditions.IsCasting = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Psychic_Instability;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Interrupt;
    SpecialSkills[ptr].Conditions.IsCasting = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Symbolic_Celerity;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    //DOMINATION
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Complicate;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Interrupt;
    SpecialSkills[ptr].Conditions.IsCasting = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Cry_of_Frustration;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Interrupt;
    SpecialSkills[ptr].Conditions.IsCasting = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Hex_Eater_Vortex;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasHex = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Power_Flux;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Hex;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Interrupt;
    SpecialSkills[ptr].Conditions.IsCasting = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Power_Leak;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Interrupt;
    SpecialSkills[ptr].Conditions.IsCasting = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Power_Lock;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Interrupt;
    SpecialSkills[ptr].Conditions.IsCasting = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Power_Spike;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Interrupt;
    SpecialSkills[ptr].Conditions.IsCasting = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Psychic_Distraction;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Interrupt;
    SpecialSkills[ptr].Conditions.IsCasting = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Shatter_Delusions;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasHex = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Shatter_Enchantment;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Shatter_Hex;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasHex = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Signet_of_Disruption;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Signet;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Interrupt;
    SpecialSkills[ptr].Conditions.IsCasting = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Signet_of_Distraction;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Signet;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Interrupt;
    SpecialSkills[ptr].Conditions.IsCasting = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Simple_Thievery;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Interrupt;
    SpecialSkills[ptr].Conditions.IsCasting = true;
    ptr++;
    //ILUSSION MAGIC
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Air_of_Disenchantment;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Hex;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Illusion_of_Haste;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Illusionary_Weaponry;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Signet_of_Clumsiness;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Signet;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Interrupt;
    SpecialSkills[ptr].Conditions.IsCasting = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Sympathetic_Visage;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    //INSPIRATION MAGIC
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Auspicious_Incantation;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Channeling;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Discharge_Enchantment;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Drain_Delusions;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasHex = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Drain_Enchantment;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Ether_Signet;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Signet;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.UniqueProperty = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Feedback;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Hex_Eater_Signet;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Signet;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasHex = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Inspired_Enchantment;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Inspired_Hex;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasHex = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Leech_Signet;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Signet;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Interrupt;
    SpecialSkills[ptr].Conditions.IsCasting = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Mantra_of_Recall;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Power_Drain;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Interrupt;
    SpecialSkills[ptr].Conditions.IsCasting = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Power_Leech;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Interrupt;
    SpecialSkills[ptr].Conditions.IsCasting = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Revealed_Enchantment;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Revealed_Hex;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasHex = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Tease;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Interrupt;
    SpecialSkills[ptr].Conditions.IsCasting = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Waste_Not_Want_Not;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.UniqueProperty = true;
    ptr++;
    //NO ATTRIBUTE
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Arcane_Echo;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Arcane_Mimicry;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Echo;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Epidemic;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Expel_Hexes;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.HasHex = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Lyssas_Balance;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Mirror_of_Disenchantment;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Shatter_Storm;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Signet_of_Disenchantment;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Signet;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Web_of_Disruption;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Interrupt;
    SpecialSkills[ptr].Conditions.IsCasting = true;
    ptr++;
    //ELEMENTALIST
    //ENERGY STORAGE
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Aura_of_Restoration;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Elemental_Attunement;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Elemental_Attunement;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Ether_Prodigy;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Ether_Renewal;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Master_of_Magic;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Over_the_Limit;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    //AIR MAGIC
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Air_Attunement;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Conjure_Lightning;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Gust;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Lightning_Javelin;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Interrupt;
    SpecialSkills[ptr].Conditions.IsCasting = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Storm_Djinns_Haste;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Windborne_Speed;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    //EARTH MAGIC
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Armor_of_Earth;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Earth_Attunement;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Iron_Mist;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Kinetic_Armor;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Magnetic_Aura;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Magnetic_Surge;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Obsidian_Flesh;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Sliver_Armor;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Stone_Sheath;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Stone_Striker;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Stoneflesh_Aura;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    //FIRE MAGIC
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Burning_Speed;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Conjure_Flame;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Double_Dragon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Elemental_Flame;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Fire_Attunement;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Flame_Djinns_Haste;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    //WATER MAGIC
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Armor_of_Frost;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Armor_of_Mist;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Conjure_Frost;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Frigid_Armor;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Ice_Spear;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Mist_Form;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Slippery_Ground;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.UniqueProperty = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Water_Attunement;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    //NO ATTRIBUTE
    //NO SKILLS IN CATEGORY
    //ASSASSIN
    //CRITICAL STRIKES
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Assassins_Remedy;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Critical_Defenses;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Dark_Apostasy;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Deadly_Haste;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Locusts_Fury;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Way_of_the_Master;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Disrupting_Stab;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Attack;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Interrupt;
    SpecialSkills[ptr].Conditions.IsCasting = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Foxs_Promise;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    //DEADLY ARTS
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Expunge_Enchantments;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Skill;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Way_of_the_Empty_Palm;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    //SHADOW ARTS
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Deaths_Retreat;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.UniqueProperty = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Feigned_Neutrality;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Heart_of_Shadow;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Healing;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Shadow_Form;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Shadow_Refuge;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Shroud_of_Distress;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Way_of_Perfection;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Way_of_the_Fox;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Way_of_the_Lotus;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    //No Attribute
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Aura_of_Displacement;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Recall;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Shadow_Meld;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Signet_of_Malice;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Signet;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.UniqueProperty = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Signet_of_Twilight;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Signet;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.UniqueProperty = true;
    ptr++;
    //RITUALIST
    //SPAWNING POWER
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Boon_of_Creation;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Explosive_Growth;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Feast_of_Souls;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    SpecialSkills[ptr].Conditions.IsPartyWide = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Ghostly_Haste;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.UniqueProperty = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Renewing_Memories;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.IsHoldingItem = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Sight_Beyond_Sight;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Spirit_Channeling;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Spirit_to_Flesh;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Spirit;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    SpecialSkills[ptr].Conditions.IsPartyWide = true;
    SpecialSkills[ptr].Conditions.UniqueProperty = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Spirits_Gift;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Spirits_Strength;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Weapon_of_Renewal;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = AllyMartial;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Wielders_Remedy;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Wielders_Zeal;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    //CHANNELLING MAGIC
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Nightmare_Weapon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = AllyMartial;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Splinter_Weapon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = AllyMartial;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Wailing_Weapon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = AllyMartial;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Warmongers_Weapon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = AllyMartial;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Weapon_of_Aggression;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Weapon_of_Fury;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = AllyMartial;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    //COMMUNING
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Brutal_Weapon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = AllyMartial;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    SpecialSkills[ptr].Conditions.UniqueProperty = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Ghostly_Weapon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Guided_Weapon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = AllyMartial;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Sundering_Weapon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = AllyMartial;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Vital_Weapon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = AllyMartial;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Weapon_of_Quickening;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = AllyCaster;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    //RESTORATION MAGIC
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Death_Pact_Signet;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Signet;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Resurrection;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Flesh_of_My_Flesh;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Resurrection;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Ghostmirror_Light;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Mend_Body_and_Soul;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Mending_Grip;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Resilient_Weapon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = AllyCaster;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.HasHex = true;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    SpecialSkills[ptr].Conditions.UniqueProperty = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Soothing_Memories;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Spirit_Light;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Spirit_Light_Weapon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Spirit_Transfer;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Vengeful_Weapon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Weapon_of_Remedy;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Weapon_of_Shadow;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Weapon_of_Warding;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Wielders_Boon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Xinraes_Weapon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    //PARAGON
    //LEADERSHIP
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Lead_the_Way;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Shout;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Angelic_Bond;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Skill;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target25Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Angelic_Protection;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Shout;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Awe;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Skill;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.IsNockedDown = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Blazing_Finale;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::EchoRefrain;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Burning_Refrain;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::EchoRefrain;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Enduring_Harmony;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::EchoRefrain;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Glowing_Signet;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Signet;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    SpecialSkills[ptr].Conditions.UniqueProperty = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Hasty_Refrain;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::EchoRefrain;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Heroic_Refrain;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::EchoRefrain;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Leaders_Comfort;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Skill;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Signet_of_Return;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Signet;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Resurrection;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
    //COMMAND
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Brace_Yourself;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Shout;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Find_Their_Weakness;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Shout;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Make_Haste;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Shout;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::We_Shall_Return;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Shout;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Resurrection;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    SpecialSkills[ptr].Conditions.IsPartyWide = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Bladeturn_Refrain;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::EchoRefrain;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    //MOTIVATION
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Its_Just_a_Flesh_Wound;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Shout;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.HasCondition = false;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Energizing_Finale;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::EchoRefrain;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Finale_of_Restoration;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::EchoRefrain;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Inspirational_Speech;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Skill;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Mending_Refrain;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::EchoRefrain;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Purifying_Finale;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::EchoRefrain;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Signet_of_Synergy;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Signet;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    //SPEAR MASTERY
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Disrupting_Throw;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Attack;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Interrupt;
    SpecialSkills[ptr].Conditions.IsCasting = true;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    ptr++;
    //NO ATTRIBUTE
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Remedy_Signet;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Signet;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    ptr++;
    //DERVISH
    //MYSTICISM
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Arcane_Zeal;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Avatar_of_Balthazar;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Form;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Avatar_of_Dwayna;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Form;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Avatar_of_Grenth;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Form;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Avatar_of_Lyssa;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Form;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Avatar_of_Melandru;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Form;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Balthazars_Rage;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Eremites_Zeal;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Faithful_Intervention;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Heart_of_Holy_Flame;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Imbue_Health;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target50Life;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Intimidating_Aura;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Meditation;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Mystic_Corruption;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Mystic_Vigor;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Pious_Renewal;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Rending_Touch;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Vow_of_Revolution;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Vow_of_Silence;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Watchful_Intervention;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Benefical;
    SpecialSkills[ptr].Conditions.Target50Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Zealous_Renewal;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    //EARTH PRAYERS
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Armor_of_Sanctity;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Aura_of_Thorns;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Conviction;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Dust_Cloak;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Ebon_Dust_Aura;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Fleeting_Stability;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Mirage_Cloak;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Mystic_Regeneration;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Sand_Shards;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Shield_of_Force;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Signet_of_Pious_Light;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Signet;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Staggering_Force;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Veil_of_Thorns;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Vital_Boon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Vow_of_Strength;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    //SCYTHE MASTERY
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Lyssas_Assault;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Attack;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Interrupt;
    SpecialSkills[ptr].Conditions.IsCasting = true;
    ptr++;
    //WIND PRAYERS
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Attackers_Insight;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Featherfoot_Grace;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Grenths_Aura;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Grenths_Fingers;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Grenths_Grasp;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Guiding_Hands;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Harriers_Grasp;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Harriers_Haste;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Lyssas_Haste;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Mystic_Healing;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Natural_Healing;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Onslaught;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Pious_Restoration;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Rending_Aura;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Vow_of_Piety;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Whirling_Charge;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Zealous_Vow;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    //NO ATTRIBUTE
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Enchanted_Haste;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Resurrection_Signet;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Signet;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Resurrection;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Aura_of_Holy_Might_kurzick;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Aura_of_Holy_Might_luxon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Elemental_Lord_kurzick;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Elemental_Lord_luxon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Selfless_Spirit_kurzick;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Selfless_Spirit_luxon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Shadow_Sanctuary_kurzick;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Shadow_Sanctuary_luxon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Critical_Agility;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Cry_of_Pain;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Interrupt;
    SpecialSkills[ptr].Conditions.IsCasting = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Eternal_Aura;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Necrosis;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Skill;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    SpecialSkills[ptr].Conditions.HasHex = true;
    SpecialSkills[ptr].Conditions.UniqueProperty = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Seed_of_Life;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Sunspear_Rebirth_Signet;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Signet;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Resurrection;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Mental_Block;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Mindbender;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Benefical;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::By_Urals_Hammer;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Shout;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Resurrection;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Breath_of_the_Great_Dwarf;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.Target75Life = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Great_Dwarf_Armor;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Signet;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Resurrection;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
}

