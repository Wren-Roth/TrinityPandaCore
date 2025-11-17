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
SDName: Blasted_Lands
SD%Complete: 90
SDComment: Quest support: 3628. Teleporter to Rise of the Defiler missing group support.
SDCategory: Blasted Lands
EndScriptData */

/* ContentData
npc_deathly_usher
EndContentData */

#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "Player.h"
#include <unordered_map>
#include <ctime>

/*######
## npc_deathly_usher
######*/

#define GOSSIP_ITEM_USHER "I wish to to visit the Rise of the Defiler."

enum DeathlyUsher
{
    SPELL_TELEPORT_SINGLE               = 12885,
    SPELL_TELEPORT_SINGLE_IN_GROUP      = 13142,
    SPELL_TELEPORT_GROUP                = 27686
};

class npc_deathly_usher : public CreatureScript
{
public:
    npc_deathly_usher() : CreatureScript("npc_deathly_usher") { }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        player->PlayerTalkClass->ClearMenus();
        if (action == GOSSIP_ACTION_INFO_DEF)
        {
            player->CLOSE_GOSSIP_MENU();
            creature->CastSpell(player, SPELL_TELEPORT_SINGLE, true);
        }

        return true;
    }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (player->GetQuestStatus(3628) == QUEST_STATUS_INCOMPLETE && player->HasItemCount(10757))
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_ITEM_USHER, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF);

        player->SEND_GOSSIP_MENU(player->GetGossipTextId(creature), creature->GetGUID());

        return true;
    }
};

class npc_darktail_bonepicker : public CreatureScript
{
public:
    npc_darktail_bonepicker() : CreatureScript("npc_darktail_bonepicker") {}

    struct npc_darktail_bonepickerAI : public ScriptedAI
    {
        npc_darktail_bonepickerAI(Creature* creature) : ScriptedAI(creature) {}

        void Reset() override
        {
            // Make passive in world and ensure flying flags are applied client- and server-side.
            me->SetReactState(REACT_PASSIVE);

            // Force flying / airborne state so the client won't play falling animations
            me->SetCanFly(true);
            me->SetFlying(true);
            me->OverrideInhabitType(INHABIT_AIR);

            // Optional: ensure flight speed is reasonable (adjust multiplier as needed)
            me->SetSpeed(MOVE_FLIGHT, 1.0f);

            // Push movement flags to client
            me->UpdateMovementFlags();
        }

        void EnterEvadeMode() override
        {
            // Keep flying state while evading / resetting
            me->SetCanFly(true);
            me->SetFlying(true);
            me->OverrideInhabitType(INHABIT_AIR);
            me->UpdateMovementFlags();

            ScriptedAI::EnterEvadeMode();
        }

        // If the creature uses waypoint movement and still glitches, this hook can reapply flags
        void MovementInform(uint32 /*type*/, uint32 /*id*/) override
        {
            // Re-assert flying flags after reaching waypoints — prevents transient falling state
            me->SetCanFly(true);
            me->SetFlying(true);
            me->OverrideInhabitType(INHABIT_AIR);
            me->UpdateMovementFlags();
        }

        // React to being hit by a spell: if spell 78838 hits this creature, credit and kill it.
        void SpellHit(Unit* caster, const SpellInfo* spell) override
        {
            if (!spell)
                return;

            constexpr uint32 SPELL_KILLER = 78838;

            if (spell->Id != SPELL_KILLER)
                return;

            if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
                return;

            Player* player = caster->ToPlayer();
            if (!player)
                return;

            // Give the player kill credit for this creature
            player->KilledMonsterCredit(me->GetEntry(), me->GetGUID());

            // Make the creature die/vanish cleanly.
            // Use DisappearAndDie if available to play death properly; fallback to DespawnOrUnsummon.
            if (me->IsAlive())
            {
                // Attempt a proper death
                me->DisappearAndDie();
                // If DisappearAndDie doesn't remove immediately, ensure removal:
                if (me->IsInWorld())
                    me->DespawnOrUnsummon(1);
            }
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_darktail_bonepickerAI(creature);
    }
};

// Gilnean Settler (42249) -> summon Drowned Gilnean Spirit (42252) on death.
// Drowned Gilnean Spirit will say "Thank you." (SAY_THANK_YOU) and despawn in 3 seconds.
enum GilneanSettlerNPCs
{
    NPC_GILNEAN_SETTLER = 42249,
    NPC_DROWNED_GILNEAN_SPIRIT = 42252
};

enum GilneanSpiritTexts
{
    SAY_THANK_YOU = 0
};

class npc_gilnean_settler : public CreatureScript
{
public:
    npc_gilnean_settler() : CreatureScript("npc_gilnean_settler") {}

