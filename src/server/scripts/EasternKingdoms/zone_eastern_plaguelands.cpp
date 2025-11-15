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
SDName: Eastern_Plaguelands
SD%Complete: 100
SDComment: Quest support: 5211, 5742. Special vendor Augustus the Touched
SDCategory: Eastern Plaguelands
EndScriptData */

/* ContentData
npc_ghoul_flayer
npc_augustus_the_touched
npc_darrowshire_spirit
npc_tirion_fordring
EndContentData */

#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "Player.h"
#include "WorldSession.h"

class npc_ghoul_flayer : public CreatureScript
{
public:
    npc_ghoul_flayer() : CreatureScript("npc_ghoul_flayer") { }

    struct npc_ghoul_flayerAI : public ScriptedAI
    {
        npc_ghoul_flayerAI(Creature* creature) : ScriptedAI(creature) { }

        void Reset() override { }

        void EnterCombat(Unit* /*who*/) override { }

        void JustDied(Unit* killer) override
        {
            if (killer->GetTypeId() == TYPEID_PLAYER)
                me->SummonCreature(11064, 0.0f, 0.0f, 0.0f, 0.0f, TEMPSUMMON_TIMED_DESPAWN, 60000);
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_ghoul_flayerAI(creature);
    }
};

/*######
## npc_augustus_the_touched
######*/

class npc_augustus_the_touched : public CreatureScript
{
public:
    npc_augustus_the_touched() : CreatureScript("npc_augustus_the_touched") { }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        player->PlayerTalkClass->ClearMenus();
        if (action == GOSSIP_ACTION_TRADE)
            player->GetSession()->SendListInventory(creature->GetGUID());
        return true;
    }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (creature->IsQuestGiver())
            player->PrepareQuestMenu(creature->GetGUID());

        if (creature->IsVendor() && player->GetQuestRewardStatus(6164))
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_VENDOR, GOSSIP_TEXT_BROWSE_GOODS, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_TRADE);

        player->SEND_GOSSIP_MENU(player->GetGossipTextId(creature), creature->GetGUID());
        return true;
    }
};

/*######
## npc_darrowshire_spirit
######*/
// npc_darrowshire_spirit — play die spell 3 times (every 1.5s) then despawn

enum DarrowshireSpiritDefs
{
    SPELL_SPIRIT_SPAWNIN = 17321,
    SPELL_DIE_EFFECT = 99634,

    ACTION_PLAY_DIE_SEQUENCE = 1
};

class npc_darrowshire_spirit : public CreatureScript
{
public:
    npc_darrowshire_spirit() : CreatureScript("npc_darrowshire_spirit") {}

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        // Prevent re-triggering if sequence already started on this NPC
        if (creature->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE))
        {
            player->SEND_GOSSIP_MENU(3873, creature->GetGUID());
            return true;
        }

        player->SEND_GOSSIP_MENU(3873, creature->GetGUID());
        player->TalkedToCreature(creature->GetEntry(), creature->GetGUID());

        // Give quest credit immediately for the original spirit entry.
        player->KilledMonsterCredit(creature->GetEntry());

        // Prevent further interaction while we play the effect
        creature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);

        // Instruct this creature's AI to run the die sequence (3 casts @ 1.5s)
        if (CreatureAI* ai = creature->AI())
        {
            if (auto* specificAI = static_cast<npc_darrowshire_spirit::npc_darrowshire_spiritAI*>(ai))
                specificAI->DoAction(ACTION_PLAY_DIE_SEQUENCE);
        }

        return true;
    }

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_darrowshire_spiritAI(creature);
    }

    struct npc_darrowshire_spiritAI : public ScriptedAI
    {
        explicit npc_darrowshire_spiritAI(Creature* creature)
            : ScriptedAI(creature), m_running(false), m_castsRemaining(0), m_castTimer(0), m_despawnTimer(0)
        {
        }

        void Reset() override
        {
            DoCast(me, SPELL_SPIRIT_SPAWNIN);
            me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
            m_running = false;
            m_castsRemaining = 0;
            m_castTimer = 0;
            m_despawnTimer = 0;
        }

        void EnterCombat(Unit* /*who*/) override {}


        void DoAction(int32 action) override
        {
            if (action == ACTION_PLAY_DIE_SEQUENCE && !m_running)
            {
                m_running = true;
                m_castsRemaining = 3;        // total casts

                // Make the NPC non-selectable and non-attackable BEFORE casting
                me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_ATTACKABLE_1);

                // Ensure it's passive and out of combat state
                me->CombatStop();
                me->DeleteThreatList();
                me->AttackStop();
                me->RemoveAllAuras();

                // Set react state to passive so it won't try to fight back (engine provides this on Unit)
                me->SetReactState(REACT_PASSIVE);

                // perform first cast immediately
                DoCast(me, SPELL_DIE_EFFECT);
                --m_castsRemaining;

                // schedule next cast in 1500ms
                m_castTimer = 1500;
                m_despawnTimer = 0;
            }
        }

        void UpdateAI(uint32 diff) override
        {
            if (m_running)
            {
                if (m_castsRemaining > 0)
                {
                    if (m_castTimer > diff)
                        m_castTimer -= diff;
                    else
                    {
                        // time to cast
                        DoCast(me, SPELL_DIE_EFFECT);
                        --m_castsRemaining;
                        m_castTimer = 1500; // next cast in 1.5s (if any)
                    }
                }
                else
                {
                    // all casts done — start short despawn timer (keep last visual visible)
                    if (m_despawnTimer == 0)
                        m_despawnTimer = 500; // 0.5s after last cast
                    if (m_despawnTimer > diff)
                        m_despawnTimer -= diff;
                    else
                    {
                        m_running = false;
                        me->DespawnOrUnsummon();
                        return;
                    }
                }
            }

            ScriptedAI::UpdateAI(diff);
        }

    private:
        bool m_running;
        int32 m_castsRemaining;
        uint32 m_castTimer;
        uint32 m_despawnTimer;
    };
};


