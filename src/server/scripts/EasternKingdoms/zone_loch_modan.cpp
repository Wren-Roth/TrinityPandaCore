
/*
 * Copyright (C) 2011-2016 Project SkyFire <http://www.projectskyfire.org/>
 * Copyright (C) 2008-2016 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2016 MaNGOS <http://www.getmangos.com/>
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
 SDName: Loch_Modan
 SD%Complete: 0
 SDComment:
 SDCategory: Loch Modan
 EndScriptData */

 /* ContentData
 EndContentData */

#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "Player.h"
#include "TemporarySummon.h"
#include "MovementGenerator.h"
#include "MoveSplineInit.h" // <--- needed to set spline velocity per-leg
#include <cmath>
#include <vector>

 // Fallback visual mount display id (0 = no visual)
    constexpr uint32 FALLBACK_MOUNT_DISPLAY = 14376;

// Movement speed used to compute travel time (world units per second). Tweak as needed.
constexpr float PLAYER_TRAVEL_SPEED = 25.0f; // units/sec

enum LochModanData
{
    NPC_SKYSTRIDER = 44572,
    NPC_ANDO = 44870,
    QUEST_THE_WINDS = 27116
};

enum GossipItems
{
    GOSSIP_TEXT_MOUNT = 1
};

enum Actions
{
    ACTION_START_RIDE = 1
};

enum Spells
{
    SPELL_EXAUST_FLAMES = 83984
};

class npc_ando_loch_modan : public CreatureScript
{
public:
    npc_ando_loch_modan() : CreatureScript("npc_ando_loch_modan") {}

    bool OnQuestAccept(Player* player, Creature* /*creature*/, Quest const* quest) override
    {
        if (!player || !quest)
            return false;

        if (quest->GetQuestId() != QUEST_THE_WINDS)
            return false;

        Position spawnPos;
        player->GetPosition(&spawnPos);
        spawnPos.m_positionX += 1.5f;

        Creature* mount = player->SummonCreature(NPC_SKYSTRIDER, spawnPos, TEMPSUMMON_TIMED_DESPAWN, 15 * MINUTE * IN_MILLISECONDS);
        if (mount && mount->AI())
        {
            mount->AI()->SetGUID(player->GetGUID());

            // Make clickable for gossip only.
            mount->SetFlag(UNIT_FIELD_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);

            TC_LOG_INFO("scripts", "zone_loch_modan: Spawned skystrider (entry %u) for player %llu (fallback mount display %u)",
                mount->GetEntry(), player->GetGUID(), FALLBACK_MOUNT_DISPLAY);
        }

        return true;
    }
};

class npc_skystrider_loch_modan : public CreatureScript
{
public:
    npc_skystrider_loch_modan() : CreatureScript("npc_skystrider_loch_modan") {}

    struct npc_skystrider_loch_modanAI : public ScriptedAI
    {
        npc_skystrider_loch_modanAI(Creature* creature)
            : ScriptedAI(creature),
            _currentPoint(0),
            _ownerGUID(0),
            _riding(false),
            _flameActive(false),
            _moveTimer(0),
            _moveDuration(0)
        {
            _waypoints =
            {
                { -5014.09f, -3653.12f,  304.72f, 0.0f }, // point 0
                { -4827.05f, -3623.09f,  319.29f, 0.0f }, // point 1
                { -4770.23f, -3520.06f,  317.29f, 0.0f }, // point 2
                { -4737.13f, -3317.39f,  337.08f, 0.0f }, // point 3
                { -4746.32f, -3031.16f,  363.44f, 0.0f }, // point 4
                { -4821.15f, -2830.97f,  472.40f, 0.0f }, // point 5
                { -4815.50f, -2738.37f,  349.43f, 0.0f }, // point 6
                { -4814.25f, -2700.92f,  330.76f, 0.0f }  // point 7 FINAL
            };
        }

        void Reset() override
        {
            _currentPoint = 0;
            _riding = false;
            _flameActive = false;
            _moveTimer = 0;
            _moveDuration = 0;

            if (_ownerGUID == 0)
                me->RemoveFlag(UNIT_FIELD_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);

            if (me->HasAura(SPELL_EXAUST_FLAMES))
                me->RemoveAurasDueToSpell(SPELL_EXAUST_FLAMES);

            me->SendSetPlayHoverAnim(false);
            me->ResetInhabitTypeOverride();
            me->SetAnimationTier(UnitAnimationTier::Ground);
            me->UpdateMovementFlags();
        }

