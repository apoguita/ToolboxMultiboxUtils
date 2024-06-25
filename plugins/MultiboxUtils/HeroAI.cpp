#include "HeroAI.h"
#include <GWCA/Utilities/Hooker.h>

#include "Headers.h"

using namespace DirectX; //for legacy uncommented code at the end

GameStateClass GameState;

enum combatParams { unknown, skilladd, skillremove };
combatParams CombatTextstate;

static GW::AgentID oldCalledTarget;

float ScatterPos[8] = {0.0f, 45.0f, -45.0f, 90.0f, -90.0f, 135.0f, -135.0f, 180.0f };

bool OneTimeRun = false;
bool OneTimeRun2 = false;


DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static HeroAI instance;
    return &instance;
}

//MOUSE STUFF
bool HeroAI::WndProc(const UINT Message, const WPARAM wParam, const LPARAM lParam)
{

    switch (Message) {
        case WM_MOUSEMOVE:
            return OnMouseMove(Message, wParam, lParam);
        /*case WM_LBUTTONDOWN:
            return OnMouseDown(Message, wParam, lParam);
        case WM_MOUSEWHEEL:
            return OnMouseWheel(Message, wParam, lParam);
        case WM_LBUTTONDBLCLK:
            return OnMouseDblClick(Message, wParam, lParam);*/
        case WM_LBUTTONUP:
            return OnMouseUp(Message, wParam, lParam);
    default:
        return false;
    }
}



bool HeroAI::OnMouseMove(const UINT, const WPARAM, const LPARAM lParam)
{
    
    // Indicate that the message was handled, but we want to ensure default processing continues
    return false;
}


bool HeroAI::OnMouseUp(const UINT, const WPARAM, const LPARAM lParam)
{
   
    // Indicate that the message was handled, but we want to ensure default processing continues
    return false;
}


void wcs_tolower(wchar_t* s)
{
    for (size_t i = 0; s[i]; i++)
        s[i] = towlower(s[i]);
}

static void CmdCombat(const wchar_t*, const int argc, const LPWSTR* argv) {
    wcs_tolower(*argv);

    if (argc == 1) {
        GameState.state.toggleCombat();
        WriteChat(GW::Chat::CHANNEL_GWCA1, GameState.state.isCombatEnabled() ? L"CombatAI Enabled" : L"CombatAI Disabled", L"HeroAI");
        return; }

    if (argc == 2) { WriteChat(GW::Chat::CHANNEL_GWCA1, L"CombatAI: bad parameters, need at least 2", L"HeroAI"); return; }

    int cIndex = -1;
    const wchar_t first_char_of_first_arg = *argv[1];
    const wchar_t first_char_of_last_arg = *argv[argc - 1];

    CombatTextstate = unknown;
    if (first_char_of_first_arg == 'a') { CombatTextstate = skilladd; }
    if (first_char_of_first_arg == 'r') { CombatTextstate = skillremove; }

    switch (CombatTextstate) {
    case skillremove:
        if (first_char_of_last_arg >= '1' && first_char_of_last_arg <= '8') { cIndex = first_char_of_last_arg - '0'; }
        else if (first_char_of_last_arg == 'a') { cIndex = 0; }
        else { WriteChat(GW::Chat::CHANNEL_GWCA1, L"CombatAI: bad parameter, skill#", L"Hero AI"); break; }

        if (cIndex == 0) {
            std::fill(std::begin(GameState.CombatSkillState), std::end(GameState.CombatSkillState), false);
            WriteChat(GW::Chat::CHANNEL_GWCA1, L"CombatAI: skills 1-8 removed from combat loop", L"Hero AI");
        }
        else {
            if (cIndex >= 1 && cIndex <= 8) {
                GameState.CombatSkillState[cIndex - 1] = false;
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"CombatAI: skill removed from combat loop", L"Hero AI");
            }
            else { WriteChat(GW::Chat::CHANNEL_GWCA1, L"CombatAI: bad parameter, skill#", L"Hero AI"); }
        }
        break;
    case skilladd:

        if (first_char_of_last_arg >= '1' && first_char_of_last_arg <= '8') { cIndex = first_char_of_last_arg - '0'; }
        else if (first_char_of_last_arg == 'a') { cIndex = 0; }
        else { WriteChat(GW::Chat::CHANNEL_GWCA1, L"CombatAI: bad parameter, skill#", L"Hero AI"); break; }

        if (cIndex < 0) { WriteChat(GW::Chat::CHANNEL_GWCA1, L"CombatAI: bad parameter, skill#", L"Hero AI"); break; }

        if (cIndex == 0) {
            std::fill(std::begin(GameState.CombatSkillState), std::end(GameState.CombatSkillState), true);
            WriteChat(GW::Chat::CHANNEL_GWCA1, L"CombatAI: skills 1-8 added to combat loop", L"Hero AI");
        }
        else {
            if (cIndex >= 1 && cIndex <= 8) {
                GameState.CombatSkillState[cIndex - 1] = true;
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"CombatAI: skill added to combat loop", L"Hero AI");
            } else { WriteChat(GW::Chat::CHANNEL_GWCA1, L"CombatAI: bad parameter, skill#", L"Hero AI"); }
        }
        break;
    default:
        WriteChat(GW::Chat::CHANNEL_GWCA1, L"CombatAI: bad parameter, use add/remove", L"Hero AI");
    }
}

void HeroAI::Initialize(ImGuiContext* ctx, const ImGuiAllocFns fns, const HMODULE toolbox_dll)
{
    ToolboxPlugin::Initialize(ctx, fns, toolbox_dll);
    
    GW::Initialize();

    GW::Chat::CreateCommand(L"ailoot", [](const wchar_t*, int, const LPWSTR*) {
        GW::GameThread::Enqueue([] {
            GameState.state.toggleLooting();
            WriteChat(GW::Chat::CHANNEL_GWCA1, GameState.state.isLootingEnabled() ? L"Looting Enabled" : L"Looting Disabled", L"HeroAI");
            });
     });
     
    GW::Chat::CreateCommand(L"aifollow", [](const wchar_t*, int, const LPWSTR*) {
        GW::GameThread::Enqueue([] {
            GameState.state.toggleFollowing();
            WriteChat(GW::Chat::CHANNEL_GWCA1, GameState.state.isFollowingEnabled() ? L"Following Enabled" : L"Following Disabled", L"HeroAI");
        });
    });

    GW::Chat::CreateCommand(L"aitarget", [](const wchar_t*, int, const LPWSTR*) {
        GW::GameThread::Enqueue([] {
            GameState.state.toggleTargetting();
            WriteChat(GW::Chat::CHANNEL_GWCA1, GameState.state.isTargettingEnabled() ? L"AI targetting Enabled" : L"AI Targetting Disabled", L"HeroAI");
        });
    });

    GW::Chat::CreateCommand(L"aiscatter", [](const wchar_t*, int, const LPWSTR*) {
        GW::GameThread::Enqueue([] {
            GameState.state.toggleScatter();
            WriteChat(GW::Chat::CHANNEL_GWCA1, GameState.state.isScatterEnabled() ? L"AI Scatter Enabled" : L"AI Scatter Disabled", L"HeroAI");
        });
    });

    GW::Chat::CreateCommand(L"aihero", [](const wchar_t*, int, const LPWSTR*) {
        GW::GameThread::Enqueue([] {
            GameState.state.toggleFollowing();
            GameState.state.toggleLooting();
            GameState.state.toggleCombat(); 
            GameState.state.toggleTargetting();
            GameState.state.toggleScatter();

            for (int i = 0; i < 8; i++) { GameState.CombatSkillState[i] = !GameState.CombatSkillState[i]; }

            WriteChat(GW::Chat::CHANNEL_GWCA1, L"AI Hero Values toggled", L"Hero AI");
        
        });
    });

    GW::Chat::CreateCommand(L"aihelp", [](const wchar_t*, int, const LPWSTR*) {
        GW::GameThread::Enqueue([] {
            WriteChat(GW::Chat::CHANNEL_GWCA1,L"Available commands:", L"HeroAI");
            WriteChat(GW::Chat::CHANNEL_GWCA1, L"/aifollow - Auto follow", L"HeroAI");
            WriteChat(GW::Chat::CHANNEL_GWCA1, L"/ailoot - Auto loot", L"HeroAI");
            WriteChat(GW::Chat::CHANNEL_GWCA1, L"/aitarget - AI Target Selector", L"HeroAI");
            WriteChat(GW::Chat::CHANNEL_GWCA1, L"/aiscatter - Will preserve approriate distance against AoE", L"HeroAI");
            WriteChat(GW::Chat::CHANNEL_GWCA1, L"/aicombat - Activates Hero combat AI", L"HeroAI");
            WriteChat(GW::Chat::CHANNEL_GWCA1, L"/aicombat add [1-8,all] To add skill to combat queue", L"HeroAI");
            WriteChat(GW::Chat::CHANNEL_GWCA1, L"/aicombat remove [1-8,all] To remove skill from combat queue", L"HeroAI");
            WriteChat(GW::Chat::CHANNEL_GWCA1, L"/aihero - Toggles all options", L"HeroAI");
            });
        });
    
    GW::Chat::CreateCommand(L"aicombat", &CmdCombat);

    WriteChat(GW::Chat::CHANNEL_GWCA1, L"Use /heroai to toggle all features at once /aihelp fore more commands", L"Hero AI");

    MemMgr.InitializeSharedMemory(MemMgr.MemData);
    MemMgr.GetBasePointer();
   
    std::wstring file_name = L"skills.json";
    CustomSkillData.Init(file_name.c_str());
}



