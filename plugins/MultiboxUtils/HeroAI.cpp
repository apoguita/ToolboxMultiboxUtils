#include "HeroAI.h"
#include <GWCA/Utilities/Hooker.h>

#include "Headers.h"

Timer synchTimer;
Timer ScatterRest;

GameStateClass GameState;


enum combatParams { unknown, skilladd, skillremove };
combatParams CombatTextstate;

static GW::AgentID oldCalledTarget;

float ScatterPos[8] = {0.0f, 45.0f, -45.0f, 90.0f, -90.0f, 135.0f, -135.0f, 180.0f };

GlobalMouseClass GlobalMouse;

OverlayClass Overlay;
ExperimentalFlagsClass Experimental;
float RangedRangeValue = GW::Constants::Range::Spellcast;
float MeleeRangeValue = GW::Constants::Range::Spellcast;
 

GW::Vec3f PlayerPos;
int CandidateIndex = -1;
int CandidateUniqueKey = -1;

ID3D11ShaderResourceView* myTexture = nullptr;
int myImageWidth = 0;
int myImageHeight = 0;

DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static HeroAI instance;
    return &instance;
}



bool HeroAI::IsHeroFlagged(int hero) {
    if (hero <= GW::PartyMgr::GetPartyHeroCount()) {
        if (hero == 0) {
            const GW::Vec3f& allflag = GW::GetGameContext()->world->all_flag;
            if (allflag.x != 0 && allflag.y != 0 && (!std::isinf(allflag.x) || !std::isinf(allflag.y))) {
                return true;
            }
        }
        else {
            const GW::HeroFlagArray& flags = GW::GetGameContext()->world->hero_flags;
            if (!flags.valid() || static_cast<uint32_t>(hero) > flags.size()) {
                return false;
            }

            const GW::HeroFlag& flag = flags[hero - 1];
            if (!std::isinf(flag.flag.x) || !std::isinf(flag.flag.y)) {
                return true;
            }
        }
    }
    else {
        MemMgr.MutexAquire();
        bool ret = ((MemMgr.MemData->MemPlayers[hero - GW::PartyMgr::GetPartyHeroCount()].IsFlagged) && (MemMgr.MemData->MemPlayers[hero - GW::PartyMgr::GetPartyHeroCount()].IsActive)) ? true : false;
        MemMgr.MutexRelease();
        return ret;

    }


    return false;

}


float DegToRad(float degree) {
    return (degree * 3.14159 / 180);
}

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
    const int x = GET_X_LPARAM(lParam);
    const int y = GET_Y_LPARAM(lParam);


    GlobalMouse.SetMouseCoords(x, y);

    return false;
}

enum eClickHandler { eHeroFlag };
struct ClickHandlerStruct {
    bool WaitForClick = false;
    eClickHandler msg;
    int data;
    bool IsPartyWide = false;

} ClickHandler;

bool HeroAI::OnMouseUp(const UINT, const WPARAM, const LPARAM lParam)
{
   
    const int x = GET_X_LPARAM(lParam);
    const int y = GET_Y_LPARAM(lParam);


    if (ClickHandler.WaitForClick) {
        GW::Vec3f wolrdPos = Overlay.ScreenToWorld();
        switch (ClickHandler.msg){
            case eHeroFlag:
                MemMgr.MutexAquire();
                MemMgr.MemData->MemPlayers[ClickHandler.data].IsFlagged = true;
                MemMgr.MemData->MemPlayers[ClickHandler.data].IsAllFlag = (ClickHandler.IsPartyWide) ? true : false;
                MemMgr.MemData->MemPlayers[ClickHandler.data].FlagPos.x = wolrdPos.x;
                MemMgr.MemData->MemPlayers[ClickHandler.data].FlagPos.y = wolrdPos.y;
                MemMgr.MemData->MemPlayers[ClickHandler.data].ForceCancel = true;

                MemMgr.MutexRelease();
             
                ClickHandler.WaitForClick = false;
                break;
            default:
                break;

        }
    }
    // Indicate that the message was handled, but we want to ensure default processing continues
    return false;
}


void wcs_tolower(wchar_t* s)
{
    for (size_t i = 0; s[i]; i++)
        s[i] = towlower(s[i]);
}

namespace {

    void CmdCombat(const wchar_t*, const int argc, const LPWSTR* argv) {
        wcs_tolower(*argv);

        if (argc == 1) {

            GameState.state.toggleCombat();
            WriteChat(GW::Chat::CHANNEL_GWCA1, GameState.state.isCombatEnabled() ? L"CombatAI Enabled" : L"CombatAI Disabled", L"HeroAI");
            return;
        }

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
                }
                else { WriteChat(GW::Chat::CHANNEL_GWCA1, L"CombatAI: bad parameter, skill#", L"Hero AI"); }
            }
            break;
        default:
            WriteChat(GW::Chat::CHANNEL_GWCA1, L"CombatAI: bad parameter, use add/remove", L"Hero AI");
        }
    }
}

void HeroAI::Initialize(ImGuiContext* ctx, const ImGuiAllocFns fns, const HMODULE toolbox_dll)
{
    ToolboxPlugin::Initialize(ctx, fns, toolbox_dll);
    
    GW::Initialize();

    /////////////////////////////////////////////

    MemMgr.InitializeSharedMemory(MemMgr.MemData);
    MemMgr.GetBasePtr();

    std::wstring file_name = L"skills.json";
    CustomSkillData.Init(file_name.c_str());

    synchTimer.start();
    GameState.combat.LastActivity.start();
    GameState.combat.IntervalSkillCasting = 0.0f;
    ScatterRest.start();

    UpdateGameVars();
    

    ///////////////////////////////////////////////


    GW::Chat::CreateCommand(L"ailoot", [](GW::HookStatus*, const wchar_t*, const int, const LPWSTR*) {
        GW::GameThread::Enqueue([] {
            GameState.state.toggleLooting();
            WriteChat(GW::Chat::CHANNEL_GWCA1, GameState.state.isLootingEnabled() ? L"Looting Enabled" : L"Looting Disabled", L"HeroAI");
            });
        });
     
    
    GW::Chat::CreateCommand(L"aifollow", [](GW::HookStatus*, const wchar_t*, const int, const LPWSTR*) {
        GW::GameThread::Enqueue([] {
            GameState.state.toggleFollowing();
            WriteChat(GW::Chat::CHANNEL_GWCA1, GameState.state.isFollowingEnabled() ? L"Following Enabled" : L"Following Disabled", L"HeroAI");
        });
    });
    
    GW::Chat::CreateCommand(L"aitarget", [](GW::HookStatus*, const wchar_t*, const int, const LPWSTR*) {
        GW::GameThread::Enqueue([] {
            GameState.state.toggleTargetting();
            WriteChat(GW::Chat::CHANNEL_GWCA1, GameState.state.isTargettingEnabled() ? L"AI targetting Enabled" : L"AI Targetting Disabled", L"HeroAI");
        });
    });

    GW::Chat::CreateCommand(L"aiscatter", [](GW::HookStatus*, const wchar_t*, const int, const LPWSTR*) {
        GW::GameThread::Enqueue([] {
            GameState.state.toggleScatter();
            WriteChat(GW::Chat::CHANNEL_GWCA1, GameState.state.isScatterEnabled() ? L"AI Scatter Enabled" : L"AI Scatter Disabled", L"HeroAI");
        });
    });

    GW::Chat::CreateCommand(L"aihero", [](GW::HookStatus*, const wchar_t*, const int, const LPWSTR*) {
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

    GW::Chat::CreateCommand(L"aihelp", [](GW::HookStatus*, const wchar_t*, const int, const LPWSTR*) {
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
    
    GW::Chat::CreateCommand(L"aicombat", [](GW::HookStatus*, const wchar_t*, const int, const LPWSTR*) {
        GW::GameThread::Enqueue([] {
            //combat commands implementation
            WriteChat(GW::Chat::CHANNEL_GWCA1, L"/aicombat - chat command not working (wip)", L"HeroAI");
            });
        });

    WriteChat(GW::Chat::CHANNEL_GWCA1, L"Use /aihero to toggle all features at once /aihelp fore more commands", L"Hero AI");

 
}



bool TickFirstTime = false;

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

    /// CANDIDATES

    if (GameVars.Map.IsOutpost)
    {
            Scandidates candidateSelf;

            candidateSelf.player_ID = GameVars.Target.Self.Living->player_number;
            candidateSelf.MapID = GW::Map::GetMapID();
            candidateSelf.MapRegion = GW::Map::GetRegion();
            candidateSelf.MapDistrict = GW::Map::GetDistrict();
            MemMgr.MutexAquire();
            CandidateUniqueKey = MemMgr.MemData->NewCandidates.Subscribe(candidateSelf);
            MemMgr.MemData->NewCandidates.DoKeepAlive(CandidateUniqueKey);
            MemMgr.MutexRelease();
    }

    //END CANDIDATES


    //Synch Remote Control
    MemMgr.MutexAquire();
        GameState.state.Following   = MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.Following;
        GameState.state.Combat      = MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.Combat;
        GameState.state.Targetting  = MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.Targetting;
        GameState.state.Scatter     = MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.Scatter;
        GameState.state.Looting     = MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.Looting;
    MemMgr.MutexRelease();
    MemMgr.MutexAquire();
        for (int i = 0; i < 8; i++) {
            GameState.CombatSkillState[i] = MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].CombatSkillState[i];
        }
    MemMgr.MutexRelease();
    MemMgr.MutexAquire();
        RangedRangeValue = MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.RangedRangeValue;
        MeleeRangeValue = MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.MeleeRangeValue;

        if (RangedRangeValue==0) {
            RangedRangeValue = GW::Constants::Range::Spellcast;
            MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.RangedRangeValue = RangedRangeValue;
        }
    MemMgr.MutexRelease();
    MemMgr.MutexAquire();
        if (MeleeRangeValue == 0) {
            MeleeRangeValue = GW::Constants::Range::Spellcast;
            MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.MeleeRangeValue = MeleeRangeValue;
        }

    MemMgr.MutexRelease();

    //KEEP ALIVE
    MemMgr.MutexAquire();
    if ((GameVars.Party.SelfPartyNumber == 1) && (synchTimer.hasElapsed(500))) {
        for (int i = 1; i < 8; i++) {
            if (MemMgr.MemData->MemPlayers[i].IsActive) {
                if (MemMgr.MemData->MemPlayers[i].Keepalivecounter == 0)
                {
                    MemMgr.MemData->MemPlayers[i].IsActive = false;
                }
                else
                {
                    MemMgr.MemData->MemPlayers[i].Keepalivecounter = 0;
                }
            }
        }
        synchTimer.reset();
    }
    MemMgr.MutexRelease();


    //SELF SUBSCRIBE
    MemMgr.MutexAquire();
    MemMgr.MemData->MemPlayers[GameVars.Party.SelfPartyNumber - 1].ID = GameVars.Target.Self.ID;
    MemMgr.MemData->MemPlayers[GameVars.Party.SelfPartyNumber - 1].Energy = GameVars.Target.Self.Living->energy;
    MemMgr.MemData->MemPlayers[GameVars.Party.SelfPartyNumber - 1].energyRegen = GameVars.Target.Self.Living->energy_regen;
    MemMgr.MemData->MemPlayers[GameVars.Party.SelfPartyNumber - 1].IsActive = true;
    MemMgr.MemData->MemPlayers[GameVars.Party.SelfPartyNumber - 1].Keepalivecounter += 1;
    if (MemMgr.MemData->MemPlayers[GameVars.Party.SelfPartyNumber - 1].Keepalivecounter > 10000) {
        MemMgr.MemData->MemPlayers[GameVars.Party.SelfPartyNumber - 1].Keepalivecounter = 1;
    };
    MemMgr.MutexRelease();


    //EXPLORABLE ONLY

    if (!GameVars.Map.IsExplorable) {
        UnflagAllHeroes();
        return false;
    } //moved for checks outside explorable
    GameVars.Party.DistanceFromLeader = GW::GetDistance(GameVars.Target.Self.Living->pos, GameVars.Party.LeaderLiving->pos);
    if (GameVars.Party.DistanceFromLeader >= GW::Constants::Range::Compass) { return false; }

   
    //FLAGS
    if (GameVars.Party.SelfPartyNumber == 1) {

        const GW::Vec3f& allflag = GW::GetGameContext()->world->all_flag;
        MemMgr.MutexAquire();
        if ((IsHeroFlagged(0)) && 
            (allflag.x) && 
            (allflag.x) &&
            (!MemMgr.MemData->MemPlayers[0].IsFlagged)
            ){
            MemMgr.MemData->MemPlayers[0].IsFlagged = true;
            MemMgr.MemData->MemPlayers[0].FlagPos.x = allflag.x;
            MemMgr.MemData->MemPlayers[0].FlagPos.y = allflag.y;
            MemMgr.MemData->MemPlayers[0].IsAllFlag = true;
            GameVars.Party.Flags[0] = true;
        }
        else {
            if (!MemMgr.MemData->MemPlayers[0].IsAllFlag) {
                MemMgr.MemData->MemPlayers[0].IsFlagged = false;
                MemMgr.MemData->MemPlayers[0].IsAllFlag = false;
                MemMgr.MemData->MemPlayers[0].FlagPos.x = 0;
                MemMgr.MemData->MemPlayers[0].FlagPos.y = 0;
                GameVars.Party.Flags[0] = false;
            }
        }
        MemMgr.MutexRelease();
        for (int i = 0; i < 8; i++) {
            GameVars.Party.Flags[i] = IsHeroFlagged(i);
        }

       
    }

    
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

    GameVars.Target.NearestMartialMelee.ID = TargetNearestEnemyInAggro(false, EnemyMartialMelee);
    if (GameVars.Target.NearestMartialMelee.ID) {
        GameVars.Target.NearestMartialMelee.Agent = GW::Agents::GetAgentByID(GameVars.Target.NearestMartialMelee.ID);
        GameVars.Target.NearestMartialMelee.Living = GameVars.Target.NearestMartialMelee.Agent->GetAsAgentLiving();
    }

    GameVars.Target.NearestMartialRanged.ID = TargetNearestEnemyInAggro(false, EnemyMartialRanged);
    if (GameVars.Target.NearestMartialRanged.ID) {
        GameVars.Target.NearestMartialRanged.Agent = GW::Agents::GetAgentByID(GameVars.Target.NearestMartialRanged.ID);
        GameVars.Target.NearestMartialRanged.Living = GameVars.Target.NearestMartialRanged.Agent->GetAsAgentLiving();
    }

    GameVars.Party.InAggro = (TargetNearestEnemyInAggro()) ? true : false;

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

    GameVars.Target.LowestAllyMartialMelee.ID = TargetLowestAllyInAggro(false, false, AllyMartialMelee);
    if (GameVars.Target.LowestAllyMartialMelee.ID) {
        GameVars.Target.LowestAllyMartialMelee.Agent = GW::Agents::GetAgentByID(GameVars.Target.LowestAllyMartialMelee.ID);
        GameVars.Target.LowestAllyMartialMelee.Living = GameVars.Target.LowestAllyMartialMelee.Agent->GetAsAgentLiving();
    }

    GameVars.Target.LowestAllyMartialRanged.ID = TargetLowestAllyInAggro(false, false, AllyMartialRanged);
    if (GameVars.Target.LowestAllyMartialRanged.ID) {
        GameVars.Target.LowestAllyMartialRanged.Agent = GW::Agents::GetAgentByID(GameVars.Target.LowestAllyMartialRanged.ID);
        GameVars.Target.LowestAllyMartialRanged.Living = GameVars.Target.LowestAllyMartialRanged.Agent->GetAsAgentLiving();
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

        if (Skillptr != 0) {
            GameVars.SkillBar.Skills[i].CustomData = CustomSkillData.GetSkillByPtr(Skillptr);
            GameVars.SkillBar.Skills[i].HasCustomData = true;
        }
        else { GameVars.SkillBar.Skills[i].HasCustomData = false; }
    }

    //interrupt/ remove ench/ REz / heal / cleanse Hex /cleanse condi / buff / benefical / Hex / Dmg / attack
    // skill priority order
    int ptr = 0;
    bool ptr_chk[8] = { false,false,false,false,false,false,false,false };
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.Nature == Interrupt))           { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true; ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.Nature == Enchantment_Removal)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true; ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.Nature == Healing))             { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true; ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.Nature == Hex_Removal))         { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true; ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.Nature == Condi_Cleanse))       { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true; ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.Nature == EnergyBuff))          { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true; ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.Nature == Buff))                { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true; ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.Nature == Resurrection))        { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true; ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.SkillType == GW::Constants::SkillType::Form)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.SkillType == GW::Constants::SkillType::Enchantment)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.SkillType == GW::Constants::SkillType::EchoRefrain)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.SkillType == GW::Constants::SkillType::WeaponSpell)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.SkillType == GW::Constants::SkillType::Chant)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.SkillType == GW::Constants::SkillType::Preparation)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.SkillType == GW::Constants::SkillType::Ritual)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.SkillType == GW::Constants::SkillType::Ward)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.SkillType == GW::Constants::SkillType::Hex)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.SkillType == GW::Constants::SkillType::Trap)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.SkillType == GW::Constants::SkillType::Stance)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.SkillType == GW::Constants::SkillType::Shout)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.SkillType == GW::Constants::SkillType::Glyph)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.SkillType == GW::Constants::SkillType::Signet)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }

    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].Data.type == GW::Constants::SkillType::Form)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].Data.type == GW::Constants::SkillType::Enchantment)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].Data.type == GW::Constants::SkillType::EchoRefrain)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].Data.type == GW::Constants::SkillType::WeaponSpell)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].Data.type == GW::Constants::SkillType::Chant)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].Data.type == GW::Constants::SkillType::Preparation)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].Data.type == GW::Constants::SkillType::Ritual)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].Data.type == GW::Constants::SkillType::Ward)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].Data.type == GW::Constants::SkillType::Hex)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; }}
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].Data.type == GW::Constants::SkillType::Trap)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].Data.type == GW::Constants::SkillType::Stance)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].Data.type == GW::Constants::SkillType::Shout)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].Data.type == GW::Constants::SkillType::Glyph)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].Data.type == GW::Constants::SkillType::Signet)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].Data.combo == 3))                          { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } } //dual attack
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].Data.combo == 2))                          { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } } //off-hand attack
    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].Data.combo == 1))                          { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } } //lead attack
    for (int i = 0; i < 8; i++) { if  (!ptr_chk[i])   { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true;  ptr++; } }

    
    if (GameState.combat.LastActivity.hasElapsed(GameState.combat.IntervalSkillCasting)) { GameState.combat.IsSkillCasting = false; }
    PlayerPos.x = GameVars.Target.Self.Living->x;
    PlayerPos.y = GameVars.Target.Self.Living->y;

   return true;
}

