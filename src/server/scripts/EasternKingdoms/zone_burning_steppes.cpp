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
SDName: Burning_Steppes
SD%Complete: 0
SDComment:
SDCategory: Burning Steppes
EndScriptData */
#include "creature.h"
#include "Map.h"
#include "scriptmgr.h"
#include "SpellScript.h"


enum ScrappedGolemsType
{
    SPELL_CREATE_WAR_REAVER_PARTS_AB = 89413, // Sieve & Piston
    SPELL_CREATE_WAR_REAVER_PARTS_AC = 89416, // Sieve & Gearshaft
    SPELL_CREATE_WAR_REAVER_PARTS_AD = 89417, // Sieve & Core
    SPELL_CREATE_WAR_REAVER_PARTS_BC = 89418, // Piston & Gearshaft
    SPELL_UPPERCUTE                  = 10966,
    SPELL_FLAME_BLAST                = 84165,
    SPELL_BATTLE_READY               = 89353, // first defeat
    SPELL_WELL_HONED                 = 89352, // second defeat
    SPELL_PERFECTLY_TUNNED           = 89355, // credit after this
    SPELL_HEAVE                      = 91314,
    SPELL_FLAME_BLAST_TRAIN          = 91313,

    EVENT_UPPERCUTE = 1,
    EVENT_FLAME_BLAST,

    ITEM_OBSIDIAN_PISTON             = 63333,
    ITEM_FLUX_EXHAUST_SIEVE          = 63336,
    ITEM_THORIUM_GEARSHAFT           = 63335,
    ITEM_STONE_POWER_CORE            = 63334,

    GO_PART_AB                       = 206971,
    GO_PART_AC                       = 206972,
    GO_PART_AD                       = 206973,
    GO_PART_BC                       = 206974,

    QUEST_GOLEM_TRAINING             = 28227,

    ACTION_GOLEM_TRAINING = 0,

    TALK_DEFEAT_1 = 0,
    TALK_OVERRIDE,
    TALK_DEFEAT_2,
    TALK_TRAINING_SUCCESS,
};

const std::list<uint32> reaverSpellParts =
{
    SPELL_CREATE_WAR_REAVER_PARTS_AB,
    SPELL_CREATE_WAR_REAVER_PARTS_AC,
    SPELL_CREATE_WAR_REAVER_PARTS_AD,
    SPELL_CREATE_WAR_REAVER_PARTS_BC,
};

const std::map<uint32, std::array<uint32, 2>> golemParts =
{
    { GO_PART_AB, { ITEM_FLUX_EXHAUST_SIEVE,ITEM_OBSIDIAN_PISTON } },
    { GO_PART_AC, { ITEM_FLUX_EXHAUST_SIEVE, ITEM_THORIUM_GEARSHAFT } },
    { GO_PART_AD, { ITEM_FLUX_EXHAUST_SIEVE, ITEM_STONE_POWER_CORE } },
    { GO_PART_BC, { ITEM_OBSIDIAN_PISTON, ITEM_THORIUM_GEARSHAFT } },
};

// War Reaver 7039
struct npc_burning_steppes_war_reaver : public customCreatureAI
{
    npc_burning_steppes_war_reaver(Creature* creature) : customCreatureAI(creature) { }

    void Reset() override
    {
        events.Reset();
    }

    void EnterCombat(Unit* /*who*/) override
    {
        events.ScheduleEvent(EVENT_UPPERCUTE, 5.5 * IN_MILLISECONDS);
        events.ScheduleEvent(SPELL_FLAME_BLAST, 8 * IN_MILLISECONDS);
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (!roll_chance_i(80))
            return;

        DoCast(me, Trinity::Containers::SelectRandomContainerElement(reaverSpellParts));
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        events.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = events.ExecuteEvent())
        {
            ExecuteTargetEvent(SPELL_UPPERCUTE, 10.5 * IN_MILLISECONDS, EVENT_UPPERCUTE, eventId);
            ExecuteTargetEvent(SPELL_FLAME_BLAST, urand(12 * IN_MILLISECONDS, 15 * IN_MILLISECONDS), EVENT_FLAME_BLAST, eventId, PRIORITY_CHANNELED);
            break;
        }

