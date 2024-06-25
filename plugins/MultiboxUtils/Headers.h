#pragma once

#ifndef PCH_H
#define PCH_H

enum SkillTarget { Enemy, EnemyCaster, EnemyMartial, Ally, AllyCaster, AllyMartial, OtherAlly, DeadAlly, Self, Corpse, Minion, Spirit, Pet };
enum SkillNature { Offensive, OffensiveCaster, OffensiveMartial, Enchantment_Removal, Healing, Hex_Removal, Condi_Cleanse, Buff, EnergyBuff, Neutral, SelfTargetted, Resurrection, Interrupt };
enum WeaponType { bow = 1, axe, hammer, daggers, scythe, spear, sword, wand = 10, staff1 = 12, staff2 = 14 }; // 1=bow, 2=axe, 3=hammer, 4=daggers, 5=scythe, 6=spear, 7=sWORD, 10=wand, 12=staff, 14=staff};




#pragma comment(lib, "Shlwapi.lib")
#include <d3d11.h>
#include <DirectXMath.h>

// ----------   GW INCLUDES
#include <GWCA/GWCA.h>

#include <GWCA/Utilities/Macros.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameEntities/Skill.h>
#include <GWCA/Managers/SkillbarMgr.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Managers/AgentMgr.h>

#include <GWCA/GameEntities/Party.h>
#include <GWCA/Managers/PartyMgr.h>


#include <GWCA/GameEntities/Camera.h>
#include <GWCA/Managers/CameraMgr.h> 
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/RenderMgr.h>
#include <GWCA/Managers/UIMgr.h> 

#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/ItemMgr.h>

#include <GWCA/Managers/EffectMgr.h>

//manually added libs

#include "SpecialSkilldata.h"
#include "MemMgr.h"
#include "StateMachine.h"
#endif // PCH_H







