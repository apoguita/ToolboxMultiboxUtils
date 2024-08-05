#include "HeroAI.h"
#include <GWCA/Utilities/Hooker.h>

#include "Headers.h"

Timer synchTimer;
Timer ScatterRest;

Timer Pathingdisplay;


enum combatParams { unknown, skilladd, skillremove };
combatParams CombatTextstate;


float ScatterPos[8] = {0.0f, 45.0f, -45.0f, 90.0f, -90.0f, 135.0f, -135.0f, 180.0f };

GlobalMouseClass GlobalMouse;


ExperimentalFlagsClass Experimental;
float RangedRangeValue = GW::Constants::Range::Spellcast;
float MeleeRangeValue = GW::Constants::Range::Spellcast;
 
float Agentradius = 80.0f;

GW::Vec3f PlayerPos;
int CandidateUniqueKey = -1;

bool TickFirstTime = false;

GW::Vec3f wolrdPos;


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
    const int x = GET_X_LPARAM(lParam);
    const int y = GET_Y_LPARAM(lParam);

    GlobalMouse.SetMouseCoords(x, y);

    return false;
}


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

    return false;
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

namespace GW {
    typedef struct {
        GamePos pos;
        const PathingTrapezoid* t;
    } PathPoint;

    //typedef void(__cdecl* FindPath_pt)(void* MapCtxSub1, PathPoint* start, PathPoint* goal, int unk, uint32_t maxCount, uint32_t* count, PathPoint* pathArray);
    //typedef void(__cdecl* FindPath_pt)(void* MapCtxSub1, PathPoint* start, PathPoint* goal, float unk, uint32_t maxCount, uint32_t* count, PathPoint* pathArray); //typedef void(__cdecl* FindPath_pt)(void* MapCtxSub1, GamePos* start, GamePos* goal, float unk, uint32_t maxCount, uint32_t* count, PathPoint* pathArray);
    typedef void(__cdecl* FindPath_pt)(PathPoint* start, PathPoint* goal, float range, uint32_t maxCount, uint32_t* count, PathPoint* pathArray);
    static FindPath_pt FindPath_Func = nullptr;

