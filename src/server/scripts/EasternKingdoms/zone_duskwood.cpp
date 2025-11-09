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
SDName: Duskwood
SD%Complete: 100
SDComment: Quest Support:8735
SDCategory: Duskwood
EndScriptData */

enum DuskWoodQuests
{
    QUEST_THE_EMBALMERS_REVENGE = 26727
   
};

class the_embalmers_revenge : public CreatureScript
{
public:
    the_embalmers_revenge(const char* ScriptName) : CreatureScript(ScriptName) {}




    bool OnQuestAccept(Player* player, Creature* creature, Quest const* quest)
    {
        if (quest->GetQuestId() == QUEST_THE_EMBALMERS_REVENGE)
        {


            player->SetPhaseMask(2, true);

        }
        return true;
    }

    bool OnQuestReward(Player* player, Creature* creature, Quest const* quest, uint32 /*opt*/)
    {
        if (quest->GetQuestId() == QUEST_THE_EMBALMERS_REVENGE)

            player->SetPhaseMask(1, true);



        return true;


    }

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new the_embalmers_revengeAI(creature);
    }
    struct the_embalmers_revengeAI : public ScriptedAI
    {
        the_embalmers_revengeAI(Creature* creature) : ScriptedAI(creature)
        {
        }

        void UpdateAI(uint32 diff) override
        {
            if (!UpdateVictim())
                return;
        }
    };
};


void AddSC_duskwood()
{
    new the_embalmers_revenge("the_embalmers_revenge");
}