        DoMeleeAttackIfReady();
    }
};

// War Reaver Parts 206971, 206972, 206973, 206974
class go_burning_steppes_war_reaver_part : public GameObjectScript
{
    public:
        go_burning_steppes_war_reaver_part() : GameObjectScript("go_burning_steppes_war_reaver_part") { }

        bool OnGossipHello(Player* player, GameObject* go) override
        {
            player->ADD_GOSSIP_ITEM_DB(player->GetDefaultGossipMenuForSource(go), 0, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);
            player->ADD_GOSSIP_ITEM_DB(player->GetDefaultGossipMenuForSource(go), 1, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 2);

            player->SEND_GOSSIP_MENU(player->GetGossipTextId(go), go->GetGUID());

            return true;
        }

        bool OnGossipSelect(Player* player, GameObject* go, uint32 /*sender*/, uint32 action) override
        {
            player->PlayerTalkClass->ClearMenus();

            // Send item to player
            auto itr = golemParts.find(go->GetEntry());
            player->AddItem(action == GOSSIP_ACTION_INFO_DEF + 1 ? itr->second[0] : itr->second[1], 1);

            player->CLOSE_GOSSIP_MENU();
            go->Delete();

            return true;
        }
};

// Chiseled Golem 48037
class npc_burning_steppes_chiseled_golem : public CreatureScript
{
public:
    npc_burning_steppes_chiseled_golem() : CreatureScript("npc_burning_steppes_chiseled_golem") {}

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        player->PlayerTalkClass->ClearMenus();

        if (action == GOSSIP_ACTION_INFO_DEF + 1)
        {
            creature->AI()->SetGUID(player->GetGUID());
            creature->AI()->DoAction(ACTION_GOLEM_TRAINING);
        }

        player->CLOSE_GOSSIP_MENU();

