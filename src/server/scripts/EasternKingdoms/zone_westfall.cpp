/*
 * Copyright (C) 2011-2016 Project SkyFire <http://www.projectskyfire.org/>
 * Copyright (C) 2008-2016 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2016 MaNGOS <http://getmangos.com/>
 * Copyright (C) 2006-2014 ScriptDev2 <https://github.com/scriptdev2/scriptdev2/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/* ScriptData
SDName: Westfall
SD%Complete: 0
SDComment:
SDCategory: Westfall
EndScriptData */

/* ContentData
EndContentData */
#include "Group.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedEscortAI.h"
#include "PassiveAI.h"
#include "Vehicle.h"
#include "GameObjectAI.h"
#include "TaskScheduler.h"
#include "SpellScript.h"
#include "UnitAI.h"


    enum WestfallQuest
    {
        QUEST_MURDER_WAS_THE_CASE_THAT_THEY_GAVE_ME = 26209,
        QUEST_LOUS_PARTING_THOUGHTS = 26232
    };

    enum ThugText
    {
        SAY_THUG_0 = 0,
        SAY_THUG_1 = 1,
        SAY_THUG_2 = 2,
        SAY_THUG_3 = 3,
        SAY_THUG_4 = 4,
        SAY_THUG_5 = 5,
        SAY_THUG_6 = 6,
        SAY_WARNING = 0
    };

    enum ThugEvent
    {
        EVENT_SUMMON_THUGS = 1,
        EVENT_THUG1_SAY_0 = 2,
        EVENT_THUG2_SAY_1 = 3,
        EVENT_THUG2_SAY_2 = 4,
        EVENT_THUG3_SAY_3 = 5,
        EVENT_THUG1_SAY_4 = 6,
        EVENT_THUG1_SAY_5 = 7,
        EVENT_THUG1_SAY_6 = 8,
        EVENT_THUG_CREDIT = 9,
        EVENT_THUG_RESET = 10,
        EVENT_THUG_SHOOT_1 = 11,
        EVENT_THUG_SHOOT_2 = 12,
        EVENT_THUG_SCREAM = 13,
        EVENT_THUG_WARNING = 14
    };

    enum ThugAction
    {
        ACTION_THUG_RESET = 1
    };

    enum ThugData
    {
        DATA_THUG_DEATH = 1
    };

    enum ThugMisc
    {
        OBJECT_SOUND_SHOOTING = 15071,
        OBJECT_SOUND_SCREAM = 17852
    };
    enum WestfallCreature
    {
        NPC_HOMELESS_STORMWIND_CITIZEN_1 = 42386,
        NPC_HOMELESS_STORMWIND_CITIZEN_2 = 42384,
        NPC_WEST_PLAINS_DRIFTER = 42391,
        NPC_TRANSIENT = 42383,
        NPC_THUG = 42387,
        MPC_LOUS_PARTING_THOUGHTS_TRIGGER = 42562,
        NPC_SMALL_TIME_HUSTLER = 42390,
        NPC_MERCENARY = 42656,
        NPC_AGENT_KEARNEN = 7024
    };

    enum WestfallSpell
    {
        SPELL_HOBO_INFORMATION_1 = 79181,
        SPELL_HOBO_INFORMATION_2 = 79182,
        SPELL_HOBO_INFORMATION_3 = 79183,
        SPELL_HOBO_INFORMATION_4 = 79184,
        SPELL_SUMMON_RAGAMUFFIN_LOOTER = 79169,
        SPELL_SUMMON_RAGAMUFFIN_LOOTER_1 = 79170,
        SPELL_SUMMON_RAGAMUFFIN_LOOTER_2 = 79171,
        SPELL_SUMMON_RAGAMUFFIN_LOOTER_3 = 79172,
        SPELL_SUMMON_RAGAMUFFIN_LOOTER_4 = 79173,
        SPELL_AGGRO_HOBO = 79168,
        SPELL_HOBO_INFORMATION = 79184,
        SPELL_KILL_SHOT_TRIGGERED = 79525
    };


// Wake Harvest Golem 79436
class spell_westfall_wake_harvest_golem : public SpellScript
{
    PrepareSpellScript(spell_westfall_wake_harvest_golem);

    void HandleHit(SpellEffIndex effIndex)
    {
        if (Player* caster = GetCaster()->ToPlayer())
            caster->KilledMonsterCredit(GetSpellInfo()->Effects[EFFECT_1].MiscValue);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_westfall_wake_harvest_golem::HandleHit, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }

};



class two_shoed_lou_thugs : public CreatureScript
{
public:
    two_shoed_lou_thugs(const char* ScriptName) : CreatureScript(ScriptName) {}

    bool OnQuestAccept(Player* player, Creature* creature, Quest const* quest)
    {
        if (quest->GetQuestId() == QUEST_LOUS_PARTING_THOUGHTS)
        {                             //the quest definition


            player->CastSpell(player, SPELL_HOBO_INFORMATION, false);
            creature->AI()->Talk(0);    // the spell definition
        }
        return true;
    }



    CreatureAI* GetAI(Creature* creature) const override
    {
        return new two_shoed_lou_thugsAI(creature);
    }
    struct two_shoed_lou_thugsAI : public ScriptedAI
    {
        two_shoed_lou_thugsAI(Creature* creature) : ScriptedAI(creature)
        {
        }

        void UpdateAI(uint32 diff) override
        {
            if (!UpdateVictim())
                return;
        }
    };
};



    










void AddSC_westfall()
{
    new spell_script<spell_westfall_wake_harvest_golem>("spell_westfall_wake_harvest_golem");
    new two_shoed_lou_thugs("two_shoed_lou_thugs");
}
