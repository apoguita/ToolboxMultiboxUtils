#include "MultiboxUtils.h"

#include <GWCA/GWCA.h>

#pragma comment(lib, "Shlwapi.lib")
#include <GWCA/Utilities/Hooker.h>
#include <Path.h>
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
#include <GWCA/Managers/EffectMgr.h>
#include <nlohmann/json.hpp>
#include <string>
#include <locale> 
#include <codecvt>
#include <windows.h>
#include <iostream>
#include <cstring>

#define CLIENT_COUNT 8
#define GAMEVARS_SIZE sizeof(GameVarsStruct)
HANDLE hFileMapping; 
//#define FILE_NAME "shared_gamevars.dat"
std::wstring FILE_NAME;

void HeroAI::setFileSize(HANDLE hFile, DWORD size) {
    LARGE_INTEGER li;
    li.QuadPart = size;
    if (!SetFilePointerEx(hFile, li, NULL, FILE_BEGIN)) {
        std::cerr << "SetFilePointerEx failed (" << GetLastError() << ")." << std::endl;
    }
    if (!SetEndOfFile(hFile)) {
        std::cerr << "SetEndOfFile failed (" << GetLastError() << ")." << std::endl;
    }
}


HANDLE HeroAI::createMemoryMappedFile(HANDLE& hFileMapping) {
    HANDLE hFile = CreateFileW (/*CreateFile(*/
        FILE_NAME.c_str(),              // Name of the file
        GENERIC_READ | GENERIC_WRITE, // Desired access
        FILE_SHARE_READ | FILE_SHARE_WRITE,//sharing read and write
        NULL,                   // Default security
        OPEN_ALWAYS,            // Open existing or create new
        FILE_ATTRIBUTE_NORMAL,  // Normal file
        NULL                    // No template file
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        std::cerr << "Could not create or open file (" << GetLastError() << ")." << std::endl;
        return NULL;
    }

    setFileSize(hFile, CLIENT_COUNT * GAMEVARS_SIZE); 

    hFileMapping = CreateFileMapping(
        hFile,                  // Handle to the file
        NULL,                   // Default security
        PAGE_READWRITE,         // Read/write access
        0,                      // Maximum object size (high-order DWORD)
        CLIENT_COUNT * GAMEVARS_SIZE,  // Maximum object size (low-order DWORD)
        NULL                    // Name of mapping object
    );

    if (hFileMapping == NULL) {
        std::cerr << "Could not create file mapping object (" << GetLastError() << ")." << std::endl;
        CloseHandle(hFile);
        return NULL;
    }

    CloseHandle(hFile);  // The file handle is no longer needed after creating the mapping
    return hFileMapping;
}

bool HeroAI::writeGameVarsToFile(HANDLE hFileMapping, int clientIndex, GameVarsStruct* gameVars) {
    if (clientIndex < 0 || clientIndex >= CLIENT_COUNT) {
        std::cerr << "Invalid client index." << std::endl;
        return false;
    }

    SYSTEM_INFO si;
    GetSystemInfo(&si);

    // Ensure the offset is aligned to the allocation granularity
    DWORD offset = clientIndex * GAMEVARS_SIZE; 
    DWORD alignedOffset = (offset / si.dwAllocationGranularity) * si.dwAllocationGranularity; 


    LPVOID pBuf = MapViewOfFile(
        hFileMapping,           // Handle to map object
        FILE_MAP_ALL_ACCESS,         // Read/write permission
        0,
        alignedOffset, 
        GAMEVARS_SIZE
    );

    if (pBuf == NULL) {
        std::cerr << "Could not map view of file (" << GetLastError() << ")." << std::endl;
        return false;
    }

    std::memcpy(pBuf, gameVars, GAMEVARS_SIZE);
    UnmapViewOfFile(pBuf);
    return true;
}

bool HeroAI::readGameVarsFromFile(HANDLE hFileMapping, int clientIndex, GameVarsStruct* gameVars) {
    if (clientIndex < 0 || clientIndex >= CLIENT_COUNT) {
        std::cerr << "Invalid client index." << std::endl;
        return false;
    }

    SYSTEM_INFO si;
    GetSystemInfo(&si);

    // Ensure the offset is aligned to the allocation granularity
    DWORD offset = clientIndex * GAMEVARS_SIZE;
    DWORD alignedOffset = (offset / si.dwAllocationGranularity) * si.dwAllocationGranularity;

    LPVOID pBuf = MapViewOfFile(
        hFileMapping,           // Handle to map object
        FILE_MAP_READ,          // Read permission
        0,
        alignedOffset,
        GAMEVARS_SIZE
    );

    if (pBuf == NULL) {
        std::cerr << "Could not map view of file (" << GetLastError() << ")." << std::endl;
        return false;
    }

    std::memcpy(gameVars, pBuf, GAMEVARS_SIZE);
    UnmapViewOfFile(pBuf);
    return true;
}


namespace fs = std::filesystem;
using json = nlohmann::json;

void to_json(json& j, const HeroAI::castConditions& cc) {
    j = json{
        {"IsAlive", cc.IsAlive},
        {"HasCondition", cc.HasCondition},
        {"HasDeepWound", cc.HasDeepWound},
        {"HasCrippled", cc.HasCrippled},
        {"HasBleeding", cc.HasBleeding},
        {"HasPoison", cc.HasPoison},
        {"HasWeaponSpell", cc.HasWeaponSpell},
        {"HasEnchantment", cc.HasEnchantment},
        {"HasHex", cc.HasHex},
        {"IsCasting", cc.IsCasting},
        {"IsNockedDown", cc.IsNockedDown},
        {"IsMoving", cc.IsMoving},
        {"IsAttacking", cc.IsAttacking},
        {"IsHoldingItem", cc.IsHoldingItem},
        {"LessLife", cc.LessLife},
        {"MoreLife", cc.MoreLife},
        {"LessEnergy", cc.LessEnergy},
        {"IsPartyWide", cc.IsPartyWide},
        {"UniqueProperty", cc.UniqueProperty}
    };
}

void from_json(const json& j, HeroAI::castConditions& cc) {
    j.at("IsAlive").get_to(cc.IsAlive);
    j.at("HasCondition").get_to(cc.HasCondition);
    j.at("HasDeepWound").get_to(cc.HasDeepWound);
    j.at("HasCrippled").get_to(cc.HasCrippled);
    j.at("HasBleeding").get_to(cc.HasBleeding);
    j.at("HasPoison").get_to(cc.HasPoison);
    j.at("HasWeaponSpell").get_to(cc.HasWeaponSpell);
    j.at("HasEnchantment").get_to(cc.HasEnchantment);
    j.at("HasHex").get_to(cc.HasHex);
    j.at("IsCasting").get_to(cc.IsCasting);
    j.at("IsNockedDown").get_to(cc.IsNockedDown);
    j.at("IsMoving").get_to(cc.IsMoving);
    j.at("IsAttacking").get_to(cc.IsAttacking);
    j.at("IsHoldingItem").get_to(cc.IsHoldingItem);
    j.at("LessLife").get_to(cc.LessLife);
    j.at("MoreLife").get_to(cc.MoreLife);
    j.at("LessEnergy").get_to(cc.LessEnergy);
    j.at("IsPartyWide").get_to(cc.IsPartyWide);
    j.at("UniqueProperty").get_to(cc.UniqueProperty);
}