float DegToRad(float degree) {
    return (degree * 3.14159 / 180);
}

bool HeroAI::UpdateGameVars() {

    GameVars.Map.IsExplorable = (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) ? true : false;
    GameVars.Map.IsLoading = (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) ? true : false;
    GameVars.Map.IsOutpost = (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) ? true : false;
    GameVars.Map.IsPartyLoaded = (GW::PartyMgr::GetIsPartyLoaded()) ? true : false;

    
    if (!GameVars.Map.IsPartyLoaded) { return false; }

    GameVars.Target.Self.Living = GW::Agents::GetPlayerAsAgentLiving();
    if (!GameVars.Target.Self.Living) { return false; }
    GameVars.Target.Self.ID = GameVars.Target.Self.Living->agent_id;
    if (!GameVars.Target.Self.ID) { return false; }
    GameVars.Target.Self.Agent = GW::Agents::GetAgentByID(GameVars.Target.Self.ID);
    if (!GameVars.Target.Self.Agent) { return false; }

    GameVars.Party.PartyInfo = GW::PartyMgr::GetPartyInfo();
    if (!GameVars.Party.PartyInfo) { return false; }

    GameVars.Party.LeaderID = GW::Agents::GetAgentIdByLoginNumber(GameVars.Party.PartyInfo->players[0].login_number);
    if (!GameVars.Party.LeaderID) { return false; }

    GameVars.Party.Leader = GW::Agents::GetAgentByID(GameVars.Party.LeaderID);
    if (!GameVars.Party.Leader) { return false; }

    GameVars.Party.LeaderLiving = GameVars.Party.Leader->GetAsAgentLiving();
    if (!GameVars.Party.LeaderLiving) { return false; }

    GameVars.Party.SelfPartyNumber = GetPartyNumber();

    GameVars.Party.DistanceFromLeader = GW::GetDistance(GameVars.Target.Self.Living->pos, GameVars.Party.LeaderLiving->pos);

    if (!GameVars.Map.IsExplorable) { return false; } //moved for checks outside explorable

    GameVars.Target.Nearest.ID = TargetNearestEnemyInAggro();
    if (GameVars.Target.Nearest.ID) {
        GameVars.Target.Nearest.Agent = GW::Agents::GetAgentByID(GameVars.Target.Nearest.ID);
        GameVars.Target.Nearest.Living = GameVars.Target.Nearest.Agent->GetAsAgentLiving();
    }

    GameVars.Target.NearestCaster.ID = TargetNearestEnemyInAggro(false, EnemyCaster); 
    if (GameVars.Target.NearestCaster.ID) {
        GameVars.Target.NearestCaster.Agent = GW::Agents::GetAgentByID(GameVars.Target.NearestCaster.ID);
        GameVars.Target.NearestCaster.Living = GameVars.Target.NearestCaster.Agent->GetAsAgentLiving();
    }

    GameVars.Target.NearestMartial.ID = TargetNearestEnemyInAggro(false, EnemyMartial);
    if (GameVars.Target.NearestMartial.ID) {
        GameVars.Target.NearestMartial.Agent = GW::Agents::GetAgentByID(GameVars.Target.NearestMartial.ID);
        GameVars.Target.NearestMartial.Living = GameVars.Target.NearestMartial.Agent->GetAsAgentLiving();
    }

    GameVars.Party.InAggro = (GameVars.Target.Nearest.ID) ? true : false;

    GameVars.Target.Called.ID = GameVars.Party.PartyInfo->players[0].calledTargetId;
    if (GameVars.Target.Called.ID) {
        GameVars.Target.Called.Agent = GW::Agents::GetAgentByID(GameVars.Target.Called.ID);
        GameVars.Target.Called.Living = GameVars.Target.Called.Agent->GetAsAgentLiving();
    }

    GameVars.Target.LowestAlly.ID = TargetLowestAllyInAggro(false,false);
    if (GameVars.Target.LowestAlly.ID) {
        GameVars.Target.LowestAlly.Agent = GW::Agents::GetAgentByID(GameVars.Target.LowestAlly.ID);
        GameVars.Target.LowestAlly.Living = GameVars.Target.LowestAlly.Agent->GetAsAgentLiving();
    }

    GameVars.Target.LowestOtherAlly.ID = TargetLowestAllyInAggro(true,false);
    if (GameVars.Target.LowestOtherAlly.ID) {
        GameVars.Target.LowestOtherAlly.Agent = GW::Agents::GetAgentByID(GameVars.Target.LowestOtherAlly.ID);
        GameVars.Target.LowestOtherAlly.Living = GameVars.Target.LowestOtherAlly.Agent->GetAsAgentLiving();
    }

    GameVars.Target.LowestEnergyOtherAlly.ID = TargetLowestAllyInAggro(true, true);
    if (GameVars.Target.LowestEnergyOtherAlly.ID) {
        GameVars.Target.LowestEnergyOtherAlly.Agent = GW::Agents::GetAgentByID(GameVars.Target.LowestEnergyOtherAlly.ID);
        GameVars.Target.LowestEnergyOtherAlly.Living = GameVars.Target.LowestEnergyOtherAlly.Agent->GetAsAgentLiving();
    }

    GameVars.Target.LowestAllyCaster.ID = TargetLowestAllyInAggro(false, false,AllyCaster);
    if (GameVars.Target.LowestAllyCaster.ID) {
        GameVars.Target.LowestAllyCaster.Agent = GW::Agents::GetAgentByID(GameVars.Target.LowestAllyCaster.ID);
        GameVars.Target.LowestAllyCaster.Living = GameVars.Target.LowestAllyCaster.Agent->GetAsAgentLiving();
    }

    GameVars.Target.LowestAllyMartial.ID = TargetLowestAllyInAggro(false, false, AllyMartial);
    if (GameVars.Target.LowestAllyMartial.ID) {
        GameVars.Target.LowestAllyMartial.Agent = GW::Agents::GetAgentByID(GameVars.Target.LowestAllyMartial.ID);
        GameVars.Target.LowestAllyMartial.Living = GameVars.Target.LowestAllyMartial.Agent->GetAsAgentLiving();
    }

    GameVars.Target.DeadAlly.ID = TargetDeadAllyInAggro();
    if (GameVars.Target.DeadAlly.ID) {
        GameVars.Target.DeadAlly.Agent = GW::Agents::GetAgentByID(GameVars.Target.DeadAlly.ID);
        GameVars.Target.DeadAlly.Living = GameVars.Target.DeadAlly.Agent->GetAsAgentLiving();
    }

    GameVars.SkillBar.SkillBar = GW::SkillbarMgr::GetPlayerSkillbar();
    if (!GameVars.SkillBar.SkillBar || !GameVars.SkillBar.SkillBar->IsValid()) { return false; }

    for (int i = 0; i < 8; i++) {
        GameVars.SkillBar.Skills[i].Skill = GameVars.SkillBar.SkillBar->skills[i];
        GameVars.SkillBar.Skills[i].Data = *GW::SkillbarMgr::GetSkillConstantData(GameVars.SkillBar.Skills[i].Skill.skill_id);

        int Skillptr = 0;

        Skillptr = CustomSkillData.GetPtrBySkillID(GameVars.SkillBar.Skills[i].Skill.skill_id);

        if (Skillptr) {
            GameVars.SkillBar.Skills[i].CustomData = CustomSkillData.GetSkillByPtr(Skillptr);
            GameVars.SkillBar.Skills[i].HasCustomData = true;
        }
    }

    //interrupt/ remove ench/ REz / heal / cleanse Hex /cleanse condi / buff / benefical / Hex / Dmg / attack
    // skill priority order
    int ptr = 0;
    bool ptr_chk[8] = { false,false,false,false,false,false,false,false };
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.Nature == Interrupt))           { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true; ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.Nature == Enchantment_Removal)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true; ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.Nature == Resurrection))        { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true; ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.Nature == Healing))             { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true; ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.Nature == Hex_Removal))         { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true; ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.Nature == Condi_Cleanse))       { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true; ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.Nature == Buff))                { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true; ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.SkillType == GW::Constants::SkillType::Hex)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; }}
    for (int i = 0; i < 8; i++) { if  (!ptr_chk[i])   { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }


   return true;
}

