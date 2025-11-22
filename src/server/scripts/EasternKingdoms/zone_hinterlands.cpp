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
 SDName: Hinterlands
 SD%Complete: 100
 SDComment: Quest support: 836
 SDCategory: The Hinterlands
 EndScriptData */

 /* ContentData
 npc_00x09hl
 EndContentData */

#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedEscortAI.h"
#include "Player.h"
#include "ScriptedGossip.h"
#include "TemporarySummon.h"
#include "MovementGenerator.h"
#include "MoveSplineInit.h"
#include <vector>
#include <cmath>

 /*######
 ## npc_00x09hl
 ######*/

enum eOOX
{
    SAY_OOX_START = 0,
    SAY_OOX_AGGRO = 1,
    SAY_OOX_AMBUSH = 3,
    SAY_OOX_AMBUSH_REPLY = 4,
    SAY_OOX_END = 5,

    QUEST_RESQUE_OOX_09 = 836,

    NPC_MARAUDING_OWL = 7808,
    NPC_VILE_AMBUSHER = 7809,

    FACTION_ESCORTEE_A = 774,
    FACTION_ESCORTEE_H = 775
};

class npc_00x09hl : public CreatureScript
{
public:
    npc_00x09hl() : CreatureScript("npc_00x09hl") {}

    bool OnQuestAccept(Player* player, Creature* creature, const Quest* quest)
    {
        if (quest->GetQuestId() == QUEST_RESQUE_OOX_09)
        {
            creature->SetStandState(UNIT_STAND_STATE_STAND);

            if (player->GetTeam() == ALLIANCE)
                creature->setFaction(FACTION_ESCORTEE_A);
            else if (player->GetTeam() == HORDE)
                creature->setFaction(FACTION_ESCORTEE_H);

            creature->AI()->Talk(SAY_OOX_START, player);

            if (npc_00x09hlAI* pEscortAI = CAST_AI(npc_00x09hl::npc_00x09hlAI, creature->AI()))
                pEscortAI->Start(false, false, player->GetGUID(), quest);
        }
        return true;
    }

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_00x09hlAI(creature);
    }

    struct npc_00x09hlAI : public npc_escortAI
    {
        npc_00x09hlAI(Creature* creature) : npc_escortAI(creature) {}

        void Reset() override {}

        void WaypointReached(uint32 waypointId)
        {
            switch (waypointId)
            {
            case 26:
                Talk(SAY_OOX_AMBUSH);
                break;
            case 43:
                Talk(SAY_OOX_AMBUSH);
                break;
            case 64:
                Talk(SAY_OOX_END);
                if (Player* player = GetPlayerForEscort())
                    player->GroupEventHappens(QUEST_RESQUE_OOX_09, me);
                break;
            }
        }

        void WaypointStart(uint32 uiPointId)
        {
            switch (uiPointId)
            {
            case 27:
                for (uint8 i = 0; i < 3; ++i)
                {
                    const Position src = { 147.927444f, -3851.513428f, 130.893f, 0 };
                    Position dst;
                    me->GetRandomPoint(src, 7.0f, dst);
                    DoSummon(NPC_MARAUDING_OWL, dst, 25000, TEMPSUMMON_CORPSE_TIMED_DESPAWN);
                }
                break;
            case 44:
                for (uint8 i = 0; i < 3; ++i)
                {
                    const Position src = { -141.151581f, -4291.213867f, 120.130f, 0 };
                    Position dst;
                    me->GetRandomPoint(src, 7.0f, dst);
                    me->SummonCreature(NPC_VILE_AMBUSHER, dst, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 25000);
                }
                break;
            }
        }

        void EnterCombat(Unit* who)
        {
            if (who->GetEntry() == NPC_MARAUDING_OWL || who->GetEntry() == NPC_VILE_AMBUSHER)
                return;

            Talk(SAY_OOX_AGGRO);
        }

        void JustSummoned(Creature* summoned) override
        {
            summoned->GetMotionMaster()->MovePoint(0, me->GetPositionX(), me->GetPositionY(), me->GetPositionZ());
        }
    };
};

// --- FLIGHT RIDE SCRIPT ---

constexpr uint32 FALLBACK_MOUNT_DISPLAY = 1149;
constexpr float HINTERLANDS_TRAVEL_SPEED = 30.0f;

enum HinterlandsWaypointData
{
    NPC_FLIGHT_STARTER = 80183,
    NPC_FLIGHT_MOUNT = 2657,
    QUEST_FLIGHT_PATH = 26548
};

