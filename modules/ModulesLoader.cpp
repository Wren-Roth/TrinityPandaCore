#include "Define.h"
#include <vector>
#include <string>

// Includes list
void AddSC_solocraft_system();
void AddSC_mod_items();
void AddSC_mod_leech();
void AddSC_mod_example();


#include "ModulesLoader.h"

/// Exposed in script modules to register all scripts to the ScriptMgr.
void AddModulesScripts()
{
    // Modules
    AddSC_solocraft_system();
    AddSC_mod_items();
    AddSC_mod_leech();
    AddSC_mod_example();

}