void HeroAI::Update(float)
{
    if (!UpdateGameVars()) { return; };
    
    MemMgr.MemData->MemPlayers[GameVars.Party.SelfPartyNumber - 1].ID = GameVars.Target.Self.ID;
    MemMgr.MemData->MemPlayers[GameVars.Party.SelfPartyNumber - 1].Energy = GameVars.Target.Self.Living->energy;
    MemMgr.MemData->MemPlayers[GameVars.Party.SelfPartyNumber - 1].energyRegen = GameVars.Target.Self.Living->energy_regen;
    MemMgr.MemData->MemPlayers[GameVars.Party.SelfPartyNumber - 1].IsActive = true;

    for (int i = 0; i < 8; i++) {
        if (MemMgr.MemData->MemPlayers[i].IsActive) {
            MemMgr.MemCopy.MemPlayers[i].ID = MemMgr.MemData->MemPlayers[i].ID;
            MemMgr.MemCopy.MemPlayers[i].Energy = MemMgr.MemData->MemPlayers[i].Energy;
            MemMgr.MemCopy.MemPlayers[i].energyRegen = MemMgr.MemData->MemPlayers[i].energyRegen;
            MemMgr.MemCopy.MemPlayers[i].IsActive = MemMgr.MemData->MemPlayers[i].IsActive;
        }

    }

    //const auto DisableMouseWalking = 23U;
    //const auto pref = static_cast<GW::UI::FlagPreference>(DisableMouseWalking);
    

    //bool CanMouseWalk = GetPreference(pref); //true is unticked = can mouse walk
    //if (!CanMouseWalk) { SetPreference(pref, 1);}

    /****************** TARGERT MONITOR **********************/
    const auto targetChange = PartyTargetIDChanged();
    if ((GameState.state.isTargettingEnabled()) &&
        (targetChange != 0)) {
        GW::Agents::ChangeTarget(targetChange);
        GameState.combat.StayAlert = clock();
    }
    /****************** END TARGET MONITOR **********************/ 

    if (!GameVars.Party.InAggro) { GameState.combat.castingptr = 0; GameState.combat.State = cIdle; }
    // ENHANCE SCANNING AREA ON COMBAT ACTIVITY 
    if (GameVars.Party.LeaderLiving->GetIsAttacking() || GameVars.Party.LeaderLiving->GetIsCasting()) { GameState.combat.StayAlert = clock(); }

    if (GameVars.Target.Self.Living->GetIsMoving()) { return; }
    if (GameVars.Target.Self.Living->GetIsKnockedDown()) { return; }
    if (InCastingRoutine()) { return; }

    /**************** LOOTING *****************/
    uint32_t tItem;

    tItem = TargetNearestItem();
    if ((GameState.state.isLootingEnabled()) &&
        (!GameVars.Party.InAggro) &&
        (ThereIsSpaceInInventory()) &&
        (tItem != 0)
        ) {

        GW::Agents::PickUpItem(GW::Agents::GetAgentByID(tItem), false);
        return;
    }
    /**************** END LOOTING *************/

    /************ FOLLOWING *********************/

    if ((GameState.state.isFollowingEnabled()) &&
        (GameVars.Party.DistanceFromLeader > GW::Constants::Range::Area) &&
        (!GameVars.Target.Self.Living->GetIsCasting()) &&
        (GameVars.Party.SelfPartyNumber > 1)
        ) {
        
        const auto xx = GameVars.Party.LeaderLiving->x;
        const auto yy = GameVars.Party.LeaderLiving->y;
        const auto heropos = (GameVars.Party.SelfPartyNumber - 1) + GW::PartyMgr::GetPartyHeroCount() + GW::PartyMgr::GetPartyHenchmanCount();
        const auto angle = GameVars.Party.LeaderLiving->rotation_angle + DegToRad(ScatterPos[heropos]);
        const auto rotcos = cos(angle);
        const auto rotsin = sin(angle);

        GW::Agents::InteractAgent(GameVars.Party.Leader, false);
        //GW::Agents::Move(xx, yy);
        const auto posx = (GW::Constants::Range::Adjacent /* + (43.0f)*/) * rotcos;
        const auto posy = (GW::Constants::Range::Adjacent /* + (43.0f)*/) * rotsin;

        GW::Agents::Move(xx + posx, yy + posy);
        return;
    }
    /************ END FOLLOWING *********************/

    /*************** SCATTER ****************************/
    if ((GameState.state.isScatterEnabled()) &&
        (!GameVars.Target.Self.Living->GetIsCasting()) && 
        (GameVars.Party.SelfPartyNumber > 1) && 
        (!TargetNearestEnemyInAggro()) && 
        ((GameVars.Party.DistanceFromLeader <= (GW::Constants::Range::Touch + (43.0f))) || (AngleChange()))
        //Need to include checks for melee chars
        ) {
        //GW::Agents::InteractAgent(GameVars.Party.Leader, false);
        const auto xx = GameVars.Party.LeaderLiving->x;
        const auto yy = GameVars.Party.LeaderLiving->y;
        const auto heropos = (GameVars.Party.SelfPartyNumber - 1) + GW::PartyMgr::GetPartyHeroCount() + GW::PartyMgr::GetPartyHenchmanCount();
        
        static auto angle = GameVars.Party.LeaderLiving->rotation_angle + DegToRad(ScatterPos[7]);
        static auto rotcos = cos(angle);
        static auto rotsin = sin(angle);
        
        static auto posx = (GW::Constants::Range::Adjacent + (43.0f)) * rotcos;
        static auto posy = (GW::Constants::Range::Adjacent + (43.0f)) * rotsin;
        
        GW::Agents::Move(xx + posx, yy + posy);
         
        angle = GameVars.Party.LeaderLiving->rotation_angle + DegToRad(ScatterPos[heropos]);
        rotcos = cos(angle);
        rotsin = sin(angle);   
        
        posx = (GW::Constants::Range::Adjacent + (43.0f)) * rotcos;
        posy = (GW::Constants::Range::Adjacent + (43.0f)) * rotsin;

        GW::Agents::Move(xx + posx, yy + posy);
        return;
    }
    /***************** END SCATTER   ************************/

    /******************COMBAT ROUTINES *********************************/
    if ((GameState.state.isCombatEnabled()) && (GameVars.Party.InAggro)) {
        if (GameState.combat.castingptr >= 8) { GameState.combat.castingptr = 0; }
        if (GameVars.SkillBar.Skillptr >= 8) { GameVars.SkillBar.Skillptr = 0; }
        switch (GameState.combat.State) {
        case cIdle:
            if (GameState.combat.State != cInCastingRoutine) {
                if ((GameState.CombatSkillState[GameVars.SkillBar.SkillOrder[GameVars.SkillBar.Skillptr]]) &&
                    (IsSkillready(GameVars.SkillBar.SkillOrder[GameVars.SkillBar.Skillptr])) && 
                    (!InCastingRoutine()) &&
                    (IsReadyToCast(GameVars.SkillBar.SkillOrder[GameVars.SkillBar.Skillptr]))) 
                {
                    GameState.combat.State = cInCastingRoutine;
                    GameState.combat.currentCastingSlot = GameVars.SkillBar.SkillOrder[GameVars.SkillBar.Skillptr];
                    GameState.combat.LastActivity = clock();
                    GameState.combat.IntervalSkillCasting = 750.0f + GetPing();
                    GameState.combat.IsSkillCasting = true;

                    GW::AgentID vTarget = 0; 

                    vTarget = GetAppropiateTarget(GameState.combat.currentCastingSlot);
                    ChooseTarget(); 
                    GW::Agents::InteractAgent(GW::Agents::GetAgentByID((GameVars.Target.Called.ID) ? GameVars.Target.Called.ID : GameVars.Target.Nearest.ID), false); 
                    if (CastSkill(GameState.combat.currentCastingSlot)) { GameVars.SkillBar.Skillptr = 0; }
                    else { GameVars.SkillBar.Skillptr++; } 
                    break;
                }
                else { GameVars.SkillBar.Skillptr++; } 
             }
            break;
        case cInCastingRoutine:

            if (InCastingRoutine()) { break; }
            //CombatMachine.castingptr = 0;
            GameState.combat.State = cIdle;

            GameState.combat.castingptr++;

            break;
        default:
            GameState.combat.State = cIdle;
        }

    }

}