Timer loottimeout;
uint32_t timeoutitem = 0;
GW::GamePos timeoutitempos;
float timeoutdistance;
uint32_t timeoutagentID;
bool PartyfirstTime = false;



void HeroAI::Update(float)
{
   
    //force Party Flag
    if ((GameVars.Map.IsOutpost) && (GameVars.Map.IsPartyLoaded) && (!TickFirstTime))
    {
        GW::GameThread::Enqueue([] {
            GW::PartyMgr::Tick(true);
            });
        TickFirstTime = true;
    }

    if (GameVars.Map.IsLoading) {
        TickFirstTime = false;
        PartyfirstTime = false;
    }

    ///// REMOTE CONTROL
    MemMgr.MutexAquire();
    RemoteCommand rc = MemMgr.MemData->command[GameVars.Party.SelfPartyNumber - 1];
    MemMgr.MutexRelease();
    uint32_t npcID;
    GW::Agent* tempAgent;
    GW::AgentLiving* tempAgentLiving;


    switch (rc) {
        case rc_ForceFollow:
            GW::Agents::InteractAgent(GameVars.Party.Leader, false);
            break;
        case rc_Pcons:
            GW::Chat::SendChat('/', "pcons");
            break;
        case rc_Title:
            GW::Chat::SendChat('/', "title");
            WriteChat(GW::Chat::CHANNEL_GWCA1, L"/title", L"Hero AI");
            break;
        case rc_GetQuest:
            npcID = TargetNearestNpc();
            if (npcID) { 
                tempAgent = GW::Agents::GetAgentByID(npcID);
                if (tempAgent) {
                    tempAgentLiving = tempAgent->GetAsAgentLiving();
                    if (tempAgentLiving) {
                        GW::Agents::InteractAgent(tempAgentLiving, false);
                        GW::GameThread::Enqueue([] {
                            GW::Chat::SendChat('/', "dialog 0");
                           });
                    }
                    else { WriteChat(GW::Chat::CHANNEL_GWCA1, L"No Living", L"Hero AI"); }
                }
                else { WriteChat(GW::Chat::CHANNEL_GWCA1, L"No Agent", L"Hero AI"); }
            } else { WriteChat(GW::Chat::CHANNEL_GWCA1, L"No ID", L"Hero AI"); }
            break;
        case rc_Resign:
            GW::Chat::SendChat('/', "resign");
            WriteChat(GW::Chat::CHANNEL_GWCA1, L"/resign", L"Hero AI");
            break;
        case rc_Chest:
            GW::GameThread::Enqueue([] {
                GW::Items::OpenXunlaiWindow();
                });
            GW::Chat::SendChat('/', "chest");
            WriteChat(GW::Chat::CHANNEL_GWCA1, L"/chest", L"Hero AI");
            break;
        default:
            break;
    }
    MemMgr.MutexAquire();
    MemMgr.MemData->command[GameVars.Party.SelfPartyNumber - 1] = rc_None;
    MemMgr.MutexRelease();

    if (!UpdateGameVars()) { return; };

    
    /****************** TARGERT MONITOR **********************/
    const auto targetChange = PartyTargetIDChanged();
    if ((GameState.state.isTargettingEnabled()) &&
        (targetChange != 0)) {
        GW::Agents::ChangeTarget(targetChange);
        GameState.combat.StayAlert.reset();
    }
    /****************** END TARGET MONITOR **********************/ 



    if ((loottimeout.isRunning())) {
        double elapsed = loottimeout.getElapsedTime();
        std::wstring strElapsed =  std::to_wstring(elapsed);

        //if (timeoutagentID != TargetNearestItem()) {
        //    loottimeout.stop();
        //}
        //else 
        {
            if (elapsed > 2000) {
                if (GW::GetDistance(GameVars.Target.Self.Living->pos, timeoutitempos) >= timeoutdistance)
                {
                    GW::Agents::Move(GameVars.Party.LeaderLiving->pos.x, GameVars.Party.LeaderLiving->pos.y);
                    GameState.state.Looting = false;
                    loottimeout.stop();
                    WriteChat(GW::Chat::CHANNEL_GWCA1, L"Theres a problem Looting, Halting function", L"Gwtoolbot");
                }

            }
        }   
    }

    

    if (!GameVars.Party.InAggro) { GameState.combat.castingptr = 0; GameState.combat.State = cIdle; }
    // ENHANCE SCANNING AREA ON COMBAT ACTIVITY 
    if (GameVars.Party.LeaderLiving->GetIsAttacking() || GameVars.Party.LeaderLiving->GetIsCasting()) { GameState.combat.StayAlert.reset(); }
    if (GameVars.Target.Self.Living->GetIsMoving()) { return; }
    if (GameVars.Target.Self.Living->GetIsKnockedDown()) { return; }
    if (InCastingRoutine()) { return; }

    /**************** LOOTING *****************/
    uint32_t tItem;

    tItem = TargetNearestItem();
    if (tItem == 0) {
        loottimeout.stop();
        timeoutagentID = 0;
        timeoutdistance = 0;
    }

    if ((tItem != 0) && (timeoutagentID != tItem)) {
        loottimeout.stop();
        timeoutagentID = 0;
        timeoutdistance = 0;
    }

    if ((GameState.state.isLootingEnabled()) &&
        (!GameVars.Party.InAggro) &&
        (ThereIsSpaceInInventory()) &&
        (!loottimeout.isRunning()) &&
        (tItem != 0)
        ) {

        //loottimeout.start();
        timeoutagentID - tItem;
        GW::Agent* itemagent = GW::Agents::GetAgentByID(tItem);
        timeoutitempos = itemagent->pos;
        timeoutdistance = GW::GetDistance(GameVars.Target.Self.Living->pos, timeoutitempos);
        GW::Agents::InteractAgent(GW::Agents::GetAgentByID(tItem), false);
        //GW::Agents::PickUpItem(GW::Agents::GetAgentByID(tItem), false);
        return;
    }

    /**************** END LOOTING *************/


    struct FollowTarget {
        GW::GamePos Pos;
        float DistanceFromLeader;
        bool IsCasting;
        float angle;
    } TargetToFollow;

    const GW::Vec3f& allflag = GW::GetGameContext()->world->all_flag;

    bool FalseLeader = false;
    bool DirectFollow = false;

    TargetToFollow.Pos = GameVars.Party.LeaderLiving->pos;
    TargetToFollow.DistanceFromLeader = GameVars.Party.DistanceFromLeader;
    TargetToFollow.IsCasting = GameVars.Target.Self.Living->GetIsCasting();
    TargetToFollow.angle = GameVars.Party.LeaderLiving->rotation_angle;

    MemMgr.MutexAquire();
    if ((MemMgr.MemData->MemPlayers[0].IsActive) && (MemMgr.MemData->MemPlayers[0].IsFlagged) && (MemMgr.MemData->MemPlayers[0].FlagPos.x) && (MemMgr.MemData->MemPlayers[0].FlagPos.y)) {
        TargetToFollow.Pos = MemMgr.MemData->MemPlayers[0].FlagPos;
        TargetToFollow.DistanceFromLeader = GW::GetDistance(GameVars.Target.Self.Living->pos, TargetToFollow.Pos);
        TargetToFollow.IsCasting = false;
        TargetToFollow.angle = 0.0f;
        FalseLeader = true;
    }
    MemMgr.MutexRelease();
   
    MemMgr.MutexAquire();
    if ((MemMgr.MemData->MemPlayers[GameVars.Party.SelfPartyNumber - 1].IsFlagged) && (MemMgr.MemData->MemPlayers[GameVars.Party.SelfPartyNumber - 1].FlagPos.x != 0) && (MemMgr.MemData->MemPlayers[GameVars.Party.SelfPartyNumber - 1].FlagPos.y != 0)) {
        TargetToFollow.Pos = MemMgr.MemData->MemPlayers[GameVars.Party.SelfPartyNumber - 1].FlagPos;
        TargetToFollow.DistanceFromLeader = GW::GetDistance(GameVars.Target.Self.Living->pos, TargetToFollow.Pos);
        TargetToFollow.IsCasting = false;
        TargetToFollow.angle = 0.0f;
        FalseLeader = true;
        DirectFollow = true;
    }
    MemMgr.MutexRelease();

    float FollowDistanceOnCombat;
    if (IsMelee(GameVars.Target.Self.ID)) {
        FollowDistanceOnCombat = MeleeRangeValue;
    }
    else {
        FollowDistanceOnCombat = RangedRangeValue;
    }
    /************ FOLLOWING *********************/
    bool DistanceOutOfcombat = ((TargetToFollow.DistanceFromLeader > GW::Constants::Range::Area) && (!GameVars.Party.InAggro));
    bool DistanceInCombat = ((TargetToFollow.DistanceFromLeader > FollowDistanceOnCombat) && (GameVars.Party.InAggro));
    bool AngleChangeOutOfCombat = ((!GameVars.Party.InAggro) && (AngleChange()));
    
    if ((GameState.state.isFollowingEnabled()) &&
        (
          (DistanceOutOfcombat || DistanceInCombat || AngleChangeOutOfCombat) &&
          (!GameVars.Target.Self.Living->GetIsCasting()) &&
          (GameVars.Party.SelfPartyNumber > 1)
        )) {

        float xx = TargetToFollow.Pos.x;
        float yy = TargetToFollow.Pos.y;

        const auto heropos = (GameVars.Party.SelfPartyNumber - 1) + GW::PartyMgr::GetPartyHeroCount() + GW::PartyMgr::GetPartyHenchmanCount();
        const auto angle = TargetToFollow.angle + DegToRad(ScatterPos[heropos]);
        const auto rotcos = cos(angle);
        const auto rotsin = sin(angle);
        float posx = 0.0f;
        float posy = 0.0f;
        if (!DirectFollow) {
            posx = (GW::Constants::Range::Touch)*rotcos;
            posy = (GW::Constants::Range::Touch)*rotsin;
            xx = xx + posx;
            yy = yy + posy;
        }
        if (!FalseLeader) {
            GW::Agents::InteractAgent(GameVars.Party.Leader, false);
        }
        GW::Agents::Move(xx, yy);
              
        return;
    }
    /************ END FOLLOWING *********************/

    /*************** SCATTER ****************************/
    
    if ((GameState.state.isScatterEnabled()) &&
        (!GameVars.Target.Self.Living->GetIsCasting()) &&
        (GameVars.Party.SelfPartyNumber > 1) &&
        (GameVars.Party.InAggro) &&
        (!IsMelee(GameVars.Target.Self.ID)) &&
        (!FalseLeader) &&
        ((TargetToFollow.DistanceFromLeader <= (GW::Constants::Range::Adjacent))) &&
        (ScatterRest.getElapsedTime() > 500)
        //Need to include checks for melee chars
        ) {
        //GW::Agents::InteractAgent(GameVars.Party.Leader, false);
        const auto xx = TargetToFollow.Pos.x;
        const auto yy = TargetToFollow.Pos.y;
        const auto heropos = (GameVars.Party.SelfPartyNumber - 1) + GW::PartyMgr::GetPartyHeroCount() + GW::PartyMgr::GetPartyHenchmanCount();

        static auto angle = TargetToFollow.angle + DegToRad(ScatterPos[heropos]);
        static auto rotcos = cos(angle);
        static auto rotsin = sin(angle);

        static auto posx = (GW::Constants::Range::Nearby) * rotcos;
        static auto posy = (GW::Constants::Range::Nearby) * rotsin;

        GW::Agents::InteractAgent(GameVars.Party.Leader, false);
        GW::Agents::Move(xx + posx, yy + posy);
        ScatterRest.reset();
        return;
    }
    

    /***************** END SCATTER   ************************/

    /******************COMBAT ROUTINES *********************************/
    if ((GameState.state.isCombatEnabled())  && (GameVars.Party.InAggro)) {
        if (GameVars.SkillBar.Skillptr >= 8) { 
            GameVars.SkillBar.Skillptr = 0; 
            if (GameVars.Target.Self.Living->weapon_type != 0) {
                GW::Agents::InteractAgent(GW::Agents::GetAgentByID((GameVars.Target.Called.ID) ? GameVars.Target.Called.ID : GameVars.Target.Nearest.ID), false);
            }
        }
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
                    GameState.combat.LastActivity.reset();
                    GameState.combat.IntervalSkillCasting = 750.0f + GetPing();
                    GameState.combat.IsSkillCasting = true;
                
                    if (GameVars.Target.Self.Living->weapon_type != 0) {
                        GW::Agents::InteractAgent(GW::Agents::GetAgentByID((GameVars.Target.Called.ID) ? GameVars.Target.Called.ID : GameVars.Target.Nearest.ID), false);
                    }
                    if (CastSkill(GameState.combat.currentCastingSlot)) { GameVars.SkillBar.Skillptr = 0; ChooseTarget();}
                    else {
                        if (GameVars.Target.Self.Living->weapon_type != 0) {
                            GW::Agents::InteractAgent(GW::Agents::GetAgentByID((GameVars.Target.Called.ID) ? GameVars.Target.Called.ID : GameVars.Target.Nearest.ID), false);
                        }
                        GameVars.SkillBar.Skillptr++;
                    }
                    break;
                }
                else { GameVars.SkillBar.Skillptr++; 
                GW::Agents::ChangeTarget(GW::Agents::GetAgentByID((GameVars.Target.Called.ID) ? GameVars.Target.Called.ID : GameVars.Target.Nearest.ID));
                }
             }
            break;
        case cInCastingRoutine:

            if (InCastingRoutine()) { break; }
            GameState.combat.State = cIdle;

            if (GameVars.Target.Self.Living->weapon_type != 0) {
                GW::Agents::InteractAgent(GW::Agents::GetAgentByID((GameVars.Target.Called.ID) ? GameVars.Target.Called.ID : GameVars.Target.Nearest.ID), false);
            }
            break;
        default:
            GameState.combat.State = cIdle;
        }

    }
    /******************COMBAT ROUTINES *********************************/
    /****************** OUT OF COMBAT ROUTINES *********************************/
    if ((GameState.state.isCombatEnabled()) && (!GameVars.Party.InAggro)) {
        if (GameVars.SkillBar.Skillptr >= 8) {
            GameVars.SkillBar.Skillptr = 0;
            GW::Agents::ChangeTarget(GW::Agents::GetAgentByID((GameVars.Target.Called.ID) ? GameVars.Target.Called.ID : GameVars.Target.Nearest.ID));
        }
        switch (GameState.combat.State) {
        case cIdle:
            if (GameState.combat.State != cInCastingRoutine) {
                    if ((GameState.CombatSkillState[GameVars.SkillBar.SkillOrder[GameVars.SkillBar.Skillptr]]) &&
                        (IsSkillready(GameVars.SkillBar.SkillOrder[GameVars.SkillBar.Skillptr])) &&
                        (!InCastingRoutine()) &&
                        (IsReadyToCast(GameVars.SkillBar.SkillOrder[GameVars.SkillBar.Skillptr])) &&
                        (IsOOCSkill(GameVars.SkillBar.SkillOrder[GameVars.SkillBar.Skillptr]))
                       )
                    {
                        GameState.combat.State = cInCastingRoutine;
                        GameState.combat.currentCastingSlot = GameVars.SkillBar.SkillOrder[GameVars.SkillBar.Skillptr];
                        GameState.combat.LastActivity.reset();
                        GameState.combat.IntervalSkillCasting = 750.0f + GetPing();
                        GameState.combat.IsSkillCasting = true;

                        if (CastSkill(GameState.combat.currentCastingSlot)) 
                        { GameVars.SkillBar.Skillptr = 0; ChooseTarget(); }
                        else {
                            GameVars.SkillBar.Skillptr++;
                            GW::Agents::ChangeTarget(GW::Agents::GetAgentByID((GameVars.Target.Called.ID) ? GameVars.Target.Called.ID : GameVars.Target.Nearest.ID));
                        }
                        break;
                    }
                    else {
                        GameVars.SkillBar.Skillptr++;
                        GW::Agents::ChangeTarget(GW::Agents::GetAgentByID((GameVars.Target.Called.ID) ? GameVars.Target.Called.ID : GameVars.Target.Nearest.ID));
                    }
            }
            break;
        case cInCastingRoutine:

            if (InCastingRoutine()) { break; }
            GameState.combat.State = cIdle;

            GW::Agents::ChangeTarget(GW::Agents::GetAgentByID((GameVars.Target.Called.ID) ? GameVars.Target.Called.ID : GameVars.Target.Nearest.ID));
            break;
        default:
            GameState.combat.State = cIdle;
        }

    }
    /****************** OUT OF COMBAT ROUTINES *********************************/



}