    struct npc_gilnean_settlerAI : public ScriptedAI
    {
        npc_gilnean_settlerAI(Creature* creature) : ScriptedAI(creature) {}

        void JustDied(Unit* /*killer*/) override
        {
            // spawn the spirit at the settler's location with a short timed despawn (safety)
            float x = me->GetPositionX();
            float y = me->GetPositionY();
            float z = me->GetPositionZ();
            float o = me->GetOrientation();

            // TEMPSUMMON_TIMED_DESPAWN with 3 seconds lifespan for the spirit (3000 ms)
            if (Creature* spirit = me->SummonCreature(NPC_DROWNED_GILNEAN_SPIRIT, x, y, z, o, TEMPSUMMON_TIMED_DESPAWN, 3000))
            {
                // Ensure spirit is passive and visible immediately
                spirit->SetReactState(REACT_PASSIVE);

                // If the summoned creature uses an AI Talk() entry, we trigger it from its AI on spawn.
                // Otherwise you can set text in DB for the summoned NPC entry under its text id 0 = "Thank you."
            }

            // Keep default death handling
            ScriptedAI::JustDied(nullptr);
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_gilnean_settlerAI(creature);
    }
};


// Drowned Gilnean Spirit behaviour: text emote "Thank you." then despawn after 3 seconds.
class npc_drowned_gilnean_spirit : public CreatureScript
{
public:
    npc_drowned_gilnean_spirit() : CreatureScript("npc_drowned_gilnean_spirit") {}

    struct npc_drowned_gilnean_spiritAI : public ScriptedAI
    {
        npc_drowned_gilnean_spiritAI(Creature* creature) : ScriptedAI(creature) {}

        void IsSummonedBy(Unit* /*summoner*/) override
        {
            // No text emote — simply remain passive and despawn after 3 seconds.
            me->SetReactState(REACT_PASSIVE);
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);

            // Despawn after 3 seconds (3000 ms)
            me->DespawnOrUnsummon(3 * IN_MILLISECONDS);
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_drowned_gilnean_spiritAI(creature);
    }
};

class npc_abandoned_bloodwash_crate : public CreatureScript
{
public:
    npc_abandoned_bloodwash_crate() : CreatureScript("npc_abandoned_bloodwash_crate") {}

    // Complete the quest immediately when accepted from this NPC
    bool OnQuestAccept(Player* player, Creature* creature, Quest const* quest) override
    {
        if (!player || !creature || !quest)
            return false;

        constexpr uint32 NPC_ABANDONED_BLOODWASH = 41402;
        constexpr uint32 QUEST_THE_FUTURE_OF_ROCKPOOL = 25707;

        if (quest->GetQuestId() != QUEST_THE_FUTURE_OF_ROCKPOOL)
            return false;

        if (creature->GetEntry() != NPC_ABANDONED_BLOODWASH)
            return false;

        // Only act if quest is currently incomplete in player's log
        if (player->GetQuestStatus(QUEST_THE_FUTURE_OF_ROCKPOOL) != QUEST_STATUS_INCOMPLETE)
            return false;

        // Satisfy objective and force-complete the quest
        player->QuestObjectiveSatisfy(NPC_ABANDONED_BLOODWASH, 1, 0, creature->GetGUID());
        player->CompleteQuest(QUEST_THE_FUTURE_OF_ROCKPOOL, true);

        return true;
    }

    // Keep the clickable fallback (spellclick) behavior so older clients/interactions still work
    struct npc_abandoned_bloodwash_crateAI : public ScriptedAI
    {
        npc_abandoned_bloodwash_crateAI(Creature* creature) : ScriptedAI(creature) {}

        void Reset() override
        {
            me->SetFlag(UNIT_FIELD_NPC_FLAGS, UNIT_NPC_FLAG_SPELLCLICK);
            me->SetReactState(REACT_PASSIVE);
        }

        void OnSpellClick(Unit* clicker, bool& /*result*/) override
        {
            if (!clicker || clicker->GetTypeId() != TYPEID_PLAYER)
                return;

            Player* player = clicker->ToPlayer();
            if (!player)
                return;

            constexpr uint32 NPC_ABANDONED_BLOODWASH = 41402;
            constexpr uint32 QUEST_THE_FUTURE_OF_ROCKPOOL = 25707;

            if (me->GetEntry() != NPC_ABANDONED_BLOODWASH)
                return;

            if (player->GetQuestStatus(QUEST_THE_FUTURE_OF_ROCKPOOL) != QUEST_STATUS_INCOMPLETE)
                return;

            player->QuestObjectiveSatisfy(NPC_ABANDONED_BLOODWASH, 1, 0, me->GetGUID());
            player->CompleteQuest(QUEST_THE_FUTURE_OF_ROCKPOOL, true);

            me->RemoveFlag(UNIT_FIELD_NPC_FLAGS, UNIT_NPC_FLAG_SPELLCLICK);
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_abandoned_bloodwash_crateAI(creature);
    }
};

enum BonepickerFelfeederNPC
{
    NPC_BONEPICKER_FELFEEDER = 5983
};

class npc_bonepicker_felfeeder : public CreatureScript
{
public:
    npc_bonepicker_felfeeder() : CreatureScript("npc_bonepicker_felfeeder") {}

