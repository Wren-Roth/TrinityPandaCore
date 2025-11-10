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
SDName: Stranglethorn_Vale
SD%Complete: 100
SDComment: Quest support: 592
SDCategory: Stranglethorn Vale
EndScriptData */

/* ContentData
npc_yenniku
EndContentData */

#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "Player.h"
#include "SpellInfo.h"

enum Spells
{
    SPELL_YENNIKUS_RELEASE         = 3607,
    SPELL_GEN_QUEST_INVISIBILITY_1 = 49414,
};

/*######
## npc_yenniku
######*/

class npc_yenniku : public CreatureScript
{
public:
    npc_yenniku() : CreatureScript("npc_yenniku") { }

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_yennikuAI(creature);
    }

    struct npc_yennikuAI : public ScriptedAI
    {
        npc_yennikuAI(Creature* creature) : ScriptedAI(creature)
        {
            bReset = false;
        }

        uint32 Reset_Timer;
        bool bReset;

        void Reset() override
        {
            Reset_Timer = 0;
            me->HandleEmoteStateCommand(EMOTE_STATE_NONE);
        }

        void SpellHit(Unit* caster, const SpellInfo* spell) override
        {
            if (bReset || spell->Id != SPELL_YENNIKUS_RELEASE)
                return;

            if (Player* player = caster->ToPlayer())
            {
                if (player->GetQuestStatus(592) == QUEST_STATUS_INCOMPLETE) //Yenniku's Release
                {
                    me->HandleEmoteStateCommand(EMOTE_STATE_STUN);
                    me->CombatStop();                   //stop combat
                    me->DeleteThreatList();             //unsure of this
                    me->setFaction(83);                 //horde generic

                    bReset = true;
                    Reset_Timer = 60000;
                }
            }
        }

        void EnterCombat(Unit* /*who*/) override { }

        void UpdateAI(uint32 diff) override
        {
            if (bReset)
            {
                if (Reset_Timer <= diff)
                {
                    EnterEvadeMode();
                    bReset = false;
                    me->setFaction(28);                     //troll, bloodscalp
                    return;
                }

                Reset_Timer -= diff;

                if (me->IsInCombat() && me->GetVictim())
                {
                    if (Player* player = me->GetVictim()->ToPlayer())
                    {
                        if (player->GetTeam() == HORDE)
                        {
                            me->CombatStop();
                            me->DeleteThreatList();
                        }
                    }
                }
             }

            //Return since we have no target
            if (!UpdateVictim())
                return;

            DoMeleeAttackIfReady();
        }
    };
};

/*######
##
######*/

// Emerine 43885
struct npc_stranglethorn_emerine : public ScriptedAI
{
    npc_stranglethorn_emerine(Creature* creature) : ScriptedAI(creature) { }

    TaskScheduler scheduler;
    uint64 summonerGUID;

    void IsSummonedBy(Unit* summoner) override
    {
        summonerGUID = summoner->GetGUID();
    }

    void Reset() override
    {
        if (me->GetDBTableGUIDLow())
            return;

        scheduler
            .Schedule(Milliseconds(500), [this](TaskContext context)
        {
            me->RemoveAurasDueToSpell(SPELL_GEN_QUEST_INVISIBILITY_1); // not phasing quest yet
        });

        scheduler
            .Schedule(Seconds(10), [this](TaskContext context)
        {
            // Check if summoner leave us (default remove 2m)
            if (Player* owner = ObjectAccessor::GetPlayer(*me, summonerGUID))
                if (me->GetExactDist2d(owner) > 60.0f)
                    me->DespawnOrUnsummon();

            context.Repeat(Seconds(3));
        });
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);
    }
};

enum priestessTypes
{
    QUEST_SEE_RAPTOR_A = 26773,
    QUEST_SEE_RAPTOR_H = 26359,
    QUEST_BE_RAPTOR_A = 26775,
    QUEST_BE_RAPTOR_H = 26362,
    NPC_CAPTURED_LASHTAIL_HATCHLING = 42840,
};

