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

#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "Player.h"
#include "TemporarySummon.h"
#include "MovementGenerator.h"
#include "MoveSplineInit.h"
#include <vector>
#include <cmath>

constexpr uint32 FALLBACK_RAM_DISPLAY = 14347;
constexpr float PLAYER_TRAVEL_SPEED = 15.0f;

enum WetlandsData
{
    NPC_RAM_MOUNT = 41260,
    QUEST_ONWARDS_TO_MENETHIL = 25777
};

enum GossipItemsWetlands
{
    GOSSIP_TEXT_MOUNT_RAM = 1
};

enum ActionsWetlands
{
    ACTION_START_RAM_RIDE = 1
};

class npc_ram_handler_wetlands : public CreatureScript
{
public:
    npc_ram_handler_wetlands() : CreatureScript("npc_ram_handler_wetlands") {}

    bool OnQuestAccept(Player* player, Creature* /*creature*/, Quest const* quest) override
    {
        if (!player || !quest)
            return false;

        if (quest->GetQuestId() != QUEST_ONWARDS_TO_MENETHIL)
            return false;

        Position spawnPos;
        player->GetPosition(&spawnPos);
        spawnPos.m_positionX += 1.5f;

        Creature* mount = player->SummonCreature(NPC_RAM_MOUNT, spawnPos, TEMPSUMMON_TIMED_DESPAWN, 15 * MINUTE * IN_MILLISECONDS);
        if (mount && mount->AI())
        {
            mount->AI()->SetGUID(player->GetGUID());
            mount->SetFlag(UNIT_FIELD_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
            TC_LOG_INFO("scripts", "zone_wetlands: Spawned ram mount (entry %u) for player %llu (fallback mount display %u)",
                mount->GetEntry(), player->GetGUID(), FALLBACK_RAM_DISPLAY);
        }

        return true;
    }
};

class npc_ram_mount_wetlands : public CreatureScript
{
public:
    npc_ram_mount_wetlands() : CreatureScript("npc_ram_mount_wetlands") {}

    struct npc_ram_mount_wetlandsAI : public ScriptedAI
    {
        npc_ram_mount_wetlandsAI(Creature* creature)
            : ScriptedAI(creature),
            _currentPoint(0),
            _ownerGUID(0),
            _riding(false),
            _moveTimer(0),
            _moveDuration(0)
        {
			_waypoints =   // works, needs tuning for positions, bit glitchy at times
            {
                { -4123.509766f, -2749.500000f, 18.148001f, 0.0f },
                { -4062.238037f, -2764.238770f, 17.822952f, 0.0f },
                { -3992.754395f, -2815.018555f, 17.807529f, 0.0f },
                { -3907.799072f, -2820.700195f, 17.859129f, 0.0f },
                { -3854.158447f, -2783.296875f, 18.077957f, 0.0f },
                { -3746.792236f, -2785.200684f, 17.748785f, 0.0f },
                { -3588.572021f, -2698.978027f, 19.053427f, 0.0f },
                { -3369.136963f, -2585.720459f, 15.962731f, 0.0f },
                { -3316.303223f, -2573.647461f, 15.962731f, 0.0f },
                { -3256.875244f, -2509.426025f, 16.051151f, 0.0f },
                { -3224.636963f, -2416.211914f, 15.805967f, 0.0f },
                { -3220.253906f, -2255.453369f, 15.804342f, 0.0f },
                { -3188.215820f, -2181.207275f, 15.804749f, 0.0f },
                { -3160.730713f, -1958.889893f, 13.574751f, 0.0f },
                { -3183.900146f, -1905.623291f, 14.050876f, 0.0f },
                { -3187.923096f, -1846.371704f, 9.146303f, 0.0f },
                { -3308.753174f, -1482.454590f, 9.146404f, 0.0f },
                { -3320.298828f, -1402.959961f, 9.146404f, 0.0f },
                { -3353.691650f, -1336.406006f, 9.146404f, 0.0f },
                { -3340.740234f, -1266.152466f, 9.147142f, 0.0f },
                { -3359.542480f, -1083.653076f, 9.145136f, 0.0f },
                { -3437.575928f, -965.146423f, 8.866099f, 0.0f },
                { -3540.034180f, -897.294434f, 5.505500f, 0.0f },
                { -3677.575439f, -827.796204f, 0.250592f, 0.0f }
            };
        }

        void Reset() override
        {
            _currentPoint = 0;
            _riding = false;
            _moveTimer = 0;
            _moveDuration = 0;

            if (_ownerGUID == 0)
                me->RemoveFlag(UNIT_FIELD_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);

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
            _moveTimer = 0;
            _moveDuration = 0;
            me->setActive(false);
            me->SendSetPlayHoverAnim(false);
            me->ResetInhabitTypeOverride();
            me->SetAnimationTier(UnitAnimationTier::Ground);
            me->UpdateMovementFlags();
        }

        uint32 ComputeTravelTimeMs(Position const& from, Position const& to) const
        {
            const float dx = to.m_positionX - from.m_positionX;
            const float dy = to.m_positionY - from.m_positionY;
            const float dz = to.m_positionZ - from.m_positionZ;
            const float dist = sqrtf(dx * dx + dy * dy + dz * dz);
            const float secs = std::max(0.1f, dist / PLAYER_TRAVEL_SPEED);
            return uint32(secs * IN_MILLISECONDS);
        }