    struct npc_bonepicker_felfeederAI : public ScriptedAI
    {
        npc_bonepicker_felfeederAI(Creature* creature) : ScriptedAI(creature) {}

        void Reset() override
        {
            me->SetReactState(REACT_PASSIVE);

            // Keep creature in flying state to avoid falling animations between waypoints
            me->SetCanFly(true);
            me->SetFlying(true);
            me->OverrideInhabitType(INHABIT_AIR);

            // Ensure reasonable flight speed
            me->SetSpeed(MOVE_FLIGHT, 1.0f);

            // Push movement flags to client
            me->UpdateMovementFlags();
        }

        void EnterEvadeMode() override
        {
            // Re-assert flying state on evade/reset
            me->SetCanFly(true);
            me->SetFlying(true);
            me->OverrideInhabitType(INHABIT_AIR);
            me->UpdateMovementFlags();

            ScriptedAI::EnterEvadeMode();
        }

        void MovementInform(uint32 /*type*/, uint32 /*id*/) override
        {
            // Re-assert flying flags after reaching waypoints — prevents transient falling state
            me->SetCanFly(true);
            me->SetFlying(true);
            me->OverrideInhabitType(INHABIT_AIR);
            me->UpdateMovementFlags();
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_bonepicker_felfeederAI(creature);
    }
};



class npc_rahk_bunny : public CreatureScript
{
public:
    npc_rahk_bunny() : CreatureScript("npc_rahk_bunny") {}

    struct npc_rahk_bunnyAI : public ScriptedAI
    {
        npc_rahk_bunnyAI(Creature* creature) : ScriptedAI(creature), m_scanTimer(SCAN_INTERVAL_MS) {}

        // per-player cooldown (GUID -> last teleport time in seconds)
        std::unordered_map<uint64, uint32> m_lastTeleport;

        // scan timer
        uint32 m_scanTimer;
        static constexpr uint32 COOLDOWN_S = 5;                 // seconds
        static constexpr uint32 SCAN_INTERVAL_MS = 500;         // scan every 500 ms

        void Reset() override
        {
            // Keep the bunny visible and non-interactable
            me->SetVisible(true);
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);
            me->SetReactState(REACT_PASSIVE);

            m_scanTimer = SCAN_INTERVAL_MS;
            m_lastTeleport.clear();
        }

        void UpdateAI(uint32 diff) override
        {
            if (m_scanTimer > diff)
            {
                m_scanTimer -= diff;
                return;
            }
            m_scanTimer = SCAN_INTERVAL_MS;

            // Configurable values - change these to your desired destination / radius
            constexpr uint32 NPC_ENTRY = 479712;
            constexpr uint32 REQUIRED_QUEST = 26171;
            constexpr float  TRIGGER_RADIUS = 5.0f;           // distance in yards to trigger teleport
            constexpr uint32 TARGET_MAP_ID = 0;              // set your target map id
            constexpr float  TARGET_X = -11245.89f;     // set destination X
            constexpr float  TARGET_Y = -2830.72f;      // set destination Y
            constexpr float  TARGET_Z = 159.77f;        // set destination Z
            constexpr float  TARGET_O = 5.512159f;      // set destination orientation

            // Only run for configured NPC entry
            if (me->GetEntry() != NPC_ENTRY)
                return;

            Map* map = me->GetMap();
            if (!map)
                return;

            // Iterate players on the same map
            auto const& players = map->GetPlayers();
            for (auto itr = players.begin(); itr != players.end(); ++itr)
            {
                Player* player = itr->GetSource();
                if (!player)
                    continue;

                // Only consider players with the required in-progress quest
                if (player->GetQuestStatus(REQUIRED_QUEST) != QUEST_STATUS_INCOMPLETE)
                    continue;

                // Distance check
                if (!me->IsWithinDistInMap(player, TRIGGER_RADIUS))
                    continue;

                // per-player cooldown (use server time)
                uint32 now = static_cast<uint32>(std::time(nullptr));
                auto it = m_lastTeleport.find(player->GetGUID());
                if (it != m_lastTeleport.end() && now - it->second < COOLDOWN_S)
                    continue;
                m_lastTeleport[player->GetGUID()] = now;

                // Small safety: don't teleport if already essentially at destination
                if (player->GetMapId() == TARGET_MAP_ID)
                {
                    const float dx = player->GetPositionX() - TARGET_X;
                    const float dy = player->GetPositionY() - TARGET_Y;
                    const float dz = player->GetPositionZ() - TARGET_Z;
                    const float distSq = dx * dx + dy * dy + dz * dz;
                    if (distSq < 1.0f) // within 1 yard
                        continue;
                }

                // Teleport: NearTeleportTo when same map, TeleportTo otherwise
                if (player->GetMapId() == TARGET_MAP_ID)
                    player->NearTeleportTo(TARGET_X, TARGET_Y, TARGET_Z, TARGET_O);
                else
                    player->TeleportTo(TARGET_MAP_ID, TARGET_X, TARGET_Y, TARGET_Z, TARGET_O);
            }
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_rahk_bunnyAI(creature);
    }
};

class npc_rahk_bunny2 : public CreatureScript
{
public:
    npc_rahk_bunny2() : CreatureScript("npc_rahk_bunny2") {}