        void SetGUID(uint64 guid, int32 /*id*/ = 0) override
        {
            _ownerGUID = guid;
            if (_ownerGUID)
                me->SetFlag(UNIT_FIELD_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
        }

        void JustDied(Unit* /*killer*/) override
        {
            cleanupRideState();
        }

        void cleanupRideState()
        {
            _ownerGUID = 0;
            _riding = false;
            _flameActive = false;
            _moveTimer = 0;
            _moveDuration = 0;

            if (me->HasAura(SPELL_EXAUST_FLAMES))
                me->RemoveAurasDueToSpell(SPELL_EXAUST_FLAMES);

            // make sure the helper stops forcing active update
            me->setActive(false);

            me->SendSetPlayHoverAnim(false);
            me->ResetInhabitTypeOverride();
            me->SetAnimationTier(UnitAnimationTier::Ground);
            me->UpdateMovementFlags();
        }

        // Compute travel duration (ms) between two points using PLAYER_TRAVEL_SPEED
        uint32 ComputeTravelTimeMs(Position const& from, Position const& to) const
        {
            const float dx = to.m_positionX - from.m_positionX;
            const float dy = to.m_positionY - from.m_positionY;
            const float dz = to.m_positionZ - from.m_positionZ;
            const float dist = sqrtf(dx * dx + dy * dy + dz * dz);
            const float secs = std::max(0.1f, dist / PLAYER_TRAVEL_SPEED);
            return uint32(secs * IN_MILLISECONDS);
        }

        // Start player-driven ride: per-leg MoveSplineInit so we can set velocity and use SetFly()
        void DoAction(int32 action) override
        {
            if (action != ACTION_START_RIDE)
                return;

            if (_ownerGUID == 0 || _riding)
                return;

            Player* owner = ObjectAccessor::FindPlayer(_ownerGUID);
            if (!owner || !owner->IsInWorld())
                return;

            // Ensure creature remains active server-side so its AI continues while player moves out of grid
            me->setActive(true);
            me->SetReactState(REACT_PASSIVE);

            // Apply visual mount if configured
            if (FALLBACK_MOUNT_DISPLAY)
            {
                owner->Mount(FALLBACK_MOUNT_DISPLAY);
                TC_LOG_INFO("scripts", "npc_skystrider_loch_modan: Applied fallback mount display %u for player %llu",
                    FALLBACK_MOUNT_DISPLAY, owner->GetGUID());
            }

            // Teleport player to exact start position
            owner->NearTeleportTo(me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), owner->GetOrientation());

            // Start first leg using MoveSplineInit so we control velocity and fly animation
            _currentPoint = 0;
            _riding = true;
            owner->StopMoving();

            Movement::MoveSplineInit init(owner);
            init.MoveTo(_waypoints[_currentPoint].m_positionX, _waypoints[_currentPoint].m_positionY, _waypoints[_currentPoint].m_positionZ, false);
            init.SetFly();                       // request fly animation
            init.SetUncompressed();
            init.SetVelocity(PLAYER_TRAVEL_SPEED); // enforce server velocity
            init.Launch();

            // compute expected duration for the leg
            Position playerPos;
            owner->GetPosition(&playerPos);
            _moveDuration = ComputeTravelTimeMs(playerPos, _waypoints[_currentPoint]);
            _moveTimer = 0;

            TC_LOG_INFO("scripts", "npc_skystrider_loch_modan: Launched spline for player %llu to waypoint %d -> duration %ums (VEL=%f)",
                owner->GetGUID(), _currentPoint, _moveDuration, PLAYER_TRAVEL_SPEED);
        }