//hero stuff
enum FlaggingState : uint32_t {
    FlagState_All = 0,
    FlagState_Hero1,
    FlagState_Hero2,
    FlagState_Hero3,
    FlagState_Hero4,
    FlagState_Hero5,
    FlagState_Hero6,
    FlagState_Hero7,
    FlagState_None
};

enum CaptureMouseClickType : uint32_t {
    CaptureType_None [[maybe_unused]] = 0,
    CaptureType_FlagHero [[maybe_unused]] = 1,
    CaptureType_SalvageWithUpgrades [[maybe_unused]] = 11,
    CaptureType_SalvageMaterials [[maybe_unused]] = 12,
    CaptureType_Idenfify [[maybe_unused]] = 13
};

struct MouseClickCaptureData {
    struct sub1 {
        uint8_t pad0[0x3C];

        struct sub2 {
            uint8_t pad1[0x14];

            struct sub3 {
                uint8_t pad2[0x24];

                struct sub4 {
                    uint8_t pad3[0x2C];

                    struct sub5 {
                        uint8_t pad4[0x4];
                        FlaggingState* flagging_hero;
                    } *sub5;
                } *sub4;
            } *sub3;
        } *sub2;
    } *sub1;
}*MouseClickCaptureDataPtr = nullptr;

uint32_t* GameCursorState = nullptr;
CaptureMouseClickType* CaptureMouseClickTypePtr = nullptr;

FlaggingState GetFlaggingState()
{
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) {
        return FlagState_None;
    }
    if (!CaptureMouseClickTypePtr || *CaptureMouseClickTypePtr != CaptureType_FlagHero || !MouseClickCaptureDataPtr || !MouseClickCaptureDataPtr->sub1) {
        return FlagState_None;
    }
    return *MouseClickCaptureDataPtr->sub1->sub2->sub3->sub4->sub5->flagging_hero;
}