// Priestess Hu`rala 42812, Priestess Thaalia 44017
class npc_stranglethorn_priestess_hurala : public CreatureScript
{
    public:
        npc_stranglethorn_priestess_hurala() : CreatureScript("npc_stranglethorn_priestess_hurala") { }

        bool OnGossipHello(Player* player, Creature* creature)
        {
            if (creature->IsQuestGiver())
                player->PrepareQuestMenu(creature->GetGUID());

            if (player->GetQuestStatus(QUEST_SEE_RAPTOR_A) == QUEST_STATUS_INCOMPLETE || player->GetQuestStatus(QUEST_SEE_RAPTOR_H) == QUEST_STATUS_INCOMPLETE)
                player->ADD_GOSSIP_ITEM_DB(player->GetDefaultGossipMenuForSource(creature), 0, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);

            player->SEND_GOSSIP_MENU(player->GetGossipTextId(creature), creature->GetGUID());
            return true;
        }

        bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
        {
            player->PlayerTalkClass->ClearMenus();

            if (action == GOSSIP_ACTION_INFO_DEF + 1) // @Todo:: Event with teleport into Zul`Gurub
                player->KilledMonsterCredit(NPC_CAPTURED_LASHTAIL_HATCHLING);

            player->CLOSE_GOSSIP_MENU();
            return true;
        }

        bool OnQuestAccept(Player* player, Creature* creature, Quest const* quest)
        {
            if (player->GetQuestStatus(QUEST_BE_RAPTOR_A))

            {


                player->CompleteQuest(26775, creature);

            }
            return true;

            if (player->GetQuestStatus(QUEST_BE_RAPTOR_H))

            {


                player->CompleteQuest(26362, creature);

            }
            return true;
        }
};


enum piratestuff
{
    QUEST_THE_DAMSELS_BAD_LUCK = 26700,
    QUEST_TURNING_THE_BRASHTIDE = 26699,
    QUEST_DRIVE_BY_PIRACY = 26649,
	QUEST_SINKING_FROM_WITHIN = 26663,
    QUEST_THE_FINAL_VOYAGE = 26697
};


class drive_by_piracy : public CreatureScript
{
public:
    drive_by_piracy() : CreatureScript("quest_drive_by_piracy") {}




    bool OnQuestAccept(Player* player, Creature* creature, Quest const* quest)
    {
        if (player->GetQuestStatus(QUEST_DRIVE_BY_PIRACY))

        {


            player->CompleteQuest(26649, creature);

        }
		return true;
    }


    CreatureAI* GetAI(Creature* creature) const override
    {
        return new drive_by_piracyAI(creature);
    }
    struct drive_by_piracyAI : public ScriptedAI
    {
        drive_by_piracyAI(Creature* creature) : ScriptedAI(creature)
        {
        }

        void UpdateAI(uint32 diff) override
        {
            if (!UpdateVictim())
                return;
        }
    };
};


class final_voyage : public CreatureScript
{
public:
    final_voyage() : CreatureScript("final_voyage") {}




    bool OnQuestAccept(Player* player, Creature* creature, Quest const* quest)
    {
        if (player->GetQuestStatus(QUEST_THE_FINAL_VOYAGE))

        {


            player->CompleteQuest(26697, creature);

        }
        return true;
    }


    CreatureAI* GetAI(Creature* creature) const override
    {
        return new final_voyageAI(creature);
    }
    struct final_voyageAI : public ScriptedAI
    {
        final_voyageAI(Creature* creature) : ScriptedAI(creature)
        {
        }

        void UpdateAI(uint32 diff) override
        {
            if (!UpdateVictim())
                return;
        }
    };
};

void AddSC_stranglethorn_vale()
{
    new npc_yenniku();
    new creature_script<npc_stranglethorn_emerine>("npc_stranglethorn_emerine");
    new npc_stranglethorn_priestess_hurala();
	new final_voyage();
    new drive_by_piracy();
 
}