void to_json(json& j, const HeroAI::CustomSkillData& csd) {
    j = json{
        {"SkillID", csd.SkillID},
        {"SkillType", csd.SkillType},
        {"TargetAllegiance", csd.TargetAllegiance},
        {"Nature", csd.Nature},
        {"Conditions", csd.Conditions}
    };
}

void from_json(const json& j, HeroAI::CustomSkillData& csd) {
    j.at("SkillID").get_to(csd.SkillID);
    j.at("SkillType").get_to(csd.SkillType);
    j.at("TargetAllegiance").get_to(csd.TargetAllegiance);
    j.at("Nature").get_to(csd.Nature);
    j.at("Conditions").get_to(csd.Conditions);
}

void HeroAI::serializeSpecialSkills(const std::string& filename) {
    json j = SpecialSkills; 
    std::ofstream file(filename);
    if (file.is_open()) {
        file << j.dump(4);  // Pretty print with an indent of 4 spaces
        file.close();
    }
    else {
        //std::cerr << "Unable to open file for writing: " << filename << std::endl;
        WriteChat(GW::Chat::CHANNEL_GWCA1, L"Unable to open file for writing skills.json", L"HeroAI");
    }
}

void HeroAI::deserializeSpecialSkills(const std::string& filename) {
    std::ifstream file(filename);
    if (file.is_open()) {
        json j;
        file >> j;
        file.close();
        j.get_to(SpecialSkills);
    }
    else {
        //std::cerr << "Unable to open file for reading: " << filename << std::endl;
        WriteChat(GW::Chat::CHANNEL_GWCA1, L"Unable to open file for reading skills.json", L"HeroAI");
    }
}





uint32_t HeroAI::GetPing() { return 750.0f; }

#define GAME_CMSG_INTERACT_GADGET                   (0x004F) // 79
#define GAME_CMSG_SEND_SIGNPOST_DIALOG              (0x0051) // 81

uint32_t last_hovered_item_id = 0;


enum eState { Idle, Looting, Following, Scatter };

struct StateMachineStruct {
    eState State;
    clock_t LastActivity;
    bool IsFollowingEnabled;
    bool IsLootingEnabled;
    bool IsCombatEnabled;
    bool IsTargettingEnabled;
    bool IsScatterEnabled;
};

StateMachineStruct StateMachine;



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
static GW::AgentID AllyTarget;
float oldAngle;

float ScatterPos[8] = {0.0f, 45.0f, -45.0f, 90.0f, -90.0f, 135.0f, -135.0f, 180.0f };

bool OneTimeRun = false;
bool OneTimeRun2 = false;


void wcs_tolower(wchar_t* s)
{
    for (size_t i = 0; s[i]; i++)
        s[i] = towlower(s[i]);
}


DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static HeroAI instance;
    return &instance;
}

static void CmdCombat(const wchar_t*, const int argc, const LPWSTR* argv) {
    wcs_tolower(*argv);
    
    if (argc == 1) {
        StateMachine.IsCombatEnabled = !StateMachine.IsCombatEnabled;
        if (StateMachine.IsCombatEnabled) {
            WriteChat(GW::Chat::CHANNEL_GWCA1, L"CombatAI Enabled", L"HeroAI");
        }
        else {
            WriteChat(GW::Chat::CHANNEL_GWCA1, L"CombatAI Disabled", L"HeroAI");
        }
        return;
    }

    if (argc == 2) {
        WriteChat(GW::Chat::CHANNEL_GWCA1, L"CombatAI: bad parameters, need at least 2", L"HeroAI");
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
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"CombatAI: bad parameter, skill#", L"Hero AI");
                break;
            }

            if (cIndex == 0) {
                for (int i = 0; i < 8; i++) {
                    CombatSkillState[i] = false;
                }
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"CombatAI: skills 1-8 removed from combat loop", L"Hero AI");

            }
            else {
                CombatSkillState[cIndex - 1] = false;
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"CombatAI: skill removed from combat loop", L"Hero AI");

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
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"CombatAI: bad parameter, skill#", L"Hero AI");
                break;
            }

            if (cIndex == 0) {
                for (int i = 0; i < 8; i++) {
                    CombatSkillState[i] = true;
                }
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"CombatAI: skills 1-8 added to combat loop", L"Hero AI");

            }
            else {
                CombatSkillState[cIndex-1] = true;
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"CombatAI: skill added to combat loop", L"Hero AI");

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
    FileAccessClock.LastTimeAccesed = clock();
    //InitSkillData();

    // SERIALIZE SKILL FILE
    wchar_t path[MAX_PATH];
    const auto result = GetModuleFileNameW(plugin_handle, path, MAX_PATH); 
    
    PathRemoveFileSpecW(path); 

    std::wstring file_dest = path; 
    FILE_NAME = path; 
    FILE_NAME += L"\\shared_data.dat";
    file_dest += L"\\skills.json"; 
    std::string file_dest_mb(file_dest.begin(), file_dest.end());

    if (!fs::exists(file_dest)) {
        InitSkillData();
        
        serializeSpecialSkills(file_dest_mb);
    }
    else {
        deserializeSpecialSkills(file_dest_mb);
    }
    
    
    


    StateMachine.State = Idle;
    StateMachine.IsFollowingEnabled = false;
    StateMachine.IsLootingEnabled = false;


    GW::Chat::CreateCommand(L"autoloot", [](const wchar_t*, int, const LPWSTR*) {
        GW::GameThread::Enqueue([] { 
            StateMachine.IsLootingEnabled = !StateMachine.IsLootingEnabled;

            if (StateMachine.IsLootingEnabled) {
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"LootAI Enabled", L"Hero AI"); 
            }
            else {
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"LootAI Disabled", L"Hero AI"); 
            }
            });
        }); 
     
    WriteChat(GW::Chat::CHANNEL_GWCA1, L"Use /autoloot To toggle LootAI", L"Hero AI");

    GW::Chat::CreateCommand(L"autofollow", [](const wchar_t*, int, const LPWSTR*) {
        GW::GameThread::Enqueue([] {
            StateMachine.IsFollowingEnabled = !StateMachine.IsFollowingEnabled;
            
            if (StateMachine.IsFollowingEnabled) {
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"FollowAI Enabled", L"Hero AI");
            }
            else {
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"FollowAI Disabled", L"Hero AI");
            }
            });
        });
    WriteChat(GW::Chat::CHANNEL_GWCA1, L"Use /autofollow To toggle Follow party Leader", L"Hero AI");

    GW::Chat::CreateCommand(L"dropitem", [](const wchar_t*, int, const LPWSTR*) {
        GW::GameThread::Enqueue([] {
            if ((GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) && (GW::PartyMgr::GetIsPartyLoaded())) {
                const GW::Item* current = GW::Items::GetHoveredItem();
                if (current) {
                    GW::Items::DropItem(current, current->quantity);
                }
            }
            else {
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"/dropitem Command only works in Explorable areas", L"Hero AI");
            }
            });
        });
    WriteChat(GW::Chat::CHANNEL_GWCA1, L"Use /dropitem To drop hovered item to the ground", L"Hero AI");

    GW::Chat::CreateCommand(L"autotarget", [](const wchar_t*, int, const LPWSTR*) {
            GW::GameThread::Enqueue([] {
                StateMachine.IsTargettingEnabled = !StateMachine.IsTargettingEnabled;

            if (StateMachine.IsTargettingEnabled) {
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"TargettingAI Enabled", L"Hero AI");
            }
            else {
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"TargettingAI Disabled", L"Hero AI");
            }
            });
        });
    WriteChat(GW::Chat::CHANNEL_GWCA1, L"Use /autotarget To drop hovered item to the ground", L"Hero AI");

    GW::Chat::CreateCommand(L"scatter", [](const wchar_t*, int, const LPWSTR*) {
        GW::GameThread::Enqueue([] {
            StateMachine.IsScatterEnabled = !StateMachine.IsScatterEnabled;

            if (StateMachine.IsScatterEnabled) {
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"ScatterAI Enabled", L"Hero AI");
            }
            else {
                WriteChat(GW::Chat::CHANNEL_GWCA1, L"ScatterAI Disabled", L"Hero AI");
            }
            });
        });
    WriteChat(GW::Chat::CHANNEL_GWCA1, L"Use /scatter toggle Scatter AI", L"Hero AI");

    GW::Chat::CreateCommand(L"heroai", [](const wchar_t*, int, const LPWSTR*) {
        GW::GameThread::Enqueue([] {
            
            StateMachine.IsFollowingEnabled = !StateMachine.IsFollowingEnabled;
            StateMachine.IsLootingEnabled = !StateMachine.IsLootingEnabled;
            StateMachine.IsCombatEnabled = !StateMachine.IsCombatEnabled;
            StateMachine.IsTargettingEnabled = !StateMachine.IsTargettingEnabled;
            StateMachine.IsScatterEnabled = !StateMachine.IsScatterEnabled;


for (int i = 0; i < 8; i++) {
    CombatSkillState[i] = !CombatSkillState[i];
}

if (StateMachine.IsFollowingEnabled) {
    WriteChat(GW::Chat::CHANNEL_GWCA1, L"HeroAI Enabled", L"Hero AI");
}
else {
    WriteChat(GW::Chat::CHANNEL_GWCA1, L"HeroAI Disabled", L"Hero AI");
}
            });
        });
        WriteChat(GW::Chat::CHANNEL_GWCA1, L"Use /heroai to toggle all features at once", L"Hero AI");

        GW::Chat::CreateCommand(L"autocombat", &CmdCombat);

        WriteChat(GW::Chat::CHANNEL_GWCA1, L"Use /autocombat add [1-8,all] To add skill to combat queue.", L"Hero AI");
        WriteChat(GW::Chat::CHANNEL_GWCA1, L"Use /autocombat remove [1-8,all] To remove skill from combat queue.", L"Hero AI");

}