    struct npc_rahk_bunny2AI : public ScriptedAI
    {
        npc_rahk_bunny2AI(Creature* creature) : ScriptedAI(creature), m_scanTimer(SCAN_INTERVAL_MS) {}

        std::unordered_map<uint64, uint32> m_lastTeleport;
        uint32 m_scanTimer;
        static constexpr uint32 COOLDOWN_S = 5;                 // seconds
        static constexpr uint32 SCAN_INTERVAL_MS = 500;         // scan every 500 ms

        void Reset() override
        {
            // Keep the bunny visible and non-interactable
            me->SetVisible(true);
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);
            me->SetReactState(REACT_PASSIVE);

            m_scanTimer = SCAN_INTERVAL_MS;
            m_lastTeleport.clear();
        }

        void UpdateAI(uint32 diff) override
        {
            if (m_scanTimer > diff)
            {
                m_scanTimer -= diff;
                return;
            }
            m_scanTimer = SCAN_INTERVAL_MS;

            constexpr uint32 NPC_ENTRY = 479713;
            constexpr uint32 REQUIRED_QUEST = 26171;
            constexpr float  TRIGGER_RADIUS = 5.0f;           // distance in yards to trigger teleport
            constexpr uint32 TARGET_MAP_ID = 0;               // set your target map id if different
            constexpr float  TARGET_X = -11156.06f;           // destination X
            constexpr float  TARGET_Y = -2995.48f;            // destination Y
            constexpr float  TARGET_Z = 8.51f;              // destination Z
            constexpr float  TARGET_O = 3.823557f;            // destination orientation

            if (me->GetEntry() != NPC_ENTRY)
                return;

            Map* map = me->GetMap();
            if (!map)
                return;

            auto const& players = map->GetPlayers();
            for (auto itr = players.begin(); itr != players.end(); ++itr)
            {
                Player* player = itr->GetSource();
                if (!player)
                    continue;

                

                if (!me->IsWithinDistInMap(player, TRIGGER_RADIUS))
                    continue;

                uint32 now = static_cast<uint32>(std::time(nullptr));
                auto it = m_lastTeleport.find(player->GetGUID());
                if (it != m_lastTeleport.end() && now - it->second < COOLDOWN_S)
                    continue;
                m_lastTeleport[player->GetGUID()] = now;

                if (player->GetMapId() == TARGET_MAP_ID)
                {
                    const float dx = player->GetPositionX() - TARGET_X;
                    const float dy = player->GetPositionY() - TARGET_Y;
                    const float dz = player->GetPositionZ() - TARGET_Z;
                    const float distSq = dx * dx + dy * dy + dz * dz;
                    if (distSq < 1.0f)
                        continue;
                }

                if (player->GetMapId() == TARGET_MAP_ID)
                    player->NearTeleportTo(TARGET_X, TARGET_Y, TARGET_Z, TARGET_O);
                else
                    player->TeleportTo(TARGET_MAP_ID, TARGET_X, TARGET_Y, TARGET_Z, TARGET_O);
            }
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_rahk_bunny2AI(creature);
    }
};



void AddSC_blasted_lands()
{
    new npc_deathly_usher();
	new npc_darktail_bonepicker();
    new npc_gilnean_settler();
	new npc_drowned_gilnean_spirit();
	new npc_abandoned_bloodwash_crate();
	new npc_bonepicker_felfeeder();
	new npc_rahk_bunny();
	new npc_rahk_bunny2();
	

}
