#pragma once

#include "Headers.h"

#define GAMEVARS_SIZE sizeof(MemPlayerStruct) 
#define SHM_NAME "GWHeroAISharedMemory"

typedef float(__cdecl* ScreenToWorldPoint_pt)(GW::Vec3f* vec, float screen_x, float screen_y, int unk1);
ScreenToWorldPoint_pt ScreenToWorldPoint_Func = 0;

typedef uint32_t(__cdecl* MapIntersect_pt)(GW::Vec3f* origin, GW::Vec3f* unit_direction, GW::Vec3f* hit_point, int* propLayer);
MapIntersect_pt MapIntersect_Func = 0;

uintptr_t ptrBase_address = 0;
uintptr_t ptrNdcScreenCoords = 0;

uintptr_t ptrPathing_func = 0;

class MemMgrClass {
private:
    
    ScreenToWorldPoint_pt ptrScreenToWorldPoint_Func = 0;
    MapIntersect_pt ptrMapIntersect_Func = 0;

    float* GetC2M_MouseX();
    float* GetC2M_MouseY();

public:

    struct MemSinglePlayer {
        GW::AgentID ID = 0;
        float Energy = 0.0f;
        float energyRegen = 0.0f;
        bool IsActive = false;
        bool IsHero = false;
        bool IsFlagged;
        bool IsAllFlag;
        bool ForceCancel = false;
        GW::Vec2f FlagPos;
        Timer LastActivity;
        uint32_t Keepalivecounter=0;


    };

    struct MemPlayerStruct {
        MemSinglePlayer MemPlayers[8];
        GameStateClass GameState[8];
        RemoteCommand command[8];
        CandidateArray Candidates;
        PartyCandidates  NewCandidates;
        uint32_t LeaderID = 0;
    };

    HANDLE hMapFile;
    HANDLE hMutex;
    void InitializeSharedMemory(MemPlayerStruct*& pData);

    //MemPlayerStruct MemCopy;
    MemPlayerStruct* MemData;

    uintptr_t GetBasePtr();  
    uintptr_t GetNdcScreenCoordsPtr();
    ScreenToWorldPoint_pt GetScreenToWorldPtr();
    MapIntersect_pt GetMapIntersectPtr();
    GW::Vec2f GetClickToMoveCoords();

    void MutexAquire();
    void MutexRelease();

};

void MemMgrClass::InitializeSharedMemory(MemPlayerStruct*& pData) {
    hMapFile = OpenFileMapping(
        FILE_MAP_ALL_ACCESS,    // read/write access
        FALSE,                  // do not inherit the name
        SHM_NAME                // name of mapping object
    );

    if (hMapFile == NULL) {
        // If opening failed, create the file mapping object
        hMapFile = CreateFileMapping(
            INVALID_HANDLE_VALUE,   // use paging file
            NULL,                   // default security
            PAGE_READWRITE,         // read/write access
            0,                      // maximum object size (high-order DWORD)
            GAMEVARS_SIZE,          // maximum object size (low-order DWORD)
            SHM_NAME                // name of mapping object
        );

        if (hMapFile == NULL) {
            //std::cerr << "Could not create or open file mapping object (" << GetLastError() << ")." << std::endl;
            WriteChat(GW::Chat::CHANNEL_GWCA1, L"Could not create or open file mapping object", L"Hero AI");
            return;
        }

        // Map the file mapping object into the process's address space
        pData = (MemPlayerStruct*)MapViewOfFile(
            hMapFile,               // handle to map object
            FILE_MAP_ALL_ACCESS,    // read/write permission
            0,                      // offset high
            0,                      // offset low
            GAMEVARS_SIZE
        );

        if (pData == NULL) {
            //std::cerr << "Could not map view of file (" << GetLastError() << ")." << std::endl;
            WriteChat(GW::Chat::CHANNEL_GWCA1, L"Could not map view of file", L"Hero AI");
            CloseHandle(hMapFile);
            return;
        }

        // Initialize shared memory data if this is the first creation
        // pData->some_variable = 0;
        for (int i = 0; i < 8; i++) {
            pData->MemPlayers[i].ID = 0;
            pData->MemPlayers[i].energyRegen = 0.0f;
            pData->MemPlayers[i].Energy = 0.0f;
            pData->MemPlayers[i].IsActive = false;
            pData->MemPlayers[i].LastActivity.start();
            pData->MemPlayers[i].LastActivity.stop();
            pData->MemPlayers[i].IsHero = false;
            pData->MemPlayers[i].IsFlagged = false;
            pData->MemPlayers[i].FlagPos.x = 0.0f;
            pData->MemPlayers[i].FlagPos.y = 0.0f;

            pData->GameState[i].state.Following = true;
            pData->GameState[i].state.Collision = true;
            pData->GameState[i].state.Looting = true;
            pData->GameState[i].state.Combat = true;
            pData->GameState[i].state.Targetting = true;
            pData->GameState[i].state.RangedRangeValue = GW::Constants::Range::Spellcast;
            pData->GameState[i].state.MeleeRangeValue = GW::Constants::Range::Spellcast;
            pData->GameState[i].state.CollisionBubble = 80.0f;

            for (int j = 0; j < 8; j++) {
                pData->GameState[i].CombatSkillState[j] = true;
            }
        }
        // Initialize other variables as needed

    }
    else {
        // Map the existing file mapping object into the process's address space
        pData = (MemPlayerStruct*)MapViewOfFile(
            hMapFile,               // handle to map object
            FILE_MAP_ALL_ACCESS,    // read/write permission
            0,                      // offset high
            0,                      // offset low
            GAMEVARS_SIZE
        );

        if (pData == NULL) {
            //std::cerr << "Could not map view of file (" << GetLastError() << ")." << std::endl;
            WriteChat(GW::Chat::CHANNEL_GWCA1, L"Could not map view of file2", L"Hero AI");
            CloseHandle(hMapFile);
            return;

        }
    }
    // Create or open the named mutex
    hMutex = CreateMutexA(NULL, FALSE, "HeroAIMutex");
    if (hMutex == NULL) {
        WriteChat(GW::Chat::CHANNEL_GWCA1, L"Could not create or open mutex", L"Hero AI");
        return;
    }
}

