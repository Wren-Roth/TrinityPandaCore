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
    NPC_RHEA = 46654, // quest giver Rhea
    NPC_NYXONDRA = 46658, // dragon
    NPC_OLAF = 46857,
    NPC_BAELOG = 46856,

    QUEST_RHEA_REVEALED = 27769, // Rhea Revealed
	QUEST_DEVASTATION = 27930, // Devastation
    QUEST_LIFTING_THE_VEIL = 27770, // quest id
    QUEST_RHEASTRAZAS_GIFT = 27858,
    QUEST_SENTINELS_GAME = 27709,
    QUEST_WARDENS_GAME = 27693,

    SPELL_DRAGON_SLEEP = 148789, // spell that keeps dragon sealed / non-interactable
    SPELL_DRAGON_BREATH = 8873,   // dragon breath to cast every 10s while awake
    SPELL_ERIK_CHARGE = 87278,
    SPELL_BAELOG_WARCRY = 87277,
    SPELL_OLAF_SHIELD = 87275,
    SPELL_ERIK_HEAL = 87279,

    GO_MARBLE_SLAB = 206336,
    GO_STONE_SLAB = 206335
  
};

class npc_nyxondra : public CreatureScript
{
public:
    npc_nyxondra() : CreatureScript("npc_nyxondra") {}

    struct npc_nyxondraAI : public ScriptedAI
    {
        npc_nyxondraAI(Creature* creature) : ScriptedAI(creature),
            m_awake(false), m_awakeTimer(0), m_breathTimer(0) {
        }

        bool m_awake;
        uint32 m_awakeTimer; // ms
        uint32 m_breathTimer; // ms

        void Reset() override
        {
            // Ensure the spell is present on spawn/reset to keep the dragon sealed.
            if (!me->HasAura(SPELL_DRAGON_SLEEP))
                me->CastSpell(me, SPELL_DRAGON_SLEEP, true);

            // Make non-interactable while sealed
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_ATTACKABLE_1);

            // Keep passive until awakened
            me->SetReactState(REACT_PASSIVE);
            me->CombatStop();
            me->AttackStop();
            me->DeleteThreatList();

            // reset awake state and timers
            m_awake = false;
            m_awakeTimer = 0;
            m_breathTimer = 0;
        }

        void EnterCombat(Unit* /*who*/) override
        {
            // When real combat begins, schedule first breath in 1s
            m_awake = true;
            m_breathTimer = 1000; // 1 second until first breath
        }

        void JustDied(Unit* /*killer*/) override {}

        // Called from external script (npc_rhea) to wake the dragon for `durationMs`.
        void SetData(uint32 type, uint32 data) override
        {
            // type 1 == wake with duration (ms)
            if (type != 1)
                return;

            uint32 duration = data;
            if (duration == 0)
                return;

            // If already awake, refresh timers (breath will trigger 1s after combat start via EnterCombat)
            if (m_awake)
            {
                m_awakeTimer = duration;
                // when already awake out of combat, keep breath cooldown at 5s (will only run in combat)
                m_breathTimer = 5000;
                return;
            }

            // Remove sealing aura
            me->RemoveAurasDueToSpell(SPELL_DRAGON_SLEEP);

            // Make selectable / attackable while awake
            me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
            me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_ATTACKABLE_1);

            // Allow normal reactions but don't force an immediate attack
            me->InitializeReactState();
            me->SetReactState(REACT_DEFENSIVE);

            // Let the dragon move a short distance forward as a "stir/wake" animation
            float distance = 6.0f;
            float nx = me->GetPositionX() + distance * cos(me->GetOrientation());
            float ny = me->GetPositionY() + distance * sin(me->GetOrientation());
            float nz = me->GetPositionZ();

            me->GetMotionMaster()->MovePoint(0, nx, ny, nz);