/*######
## npc_tirion_fordring
######*/

#define GOSSIP_HELLO    "I am ready to hear your tale, Tirion."
#define GOSSIP_SELECT1  "Thank you, Tirion.  What of your identity?"
#define GOSSIP_SELECT2  "That is terrible."
#define GOSSIP_SELECT3  "I will, Tirion."

class npc_tirion_fordring : public CreatureScript
{
public:
    npc_tirion_fordring() : CreatureScript("npc_tirion_fordring") { }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        player->PlayerTalkClass->ClearMenus();
        switch (action)
        {
            case GOSSIP_ACTION_INFO_DEF+1:
                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_SELECT1, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 2);
                player->SEND_GOSSIP_MENU(4493, creature->GetGUID());
                break;
            case GOSSIP_ACTION_INFO_DEF+2:
                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_SELECT2, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 3);
                player->SEND_GOSSIP_MENU(4494, creature->GetGUID());
                break;
            case GOSSIP_ACTION_INFO_DEF+3:
                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_SELECT3, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 4);
                player->SEND_GOSSIP_MENU(4495, creature->GetGUID());
                break;
            case GOSSIP_ACTION_INFO_DEF+4:
                player->CLOSE_GOSSIP_MENU();
                player->AreaExploredOrEventHappens(5742);
                break;
        }
        return true;
    }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (creature->IsQuestGiver())
            player->PrepareQuestMenu(creature->GetGUID());

        if (player->GetQuestStatus(5742) == QUEST_STATUS_INCOMPLETE && player->getStandState() == UNIT_STAND_STATE_SIT)
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_HELLO, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF+1);

        player->SEND_GOSSIP_MENU(player->GetGossipTextId(creature), creature->GetGUID());

        return true;
    }
};

enum
{
    TALK_DIED                  = 0,
    TALK_CREDIT_1              = 1,
    TALK_CREDIT_2              = 2,

    EVENT_THROW_DYNAMITE       = 1,
    SPELL_THROW_DYNAMITE       = 86088,

    QUEST_BEAT_IT_OUT_OF_THEM  = 27522,

    OBJECTIVE_ENGINEERS        = 265859,
    OBJECTIVE_GIDVINS_LOCATION = 265860,
    OBJECTIVE_GIDVINS_CAPTOR   = 265861,

    NPC_CREDIT_1               = 45845,
    NPC_CREDIT_2               = 45846,
};

// Scourge Siege Engineer - 17878
struct npc_scourge_siege_engineer : public ScriptedAI
{
    npc_scourge_siege_engineer(Creature* creature) : ScriptedAI(creature) { }

    EventMap events;

    void Reset() override
    {
        events.Reset();
    }

    void EnterCombat(Unit* /*who*/) override
    {
        events.ScheduleEvent(EVENT_THROW_DYNAMITE, 10000);
    }