        return true;
    }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (player->GetQuestStatus(QUEST_GOLEM_TRAINING) == QUEST_STATUS_INCOMPLETE)
            player->ADD_GOSSIP_ITEM_DB(player->GetDefaultGossipMenuForSource(creature), 0, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);

        player->SEND_GOSSIP_MENU(player->GetGossipTextId(creature), creature->GetGUID());
        return true;
    }

    struct npc_burning_steppes_chiseled_golemAI : public customCreatureAI
    {
        npc_burning_steppes_chiseled_golemAI(Creature* creature) : customCreatureAI(creature), scheduler(), targetGUID(0), hasDefeat(false), defeatCount(0) {}

        TaskScheduler scheduler;
        uint64 targetGUID;
        bool hasDefeat;
        uint8 defeatCount;

        void Reset() override
        {
            events.Reset();
            scheduler.CancelAll();
            targetGUID = 0;
            hasDefeat = false;
            defeatCount = 0;
            me->SetReactState(REACT_AGGRESSIVE);
            me->setFaction(1474); // default faction
        }

        void SetGUID(uint64 guid, int32 /*type*/) override
        {
            targetGUID = guid;
        }

        void DoAction(int32 actionId) override
        {
            if (actionId == ACTION_GOLEM_TRAINING)
            {
                me->setFaction(16); // become friendly to player side for training
                if (Player* target = ObjectAccessor::GetPlayer(*me, targetGUID))
                    AttackStart(target);
            }
        }

        void DamageTaken(Unit* attacker, uint32& damage) override
        {
            if (hasDefeat)
                damage = 0;

            // only process when this would be a killing blow and not already handling a defeat
            if (damage >= me->GetHealth() && !hasDefeat)
            {
                damage = 0;
                hasDefeat = true;
                ++defeatCount; // increment defeat counter

                // First defeat -> Battle Ready
                if (defeatCount == 1)
                {
                    me->PrepareChanneledCast(me->GetOrientation());
                    DoCast(me, SPELL_BATTLE_READY);
                    Talk(TALK_DEFEAT_1);

                    scheduler
                        .Schedule(Seconds(5), [this](TaskContext /*context*/)
                            {
                                me->SetFullHealth();
                                Talk(TALK_OVERRIDE);
                                hasDefeat = false;

                                if (Player* target = ObjectAccessor::GetPlayer(*me, targetGUID))
                                    me->RemoveChanneledCast(target->GetGUID());

                                events.ScheduleEvent(EVENT_FLAME_BLAST, 2 * IN_MILLISECONDS);
                            });
                }
                // Second defeat -> Well Honed
                else if (defeatCount == 2)
                {
                    me->PrepareChanneledCast(me->GetOrientation());
                    DoCast(me, SPELL_WELL_HONED);
                    Talk(TALK_DEFEAT_2);

                    scheduler
                        .Schedule(Seconds(5), [this](TaskContext /*context*/)
                            {
                                me->SetFullHealth();
                                Talk(TALK_OVERRIDE);
                                hasDefeat = false;

                                if (Player* target = ObjectAccessor::GetPlayer(*me, targetGUID))
                                    me->RemoveChanneledCast(target->GetGUID());

                                events.ScheduleEvent(EVENT_UPPERCUTE, 2 * IN_MILLISECONDS);
                            });
                }
                // Third defeat (or more) -> Training success & credit
                else
                {
                    Talk(TALK_TRAINING_SUCCESS);
                    DoCast(me, SPELL_PERFECTLY_TUNNED);
                    me->PrepareChanneledCast(me->GetOrientation());

                    if (Player* target = ObjectAccessor::GetPlayer(*me, targetGUID))
                        target->KilledMonsterCredit(me->GetEntry());

                    me->DespawnOrUnsummon(4 * IN_MILLISECONDS);
                }
            }
        }

        void EnterCombat(Unit* /*who*/) override { /* training uses DoAction; no combat on enter */ }

        void UpdateAI(uint32 diff) override
        {
            scheduler.Update(diff);

            if (!UpdateVictim())
                return;

            events.Update(diff);

            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;

            while (uint32 eventId = events.ExecuteEvent())
            {
                ExecuteTargetEvent(SPELL_HEAVE, 10.5 * IN_MILLISECONDS, EVENT_UPPERCUTE, eventId);
                ExecuteTargetEvent(SPELL_FLAME_BLAST_TRAIN, urand(12 * IN_MILLISECONDS, 15 * IN_MILLISECONDS), EVENT_FLAME_BLAST, eventId, PRIORITY_CHANNELED);
                break;
            }

            DoMeleeAttackIfReady();
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_burning_steppes_chiseled_golemAI(creature);
    }
};


//done nothing wrong.
// Clickable whelplings: grant kill credit and despawn when spell-clicked
enum done_nothing_wrong
{
    QUEST_DONE_NOTHING_WRONG_HORDE_VERSION = 28172,
    QUEST_DONE_NOTHING_WRONG_ALLIANCE_VERSION = 28417,

    WELPLING_1 = 47814,
    WELPLING_2 = 47822,
    WELPLING_3 = 47821,
    WELPLING_4 = 47820
};

class npc_whelplings : public CreatureScript
{
public:
    npc_whelplings() : CreatureScript("npc_whelplings_click") {}

    struct npc_whelplingsAI : public ScriptedAI
    {
        npc_whelplingsAI(Creature* creature) : ScriptedAI(creature) {}

        void Reset() override
        {
            // Make the creature clickable by players and passive by default.
            me->SetFlag(UNIT_FIELD_NPC_FLAGS, UNIT_NPC_FLAG_SPELLCLICK);
            me->SetReactState(REACT_PASSIVE);
        }