        void UpdateAI(uint32 diff) override
        {
            if (!_riding || _ownerGUID == 0)
                return;

            Player* owner = ObjectAccessor::FindPlayer(_ownerGUID);
            if (!owner || !owner->IsInWorld())
            {
                // If the player is no longer world-present, end ride safely
                _riding = false;
                // Restore helper active state to default
                me->setActive(false);
                return;
            }

            // accumulate leg time
            _moveTimer += diff;
            if (_moveTimer < _moveDuration)
                return;

            // Leg complete (timed). Treat as arrival.
            TC_LOG_INFO("scripts", "npc_skystrider_loch_modan: timed arrival for player %llu at waypoint %d", owner->GetGUID(), _currentPoint);

            if (_currentPoint == 0 && !_flameActive)
            {
                // Apply flame aura (triggered)
                owner->CastSpell(owner, SPELL_EXAUST_FLAMES, true);
                _flameActive = true;

                // enable flight visuals/flags (keep to prevent snap)
                uint32 mvFlags = owner->GetUnitMovementFlags();
                mvFlags |= MOVEMENTFLAG_CAN_FLY | MOVEMENTFLAG_DISABLE_GRAVITY;
                owner->SetUnitMovementFlags(mvFlags);
            }

            // Advance to next waypoint
            ++_currentPoint;
            _moveTimer = 0;

            // If finished, cleanup
            if (_currentPoint >= static_cast<int>(_waypoints.size()))
            {
                // stop movement and restore control
                owner->StopMoving();
                owner->GetMotionMaster()->MoveIdle();

                // remove visual mount
                if (FALLBACK_MOUNT_DISPLAY)
                    owner->Dismount();

                // remove aura if applied
                if (_flameActive)
                {
                    owner->RemoveAurasDueToSpell(SPELL_EXAUST_FLAMES);
                    _flameActive = false;
                }

                // Restore ground movement flags for player
                uint32 mvFlags = owner->GetUnitMovementFlags();
                mvFlags &= ~(MOVEMENTFLAG_CAN_FLY | MOVEMENTFLAG_DISABLE_GRAVITY);
                owner->SetUnitMovementFlags(mvFlags);

                // place player slightly ahead to avoid overlap
                const Position& last = _waypoints.back();
                owner->NearTeleportTo(last.m_positionX + 1.5f, last.m_positionY, last.m_positionZ, owner->GetOrientation());

                // Despawn helper NPC now that the ride is complete
                if (me && me->IsInWorld())
                {
                    me->setActive(false);
                    me->DespawnOrUnsummon(2 * IN_MILLISECONDS);
                }

                _riding = false;
                _ownerGUID = 0;

                TC_LOG_INFO("scripts", "npc_skystrider_loch_modan: Player-driven ride finished for %llu", owner->GetGUID());
                return;
            }

            // Command the player to move to the next waypoint using MoveSplineInit (so we can set velocity and SetFly)
            owner->StopMoving();

            Movement::MoveSplineInit init(owner);
            init.MoveTo(_waypoints[_currentPoint].m_positionX, _waypoints[_currentPoint].m_positionY, _waypoints[_currentPoint].m_positionZ, false);
            init.SetFly();
            init.SetUncompressed();
            init.SetVelocity(PLAYER_TRAVEL_SPEED);
            init.Launch();

            // compute expected duration for this leg
            Position fromPos = _waypoints[_currentPoint - 1];
            Position toPos = _waypoints[_currentPoint];
            _moveDuration = ComputeTravelTimeMs(fromPos, toPos);
            _moveTimer = 0;

            TC_LOG_INFO("scripts", "npc_skystrider_loch_modan: Launched spline for player %llu to waypoint %d -> duration %ums (VEL=%f)",
                owner->GetGUID(), _currentPoint, _moveDuration, PLAYER_TRAVEL_SPEED);
        }

        // Gossip helpers
        bool HandleGossipHello(Player* player)
        {
            if (!player)
                return false;

            if (player->GetGUID() != _ownerGUID)
                return false;

            player->PlayerTalkClass->ClearMenus();
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Mount the Skystrider.", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF);
            player->SEND_GOSSIP_MENU(DEFAULT_GOSSIP_MESSAGE, me->GetGUID());
            return true;
        }

        bool HandleGossipSelect(Player* player, uint32 /*sender*/, uint32 action)
        {
            if (!player)
                return false;

            if (player->GetGUID() != _ownerGUID)
            {
                player->CLOSE_GOSSIP_MENU();
                return false;
            }

            if (action == GOSSIP_ACTION_INFO_DEF)
            {
                player->CLOSE_GOSSIP_MENU();
                DoAction(ACTION_START_RIDE);
                return true;
            }

            player->CLOSE_GOSSIP_MENU();
            return false;
        }

    private:
        std::vector<Position> _waypoints;
        int _currentPoint;
        uint64 _ownerGUID;
        bool _riding;
        bool _flameActive;

        // timing for each leg (ms)
        uint32 _moveTimer;
        uint32 _moveDuration;
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_skystrider_loch_modanAI(creature);
    }

    // CreatureScript overrides that delegate to AI helper methods
    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (!creature || !creature->AI())
            return false;

        if (auto ai = dynamic_cast<npc_skystrider_loch_modanAI*>(creature->AI()))
            return ai->HandleGossipHello(player);

        return false;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action) override
    {
        if (!creature || !creature->AI())
            return false;

        if (auto ai = dynamic_cast<npc_skystrider_loch_modanAI*>(creature->AI()))
            return ai->HandleGossipSelect(player, sender, action);

        return false;
    }
};

void AddSC_loch_modan()
{
    new npc_ando_loch_modan();
    new npc_skystrider_loch_modan();
}