int HeroAI::GetPartyNumber() {
    for (int i = 0; i < GW::PartyMgr::GetPartySize(); i++)
    {
        const auto playerId = GW::Agents::GetAgentIdByLoginNumber(GameVars.Party.PartyInfo->players[i].login_number);
        if (playerId == GameVars.Target.Self.ID) { return i + 1; }
    }
    return 0;
}

float HeroAI::AngleChange() {
    const auto angle = GameVars.Party.LeaderLiving->rotation_angle;
    if (!angle) {
        oldAngle = 0.0f;
        return 0.0f;
    }

    if (oldAngle != angle) {
        oldAngle = angle;
        return oldAngle;
    }
    return 0.0f;
}

GW::AgentID HeroAI::PartyTargetIDChanged() {
    const auto target = GameVars.Party.PartyInfo->players[0].calledTargetId;
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

uint32_t  HeroAI::TargetNearestEnemyInAggro(bool IsBigAggro, SkillTarget Target) {
    const auto agents = GW::Agents::GetAgentArray();

    bool IsCaster = false;
    bool IsMartial = false;

    float distance = GW::Constants::Range::Earshot;
    float maxdistance = GW::Constants::Range::Earshot;
    if (((clock() - GameState.combat.StayAlert) <= 3000) || (IsBigAggro)){
        distance = GW::Constants::Range::Spellcast;
        maxdistance = GW::Constants::Range::Spellcast;
    }

    static GW::AgentID EnemyID;
    static GW::AgentID tStoredEnemyID = 0;

    for (const GW::Agent* agent : *agents)
    {
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

        switch (tEnemyLiving->weapon_type) {
        case wand: 
        case staff1:
        case staff2:
            IsCaster = true;
            IsMartial = false;
            break;
        default:
            IsCaster = false;
            IsMartial = true;
            break;
        }

        if ((Target == EnemyCaster) && (!IsCaster)) { continue; }
        if ((Target == EnemyMartial) && (!IsMartial)) { continue; }

        const auto dist = GW::GetDistance(GameVars.Target.Self.Living->pos, tempAgent->pos);

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

uint32_t  HeroAI::TargetLowestAllyInAggro(bool OtherAlly,bool CheckEnergy,SkillTarget Target) {
    const auto agents = GW::Agents::GetAgentArray();

    float distance = GW::Constants::Range::Spirit;
    static GW::AgentID AllyID;
    static GW::AgentID tStoredAllyID = 0;
    static GW::AgentID tStoredAllyenergyID = 0;
    float StoredLife = 2.0f;
    float StoredEnergy = 2.0f;

    bool IsCaster = false;
    bool IsMartial = false;

    for (const GW::Agent* agent : *agents)
    {
        if (!agent) { continue; }
        if (!agent->GetIsLivingType()) { continue; }
        const auto tAllyLiving = agent->GetAsAgentLiving();
        if (!tAllyLiving) { continue; } 
        if (tAllyLiving->allegiance != GW::Constants::Allegiance::Ally_NonAttackable) { continue; } 

        AllyID = tAllyLiving->agent_id;

        if (!AllyID) { continue; }

        const auto tempAgent = GW::Agents::GetAgentByID(AllyID);

        if (!tempAgent) { continue; }
        if (!tempAgent->GetAsAgentLiving()->GetIsAlive()) { continue; }
        
        const float vlife = tAllyLiving->hp;
        const float venergy = tAllyLiving->energy;

        if ((OtherAlly) && (AllyID == GameVars.Target.Self.ID)) { continue; }

        switch (tAllyLiving->weapon_type) {
            case wand:
            case staff1:
            case staff2:
                IsCaster = true;
                IsMartial = false;
                break;
            default:
                IsCaster = false;
                IsMartial = true;
                break;
        }

        if ((Target == AllyCaster) && (!IsCaster)) { continue; }
        if ((Target == AllyMartial) && (!IsMartial)) { continue; }

        if (vlife < StoredLife)
        {
            StoredLife = vlife;
            tStoredAllyID = AllyID;
        }
        if ((venergy < StoredEnergy) && (tAllyLiving->energy_regen < 0.03f))
        {
            StoredEnergy = venergy;
            tStoredAllyenergyID = AllyID;
        }
    }
    if ((!CheckEnergy) && (StoredLife != 2.0f)) { return tStoredAllyID; }
    if ((CheckEnergy) && (StoredEnergy != 2.0f)) { return tStoredAllyenergyID; }

    return 0;

}

uint32_t  HeroAI::TargetDeadAllyInAggro() {
    const auto agents = GW::Agents::GetAgentArray();

    float distance = GW::Constants::Range::Spirit;
    static GW::AgentID AllyID;
    static GW::AgentID tStoredAllyID = 0;
    float StoredLife = 2.0f;

    for (const GW::Agent* agent : *agents)
    {
        if (!agent) { continue; }
        if (!agent->GetIsLivingType()) { continue; }
        const auto tAllyLiving = agent->GetAsAgentLiving();
        if (!tAllyLiving) { continue; }
        if (tAllyLiving->allegiance != GW::Constants::Allegiance::Ally_NonAttackable) { continue; }

        AllyID = tAllyLiving->agent_id;

        if (!AllyID) { continue; }

        const auto tempAgent = GW::Agents::GetAgentByID(AllyID);

        if (!tempAgent) { continue; }
        if (!tempAgent->GetAsAgentLiving()->GetIsDead()) { continue; }

        return AllyID;
    }
    return 0;

}

uint32_t HeroAI::TargetNearestItem()
{
    const auto agents = GW::Agents::GetAgentArray();
    float distance = GW::Constants::Range::Spellcast;
    
    static GW::AgentID ItemID;
    static GW::AgentID tStoredItemID = 0;

    for (const GW::Agent* agent : *agents)
    {
        if (!agent) { continue; }
        if (!agent->GetIsItemType()) { continue; }

        const auto AgentItem = agent->GetAsAgentItem();

        if (!AgentItem) { continue; }
        
        uint32_t ItemID = AgentItem->agent_id;
        const auto tAssignedTo = AgentItem->owner;
        
        if (!ItemID) { continue; }
        if ((tAssignedTo) && (tAssignedTo != GameVars.Target.Self.ID)) { continue; }

        const auto tempAgent = GW::Agents::GetAgentByID(ItemID);

        if (!tempAgent) { continue; }

       
        const auto dist = GW::GetDistance(GameVars.Target.Self.Living->pos, tempAgent->pos);

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

bool HeroAI::IsSkillready(uint32_t slot) {
    if (GameVars.SkillBar.Skills[slot].Skill.skill_id == GW::Constants::SkillID::No_Skill) { return false; }
    if (GameVars.SkillBar.Skills[slot].Skill.GetRecharge() != 0) { return false; }
    return true;
}

GW::AgentID HeroAI::GetAppropiateTarget(uint32_t slot) {
    GW::AgentID vTarget = 0;
    /* --------- TARGET SELECTING PER SKILL --------------*/
    if (GameState.state.isTargettingEnabled()) {
        if (GameVars.SkillBar.Skills[slot].HasCustomData) {
            switch (GameVars.SkillBar.Skills[slot].CustomData.TargetAllegiance) {
            case Enemy:
                vTarget = GameVars.Target.Called.ID;
                if (!vTarget) { vTarget = GameVars.Target.Nearest.ID; }
                break;
            case EnemyCaster:
                vTarget = GameVars.Target.NearestCaster.ID;
                if (!vTarget) { vTarget = GameVars.Target.Nearest.ID; }
                break;
            case EnemyMartial:
                vTarget = GameVars.Target.NearestMartial.ID;
                if (!vTarget) { vTarget = GameVars.Target.Nearest.ID; }
                break;
            case Ally:
                vTarget = GameVars.Target.LowestAlly.ID;
                break;
            case AllyCaster:
                vTarget = GameVars.Target.LowestAllyCaster.ID;
                if (!vTarget) { vTarget = GameVars.Target.LowestAlly.ID; }
                break;
            case AllyMartial:
                vTarget = GameVars.Target.LowestAllyMartial.ID;
                if (!vTarget) { vTarget = GameVars.Target.LowestAlly.ID; }
                break;
            case OtherAlly:
                if ((GameVars.SkillBar.Skills[slot].HasCustomData) &&
                    (GameVars.SkillBar.Skills[slot].CustomData.Nature == EnergyBuff)) {
                    vTarget = GameVars.Target.LowestEnergyOtherAlly.ID;
                }
                else { vTarget = GameVars.Target.LowestOtherAlly.ID; }

                if (!vTarget) { vTarget = GameVars.Target.LowestAlly.ID; }
                break;
            case Self:
                vTarget = GameVars.Target.Self.ID;
                break;
            case DeadAlly:
                vTarget = GameVars.Target.DeadAlly.ID;
                break;
            default:
                vTarget = GameVars.Target.Called.ID;
                if (!vTarget) { vTarget = GameVars.Target.Nearest.ID; }
                break;
                //  enum SkillTarget { Enemy, EnemyCaster, EnemyMartial, Ally, AllyCaster, AllyMartial, OtherAlly,DeadAlly, Self, Corpse, Minion, Spirit, Pet };
            }
        }
        else {
            vTarget = GameVars.Target.Called.ID;
            if (!vTarget) { vTarget = GameVars.Target.Nearest.ID; }
        }
        return vTarget;
    }
    else
    {
        return GW::Agents::GetTargetId();
    }
}

bool HeroAI::IsReadyToCast(uint32_t slot) {
    if (GameVars.Target.Self.Living->GetIsCasting()) { GameState.combat.IsSkillCasting = false; return false; }
    if (GameVars.SkillBar.Skills[slot].Skill.skill_id == GW::Constants::SkillID::No_Skill) { GameState.combat.IsSkillCasting = false; return false; }
    if (GameVars.SkillBar.Skills[slot].Skill.GetRecharge() != 0) { GameState.combat.IsSkillCasting = false; return false; }

    const auto current_energy = static_cast<uint32_t>((GameVars.Target.Self.Living->energy * GameVars.Target.Self.Living->max_energy));
    if (current_energy < GameVars.SkillBar.Skills[slot].Data.energy_cost) { GameState.combat.IsSkillCasting = false; return false; }

    const auto enough_adrenaline =
        (GameVars.SkillBar.Skills[slot].Data.adrenaline == 0) ||
        (GameVars.SkillBar.Skills[slot].Data.adrenaline > 0 &&
            GameVars.SkillBar.Skills[slot].Skill.adrenaline_a >= GameVars.SkillBar.Skills[slot].Data.adrenaline);

    if (!enough_adrenaline) { GameState.combat.IsSkillCasting = false; return false; }

    GW::AgentID vTarget = GetAppropiateTarget(slot);

    if ((GameVars.SkillBar.Skills[slot].HasCustomData) && 
        (!AreCastConditionsMet(slot, vTarget))) { 
        GameState.combat.IsSkillCasting = false; return false;
    }
    if (doesSpiritBuffExists(GameVars.SkillBar.Skills[slot].Skill.skill_id)) { GameState.combat.IsSkillCasting = false; return false; }

    if (HasEffect(GameVars.SkillBar.Skills[slot].Skill.skill_id, GW::Agents::GetTargetId())) { GameState.combat.IsSkillCasting = false; return false; }

    if ((GameVars.Target.Self.Living->GetIsCasting()) && (GameVars.SkillBar.Skills[slot].Skill.GetRecharge() != 0)) { GameState.combat.IsSkillCasting = false; return false; }

    return true;
}

bool HeroAI::CastSkill(uint32_t slot) {
    //this function is legacy in case i need it later
    GW::AgentID vTarget = 0;
    
    vTarget = GetAppropiateTarget(slot); 
    if (IsReadyToCast(slot))
     {
        GameState.combat.IsSkillCasting = true;
        GameState.combat.LastActivity = clock();
        GameState.combat.IntervalSkillCasting = GameVars.SkillBar.Skills[slot].Data.activation + GameVars.SkillBar.Skills[slot].Data.aftercast + 750.0f;

        GW::SkillbarMgr::UseSkill(slot, vTarget);
        //GW::Agents::InteractAgent(GW::Agents::GetAgentByID((GameVars.Target.Called.ID) ? GameVars.Target.Called.ID: GameVars.Target.Nearest.ID), false); 
        return true;
    }
    return false;
}

bool HeroAI::InCastingRoutine() {
    return GameState.combat.IsSkillCasting && (clock() - GameState.combat.LastActivity) <= GameState.combat.IntervalSkillCasting;
}

void HeroAI::ChooseTarget() { 
    if (GameState.state.isTargettingEnabled()) {
        const auto target = GW::Agents::GetTargetId();
        if (!target) {
            if (GameVars.Target.Called.ID) {
                GW::Agents::ChangeTarget(GameVars.Target.Called.Agent); 
                GW::Agents::InteractAgent(GameVars.Target.Called.Agent, false);
            }
            else {
                GW::Agents::ChangeTarget(GameVars.Target.Nearest.Agent);
                GW::Agents::InteractAgent(GameVars.Target.Nearest.Agent, false);
            }
        }       
    }  
}

bool HeroAI::ThereIsSpaceInInventory()
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

void HeroAI::SignalTerminate() 
{
    //const auto DisableMouseWalking = 23U;
    //const auto pref = static_cast<GW::UI::FlagPreference>(DisableMouseWalking);

    //if (STORED_DISABLEMOUSEWALKING) { SetPreference(pref, 1); }
    //else { SetPreference(pref, 0); }


    MemMgr.MemData->MemPlayers[GameVars.Party.SelfPartyNumber - 1].IsActive = false;
    //UnmapViewOfFile(MemData);
    //CloseHandle(hMapFile);
    
    GW::Chat::DeleteCommand(L"autoloot");
    GW::Chat::DeleteCommand(L"autofollow");
    GW::Chat::DeleteCommand(L"autoloot");
    GW::Chat::DeleteCommand(L"dropitem");
    GW::Chat::DeleteCommand(L"autotarget");
    GW::Chat::DeleteCommand(L"autocombat");
    GW::Chat::DeleteCommand(L"heroai");
    GW::DisableHooks();
}

bool HeroAI::CanTerminate()
{
    return GW::HookBase::GetInHookCount() == 0;
}

void HeroAI::Terminate()
{
    ToolboxPlugin::Terminate();
    GW::Terminate();
}

bool ToggleButton(const char* str_id, bool* v, int width, int height, bool enabled)
{
    bool clicked = false;

    if (enabled)
    {
        if (*v)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.153f, 0.318f, 0.929f, 1.0f)); // Customize color when off 
            
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.6f, 0.6f, 1.0f)); // Customize active color 
            clicked = ImGui::Button(str_id, ImVec2(width, height)); // Customize size if needed 
            ImGui::PopStyleColor(2);
        }
        else
        {
            clicked = ImGui::Button(str_id, ImVec2(width, height)); // Customize size if needed        
        }
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0.9f, 0.9f, 0.3f)); // Customize active color 
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.9f, 0.9f, 0.3f)); // Customize hover color 
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.9f, 0.9f, 0.3f)); // Custom disabled color
        ImGui::Button(str_id, ImVec2(width, height)); // Custom size if needed
        ImGui::PopStyleColor(3);
    }

    if (clicked)
    {
        *v = !(*v);
    }

    return clicked;
}