            // start awake timer; breath timer will be set when combat actually starts (EnterCombat)
            m_awake = true;
            m_awakeTimer = duration;
            m_breathTimer = 5000; // default repeat cooldown (used after first 1s cast)
        }

        void UpdateAI(uint32 diff) override
        {
            // If awake, manage timers (breath + reseal)
            if (m_awake)
            {
                // Breath casting logic: only cast if we have a victim (i.e. engaged)
                if (Unit* victim = me->GetVictim())
                {
                    if (m_breathTimer <= diff)
                    {
                        // Prevent casting while already casting other spells
                        if (!me->HasUnitState(UNIT_STATE_CASTING))
                        {
                            DoCastVictim(SPELL_DRAGON_BREATH);
                        }
                        // after first cast (which was scheduled by EnterCombat at 1s),
                        // subsequent repeats use 5s interval
                        m_breathTimer = 5000; // reset to 5 seconds
                    }
                    else
                        m_breathTimer -= diff;
                }

                // Reseal timer
                if (m_awakeTimer <= diff)
                {
                    // time to reseal the dragon
                    m_awake = false;
                    m_awakeTimer = 0;
                    m_breathTimer = 0;

                    // Stop movement and reset to idle
                    me->GetMotionMaster()->Clear();
                    me->GetMotionMaster()->MoveIdle();

                    // Reapply sealing spell
                    if (!me->HasAura(SPELL_DRAGON_SLEEP))
                        me->CastSpell(me, SPELL_DRAGON_SLEEP, true);

                    // Make non-interactable again
                    me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                    me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_ATTACKABLE_1);

                    // Return to passive state
                    me->SetReactState(REACT_PASSIVE);
                    me->CombatStop();
                    me->AttackStop();
                    me->DeleteThreatList();

                    return;
                }
                else
                    m_awakeTimer -= diff;
            }

            // Combat AI: if we have a victim, let the engine handle melee etc.
            if (!UpdateVictim())
                return;

            // Prevent interrupting casts
            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;

            DoMeleeAttackIfReady();
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_nyxondraAI(creature);
    }
};

/* npc_rhea: when player accepts QUEST_LIFTING_THE_VEIL wake the dragon (remove spell)
   and instruct it to move for a short time; after that the dragon will reseal itself. */
   // Add gossip option "Who are you?" for quest 27769 (Rhea Revealed).
   // When chosen, Rhea despawns and Rheastraza (46655) is summoned at her location.
class npc_rhea : public CreatureScript
{
public:
    npc_rhea() : CreatureScript("npc_rhea_dragon_quest") {}

    struct npc_rheaAI : public ScriptedAI
    {
        npc_rheaAI(Creature* creature) : ScriptedAI(creature), m_transformPending(false), m_transformTimer(0), m_transformSpawnEntry(0) {}

        bool m_transformPending;
        uint32 m_transformTimer;      // ms until transform
        uint32 m_transformSpawnEntry; // NPC entry to spawn

        void Reset() override
        {
            m_transformPending = false;
            m_transformTimer = 0;
            m_transformSpawnEntry = 0;
        }

        void EnterCombat(Unit* /*who*/) override {}
        void JustDied(Unit* /*killer*/) override {}

        // External trigger to schedule the transform (type == 2, data == spawnEntry)
        void SetData(uint32 type, uint32 data) override
        {
            if (type != 2)
                return;

            m_transformSpawnEntry = data;
            m_transformTimer = 1000; // 1 second until spawn/despawn
            m_transformPending = true;
        }

        void UpdateAI(uint32 diff) override
        {
            // Handle scheduled transform (vanish already applied by caller)
            if (m_transformPending)
            {
                if (m_transformTimer <= diff)
                {
                    m_transformPending = false;
                    m_transformTimer = 0;

                    // spawn Rheastraza at Rhea's position/orientation
                    Creature* spawned = me->SummonCreature(
                        m_transformSpawnEntry,
                        me->GetPositionX(),
                        me->GetPositionY(),
                        me->GetPositionZ(),
                        me->GetOrientation(),
                        TEMPSUMMON_MANUAL_DESPAWN);

                    // despawn Rhea
                    me->DespawnOrUnsummon();

                    // ensure spawned is interactive
                    if (spawned)
                    {
                        spawned->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                        spawned->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_ATTACKABLE_1);
                        spawned->InitializeReactState();
                    }

                    return;
                }
                else
                    m_transformTimer -= diff;
            }