enum GossipItemsHinterlands
{
    GOSSIP_TEXT_START_FLIGHT = 1
};

enum ActionsHinterlands
{
    ACTION_START_FLIGHT = 1
};

class npc_flight_starter_hinterlands : public CreatureScript
{
public:
    npc_flight_starter_hinterlands() : CreatureScript("npc_flight_starter_hinterlands") {}

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (!player || !creature)
            return false;

        player->PlayerTalkClass->ClearMenus();
        player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Begin the flight.", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF);
        player->SEND_GOSSIP_MENU(DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        if (!player || !creature)
            return false;

        player->CLOSE_GOSSIP_MENU();

        if (action == GOSSIP_ACTION_INFO_DEF)
        {
            Position spawnPos;
            player->GetPosition(&spawnPos);
            spawnPos.m_positionX += 1.5f;

            Creature* mount = player->SummonCreature(NPC_FLIGHT_MOUNT, spawnPos, TEMPSUMMON_TIMED_DESPAWN, 15 * MINUTE * IN_MILLISECONDS);
            if (mount && mount->AI())
            {
                mount->AI()->SetGUID(player->GetGUID());
                mount->SetFlag(UNIT_FIELD_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
                TC_LOG_INFO("scripts", "zone_hinterlands: Spawned flight mount (entry %u) for player %llu (mount display %u)",
                    mount->GetEntry(), player->GetGUID(), FALLBACK_MOUNT_DISPLAY);
            }
        }
        return true;
    }
};

class npc_flight_mount_hinterlands : public CreatureScript
{
public:
    npc_flight_mount_hinterlands() : CreatureScript("npc_flight_mount_hinterlands") {}

    struct npc_flight_mount_hinterlandsAI : public ScriptedAI
    {
        npc_flight_mount_hinterlandsAI(Creature* creature)
            : ScriptedAI(creature),
            _currentPoint(0),
            _ownerGUID(0),
            _riding(false),
            _moveTimer(0),
            _moveDuration(0)
        {
            _waypoints = {
                { 274.707733f,  -2020.807495f, 199.292282f, 0.0f },
                {  64.873444f,  -2385.246826f, 218.008118f, 0.0f },
                { 187.696854f,  -3328.645264f, 210.665283f, 0.0f },
                { 209.462189f,  -4139.488281f, 249.627319f, 0.0f },
                { 278.974640f,  -4104.114258f, 133.077209f, 0.0f },
                { 310.467560f,  -4104.699707f, 121.964745f, 0.0f }
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
            const float secs = std::max(0.1f, dist / HINTERLANDS_TRAVEL_SPEED);
            return uint32(secs * IN_MILLISECONDS);
        }

        void DoAction(int32 action) override
        {
            if (action != ACTION_START_FLIGHT)
                return;

            if (_ownerGUID == 0 || _riding)
                return;

            Player* owner = ObjectAccessor::FindPlayer(_ownerGUID);
            if (!owner || !owner->IsInWorld())
                return;

            me->setActive(true);
            me->SetReactState(REACT_PASSIVE);

            // Apply fallback mount display
            if (FALLBACK_MOUNT_DISPLAY)
            {
                owner->Mount(FALLBACK_MOUNT_DISPLAY);
                TC_LOG_INFO("scripts", "npc_flight_mount_hinterlands: Applied fallback mount display %u for player %llu",
                    FALLBACK_MOUNT_DISPLAY, owner->GetGUID());
            }

            // Enable flying and disable gravity for the player
            uint32 mvFlags = owner->GetUnitMovementFlags();
            mvFlags |= MOVEMENTFLAG_CAN_FLY | MOVEMENTFLAG_DISABLE_GRAVITY;
            owner->SetUnitMovementFlags(mvFlags);

            owner->NearTeleportTo(me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), owner->GetOrientation());

            _currentPoint = 0;
            _riding = true;
            owner->StopMoving();

            if (_waypoints.empty())
                return;

            Movement::MoveSplineInit init(owner);
            init.MoveTo(_waypoints[_currentPoint].m_positionX, _waypoints[_currentPoint].m_positionY, _waypoints[_currentPoint].m_positionZ, false);
            init.SetUncompressed();
            init.SetVelocity(HINTERLANDS_TRAVEL_SPEED);
            init.Launch();

            Position playerPos;
            owner->GetPosition(&playerPos);
            _moveDuration = ComputeTravelTimeMs(playerPos, _waypoints[_currentPoint]);
            _moveTimer = 0;

            TC_LOG_INFO("scripts", "npc_flight_mount_hinterlands: Launched spline for player %llu to waypoint %d -> duration %ums (VEL=%f)",
                owner->GetGUID(), _currentPoint, _moveDuration, HINTERLANDS_TRAVEL_SPEED);
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

            TC_LOG_INFO("scripts", "npc_flight_mount_hinterlands: timed arrival for player %llu at waypoint %d", owner->GetGUID(), _currentPoint);

            ++_currentPoint;
            _moveTimer = 0;

            if (_currentPoint >= static_cast<int>(_waypoints.size()))
            {
                owner->StopMoving();
                owner->GetMotionMaster()->MoveIdle();

                // Remove fallback mount display
                if (FALLBACK_MOUNT_DISPLAY)
                    owner->Dismount();

                // Restore ground movement flags for the player
                uint32 mvFlags = owner->GetUnitMovementFlags();
                mvFlags &= ~(MOVEMENTFLAG_CAN_FLY | MOVEMENTFLAG_DISABLE_GRAVITY);
                owner->SetUnitMovementFlags(mvFlags);

                if (!_waypoints.empty())
                {
                    const Position& last = _waypoints.back();
                    owner->NearTeleportTo(last.m_positionX + 1.5f, last.m_positionY, last.m_positionZ, owner->GetOrientation());
                }

                if (me && me->IsInWorld())
                {
                    me->setActive(false);
                    me->DespawnOrUnsummon(2 * IN_MILLISECONDS);
                }

                _riding = false;
                _ownerGUID = 0;

                TC_LOG_INFO("scripts", "npc_flight_mount_hinterlands: Player-driven flight finished for %llu", owner->GetGUID());
                return;
            }

            owner->StopMoving();

            Movement::MoveSplineInit init(owner);
            init.MoveTo(_waypoints[_currentPoint].m_positionX, _waypoints[_currentPoint].m_positionY, _waypoints[_currentPoint].m_positionZ, false);
            init.SetUncompressed();
            init.SetVelocity(HINTERLANDS_TRAVEL_SPEED);
            init.Launch();

            Position fromPos = _waypoints[_currentPoint - 1];
            Position toPos = _waypoints[_currentPoint];
            _moveDuration = ComputeTravelTimeMs(fromPos, toPos);
            _moveTimer = 0;

            TC_LOG_INFO("scripts", "npc_flight_mount_hinterlands: Launched spline for player %llu to waypoint %d -> duration %ums (VEL=%f)",
                owner->GetGUID(), _currentPoint, _moveDuration, HINTERLANDS_TRAVEL_SPEED);
        }