void ShowColorPicker(ImVec4& color) {
    ImGui::ColorPicker4("Color Picker", (float*)&color); // ImGui takes float* for color
}

bool showSubWindow = true;

void HeroAI::Draw(IDirect3DDevice9*)
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
        
        ImGui::BeginTable("maintable", 3);
        ImGui::TableNextRow(); 
        ImGui::TableSetColumnIndex(0);
        if (ToggleButton("Loot", &GameState.state.Looting, 60, 20, true)) { /*do stuff*/ };
        ImGui::TableSetColumnIndex(1);
        if (ToggleButton("Follow", &GameState.state.Following, 60, 20, (GameVars.Party.SelfPartyNumber==1) ? false : true)) { /*do stuff*/ };
        ImGui::TableSetColumnIndex(2);
        if (ToggleButton("Combat", &GameState.state.Combat, 60, 20, true)) { /*do stuff*/ };
        ImGui::TableNextRow(); 
        ImGui::TableSetColumnIndex(0);
        if (ToggleButton("Target", &GameState.state.Targetting, 60, 20, true)) { /*do stuff*/ };
        ImGui::TableSetColumnIndex(1);
        if (ToggleButton("Scatter", &GameState.state.Scatter, 60, 20, (GameVars.Party.SelfPartyNumber==1) ? false : true)) { /*do stuff*/ };
        ImGui::TableSetColumnIndex(2);
        if (ToggleButton("Flagging", &GameState.state.HeroFlag, 60, 20, (GameVars.Party.SelfPartyNumber == 1) ? true : false)) { /*do stuff*/ };
        ImGui::EndTable(); 

        ImGui::Text("Allowed Skills");

        ImGui::BeginTable("skills", 8);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); 
            if (ToggleButton("1", &GameState.CombatSkillState[0], 20, 20, true)) { /*do stuff*/ }; ImGui::TableSetColumnIndex(1);
            if (ToggleButton("2", &GameState.CombatSkillState[1], 20, 20, true)) { /*do stuff*/ }; ImGui::TableSetColumnIndex(2);
            if (ToggleButton("3", &GameState.CombatSkillState[2], 20, 20, true)) { /*do stuff*/ }; ImGui::TableSetColumnIndex(3);
            if (ToggleButton("4", &GameState.CombatSkillState[3], 20, 20, true)) { /*do stuff*/ }; ImGui::TableSetColumnIndex(4);
            if (ToggleButton("5", &GameState.CombatSkillState[4], 20, 20, true)) { /*do stuff*/ }; ImGui::TableSetColumnIndex(5);
            if (ToggleButton("6", &GameState.CombatSkillState[5], 20, 20, true)) { /*do stuff*/ }; ImGui::TableSetColumnIndex(6);
            if (ToggleButton("7", &GameState.CombatSkillState[6], 20, 20, true)) { /*do stuff*/ }; ImGui::TableSetColumnIndex(7);
            if (ToggleButton("8", &GameState.CombatSkillState[7], 20, 20, true)) { /*do stuff*/ };
        
        ImGui::EndTable();
        //static ImVec4 color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Initial color (red)
        //ShowColorPicker(color);

        
        if (GameState.state.HeroFlag)
        {

            ImGui::BeginChild("HeroAI Flagging", ImVec2(100, 120), true);

            // Add content to the second window
            //ImGui::Text("This is the second window.");
            //ImGui::Checkbox("Checkbox in Second Window", &GameState.state.HeroFlag);
            ImGui::Text("Flagging");
            ImGui::BeginTable("Flags", 3);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
             if (ToggleButton("All", &GameVars.Party.FlaggingStatus[0], 20, 20, true)) { /*do stuff*/ }; ImGui::TableSetColumnIndex(1);
            if (ToggleButton("1", &GameVars.Party.FlaggingStatus[1], 20, 20, true)) { /*do stuff*/ }; ImGui::TableSetColumnIndex(2);
            if (ToggleButton("2", &GameVars.Party.FlaggingStatus[2], 20, 20, true)) { /*do stuff*/ }; ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0);
            if (ToggleButton("3", &GameVars.Party.FlaggingStatus[3], 20, 20, true)) { /*do stuff*/ }; ImGui::TableSetColumnIndex(1);
            if (ToggleButton("4", &GameVars.Party.FlaggingStatus[4], 20, 20, true)) { /*do stuff*/ }; ImGui::TableSetColumnIndex(2);
            if (ToggleButton("5", &GameVars.Party.FlaggingStatus[5], 20, 20, true)) { /*do stuff*/ }; ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0);
            if (ToggleButton("6", &GameVars.Party.FlaggingStatus[6], 20, 20, true)) { /*do stuff*/ }; ImGui::TableSetColumnIndex(1);
            if (ToggleButton("7", &GameVars.Party.FlaggingStatus[7], 20, 20, true)) { /*do stuff*/ }; ImGui::TableSetColumnIndex(2);
            if (ToggleButton("X", &GameVars.Party.FlaggingStatus[7], 20, 20, true)) { /*do stuff*/ };
        
            ImGui::EndTable();

            // End the second window
            ImGui::EndChild();

        }
        
    }
    ImGui::End();
}