            // default behavior; Rhea has no combat AI here
            if (!UpdateVictim())
                return;
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_rheaAI(creature);
    }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (creature->IsQuestGiver())
            player->PrepareQuestMenu(creature->GetGUID());

        // Offer gossip only when player is on quest QUEST_RHEA_REVEALED and it's incomplete
        if (player->GetQuestStatus(QUEST_RHEA_REVEALED) == QUEST_STATUS_INCOMPLETE)
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Who are you?", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);

        player->SEND_GOSSIP_MENU(player->GetGossipTextId(creature), creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        player->PlayerTalkClass->ClearMenus();

        if (action == GOSSIP_ACTION_INFO_DEF + 1)
        {
            // Auto-complete the quest for the player (if still incomplete)
            if (player->GetQuestStatus(QUEST_RHEA_REVEALED) == QUEST_STATUS_INCOMPLETE)
                player->CompleteQuest(QUEST_RHEA_REVEALED);

            // Cast vanish on Rhea immediately (visual), then schedule transform in 1s.
            constexpr uint32 SPELL_RHEA_VANISH = 127842;
            constexpr uint32 RHEASTRAZA_ENTRY = 46655;
            constexpr uint32 TRANSFORM_ACTION = 2;

            creature->CastSpell(creature, SPELL_RHEA_VANISH, true);

            // schedule the transform via AI (will spawn Rheastraza and despawn Rhea after 1s)
            if (CreatureAI* cai = creature->AI())
                cai->SetData(TRANSFORM_ACTION, RHEASTRAZA_ENTRY);

            player->CLOSE_GOSSIP_MENU();
        }
        else
        {
            player->CLOSE_GOSSIP_MENU();
        }

        return true;
    }
    bool OnQuestAccept(Player* player, Creature* creature, Quest const* quest) override
    {
        if (!quest)
            return false;

        const uint32 qid = quest->GetQuestId();

     
        if (qid == QUEST_RHEASTRAZAS_GIFT)
        {
            if (player->GetQuestStatus(QUEST_RHEASTRAZAS_GIFT) == QUEST_STATUS_INCOMPLETE)
                player->CompleteQuest(QUEST_RHEASTRAZAS_GIFT);

            return true;
        }

        // New: Devastation (27930) — teleport player on accept.
        if (qid == QUEST_DEVASTATION)
        {
            
            // mapId, x, y, z, orientation
            constexpr uint32 DEST_MAP_ID = 0;    // <MAP_ID>
            constexpr float  DEST_X = -6478.72f; // <X>
            constexpr float  DEST_Y = -2451.92f; // <Y>
            constexpr float  DEST_Z = 306.65f; // <Z>
            constexpr float  DEST_O = 5.133069f; // <Orientation>

          
            WorldLocation dest{ DEST_MAP_ID, DEST_X, DEST_Y, DEST_Z, DEST_O };
            player->TeleportTo(dest);

              

            return true;
        }

        return false;
    }
};

class npc_rheastraza : public CreatureScript
{
public:
    npc_rheastraza() : CreatureScript("npc_rheastraza") {}

    // Auto-despawn 5s after the player accepts quest 27772 on this NPC
    bool OnQuestAccept(Player* /*player*/, Creature* creature, Quest const* quest) override
    {
        if (!quest)
            return false;

        if (quest->GetQuestId() != 27772)
            return false;

        // Despawn the NPC after 5000 ms
        creature->DespawnOrUnsummon(5000);
        return true;
    }
};

enum deathwingstorydefs
{
    QUEST_THE_DAY_THAT_DEATHWING_CAME_1 = 27713,
	QUEST_THE_DAY_THAT_DEATHWING_CAME_2 = 27714,
	QUEST_THE_DAY_THAT_DEATHWING_CAME_3 = 27715
};