float DegToRad(float degree) {
    // converting degrees to radians
    return (degree * 3.14159 / 180);
}

bool HeroAI::UpdateGameVars() {

    GameVars.Map.IsExplorable = (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable) ? true : false;
    GameVars.Map.IsLoading = (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading) ? true : false;
    GameVars.Map.IsOutpost = (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) ? true : false;
    GameVars.Map.IsPartyLoaded = (GW::PartyMgr::GetIsPartyLoaded()) ? true : false;

    if (!GameVars.Map.IsExplorable) { return false; }
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

    GameVars.CombatField.DistanceFromLeader = GW::GetDistance(GameVars.Target.Self.Living->pos, GameVars.Party.LeaderLiving->pos);
    GameVars.Party.IsLeaderAttacking = (GameVars.Party.LeaderLiving->GetIsAttacking()) ? true : false;
    GameVars.Party.IsLeaderCasting = (GameVars.Party.LeaderLiving->GetIsCasting()) ? true : false;
    GameVars.CombatField.IsSelfMoving = (GameVars.Target.Self.Living->GetIsMoving()) ? true : false;
    GameVars.CombatField.IsSelfCasting = (GameVars.Target.Self.Living->GetIsCasting()) ? true : false;
    GameVars.CombatField.IsSelfKnockedDwon = (GameVars.Target.Self.Living->GetIsKnockedDown()) ? true : false;


    GameVars.Target.Nearest.ID = TargetNearestEnemyInAggro();
    if (GameVars.Target.Nearest.ID) {
        GameVars.Target.Nearest.Agent = GW::Agents::GetAgentByID(GameVars.Target.Nearest.ID);
        GameVars.Target.Nearest.Living = GameVars.Target.Nearest.Agent->GetAsAgentLiving();

    }
    GameVars.CombatField.InAggro = (GameVars.Target.Nearest.ID) ? true : false;

    GameVars.Target.Called.ID = GameVars.Party.PartyInfo->players[0].calledTargetId;
    if (GameVars.Target.Called.ID) {
        GameVars.Target.Called.Agent = GW::Agents::GetAgentByID(GameVars.Target.Called.ID);
        GameVars.Target.Called.Living = GameVars.Target.Called.Agent->GetAsAgentLiving();
    }

    GameVars.Target.NearestAlly.ID = TargetLowestAllyInAggro();
    if (GameVars.Target.NearestAlly.ID) {
        GameVars.Target.NearestAlly.Agent = GW::Agents::GetAgentByID(GameVars.Target.NearestAlly.ID);
        GameVars.Target.NearestAlly.Living = GameVars.Target.NearestAlly.Agent->GetAsAgentLiving();

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
        int ptr = 0;
        while ((Skillptr == 0) && (ptr < MaxSkillData))
        {
            if ((SpecialSkills[ptr].SkillID != GW::Constants::SkillID::No_Skill) &&
                ((SpecialSkills[ptr].SkillID) == GameVars.SkillBar.Skills[i].Skill.skill_id)) {
                Skillptr = ptr; 
            }
            ptr++; 
        }

        if (Skillptr) {
            GameVars.SkillBar.Skills[i].CustomData = SpecialSkills[Skillptr];
            GameVars.SkillBar.Skills[i].CustomData.Conditions = SpecialSkills[Skillptr].Conditions;
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

    if (FileAccessClock.CanAccess()) {
  
        const auto res = createMemoryMappedFile(hFileMapping);
        // Write to the file
        GameVars.LastTimeSubscribed = clock();
        if (!writeGameVarsToFile(hFileMapping, GameVars.Party.SelfPartyNumber - 1, &GameVars)) {
            CloseHandle(hFileMapping);
        }

        // Read from the file
        
        for (int i = 0; i < 8; i++) {
            GameVarsStruct readData; 
            readGameVarsFromFile(hFileMapping, i, &readData);
            if (readData.RecentlySubscribed()) {
                MemPlayers[i] = readData;
                MemPlayers[i].IsMemoryLoaded = true;
            }
            else {
                MemPlayers[i].IsMemoryLoaded = false;
            }

        }
        CloseHandle(hFileMapping);
        /*if (!readGameVarsFromFile(hFileMapping, GameVars.Party.SelfPartyNumber - 1, &readData)) {
            CloseHandle(hFileMapping);
        }*/


        FileAccessClock.LastTimeAccesed = clock();
    }
    
    /****************** TARGERT MONITOR **********************/
    const auto targetChange = PartyTargetIDChanged();
    if ((StateMachine.IsTargettingEnabled) &&
        (targetChange != 0)) {
        GW::Agents::ChangeTarget(targetChange);
        CombatMachine.StayAlert = clock();
    }
    /****************** END TARGET MONITOR **********************/ 

    if (!GameVars.CombatField.InAggro) { CombatMachine.castingptr = 0; CombatMachine.State = cIdle; }
    // ENHANCE SCANNING AREA ON COMBAT ACTIVITY 
    if (GameVars.Party.IsLeaderAttacking || GameVars.Party.IsLeaderCasting) { CombatMachine.StayAlert = clock(); }

    if (GameVars.CombatField.IsSelfMoving) { return; }
    if (GameVars.CombatField.IsSelfKnockedDwon) { return; }
    if (!IsAllowedCast()) { return; }

    //if (CombatMachine.State == InCastingRoutine) { goto forceCombat; }; //dirty trick to enforce combat check

    /**************** LOOTING *****************/
    uint32_t tItem;

    tItem = TargetNearestItem();
    if ((StateMachine.IsLootingEnabled) &&
        (!GameVars.CombatField.InAggro) &&
        (ThereIsSpaceInInventory()) &&
        (tItem != 0)
        ) {

        GW::Agents::PickUpItem(GW::Agents::GetAgentByID(tItem), false);
        return;
    }
    /**************** END LOOTING *************/

    /************ FOLLOWING *********************/

    if ((StateMachine.IsFollowingEnabled) &&
        (GameVars.CombatField.DistanceFromLeader > GW::Constants::Range::Area) &&
        (!GameVars.CombatField.IsSelfCasting) &&
        (GameVars.Party.SelfPartyNumber > 1)
        ) {
        
        const auto xx = GameVars.Party.LeaderLiving->x;
        const auto yy = GameVars.Party.LeaderLiving->y;
        const auto heropos = (GameVars.Party.SelfPartyNumber - 1) + GW::PartyMgr::GetPartyHeroCount() + GW::PartyMgr::GetPartyHenchmanCount();
        const auto angle = GameVars.Party.LeaderLiving->rotation_angle + DegToRad(ScatterPos[heropos]);
        const auto rotcos = cos(angle);
        const auto rotsin = sin(angle);

        //GW::Agents::InteractAgent(GameVars.Party.Leader, false);
        GW::Agents::Move(xx, yy);
        const auto posx = (GW::Constants::Range::Adjacent /* + (43.0f)*/) * rotcos;
        const auto posy = (GW::Constants::Range::Adjacent /* + (43.0f)*/) * rotsin;

        GW::Agents::Move(xx + posx, yy + posy);
        return;
    }
    /************ END FOLLOWING *********************/

    /*************** SCATTER ****************************/
    if ((StateMachine.IsScatterEnabled) && 
        (!GameVars.CombatField.IsSelfCasting) && 
        (GameVars.Party.SelfPartyNumber > 1) && 
        (!TargetNearestEnemyInAggro()) && 
        ((GameVars.CombatField.DistanceFromLeader <= (GW::Constants::Range::Touch + (43.0f))) || (AngleChange()))
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
    if ((StateMachine.IsCombatEnabled) && (GameVars.CombatField.InAggro)) {
        if (CombatMachine.castingptr >= 8) { CombatMachine.castingptr = 0; }
        if (GameVars.SkillBar.Skillptr >= 8) { GameVars.SkillBar.Skillptr = 0; }
        switch (CombatMachine.State) {
        case cIdle:
            if (CombatMachine.State != InCastingRoutine) {
                if ((CombatSkillState[GameVars.SkillBar.SkillOrder[GameVars.SkillBar.Skillptr]]) && 
                    (IsSkillready(GameVars.SkillBar.SkillOrder[GameVars.SkillBar.Skillptr])) && 
                    (IsAllowedCast()) && 
                    (IsReadyToCast(GameVars.SkillBar.SkillOrder[GameVars.SkillBar.Skillptr]))) 
                {
                    CombatMachine.State = InCastingRoutine; 
                    CombatMachine.currentCastingSlot = GameVars.SkillBar.SkillOrder[GameVars.SkillBar.Skillptr]; 
                    CombatMachine.LastActivity = clock(); 
                    CombatMachine.IntervalSkillCasting = 750.0f + GetPing(); 
                    CombatMachine.IsSkillCasting = true; 

                    GW::AgentID vTarget = 0; 

                    vTarget = GetAppropiateTarget(CombatMachine.currentCastingSlot); 
                    ChooseTarget(); 
                    GW::Agents::InteractAgent(GW::Agents::GetAgentByID((GameVars.Target.Called.ID) ? GameVars.Target.Called.ID : GameVars.Target.Nearest.ID), false); 
                    if (CastSkill(CombatMachine.currentCastingSlot)) { GameVars.SkillBar.Skillptr = 0; } 
                    else { GameVars.SkillBar.Skillptr++; } 
                    break;
                }
                else { GameVars.SkillBar.Skillptr++; } 
             }
            break;
        case InCastingRoutine:

            if (!IsAllowedCast()) { break; }
            //CombatMachine.castingptr = 0;
            CombatMachine.State = cIdle;

            CombatMachine.castingptr++;

            break;
        default:
            CombatMachine.State = cIdle;
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

uint32_t  HeroAI::TargetNearestEnemyInAggro(bool IsBigAggro) {
    const auto agents = GW::Agents::GetAgentArray();

    float distance = GW::Constants::Range::Earshot;
    float maxdistance = GW::Constants::Range::Earshot;
    if (((clock() - CombatMachine.StayAlert) <= 3000) || (IsBigAggro)){
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

uint32_t  HeroAI::TargetLowestAllyInAggro() {
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
        if (!tempAgent->GetAsAgentLiving()->GetIsAlive()) { continue; }
        
        const float vlife = tAllyLiving->hp;


        if (vlife < StoredLife) {
            StoredLife = vlife;
            tStoredAllyID = AllyID;
        }
    }
    if (StoredLife != 2.0f) { return tStoredAllyID; }
    else {
        return 0;
    }
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
    if (GameVars.SkillBar.Skills[slot].HasCustomData) {
        switch (GameVars.SkillBar.Skills[slot].CustomData.TargetAllegiance) {
        case Enemy:
        case EnemyCaster:
        case EnemyMartial:
            vTarget = GameVars.Target.Called.ID;
            if (!vTarget) { vTarget = GameVars.Target.Nearest.ID; }
            break;
        case Ally:
        case AllyCaster:
        case AllyMartial:
        case OtherAlly:
            vTarget = GameVars.Target.NearestAlly.ID;
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

bool HeroAI::IsReadyToCast(uint32_t slot) {
    if (GameVars.CombatField.IsSelfCasting) { CombatMachine.IsSkillCasting = false; return false; }
    if (GameVars.SkillBar.Skills[slot].Skill.skill_id == GW::Constants::SkillID::No_Skill) { CombatMachine.IsSkillCasting = false; return false; }
    if (GameVars.SkillBar.Skills[slot].Skill.GetRecharge() != 0) { CombatMachine.IsSkillCasting = false; return false; }

    const auto current_energy = static_cast<uint32_t>((GameVars.Target.Self.Living->energy * GameVars.Target.Self.Living->max_energy));
    if (current_energy < GameVars.SkillBar.Skills[slot].Data.energy_cost) { CombatMachine.IsSkillCasting = false; return false; }

    const auto enough_adrenaline =
        (GameVars.SkillBar.Skills[slot].Data.adrenaline == 0) ||
        (GameVars.SkillBar.Skills[slot].Data.adrenaline > 0 &&
            GameVars.SkillBar.Skills[slot].Skill.adrenaline_a >= GameVars.SkillBar.Skills[slot].Data.adrenaline);

    if (!enough_adrenaline) { CombatMachine.IsSkillCasting = false; return false; }

    GW::AgentID vTarget = GetAppropiateTarget(slot);

    if ((GameVars.SkillBar.Skills[slot].HasCustomData) && 
        (!AreCastConditionsMet(slot, vTarget))) { 
        CombatMachine.IsSkillCasting = false; return false;
    }
    if (doesSpiritBuffExists(GameVars.SkillBar.Skills[slot].Skill.skill_id)) { CombatMachine.IsSkillCasting = false; return false; }

    if (HasEffect(GameVars.SkillBar.Skills[slot].Skill.skill_id, GW::Agents::GetTargetId())) { CombatMachine.IsSkillCasting = false; return false; }

    if ((GameVars.CombatField.IsSelfCasting) && (GameVars.SkillBar.Skills[slot].Skill.GetRecharge() != 0)) { CombatMachine.IsSkillCasting = false; return false; }

    return true;
}

bool HeroAI::CastSkill(uint32_t slot) {
    //this function is legacy in case i need it later
    GW::AgentID vTarget = 0;
    
    vTarget = GetAppropiateTarget(slot); 
    if (IsReadyToCast(slot))
     {
        CombatMachine.IsSkillCasting = true;
        CombatMachine.LastActivity = clock();
        CombatMachine.IntervalSkillCasting = GameVars.SkillBar.Skills[slot].Data.activation + GameVars.SkillBar.Skills[slot].Data.aftercast + 750.0f;

        GW::SkillbarMgr::UseSkill(slot, vTarget);
        //GW::Agents::InteractAgent(GW::Agents::GetAgentByID((GameVars.Target.Called.ID) ? GameVars.Target.Called.ID: GameVars.Target.Nearest.ID), false); 
        return true;
    }
    return false;
}

bool HeroAI::IsAllowedCast() {
    if (!CombatMachine.IsSkillCasting) { return true; }
    
    if ((clock() - CombatMachine.LastActivity) > CombatMachine.IntervalSkillCasting) {
        CombatMachine.IsSkillCasting = false;
        return true;
    }
    return false;
}

void HeroAI::ChooseTarget() { 
    if (StateMachine.IsTargettingEnabled) {
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
    CloseHandle(hFileMapping);
    ToolboxPlugin::Terminate();
    GW::Terminate();
}

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
        
        ImGui::BeginTable("maintable", 2);
        ImGui::TableNextRow(); 
        ImGui::TableSetColumnIndex(0);
        ImGui::Checkbox("Loot", &StateMachine.IsLootingEnabled);
        ImGui::TableSetColumnIndex(1);
        ImGui::Checkbox("Follow", &StateMachine.IsFollowingEnabled);
        ImGui::TableNextRow(); 
        ImGui::TableSetColumnIndex(0);
        ImGui::Checkbox("Target", &StateMachine.IsTargettingEnabled);
        ImGui::TableSetColumnIndex(1);
        ImGui::Checkbox("Combat", &StateMachine.IsCombatEnabled);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Checkbox("Scatter", &StateMachine.IsScatterEnabled);
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
        if ((MemPlayers[i].RecentlySubscribed()) && (MemPlayers[i].Target.Self.ID == agentID)) {
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

    while ((Skillptr == 0) && (ptr < MaxSkillData)) 
    {
        if (SpecialSkills[ptr].SkillID == GameVars.SkillBar.Skills[slot].Skill.skill_id) {
            Skillptr = ptr; 
        }
        ptr++; 
    }
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
            return false;
        } 
    }

    featurecount += (SpecialSkills[Skillptr].Conditions.IsAlive) ? 1 : 0;
    featurecount += (SpecialSkills[Skillptr].Conditions.HasCondition) ? 1 : 0;
    featurecount += (SpecialSkills[Skillptr].Conditions.HasDeepWound) ? 1 : 0;
    featurecount += (SpecialSkills[Skillptr].Conditions.HasCrippled) ? 1 : 0;
    featurecount += (SpecialSkills[Skillptr].Conditions.HasBleeding) ? 1 : 0;
    featurecount += (SpecialSkills[Skillptr].Conditions.HasPoison) ? 1 : 0;
    featurecount += (SpecialSkills[Skillptr].Conditions.HasWeaponSpell) ? 1 : 0;
    featurecount += (SpecialSkills[Skillptr].Conditions.HasEnchantment) ? 1 : 0;
    featurecount += (SpecialSkills[Skillptr].Conditions.HasHex) ? 1 : 0;
    featurecount += (SpecialSkills[Skillptr].Conditions.IsCasting) ? 1 : 0;
    featurecount += (SpecialSkills[Skillptr].Conditions.IsNockedDown) ? 1 : 0;
    featurecount += (SpecialSkills[Skillptr].Conditions.IsMoving) ? 1 : 0;
    featurecount += (SpecialSkills[Skillptr].Conditions.IsAttacking) ? 1 : 0;
    featurecount += (SpecialSkills[Skillptr].Conditions.IsHoldingItem) ? 1 : 0;
    featurecount += (SpecialSkills[Skillptr].Conditions.LessLife > 0.0f) ? 1 : 0;
    featurecount += (SpecialSkills[Skillptr].Conditions.MoreLife > 0.0f) ? 1 : 0;
    featurecount += (SpecialSkills[Skillptr].Conditions.LessEnergy > 0.0f) ? 1 : 0;

    if (tempAgentLiving->GetIsAlive()       && GameVars.SkillBar.Skills[slot].CustomData.Conditions.IsAlive) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsConditioned() && GameVars.SkillBar.Skills[slot].CustomData.Conditions.HasCondition) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsDeepWounded() && GameVars.SkillBar.Skills[slot].CustomData.Conditions.HasDeepWound) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsCrippled()    && GameVars.SkillBar.Skills[slot].CustomData.Conditions.HasCrippled) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsBleeding()    && GameVars.SkillBar.Skills[slot].CustomData.Conditions.HasBleeding) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsPoisoned()    && GameVars.SkillBar.Skills[slot].CustomData.Conditions.HasPoison) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsWeaponSpelled() && GameVars.SkillBar.Skills[slot].CustomData.Conditions.HasWeaponSpell) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsEnchanted()   && GameVars.SkillBar.Skills[slot].CustomData.Conditions.HasEnchantment) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsHexed()       && GameVars.SkillBar.Skills[slot].CustomData.Conditions.HasHex) { NoOfFeatures++; }
    
    if (tempAgentLiving->GetIsKnockedDown() && GameVars.SkillBar.Skills[slot].CustomData.Conditions.IsNockedDown) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsMoving()      && GameVars.SkillBar.Skills[slot].CustomData.Conditions.IsMoving) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsAttacking()   && GameVars.SkillBar.Skills[slot].CustomData.Conditions.IsAttacking) { NoOfFeatures++; }
    if (tempAgentLiving->weapon_type == 0   && GameVars.SkillBar.Skills[slot].CustomData.Conditions.IsHoldingItem) { NoOfFeatures++; }

    if ((GameVars.SkillBar.Skills[slot].CustomData.Conditions.LessLife != 0.0f) && (tempAgentLiving->hp < GameVars.SkillBar.Skills[slot].CustomData.Conditions.LessLife)) { NoOfFeatures++; }
    if ((GameVars.SkillBar.Skills[slot].CustomData.Conditions.MoreLife != 0.0f) && (tempAgentLiving->hp > GameVars.SkillBar.Skills[slot].CustomData.Conditions.MoreLife)) { NoOfFeatures++; }
    
    
    //ENERGY CHECKS
    if ((MemPlayers[GetMemPosByID(agentID)].RecentlySubscribed()) &&
        (GameVars.SkillBar.Skills[slot].CustomData.Conditions.LessEnergy != 0.0f) &&
        (MemPlayers[GetMemPosByID(agentID)].Target.Self.Living->energy < GameVars.SkillBar.Skills[slot].CustomData.Conditions.LessEnergy) &&
        (MemPlayers[GetMemPosByID(agentID)].Target.Self.Living->energy_regen < 0.03f)) {
        NoOfFeatures++; 
    }
    
    const auto TargetSkill = GW::SkillbarMgr::GetSkillConstantData(static_cast<GW::Constants::SkillID>(tempAgentLiving->skill));

    if (tempAgentLiving->GetIsCasting() && GameVars.SkillBar.Skills[slot].CustomData.Conditions.IsCasting && TargetSkill && TargetSkill->activation > 0.250f) { NoOfFeatures++; }

    if (featurecount == NoOfFeatures) { return true; }

    return false;
}

