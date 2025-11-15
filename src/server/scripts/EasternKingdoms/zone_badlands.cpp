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

enum BadlandsDefs
{
    NPC_RHEA = 46654, // quest giver NPC (Rhea)
    NPC_NYXONDRA = 46658, // dragon that should sleep / wake
    QUEST_LIFTING_THE_VEIL = 27770  // quest that wakes the dragon
};

/*####################################################
# npc_nyxondra - dragon: sleeps by default, not selectable/attackable
#####################################################*/
class npc_nyxondra : public CreatureScript
{
public:
    npc_nyxondra() : CreatureScript("npc_nyxondra") {}

    struct npc_nyxondraAI : public ScriptedAI
    {
        npc_nyxondraAI(Creature* creature) : ScriptedAI(creature) {}

        void Reset() override
        {
            // Put the dragon to "sleep" by default and make it non-interactable
            me->HandleEmoteStateCommand(EMOTE_STATE_SLEEP);
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_ATTACKABLE_1);
            me->SetReactState(REACT_PASSIVE);
            me->CombatStop();
            me->AttackStop();
            me->DeleteThreatList();
        }

        void EnterCombat(Unit* /*who*/) override {}

        void JustDied(Unit* /*killer*/) override {}
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_nyxondraAI(creature);
    }
};

/*####################################################
# npc_rhea - quest giver: when player accepts QUEST_LIFTING_THE_VEIL,
# wake nearest dragon (NPC_NYXONDRA) so it becomes interactable/attackable
#####################################################*/
class npc_rhea : public CreatureScript
{
public:
    npc_rhea() : CreatureScript("npc_rhea_dragon_quest") {}

    struct npc_rheaAI : public ScriptedAI
    {
        npc_rheaAI(Creature* creature) : ScriptedAI(creature) {}

        void Reset() override {}

        void EnterCombat(Unit* /*who*/) override {}

        void JustDied(Unit* /*killer*/) override {}
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_rheaAI(creature);
    }

    bool OnQuestAccept(Player* /*player*/, Creature* creature, Quest const* quest) override
    {
        if (!quest)
            return false;

        if (quest->GetQuestId() != QUEST_LIFTING_THE_VEIL)
            return false;

        // Find the nearest Nyxondra instance (search radius 200). Adjust radius if needed.
        if (Creature* dragon = creature->FindNearestCreature(NPC_NYXONDRA, 200.0f, true))
        {
            // Wake the dragon: remove sleep state and make it selectable & attackable.
            dragon->HandleEmoteStateCommand(EMOTE_STATE_NONE);
            dragon->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
            dragon->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_ATTACKABLE_1);

            // Restore normal reaction (so it can be engaged). Do not force attack a player.
            dragon->InitializeReactState();
            dragon->SetReactState(REACT_AGGRESSIVE);

            // Ensure AI updates to awake state
            if (UnitAI* uai = dragon->GetAI())
                uai->Reset();
        }

        return true;
    }
};

void AddSC_zone_badlands()
{
    new npc_nyxondra();
    new npc_rhea();
}