void HeroAI::DrawSettings()
{
    ToolboxUIPlugin::DrawSettings();

}

bool HeroAI::doesSpiritBuffExists(GW::Constants::SkillID skillID) {
        const auto agents = GW::Agents::GetAgentArray(); 

        float distance = GW::Constants::Range::Spirit; 
        static GW::AgentID SpiritID;
        static GW::AgentID tStoredSpiritID = 0;

        for (const GW::Agent* agent : *agents)
        {
            if (!agent) { continue; }
            if (!agent->GetIsLivingType()) { continue; }
            const auto tSpiritLiving = agent->GetAsAgentLiving();
            if (!tSpiritLiving) { continue; }
            if (tSpiritLiving->allegiance != GW::Constants::Allegiance::Spirit_Pet) { continue; }

            SpiritID = tSpiritLiving->agent_id;
            if (!SpiritID) { continue; }

            if (!tSpiritLiving->IsNPC()) { continue; }
            if (!tSpiritLiving->GetIsAlive()) { continue; }

            const auto SpiritModelID = tSpiritLiving->player_number;

            if ((SpiritModelID == 2882) && (skillID == GW::Constants::SkillID::Frozen_Soil)) { return true; }
            if ((SpiritModelID == 4218) && (skillID == GW::Constants::SkillID::Life)) { return true; }
            if ((SpiritModelID == 4227) && (skillID == GW::Constants::SkillID::Bloodsong)) { return true; }
            if ((SpiritModelID == 4229) && (skillID == GW::Constants::SkillID::Signet_of_Spirits)) { return true; } //anger
            if ((SpiritModelID == 4230) && (skillID == GW::Constants::SkillID::Signet_of_Spirits)) { return true; } //hate
            if ((SpiritModelID == 4231) && (skillID == GW::Constants::SkillID::Signet_of_Spirits)) { return true; } //suffering
            if ((SpiritModelID == 5720) && (skillID == GW::Constants::SkillID::Anguish)) { return true; }
            if ((SpiritModelID == 4225) && (skillID == GW::Constants::SkillID::Disenchantment)) { return true; }
            if ((SpiritModelID == 4221) && (skillID == GW::Constants::SkillID::Dissonance)) { return true; }
            if ((SpiritModelID == 4214) && (skillID == GW::Constants::SkillID::Pain)) { return true; }
            if ((SpiritModelID == 4213) && (skillID == GW::Constants::SkillID::Shadowsong)) { return true; }
            if ((SpiritModelID == 4228) && (skillID == GW::Constants::SkillID::Wanderlust)) { return true; }
            if ((SpiritModelID == 5723) && (skillID == GW::Constants::SkillID::Vampirism)) { return true; }
            if ((SpiritModelID == 5854) && (skillID == GW::Constants::SkillID::Agony)) { return true; }
            if ((SpiritModelID == 4217) && (skillID == GW::Constants::SkillID::Displacement)) { return true; }
            if ((SpiritModelID == 4222) && (skillID == GW::Constants::SkillID::Earthbind)) { return true; }
            if ((SpiritModelID == 5721) && (skillID == GW::Constants::SkillID::Empowerment)) { return true; }
            if ((SpiritModelID == 4219) && (skillID == GW::Constants::SkillID::Preservation)) { return true; }
            if ((SpiritModelID == 5719) && (skillID == GW::Constants::SkillID::Recovery)) { return true; }
            if ((SpiritModelID == 4220) && (skillID == GW::Constants::SkillID::Recuperation)) { return true; }
            if ((SpiritModelID == 5853) && (skillID == GW::Constants::SkillID::Rejuvenation)) { return true; }
            if ((SpiritModelID == 4223) && (skillID == GW::Constants::SkillID::Shelter)) { return true; }
            if ((SpiritModelID == 4216) && (skillID == GW::Constants::SkillID::Soothing)) { return true; }
            if ((SpiritModelID == 4224) && (skillID == GW::Constants::SkillID::Union)) { return true; }
            if ((SpiritModelID == 4215) && (skillID == GW::Constants::SkillID::Destruction)) { return true; }
            if ((SpiritModelID == 4226) && (skillID == GW::Constants::SkillID::Restoration)) { return true; }
            if ((SpiritModelID == 2884) && (skillID == GW::Constants::SkillID::Winds)) { return true; }
            if ((SpiritModelID == 4239) && (skillID == GW::Constants::SkillID::Brambles)) { return true; }
            if ((SpiritModelID == 4237) && (skillID == GW::Constants::SkillID::Conflagration)) { return true; }
            if ((SpiritModelID == 2885) && (skillID == GW::Constants::SkillID::Energizing_Wind)) { return true; }
            if ((SpiritModelID == 4236) && (skillID == GW::Constants::SkillID::Equinox)) { return true; }
            if ((SpiritModelID == 2876) && (skillID == GW::Constants::SkillID::Edge_of_Extinction)) { return true; }
            if ((SpiritModelID == 4238) && (skillID == GW::Constants::SkillID::Famine)) { return true; }
            if ((SpiritModelID == 2883) && (skillID == GW::Constants::SkillID::Favorable_Winds)) { return true; }
            if ((SpiritModelID == 2878) && (skillID == GW::Constants::SkillID::Fertile_Season)) { return true; }
            if ((SpiritModelID == 2877) && (skillID == GW::Constants::SkillID::Greater_Conflagration)) { return true; }
            if ((SpiritModelID == 5715) && (skillID == GW::Constants::SkillID::Infuriating_Heat)) { return true; }
            if ((SpiritModelID == 4232) && (skillID == GW::Constants::SkillID::Lacerate)) { return true; }
            if ((SpiritModelID == 2888) && (skillID == GW::Constants::SkillID::Muddy_Terrain)) { return true; }
            if ((SpiritModelID == 2887) && (skillID == GW::Constants::SkillID::Natures_Renewal)) { return true; }
            if ((SpiritModelID == 4234) && (skillID == GW::Constants::SkillID::Pestilence)) { return true; }
            if ((SpiritModelID == 2881) && (skillID == GW::Constants::SkillID::Predatory_Season)) { return true; }
            if ((SpiritModelID == 2880) && (skillID == GW::Constants::SkillID::Primal_Echoes)) { return true; }
            if ((SpiritModelID == 2886) && (skillID == GW::Constants::SkillID::Quickening_Zephyr)) { return true; }
            if ((SpiritModelID == 5718) && (skillID == GW::Constants::SkillID::Quicksand)) { return true; }
            if ((SpiritModelID == 5717) && (skillID == GW::Constants::SkillID::Roaring_Winds)) { return true; }
            if ((SpiritModelID == 2879) && (skillID == GW::Constants::SkillID::Symbiosis)) { return true; }
            if ((SpiritModelID == 5716) && (skillID == GW::Constants::SkillID::Toxicity)) { return true; }
            if ((SpiritModelID == 4235) && (skillID == GW::Constants::SkillID::Tranquility)) { return true; }          
            if ((SpiritModelID == 2874) && (skillID == GW::Constants::SkillID::Winter)) { return true; }
            if ((SpiritModelID == 2875) && (skillID == GW::Constants::SkillID::Winnowing)) { return true; }
        }
        return false;
}