        // Called when player clicks the creature (spellclick)
        void OnSpellClick(Unit* clicker, bool& /*result*/) override
        {
            if (!clicker || clicker->GetTypeId() != TYPEID_PLAYER)
                return;

            Player* player = clicker->ToPlayer();
            if (!player)
                return;

            uint32 entry = me->GetEntry();
            bool isTarget = (entry == WELPLING_1 || entry == WELPLING_2 ||
                entry == WELPLING_3 || entry == WELPLING_4);

            if (!isTarget)
                return;

            // Only grant credit if player has either quest incomplete.
            if (player->GetQuestStatus(QUEST_DONE_NOTHING_WRONG_HORDE_VERSION) == QUEST_STATUS_INCOMPLETE ||
                player->GetQuestStatus(QUEST_DONE_NOTHING_WRONG_ALLIANCE_VERSION) == QUEST_STATUS_INCOMPLETE)
            {
                // Prevent further clicks
                me->RemoveFlag(UNIT_FIELD_NPC_FLAGS, UNIT_NPC_FLAG_SPELLCLICK);

                // Give kill credit using entry + GUID to be robust
                player->KilledMonsterCredit(entry, me->GetGUID());

                // Make non-attackable and non-interactable immediately
                me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
                me->SetReactState(REACT_PASSIVE);

                // Allow client to process credit, then despawn.
                me->DespawnOrUnsummon(1000);
            }
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_whelplingsAI(creature);
    }
};

enum TrialByMagma
{
    QUEST_TRIAL_BY_MAGMA = 28266,
    NPC_GENERAL = 48133, // quest giver (not hooked here, quest acceptance is used by Wyrl check)
    NPC_WYRL = 48159,
    NPC_MAGMA_LORD = 48156,
};

// Wyrl: player interacts only if they have quest 28266 incomplete.
// Gossip option will convert the nearby Magma Lord's faction to 14.
class npc_wyrl_interact : public CreatureScript
{
public:
    npc_wyrl_interact() : CreatureScript("npc_wyrl_interact") {}

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (!player || !creature)
            return false;

        // Only offer the interaction if player has the Trial by Magma quest and it is not complete
        if (player->GetQuestStatus(QUEST_TRIAL_BY_MAGMA) == QUEST_STATUS_INCOMPLETE)
        {
            // Add a clear, user-visible gossip option text
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Please release the Magma Lord", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);
        }

        player->SEND_GOSSIP_MENU(player->GetGossipTextId(creature), creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        if (!player || !creature)
            return false;

        player->PlayerTalkClass->ClearMenus();

        if (action == GOSSIP_ACTION_INFO_DEF + 1)
        {
            // Find the nearest Magma Lord instance and switch its faction to the "hostile" value 14.
            if (Creature* magma = GetClosestCreatureWithEntry(creature, NPC_MAGMA_LORD, 60.0f))
            {
                // Set faction to 14 so it becomes hostile/attackable as required
                magma->setFaction(14);
                magma->SetReactState(REACT_AGGRESSIVE);

                // If we have an AI and player is valid, ensure it targets the player
                if (magma->AI())
                {
                    // Start aggression against the player who triggered the change
                    if (player->IsAlive())
                        magma->AI()->AttackStart(player);
                    // Also force zone-in-combat for the magma so nearby mobs behave consistently
                    magma->AI()->DoZoneInCombat();
                }

         
            }
        }

        player->CLOSE_GOSSIP_MENU();
        return true;
    }
};

// Magma Lord: ensure default faction resets to 35 on respawn/reset.
class npc_magma_lord : public CreatureScript
{
public:
    npc_magma_lord() : CreatureScript("npc_magma_lord") {}

    struct npc_magma_lordAI : public ScriptedAI
    {
        npc_magma_lordAI(Creature* creature) : ScriptedAI(creature) {}

        void Reset() override
        {
            // Default faction as requested (35)
            me->setFaction(35);
            // default react state can be passive until triggered
            me->SetReactState(REACT_PASSIVE);
            // Clear any combat state / aggro
            me->CombatStop(true);
            me->AttackStop();
        }

        // keep default behavior for combat / AI if present, do not override other handlers
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_magma_lordAI(creature);
    }
};

constexpr uint32 QUEST_A_DEAL_WITH_A_DRAGON = 28316;
constexpr uint32 NPC_QUEST_GIVER = 48292; // General (only teleport when rewarded by this NPC)
constexpr uint32 TARGET_MAP_ID = 0;    // e.g. 0 for Azeroth
constexpr float  TARGET_X = -8391.02f; // set X
constexpr float  TARGET_Y = -2778.08f; // set Y
constexpr float  TARGET_Z = 194.68f; // set Z
constexpr float  TARGET_O = 4.420233f; // set orientation

class npc_deal_with_dragon_reward : public CreatureScript
{
public:
    npc_deal_with_dragon_reward() : CreatureScript("npc_deal_with_dragon_reward") {}