        bool HandleGossipHello(Player* player)
        {
            if (!player)
                return false;

            if (player->GetGUID() != _ownerGUID)
                return false;

            player->PlayerTalkClass->ClearMenus();
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Begin the flight.", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF);
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
                DoAction(ACTION_START_FLIGHT);
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
        return new npc_flight_mount_hinterlandsAI(creature);
    }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (!creature || !creature->AI())
            return false;

        if (auto ai = dynamic_cast<npc_flight_mount_hinterlandsAI*>(creature->AI()))
            return ai->HandleGossipHello(player);

        return false;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action) override
    {
        if (!creature || !creature->AI())
            return false;

        if (auto ai = dynamic_cast<npc_flight_mount_hinterlandsAI*>(creature->AI()))
            return ai->HandleGossipSelect(player, sender, action);

        return false;
    }
};


enum ShadraQuestData
{
    QUEST_SHADRA_ALLIANCE = 26532,
    NPC_QUEST_GIVER = 43298,
    NPC_SHADRA = 43007
};

class npc_quest_giver_shadra : public CreatureScript
{
public:
    npc_quest_giver_shadra() : CreatureScript("npc_quest_giver_shadra") {}

    bool OnQuestAccept(Player* player, Creature* creature, const Quest* quest) override
    {
        if (!player || !creature || !quest)
            return false;

        if (quest->GetQuestId() == QUEST_SHADRA_ALLIANCE)
        {
            // Summon Shadra at the specified location
            float x = -354.881195f;
            float y = -2886.518555f;
            float z = 69.719444f;
            float o = 0.0f; // Set orientation as needed

            creature->SummonCreature(NPC_SHADRA, x, y, z, o, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 10 * MINUTE * IN_MILLISECONDS);
        }
        return true;
    }
};

void AddSC_hinterlands()
{
    new npc_flight_starter_hinterlands();
    new npc_flight_mount_hinterlands();
    new npc_00x09hl();
	new npc_quest_giver_shadra();
}