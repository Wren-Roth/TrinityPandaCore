/*
** Made by Atidote https://github.com/Atidot3
*/

#include "Common.h"
#include "spellinfo.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Config.h"
#include "World.h"
#include "Chat.h"

class example_announce : public PlayerScript
{
public:
    example_announce() : PlayerScript("example_announce") {}

    void OnLogin(Player* player) override
    {
        // Announce Module
        ChatHandler(player->GetSession()).SendSysMessage("This server is running the |cff4CFF00example announce |rmodule.");
    }

};


extern void AddSC_mod_example()
{
    new example_announce();
}
