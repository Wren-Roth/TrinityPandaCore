/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "Leech.h"
#include <sstream>
#include <set>
#include <string>

class Leech_UnitScript : public UnitScript
{
public:
    Leech_UnitScript() : UnitScript("Leech_UnitScript")
    {

        // Parse disallowed specs from config
        std::string specStr = sConfigMgr->GetStringDefault("Leech.DisallowedSpecs", "");
        std::istringstream ss(specStr);
        std::string token;
        while (std::getline(ss, token, ','))
        {
            try
            {
                disallowedSpecs.insert(static_cast<uint32>(std::stoul(token)));
            }
            catch (...) { /* ignore invalid entries */ }
        }
    }

    void OnDamage(Unit* attacker, Unit* victim, uint32& damage) override
    {

        if (!sConfigMgr->GetBoolDefault("Leech.Enable", true) || !attacker)
        {
            return;
        }

        bool isPet = attacker->GetOwner() && attacker->GetOwner()->GetTypeId() == TYPEID_PLAYER;
        if (!isPet && attacker->GetTypeId() != TYPEID_PLAYER)
        {
            return;
        }
        Player* player = isPet ? attacker->GetOwner()->ToPlayer() : attacker->ToPlayer();

        uint32 playerSpec = player->GetLootSpecOrClassSpec();
        if (disallowedSpecs.count(playerSpec))
        {
            return;
        }
       /* player->SendPersonalMessage(
            "Your spec ID: " + std::to_string(player->GetLootSpecOrClassSpec()),
            CHAT_MSG_WHISPER,
            LANG_UNIVERSAL
        );*/
        if (sConfigMgr->GetBoolDefault("Leech.DungeonsOnly", true) && !(player->GetMap()->IsDungeon()))
        {
            return;
        }
       
        auto leechAmount = sConfigMgr->GetFloatDefault("Leech.Amount", 0.05f);
        auto bp1 = static_cast<int32>(leechAmount * float(damage));
        player->CastCustomSpell(attacker, SPELL_HEAL, &bp1, nullptr, nullptr, true);
    }

private:
    std::set<uint32> disallowedSpecs;
};
// Add all scripts in one
void AddSC_mod_leech()
{
    new Leech_UnitScript();
}