class deathwingstory : public CreatureScript
{
public:
    deathwingstory() : CreatureScript("npc_deathwingstory") {}

    bool OnQuestAccept(Player* player, Creature* creature, Quest const* quest) override
    {
        if (!player || !quest)
            return false;

        uint32 qid = quest->GetQuestId();
        switch (qid)
        {
        case 27713: // QUEST_THE_DAY_THAT_DEATHWING_CAME_1
        case 27714: // QUEST_THE_DAY_THAT_DEATHWING_CAME_2
        case 27715: // QUEST_THE_DAY_THAT_DEATHWING_CAME_3
            if (player->GetQuestStatus(qid) == QUEST_STATUS_INCOMPLETE)
                player->CompleteQuest(qid, creature);
            return true;
        default:
            return false;
        }
    }
};
struct npc_zone_helperAI : public ScriptedAI
{
    explicit npc_zone_helperAI(Creature* creature) : ScriptedAI(creature), m_ownerGUID(0), m_followTargetGUID(0)
    {
        me->SetReactState(REACT_PASSIVE);
        // try to cache owner GUID if this creature was summoned/created with an owner
        m_ownerGUID = me->GetOwnerGUID();
    }

    static constexpr float FOLLOW_DISTANCE = 2.0f; // distance to keep when following

protected:
    uint64 m_ownerGUID;       // GUID of the configured owner (guardian owner)
    uint64 m_followTargetGUID; // GUID of player to follow (set when assisting/resuming)

    Player* GetOwnerPlayer()
    {
        if (!m_ownerGUID)
            return nullptr;
        return ObjectAccessor::GetPlayer(*me, m_ownerGUID);
    }

    void StartFollowing(Player* player)
    {
        if (!player)
            return;
        m_followTargetGUID = player->GetGUID();
        me->GetMotionMaster()->MoveFollow(player, FOLLOW_DISTANCE, 0.0f);
    }

public:
    void Reset() override
    {
        me->SetReactState(REACT_PASSIVE);
        me->CombatStop();
        me->AttackStop();
        me->DeleteThreatList();

        // refresh owner GUID (in case it changed)
        if (!m_ownerGUID)
            m_ownerGUID = me->GetOwnerGUID();

        m_followTargetGUID = m_ownerGUID;

        // Start following owner if present
        if (Player* owner = GetOwnerPlayer())
            StartFollowing(owner);
        else
            me->GetMotionMaster()->MoveIdle();
    }

    void EnterCombat(Unit* /*who*/) override
    {
        // stop following when combat starts
        me->GetMotionMaster()->Clear();
    }

    void UpdateAI(uint32 diff) override
    {
        // If currently fighting, run normal combat AI
        if (me->GetVictim())
        {
            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;

            DoMeleeAttackIfReady();
            return;
        }

        // If there is an owner configured, only assist that owner
        if (m_ownerGUID)
        {
            Player* owner = GetOwnerPlayer();
            if (owner && owner->IsInWorld())
            {
                // if owner is in combat, assist their target
                if (owner->IsInCombat())
                {
                    Unit* target = nullptr;
                    if (Unit* sel = owner->GetSelectedUnit())
                        target = sel;
                    if (!target)
                        target = owner->GetVictim();

                    if (target && target->IsHostileTo(owner))
                    {
                        // Stop following and assist
                        me->GetMotionMaster()->Clear();
                        me->InitializeReactState();
                        me->SetReactState(REACT_AGGRESSIVE);

                        // remember owner to resume following later
                        m_followTargetGUID = m_ownerGUID;

                        AttackStart(target);
                        return;
                    }
                }

                // Owner not in combat: ensure we follow the owner
                if (!me->IsWithinDistInMap(owner, FOLLOW_DISTANCE))
                    me->GetMotionMaster()->MoveFollow(owner, FOLLOW_DISTANCE, 0.0f);

                return;
            }
            else
            {
                // no valid owner found, clear cached owner GUID and fallback to idle
                m_ownerGUID = 0;
                m_followTargetGUID = 0;
                me->GetMotionMaster()->MoveIdle();
                return;
            }
        }

        // No owner configured: do nothing (helpers without owner won't auto-assist)
        if (!me->GetVictim())
            me->GetMotionMaster()->MoveIdle();
    }
};

