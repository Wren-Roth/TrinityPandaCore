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

#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "Player.h"
#include "WorldSession.h"

enum GlasswebSpider
{
    NPC_GLASSWEB_SPIDER = 5856
};

enum GlasswebSpiderSpells
{
    SPELL_POISON = 744
};

enum GlasswebSpiderEvents
{
    EVENT_POISON = 1
};

class npc_glassweb_spider : public CreatureScript
{
public:
    npc_glassweb_spider() : CreatureScript("npc_glassweb_spider") {}

    struct npc_glassweb_spiderAI : public ScriptedAI
    {
        npc_glassweb_spiderAI(Creature* creature) : ScriptedAI(creature) {}

        EventMap events;

        void Reset() override
        {
            events.Reset();
            // keep default react state; no extra setup required here
        }

        void EnterCombat(Unit* who) override
        {
            // Ensure the spider engages the attacker immediately
            if (who)
                AttackStart(who);
            else if (Player* player = me->FindNearestPlayer(20.0f))
                AttackStart(player);

            // Schedule initial poison cast after ~1 second
            events.ScheduleEvent(EVENT_POISON, 1 * IN_MILLISECONDS);
        }

        void AttackStart(Unit* who) override
        {
            if (!who || !me->CanCreatureAttack(who, false))
                return;

            ScriptedAI::AttackStart(who);
        }

        void UpdateAI(uint32 diff) override
        {
            if (!UpdateVictim())
                return;

            events.Update(diff);

            while (uint32 eventId = events.ExecuteEvent())
            {
                switch (eventId)
                {
                case EVENT_POISON:
                {
                    // Cast poison on current victim if available
                    if (Unit* victim = me->GetVictim())
                        DoCast(victim, SPELL_POISON);

                    // Reschedule every 18 seconds
                    events.ScheduleEvent(EVENT_POISON, 18 * IN_MILLISECONDS);
                    break;
                }
                default:
                    break;
                }
            }

            DoMeleeAttackIfReady();
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_glassweb_spiderAI(creature);
    }
};

// Lunk / quest interaction
enum LunkAndQuest
{
    NPC_LUNK = 47429,
    NPC_INTERACT_TARGET = 47434, // the NPC the player interacts with to complete the quest
    QUEST_LUNK_ADVENTURE = 28034
};

// Minimal presence for Lunk (no behavior required beyond DB entries)
// Keep this so the script system knows about the NPC if you want to extend later.
class npc_lunk : public CreatureScript
{
public:
    npc_lunk() : CreatureScript("npc_lunk") {}

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (!player || !creature)
            return false;

        // If the creature is a quest giver, show the standard quest menu.
        if (creature->IsQuestGiver())
            player->PrepareQuestMenu(creature->GetGUID());

        return true;
    }
};

// The NPC the player interacts with to complete the quest (entry 47434)
class npc_lunk_rendan_interact : public CreatureScript
{
public:
    npc_lunk_rendan_interact() : CreatureScript("npc_lunk_rendan_interact") {}

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (!player || !creature)
            return false;

        // If player has the quest and it's incomplete, complete it.
        if (player->GetQuestStatus(QUEST_LUNK_ADVENTURE) == QUEST_STATUS_INCOMPLETE)
        {
            // Use the same pattern existing scripts use for completion.
            player->CompleteQuest(QUEST_LUNK_ADVENTURE, creature);
            player->PlayerTalkClass->SendCloseGossip();
            return true;
        }

        // Show default quest menu if the NPC is a quest giver, otherwise close.
        if (creature->IsQuestGiver())
            player->PrepareQuestMenu(creature->GetGUID());
        else
            player->PlayerTalkClass->SendCloseGossip();

        return true;
    }
};

void AddSC_searing_gorge()
{
    new npc_glassweb_spider();
    new npc_lunk();
    new npc_lunk_rendan_interact();
}