bool SetFlaggingState(FlaggingState set_state)
{
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) {
        return false;
    }
    // keep an internal flag for the minimap flagging until StartCaptureMouseClick_Func is working
    // minimap_flagging_state = set_state;
    if (GetFlaggingState() == set_state) { return true; }
    if (set_state == FlagState_None) {
        set_state = GetFlaggingState();
    }
    GW::UI::ControlAction key;
    switch (set_state) {
    case FlagState_Hero1:
        key = GW::UI::ControlAction_CommandHero1;
        break;
    case FlagState_Hero2:
        key = GW::UI::ControlAction_CommandHero2;
        break;
    case FlagState_Hero3:
        key = GW::UI::ControlAction_CommandHero3;
        break;
    case FlagState_Hero4:
        key = GW::UI::ControlAction_CommandHero4;
        break;
    case FlagState_Hero5:
        key = GW::UI::ControlAction_CommandHero5;
        break;
    case FlagState_Hero6:
        key = GW::UI::ControlAction_CommandHero6;
        break;
    case FlagState_Hero7:
        key = GW::UI::ControlAction_CommandHero7;
        break;
    case FlagState_All:
        key = GW::UI::ControlAction_CommandParty;
        break;
    default:
        return false;
    }
    return Keypress(key);
}

bool FlagHero(uint32_t idx)
{
    if (idx > FlagState_None) {
        return false;
    }
    return SetFlaggingState(static_cast<FlaggingState>(idx));
}

void HeroAI::Flag(int hero) {

    if ((hero <= GW::PartyMgr::GetPartyHeroCount()) && (GW::PartyMgr::GetPartyHeroCount() > 0)){
        FlagHero(hero);
        GameVars.Party.Flags[hero] = true;
    }
    else {
        if (GW::PartyMgr::GetPartyHeroCount() == 0) {
            ClickHandler.IsPartyWide = true;
        }
        ClickHandler.WaitForClick = true;
        ClickHandler.msg = eHeroFlag;
        ClickHandler.data = hero - GW::PartyMgr::GetPartyHeroCount();
    }
}

void HeroAI::UnflagAllHeroes() {
     
    GW::PartyMgr::UnflagAll();
    MemMgr.MutexAquire();
    MemMgr.MemData->MemPlayers[0].FlagPos.x = GameVars.Party.LeaderLiving->x;
    MemMgr.MemData->MemPlayers[0].FlagPos.y = GameVars.Party.LeaderLiving->y;
    GameVars.Party.Flags[0] = false;
    MemMgr.MutexRelease();
    for (int i = 1; i <= GW::PartyMgr::GetPartyHeroCount(); i++) {
        GW::PartyMgr::UnflagHero(i);
    }
     

     for (int i = 0; i < GW::PartyMgr::GetPartySize(); i++)
     {
         MemMgr.MutexAquire();
         MemMgr.MemData->MemPlayers[i].IsFlagged = false;
         MemMgr.MemData->MemPlayers[i].IsAllFlag = false;

         MemMgr.MemData->MemPlayers[i].FlagPos.x = 0;
         MemMgr.MemData->MemPlayers[i].FlagPos.y = 0;
         MemMgr.MutexRelease();
     }
}



bool ToggleButton(const char* str_id, bool* v, int width, int height, bool enabled)
{
    bool clicked = false;

    if (enabled)
    {
        if (*v)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.153f, 0.318f, 0.929f, 1.0f)); // Customize color when off 
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.9f, 1.0f)); // Customize hover color 
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.6f, 0.6f, 1.0f)); // Customize active color 
            clicked = ImGui::Button(str_id, ImVec2(width, height)); // Customize size if needed 
            ImGui::PopStyleColor(3);
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

bool showSubWindow = true;

struct NamedValue {
    float value;
    const char* name;
};

/*constexpr float Touch = 144.f;
constexpr float Adjacent = 166.0f;
constexpr float Nearby = 252.0f;
constexpr float Area = 322.0f;
constexpr float Earshot = 1012.0f;
constexpr float Spellcast = 1248.0f;
constexpr float Spirit = 2500.0f;
constexpr float Compass = 5000.0f;*/


std::vector<NamedValue> namedValues = {
    { 322.0f, "Area" },
    { 322.0f + 322.0f, "Area x 2" },
    { 1012.0f, "Earshot" },
    { 1248.0f, "Spellcast" },
    { 1248.0f + 322.0f , "Spellcast + Area" },
    { 2500.0f , "Spirit" },
    { 2500.0f + 1248.0f, "Spirit + Spellcast" },
    { 5000.0f , "Compass" },

};


int MeleeAutonomy=3;
int RangedAutonomy=3;

bool SelectableComboValues(const char* label, int* currentIndex, std::vector<NamedValue>& values)
{
    bool changed = false;

    // Prepare items for Combo box
    std::vector<std::string> items;
    for (const auto& value : values)
    {
        std::stringstream ss;
        ss << value.name << " (" << std::fixed << std::setprecision(2) << value.value << ")";
        items.push_back(ss.str());
    }

    std::vector<const char*> items_cstr;
    for (const auto& item : items)
    {
        items_cstr.push_back(item.c_str());
    }

    // Display label and Combo box
    ImGui::Text("%s", label);
    ImGui::SameLine();
    if (ImGui::Combo((std::string("##") + label).c_str(), currentIndex, items_cstr.data(), static_cast<int>(items_cstr.size())))
    {
        changed = true;
    }

    return changed;
}

void DrawYesNoText(const char* label, bool condition) {
    if (condition) {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255)); // Green color
        ImGui::Text("%s: Yes", label);
        ImGui::PopStyleColor();
    }
    else {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255)); // Red color
        ImGui::Text("%s: No", label);
        ImGui::PopStyleColor();
    }
}

void DrawEnergyValue(const char* label, float energy, float threshold) {
    if (energy < threshold) {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255)); // Red color
        ImGui::Text("%s: %.2f", label, energy);
        ImGui::PopStyleColor();
    }
    else {
        ImGui::Text("%s: %.2f", label, energy);
    }
}

void ShowTooltip(const char* tooltipText)
{
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("%s", tooltipText);
        ImGui::EndTooltip();
    }
}

char* WCharToChar(const wchar_t* wstr)
{
    if (!wstr) return nullptr;

    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
    char* strTo = new char[size_needed];
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, strTo, size_needed, NULL, NULL);
    return strTo;
}

std::string WStringToString(const std::wstring& s)
{
    // @Cleanup: ASSERT used incorrectly here; value passed could be from anywhere!
    if (s.empty()) {
        return "Error In Name";
    }
    // NB: GW uses code page 0 (CP_ACP)
    const auto size_needed = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    PLUGIN_ASSERT(size_needed != 0);
    std::string strTo(size_needed, 0);
    PLUGIN_ASSERT(WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), strTo.data(), size_needed, NULL, NULL));
    return strTo;
}


ImVec2 mainWindowPos;
ImVec2 mainWindowSize;