// Replace the old simple npc_olaf wrapper with this specialized AI that handles a 30s shield cooldown.
class npc_olaf : public CreatureScript
{
public:
    npc_olaf() : CreatureScript("npc_olaf") {}

    struct npc_olafAI : public npc_zone_helperAI
    {
        npc_olafAI(Creature* creature) : npc_zone_helperAI(creature), m_shieldTimer(30000) {}

        uint32 m_shieldTimer; // ms until next shield cast

        void Reset() override
        {
            npc_zone_helperAI::Reset();
            m_shieldTimer = 30000; // start with full cooldown; set to 0 to allow immediate first cast
        }

        void UpdateAI(uint32 diff) override
        {
            // run base assist/follow/attack logic first
            npc_zone_helperAI::UpdateAI(diff);

            // don't attempt to cast while we're casting
            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;

            // determine if we should consider casting: in combat ourselves or owner in combat
            bool shouldCast = false;
            if (me->GetVictim())
                shouldCast = true;
            else if (Player* owner = GetOwnerPlayer())
                shouldCast = owner->IsInCombat();

            // tick local timer while idle so it's available when combat starts
            if (!shouldCast)
            {
                if (m_shieldTimer > diff)
                    m_shieldTimer -= diff;
                else
                    m_shieldTimer = 0;
                return;
            }

            // if ready and no server-side cooldown, cast shield
            if (m_shieldTimer == 0 && !me->HasSpellCooldown(SPELL_OLAF_SHIELD))
            {
                DoCast(me, SPELL_OLAF_SHIELD);
                me->AddCreatureSpellCooldown(SPELL_OLAF_SHIELD); // server cooldown
                m_shieldTimer = 30000; // local cooldown 30s
            }
            else
            {
                if (m_shieldTimer > diff)
                    m_shieldTimer -= diff;
                else
                    m_shieldTimer = 0;
            }
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_olafAI(creature);
    }
};

// Baelog script (unique script name)
class npc_baelog : public CreatureScript
{
public:
    npc_baelog() : CreatureScript("npc_baelog") {}

    struct npc_baelogAI : public npc_zone_helperAI
    {
        npc_baelogAI(Creature* creature) : npc_zone_helperAI(creature), m_warcryTimer(60000) {}

        uint32 m_warcryTimer; // ms until next warcry

        void Reset() override
        {
            npc_zone_helperAI::Reset();
            m_warcryTimer = 60000; // start with full cooldown so first cast happens after 60s or you can set to 0 to cast immediately
        }

        void UpdateAI(uint32 diff) override
        {
            // Let base AI handle assist/follow/attack logic first
            npc_zone_helperAI::UpdateAI(diff);

            // Don't attempt to cast while casting
            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;

            // Only consider casting warcry when engaged (self in combat) or owner is in combat
            bool shouldCast = false;
            if (me->GetVictim())
                shouldCast = true;
            else if (Player* owner = GetOwnerPlayer())
                shouldCast = owner->IsInCombat();

            if (!shouldCast)
            {
                // still tick down cooldown while idle so warcry will be available when combat starts
                if (m_warcryTimer > diff)
                    m_warcryTimer -= diff;
                else
                    m_warcryTimer = 0;
                return;
            }

            // Warcry available?
            if (m_warcryTimer == 0 && !me->HasSpellCooldown(SPELL_BAELOG_WARCRY))
            {
                // Cast warcry (buff) on self / group as defined by the spell
                DoCast(me, SPELL_BAELOG_WARCRY);

                // start server-side creature cooldown (uses default handling)
                me->AddCreatureSpellCooldown(SPELL_BAELOG_WARCRY);

                // reset local timer to 60s
                m_warcryTimer = 60000;
            }
            else
            {
                if (m_warcryTimer > diff)
                    m_warcryTimer -= diff;
                else
                    m_warcryTimer = 0;
            }
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_baelogAI(creature);
    }
};

class npc_erik : public CreatureScript
{
public:
    npc_erik() : CreatureScript("npc_erik") {}