    void JustDied(Unit* killer) override
    {
        if (Player* player = killer->ToPlayer())
        {
            if (player->GetQuestStatus(QUEST_BEAT_IT_OUT_OF_THEM) == QUEST_STATUS_INCOMPLETE)
            {
                if (player->GetQuestObjectiveCounter(OBJECTIVE_ENGINEERS) == 3 && !player->GetQuestObjectiveCounter(OBJECTIVE_GIDVINS_LOCATION))
                {
                    Talk(TALK_CREDIT_1, player);
                    player->KilledMonsterCredit(NPC_CREDIT_1);
                }
                else if (player->GetQuestObjectiveCounter(OBJECTIVE_ENGINEERS) == 6 && !player->GetQuestObjectiveCounter(OBJECTIVE_GIDVINS_CAPTOR))
                {
                    Talk(TALK_CREDIT_2, player);
                    player->KilledMonsterCredit(NPC_CREDIT_2);
                }
                else
                    Talk(TALK_DIED, player);
            }
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        events.Update(diff);

        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_THROW_DYNAMITE:
                    DoCastVictim(SPELL_THROW_DYNAMITE);
                    events.ScheduleEvent(EVENT_THROW_DYNAMITE, urand(5000, 7000));
                    break;
                default:
                    break;
            }
        }

        DoMeleeAttackIfReady();
    }
};

enum questfixes
{
    QUEST_NOBODY_TO_BLAME = 27489,
	QUEST_GIDWINS_FATE = 27526,
	QUEST_FUSELIGHT_HO = 27762,
};

class nobody_to_blamefix : public CreatureScript
{
public:
    nobody_to_blamefix() : CreatureScript("npc_nobody_to_blamefix") {}

    bool OnQuestAccept(Player* player, Creature* creature, Quest const* quest) override
    {
        if (!quest)
            return false;

        // Only handle the specific quest we're fixing
        if (quest->GetQuestId() != QUEST_NOBODY_TO_BLAME)
            return false;

        // Only complete if player currently has the quest (incomplete)
        if (player->GetQuestStatus(QUEST_NOBODY_TO_BLAME) == QUEST_STATUS_INCOMPLETE)
            player->CompleteQuest(QUEST_NOBODY_TO_BLAME, creature);

        // Teleport only for this quest accept
        player->TeleportTo(0, 3147.56f, -4332.05f, 132.25f, 5.297140f); // teleport to northpass
        return true;
    }
};


class gidwin_fix : public CreatureScript
{
public:
    gidwin_fix() : CreatureScript("npc_gidwin_fix") {}

    bool OnQuestAccept(Player* player, Creature* creature, Quest const* quest) override
    {
        if (!quest)
            return false;

        // Only handle the specific quest we're fixing
        if (quest->GetQuestId() != QUEST_GIDWINS_FATE)
            return false;

        // Only complete if player currently has the quest (incomplete)
        if (player->GetQuestStatus(QUEST_GIDWINS_FATE) == QUEST_STATUS_INCOMPLETE)
            player->CompleteQuest(QUEST_GIDWINS_FATE, creature);

      
        return true;
    }
};

class fuselight_ho_fix : public CreatureScript
{
public:
    fuselight_ho_fix() : CreatureScript("npc_fuselight_ho_fix") {}

    bool OnQuestAccept(Player* player, Creature* creature, Quest const* quest) override
    {
        if (!quest)
            return false;

        // Only handle the specific quest we're fixing
        if (quest->GetQuestId() != QUEST_FUSELIGHT_HO)
            return false;

        // Only complete if player currently has the quest (incomplete)
        if (player->GetQuestStatus(QUEST_FUSELIGHT_HO) == QUEST_STATUS_INCOMPLETE)
            player->CompleteQuest(QUEST_FUSELIGHT_HO, creature);

        // Teleport only for this quest accept
        player->TeleportTo(0, -6651.76f, -4760.20f, 7.56f, 1.719540f); // teleport to fuselight at the sea
        return true;
    }
};

void AddSC_eastern_plaguelands()
{
    new npc_ghoul_flayer();
	new gidwin_fix();
	new fuselight_ho_fix();
	new nobody_to_blamefix();
    new npc_augustus_the_touched();
    new npc_darrowshire_spirit();
    new npc_tirion_fordring();
    new creature_script<npc_scourge_siege_engineer>("npc_scourge_siege_engineer");
}
