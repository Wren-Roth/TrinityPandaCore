#include "Define.h"
#include <vector>
#include <string>

// Includes list
void AddSC_balance_dungeon_boost();
void AddSC_mod_leech();
void AddSC_mod_example();
void AddSC_items_gearup();

#include "ModulesLoader.h"

/// Exposed in script modules to register all scripts to the ScriptMgr.
void AddModulesScripts()
{
    // Modules
	AddSC_balance_dungeon_boost();
    AddSC_mod_leech();
    AddSC_mod_example();
    AddSC_items_gearup();

}