    struct npc_erikAI : public npc_zone_helperAI
    {
        npc_erikAI(Creature* creature) : npc_zone_helperAI(creature),
            m_healCheckTimer(2000)
        {
        }

        uint32 m_healCheckTimer;  // ms between ally-heal scans

        void Reset() override
        {
            npc_zone_helperAI::Reset();
            m_healCheckTimer = 2000;
            // ensure charge cooldown is cleared on reset so it's available when engage begins
            // (Creature::RemoveSpellCooldown not exposed here; using creature cooldown storage API isn't required)
        }

        void UpdateAI(uint32 diff) override
        {
            // Let base AI handle assist/follow/attack decisions first.
            npc_zone_helperAI::UpdateAI(diff);

            // If we're currently casting, don't start other casts.
            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;

            // CHARGE: If we have a victim and charge is off cooldown, cast charge.
            if (Unit* victim = me->GetVictim())
            {
                if (!me->HasSpellCooldown(SPELL_ERIK_CHARGE))
                {
                    // Cast charge on current victim
                    DoCastVictim(SPELL_ERIK_CHARGE);
                    // Start creature spell cooldown for charge (15s)
                    me->AddCreatureSpellCooldown(SPELL_ERIK_CHARGE);
                }
            }

            // HEAL: check allies periodically
            if (m_healCheckTimer <= diff)
            {
                m_healCheckTimer = 2000; // reset interval

                // scan for nearby friendly helpers (owner + Olaf + Baelog + self optional)
                float const HEAL_SEARCH_RADIUS = 30.0f;
                uint32 const allyEntries[] = { NPC_OLAF, NPC_BAELOG };

                for (uint32 entry : allyEntries)
                {
                    // find nearest ally of that entry
                    if (Creature* ally = me->FindNearestCreature(entry, HEAL_SEARCH_RADIUS, true))
                    {
                        if (ally->IsAlive() && ally->GetHealthPct() <= 80.0f)
                        {
                            // Cast heal on the ally (do not spam if ally already has heal aura)
                            if (!ally->HasAura(SPELL_ERIK_HEAL))
                            {
                                DoCast(ally, SPELL_ERIK_HEAL);
                                // After casting a heal, break to avoid multiple heals at once
                                break;
                            }
                        }
                    }
                }
            }
            else
                m_healCheckTimer -= diff;
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_erikAI(creature);
    }
};

class go_marble_slab : public GameObjectScript
{
public:
    go_marble_slab() : GameObjectScript("go_marble_slab") {}

    bool OnQuestAccept(Player* player, GameObject* go, Quest const* quest) override
    {
        if (!player || !quest)
            return false;

        if (quest->GetQuestId() != QUEST_SENTINELS_GAME)
            return false;

        if (player->GetQuestStatus(QUEST_SENTINELS_GAME) == QUEST_STATUS_INCOMPLETE)
            player->CompleteQuest(QUEST_SENTINELS_GAME);

        return true;
    }
};

class go_stone_slab : public GameObjectScript
{
public:
    go_stone_slab() : GameObjectScript("go_stone_slab") {}

    bool OnQuestAccept(Player* player, GameObject* /*go*/, Quest const* quest) override
    {
        if (!player || !quest)
            return false;

        if (quest->GetQuestId() != QUEST_WARDENS_GAME)
            return false;

        if (player->GetQuestStatus(QUEST_WARDENS_GAME) == QUEST_STATUS_INCOMPLETE)
            player->CompleteQuest(QUEST_WARDENS_GAME);

        return true;
    }
};

// Register the new script together with the existing ones
void AddSC_zone_badlands()
{
    new npc_nyxondra();
    new npc_rhea();
    new npc_rheastraza();
	new deathwingstory();
	new npc_olaf();
	new npc_baelog();
	new npc_erik();
	new go_marble_slab();
	new go_stone_slab();

}