void HeroAI::InitSkillData() {
    //WARRIOR
    //STRENGTH
    int ptr = 0;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::I_Meant_to_Do_That;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Shout;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.IsNockedDown = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::I_Will_Survive;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Shout;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::You_Will_Die;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Shout;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.LessLife = 0.5F;
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
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
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
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
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
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
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
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
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
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Blessed_Light;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Boon_Signet; 
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Signet; 
    SpecialSkills[ptr].TargetAllegiance = Ally; 
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Contemplation_of_Purity;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Skill;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Deny_Hexes;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.HasHex = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Divine_Boon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Divine_Healing;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    SpecialSkills[ptr].Conditions.IsPartyWide = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Divine_Intervention;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.LessLife = 0.25F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Divine_Spirit;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Healers_Boon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Heavens_Delight;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    SpecialSkills[ptr].Conditions.IsPartyWide = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Holy_Haste;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Peace_and_Harmony;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    SpecialSkills[ptr].Conditions.HasHex = true;
    SpecialSkills[ptr].Conditions.UniqueProperty = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Release_Enchantments;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    SpecialSkills[ptr].Conditions.IsPartyWide = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Scribes_Insight;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Signet_of_Devotion;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Signet;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Smiters_Boon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Spell_Breaker;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Spell_Shield;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Unyielding_Aura;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.UniqueProperty = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Watchful_Healing;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Watchful_Healing;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Withdraw_Hexes;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.HasHex = true;
    ptr++;
    //HEALING PRAYERS
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Cure_Hex;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.HasHex = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Dwaynas_Sorrow;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.LessLife = 0.25F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Dwaynas_Kiss;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Ethereal_Light;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Gift_of_Health;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Glimmer_of_Light;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Heal_Area;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    SpecialSkills[ptr].Conditions.IsPartyWide = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Heal_Other;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly; 
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Heal_Party;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    SpecialSkills[ptr].Conditions.IsPartyWide = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Healers_Covenant;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Healing_Breeze;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Healing_Burst;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Healing_Hands;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Healing_Light;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Healing_Ribbon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Healing_Ring;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    SpecialSkills[ptr].Conditions.IsPartyWide = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Healing_Seed;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Healing_Touch;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Healing_Whisper;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Infuse_Health;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.25F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Jameis_Gaze;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Kareis_Healing_Circle;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    SpecialSkills[ptr].Conditions.IsPartyWide = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Light_of_Deliverance;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    SpecialSkills[ptr].Conditions.IsPartyWide = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Live_Vicariously;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Mending;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Orison_of_Healing;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Patient_Spirit;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Renew_Life;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = DeadAlly;
    SpecialSkills[ptr].Nature = Resurrection; 
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Restful_Breeze;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Restore_Life;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = DeadAlly;
    SpecialSkills[ptr].Nature = Resurrection;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Resurrection_Chant;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = DeadAlly;
    SpecialSkills[ptr].Nature = Resurrection;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Signet_of_Rejuvenation;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Signet;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Spotless_Mind;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.HasHex = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Spotless_Soul;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Supportive_Spirit;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Vigorous_Spirit;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Word_of_Healing;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.5F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Words_of_Comfort;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    ptr++;
    //PROTECTION PRAYERS
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Air_of_Enchantment;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature =Enchantment_Removal;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Aura_of_Faith;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Aura_of_Stability;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Convert_Hexes;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.HasHex = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Dismiss_Condition;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Divert_Hexes;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.HasHex = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Draw_Conditions;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Guardian;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Life_Attunement;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Life_Barrier;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Life_Bond;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Life_Sheath;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Mark_of_Protection;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Mend_Ailment;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Mend_Condition;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Mending_Touch;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Pensive_Guardian;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Protective_Bond;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Protective_Spirit;//108
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.LessLife = 0.75F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Purifying_Veil;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Rebirth;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = DeadAlly;
    SpecialSkills[ptr].Nature = Resurrection;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Restore_Condition;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Reversal_of_Fortune;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.LessLife = 0.5F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Reverse_Hex;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.HasHex = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Shield_of_Absorption;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.LessLife = 0.75F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Shield_of_Deflection;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Shield_of_Regeneration;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Shielding_Hands;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.LessLife = 0.5F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Spirit_Bond;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.LessLife = 0.5F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Vital_Blessing;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Zealous_Benediction;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.LessLife = 0.5F;
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
    SpecialSkills[ptr].Nature = Buff;
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
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Shield_of_Judgment;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Offensive;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Smite_Condition;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Smite_Hex;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.HasHex = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Strength_of_Honor;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
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
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    SpecialSkills[ptr].Conditions.HasHex = true;
    SpecialSkills[ptr].Conditions.UniqueProperty = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Essence_Bond;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Holy_Veil;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Light_of_Dwayna;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = DeadAlly;
    SpecialSkills[ptr].Nature = Resurrection;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Purge_Conditions;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Remove_Hex;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.HasHex = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Resurrect;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = DeadAlly;
    SpecialSkills[ptr].Nature = Resurrection;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Signet_of_Removal;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Signet;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    SpecialSkills[ptr].Conditions.HasHex = true;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    SpecialSkills[ptr].Conditions.UniqueProperty = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Succor;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Vengeance;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = DeadAlly;
    SpecialSkills[ptr].Nature = Resurrection;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
    //NECROMANCER
    //SOUL REAPING
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Foul_Feast;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Hexers_Vigor;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Masochism;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Signet_of_Lost_Souls;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Signet;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.LessLife = 0.5f;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Soul_Taker;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    //BLOOD MAGIC
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Awaken_the_Blood;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self; 
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Blood_Renewal;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Blood_Ritual;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = EnergyBuff;
    SpecialSkills[ptr].Conditions.LessEnergy = 0.3f;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Blood_is_Power;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = EnergyBuff;
    SpecialSkills[ptr].Conditions.LessEnergy = 0.3f;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Contagion;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
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
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Dark_Bond;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Dark_Fury; 
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment; 
    SpecialSkills[ptr].TargetAllegiance = AllyMartial;
    SpecialSkills[ptr].Nature = Buff; 
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Demonic_Flesh;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Offensive;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Jaundiced_Gaze;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Enchantment_Removal;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Mark_of_Subversion;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Hex;
    SpecialSkills[ptr].TargetAllegiance = EnemyCaster;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.UniqueProperty = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Order_of_Pain;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Order_of_the_Vampire;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Strip_Enchantment;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Enchantment_Removal;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Vampiric_Spirit;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    ptr++;
    //CURSES
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Chilblains;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Enchantment_Removal;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Corrupt_Enchantment;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Hex;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Enchantment_Removal;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Defile_Flesh;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Hex;
    SpecialSkills[ptr].TargetAllegiance = EnemyCaster;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.UniqueProperty = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Envenom_Enchantments;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Enchantment_Removal;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Order_of_Apostasy;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Pain_of_Disenchantment;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Enchantment_Removal;
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
    SpecialSkills[ptr].Nature = Enchantment_Removal;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Rip_Enchantment;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Enchantment_Removal;
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
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Death_Nova;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Minion;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.LessLife = 0.25F;
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
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Jagged_Bones;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
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
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Taste_of_Pain;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Offensive;
    SpecialSkills[ptr].Conditions.LessLife = 0.5F;
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
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    //NO ATTRIBUTE
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Gaze_of_Contempt;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Enchantment_Removal;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    SpecialSkills[ptr].Conditions.UniqueProperty = true;
    ptr++;
    //MESMER
    //FAST CASTING
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Persistence_of_Memory;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
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
    SpecialSkills[ptr].Nature = Buff;
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
    SpecialSkills[ptr].Nature = Enchantment_Removal;
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
    SpecialSkills[ptr].Nature = Enchantment_Removal;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Illusion_of_Haste;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Illusionary_Weaponry;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
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
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    //INSPIRATION MAGIC
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Auspicious_Incantation;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Channeling;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Discharge_Enchantment;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Enchantment_Removal;
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
    SpecialSkills[ptr].Nature = Enchantment_Removal;
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
    SpecialSkills[ptr].Nature = Enchantment_Removal;
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
    SpecialSkills[ptr].Nature = Enchantment_Removal;
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
    SpecialSkills[ptr].Nature = Buff;
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
    SpecialSkills[ptr].Nature = Enchantment_Removal;
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
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Arcane_Mimicry;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Echo;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
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
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.HasHex = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Lyssas_Balance;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Enchantment_Removal;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Mirror_of_Disenchantment;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Enchantment_Removal;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Shatter_Storm;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Enchantment_Removal;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Signet_of_Disenchantment;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Signet;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Enchantment_Removal;
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
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Elemental_Attunement;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Elemental_Attunement;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Ether_Prodigy;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Ether_Renewal;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Master_of_Magic;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Over_the_Limit;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    //AIR MAGIC
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Air_Attunement;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Conjure_Lightning;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Gust;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
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
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Windborne_Speed;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    //EARTH MAGIC
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Armor_of_Earth;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Earth_Attunement;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Iron_Mist;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Kinetic_Armor;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Magnetic_Aura;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Magnetic_Surge;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Obsidian_Flesh;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Sliver_Armor;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Stone_Sheath;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Stone_Striker;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Stoneflesh_Aura;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    //FIRE MAGIC
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Burning_Speed;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Conjure_Flame;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Double_Dragon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Elemental_Flame;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Fire_Attunement;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Flame_Djinns_Haste;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    //WATER MAGIC
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Armor_of_Frost;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Armor_of_Mist;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Conjure_Frost;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Frigid_Armor;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Ice_Spear;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Mist_Form;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
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
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    //NO ATTRIBUTE
    //NO SKILLS IN CATEGORY
    //ASSASSIN
    //CRITICAL STRIKES
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Assassins_Remedy;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Critical_Defenses;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Dark_Apostasy;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Deadly_Haste;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Locusts_Fury;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Way_of_the_Master;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
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
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Shattering_Assault;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Attack;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Enchantment_Removal;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    ptr++;
    //DEADLY ARTS
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Expunge_Enchantments;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Skill;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Enchantment_Removal;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Way_of_the_Empty_Palm;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
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
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Heart_of_Shadow;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Healing;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Shadow_Form;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Shadow_Refuge;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Shroud_of_Distress;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Way_of_Perfection;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Way_of_the_Fox;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Way_of_the_Lotus;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    //No Attribute
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Aura_of_Displacement;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Recall;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Shadow_Meld;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Buff;
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
    SpecialSkills[ptr].Nature = Enchantment_Removal;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    SpecialSkills[ptr].Conditions.HasHex = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Assault_Enchantments;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Skill;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Enchantment_Removal;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Lift_Enchantment;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Skill;
    SpecialSkills[ptr].TargetAllegiance = Enemy;
    SpecialSkills[ptr].Nature = Enchantment_Removal;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    ptr++;
    //RITUALIST
    //SPAWNING POWER
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Boon_of_Creation;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Explosive_Growth;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Feast_of_Souls;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    SpecialSkills[ptr].Conditions.IsPartyWide = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Ghostly_Haste;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.UniqueProperty = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Renewing_Memories;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.IsHoldingItem = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Sight_Beyond_Sight;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Spirit_Channeling;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Spirit_to_Flesh;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Spirit;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    SpecialSkills[ptr].Conditions.IsPartyWide = true;
    SpecialSkills[ptr].Conditions.UniqueProperty = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Spirits_Gift;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Spirits_Strength;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Weapon_of_Renewal;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = AllyMartial;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Wielders_Remedy;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Wielders_Zeal;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    //CHANNELLING MAGIC
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Nightmare_Weapon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = AllyMartial;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Splinter_Weapon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = AllyMartial;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Wailing_Weapon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = AllyMartial;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Warmongers_Weapon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = AllyMartial;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Weapon_of_Aggression;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Weapon_of_Fury;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = AllyMartial;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    //COMMUNING
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Brutal_Weapon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = AllyMartial;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.HasEnchantment = true;
    SpecialSkills[ptr].Conditions.UniqueProperty = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Ghostly_Weapon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Guided_Weapon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = AllyMartial;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Sundering_Weapon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = AllyMartial;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Vital_Weapon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = AllyMartial;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Weapon_of_Quickening;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = AllyCaster;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    //RESTORATION MAGIC
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Death_Pact_Signet;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Signet;
    SpecialSkills[ptr].TargetAllegiance = DeadAlly;
    SpecialSkills[ptr].Nature = Resurrection;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Flesh_of_My_Flesh;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = DeadAlly;
    SpecialSkills[ptr].Nature = Resurrection;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Ghostmirror_Light;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Mend_Body_and_Soul;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Mending_Grip;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Resilient_Weapon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = AllyCaster;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.HasHex = true;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    SpecialSkills[ptr].Conditions.UniqueProperty = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Soothing_Memories;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Spirit_Light;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Spirit_Light_Weapon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Spirit_Transfer;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Vengeful_Weapon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Weapon_of_Remedy;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Weapon_of_Shadow;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Weapon_of_Warding;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Wielders_Boon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Xinraes_Weapon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::WeaponSpell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    //PARAGON
    //LEADERSHIP
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Lead_the_Way;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Shout;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Angelic_Bond;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Skill;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.25F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Angelic_Protection;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Shout;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Buff;
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
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Burning_Refrain;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::EchoRefrain;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Enduring_Harmony;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::EchoRefrain;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
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
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Heroic_Refrain;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::EchoRefrain;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Leaders_Comfort;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Skill;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Signet_of_Return;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Signet;
    SpecialSkills[ptr].TargetAllegiance = DeadAlly;
    SpecialSkills[ptr].Nature = Resurrection;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Theyre_on_Fire;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Shout;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    //COMMAND
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Brace_Yourself;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Shout;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Find_Their_Weakness;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Shout;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Make_Haste;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Shout;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::We_Shall_Return;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Shout;
    SpecialSkills[ptr].TargetAllegiance = DeadAlly;
    SpecialSkills[ptr].Nature = Resurrection;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    SpecialSkills[ptr].Conditions.IsPartyWide = true;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Bladeturn_Refrain;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::EchoRefrain;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    //MOTIVATION
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Its_Just_a_Flesh_Wound;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Shout;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.HasCondition = false;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Energizing_Finale;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::EchoRefrain;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Finale_of_Restoration;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::EchoRefrain;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Inspirational_Speech;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Skill;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Mending_Refrain;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::EchoRefrain;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Purifying_Finale;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::EchoRefrain;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Signet_of_Synergy;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Signet;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
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
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.HasCondition = true;
    ptr++;
    //DERVISH
    //MYSTICISM
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Arcane_Zeal;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Avatar_of_Balthazar;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Form;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Avatar_of_Dwayna;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Form;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Avatar_of_Grenth;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Form;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Avatar_of_Lyssa;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Form;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Avatar_of_Melandru;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Form;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Balthazars_Rage;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Eremites_Zeal;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Faithful_Intervention;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Heart_of_Holy_Flame;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Imbue_Health;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = OtherAlly;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.5F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Intimidating_Aura;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Meditation;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Mystic_Corruption;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Mystic_Vigor;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Pious_Renewal;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
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
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Vow_of_Silence;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Watchful_Intervention;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Buff;
    SpecialSkills[ptr].Conditions.LessLife = 0.5F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Zealous_Renewal;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    //EARTH PRAYERS
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Armor_of_Sanctity;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Aura_of_Thorns;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Conviction;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Dust_Cloak;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Ebon_Dust_Aura;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Fleeting_Stability;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Mirage_Cloak;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Mystic_Regeneration;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Sand_Shards;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Shield_of_Force;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Signet_of_Pious_Light;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Signet;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Staggering_Force;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Veil_of_Thorns;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Vital_Boon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Vow_of_Strength;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
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
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Featherfoot_Grace;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Grenths_Aura;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Grenths_Fingers;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Grenths_Grasp;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Guiding_Hands;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++; 
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Harriers_Grasp;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Harriers_Haste;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Lyssas_Haste;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Mystic_Healing;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.75F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Natural_Healing;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.75F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Onslaught;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Pious_Restoration;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.75F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Rending_Aura;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Vow_of_Piety;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Whirling_Charge;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Zealous_Vow;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    //NO ATTRIBUTE
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Enchanted_Haste;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Resurrection_Signet;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Signet;
    SpecialSkills[ptr].TargetAllegiance = DeadAlly;
    SpecialSkills[ptr].Nature = Resurrection;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Aura_of_Holy_Might_kurzick;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Aura_of_Holy_Might_luxon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Elemental_Lord_kurzick;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Elemental_Lord_luxon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Selfless_Spirit_kurzick;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Selfless_Spirit_luxon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Shadow_Sanctuary_kurzick;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Shadow_Sanctuary_luxon;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Critical_Agility;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
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
    SpecialSkills[ptr].Nature = Buff;
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
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Sunspear_Rebirth_Signet;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Signet;
    SpecialSkills[ptr].TargetAllegiance = DeadAlly;
    SpecialSkills[ptr].Nature = Resurrection;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Mental_Block;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Mindbender;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Enchantment;
    SpecialSkills[ptr].TargetAllegiance = Self;
    SpecialSkills[ptr].Nature = Buff;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::By_Urals_Hammer;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Shout;
    SpecialSkills[ptr].TargetAllegiance = DeadAlly;
    SpecialSkills[ptr].Nature = Resurrection;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Breath_of_the_Great_Dwarf;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Spell;
    SpecialSkills[ptr].TargetAllegiance = Ally;
    SpecialSkills[ptr].Nature = Healing;
    SpecialSkills[ptr].Conditions.LessLife = 0.9F;
    ptr++;
    SpecialSkills[ptr].SkillID = GW::Constants::SkillID::Great_Dwarf_Armor;
    SpecialSkills[ptr].SkillType = GW::Constants::SkillType::Signet;
    SpecialSkills[ptr].TargetAllegiance = DeadAlly;
    SpecialSkills[ptr].Nature = Resurrection;
    SpecialSkills[ptr].Conditions.IsAlive = false;
    ptr++;
}