    // Called when a quest is rewarded at this creature.
    bool OnQuestReward(Player* player, Creature* creature, Quest const* quest, uint32 /*opt*/) override
    {
        if (!player || !creature || !quest)
            return false;

        // Only act for the requested quest and when rewarded by the configured NPC
        if (quest->GetQuestId() != QUEST_A_DEAL_WITH_A_DRAGON)
            return false;

        if (creature->GetEntry() != NPC_QUEST_GIVER)
            return false;

        // Teleport the player to the configured location after reward
        player->TeleportTo(TARGET_MAP_ID, TARGET_X, TARGET_Y, TARGET_Z, TARGET_O);

        return true; // handled
    }
};

constexpr uint32 DISMOUNT_MAP_ID = 0;
constexpr float  DISMOUNT_X = -8391.02f;
constexpr float  DISMOUNT_Y = -2778.08f;
constexpr float  DISMOUNT_Z = 194.68f;
constexpr float  DISMOUNT_O = 4.420233f;
constexpr uint32 NPC_OBSIDIAN_CLOAKED_DRAGON = 48434;
constexpr float  OBSIDIAN_DRAGON_FLIGHT_MULTIPLIER = 4.0f; // flight speed multiplier while dragon is active

class npc_obsidian_cloaked_dragon : public CreatureScript
{
public:
    npc_obsidian_cloaked_dragon() : CreatureScript("npc_obsidian_cloaked_dragon") {}

    struct npc_obsidian_cloaked_dragonAI : public ScriptedAI
    {
        npc_obsidian_cloaked_dragonAI(Creature* creature) : ScriptedAI(creature), _currentRiderGUID(0), _summonerGUID(0) {}

        uint64 _currentRiderGUID; // GUID of the current rider (charmer) if any
        uint64 _summonerGUID;     // GUID of the player who summoned/spawned the dragon (used to restore phase)

        void Reset() override
        {
            me->SetReactState(REACT_PASSIVE);
            me->SetCanFly(true);
            me->SetFlying(true);
            me->OverrideInhabitType(INHABIT_AIR);
            me->SetSpeed(MOVE_FLIGHT, OBSIDIAN_DRAGON_FLIGHT_MULTIPLIER);
            me->UpdateMovementFlags();

            _currentRiderGUID = 0;
            _summonerGUID = 0;
        }

        void IsSummonedBy(Unit* summoner) override
        {
            // Ensure dragon is in proper flying state
            Reset();

            // small position nudge so client sees correct movement flags
            float x, y, z, o;
            me->GetPosition(x, y, z, o);
            me->NearTeleportTo(x, y, z + 0.5f, o);

            // If summoned by a player, remember them and set their phase to 2
            _summonerGUID = 0;
            if (summoner)
            {
                if (Player* player = summoner->ToPlayer())
                {
                    player->SetPhaseMask(2, true);
                    _summonerGUID = player->GetGUID();
                }
            }
        }

        void JustDied(Unit* killer) override
        {
            // Clear rider/summoner state and restore player(s) to normal
            ClearRiderState(false);

            // preserve default death handling
            ScriptedAI::JustDied(killer);
        }

        void EnterEvadeMode() override
        {
            ClearRiderState(false);
            ScriptedAI::EnterEvadeMode();
        }