GameStateClass ControlAll;



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

    if (ImGui::Begin(Name(), can_close && show_closebutton ? GetVisiblePtr() : nullptr, GetWinFlags(ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize))) {
        ImGui::BeginTable("controltable", 6);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        MemMgr.MutexAquire();
        if (ToggleButton(ICON_FA_RUNNING "", &GameState.state.Following, 30, 30, (GameVars.Party.SelfPartyNumber == 1) ? false : true)) {
            MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.Following = !MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.Following;
        };
        MemMgr.MutexRelease();
        ShowTooltip("Follow");
        ImGui::TableSetColumnIndex(1);
        MemMgr.MutexAquire();
        if (ToggleButton(ICON_FA_PODCAST"", &GameState.state.Scatter, 30, 30, (GameVars.Party.SelfPartyNumber == 1) ? false : true)) {
            MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.Scatter = !MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.Scatter;;
        };
        MemMgr.MutexRelease();
        ShowTooltip("Scatter");
        ImGui::TableSetColumnIndex(2);
        MemMgr.MutexAquire();
        if (ToggleButton(ICON_FA_COINS"", &GameState.state.Looting, 30, 30, true)) {
            MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.Looting = !MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.Looting;
        };
        MemMgr.MutexRelease();
        ShowTooltip("Loot");
        ImGui::TableSetColumnIndex(3);
        MemMgr.MutexAquire();
        if (ToggleButton(ICON_FA_BULLSEYE"", &GameState.state.Targetting, 30, 30, true)) {
            MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.Targetting = !MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.Targetting;
        };
        MemMgr.MutexRelease();
        ShowTooltip("Target");
        ImGui::TableSetColumnIndex(4);
        MemMgr.MutexAquire();
        if (ToggleButton(ICON_FA_SKULL_CROSSBONES"", &GameState.state.Combat, 30, 30, true)) {
            MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.Combat = !MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.Combat;
        };
        MemMgr.MutexRelease();
        ShowTooltip("Combat");
        ImGui::TableSetColumnIndex(5);
        if (ToggleButton(ICON_FA_BUG"", &Experimental.Debug, 30, 30, true)) {};
        ShowTooltip("Debug Options");

        ImGui::EndTable();
        

        ImGui::Separator();
        //ImGui::Text(ICON_FA_MAP_MARKER_ALT " Icon example");
        //ImGui::Text(WStringToString(player->name).c_str());

        MemMgr.MutexAquire();
        ImGui::BeginTable("skills", 8);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        if (ToggleButton("1", &GameState.CombatSkillState[0], 20, 20, true)) {
            MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].CombatSkillState[0] = !MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].CombatSkillState[0];
        }; ImGui::TableSetColumnIndex(1);
        if (ToggleButton("2", &GameState.CombatSkillState[1], 20, 20, true)) {
            MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].CombatSkillState[1] = !MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].CombatSkillState[1];
        }; ImGui::TableSetColumnIndex(2);
        if (ToggleButton("3", &GameState.CombatSkillState[2], 20, 20, true)) {
            MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].CombatSkillState[2] = !MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].CombatSkillState[2];
        }; ImGui::TableSetColumnIndex(3);
        if (ToggleButton("4", &GameState.CombatSkillState[3], 20, 20, true)) {
            MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].CombatSkillState[3] = !MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].CombatSkillState[3];
        }; ImGui::TableSetColumnIndex(4);
        if (ToggleButton("5", &GameState.CombatSkillState[4], 20, 20, true)) {
            MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].CombatSkillState[4] = !MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].CombatSkillState[4];
        }; ImGui::TableSetColumnIndex(5);
        if (ToggleButton("6", &GameState.CombatSkillState[5], 20, 20, true)) {
            MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].CombatSkillState[5] = !MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].CombatSkillState[5];
        }; ImGui::TableSetColumnIndex(6);
        if (ToggleButton("7", &GameState.CombatSkillState[6], 20, 20, true)) {
            MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].CombatSkillState[6] = !MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].CombatSkillState[6];
        }; ImGui::TableSetColumnIndex(7);
        if (ToggleButton("8", &GameState.CombatSkillState[7], 20, 20, true)) {
            MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].CombatSkillState[7] = !MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].CombatSkillState[7];
        };

        ImGui::EndTable();
        MemMgr.MutexRelease();


        mainWindowPos = ImGui::GetWindowPos();
        mainWindowSize = ImGui::GetWindowSize();
    }
    ImGui::End();
    // BUTTON UTILS

    Experimental.CreateParty = (GW::PartyMgr::GetPartySize() > 1) ? true : false;
    
    if ((GameVars.Party.SelfPartyNumber == 1) && (GameVars.Map.IsPartyLoaded))
    {
        ImVec2 main_window_pos = mainWindowPos;
        ImVec2 main_window_size = mainWindowSize;

        ImVec2 new_window_pos = ImVec2(main_window_pos.x + main_window_size.x, main_window_pos.y);
        ImGui::SetNextWindowPos(new_window_pos, ImGuiCond_FirstUseEver);

        ImGui::Begin("Utilities", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        if (!ImGui::IsWindowCollapsed())
        {                          
                if ((ImGui::CollapsingHeader("Party Setup")) && ((GameVars.Map.IsOutpost)))
                {
                    ImGui::BeginTable("USetuptable", 2);
                    ImGui::TableSetupColumn("Invite");
                    ImGui::TableSetupColumn("Candidate");
                    ImGui::TableHeadersRow();
              
                    Scandidates candidateSelf;

                    if (GameVars.Target.Self.Living) {
                        candidateSelf.player_ID = GameVars.Target.Self.Living->player_number;
                    }
                    else
                    {
                        candidateSelf.player_ID - 1;
                    }
                    candidateSelf.MapID = GW::Map::GetMapID();
                    candidateSelf.MapRegion = GW::Map::GetRegion();
                    candidateSelf.MapDistrict = GW::Map::GetDistrict();

                    if (candidateSelf.player_ID > 0) {
                        MemMgr.MutexAquire();
                        Scandidates* candidates = MemMgr.MemData->NewCandidates.ListClients();
                        MemMgr.MutexRelease();

                        for (int i = 0; i < 8; ++i) {                 
                            if (candidates[i].player_ID != 0) { // Check if the slot is not empty

                                bool SameMapId = (candidateSelf.MapID == candidates[i].MapID);
                                bool SameMapRegion = (candidateSelf.MapRegion == candidates[i].MapRegion);
                                bool SameMapDistrict = (candidateSelf.MapDistrict == candidates[i].MapDistrict);
                                bool SamePlayerID = (candidateSelf.player_ID == candidates[i].player_ID);
                                //bool ExistsInParty = existsInParty(candidates[i].player_ID);
                                bool ExistsInParty = false;

                                if ((candidates[i].player_ID > 0) && SameMapId && SameMapRegion && SameMapDistrict && !SamePlayerID && !ExistsInParty)
                                {
                                    std::string playername("Not Accessible");
                                    playername = WStringToString(GW::PlayerMgr::GetPlayerName(candidates[i].player_ID));

                                    ImGui::TableNextRow();
                                    ImGui::TableSetColumnIndex(0);
                                    auto player = GW::PlayerMgr::GetPlayerByID(candidates[i].player_ID);
                                    std::string buttonName = ICON_FA_USERS + playername + std::to_string(i);

                                    if (ToggleButton(buttonName.c_str(), &Experimental.CreateParty, 30, 30, true)) { GW::PartyMgr::InvitePlayer(player->name); };
                                    ShowTooltip(playername.c_str());

                                    ImGui::TableSetColumnIndex(1);
                                    ImGui::Text(playername.c_str());

                                }
                            }
                        }
                     }
                    ImGui::EndTable();                  
                }
            
            ImGui::Separator();
    
            if (ImGui::CollapsingHeader("Control"))
            {
                ImGui::BeginTable("UControlTable", 4);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (ToggleButton(ICON_FA_MAP_MARKER_ALT"", &GameState.state.HeroFlag, 30, 30, ((GameVars.Party.SelfPartyNumber == 1) && (GameVars.Map.IsExplorable)) ? true : false)) { /*do stuff*/ };
                ShowTooltip("Flag");
                ImGui::TableSetColumnIndex(1);
                Experimental.ForceFollow = false;
                if (ToggleButton(ICON_FA_LINK"", &Experimental.ForceFollow, 30, 30, ((GameVars.Party.SelfPartyNumber == 1) && (GameVars.Map.IsExplorable)) ? true : false)) {
                    MemMgr.MutexAquire();
                    for (int i = 0; i < 8; i++) {
                        MemMgr.MemData->command[i] = rc_ForceFollow;
                    }
                    MemMgr.MutexRelease();
                };
                ShowTooltip("Force Follow");
        
                ImGui::EndTable();
            }
            ImGui::Separator();
            
            if (ImGui::CollapsingHeader("Toggles"))
            {
                ImGui::BeginTable("UToggleTable", 5);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (ToggleButton(ICON_FA_BIRTHDAY_CAKE"", &Experimental.Pcons, 30, 30, true)) {
                    //GW::Chat::SendChat('/', "pcons"); 
                    MemMgr.MutexAquire();
                    for (int i = 0; i < 8; i++) {
                        MemMgr.MemData->command[i] = rc_Pcons;
                    }
                    MemMgr.MutexRelease();
                };
                ShowTooltip("Toggle Pcons");
                ImGui::TableSetColumnIndex(1);
                Experimental.Title = false;
                if (ToggleButton(ICON_FA_GRADUATION_CAP"", &Experimental.Title, 30, 30, true)) {
                    //GW::Chat::SendChat('/', "title");
                    MemMgr.MutexAquire();
                    for (int i = 0; i < 8; i++) {
                        MemMgr.MemData->command[i] = rc_Title;
                    }
                    MemMgr.MutexRelease();
                };
                ShowTooltip("Title");
                ImGui::TableSetColumnIndex(2);
                Experimental.resign = false;
                if (ToggleButton(ICON_FA_WINDOW_CLOSE"", &Experimental.resign, 30, 30, true)) {
                    //GW::Chat::SendChat('/', "resign"); 
                    MemMgr.MutexAquire();
                    for (int i = 0; i < 8; i++) {
                        MemMgr.MemData->command[i] = rc_Resign;
                    }
                    MemMgr.MutexRelease();
                };
                ShowTooltip("Resign");
                ImGui::TableSetColumnIndex(3);
                Experimental.TakeQuest = false;
                if (ToggleButton(ICON_FA_EXCLAMATION"", &Experimental.TakeQuest, 30, 30, true)) {
                    //GW::Chat::SendChat('/', "dialog take"); 
                    MemMgr.MutexAquire();
                    for (int i = 0; i < 8; i++) {
                        MemMgr.MemData->command[i] = rc_GetQuest;
                    }
                    MemMgr.MutexRelease();
                };
                ShowTooltip("Get Quest (WIP)");
                ImGui::TableSetColumnIndex(4);
                Experimental.Openchest = false;
                if (ToggleButton(ICON_FA_BOX_OPEN"", &Experimental.Openchest, 30, 30, true)) {
                    //GW::Chat::SendChat('/', "chest"); 
                    MemMgr.MutexAquire();
                    for (int i = 0; i < 8; i++) {
                        MemMgr.MemData->command[i] = rc_Chest;
                    }
                    MemMgr.MutexRelease();
                };
                ShowTooltip("Open Chest");
                ImGui::EndTable();
            }
            ImGui::Separator();

            if (ImGui::CollapsingHeader("Config"))
            {
                ImGui::Text("Extended Autonomy On Combat");

                if (SelectableComboValues("Melee", &MeleeAutonomy, namedValues))
                {
                    const char* selectedName = namedValues[MeleeAutonomy].name;
                    float selectedValue = namedValues[MeleeAutonomy].value;
                    MemMgr.MutexAquire();
                    for (int i = 0; i < 8; i++) {
                        MemMgr.MemData->GameState[i].state.MeleeRangeValue = namedValues[MeleeAutonomy].value;    
                    }
                    MemMgr.MutexRelease();
                }
                if (SelectableComboValues("Ranged", &RangedAutonomy, namedValues))
                {
                    const char* selectedName = namedValues[RangedAutonomy].name;
                    float selectedValue = namedValues[RangedAutonomy].value;
                    MemMgr.MutexAquire();
                    for (int i = 0; i < 8; i++) {
                        MemMgr.MemData->GameState[i].state.RangedRangeValue = namedValues[RangedAutonomy].value;
                    }
                    MemMgr.MutexRelease();
                }
            }
        }
        ImGui::End();
    }


    // FLAGGING

    if ((GameState.state.HeroFlag) && (GameVars.Map.IsExplorable))
    {
        ImVec2 main_window_pos = mainWindowPos;
        ImVec2 main_window_size = mainWindowSize;

        ImVec2 new_window_pos = ImVec2(main_window_pos.x + main_window_size.x, main_window_pos.y);

        ImGui::SetNextWindowPos(new_window_pos, ImGuiCond_FirstUseEver);
        ImGui::Begin("Flagging", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        if (!ImGui::IsWindowCollapsed())
        {

            ImGui::BeginTable("Flags", 3);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (ToggleButton("All", &GameVars.Party.Flags[0], 30, 30, true)) { Flag(0); };
            ImGui::TableSetColumnIndex(1);
            if (ToggleButton("1", &GameVars.Party.Flags[1], 30, 30, ((GW::PartyMgr::GetPartySize() < 2) ? false : true))) { Flag(1); };
            ImGui::TableSetColumnIndex(2);
            if (ToggleButton("2", &GameVars.Party.Flags[2], 30, 30, ((GW::PartyMgr::GetPartySize() < 3) ? false : true))) { Flag(2); };

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (ToggleButton("3", &GameVars.Party.Flags[3], 30, 30, ((GW::PartyMgr::GetPartySize() < 4) ? false : true))) { Flag(3); };
            ImGui::TableSetColumnIndex(1);
            if (ToggleButton("4", &GameVars.Party.Flags[4], 30, 30, ((GW::PartyMgr::GetPartySize() < 5) ? false : true))) { Flag(4); };
            ImGui::TableSetColumnIndex(2);
            if (ToggleButton("5", &GameVars.Party.Flags[5], 30, 30, ((GW::PartyMgr::GetPartySize() < 6) ? false : true))) { Flag(5); };

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (ToggleButton("6", &GameVars.Party.Flags[6], 30, 30, ((GW::PartyMgr::GetPartySize() < 7) ? false : true))) { Flag(6); };
            ImGui::TableSetColumnIndex(1);
            if (ToggleButton("7", &GameVars.Party.Flags[7], 30, 30, ((GW::PartyMgr::GetPartySize() < 8) ? false : true))) { Flag(7); };
            ImGui::TableSetColumnIndex(2);
            if (ToggleButton("X", &GameVars.Party.Flags[0], 30, 30, true)) { UnflagAllHeroes(); };

            ImGui::EndTable();
        }

        ImGui::End();
    }

    



    //CONTROL PANEL

    if ((GameVars.Party.SelfPartyNumber == 1) && (GW::PartyMgr::GetPartySize() > 1) && (!GameVars.Map.IsLoading) && (GameVars.Map.IsPartyLoaded))
    {



        ImGui::Begin("ControlPanel", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        if (!ImGui::IsWindowCollapsed())
        {


            std::string tableName = "MaincontrolPanelTable";
            ImGui::BeginTable(tableName.c_str(), 6);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);

            MemMgr.MutexAquire();
            if (ToggleButton(ICON_FA_RUNNING "", &ControlAll.state.Following, 30, 30, true)) {
                for (int i = 0; i < 8; i++)
                {
                    MemMgr.MemData->GameState[i].state.Following = ControlAll.state.Following;
                }
            }; ShowTooltip("Follow All");
            MemMgr.MutexRelease();
            ImGui::TableSetColumnIndex(1);
            MemMgr.MutexAquire();
            if (ToggleButton(ICON_FA_PODCAST"", &ControlAll.state.Scatter, 30, 30, true)) {
                for (int i = 0; i < 8; i++) { MemMgr.MemData->GameState[i].state.Scatter = ControlAll.state.Scatter; }
            }; ShowTooltip("Scatter All");
            MemMgr.MutexRelease();
            ImGui::TableSetColumnIndex(2);
            MemMgr.MutexAquire();
            if (ToggleButton(ICON_FA_COINS"", &ControlAll.state.Looting, 30, 30, true)) {
                for (int i = 0; i < 8; i++) { MemMgr.MemData->GameState[i].state.Looting = ControlAll.state.Looting; }
            }; ShowTooltip("Loot All");
            MemMgr.MutexRelease();
            ImGui::TableSetColumnIndex(3);
            MemMgr.MutexAquire();
            if (ToggleButton(ICON_FA_BULLSEYE"", &ControlAll.state.Targetting, 30, 30, true)) {
                for (int i = 0; i < 8; i++) { MemMgr.MemData->GameState[i].state.Targetting = ControlAll.state.Targetting; }
            }; ShowTooltip("Target All");
            MemMgr.MutexRelease();
            ImGui::TableSetColumnIndex(4);
            MemMgr.MutexAquire();
            if (ToggleButton(ICON_FA_SKULL_CROSSBONES"", &ControlAll.state.Combat, 30, 30, true)) {
                for (int i = 0; i < 8; i++) { MemMgr.MemData->GameState[i].state.Combat = ControlAll.state.Combat; }
            }; ShowTooltip("Combat All");
            MemMgr.MutexRelease();
            ImGui::EndTable();
            ImGui::Separator();


            std::string tableskillName = "Maincontrolskill";
            ImGui::BeginTable(tableskillName.c_str(), 8);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            MemMgr.MutexAquire();
            if (ToggleButton("1", &ControlAll.CombatSkillState[0], 20, 20, true)) {
                for (int i = 0; i < 8; i++) {
                    MemMgr.MemData->GameState[i].CombatSkillState[0] = ControlAll.CombatSkillState[0];
                }
            }; MemMgr.MutexRelease(); ImGui::TableSetColumnIndex(1);
            MemMgr.MutexAquire();
            if (ToggleButton("2", &ControlAll.CombatSkillState[1], 20, 20, true)) {
                for (int i = 0; i < 8; i++) {
                    MemMgr.MemData->GameState[i].CombatSkillState[1] = ControlAll.CombatSkillState[1];
                }
            }; MemMgr.MutexRelease(); ImGui::TableSetColumnIndex(2);
            MemMgr.MutexAquire();
            if (ToggleButton("3", &ControlAll.CombatSkillState[2], 20, 20, true)) {
                for (int i = 0; i < 8; i++) {
                    MemMgr.MemData->GameState[i].CombatSkillState[2] = ControlAll.CombatSkillState[2];
                }
            }; MemMgr.MutexRelease(); ImGui::TableSetColumnIndex(3);
            MemMgr.MutexAquire();
            if (ToggleButton("4", &ControlAll.CombatSkillState[3], 20, 20, true)) {
                for (int i = 0; i < 8; i++) {
                    MemMgr.MemData->GameState[i].CombatSkillState[3] = ControlAll.CombatSkillState[3];
                }
            }; MemMgr.MutexRelease(); ImGui::TableSetColumnIndex(4);
            MemMgr.MutexAquire();
            if (ToggleButton("5", &ControlAll.CombatSkillState[4], 20, 20, true)) {
                for (int i = 0; i < 8; i++) {
                    MemMgr.MemData->GameState[i].CombatSkillState[4] = ControlAll.CombatSkillState[4];
                }
            }; MemMgr.MutexRelease(); ImGui::TableSetColumnIndex(5);
            MemMgr.MutexAquire();
            if (ToggleButton("6", &ControlAll.CombatSkillState[5], 20, 20, true)) {
                for (int i = 0; i < 8; i++) {
                    MemMgr.MemData->GameState[i].CombatSkillState[5] = ControlAll.CombatSkillState[5];
                }
            }; MemMgr.MutexRelease(); ImGui::TableSetColumnIndex(6);
            MemMgr.MutexAquire();
            if (ToggleButton("7", &ControlAll.CombatSkillState[6], 20, 20, true)) {
                for (int i = 0; i < 8; i++) {
                    MemMgr.MemData->GameState[i].CombatSkillState[6] = ControlAll.CombatSkillState[6];
                }
            }; MemMgr.MutexRelease(); ImGui::TableSetColumnIndex(7);
            MemMgr.MutexAquire();
            if (ToggleButton("8", &ControlAll.CombatSkillState[7], 20, 20, true)) {
                for (int i = 0; i < 8; i++) {
                    MemMgr.MemData->GameState[i].CombatSkillState[7] = ControlAll.CombatSkillState[7];
                }
            }; MemMgr.MutexRelease();
            ImGui::EndTable();

            if (ImGui::CollapsingHeader("Party"))
            {
                for (int i = 0; i < 8; i++) {

                    MemMgr.MutexAquire();
                    if (MemMgr.MemData->MemPlayers[i].IsActive) {

                        auto  chosen_player_name = GW::Agents::GetPlayerNameByLoginNumber(GameVars.Party.PartyInfo->players[i].login_number);
                        //auto  chosen_player_name = L"test char";
                        std::string playername = WStringToString(chosen_player_name);

                        if (ImGui::CollapsingHeader(playername.c_str()))
                        {

                            std::string tableName = "controlpaneltable" + std::to_string(i);
                            ImGui::BeginTable(tableName.c_str(), 6);
                            ImGui::TableNextRow();

                            ImGui::TableSetColumnIndex(0);

                            if (ToggleButton(ICON_FA_RUNNING "", &MemMgr.MemData->GameState[i].state.Following, 30, 30, (i == 0) ? false : true)) {  /* do stuff */ }; ShowTooltip("Follow");
                            ImGui::TableSetColumnIndex(1);
                            if (ToggleButton(ICON_FA_PODCAST"", &MemMgr.MemData->GameState[i].state.Scatter, 30, 30, (i == 0) ? false : true)) {  /* do stuff */ }; ShowTooltip("Scatter");
                            ImGui::TableSetColumnIndex(2);
                            if (ToggleButton(ICON_FA_COINS"", &MemMgr.MemData->GameState[i].state.Looting, 30, 30, true)) {  /* do stuff */ }; ShowTooltip("Loot");
                            ImGui::TableSetColumnIndex(3);
                            if (ToggleButton(ICON_FA_BULLSEYE"", &MemMgr.MemData->GameState[i].state.Targetting, 30, 30, true)) {  /* do stuff */ }; ShowTooltip("Target");
                            ImGui::TableSetColumnIndex(4);
                            if (ToggleButton(ICON_FA_SKULL_CROSSBONES"", &MemMgr.MemData->GameState[i].state.Combat, 30, 30, true)) {  /* do stuff */ }; ShowTooltip("Combat");
                            ImGui::EndTable();
                            ImGui::Separator();

                            std::string tableskillName = "controlskill" + std::to_string(i);
                            ImGui::BeginTable(tableskillName.c_str(), 8);
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            if (ToggleButton("1", &MemMgr.MemData->GameState[i].CombatSkillState[0], 20, 20, true)) { /* do stuff */ }; ImGui::TableSetColumnIndex(1);
                            if (ToggleButton("2", &MemMgr.MemData->GameState[i].CombatSkillState[1], 20, 20, true)) { /* do stuff */ }; ImGui::TableSetColumnIndex(2);
                            if (ToggleButton("3", &MemMgr.MemData->GameState[i].CombatSkillState[2], 20, 20, true)) { /* do stuff */ }; ImGui::TableSetColumnIndex(3);
                            if (ToggleButton("4", &MemMgr.MemData->GameState[i].CombatSkillState[3], 20, 20, true)) { /* do stuff */ }; ImGui::TableSetColumnIndex(4);
                            if (ToggleButton("5", &MemMgr.MemData->GameState[i].CombatSkillState[4], 20, 20, true)) { /* do stuff */ }; ImGui::TableSetColumnIndex(5);
                            if (ToggleButton("6", &MemMgr.MemData->GameState[i].CombatSkillState[5], 20, 20, true)) { /* do stuff */ }; ImGui::TableSetColumnIndex(6);
                            if (ToggleButton("7", &MemMgr.MemData->GameState[i].CombatSkillState[6], 20, 20, true)) { /* do stuff */ };; ImGui::TableSetColumnIndex(7);
                            if (ToggleButton("8", &MemMgr.MemData->GameState[i].CombatSkillState[7], 20, 20, true)) { /* do stuff */ };
                            ImGui::EndTable();
                        }
                    }
                    MemMgr.MutexRelease();
                }
            }

        }
        ImGui::End();
    }
    // END CONTROL PANEL


    //******************************   OVERLAY DRAWING *********************************************
    Overlay.BeginDraw();

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    if ((GameVars.Map.IsExplorable)) {
        

        if ((ClickHandler.WaitForClick) && (ClickHandler.msg == eHeroFlag) && (!ClickHandler.IsPartyWide)) {
            GW::Vec3f wolrdPos = Overlay.ScreenToWorld();
            Overlay.DrawFlagHero(drawList, wolrdPos);
        }

        if ((ClickHandler.WaitForClick) && (ClickHandler.msg == eHeroFlag) && (ClickHandler.IsPartyWide)) {
            GW::Vec3f wolrdPos = Overlay.ScreenToWorld();
            Overlay.DrawFlagAll(drawList, wolrdPos);
        }

        MemMgr.MutexAquire();
        if ((MemMgr.MemData->MemPlayers[0].IsActive) && (MemMgr.MemData->MemPlayers[0].IsFlagged)) {
            GW::Vec3f pos;
            pos.x = MemMgr.MemData->MemPlayers[0].FlagPos.x;
            pos.y = MemMgr.MemData->MemPlayers[0].FlagPos.y;
            Overlay.DrawFlagAll(drawList, pos);
        }
        MemMgr.MutexRelease();

        MemMgr.MutexAquire();
        for (int i = 1; i < 8; i++) {
            if ((MemMgr.MemData->MemPlayers[i].IsActive) && (MemMgr.MemData->MemPlayers[i].IsFlagged)) {
                GW::Vec3f pos;
                pos.x = MemMgr.MemData->MemPlayers[i].FlagPos.x;
                pos.y = MemMgr.MemData->MemPlayers[i].FlagPos.y;
                Overlay.DrawFlagHero(drawList, pos);
            }

        }
        MemMgr.MutexRelease();

    }
    if ((!GameVars.Map.IsLoading)) {
        if (Experimental.Debug) {

            ImGui::SetNextWindowPos(ImVec2(mainWindowPos.x + mainWindowSize.x + 10, mainWindowPos.y), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(50, 50), ImGuiCond_FirstUseEver);

            ImGui::Begin("Debug", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
            if (!ImGui::IsWindowCollapsed())
            {
                ImGui::BeginTable("Debugtable", 3);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (ToggleButton("Area Rings", &Experimental.AreaRings, 80, 30, true)) {};
                ImGui::TableSetColumnIndex(1);
                if (ToggleButton("MemMon", &Experimental.MemMon, 80, 30, true)) {};
                ImGui::TableSetColumnIndex(2);
                if (ToggleButton("SkillData", &Experimental.SkillData, 80, 30, true)) {};
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (ToggleButton("Candidates", &Experimental.PartyCandidates, 80, 30, true)) {};
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (ToggleButton("FormA", &Experimental.FormationA, 80, 30, true)) {};
                ImGui::EndTable();
            }
            ImGui::End();

            // Candidates
            if ((Experimental.PartyCandidates) && (GameVars.Map.IsOutpost) && (GameVars.Map.IsPartyLoaded)) {

                Scandidates candidateSelf;

                if (GameVars.Target.Self.Living) {
                    candidateSelf.player_ID = GameVars.Target.Self.Living->player_number;
                }
                else
                {
                    candidateSelf.player_ID - 1;
                }
                candidateSelf.MapID = GW::Map::GetMapID();
                candidateSelf.MapRegion = GW::Map::GetRegion();
                candidateSelf.MapDistrict = GW::Map::GetDistrict();

                ImGui::Begin("Candidates Array");
                ImGui::Columns(4, "mem_player_columns");
                
                if (candidateSelf.player_ID > 0) {
                    ImGui::Text("Live User %d", candidateSelf.player_ID);
                    std::string playername = WStringToString(GW::PlayerMgr::GetPlayerName(candidateSelf.player_ID));
                    ImGui::Text("Name: %s", playername.c_str());
                    ImGui::Text("Map ID: %d", candidateSelf.MapID);
                    ImGui::Text("Map Region: %d", candidateSelf.MapRegion);
                    ImGui::Text("Map District: %d", candidateSelf.MapDistrict);
                    MemMgr.MutexAquire();
                    auto counter = MemMgr.MemData->NewCandidates.GetKeepaliveCounter();
                    ImGui::Text("Keepalive: %d", counter);
                    MemMgr.MutexRelease();
                   

                    ImGui::Separator();


                    if (candidateSelf.player_ID > 0) {
                        MemMgr.MutexAquire();
                        Scandidates* candidates = MemMgr.MemData->NewCandidates.ListClients();
                        MemMgr.MutexRelease();

                        for (int i = 0; i < 8; ++i) {
                            if (candidates[i].player_ID != 0) { // Check if the slot is not empty

                                bool SameMapId = (candidateSelf.MapID == candidates[i].MapID);
                                bool SameMapRegion = (candidateSelf.MapRegion == candidates[i].MapRegion);
                                bool SameMapDritrict = (candidateSelf.MapDistrict == candidates[i].MapDistrict);
                                bool SamePlayerID = (candidateSelf.player_ID == candidates[i].player_ID);
                                //bool ExistsInParty = existsInParty(candidates[i].player_ID);
                                bool ExistsInParty = false;

                                if ((candidates[i].player_ID > 0))
                                {
                                    std::string playername("Not Accessible");
                                    if (SameMapId && SameMapRegion && SameMapDritrict) {
                                        playername = WStringToString(GW::PlayerMgr::GetPlayerName(candidates[i].player_ID));
                                    }

                                    ImGui::Text("Name: %s", playername.c_str());
                                    ImGui::Text("Map ID: %d", candidates[i].MapID);
                                    ImGui::Text("Map Region: %d", candidates[i].MapRegion);
                                    ImGui::Text("Map District: %d", candidates[i].MapDistrict);
                                    MemMgr.MutexAquire();
                                    auto selfkeepalive = MemMgr.MemData->NewCandidates.GetEachKeepalive(i);
                                    MemMgr.MutexRelease();
                                    ImGui::Text("Keepalive: %d", selfkeepalive);

                                   if ((i + 1) % 4 == 0) { // Move to the next row after every 4 players
                                        ImGui::Columns(1);  // Reset columns to 1
                                        //ImGui::Separator(); // Add a separator between rows
                                        ImGui::Columns(4);  // Reset columns to 4 for the next row
                                    }
                                    else {
                                        ImGui::NextColumn(); // Move to the next column
                                    }
                                }
                            }
                        }
                    }

                    ImGui::End();
                }
            }

            // TACO areas
            if (Experimental.AreaRings) {
                Overlay.DrawPoly3D(drawList, PlayerPos, GW::Constants::Range::Touch, IM_COL32(255, 255, 255, 127), 360);
                Overlay.DrawPoly3D(drawList, PlayerPos, GW::Constants::Range::Adjacent, IM_COL32(255, 255, 255, 127), 360);
                Overlay.DrawPoly3D(drawList, PlayerPos, GW::Constants::Range::Nearby, IM_COL32(255, 255, 255, 127), 360);
                Overlay.DrawPoly3D(drawList, PlayerPos, GW::Constants::Range::Area, IM_COL32(255, 255, 255, 127), 360);
                Overlay.DrawPoly3D(drawList, PlayerPos, GW::Constants::Range::Earshot, IM_COL32(255, 255, 255, 127), 360);

                Overlay.DrawPoly3D(drawList, PlayerPos, MeleeRangeValue, IM_COL32(180, 5, 11, 255), 360);
                Overlay.DrawPoly3D(drawList, PlayerPos, RangedRangeValue, IM_COL32(180, 165, 0, 255), 360);
            }

            if (Experimental.FormationA) {

                Overlay.DrawDoubleRowLineFormation(drawList, PlayerPos, 0, IM_COL32(255, 255, 0, 255));
            }


            if (Experimental.MemMon) {

                //MEMORY MONITOR

                ImGui::SetNextWindowSize(ImVec2(150, 150), ImGuiCond_FirstUseEver);
                ImGui::Begin("MemPlayer Monitor");

                ImGui::Columns(4, "mem_player_columns");
                //ImGui::Separator();


                for (int i = 0; i < 8; ++i) {
                    MemMgr.MutexAquire();
                    const MemMgrClass::MemSinglePlayer& player = MemMgr.MemData->MemPlayers[i];
                    MemMgr.MutexRelease();

                    ImGui::Text("Player %d", i);
                    ImGui::Text("ID: %d", player.ID);
                    //ImGui::Text("Energy: %.2f", player.Energy);
                    DrawEnergyValue("Energy", player.Energy, 0.4f);
                    //ImGui::Text("Skill Limit: %f", skillEnergyTarget);
                    //DrawEnergyValue("Energy Regen", 0.04f, player.energyRegen);


                    DrawYesNoText("Is Active", player.IsActive);
                    ImGui::Text("Keepalive: %d", player.Keepalivecounter);
                    DrawYesNoText("Is Hero", player.IsHero);
                    DrawYesNoText("Is Flagged", player.IsFlagged);
                    DrawYesNoText("Is All Flag", player.IsAllFlag);

                    ImGui::Text("Flag Position: (%.2f, %.2f)", player.FlagPos.x, player.FlagPos.y);


                    if ((i + 1) % 4 == 0) { // Move to the next row after every 4 players
                        ImGui::Columns(1);  // Reset columns to 1
                        //ImGui::Separator(); // Add a separator between rows
                        ImGui::Columns(4);  // Reset columns to 4 for the next row
                    }
                    else {
                        ImGui::NextColumn(); // Move to the next column
                    }
                }

                ImGui::End();
            }

            if (Experimental.SkillData) {

                //Skill Monitor

                ImGui::SetNextWindowSize(ImVec2(150, 150), ImGuiCond_FirstUseEver);

                ImGui::Begin("SkillDataMonitor");

                ImGui::Columns(4, "skill_mon_columns");
                //ImGui::Separator();

                ImGui::Text("PlayerID: %d", GameVars.Target.Self.ID);
                ImGui::Text("Player_number: %d", GameVars.Target.Self.Living->player_number);
                ImGui::Text("Dagger Status: %u", static_cast<unsigned int>(GameVars.Target.Self.Living->dagger_status));
                ImGui::Separator();

                if (GameVars.Target.Nearest.ID) {
                    ImGui::Text("TargetID: %d", GameVars.Target.Nearest.ID);
                    ImGui::Text("Target Dagger Status: %u", static_cast<unsigned int>(GameVars.Target.Nearest.Living->dagger_status));
                    ImGui::Separator();
                }


                for (int i = 0; i < 8; ++i) {
                    const HeroAI::SkillStruct& skill = GameVars.SkillBar.Skills[i];

                    ImGui::Text("Skill %d", i);
                    ImGui::Text("SkillID: %d", skill.Data.skill_id);
                    
                    std::string skillheader = "Data (";
                    skillheader += std::to_string(i) + ")";

                    if (ImGui::CollapsingHeader(skillheader.c_str()))
                    {
                        ImGui::Text("Activation: %.2f", skill.Data.activation);
                        ImGui::Text("Adrenaline: %d", skill.Data.adrenaline);
                        ImGui::Text("Aftercast: %.2f", skill.Data.aftercast);
                        ImGui::Text("Combo: %d", skill.Data.combo);
                        ImGui::Text("Combo Req: %d", skill.Data.combo_req);
                        ImGui::Text("Energy Cost: %d", skill.Data.energy_cost);
                        ImGui::Text("Health Cost: %d", skill.Data.health_cost);
                    }
                    DrawYesNoText("Custom Data", skill.HasCustomData);
                    skillheader = "CustomData (";
                    skillheader += std::to_string(i) + ")";
                    if (ImGui::CollapsingHeader(skillheader.c_str()))
                    {
                        ImGui::Text("Skill: %s", SkillNames.GetSkillNameByID(skill.CustomData.SkillID).c_str());
                        ImGui::Text("Nature: %s", SkillNames.GetNatureByID(skill.CustomData.Nature).c_str());
                        ImGui::Text("Type: %s", SkillNames.GetTypeByID(skill.CustomData.SkillType).c_str());
                        ImGui::Text("Alliegance: %s", SkillNames.GetTargetByID(skill.CustomData.TargetAllegiance).c_str());
                        skillheader = "Conditions (";
                        skillheader += std::to_string(i) + ")";
                        if (ImGui::CollapsingHeader(skillheader.c_str()))
                        {
                            DrawYesNoText("IsAlive", skill.CustomData.Conditions.IsAlive);
                             DrawYesNoText("HasCondition", skill.CustomData.Conditions.HasCondition);
                             DrawYesNoText("HasDeepWound", skill.CustomData.Conditions.HasDeepWound);
                             DrawYesNoText("HasCrippled", skill.CustomData.Conditions.HasCrippled);
                             DrawYesNoText("HasBleeding", skill.CustomData.Conditions.HasBleeding);
                             DrawYesNoText("HasPoison", skill.CustomData.Conditions.HasPoison);
                             DrawYesNoText("HasWeaponSpell", skill.CustomData.Conditions.HasWeaponSpell);
                             DrawYesNoText("HasEnchantment", skill.CustomData.Conditions.HasEnchantment);
                             DrawYesNoText("HasHex", skill.CustomData.Conditions.HasHex);
                             DrawYesNoText("IsCasting", skill.CustomData.Conditions.IsCasting);
                             DrawYesNoText("IsNockedDown", skill.CustomData.Conditions.IsNockedDown);
                             DrawYesNoText("IsMoving", skill.CustomData.Conditions.IsMoving);
                             DrawYesNoText("IsAttacking", skill.CustomData.Conditions.IsAttacking);
                             DrawYesNoText("IsHoldingItem", skill.CustomData.Conditions.IsHoldingItem);
                             DrawYesNoText("TargettingStrict", skill.CustomData.Conditions.TargettingStrict);
                             ImGui::Text("LessLife: %.2f", skill.CustomData.Conditions.LessLife);
                             ImGui::Text("MoreLife: %.2f", skill.CustomData.Conditions.MoreLife);
                             ImGui::Text("LessEnergy: %.2f", skill.CustomData.Conditions.LessEnergy);
                             ImGui::Text("SacrificeHealth: %.2f", skill.CustomData.Conditions.SacrificeHealth);
                             DrawYesNoText("IsPartyWide", skill.CustomData.Conditions.IsPartyWide);
                             DrawYesNoText("UniqueProperty", skill.CustomData.Conditions.UniqueProperty);
                        }
                    }


                    if ((i + 1) % 4 == 0) { // Move to the next row after every 4 players
                        ImGui::Columns(1);  // Reset columns to 1
                        ImGui::Separator(); // Add a separator between rows
                        ImGui::Columns(4);  // Reset columns to 4 for the next row
                    }
                    else {
                        ImGui::NextColumn(); // Move to the next column
                    }
                }



                ImGui::End();
            }
        }

    }

        Overlay.EndDraw();

    //****************************** END OVERLAY DRAWING *********************************************
}


void HeroAI::SignalTerminate()
{


    //overlay.Shutdown();
    MemMgr.MutexAquire();
    MemMgr.MemData->MemPlayers[GameVars.Party.SelfPartyNumber - 1].IsActive = false;
    MemMgr.MutexRelease();
    CloseHandle(MemMgr.hMutex);

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


void HeroAI::DrawSettings()
{
    ToolboxUIPlugin::DrawSettings();

}

int HeroAI::GetPartyNumber() {
    if (GameVars.Map.IsPartyLoaded) {
        for (int i = 0; i < GW::PartyMgr::GetPartySize(); i++)
        {
            const auto playerId = GW::Agents::GetAgentIdByLoginNumber(GameVars.Party.PartyInfo->players[i].login_number);
            if (playerId == GameVars.Target.Self.ID) { return i + 1; }
        }
}   
    return 0;
}

bool HeroAI::existsInParty(uint32_t player_id) {
    
    if (player_id <= 0) { return false; }
    for (int i = 0; i < (GW::PartyMgr::GetPartySize() - GW::PartyMgr::GetPartyHeroCount() - GW::PartyMgr::GetPartyHenchmanCount()); i++)
    {
        const auto playerId = GW::Agents::GetAgentIdByLoginNumber(GameVars.Party.PartyInfo->players[i].login_number);
        const auto agent = GW::Agents::GetAgentByID(playerId);
        const auto living = agent->GetAsAgentLiving();
        if (living->player_number == player_id) {
            return true;
        }
    }
    return false;
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
    bool IsAgentMelee = false;

    float distance = GW::Constants::Range::Earshot;
    float maxdistance = GW::Constants::Range::Earshot;
    if ((!GameState.combat.StayAlert.hasElapsed(3000)) || (IsBigAggro)) {
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

        IsAgentMelee = IsMelee(EnemyID);

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
        if ((Target == EnemyMartialMelee) && (!IsMartial) && (!IsAgentMelee)) { continue; }
        if ((Target == EnemyMartialRanged) && (!IsMartial) && (IsAgentMelee)) { continue; }

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

bool HeroAI::IsMelee(GW::AgentID AgentID) {

    const auto tempAgent = GW::Agents::GetAgentByID(AgentID);
    const auto tLiving = tempAgent->GetAsAgentLiving();
    //enum WeaponType { bow = 1, axe, hammer, daggers, scythe, spear, sword, wand = 10, staff1 = 12, staff2 = 14 }; // 1=bow, 2=axe, 3=hammer, 4=daggers, 5=scythe, 6=spear, 7=sWORD, 10=wand, 12=staff, 14=staff};

    switch (tLiving->weapon_type) {
    case axe:
    case hammer:
    case daggers:
    case scythe:
    case sword:
        return true;
        break;
    default:
        return false;
        break;
    }
    return false;

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
    bool IsAllyMelee = false;

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
        float venergy = 2.0f; // = tAllyLiving->energy;

        //ENERGY CHECKS
        MemMgr.MutexAquire();
        int PIndex = GetMemPosByID(AllyID);

        if (PIndex >= 0) {
            if ((MemMgr.MemData->MemPlayers[PIndex].IsActive) &&
                (!HasEffect(GW::Constants::SkillID::Blood_is_Power, AllyID)) &&
                (!HasEffect(GW::Constants::SkillID::Blood_Ritual, AllyID))
                ) {
                venergy = MemMgr.MemData->MemPlayers[PIndex].Energy;
            }
        }
        MemMgr.MutexRelease();

        if ((OtherAlly) && (AllyID == GameVars.Target.Self.ID)) { continue; }

        IsAllyMelee = IsMelee(AllyID);

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
        if ((Target == AllyMartialMelee) && (!IsMartial) && (!IsAllyMelee)) { continue; }
        if ((Target == AllyMartialRanged) && (!IsMartial) && (IsAllyMelee)) { continue; }

        if (vlife < StoredLife)
        {
            StoredLife = vlife;
            tStoredAllyID = AllyID;
        }

        //ENERGY CHECKS
        MemMgr.MutexAquire();
        PIndex = GetMemPosByID(AllyID);

        if (PIndex >= 0) {
            if ((MemMgr.MemData->MemPlayers[PIndex].IsActive) &&
                (MemMgr.MemData->MemPlayers[PIndex].Energy < StoredEnergy) &&
                (!HasEffect(GW::Constants::SkillID::Blood_is_Power, AllyID)) &&
                (!HasEffect(GW::Constants::SkillID::Blood_Ritual, AllyID))
               ) {
                StoredEnergy = venergy;
                tStoredAllyenergyID = AllyID;
            }
        }
        MemMgr.MutexRelease();

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

uint32_t HeroAI::TargetNearestNpc()
{
    const auto agents = GW::Agents::GetAgentArray();

    float distance = GW::Constants::Range::Earshot;

    static GW::AgentID EnemyID;
    static GW::AgentID tStoredEnemyID = 0;

    for (const GW::Agent* agent : *agents)
    {
        if (!agent) { continue; }
        if (!agent->GetIsLivingType()) { continue; }
        const auto tEnemyLiving = agent->GetAsAgentLiving();
        if (!tEnemyLiving) { continue; }
        if (tEnemyLiving->allegiance != GW::Constants::Allegiance::Npc_Minipet) { continue; }

        EnemyID = tEnemyLiving->agent_id;

        if (!EnemyID) { continue; }


        const auto tempAgent = GW::Agents::GetAgentByID(EnemyID);

        if (!tempAgent) { continue; }
        if (!tempAgent->GetAsAgentLiving()->GetIsAlive()) { continue; }

        if ((EnemyID == GameVars.Target.Self.ID)) { continue; }


        const auto dist = GW::GetDistance(GameVars.Target.Self.Living->pos, tempAgent->pos);

        if (dist < distance) {
            distance = dist;
            tStoredEnemyID = EnemyID;
        }
    }
    if (distance != GW::Constants::Range::Earshot) { return tStoredEnemyID; }
    else {
        return 0;
    }

}

bool HeroAI::IsSkillready(uint32_t slot) {
    if (GameVars.SkillBar.Skills[slot].Skill.skill_id == GW::Constants::SkillID::No_Skill) { return false; }
    if (GameVars.SkillBar.Skills[slot].Skill.GetRecharge() != 0) { return false; }
    return true;
}

bool HeroAI::IsOOCSkill(uint32_t slot) {

    if (GameVars.SkillBar.Skills[slot].CustomData.Conditions.IsOutOfCombat) { return true; }

    switch (GameVars.SkillBar.Skills[slot].CustomData.SkillType) {
    case GW::Constants::SkillType::Form:
    case GW::Constants::SkillType::Preparation:
        return true;
        break;
    }

    switch (GameVars.SkillBar.Skills[slot].Data.type) {
    case GW::Constants::SkillType::Form:
    case GW::Constants::SkillType::Preparation:
        return true;
        break;
    }
    
    switch (GameVars.SkillBar.Skills[slot].CustomData.Nature) {
        case Healing:
        case Hex_Removal:
        case Condi_Cleanse:
        case EnergyBuff:
        case Resurrection:
            return true;
            break;
        default:
            return false;
    }

    return false;
}


GW::AgentID HeroAI::GetAppropiateTarget(uint32_t slot) {
    GW::AgentID vTarget = 0;
    /* --------- TARGET SELECTING PER SKILL --------------*/
    if (GameState.state.isTargettingEnabled()) {
        if (GameVars.SkillBar.Skills[slot].HasCustomData) {
            bool Strict = GameVars.SkillBar.Skills[slot].CustomData.Conditions.TargettingStrict;
            switch (GameVars.SkillBar.Skills[slot].CustomData.TargetAllegiance) {
            case Enemy:
                vTarget = GameVars.Target.Called.ID;
                if (!vTarget) { vTarget = GameVars.Target.Nearest.ID; }
                break;
            case EnemyCaster:
                vTarget = GameVars.Target.NearestCaster.ID;
                if ((!vTarget) && (!Strict)) { vTarget = GameVars.Target.Nearest.ID; }
                break;
            case EnemyMartial:
                vTarget = GameVars.Target.NearestMartial.ID;
                if ((!vTarget) && (!Strict)) { vTarget = GameVars.Target.Nearest.ID; }
                break;
            case EnemyMartialMelee:
                vTarget = GameVars.Target.NearestMartialMelee.ID;
                if ((!vTarget) && (!Strict)) { vTarget = GameVars.Target.Nearest.ID; }
                break;
            case EnemyMartialRanged:
                vTarget = GameVars.Target.NearestMartialRanged.ID;
                if ((!vTarget) && (!Strict)) { vTarget = GameVars.Target.Nearest.ID; }
                break;
            case Ally:
                vTarget = GameVars.Target.LowestAlly.ID;
                break;
            case AllyCaster:
                vTarget = GameVars.Target.LowestAllyCaster.ID;
                if ((!vTarget) && (!Strict)) { vTarget = GameVars.Target.LowestAlly.ID; }
                break;
            case AllyMartial:
                vTarget = GameVars.Target.LowestAllyMartial.ID;
                if ((!vTarget) && (!Strict)) { vTarget = GameVars.Target.LowestAlly.ID; }
                break;
            case AllyMartialMelee:
                vTarget = GameVars.Target.LowestAllyMartialMelee.ID;
                if ((!vTarget) && (!Strict)) { vTarget = GameVars.Target.LowestAlly.ID; }
                break;
            case AllyMartialRanged:
                vTarget = GameVars.Target.LowestAllyMartial.ID;
                if ((!vTarget) && (!Strict)) { vTarget = GameVars.Target.LowestAlly.ID; }
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

    float hp_target = GameVars.SkillBar.Skills[slot].CustomData.Conditions.SacrificeHealth;
    float current_life =GameVars.Target.Self.Living->hp;
    if ((current_life < hp_target) && (GameVars.SkillBar.Skills[slot].Data.health_cost > 0.0f)) { GameState.combat.IsSkillCasting = false; return false; }

    const auto enough_adrenaline =
        (GameVars.SkillBar.Skills[slot].Data.adrenaline == 0) ||
        (GameVars.SkillBar.Skills[slot].Data.adrenaline > 0 &&
            GameVars.SkillBar.Skills[slot].Skill.adrenaline_a >= GameVars.SkillBar.Skills[slot].Data.adrenaline);

    if (!enough_adrenaline) { GameState.combat.IsSkillCasting = false; return false; }

    GW::AgentID vTarget = GetAppropiateTarget(slot);
    if (!vTarget) { return false; }

    //assassin strike chains
    const auto tempAgent = GW::Agents::GetAgentByID(vTarget);
    if (!tempAgent) { return false; }
    const auto tempAgentLiving = tempAgent->GetAsAgentLiving();
    if (!tempAgentLiving) { return false; }

   
    if ((GameVars.SkillBar.Skills[slot].Data.combo == 1) && ((tempAgentLiving->dagger_status != 0) && (tempAgentLiving->dagger_status != 3))) { return false; }
    if ((GameVars.SkillBar.Skills[slot].Data.combo == 2) && (tempAgentLiving->dagger_status != 1)) { return false; }
    if ((GameVars.SkillBar.Skills[slot].Data.combo == 3) && (tempAgentLiving->dagger_status != 2)) { return false; }


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
    if (!vTarget) { return false; } //because of forced targetting

    if (IsReadyToCast(slot))
     {
        //GameState.combat.IsSkillCasting = true;
        GameState.combat.IntervalSkillCasting = GameVars.SkillBar.Skills[slot].Data.activation + GameVars.SkillBar.Skills[slot].Data.aftercast + 750.0f;
        GameState.combat.LastActivity.reset();

        GW::SkillbarMgr::UseSkill(slot, vTarget);
        ChooseTarget();
        //GW::Agents::InteractAgent(GW::Agents::GetAgentByID((GameVars.Target.Called.ID) ? GameVars.Target.Called.ID: GameVars.Target.Nearest.ID), false); 
        return true;
    }
    return false;
}

bool HeroAI::InCastingRoutine() {
    return GameState.combat.IsSkillCasting;
}

void HeroAI::ChooseTarget() { 
    if (GameState.state.isTargettingEnabled()) {
        const auto target = GW::Agents::GetTargetId();
        if (!target) {
            if (GameVars.Target.Called.ID) {
                GW::Agents::ChangeTarget(GameVars.Target.Called.Agent); 
                if (GameVars.Target.Self.Living->weapon_type != 0) {
                    GW::Agents::InteractAgent(GameVars.Target.Called.Agent, false);
                }
               
            }
            else {
                GW::Agents::ChangeTarget(GameVars.Target.Nearest.Agent);
                if (GameVars.Target.Self.Living->weapon_type != 0) {
                    GW::Agents::InteractAgent(GameVars.Target.Nearest.Agent, false);
                }
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
    const auto* effect = GW::Effects::GetPlayerEffectBySkillId(skillID);
    const auto* buff = GW::Effects::GetPlayerBuffBySkillId(skillID);

    if ((effect) || (buff)) { return true; }

    int Skillptr = 0;

    const auto tempAgent = GW::Agents::GetAgentByID(agentID);
    if (!tempAgent) { return false; }
    const auto tempAgentLiving = tempAgent->GetAsAgentLiving();
    if (!tempAgentLiving) { return false; }

    Skillptr = CustomSkillData.GetPtrBySkillID(skillID);
    if (!Skillptr) { return false; }

    CustomSkillClass::CustomSkillDatatype vSkill = CustomSkillData.GetSkillByPtr(Skillptr);

    GW::Skill skill = *GW::SkillbarMgr::GetSkillConstantData(skillID);

    if ((vSkill.SkillType == GW::Constants::SkillType::WeaponSpell) ||
        (skill.type == GW::Constants::SkillType::WeaponSpell))
    {
        if (tempAgentLiving->GetIsWeaponSpelled()) { return true; }
    }
    return false;
}

int HeroAI::GetMemPosByID(GW::AgentID agentID) {
    for (int i = 0; i < 8; i++) {
        MemMgr.MutexAquire();
        if ((MemMgr.MemData->MemPlayers[i].IsActive) && (MemMgr.MemData->MemPlayers[i].ID == agentID)) {
            MemMgr.MutexRelease();
            return i;
        }  
        MemMgr.MutexRelease();
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
        case GW::Constants::SkillID::Unnatural_Signet:
            if ((tempAgentLiving->GetIsHexed() || tempAgentLiving->GetIsEnchanted()))
            {
                return true;
            }
            else { return false; }
            break;
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
    MemMgr.MutexAquire();
    int PIndex = GetMemPosByID(agentID);

    if (PIndex >= 0) {
        if ((MemMgr.MemData->MemPlayers[PIndex].IsActive) &&
            (GameVars.SkillBar.Skills[slot].CustomData.Conditions.LessEnergy != 0.0f) &&
            (MemMgr.MemData->MemPlayers[PIndex].Energy < GameVars.SkillBar.Skills[slot].CustomData.Conditions.LessEnergy)
           ) {
            NoOfFeatures++;
        }
    }
    MemMgr.MutexRelease();
    
    
    
    const auto TargetSkill = GW::SkillbarMgr::GetSkillConstantData(static_cast<GW::Constants::SkillID>(tempAgentLiving->skill));

    if (tempAgentLiving->GetIsCasting() && GameVars.SkillBar.Skills[slot].CustomData.Conditions.IsCasting && TargetSkill && TargetSkill->activation > 0.250f) { NoOfFeatures++; }

    if (featurecount == NoOfFeatures) { return true; }

    return false;
}


