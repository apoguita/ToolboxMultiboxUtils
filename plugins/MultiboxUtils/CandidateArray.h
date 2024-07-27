#pragma once

#include "Headers.h"

struct Scandidates {
    int player_ID = 0;
    GW::Constants::MapID MapID = GW::Constants::MapID::None;
    GW::Constants::ServerRegion MapRegion = GW::Constants::ServerRegion::Unknown;
    int MapDistrict = 0;

    bool operator==(const Scandidates& other) const {
        return player_ID == other.player_ID &&
            MapID == other.MapID &&
            MapRegion == other.MapRegion &&
            MapDistrict == other.MapDistrict;
    }
};

class CandidateArray {
public:

    CandidateArray() : size(0) {
        for (int i = 0; i < 8; ++i) {
            candidates[i] = { 0, GW::Constants::MapID::None, GW::Constants::ServerRegion::Unknown, 0 }; // Initialize all elements to default Scandidates
        }
    }

    int push(const Scandidates& candidate) {
        if (size < 8 && !exists(candidate)) {
            candidates[size++] = candidate;
            return size;
        }
        return 0;
    }

    bool pop() {
        if (size > 0) {
            candidates[--size] = { 0, GW::Constants::MapID::None, GW::Constants::ServerRegion::Unknown, 0 }; // Clear the popped value
            return true;
        }
        return false; // Array is empty
    }

    Scandidates get(int index) const {
        if (index >= 0 && index < size) {
            return candidates[index];
        }
        return { 0, GW::Constants::MapID::None, GW::Constants::ServerRegion::Unknown, 0 }; // Invalid index
    }

    bool set(int index, Scandidates value) {
        if (index >= 0 && index < size) {
            candidates[index] = value;
            return true;
        }
        return false; // Invalid index
    }

    int getSize() const {
        return size;
    }

    void advancePtr() {
        size++;
        if (size >= 8) { 
            size = 0; 
            reset();
        }
    }

    bool exists(const Scandidates& candidate) const {
        for (int i = 0; i < size; ++i) {
            if (candidates[i] == candidate) {
                return true;
            }
        }
        return false;
    }

    Scandidates find(const Scandidates& candidate) const {
        for (int i = 0; i < size; ++i) {
            if (candidates[i] == candidate) {
                return candidates[i];
            }
        }
        return { 0, GW::Constants::MapID::None, GW::Constants::ServerRegion::Unknown, 0 }; // Return a default Scandidates object if not found
    }

    void print() const {
        for (int i = 0; i < size; ++i) {
            std::cout << "Player ID: " << candidates[i].player_ID
               // << ", MapID: " << candidates[i].MapID
                //<< ", Mapregion: " << candidates[i].Mapregion
                << ", MapDistrict: " << candidates[i].MapDistrict << std::endl;
        }
    }


    void reset() {
        for (int i = 0; i < 8; ++i) {
            pop();
        }
    }

private:
    Scandidates candidates[8];
    int size =0;
};


class PartyCandidates {
public:
    int Subscribe(const Scandidates& candidate);
    void Unsubscribe(int unique_key);
    bool IsClientAvailable(int unique_key);
    Scandidates* ListClients();
    void ClearAllClients();
    void DoKeepAlive(int unique_key);
    int GetEachKeepalive(int index) { return Keepalive[index]; }
    int GetKeepaliveCounter() { return KeepAliveCounter; }

private:
    Scandidates AllCandidates[8];
    int UniqueKeys[8] = { 0 };
    int Keepalive[8] = { 0 };
    int KeepAliveOld[8] = { 0 };
    int next_unique_key=1;

    int GenerateUniqueKey();
    int FindIndexByKey(int unique_key);
    int KeepAliveCounter;
    int KeepAliveTarget=500;
};


int PartyCandidates::Subscribe(const Scandidates& candidate) {
    // Check if the candidate already exists and return its unique key
    for (int i = 0; i < 8; ++i) {
        if ((AllCandidates[i].player_ID == candidate.player_ID) &&
            (AllCandidates[i].MapID == candidate.MapID) &&
            (AllCandidates[i].MapRegion == candidate.MapRegion) &&
            (AllCandidates[i].MapDistrict == candidate.MapDistrict))
        {
            return UniqueKeys[i];
        }
    }

    // If the candidate does not exist, find an available slot and insert it
    for (int i = 0; i < 8; ++i) {
        if (UniqueKeys[i] == 0) { // Check if slot is available (unique_key 0 means empty)
            int unique_key = GenerateUniqueKey();
            AllCandidates[i] = candidate;
            UniqueKeys[i] = unique_key;
            return unique_key;
        }
    }

    // Return -1 if no space is available
    return -1;
}
void PartyCandidates::DoKeepAlive(int unique_key)
{
    // Check if the candidate already exists and return its unique key
    int index = FindIndexByKey(unique_key);
    if (index != -1) {
        Keepalive[index] ++; // Reset to default
        if (Keepalive[index] > 100000) { Keepalive[index] = 1; };
    }

    KeepAliveCounter++;

    if (KeepAliveCounter > 50) {
        for (int i = 0; i < 8; ++i) {
            if (Keepalive[i]== KeepAliveOld[i])
            {
                AllCandidates[i] = Scandidates(); // Reset to default
                UniqueKeys[i] = 0; // Mark slot as available
                Keepalive[i] = 0;
                KeepAliveOld[i] = 0;
            }
            else {
                KeepAliveOld[i] = Keepalive[i];
            }
        }
        KeepAliveCounter = 0;
    }
}

void PartyCandidates::Unsubscribe(int unique_key) {
    int index = FindIndexByKey(unique_key);
    if (index != -1) {
        AllCandidates[index] = Scandidates(); // Reset to default
        UniqueKeys[index] = 0; // Mark slot as available
    }
}

bool PartyCandidates::IsClientAvailable(int unique_key) {
    return FindIndexByKey(unique_key) != -1;
}

Scandidates* PartyCandidates::ListClients() {
    return AllCandidates;
}

void PartyCandidates::ClearAllClients() {
    for (int i = 0; i < 8; ++i) {
        AllCandidates[i] = Scandidates(); // Reset to default
        UniqueKeys[i] = 0; // Mark slot as available
    }
}

int PartyCandidates::GenerateUniqueKey() {
    if (next_unique_key == 0) { next_unique_key = 1; }
    return next_unique_key++;
}

int PartyCandidates::FindIndexByKey(int unique_key) {
    for (int i = 0; i < 8; ++i) {
        if (UniqueKeys[i] == unique_key) {
            return i;
        }
    }
    return -1; // Key not found
}
