#pragma once

#ifndef PCH_H
#define PCH_H

enum SkillTarget { Enemy, EnemyCaster, EnemyMartial, Ally, AllyCaster, AllyMartial, OtherAlly, DeadAlly, Self, Corpse, Minion, Spirit, Pet, EnemyMartialMelee, EnemyMartialRanged, AllyMartialMelee, AllyMartialRanged };
enum SkillNature { Offensive, OffensiveCaster, OffensiveMartial, Enchantment_Removal, Healing, Hex_Removal, Condi_Cleanse, Buff, EnergyBuff, Neutral, SelfTargetted, Resurrection, Interrupt };
//enum WeaponType { bow = 1, axe, hammer, daggers, scythe, spear, sword, wand = 10, staff1 = 12, staff2 = 14 }; // 1=bow, 2=axe, 3=hammer, 4=daggers, 5=scythe, 6=spear, 7=sWORD, 10=wand, 12=staff, 14=staff};

enum WeaponType { None =0, bow = 1, axe=2, hammer=3, daggers=4, scythe =5, spear=6, sword=7, scepter =8,staff=9,staff2 = 10, scepter2 = 12, staff3=13,staff4 = 14 }; // 1=bow, 2=axe, 3=hammer, 4=daggers, 5=scythe, 6=spear, 7=sWORD, 10=wand, 12=staff, 14=staff};


enum RemoteCommand {rc_None, rc_ForceFollow,rc_Pcons, rc_Title, rc_GetQuest, rc_Resign, rc_Chest};


#pragma comment(lib, "Shlwapi.lib")
#include <d3d11.h>
#include <DirectXMath.h>



// ----------   GW INCLUDES
#include <GWCA/GWCA.h>


#include <GWCA/Utilities/Macros.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameEntities/Pathing.h>

#include <GWCA/GameEntities/Skill.h>
#include <GWCA/Managers/SkillbarMgr.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Managers/AgentMgr.h>

#include <GWCA/GameEntities/Player.h>

#include <GWCA/GameEntities/Party.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/PlayerMgr.h>

#include <GWCA/GameEntities/Camera.h>
#include <GWCA/Managers/CameraMgr.h> 
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/RenderMgr.h>
#include <GWCA/Managers/UIMgr.h> 

#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/ChatMgr.h>

#include <Modules/Resources.h>
#include <Utils/GuiUtils.h>


#include <Modules/DialogModule.h>
//#include <Modules/DialogModule.cpp>


#include <GWCA/Managers/GameThreadMgr.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/ItemMgr.h>

#include <GWCA/Managers/EffectMgr.h>


#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/PartyContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/GameEntities/Hero.h>

//manually added libs

#include "Timer.h"
#include "SkillArray.h"
#include "CandidateArray.h"
#include "StateMachine.h"
#include "SpecialSkilldata.h"
#include "MemMgr.h"
#include "Overlay.h"

OverlayClass Overlay;


#include "Hexgrid.h"
#endif // PCH_H