        void UpdateAI(uint32 /*diff*/) override
        {
            Unit* charmer = me->GetCharmer();
            uint64 charmerGUID = charmer ? charmer->GetGUID() : 0;

            // New rider attached
            if (charmerGUID && charmerGUID != _currentRiderGUID)
            {
                if (_currentRiderGUID)
                    ClearRiderState(false);

                me->SetCanFly(true);
                me->SetFlying(true);
                me->OverrideInhabitType(INHABIT_AIR);
                me->SetSpeed(MOVE_FLIGHT, OBSIDIAN_DRAGON_FLIGHT_MULTIPLIER);
                me->UpdateMovementFlags();

                if (Player* p = charmer->ToPlayer())
                {
                    p->SetCanFly(true);
                    p->SetDisableGravity(true, true);
                    p->AddUnitMovementFlag(MOVEMENTFLAG_DISABLE_GRAVITY);
                    p->SendMovementFlagUpdate(true);

                    p->SendMovementSetCanTransitionBetweenSwimAndFly(true);

                    p->SetSpeed(MOVE_FLIGHT, OBSIDIAN_DRAGON_FLIGHT_MULTIPLIER);
                    p->SetSpeed(MOVE_RUN, 1.2f);

                    p->SendMovementSetCollisionHeight(2.0f);

                    // Ensure we remember the rider as current rider
                    _currentRiderGUID = p->GetGUID();
                }
                else
                {
                    // not a player (unlikely) — clear rider guid
                    _currentRiderGUID = 0;
                }
            }
            // rider removed
            else if (!charmerGUID && _currentRiderGUID)
            {
                ClearRiderState(true);
            }

            // no other AI actions — dragon is passive/transport-only in this script
        }

    private:
        void ClearRiderState(bool /*notifyDismount*/)
        {
            // Nothing to do
            if (!_currentRiderGUID && !_summonerGUID)
                return;

            // Restore the player who was riding (if present)
            if (_currentRiderGUID)
            {
                if (Player* prev = ObjectAccessor::FindPlayer(_currentRiderGUID))
                {
                    prev->SetCanFly(false);
                    prev->SetDisableGravity(false, true);
                    prev->RemoveUnitMovementFlag(MOVEMENTFLAG_DISABLE_GRAVITY);
                    prev->SendMovementFlagUpdate(true);

                    prev->SendMovementSetCanTransitionBetweenSwimAndFly(false);

                    prev->SetSpeed(MOVE_FLIGHT, 1.0f);
                    prev->SetSpeed(MOVE_RUN, 1.0f);

                    prev->SendMovementSetCollisionHeight(1.0f);

                    // restore player's phase
                    prev->SetPhaseMask(1, true);
                }

                _currentRiderGUID = 0;
            }

            // If a summoner was remembered (player who spawned the dragon but might not be the rider),
            // ensure their phase is restored as well.
            if (_summonerGUID)
            {
                if (Player* summoner = ObjectAccessor::FindPlayer(_summonerGUID))
                {
                    summoner->SetPhaseMask(1, true);
                }
                _summonerGUID = 0;
            }

            // ensure dragon remains in flying visual state for world (no change to player)
            me->SetCanFly(true);
            me->SetFlying(true);
            me->OverrideInhabitType(INHABIT_AIR);
            me->SetSpeed(MOVE_FLIGHT, OBSIDIAN_DRAGON_FLIGHT_MULTIPLIER);
            me->UpdateMovementFlags();
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_obsidian_cloaked_dragonAI(creature);
    }
};

// Replace with the actual aura/spell ID applied to the player while mounted/dragons
constexpr uint32 SPELL_OBSIDIAN_DRAGON_AURA = 52196; // aura on the dragon

class spell_obsidian_dragon_aura : public SpellScriptLoader
{
public:
    spell_obsidian_dragon_aura() : SpellScriptLoader("spell_obsidian_dragon_aura") {}

    class spell_obsidian_dragon_aura_AuraScript : public AuraScript
    {
        PrepareAuraScript(spell_obsidian_dragon_aura_AuraScript);

        static void DoTeleportPlayerSafe(Player* p)
        {
            if (!p)
                return;

            // restore phase first so client sees correct phase immediately
            p->SetPhaseMask(1, true);

            const uint32 targetMap = DISMOUNT_MAP_ID;
            const float tx = DISMOUNT_X;
            const float ty = DISMOUNT_Y;
            const float tz = DISMOUNT_Z;
            const float to = DISMOUNT_O;

            // If player already on target map, use NearTeleportTo for immediate relocation.
            // Otherwise fall back to TeleportTo which handles map changes.
            if (p->GetMapId() == targetMap)
                p->NearTeleportTo(tx, ty, tz, to);
            else
                p->TeleportTo(targetMap, tx, ty, tz, to);
        }