bool HeroAI::HasEffect(const GW::Constants::SkillID skillID, GW::AgentID agentID) 
{
    const auto* const agentEffects = GW::Effects::GetAgentEffectsArray(agentID);
    
    if (!agentEffects)
    {
        return false;
    }

    for (const auto& effect : agentEffects->effects)
    {
        const auto agent_id = effect.agent_id;
        const auto skill_id = effect.skill_id;

        if ((agent_id == agentID && skill_id == skillID) ||
           (agent_id == 0 && skill_id == skillID))
        {
                return true;
        }
    }  
    return false;
}

int HeroAI::GetMemPosByID(GW::AgentID agentID) {
    for (int i = 0; i < 8; i++) {
        if ((MemMgr.MemCopy.MemPlayers[i].IsActive) && (MemMgr.MemCopy.MemPlayers[i].ID == agentID)) {
            return i;
        }   
    }
    return -1;
}

bool HeroAI::AreCastConditionsMet(uint32_t slot, GW::AgentID agentID) {

    int Skillptr = 0; 
    int ptr = 0; 
    int NoOfFeatures = 0; 
    int featurecount = 0; 

    const auto tempAgent = GW::Agents::GetAgentByID(agentID); 
    if (!tempAgent) { return false; } 
    const auto tempAgentLiving = tempAgent->GetAsAgentLiving(); 
    if (!tempAgentLiving) { return false; } 

    Skillptr = CustomSkillData.GetPtrBySkillID(GameVars.SkillBar.Skills[slot].Skill.skill_id);
    if (!Skillptr) { return true; }

    if (GameVars.SkillBar.Skills[slot].CustomData.Nature == Resurrection) {
        if (tempAgentLiving->GetIsDead()) {
            //if (tempAgentLiving->allegiance == GW::Constants::Allegiance::Ally_NonAttackable) {
                return true;
            //}
        }
        else { return false; }
    }

    if (GameVars.SkillBar.Skills[slot].CustomData.Conditions.UniqueProperty) { 
        switch (GameVars.SkillBar.Skills[slot].Skill.skill_id) { 
        case GW::Constants::SkillID::Discord:
            if (tempAgentLiving->GetIsConditioned() && (tempAgentLiving->GetIsHexed() || tempAgentLiving->GetIsEnchanted()))
            { return true; }
            else { return false; }
            break;
        case GW::Constants::SkillID::Peace_and_Harmony: //same conditionsas below
        case GW::Constants::SkillID::Empathic_Removal:
        case GW::Constants::SkillID::Necrosis:
            if (tempAgentLiving->GetIsConditioned() || tempAgentLiving->GetIsHexed())
            { return true; }
            else { return false; }
            break;
        default:
            return true;
        } 
    }

    CustomSkillClass::CustomSkillDatatype vSkill = CustomSkillData.GetSkillByPtr(Skillptr);

    featurecount += (vSkill.Conditions.IsAlive) ? 1 : 0;
    featurecount += (vSkill.Conditions.HasCondition) ? 1 : 0;
    featurecount += (vSkill.Conditions.HasDeepWound) ? 1 : 0;
    featurecount += (vSkill.Conditions.HasCrippled) ? 1 : 0;
    featurecount += (vSkill.Conditions.HasBleeding) ? 1 : 0;
    featurecount += (vSkill.Conditions.HasPoison) ? 1 : 0;
    featurecount += (vSkill.Conditions.HasWeaponSpell) ? 1 : 0;
    featurecount += (vSkill.Conditions.HasEnchantment) ? 1 : 0;
    featurecount += (vSkill.Conditions.HasHex) ? 1 : 0;
    featurecount += (vSkill.Conditions.IsCasting) ? 1 : 0;
    featurecount += (vSkill.Conditions.IsNockedDown) ? 1 : 0;
    featurecount += (vSkill.Conditions.IsMoving) ? 1 : 0;
    featurecount += (vSkill.Conditions.IsAttacking) ? 1 : 0;
    featurecount += (vSkill.Conditions.IsHoldingItem) ? 1 : 0;
    featurecount += (vSkill.Conditions.LessLife > 0.0f) ? 1 : 0;
    featurecount += (vSkill.Conditions.MoreLife > 0.0f) ? 1 : 0;
    featurecount += (vSkill.Conditions.LessEnergy > 0.0f) ? 1 : 0; 

    CustomSkillClass::CustomSkillDatatype vSkillSB = GameVars.SkillBar.Skills[slot].CustomData;

    if (tempAgentLiving->GetIsAlive()       && vSkillSB.Conditions.IsAlive) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsConditioned() && vSkillSB.Conditions.HasCondition) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsDeepWounded() && vSkillSB.Conditions.HasDeepWound) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsCrippled()    && vSkillSB.Conditions.HasCrippled) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsBleeding()    && vSkillSB.Conditions.HasBleeding) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsPoisoned()    && vSkillSB.Conditions.HasPoison) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsWeaponSpelled() && vSkillSB.Conditions.HasWeaponSpell) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsEnchanted()   && vSkillSB.Conditions.HasEnchantment) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsHexed()       && vSkillSB.Conditions.HasHex) { NoOfFeatures++; }
    
    if (tempAgentLiving->GetIsKnockedDown() && vSkillSB.Conditions.IsNockedDown) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsMoving()      && vSkillSB.Conditions.IsMoving) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsAttacking()   && vSkillSB.Conditions.IsAttacking) { NoOfFeatures++; }
    if (tempAgentLiving->weapon_type == 0   && vSkillSB.Conditions.IsHoldingItem) { NoOfFeatures++; }

    if ((vSkillSB.Conditions.LessLife != 0.0f) && (tempAgentLiving->hp < vSkillSB.Conditions.LessLife)) { NoOfFeatures++; }
    if ((vSkillSB.Conditions.MoreLife != 0.0f) && (tempAgentLiving->hp > vSkillSB.Conditions.MoreLife)) { NoOfFeatures++; }
    
    
    //ENERGY CHECKS
    int PIndex = GetMemPosByID(agentID);
    if ((MemMgr.MemCopy.MemPlayers[PIndex].IsActive) &&
        (GameVars.SkillBar.Skills[slot].CustomData.Conditions.LessEnergy != 0.0f) &&
        (MemMgr.MemCopy.MemPlayers[PIndex].Energy < GameVars.SkillBar.Skills[slot].CustomData.Conditions.LessEnergy) &&
        (MemMgr.MemCopy.MemPlayers[PIndex].energyRegen < 0.03f)) {
        NoOfFeatures++; 
    }
    
    const auto TargetSkill = GW::SkillbarMgr::GetSkillConstantData(static_cast<GW::Constants::SkillID>(tempAgentLiving->skill));

    if (tempAgentLiving->GetIsCasting() && GameVars.SkillBar.Skills[slot].CustomData.Conditions.IsCasting && TargetSkill && TargetSkill->activation > 0.250f) { NoOfFeatures++; }

    if (featurecount == NoOfFeatures) { return true; }

    return false;
}