void MemMgrClass::MutexAquire() {
    WaitForSingleObject(hMutex, INFINITE);
}

void MemMgrClass::MutexRelease() {
    ReleaseMutex(hMutex);
}
uintptr_t MemMgrClass::GetBasePtr() {
    if (ptrBase_address == 0) {
        uintptr_t address = GW::Scanner::Find("\x50\x6A\x0F\x6A\x00\xFF\x35", "xxxxxxx", +7);

        ptrBase_address = *(uintptr_t*)address;

        if (ptrBase_address == 0) {
            WriteChat(GW::Chat::CHANNEL_GWCA1, L"Failed to find base address.", L"Gwtoolbot");
        }
    }
    return ptrBase_address;
}
uintptr_t MemMgrClass::GetNdcScreenCoordsPtr() {
    if (ptrNdcScreenCoords == 0) {
        uintptr_t address = GW::Scanner::Find("\x8B\xE5\x5D\xC3\x8B\x4F\x10\x83\xC7\x08", "xxxxxxxxxx", 0xC);

        ptrNdcScreenCoords = address;

        if (ptrNdcScreenCoords == 0) {
            WriteChat(GW::Chat::CHANNEL_GWCA1, L"Failed to find Ndc Screen Coords.", L"Gwtoolbot");
        }
    }
    return ptrNdcScreenCoords;
}

ScreenToWorldPoint_pt MemMgrClass::GetScreenToWorldPtr() {
    if (ptrScreenToWorldPoint_Func == 0) {

        ptrScreenToWorldPoint_Func = (ScreenToWorldPoint_pt)GW::Scanner::Find("\xD9\x5D\x14\xD9\x45\x14\x83\xEC\x0C", "xxxxxxxx", -0x2F);

        if (ptrScreenToWorldPoint_Func == 0) {
            WriteChat(GW::Chat::CHANNEL_GWCA1, L"Failed to find ptrScreenToWorldPoint_Func addres.", L"Gwtoolbot");
        }
    }
    return ptrScreenToWorldPoint_Func;
}

MapIntersect_pt MemMgrClass::GetMapIntersectPtr() {
    if (ptrMapIntersect_Func == 0) {

        ptrMapIntersect_Func = (MapIntersect_pt)GW::Scanner::Find("\xff\x75\x08\xd9\x5d\xfc\xff\x77\x7c", "xxxxxxxxx", -0x27);

        if (ptrMapIntersect_Func == 0) {
            WriteChat(GW::Chat::CHANNEL_GWCA1, L"Failed to find base address.", L"Gwtoolbot");
        }
    }
    return ptrMapIntersect_Func;
}

float* MemMgrClass::GetC2M_MouseX()
{
    uintptr_t basePointer = GetBasePtr();
    if (basePointer == 0) return nullptr;

    return reinterpret_cast<float*>(basePointer + 0x4ECC);
}

float* MemMgrClass::GetC2M_MouseY()
{
    uintptr_t basePointer = GetBasePtr();
    if (basePointer == 0) return nullptr;
    return reinterpret_cast<float*>(basePointer + 0x4ED0);
}

GW::Vec2f MemMgrClass::GetClickToMoveCoords()
{
    
    GW::Vec2f res; 
    res.x = *GetC2M_MouseX();
    res.x = *GetC2M_MouseY();

    return res;
}
