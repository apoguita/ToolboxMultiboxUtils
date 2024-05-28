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


enum eState { Idle, Looting, Following, Combat };

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
    uint32_t castingptr;
    uint32_t currentCastingSlot;
    bool IsSkillCasting = false;
    float IntervalSkillCasting;
};

CombatStateMachine CombatMachine;

static GW::AgentID oldCalledTarget;
static GW::AgentID newCalledTarget;

float ScatterPos[8] = {0.0f, 45.0f, 0.0f, -45.0f, 90.0f, -90.0f, 135.0f, -135.0f };




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

    /* target monitor*/
    const auto targetChange = PartyTargetIDChanged(); 
    if ((Machine.IsTargettingEnabled) &&
        (targetChange != 0)) {
        GW::Agents::ChangeTarget(targetChange);
    }
    /* */

    if ((Machine.IsScatterEnabled) && (GetPartyNumber() >1)){
        const auto xx = tLeaderliving->x;
        const auto yy = tLeaderliving->y;
        const auto heropos = (GetPartyNumber() - 1) + GW::PartyMgr::GetPartyHeroCount() + GW::PartyMgr::GetPartyHenchmanCount();
        const auto angle = tLeaderliving->rotation_angle + DegToRad(ScatterPos[heropos]);
        const auto rotcos = cos(angle); 
        const auto rotsin = sin(angle);


        const auto posx = GW::Constants::Range::Adjacent * rotcos;
        const auto posy = GW::Constants::Range::Adjacent * rotsin;

        GW::Agents::Move(xx + posx, yy + posy);
        //GW::Agents::Move(xx + GW::Constants::Range::Adjacent,yy);
        Machine.IsScatterEnabled = false;
    }

    if ((tLeaderliving->GetIsAttacking()) ||
        (tLeaderliving->GetIsCasting())) {

        CombatMachine.StayAlert = clock();
    }

    if (Machine.State != Looting) {
        if ((tSelf->GetIsMoving()) || (tSelf->GetIsCasting())  || (tSelf->GetIsAttacking())) {
            Machine.State = Idle;
            Machine.LastActivity = clock();
        }
    }

    const auto dist = GW::GetDistance(tSelf->pos, tLeaderliving->pos);

    /******************COMBAT ROUTINES *********************************/

    if ((Machine.IsCombatEnabled) && (TargetNearestEnemyInAggro() != 0) && (!tSelf->GetIsMoving())) {

        if (((dist > GW::Constants::Range::Area) && (CombatMachine.State == cIdle))/* || (tSelf->GetIsMoving())*/) {
            Machine.State = Following;
            goto forcedwalk; //dirty trick to force following mid combat
        }


        switch (CombatMachine.State) {
        case cIdle:

            if (!CombatSkillState[CombatMachine.castingptr]) { CombatMachine.castingptr++; break; }
            if (!IsAllowedCast()) {
                break;
            }

            if (IsSkillready(CombatMachine.castingptr)) {

                CombatMachine.State = InCastingRoutine;
                CombatMachine.currentCastingSlot = CombatMachine.castingptr;
                SmartSelectTarget();
                CastSkill(CombatMachine.currentCastingSlot);

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
        if (CombatMachine.castingptr >= 8) { CombatMachine.castingptr = 0; }
    }
    else {
        CombatMachine.castingptr = 0;
    }
    forcedwalk:
    /**************************************************/

    

      switch (Machine.State) {
    case Idle:

        if ((Machine.IsFollowingEnabled) &&
            (dist > GW::Constants::Range::Area) &&
            (!tSelf->GetIsMoving()) &&
            (!tSelf->GetIsCasting()) 
            ) {
            Machine.State = Following;
            break;
        }

        if ((Machine.IsLootingEnabled) && 
            ((clock() - Machine.LastActivity) > 750) && 
            (!TargetNearestEnemyInAggro()) &&
            (ThereIsSpaceInInventory())
            ) {
            Machine.State = Looting;
            break;
        }
        break;
    case Looting:
        uint32_t tItem;

        while ((tItem = TargetNearestItem()) != 0) {
            if (ThereIsSpaceInInventory()) {
                GW::Agents::PickUpItem(GW::Agents::GetAgentByID(tItem), false);
            }
            return; //this return is in place instead of a real wait while looting routine
        }
        Machine.LastActivity = clock();
        Machine.State = Idle;
        
        break;
    case Following: 
        GW::Agents::InteractAgent(tLeader, false); 
        break;
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
        CombatMachine.IntervalSkillCasting = skilldata.activation + 750;
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