// ---------------------------------     end of useful stuff -------------------------------------------------------



// end NEW STUF

struct ScreenData {
    float width;
    float height;
};

ScreenData GetInjectedWindowSize() {
    //HWND hwnd = GetForegroundWindow(); // Get the handle of the current foreground window
    HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
    //HWND hwnd gw_window_handle = GW::MemoryMgr::GetGWWindowHandle();
    RECT rect;

    ScreenData screenSize = { 0, 0 };

    if (hwnd && GetClientRect(hwnd, &rect)) {
        screenSize.width = static_cast<float>(rect.right - rect.left);
        screenSize.height = static_cast<float>(rect.bottom - rect.top);
    }

    return screenSize;
}

void HeroAI::do_all_mouse_move()
{


    GW::Vec2f* screen_coord = 0;
    uintptr_t address = GW::Scanner::Find("\xD9\x46\x04\x8D\x45\xE4\x83\xC4\x10\x6A\x01\x83\xEC\x08", "xxxxxxxxxxxxxx", -93);

    if (address) {
        screen_coord = *reinterpret_cast<GW::Vec2f**>((uint8_t*)address + 1);
    }

    typedef float(__cdecl* ScreenToWorldPoint_pt)(GW::Vec3f* vec, float screen_x, float screen_y, int unk1);
    ScreenToWorldPoint_pt ScreenToWorldPoint_Func = 0;
    ScreenToWorldPoint_Func = (ScreenToWorldPoint_pt)GW::Scanner::Find("\xD9\x5D\x14\xD9\x45\x14\x83\xEC\x0C", "xxxxxxxx", -0x2F);

    typedef uint32_t(__cdecl* MapIntersect_pt)(GW::Vec3f* origin, GW::Vec3f* unit_direction, GW::Vec3f* hit_point, int* propLayer);
    MapIntersect_pt MapIntersect_Func = 0;
    uintptr_t mapIntersectAddress = GW::Scanner::Find("\xff\x75\x08\xd9\x5d\xfc\xff\x77\x7c", "xxxxxxxxx", -0x27);
    if (mapIntersectAddress) {
        MapIntersect_Func = (MapIntersect_pt)mapIntersectAddress;
    }

    GW::Vec3f origin;
    GW::Vec3f p2;
    float unk1 = ScreenToWorldPoint_Func(&origin, 0.5, 0.5/*screen_coord->x, screen_coord->y*/, 0);
    float unk2 = ScreenToWorldPoint_Func(&p2, 0.5, 0.5 /* screen_coord->x, screen_coord->y*/, 1);

    GW::Vec3f dir = p2 - origin;
    GW::Vec3f ray_dir = GW::Normalize(dir);
    GW::Vec3f hit_point;
    int layer = 0;
    uint32_t ret = MapIntersect_Func(&origin, &ray_dir, &hit_point, &layer); //needs to run in game thread

    if (ret) {
        GW::GamePos gp(hit_point.x, hit_point.y, layer);

        //printf("screen x: %.2f y: %.2f    Hit x: %.2f y: %.2f z: %.2f layer: %d\n", screen_coord->x, screen_coord->y, gp.x, gp.y, hit_point.z, gp.zplane);

        std::wstring xMouseString = std::to_wstring(screen_coord->x);
        std::wstring yMouseString = std::to_wstring(screen_coord->y);

        std::wstringstream xWorldString;
        xWorldString << std::fixed << std::setprecision(2) << gp.x;

        std::wstringstream yWorldString;
        yWorldString << std::fixed << std::setprecision(2) << gp.y;

        std::wstringstream zWorldString;
        zWorldString << std::fixed << std::setprecision(2) << hit_point.z;

        std::wstring coords = L"(" + xMouseString + L"," + yMouseString + L") - W(" + xWorldString.str() + L"," + yWorldString.str() + L"," + zWorldString.str() + L")";
        WriteChat(GW::Chat::CHANNEL_GWCA1, coords.c_str(), L"Gwtoolbot");
    }
    else {
        //printf("No map intersect\n");
        WriteChat(GW::Chat::CHANNEL_GWCA1, L"No map intersect", L"Gwtoolbot");
    }

}




//END MOUSE STUFF