        void DoAction(int32 action) override
        {
            if (action != ACTION_START_RAM_RIDE)
                return;

            if (_ownerGUID == 0 || _riding)
                return;

            Player* owner = ObjectAccessor::FindPlayer(_ownerGUID);
            if (!owner || !owner->IsInWorld())
                return;

            me->setActive(true);
            me->SetReactState(REACT_PASSIVE);

            if (FALLBACK_RAM_DISPLAY)
            {
                owner->Mount(FALLBACK_RAM_DISPLAY);
                TC_LOG_INFO("scripts", "npc_ram_mount_wetlands: Applied fallback ram display %u for player %llu",
                    FALLBACK_RAM_DISPLAY, owner->GetGUID());
            }

            owner->NearTeleportTo(me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), owner->GetOrientation());

            _currentPoint = 0;
            _riding = true;
            owner->StopMoving();

            Movement::MoveSplineInit init(owner);
            init.MoveTo(_waypoints[_currentPoint].m_positionX, _waypoints[_currentPoint].m_positionY, _waypoints[_currentPoint].m_positionZ, false);
            init.SetUncompressed();
            init.SetVelocity(PLAYER_TRAVEL_SPEED);
            init.Launch();

            Position playerPos;
            owner->GetPosition(&playerPos);
            _moveDuration = ComputeTravelTimeMs(playerPos, _waypoints[_currentPoint]);
            _moveTimer = 0;

            TC_LOG_INFO("scripts", "npc_ram_mount_wetlands: Launched spline for player %llu to waypoint %d -> duration %ums (VEL=%f)",
                owner->GetGUID(), _currentPoint, _moveDuration, PLAYER_TRAVEL_SPEED);
        }

        void UpdateAI(uint32 diff) override
        {
            if (!_riding || _ownerGUID == 0)
                return;

            Player* owner = ObjectAccessor::FindPlayer(_ownerGUID);
            if (!owner || !owner->IsInWorld())
            {
                _riding = false;
                me->setActive(false);
                return;
            }

            _moveTimer += diff;
            if (_moveTimer < _moveDuration)
                return;

            TC_LOG_INFO("scripts", "npc_ram_mount_wetlands: timed arrival for player %llu at waypoint %d", owner->GetGUID(), _currentPoint);

            ++_currentPoint;
            _moveTimer = 0;

            if (_currentPoint >= static_cast<int>(_waypoints.size()))
            {
                owner->StopMoving();
                owner->GetMotionMaster()->MoveIdle();

                if (FALLBACK_RAM_DISPLAY)
                    owner->Dismount();

                const Position& last = _waypoints.back();
                owner->NearTeleportTo(last.m_positionX + 1.5f, last.m_positionY, last.m_positionZ, owner->GetOrientation());

                if (me && me->IsInWorld())
                {
                    me->setActive(false);
                    me->DespawnOrUnsummon(2 * IN_MILLISECONDS);
                }

                _riding = false;
                _ownerGUID = 0;

                TC_LOG_INFO("scripts", "npc_ram_mount_wetlands: Player-driven ride finished for %llu", owner->GetGUID());
                return;
            }

            owner->StopMoving();

            Movement::MoveSplineInit init(owner);
            init.MoveTo(_waypoints[_currentPoint].m_positionX, _waypoints[_currentPoint].m_positionY, _waypoints[_currentPoint].m_positionZ, false);
            init.SetUncompressed();
            init.SetVelocity(PLAYER_TRAVEL_SPEED);
            init.Launch();

            Position fromPos = _waypoints[_currentPoint - 1];
            Position toPos = _waypoints[_currentPoint];
            _moveDuration = ComputeTravelTimeMs(fromPos, toPos);
            _moveTimer = 0;

            TC_LOG_INFO("scripts", "npc_ram_mount_wetlands: Launched spline for player %llu to waypoint %d -> duration %ums (VEL=%f)",
                owner->GetGUID(), _currentPoint, _moveDuration, PLAYER_TRAVEL_SPEED);
        }

        bool HandleGossipHello(Player* player)
        {
            if (!player)
                return false;

            if (player->GetGUID() != _ownerGUID)
                return false;

            player->PlayerTalkClass->ClearMenus();
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Mount the Ram.", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF);
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
                DoAction(ACTION_START_RAM_RIDE);
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
        uint32 _moveTimer;
        uint32 _moveDuration;
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_ram_mount_wetlandsAI(creature);
    }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (!creature || !creature->AI())
            return false;

        if (auto ai = dynamic_cast<npc_ram_mount_wetlandsAI*>(creature->AI()))
            return ai->HandleGossipHello(player);

        return false;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action) override
    {
        if (!creature || !creature->AI())
            return false;

        if (auto ai = dynamic_cast<npc_ram_mount_wetlandsAI*>(creature->AI()))
            return ai->HandleGossipSelect(player, sender, action);

        return false;
    }
};

void AddSC_wetlands()
{
    new npc_ram_handler_wetlands();
    new npc_ram_mount_wetlands();
}