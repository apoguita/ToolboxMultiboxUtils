#pragma once

#include "Headers.h"

#define GAMEVARS_SIZE sizeof(MemPlayerStruct) 
#define SHM_NAME "GWHeroAISharedMemory"



class MemMgrClass {
private:
    uintptr_t Base_address = 0;
public:
    struct MemSinglePlayer {
        GW::AgentID ID = 0;
        float Energy = 0.0f;
        float energyRegen = 0.0f;
        bool IsActive = false;
        float x = 0.0f;
        float y = 0.0f;
    };

    struct MemPlayerStruct {
        MemSinglePlayer MemPlayers[8];
    };

    HANDLE hMapFile;
    void InitializeSharedMemory(MemPlayerStruct*& pData);

    MemPlayerStruct MemCopy;
    MemPlayerStruct* MemData;

    uintptr_t GetBasePointer();
    float* GetClickCoordsX();
    float* GetClickCoordsY();
    void SendCoords();

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
}

uintptr_t MemMgrClass::GetBasePointer() {
    if (Base_address == 0) {
        uintptr_t address = GW::Scanner::Find("\x50\x6A\x0F\x6A\x00\xFF\x35", "xxxxxxx", +7);

        Base_address = *(uintptr_t*)address;

        if (Base_address == 0) {
            WriteChat(GW::Chat::CHANNEL_GWCA1, L"Failed to find base address.", L"Gwtoolbot");
        }
    }
    return Base_address;
}

float* MemMgrClass::GetClickCoordsX()
{
    uintptr_t basePointer = GetBasePointer();
    if (basePointer == 0) return nullptr;

    return reinterpret_cast<float*>(basePointer + 0x4ECC);
}

float* MemMgrClass::GetClickCoordsY()
{
    uintptr_t basePointer = GetBasePointer();
    if (basePointer == 0) return nullptr;
    return reinterpret_cast<float*>(basePointer + 0x4ED0);
}

void MemMgrClass::SendCoords()
{
    float* clickCoordsX = GetClickCoordsX();
    float* clickCoordsY = GetClickCoordsY();

    if (clickCoordsX && clickCoordsY) {
        std::wstring xCoordString = std::to_wstring(*clickCoordsX);
        std::wstring yCoordString = std::to_wstring(*clickCoordsY);

        WriteChat(GW::Chat::CHANNEL_GWCA1, xCoordString.c_str(), L"Gwtoolbot");
        WriteChat(GW::Chat::CHANNEL_GWCA1, yCoordString.c_str(), L"Gwtoolbot");
    }
    else {
        WriteChat(GW::Chat::CHANNEL_GWCA1, L"Coordinates not available.", L"Gwtoolbot");
    }
}
