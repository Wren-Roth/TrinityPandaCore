#include "Define.h"
#include <vector>
#include <string>

// Includes list
void AddSC_delete_gear();
void AddSC_init();
void AddSC_mod_items();
void AddSC_mod_leech();
void AddSC_mod_example();
void AddSC_solocraft_system();


#include "ModulesLoader.h"

/// Exposed in script modules to register all scripts to the ScriptMgr.
void AddModulesScripts()
{
    // Modules
        AddSC_delete_gear();
    AddSC_init();
    AddSC_mod_items();
    AddSC_mod_leech();
    AddSC_mod_example();
    AddSC_solocraft_system();

}