    static void InitPathfinding() {
        //FindPath_Func = (FindPath_pt)GW::Scanner::Find("\x50\x8b\x45\x14\xff\x75\x08\xff\x70\x74", "xxxxxxxxxx", -0x12a);
        //FindPath_Func = (FindPath_pt)GW::Scanner::Find("\x83\xc4\x14\x8d\x77\x30\x90\x83\x7e\x0c\x00", "xxxxxxxxxx", -0xd9);
        //FindPath_Func = (FindPath_pt)GW::Scanner::Find("\x83\xec\x20\x53\x8b\x5d\x1c\x56\x57\xe8\x2f\x4b", "xxxxxxxxxxxx", -0x3);
        FindPath_Func = (FindPath_pt)GW::Scanner::Find("\x83\xec\x20\x53\x8b\x5d\x1c\x56\x57\xe8", "xxxxxxxxxx", -0x3);
        if (!FindPath_Func) {
            //todo: handle error
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
    Pathingdisplay.start();
    GW::InitPathfinding();

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

    GW::Chat::CreateCommand(L"aicollision", [](GW::HookStatus*, const wchar_t*, const int, const LPWSTR*) {
        GW::GameThread::Enqueue([] {
            GameState.state.toggleCollision();
            WriteChat(GW::Chat::CHANNEL_GWCA1, GameState.state.isCollisionEnabled() ? L"AI Collision Enabled" : L"AI Collision Disabled", L"HeroAI");
        });
    });

    GW::Chat::CreateCommand(L"aihero", [](GW::HookStatus*, const wchar_t*, const int, const LPWSTR*) {
        GW::GameThread::Enqueue([] {
            GameState.state.toggleFollowing();
            GameState.state.toggleLooting();
            GameState.state.toggleCombat(); 
            GameState.state.toggleTargetting();
            GameState.state.toggleCollision();

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
            WriteChat(GW::Chat::CHANNEL_GWCA1, L"/aiocollision - Will toggle Collision boxes", L"HeroAI");
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

int elapsedCycles = 0;

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
        GameState.state.Collision   = MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.Collision;
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
        Agentradius = MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.CollisionBubble;

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
    if (++elapsedCycles > 10000) {
        elapsedCycles = 0;
    };
    MemMgr.MutexAquire();
    if ((GameVars.Party.SelfPartyNumber == 1) && (elapsedCycles >= 50)/*(synchTimer.hasElapsed(500))*/) {
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
        //synchTimer.reset();
        elapsedCycles = 0;
    }
    MemMgr.MutexRelease();


    //SELF SUBSCRIBE
    MemMgr.MutexAquire();
    MemMgr.MemData->MemPlayers[GameVars.Party.SelfPartyNumber - 1].ID = GameVars.Target.Self.ID;
    MemMgr.MemData->MemPlayers[GameVars.Party.SelfPartyNumber - 1].Energy = GameVars.Target.Self.Living->energy;
    MemMgr.MemData->MemPlayers[GameVars.Party.SelfPartyNumber - 1].energyRegen = GameVars.Target.Self.Living->energy_regen;
    MemMgr.MemData->MemPlayers[GameVars.Party.SelfPartyNumber - 1].IsActive = true;
    ++MemMgr.MemData->MemPlayers[GameVars.Party.SelfPartyNumber - 1].Keepalivecounter;
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

    GameVars.Target.Nearest.ID = 0;
    GameVars.Target.Nearest.ID = TargetNearestEnemy();
    if (GameVars.Target.Nearest.ID > 0) {
        GameVars.Target.Nearest.Agent = GW::Agents::GetAgentByID(GameVars.Target.Nearest.ID);
        GameVars.Target.Nearest.Living = GameVars.Target.Nearest.Agent->GetAsAgentLiving();
    }

    GameVars.Target.NearestCaster.ID = 0;
    GameVars.Target.NearestCaster.ID = TargetNearestEnemyCaster(false); 
    if (GameVars.Target.NearestCaster.ID != 0) {
        GameVars.Target.NearestCaster.Agent = GW::Agents::GetAgentByID(GameVars.Target.NearestCaster.ID);
        GameVars.Target.NearestCaster.Living = GameVars.Target.NearestCaster.Agent->GetAsAgentLiving();
    }

    GameVars.Target.NearestMartial.ID = 0;
    GameVars.Target.NearestMartial.ID = TargetNearestEnemyMartial(false);
    if (GameVars.Target.NearestMartial.ID != 0) {
        GameVars.Target.NearestMartial.Agent = GW::Agents::GetAgentByID(GameVars.Target.NearestMartial.ID);
        GameVars.Target.NearestMartial.Living = GameVars.Target.NearestMartial.Agent->GetAsAgentLiving();
    }

    GameVars.Target.NearestMartialMelee.ID = 0;
    GameVars.Target.NearestMartialMelee.ID = TargetNearestEnemyMelee(false);
    if (GameVars.Target.NearestMartialMelee.ID != 0) {
        GameVars.Target.NearestMartialMelee.Agent = GW::Agents::GetAgentByID(GameVars.Target.NearestMartialMelee.ID);
        GameVars.Target.NearestMartialMelee.Living = GameVars.Target.NearestMartialMelee.Agent->GetAsAgentLiving();
    }

    GameVars.Target.NearestMartialRanged.ID = 0;
    GameVars.Target.NearestMartialRanged.ID = TargetNearestEnemyRanged(false);
    if (GameVars.Target.NearestMartialRanged.ID != 0) {
        GameVars.Target.NearestMartialRanged.Agent = GW::Agents::GetAgentByID(GameVars.Target.NearestMartialRanged.ID);
        GameVars.Target.NearestMartialRanged.Living = GameVars.Target.NearestMartialRanged.Agent->GetAsAgentLiving();
    }

    GameVars.Party.InAggro = (GameVars.Target.Nearest.ID > 0) ? true : false;

    GameVars.Target.Called.ID = 0;
    GameVars.Target.Called.ID = GameVars.Party.PartyInfo->players[0].calledTargetId;
    if (GameVars.Target.Called.ID != 0) {
        GameVars.Target.Called.Agent = GW::Agents::GetAgentByID(GameVars.Target.Called.ID);
        GameVars.Target.Called.Living = GameVars.Target.Called.Agent->GetAsAgentLiving();
    }

    GameVars.Target.LowestAlly.ID = 0;
    GameVars.Target.LowestAlly.ID = TargetLowestAlly(false);
    if (GameVars.Target.LowestAlly.ID != 0) {
        GameVars.Target.LowestAlly.Agent = GW::Agents::GetAgentByID(GameVars.Target.LowestAlly.ID);
        GameVars.Target.LowestAlly.Living = GameVars.Target.LowestAlly.Agent->GetAsAgentLiving();
    }

    GameVars.Target.LowestOtherAlly.ID = 0;
    GameVars.Target.LowestOtherAlly.ID = TargetLowestAlly(true);
    if (GameVars.Target.LowestOtherAlly.ID != 0) {
        GameVars.Target.LowestOtherAlly.Agent = GW::Agents::GetAgentByID(GameVars.Target.LowestOtherAlly.ID);
        GameVars.Target.LowestOtherAlly.Living = GameVars.Target.LowestOtherAlly.Agent->GetAsAgentLiving();
    }

    GameVars.Target.LowestEnergyOtherAlly.ID = 0;
    GameVars.Target.LowestEnergyOtherAlly.ID = TargetLowestAllyEnergy(true);
    if (GameVars.Target.LowestEnergyOtherAlly.ID != 0) {
        GameVars.Target.LowestEnergyOtherAlly.Agent = GW::Agents::GetAgentByID(GameVars.Target.LowestEnergyOtherAlly.ID);
        GameVars.Target.LowestEnergyOtherAlly.Living = GameVars.Target.LowestEnergyOtherAlly.Agent->GetAsAgentLiving();
    }

    GameVars.Target.LowestAllyCaster.ID = 0;
    GameVars.Target.LowestAllyCaster.ID = TargetLowestAllyCaster(false);
    if (GameVars.Target.LowestAllyCaster.ID != 0) {
        GameVars.Target.LowestAllyCaster.Agent = GW::Agents::GetAgentByID(GameVars.Target.LowestAllyCaster.ID);
        GameVars.Target.LowestAllyCaster.Living = GameVars.Target.LowestAllyCaster.Agent->GetAsAgentLiving();
    }

    GameVars.Target.LowestAllyMartial.ID = 0;
    GameVars.Target.LowestAllyMartial.ID = TargetLowestAllyMartial(false);
    if (GameVars.Target.LowestAllyMartial.ID != 0) {
        GameVars.Target.LowestAllyMartial.Agent = GW::Agents::GetAgentByID(GameVars.Target.LowestAllyMartial.ID);
        GameVars.Target.LowestAllyMartial.Living = GameVars.Target.LowestAllyMartial.Agent->GetAsAgentLiving();
    }

    GameVars.Target.LowestAllyMartialMelee.ID = 0;
    GameVars.Target.LowestAllyMartialMelee.ID = TargetLowestAllyMelee(false);
    if (GameVars.Target.LowestAllyMartialMelee.ID != 0) {
        GameVars.Target.LowestAllyMartialMelee.Agent = GW::Agents::GetAgentByID(GameVars.Target.LowestAllyMartialMelee.ID);
        GameVars.Target.LowestAllyMartialMelee.Living = GameVars.Target.LowestAllyMartialMelee.Agent->GetAsAgentLiving();
    }

    GameVars.Target.LowestAllyMartialRanged.ID = 0;
    GameVars.Target.LowestAllyMartialRanged.ID = TargetLowestAllyRanged(false);
    if (GameVars.Target.LowestAllyMartialRanged.ID != 0) {
        GameVars.Target.LowestAllyMartialRanged.Agent = GW::Agents::GetAgentByID(GameVars.Target.LowestAllyMartialRanged.ID);
        GameVars.Target.LowestAllyMartialRanged.Living = GameVars.Target.LowestAllyMartialRanged.Agent->GetAsAgentLiving();
    }

    GameVars.Target.DeadAlly.ID = 0;
    GameVars.Target.DeadAlly.ID = TargetDeadAllyInAggro();
    if (GameVars.Target.DeadAlly.ID != 0) {
        GameVars.Target.DeadAlly.Agent = GW::Agents::GetAgentByID(GameVars.Target.DeadAlly.ID);
        GameVars.Target.DeadAlly.Living = GameVars.Target.DeadAlly.Agent->GetAsAgentLiving();
    }

    GameVars.Target.NearestSpirit.ID = 0;
    GameVars.Target.NearestSpirit.ID = TargetNearestSpirit();
    if (GameVars.Target.NearestSpirit.ID != 0) {
        GameVars.Target.NearestSpirit.Agent = GW::Agents::GetAgentByID(GameVars.Target.NearestSpirit.ID);
        GameVars.Target.NearestSpirit.Living = GameVars.Target.NearestSpirit.Agent->GetAsAgentLiving();
    }

    GameVars.Target.LowestMinion.ID = 0;
    GameVars.Target.LowestMinion.ID = TargetLowestMinion();
    if (GameVars.Target.LowestMinion.ID != 0) {
        GameVars.Target.LowestMinion.Agent = GW::Agents::GetAgentByID(GameVars.Target.LowestMinion.ID);
        GameVars.Target.LowestMinion.Living = GameVars.Target.LowestMinion.Agent->GetAsAgentLiving();
    }

    GameVars.Target.NearestCorpse.ID = 0;
    GameVars.Target.NearestCorpse.ID = TargetNearestCorpse();
    if (GameVars.Target.NearestCorpse.ID != 0) {
        GameVars.Target.NearestCorpse.Agent = GW::Agents::GetAgentByID(GameVars.Target.NearestCorpse.ID);
        GameVars.Target.NearestCorpse.Living = GameVars.Target.NearestCorpse.Agent->GetAsAgentLiving();
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

    for (int i = 0; i < 8; i++) { if ((!ptr_chk[i]) && (GameVars.SkillBar.Skills[i].CustomData.Nature == Buff)) { GameVars.SkillBar.SkillOrder[ptr] = i; ptr_chk[i] = true; ptr++; } }

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
float previousdistance;
uint32_t timeoutagentID;
bool PartyfirstTime = false;


bool HeroAI::IsColliding(const GW::GamePos& position, const GW::GamePos& agentpos) {
    // Calculate the boundaries of the squares based on their centers
    float halfSize = GridSize / 2.0f;

    // Boundaries for the first square
    float left1 = position.x - halfSize;
    float right1 = position.x + halfSize;
    float top1 = position.y - halfSize;
    float bottom1 = position.y + halfSize;

    // Boundaries for the second square
    float left2 = agentpos.x - halfSize;
    float right2 = agentpos.x + halfSize;
    float top2 = agentpos.y - halfSize;
    float bottom2 = agentpos.y + halfSize;

    // Check for overlap between the two squares
    return !(left1 > right2 || right1 < left2 || top1 > bottom2 || bottom1 < top2);
}



bool HeroAI::IsPositionOccupied(float x, float y) {
    const auto agents = GW::Agents::GetAgentArray();

    for (const GW::Agent* agent : *agents)
    {
        if (!agent) { continue; }
        if (!agent->GetIsLivingType()) { continue; }
        const auto tAllyLiving = agent->GetAsAgentLiving();
        if (!tAllyLiving) { continue; }
        switch (tAllyLiving->allegiance) {
        case GW::Constants::Allegiance::Ally_NonAttackable:
        case GW::Constants::Allegiance::Enemy:
        case GW::Constants::Allegiance::Spirit_Pet:
            break;
        default:
            continue;
            break;
        }
        const auto AllyID = tAllyLiving->agent_id;
        if (AllyID != GameVars.Target.Self.ID) {
            GW::GamePos pos;
            pos.x = x;
            pos.y = y;
            if (IsColliding(pos, tAllyLiving->pos)) {
                return true;
            }
        }
    }
    return false;
}

// Function to get the center of a square based on grid coordinates
GW::Vec3f HeroAI::GetSquareCenter(int q, int r) {
    float centerX = q * GridSize;
    float centerY = r * GridSize;
    return GW::Vec3f(centerX, centerY, 0.0f);// Altitude to be handled in drawing;
}


void HeroAI::DrawSquare(ImDrawList* drawList, int q, int r, ImU32 color, float thickness) {
    auto center = GetSquareCenter(q, r);
    float halfSize = GridSize / 2.0f;

    // Vertices of the square in local coordinates
    std::vector<GW::Vec3f> vertices = {
        {center.x - halfSize, center.y - halfSize, 0.0f},
        {center.x + halfSize, center.y - halfSize, 0.0f},
        {center.x + halfSize, center.y + halfSize, 0.0f},
        {center.x - halfSize, center.y + halfSize, 0.0f}
    };

    // Query altitude for each vertex and adjust Z-coordinate
    for (auto& vertex : vertices) {
        float altitude = 0;
        GW::Map::QueryAltitude({ vertex.x, vertex.y, 0 }, 10, altitude);
        vertex.z = altitude;  // Set the altitude for each vertex
    }

    // Draw the edges of the square
    for (size_t i = 0; i < vertices.size(); ++i) {
        size_t next = (i + 1) % vertices.size();
        Overlay.DrawLine3D(drawList, vertices[i], vertices[next], color, thickness);
    }
}

// Function to check if a square's sides are valid
bool HeroAI::IsSquareValid(int q, int r) {
    auto center = GetSquareCenter(q, r);
    float halfSize = GridSize / 2.0f;

    // Check the center
    float centerDirectDistance = GW::GetDistance(GameVars.Target.Self.Living->pos, { center.x, center.y });
    if (centerDirectDistance > 4500) { return false; }  // If distance is too far, skip

    float centerCalculatedDistance = CalculatePathDistance(center.x, center.y, nullptr, false);
    if (centerCalculatedDistance < centerDirectDistance) { return false; }
    if ((centerCalculatedDistance - centerDirectDistance) > 50.0f) { return false; }

    // Check the corners
    std::vector<GW::Vec3f> corners = {
        { center.x - halfSize, center.y - halfSize, 0.0f },
        { center.x + halfSize, center.y - halfSize, 0.0f },
        { center.x + halfSize, center.y + halfSize, 0.0f },
        { center.x - halfSize, center.y + halfSize, 0.0f }
    };
    /*
    for (const auto& corner : corners) {
        float cornerDirectDistance = GW::GetDistance(GameVars.Target.Self.Living->pos, { corner.x, corner.y });
        if (cornerDirectDistance > 4500) { return false; }  // If distance is too far, skip

        float cornerCalculatedDistance = CalculatePathDistance(corner.x, corner.y, nullptr, false);
       
        if (cornerCalculatedDistance < cornerDirectDistance) { return false; }
        if ((cornerCalculatedDistance - cornerDirectDistance) > 50.0f) { return false; }
        
    }*/

    return true;
}

// Function to check if a square is occupied by an agent
bool HeroAI::IsSquareOccupied(int q, int r) {
    auto center = GetSquareCenter(q, r);
    float halfSize = GridSize / 2.0f;
    GW::Vec3f topLeft(center.x - halfSize, center.y - halfSize, 0.0f);
    GW::Vec3f topRight(center.x + halfSize, center.y - halfSize, 0.0f);
    GW::Vec3f bottomLeft(center.x - halfSize, center.y + halfSize, 0.0f);
    GW::Vec3f bottomRight(center.x + halfSize, center.y + halfSize, 0.0f);

    return HeroAI::IsPositionOccupied(center.x, center.y); // This should be adjusted to properly check the whole square
}


void HeroAI::RenderGridAroundPlayer(ImDrawList* drawList) {
    float size = 80.0f;  // Side length of the square
    float viewRadius = 400.0f;  // Radius around the player to draw the grid

    // Calculate grid bounds around the player
    int minQ = static_cast<int>((GameVars.Target.Self.Living->x - viewRadius) / size) - 1;
    int maxQ = static_cast<int>((GameVars.Target.Self.Living->x + viewRadius) / size) + 1;
    int minR = static_cast<int>((GameVars.Target.Self.Living->y - viewRadius) / size) - 1;
    int maxR = static_cast<int>((GameVars.Target.Self.Living->y + viewRadius) / size) + 1;

    // Draw squares within the calculated bounds
    for (int q = minQ; q <= maxQ; ++q) {
        for (int r = minR; r <= maxR; ++r) {
            // Check if the square is valid and occupied
            if (IsSquareValid(q, r) && !IsSquareOccupied(q, r)) {
                // Draw the square at the calculated position
                DrawSquare(drawList, q, r, ImColor(255, 255, 255), 1.0f); // Example color and thickness
            }
        }
    }
}

void HeroAI::MoveAway() {
    const auto agents = GW::Agents::GetAgentArray();
    float currentX = GameVars.Target.Self.Living->x;
    float currentY = GameVars.Target.Self.Living->y;

    // Determine the closest colliding agent
    float closestDistance = std::numeric_limits<float>::max();
    float closestDirectionX = 0.0f;
    float closestDirectionY = 0.0f;

    for (const GW::Agent* agent : *agents) {
        if (!agent) continue;
        if (!agent->GetIsLivingType()) continue;
        const auto tAllyLiving = agent->GetAsAgentLiving();
        if (!tAllyLiving) continue;

        if (tAllyLiving->agent_id != GameVars.Target.Self.ID && IsColliding(GameVars.Target.Self.Living->pos, tAllyLiving->pos)) {
            // Calculate the direction vector away from the colliding agent
            float deltaX = currentX - tAllyLiving->pos.x;
            float deltaY = currentY - tAllyLiving->pos.y;
            float distance = std::sqrt(deltaX * deltaX + deltaY * deltaY);

            if (distance == 0) {
                deltaX = 1.0f;
                deltaY = 0.0f;
                distance = 1.0f; // Set a non-zero distance to avoid division by zero
            }

            if (distance < closestDistance) {
                closestDistance = distance;
                closestDirectionX = deltaX;
                closestDirectionY = deltaY;
            }
        }
    }

    if (closestDirectionX != 0.0f || closestDirectionY != 0.0f) {
        float length = std::sqrt(closestDirectionX * closestDirectionX + closestDirectionY * closestDirectionY);

        if (length > 0) {
            closestDirectionX /= length;
            closestDirectionY /= length;
        }

        // Move by the exact distance needed to be out of collision range
        float newX = currentX + closestDirectionX * (GridSize + 10.0f); // Adjust the distance as needed
        float newY = currentY + closestDirectionY * (GridSize + 10.0f);

        // Check if the new position is occupied
        if (!HeroAI::IsPositionOccupied(newX, newY)) {
            GW::Agents::Move(newX, newY);
        }
        else {
            // Try to find another position around the target position
            for (int i = 1; i <= 100; ++i) {
                float angle = i * (2 * 3.141592654f / 100); // 100 positions around the target
                float offsetX = std::cos(angle) * (GridSize + 10.0f);
                float offsetY = std::sin(angle) * (GridSize + 10.0f);
                newX = currentX + offsetX;
                newY = currentY + offsetY;
                if (!HeroAI::IsPositionOccupied(newX, newY)) {
                    GW::Agents::Move(newX, newY);
                    break;
                }
            }
        }
    }
}

float HeroAI::CalculatePathDistance(float targetX, float targetY, ImDrawList* drawList, bool draw) {
    using namespace GW;

        PathPoint pathArray[30];
        uint32_t pathCount = 30;
        PathPoint start{ GameVars.Target.Self.Living->pos, nullptr };
                    

        GamePos pos;
        pos.x = targetX;
        pos.y = targetY;
        pos.zplane = GameVars.Target.Self.Living->pos.zplane;
        PathPoint goal{ pos,nullptr };

        if (GetSquareDistance(GameVars.Target.Self.Living->pos, goal.pos) > 4500 * 4500) {return -1; }
            
        if (!FindPath_Func) { return -2; }
            
        FindPath_Func(&start, &goal, 4500.0f, pathCount, &pathCount, pathArray);

        float altitude = 0;
        GamePos g1 = pathArray[0].pos;
        GW::Map::QueryAltitude(GameVars.Target.Self.Living->pos, 10, altitude);
        Vec3f p0{ GameVars.Target.Self.Living->pos.x, GameVars.Target.Self.Living->pos.y, altitude };
            
        Vec3f p1{ g1.x, g1.y, altitude };

        if (draw) { Overlay.DrawLine3D(drawList, p0, p1, IM_COL32(0, 255, 0, 127), 3); }
            

        float totalDistance = GetDistance(GameVars.Target.Self.Living->pos, pathArray[0].pos);

        for (uint32_t i = 1; i < pathCount; ++i) {
            p0 = p1;
            p1.x = pathArray[i].pos.x;
            p1.y = pathArray[i].pos.y;
            GW::Map::QueryAltitude(g1, 10, altitude);

            if (draw) { Overlay.DrawLine3D(drawList, p0, p1, IM_COL32(0, 255, 0, 127), 3); }
            totalDistance += GetDistance(p0, p1);
            }


        return totalDistance;

}


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

    if (loottimeout.isRunning()) {
        double elapsed = loottimeout.getElapsedTime();
        std::wstring strElapsed = std::to_wstring(elapsed);

        // Check if the timeout has elapsed more than 2 seconds
        if (elapsed > 2000) {
            float currentdistance = GW::GetDistance(GameVars.Target.Self.Living->pos, timeoutitempos);

            // Check if the agent is either stuck or moving farther away from the item
            if (currentdistance >= timeoutdistance) {
                GW::Agents::Move(GameVars.Party.LeaderLiving->pos.x, GameVars.Party.LeaderLiving->pos.y);
                GameState.state.Looting = false;
                loottimeout.stop();
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"There's a problem looting, halting function", L"Gwtoolbot");
            }
            else {
                // Update the previous distance and continue looting attempt
                timeoutdistance = currentdistance;
            }
        }
    }

    if ((GameState.state.isLootingEnabled()) &&
        (!GameVars.Party.InAggro) &&
        (ThereIsSpaceInInventory()) &&
        (!loottimeout.isRunning()) &&
        (tItem != 0)
        ) {

        //loottimeout.start();
        timeoutagentID = tItem;
        GW::Agent* itemagent = GW::Agents::GetAgentByID(tItem);
        timeoutitempos = itemagent->pos;
        timeoutdistance = GW::GetDistance(GameVars.Target.Self.Living->pos, timeoutitempos);
        GW::Agents::InteractAgent(GW::Agents::GetAgentByID(tItem), false);
        previousdistance = timeoutdistance;  // Track initial distance
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
            //posx = ((GW::Constants::Range::Touch < Agentradius) ? GW::Constants::Range::Touch + 10 : Agentradius + 10)*rotcos;
            //posy = ((GW::Constants::Range::Touch < Agentradius) ? GW::Constants::Range::Touch + 10 : Agentradius + 10)*rotsin;
            posx = (GW::Constants::Range::Touch + 10) * rotcos;
            posy = (GW::Constants::Range::Touch + 10) * rotsin;
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

    /*************** COLLISION ****************************/
    
    if ((GameState.state.isCollisionEnabled()) &&
        (!GameVars.Target.Self.Living->GetIsCasting()) &&
        (GameVars.Party.SelfPartyNumber > 1) &&
        //(GameVars.Party.InAggro) &&
        (!IsMelee(GameVars.Target.Self.ID)) &&
        //(!FalseLeader) &&
        //((TargetToFollow.DistanceFromLeader <= (GW::Constants::Range::Adjacent))) &&
        (ScatterRest.getElapsedTime() > 500)
        //Need to include checks for melee chars
        ) {

        /////////// COLLISSION BUBBLE //////////////////

        if (IsPositionOccupied(GameVars.Target.Self.Living->x, GameVars.Target.Self.Living->y)) {
            MoveAway();
        }

        /////////// END COLLISSION BUBLE ///////////////

        ScatterRest.reset();
        return;
    }
    

    /***************** END COLLISION ************************/

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


void HeroAI::Draw(IDirect3DDevice9*)
{
    if (GameVars.Map.IsLoading) { return; }
    if (!GameVars.Map.IsPartyLoaded) { return; }
    
    Overlay.BeginDraw();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    
    const auto& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);

    ////////////// MAIN WINDOW ///////////////////////
    MemMgr.MutexAquire();
    if (ImGui::Begin(Name(), can_close && show_closebutton ? GetVisiblePtr() : nullptr, GetWinFlags(ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize))) {
        ImGui::BeginTable("controltable", 6);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (Overlay.ToggleButton(ICON_FA_RUNNING "", &GameState.state.Following, 30, 30, (GameVars.Party.SelfPartyNumber == 1) ? false : true)) 
            { MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.Following = !MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.Following; };
            Overlay.ShowTooltip("Follow");
            ImGui::TableNextColumn();
            if (Overlay.ToggleButton(ICON_FA_PODCAST"", &GameState.state.Collision, 30, 30, (GameVars.Party.SelfPartyNumber == 1) ? false : true))
            { MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.Collision = !MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.Collision; };
            Overlay.ShowTooltip("Collision");
            ImGui::TableNextColumn();
            if (Overlay.ToggleButton(ICON_FA_COINS"", &GameState.state.Looting, 30, 30, true)) 
            { MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.Looting = !MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.Looting; };
            Overlay.ShowTooltip("Loot");
            ImGui::TableNextColumn();
            if (Overlay.ToggleButton(ICON_FA_BULLSEYE"", &GameState.state.Targetting, 30, 30, true)) 
            { MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.Targetting = !MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.Targetting; };
            Overlay.ShowTooltip("Target");
            ImGui::TableNextColumn();
            if (Overlay.ToggleButton(ICON_FA_SKULL_CROSSBONES"", &GameState.state.Combat, 30, 30, true)) 
            { MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.Combat = !MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].state.Combat; };
            Overlay.ShowTooltip("Combat");
            ImGui::TableNextColumn();
            if (Overlay.ToggleButton(ICON_FA_BUG"", &Experimental.Debug, 30, 30, true)) {};
            Overlay.ShowTooltip("Debug Options");
        ImGui::EndTable();

        ImGui::Separator();

        ImGui::BeginTable("skills", 8);
        for (int i = 0; i < 8; ++i) {
            ImGui::TableNextColumn();
            if (Overlay.ToggleButton(std::to_string(i + 1).c_str(), &GameState.CombatSkillState[i], 20, 20, true)) 
            { MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].CombatSkillState[i] = !MemMgr.MemData->GameState[GameVars.Party.SelfPartyNumber - 1].CombatSkillState[i]; }
        }
        ImGui::EndTable();

        Overlay.mainWindowPos = ImGui::GetWindowPos();
        Overlay.mainWindowSize = ImGui::GetWindowSize();
    }
    ImGui::End();
    MemMgr.MutexRelease();
    ////////////// END MAIN WINDOW ///////////////////////

    ///////////// UTILITIES //////////////////////////

    Experimental.CreateParty = (GW::PartyMgr::GetPartySize() > 1) ? true : false;
    
    if ((GameVars.Party.SelfPartyNumber == 1))
    {
        ImVec2 main_window_pos = Overlay.mainWindowPos;
        ImVec2 main_window_size = Overlay.mainWindowSize;

        ImVec2 new_window_pos = ImVec2(main_window_pos.x + main_window_size.x, main_window_pos.y);
        ImGui::SetNextWindowPos(new_window_pos, ImGuiCond_FirstUseEver);

        ImGui::Begin("Utilities", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        if (!ImGui::IsWindowCollapsed())
        {                          
            /////////////////// PARTY SETUP ////////////////////////
            if (GameVars.Map.IsOutpost) {
                if (ImGui::CollapsingHeader("Party Setup"))
                {
                    ImGui::BeginTable("USetuptable", 2);
                    ImGui::TableSetupColumn("Invite");
                    ImGui::TableSetupColumn("Candidate");
                    ImGui::TableHeadersRow();

                    Scandidates candidateSelf;

                    if (GameVars.Target.Self.Living)
                    {
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
                            if (candidates[i].player_ID > 0) { // Check if the slot is not empty

                                bool SameMapId = (candidateSelf.MapID == candidates[i].MapID);
                                bool SameMapRegion = (candidateSelf.MapRegion == candidates[i].MapRegion);
                                bool SameMapDistrict = (candidateSelf.MapDistrict == candidates[i].MapDistrict);
                                bool SamePlayerID = (candidateSelf.player_ID == candidates[i].player_ID);
                                bool ExistsInParty = existsInParty(candidates[i].player_ID);
                                //bool ExistsInParty = false;

                                if (SameMapId && SameMapRegion && SameMapDistrict && !SamePlayerID && !ExistsInParty)
                                {
                                    std::string playername("Not Accessible");
                                    playername = WStringToString(GW::PlayerMgr::GetPlayerName(candidates[i].player_ID));

                                    ImGui::TableNextRow();
                                    ImGui::TableSetColumnIndex(0);
                                    auto player = GW::PlayerMgr::GetPlayerByID(candidates[i].player_ID);
                                    std::string buttonName = ICON_FA_USERS + playername + std::to_string(i);

                                    if (Overlay.ToggleButton(buttonName.c_str(), &Experimental.CreateParty, 30, 30, true)) { GW::PartyMgr::InvitePlayer(player->name); };
                                    Overlay.ShowTooltip(playername.c_str());

                                    ImGui::TableSetColumnIndex(1);
                                    ImGui::Text(playername.c_str());

                                }
                            }
                        }
                    }
                    ImGui::EndTable();
                }
                ImGui::Separator();
            }
            /////////////////// END PARTY SETUP ////////////////////////
            
    
            /////////////////// FLAGGING & CONTROL ////////////////////
            if (GameVars.Map.IsExplorable) {
                if (ImGui::CollapsingHeader("Control"))
                {
                    ImGui::BeginTable("UControlTable", 4);
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    if (Overlay.ToggleButton(ICON_FA_MAP_MARKER_ALT"", &GameState.state.HeroFlag, 30, 30, ((GameVars.Party.SelfPartyNumber == 1) && (GameVars.Map.IsExplorable)) ? true : false)) { /*do stuff*/ };
                    Overlay.ShowTooltip("Flag");
                    ImGui::TableNextColumn();
                    Experimental.ForceFollow = false;
                    if (Overlay.ToggleButton(ICON_FA_LINK"", &Experimental.ForceFollow, 30, 30, ((GameVars.Party.SelfPartyNumber == 1) && (GameVars.Map.IsExplorable)) ? true : false)) {
                        MemMgr.MutexAquire();
                        for (int i = 0; i < 8; i++) {
                            MemMgr.MemData->command[i] = rc_ForceFollow;
                        }
                        MemMgr.MutexRelease();
                    };
                    Overlay.ShowTooltip("Force Follow");

                    ImGui::EndTable();
                }
                ImGui::Separator();
            }
            /////////////////// END FLAGGING & CONTROL ////////////////////
            
            ///////////////////// MISC TOGGLES //////////////////////////
            if (ImGui::CollapsingHeader("Toggles"))
            {
                ImGui::BeginTable("UToggleTable", 5);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (Overlay.ToggleButton(ICON_FA_BIRTHDAY_CAKE"", &Experimental.Pcons, 30, 30, true)) {
                    MemMgr.MutexAquire();
                    for (int i = 0; i < 8; i++) {
                        MemMgr.MemData->command[i] = rc_Pcons;
                    }
                    MemMgr.MutexRelease();
                };
                Overlay.ShowTooltip("Toggle Pcons");
                ImGui::TableSetColumnIndex(1);
                Experimental.Title = false;
                if (Overlay.ToggleButton(ICON_FA_GRADUATION_CAP"", &Experimental.Title, 30, 30, true)) {
                    MemMgr.MutexAquire();
                    for (int i = 0; i < 8; i++) {
                        MemMgr.MemData->command[i] = rc_Title;
                    }
                    MemMgr.MutexRelease();
                };
                Overlay.ShowTooltip("Title");
                ImGui::TableSetColumnIndex(2);
                Experimental.resign = false;
                if (Overlay.ToggleButton(ICON_FA_WINDOW_CLOSE"", &Experimental.resign, 30, 30, true)) {
                    MemMgr.MutexAquire();
                    for (int i = 0; i < 8; i++) {
                        MemMgr.MemData->command[i] = rc_Resign;
                    }
                    MemMgr.MutexRelease();
                };
                Overlay.ShowTooltip("Resign");
                ImGui::TableSetColumnIndex(3);
                Experimental.TakeQuest = false;
                if (Overlay.ToggleButton(ICON_FA_EXCLAMATION"", &Experimental.TakeQuest, 30, 30, true)) { 
                    MemMgr.MutexAquire();
                    for (int i = 0; i < 8; i++) {
                        MemMgr.MemData->command[i] = rc_GetQuest;
                    }
                    MemMgr.MutexRelease();
                };
                Overlay.ShowTooltip("Get Quest (WIP)");
                ImGui::TableSetColumnIndex(4);
                Experimental.Openchest = false;
                if (Overlay.ToggleButton(ICON_FA_BOX_OPEN"", &Experimental.Openchest, 30, 30, true)) { 
                    MemMgr.MutexAquire();
                    for (int i = 0; i < 8; i++) {
                        MemMgr.MemData->command[i] = rc_Chest;
                    }
                    MemMgr.MutexRelease();
                };
                Overlay.ShowTooltip("Open Chest");
                ImGui::EndTable();
            }
            ///////////////////// END MISC TOGGLES //////////////////////////
            ImGui::Separator();

            //////////////////// CONFIG //////////////////////////////////
            if (ImGui::CollapsingHeader("Config"))
            {
                ImGui::Text("Extended Autonomy On Combat");

                if (Overlay.SelectableComboValues("Melee", &Overlay.MeleeAutonomy, namedValues))
                {
                    const char* selectedName = namedValues[Overlay.MeleeAutonomy].name;
                    float selectedValue = namedValues[Overlay.MeleeAutonomy].value;
                    MemMgr.MutexAquire();
                    for (int i = 0; i < 8; i++) {
                        MemMgr.MemData->GameState[i].state.MeleeRangeValue = namedValues[Overlay.MeleeAutonomy].value;
                    }
                    MemMgr.MutexRelease();
                }
                if (Overlay.SelectableComboValues("Ranged", &Overlay.RangedAutonomy, namedValues))
                {
                    const char* selectedName = namedValues[Overlay.RangedAutonomy].name;
                    float selectedValue = namedValues[Overlay.RangedAutonomy].value;
                    MemMgr.MutexAquire();
                    for (int i = 0; i < 8; i++) {
                        MemMgr.MemData->GameState[i].state.RangedRangeValue = namedValues[Overlay.RangedAutonomy].value;
                    }
                    MemMgr.MutexRelease();
                }
                ImGui::Separator();

                static const float minValue = 0.0f;
                static const float maxValue = GW::Constants::Range::Touch;

                ImGui::SliderFloat("SliderCollission", &Agentradius, minValue, maxValue);
                // Determine the label based on slider value
                const char* label = "Unknown";
                if      (Agentradius <= GW::Constants::Range::Touch * 0.12f) { label = "<Touch 1/8"; }
                else if (Agentradius <= GW::Constants::Range::Touch * 0.16f) { label = "<Touch 1/6"; }
                else if (Agentradius <= GW::Constants::Range::Touch * 0.25f) { label = "<Touch 1/4"; }
                else if (Agentradius <= GW::Constants::Range::Touch * 0.33f) { label = "<Touch 1/3"; }
                else if (Agentradius <= GW::Constants::Range::Touch * 0.50f) { label = "<Touch 1/2"; }
                else if (Agentradius <= GW::Constants::Range::Touch * 0.66f) { label = "<Touch 2/3"; }
                else if (Agentradius <= GW::Constants::Range::Touch * 0.75f) { label = "<Touch 3/4"; }
                else if (Agentradius <= GW::Constants::Range::Touch) { label = "<Touch"; }
                else if (Agentradius <= GW::Constants::Range::Adjacent) { label = "Touch+"; }
                else if (Agentradius <= GW::Constants::Range::Nearby) { label = "Adjacent+"; }
                else if (Agentradius <= GW::Constants::Range::Area) { label = "Nearby+"; }
                else if (Agentradius <= GW::Constants::Range::Earshot) { label = "Area+"; }
                else if (Agentradius <= GW::Constants::Range::Spellcast) { label = "Earshot+"; }
                else if (Agentradius <= GW::Constants::Range::Spirit) { label = "Spellcast+"; }
                else if (Agentradius <= GW::Constants::Range::Compass) { label = "Spirit+"; }
                else { label = "Compass"; }
                ImGui::Text("Current Label: %s", label);
                MemMgr.MutexAquire();
                for (int i = 0; i < 8; i++) {
                    MemMgr.MemData->GameState[i].state.CollisionBubble = Agentradius;
                }
                MemMgr.MutexRelease();
                //GridSize = Agentradius;
            }
            /////////////////////// END CONFIG ////////////////////////////
        }
        ImGui::End();
    }


    ////////////////  FLAG WINDOW //////////////////////////////////

    if ((GameState.state.HeroFlag) && (GameVars.Map.IsExplorable))
    {
        ImVec2 main_window_pos = Overlay.mainWindowPos;
        ImVec2 main_window_size = Overlay.mainWindowSize;

        ImVec2 new_window_pos = ImVec2(main_window_pos.x + main_window_size.x, main_window_pos.y);

        ImGui::SetNextWindowPos(new_window_pos, ImGuiCond_FirstUseEver);
        ImGui::Begin("Flagging", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        if (!ImGui::IsWindowCollapsed())
        {

            ImGui::BeginTable("Flags", 3);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (Overlay.ToggleButton("All", &GameVars.Party.Flags[0], 30, 30, true)) { Flag(0); };
            ImGui::TableSetColumnIndex(1);
            if (Overlay.ToggleButton("1", &GameVars.Party.Flags[1], 30, 30, ((GW::PartyMgr::GetPartySize() < 2) ? false : true))) { Flag(1); };
            ImGui::TableSetColumnIndex(2);
            if (Overlay.ToggleButton("2", &GameVars.Party.Flags[2], 30, 30, ((GW::PartyMgr::GetPartySize() < 3) ? false : true))) { Flag(2); };

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (Overlay.ToggleButton("3", &GameVars.Party.Flags[3], 30, 30, ((GW::PartyMgr::GetPartySize() < 4) ? false : true))) { Flag(3); };
            ImGui::TableSetColumnIndex(1);
            if (Overlay.ToggleButton("4", &GameVars.Party.Flags[4], 30, 30, ((GW::PartyMgr::GetPartySize() < 5) ? false : true))) { Flag(4); };
            ImGui::TableSetColumnIndex(2);
            if (Overlay.ToggleButton("5", &GameVars.Party.Flags[5], 30, 30, ((GW::PartyMgr::GetPartySize() < 6) ? false : true))) { Flag(5); };

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (Overlay.ToggleButton("6", &GameVars.Party.Flags[6], 30, 30, ((GW::PartyMgr::GetPartySize() < 7) ? false : true))) { Flag(6); };
            ImGui::TableSetColumnIndex(1);
            if (Overlay.ToggleButton("7", &GameVars.Party.Flags[7], 30, 30, ((GW::PartyMgr::GetPartySize() < 8) ? false : true))) { Flag(7); };
            ImGui::TableSetColumnIndex(2);
            if (Overlay.ToggleButton("X", &GameVars.Party.Flags[0], 30, 30, true)) { UnflagAllHeroes(); };

            ImGui::EndTable();
        }

        ImGui::End();
    }
    ////////////////// END FLAG WINDOW /////////////////////////////////
    
    //////////////////// CONTROL PANEL /////////////////////////////////

    if ((GameVars.Party.SelfPartyNumber == 1) && (GW::PartyMgr::GetPartyPlayerCount() > 1))
    {

        ImGui::Begin("ControlPanel", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        if (!ImGui::IsWindowCollapsed())
        {
            /////////// CONTROL ALL ///////////////////
            MemMgr.MutexAquire();
            std::string tableName = "MaincontrolPanelTable";
            ImGui::BeginTable(tableName.c_str(), 6);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);        
                if (Overlay.ToggleButton(ICON_FA_RUNNING "", &ControlAll.state.Following, 30, 30, true)) {
                    for (int i = 0; i < 8; i++) { MemMgr.MemData->GameState[i].state.Following = ControlAll.state.Following; }
                }; Overlay.ShowTooltip("Follow All");
                ImGui::TableNextColumn();
                if (Overlay.ToggleButton(ICON_FA_PODCAST"", &ControlAll.state.Collision, 30, 30, true)) {
                    for (int i = 0; i < 8; i++) { MemMgr.MemData->GameState[i].state.Collision = ControlAll.state.Collision; }
                }; Overlay.ShowTooltip("Collision All");
                ImGui::TableNextColumn();
                if (Overlay.ToggleButton(ICON_FA_COINS"", &ControlAll.state.Looting, 30, 30, true)) {
                    for (int i = 0; i < 8; i++) { MemMgr.MemData->GameState[i].state.Looting = ControlAll.state.Looting; }
                }; Overlay.ShowTooltip("Loot All");
                ImGui::TableNextColumn();;
                if (Overlay.ToggleButton(ICON_FA_BULLSEYE"", &ControlAll.state.Targetting, 30, 30, true)) {
                    for (int i = 0; i < 8; i++) { MemMgr.MemData->GameState[i].state.Targetting = ControlAll.state.Targetting; }
                }; Overlay.ShowTooltip("Target All");
                ImGui::TableNextColumn();
                if (Overlay.ToggleButton(ICON_FA_SKULL_CROSSBONES"", &ControlAll.state.Combat, 30, 30, true)) {
                    for (int i = 0; i < 8; i++) { MemMgr.MemData->GameState[i].state.Combat = ControlAll.state.Combat; }
                }; Overlay.ShowTooltip("Combat All");
            ImGui::EndTable();
            MemMgr.MutexRelease();
            
            ImGui::Separator();

            std::string tableskillName = "Maincontrolskill";
            MemMgr.MutexAquire();
            ImGui::BeginTable(tableskillName.c_str(), 8);

            for (int col = 0; col < 8; ++col) {
                ImGui::TableNextColumn();
                if (Overlay.ToggleButton(std::to_string(col + 1).c_str(), &ControlAll.CombatSkillState[col], 20, 20, true)) {
                    for (int i = 0; i < 8; ++i) {
                        MemMgr.MemData->GameState[i].CombatSkillState[col] = ControlAll.CombatSkillState[col];
                    }
                }
            }
         
            ImGui::EndTable();
            MemMgr.MutexRelease();
            ///////////////// END CONTROL ALL /////////////////
            ImGui::Separator();
            //////////////// PARTY CONTROL /////////////////
            if (ImGui::CollapsingHeader("Party")) {
                for (int i = 0; i < 8; i++) {
                    MemMgr.MutexAquire();
                    if (MemMgr.MemData->MemPlayers[i].IsActive) {
                        auto chosen_player_name = GW::Agents::GetPlayerNameByLoginNumber(GameVars.Party.PartyInfo->players[i].login_number);
                        std::string playername = WStringToString(chosen_player_name);
                        if (ImGui::TreeNode(playername.c_str())) {
                            std::string tableName = "controlpaneltable" + std::to_string(i);
                            ImGui::BeginTable(tableName.c_str(), 6);
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            if (Overlay.ToggleButton(ICON_FA_RUNNING "", &MemMgr.MemData->GameState[i].state.Following, 30, 30, (i == 0) ? false : true)) { /* do stuff */ }
                            Overlay.ShowTooltip("Follow");
                            ImGui::TableNextColumn();
                            if (Overlay.ToggleButton(ICON_FA_PODCAST"", &MemMgr.MemData->GameState[i].state.Collision, 30, 30, (i == 0) ? false : true)) { /* do stuff */ }
                            Overlay.ShowTooltip("Collision");
                            ImGui::TableNextColumn();
                            if (Overlay.ToggleButton(ICON_FA_COINS"", &MemMgr.MemData->GameState[i].state.Looting, 30, 30, true)) { /* do stuff */ }
                            Overlay.ShowTooltip("Loot");
                            ImGui::TableNextColumn();
                            if (Overlay.ToggleButton(ICON_FA_BULLSEYE"", &MemMgr.MemData->GameState[i].state.Targetting, 30, 30, true)) { /* do stuff */ }
                            Overlay.ShowTooltip("Target");
                            ImGui::TableNextColumn();
                            if (Overlay.ToggleButton(ICON_FA_SKULL_CROSSBONES"", &MemMgr.MemData->GameState[i].state.Combat, 30, 30, true)) { /* do stuff */ }
                            Overlay.ShowTooltip("Combat");

                            ImGui::EndTable();
                            ImGui::Separator();

                            std::string tableskillName = "controlskill" + std::to_string(i);
                            ImGui::BeginTable(tableskillName.c_str(), 8);
                            ImGui::TableNextRow();

                            for (int j = 0; j < 8; j++) {
                                ImGui::TableSetColumnIndex(j);
                                if (Overlay.ToggleButton(std::to_string(j + 1).c_str(), &MemMgr.MemData->GameState[i].CombatSkillState[j], 20, 20, true)) { /* do stuff */ }
                            }

                            ImGui::EndTable();

                            ImGui::TreePop();
                        }
                    }
                    MemMgr.MutexRelease();
                }
            }
            ////////////////// END PARTY CONTROL//////////////////

        }
        ImGui::End();
    }
    // END CONTROL PANEL


    //******************************   OVERLAY DRAWING *********************************************
    

    ///////////////  DRAW FLAGS  ////////////////////////////
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
    ///////////////// END DRAW FLAGS ///////////////////////////////

    ////////////////// DEBUG WINDOW //////////////////////////////

    if (Experimental.Debug) {

        ImGui::SetNextWindowPos(ImVec2(Overlay.mainWindowPos.x + Overlay.mainWindowSize.x + 10, Overlay.mainWindowPos.y), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(50, 50), ImGuiCond_FirstUseEver);

        ImGui::Begin("Debug", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        if (!ImGui::IsWindowCollapsed())
        {
            ImGui::BeginTable("Debugtable", 3);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (Overlay.ToggleButton("Area Rings", &Experimental.AreaRings, 80, 30, GameVars.Map.IsExplorable)) {};
            ImGui::TableSetColumnIndex(1);
            if (Overlay.ToggleButton("MemMon", &Experimental.MemMon, 80, 30, true)) {};
            ImGui::TableSetColumnIndex(2);
            if (Overlay.ToggleButton("SkillData", &Experimental.SkillData, 80, 30, true)) {};
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (Overlay.ToggleButton("Candidates", &Experimental.PartyCandidates, 80, 30, GameVars.Map.IsOutpost)) {};
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (Overlay.ToggleButton("Pathing", &Experimental.Pathing, 80, 30, GameVars.Map.IsExplorable)) {};
            ImGui::TableSetColumnIndex(1);
            if (Overlay.ToggleButton("Collisions", &Experimental.Collisions, 80, 30, GameVars.Map.IsExplorable)) {};
            ImGui::TableSetColumnIndex(2);
            if (Overlay.ToggleButton("Targetting", &Experimental.TargettingDebug, 80, 30, GameVars.Map.IsExplorable)) {};
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (Overlay.ToggleButton("Formation A", &Experimental.FormationA, 80, 30, GameVars.Map.IsExplorable)) {};
            ImGui::EndTable();
        } //g_totalDistance
        ImGui::End();
    }

    ////////////////  END DEBUG WINDOW //////////////////////////
    ///////////// PARTY CANDIDATE DEBUG WINDOW ////////////////////
    if ((Experimental.PartyCandidates) && (GameVars.Map.IsOutpost)) {

        Scandidates candidateSelf;

        if (GameVars.Target.Self.Living) { candidateSelf.player_ID = GameVars.Target.Self.Living->player_number; }
        else { candidateSelf.player_ID - 1; }
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
            MemMgr.MutexRelease();
            ImGui::Text("Keepalive: %d", counter);
           
            ImGui::Separator();

            MemMgr.MutexAquire();
            Scandidates* candidates = MemMgr.MemData->NewCandidates.ListClients();
            MemMgr.MutexRelease();

            for (int i = 0; i < 8; ++i) {
                if (candidates[i].player_ID > 0) { // Check if the slot is not empty

                    bool SameMapId = (candidateSelf.MapID == candidates[i].MapID);
                    bool SameMapRegion = (candidateSelf.MapRegion == candidates[i].MapRegion);
                    bool SameMapDritrict = (candidateSelf.MapDistrict == candidates[i].MapDistrict);
                    bool SamePlayerID = (candidateSelf.player_ID == candidates[i].player_ID);
                    bool ExistsInParty = existsInParty(candidates[i].player_ID);
                    //bool ExistsInParty = false;

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
        ImGui::End();
    }

    ///////////////////// TACO AREA RINGS //////////////////////////
    if (Experimental.AreaRings) {
        Overlay.DrawPoly3D(drawList, PlayerPos, Agentradius, IM_COL32(255, 0, 255, 255), 360);
        Overlay.DrawPoly3D(drawList, PlayerPos, GW::Constants::Range::Touch, IM_COL32(255, 255, 255, 127), 360);
        Overlay.DrawPoly3D(drawList, PlayerPos, GW::Constants::Range::Adjacent, IM_COL32(255, 255, 255, 127), 360);
        Overlay.DrawPoly3D(drawList, PlayerPos, GW::Constants::Range::Nearby, IM_COL32(255, 255, 255, 127), 360);
        Overlay.DrawPoly3D(drawList, PlayerPos, GW::Constants::Range::Area, IM_COL32(255, 255, 255, 127), 360);
        Overlay.DrawPoly3D(drawList, PlayerPos, GW::Constants::Range::Earshot, IM_COL32(255, 255, 255, 127), 360);

        Overlay.DrawPoly3D(drawList, PlayerPos, MeleeRangeValue, IM_COL32(180, 5, 11, 255), 360);
        Overlay.DrawPoly3D(drawList, PlayerPos, RangedRangeValue, IM_COL32(180, 165, 0, 255), 360);
    }
    ///////////////////// END TACO AREA RINGS //////////////////////////
    ///////////////////// COLLISIONS //////////////////////////
    if ((Experimental.Collisions) && (GameVars.Map.IsExplorable)) {
        const auto agents = GW::Agents::GetAgentArray();
        static GW::AgentID AllyID = 0;
        static GW::AgentID tStoredAllyID = 0;
        float StoredLife = 2.0f;

        for (const GW::Agent* agent : *agents)
        {
            if (!agent) { continue; }
            if (!agent->GetIsLivingType()) { continue; }
            const auto tAllyLiving = agent->GetAsAgentLiving();
            if (!tAllyLiving) { continue; }
            if (!tAllyLiving->GetIsAlive()) { continue; }
           
            auto color = IM_COL32(127, 127, 127, 127);
            if (tAllyLiving->allegiance == GW::Constants::Allegiance::Ally_NonAttackable) { color = IM_COL32(0, 255, 0, 255); } // Green
            if (tAllyLiving->allegiance == GW::Constants::Allegiance::Enemy) { color = IM_COL32(255, 0, 0, 255); } // Red
            if (tAllyLiving->allegiance == GW::Constants::Allegiance::Spirit_Pet) { color = IM_COL32(0, 255, 255, 255); } // Cyan
            
            GW::Vec3f pos;
            pos.x = tAllyLiving->pos.x;
            pos.y = tAllyLiving->pos.y;
            Overlay.DrawPoly3D(drawList, pos, Agentradius, color, 360);

        }

    }
    ///////////////////// END COLLISIONS //////////////////////////
    // //////////////// TARGETTING ///////////////////////////////
     
    if ((Experimental.TargettingDebug) && (GameVars.Map.IsExplorable)) {
        ImGui::SetNextWindowSize(ImVec2(150, 150), ImGuiCond_FirstUseEver);
        ImGui::Begin("Targetting Categories Monitor");

        ImGui::Columns(4, "TargettingCat_player_columns");

        for (int i = 0; i < (GW::PartyMgr::GetPartyPlayerCount()); i++)
        {
            const auto playerId = GW::Agents::GetAgentIdByLoginNumber(GameVars.Party.PartyInfo->players[i].login_number);
            if (!playerId) { continue; }
            const auto agent = GW::Agents::GetAgentByID(playerId);
            if (!agent) { continue; }
            const auto living = agent->GetAsAgentLiving();
            if (!living) { continue; }

            ImGui::Text("Player %d", i);
            ImGui::Text("ID: %d", playerId);
            ImGui::Text("Held WeaponID: %i", living->weapon_type);
            std::string weaponName;
            switch (living->weapon_type) {
            case None:
                weaponName = "None";
                break;
            case bow:
                weaponName = "Bow";
                break;
            case axe:
                weaponName = "Axe";
                break;
            case hammer:
                weaponName = "Hammer";
                break;
            case daggers:
                weaponName = "Daggers";
                break;
            case scythe:
                weaponName = "Scythe";
                break;
            case spear:
                weaponName = "Spear";
                break;
            case sword:
                weaponName = "Sword";
            case scepter:
                weaponName = "Scepter";
                break;
            case staff:
                weaponName = "Staff";
                break;
            case staff2:
                weaponName = "Staff2";
                break;
            case scepter2:
                weaponName = "Scepter2";
                break;
            case staff3:
                weaponName = "Staff3";
                break;
            case staff4:
                weaponName = "Staff4";
                break;
            default:
                weaponName = "UNDEFINED";
                break;
            }

            ImGui::Text("Type: %s", weaponName.c_str());

            Overlay.DrawYesNoText("Is Martial", IsMartial(playerId));
            Overlay.DrawYesNoText("Is Melee", IsMelee(playerId));

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
    /////////////// END TARGETTING //////////////////////////
    //////////////////// PATHING //////////////////////////////
    if ((Experimental.Pathing) && (GameVars.Map.IsExplorable)) {

        ImGui::SetNextWindowSize(ImVec2(150, 150), ImGuiCond_FirstUseEver);
        ImGui::Begin("Pathing Monitor");

        
        if (Pathingdisplay.hasElapsed(100)) {
            wolrdPos = Overlay.ScreenToWorld();
        }
        GW::Vec2f targetpos{ wolrdPos.x, wolrdPos.y };

        if (!wolrdPos.x && !wolrdPos.y && !wolrdPos.z) {
            ImGui::Text("No Intersect with map");
        }
        else {

            ImGui::Text("target pos: (%.4f, %.4f", wolrdPos.x, wolrdPos.y, ")");
            float distance = GW::GetDistance(GameVars.Target.Self.Living->pos, targetpos);
            ImGui::Text("Direct distance: %.4f",distance);
            //if (Pathingdisplay.hasElapsed(1000)) {
            float calcdistance = CalculatePathDistance(targetpos.x, targetpos.y, drawList, true);
            //Pathingdisplay.reset();
        //}
            ImGui::Text("Pathing distance: %.4f", calcdistance);
            //float difference = std::abs((distance - calcdistance) <= 10.0f)
            Overlay.DrawYesNoText("Direct Path", (distance == calcdistance));
            Overlay.DrawYesNoText("Unreachable", (calcdistance < distance));
            Overlay.DrawYesNoText("Overextended", (calcdistance - distance) > 50.0f);
        }
        RenderGridAroundPlayer(drawList);
        ImGui::End();
    }
    //////////////////// END PATHING ////////////////////////
    ///////////////////// TEST FORMATIONS //////////////////////////
    if (Experimental.FormationA) {

        Overlay.DrawDoubleRowLineFormation(drawList, PlayerPos, 0, IM_COL32(255, 255, 0, 255));
    }
    ///////////////////// END TEST FORMATIONS //////////////////////////

    //////////////////// MEMORY MONITOR //////////////////////////////
    if (Experimental.MemMon) {

        ImGui::SetNextWindowSize(ImVec2(150, 150), ImGuiCond_FirstUseEver);
        ImGui::Begin("MemPlayer Monitor");

            ImGui::Columns(4, "mem_player_columns");

            for (int i = 0; i < 8; ++i) {
                MemMgr.MutexAquire();
                const MemMgrClass::MemSinglePlayer& player = MemMgr.MemData->MemPlayers[i];
                MemMgr.MutexRelease();

                ImGui::Text("Player %d", i);
                ImGui::Text("ID: %d", player.ID);
                Overlay.DrawEnergyValue("Energy", player.Energy, 0.4f);
                Overlay.DrawYesNoText("Is Active", player.IsActive);
                ImGui::Text("Keepalive: %d", player.Keepalivecounter);
                Overlay.DrawYesNoText("Is Hero", player.IsHero);
                Overlay.DrawYesNoText("Is Flagged", player.IsFlagged);
                Overlay.DrawYesNoText("Is All Flag", player.IsAllFlag);

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
    //////////////////// END MEMORY MONITOR ////////////////////////

    /////////////////// SKILL MONITOR ////////////////////////////
    if ((Experimental.SkillData) && (GameVars.Map.IsExplorable) && (GameVars.Target.Self.ID)) {

        ImGui::SetNextWindowSize(ImVec2(150, 150), ImGuiCond_FirstUseEver);
        ImGui::Begin("SkillDataMonitor");

        ImGui::Columns(4, "skill_mon_columns");

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
            Overlay.DrawYesNoText("Custom Data", skill.HasCustomData);
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
                    Overlay.DrawYesNoText("IsAlive", skill.CustomData.Conditions.IsAlive);
                        Overlay.DrawYesNoText("HasCondition", skill.CustomData.Conditions.HasCondition);
                        Overlay.DrawYesNoText("HasDeepWound", skill.CustomData.Conditions.HasDeepWound);
                        Overlay.DrawYesNoText("HasCrippled", skill.CustomData.Conditions.HasCrippled);
                        Overlay.DrawYesNoText("HasBleeding", skill.CustomData.Conditions.HasBleeding);
                        Overlay.DrawYesNoText("HasPoison", skill.CustomData.Conditions.HasPoison);
                        Overlay.DrawYesNoText("HasWeaponSpell", skill.CustomData.Conditions.HasWeaponSpell);
                        Overlay.DrawYesNoText("HasEnchantment", skill.CustomData.Conditions.HasEnchantment);
                        Overlay.DrawYesNoText("HasHex", skill.CustomData.Conditions.HasHex);
                        Overlay.DrawYesNoText("IsCasting", skill.CustomData.Conditions.IsCasting);
                        Overlay.DrawYesNoText("IsNockedDown", skill.CustomData.Conditions.IsNockedDown);
                        Overlay.DrawYesNoText("IsMoving", skill.CustomData.Conditions.IsMoving);
                        Overlay.DrawYesNoText("IsAttacking", skill.CustomData.Conditions.IsAttacking);
                        Overlay.DrawYesNoText("IsHoldingItem", skill.CustomData.Conditions.IsHoldingItem);
                        Overlay.DrawYesNoText("TargettingStrict", skill.CustomData.Conditions.TargettingStrict);
                        ImGui::Text("LessLife: %.2f", skill.CustomData.Conditions.LessLife);
                        ImGui::Text("MoreLife: %.2f", skill.CustomData.Conditions.MoreLife);
                        ImGui::Text("LessEnergy: %.2f", skill.CustomData.Conditions.LessEnergy);
                        ImGui::Text("SacrificeHealth: %.2f", skill.CustomData.Conditions.SacrificeHealth);
                        Overlay.DrawYesNoText("IsPartyWide", skill.CustomData.Conditions.IsPartyWide);
                        Overlay.DrawYesNoText("UniqueProperty", skill.CustomData.Conditions.UniqueProperty);
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