        // Called when an effect of the aura is removed (covers most removal paths)
        void HandleAuraEffectRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
        {
            if (Unit* target = GetTarget())
            {
                // If the aura was on the dragon, restore and teleport the associated player
                if (Creature* cr = target->ToCreature())
                {
                    // 1) Prefer the current charmer (rider) if present
                    if (Unit* charmer = cr->GetCharmer())
                    {
                        if (Player* p = charmer->ToPlayer())
                        {
                            DoTeleportPlayerSafe(p);
                            return;
                        }
                    }

                    // 2) Fallback: read stored summoner GUID from the dragon AI (if present)
                    if (cr->GetEntry() == NPC_OBSIDIAN_CLOAKED_DRAGON)
                    {
                        if (UnitAI* uai = cr->AI())
                        {
                            if (auto* myAI = dynamic_cast<npc_obsidian_cloaked_dragon::npc_obsidian_cloaked_dragonAI*>(uai))
                            {
                                if (myAI->_summonerGUID)
                                {
                                    if (Player* summoner = ObjectAccessor::FindPlayer(myAI->_summonerGUID))
                                        DoTeleportPlayerSafe(summoner);

                                    myAI->_summonerGUID = 0; // clear after handling
                                    return;
                                }
                            }
                        }
                    }
                }
                // If the aura was (unexpectedly) on a player directly, restore + teleport them
                else if (Player* p = target->ToPlayer())
                {
                    DoTeleportPlayerSafe(p);
                    return;
                }
            }
        }

        void Register() override
        {
            // Register for likely aura types — adjust/add the exact aura/effect if you know it.
            AfterEffectRemove += AuraEffectRemoveFn(spell_obsidian_dragon_aura_AuraScript::HandleAuraEffectRemove, EFFECT_0, SPELL_AURA_CONTROL_VEHICLE, AURA_EFFECT_HANDLE_REAL);
            AfterEffectRemove += AuraEffectRemoveFn(spell_obsidian_dragon_aura_AuraScript::HandleAuraEffectRemove, EFFECT_0, SPELL_AURA_MOUNTED, AURA_EFFECT_HANDLE_REAL);
            AfterEffectRemove += AuraEffectRemoveFn(spell_obsidian_dragon_aura_AuraScript::HandleAuraEffectRemove, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY, AURA_EFFECT_HANDLE_REAL);
            // Generic hook (catch-all)
            OnEffectRemove += AuraEffectRemoveFn(spell_obsidian_dragon_aura_AuraScript::HandleAuraEffectRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        }
    };

    AuraScript* GetAuraScript() const override
    {
        return new spell_obsidian_dragon_aura_AuraScript();
    }
};

class npc_colonel_autocomplete : public CreatureScript
{
public:
    npc_colonel_autocomplete() : CreatureScript("npc_colonel_autocomplete") {}

    bool OnQuestAccept(Player* player, Creature* creature, Quest const* quest) override
    {
        if (!player || !creature || !quest)
            return false;

        // Only for the specific quest and the Colonel NPC
        if (quest->GetQuestId() != 28321 || creature->GetEntry() != 48307)
            return false;

        // Only act if player actually has the quest and it's incomplete
        if (player->GetQuestStatus(28321) != QUEST_STATUS_INCOMPLETE)
            return false;

        // Complete the quest immediately on accept
        player->CompleteQuest(28321);

        return true;
    }
};

void AddSC_burning_steppes()
{
    new creature_script<npc_burning_steppes_war_reaver>("npc_burning_steppes_war_reaver");
    new go_burning_steppes_war_reaver_part();
    new npc_wyrl_interact();
    new npc_magma_lord();
    new npc_burning_steppes_chiseled_golem();
	new npc_whelplings();
	new npc_deal_with_dragon_reward();
    new npc_obsidian_cloaked_dragon();
	new spell_obsidian_dragon_aura();
	new npc_colonel_autocomplete();
	

}
