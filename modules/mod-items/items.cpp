#include "ScriptMgr.h"
#include "Player.h"
#include "Chat.h"

class mod_items : public PlayerScript
{
public:
    mod_items() : PlayerScript("items_gearup") {}

    void OnChat(Player* player, uint32 type, uint32 lang, std::string& msg) override
    {
        if (type != CHAT_MSG_SAY)
            return;

        if (msg == "gearup" || msg == "GEARUP" || msg == "GearUp")
        {

            player->ModifyMoney(200000);

            switch (player->getClass())
            {
            case CLASS_MAGE:
                player->AddItem(23162, 4);
                player->AddItem(50255, 1);
                player->AddItem(61958, 1);
                player->AddItem(93859, 1);
                player->AddItem(93860, 1);
                player->AddItem(62029, 1);
                player->AddItem(62040, 1);
                player->AddItem(93897, 2);
                player->AddItem(93854, 1);
                player->SendPersonalMessage("Mage gear added!", CHAT_MSG_SAY, LANG_UNIVERSAL);
                break;
            case CLASS_PALADIN:
                player->AddItem(23162, 4);
                player->AddItem(50255, 1);
                player->AddItem(61931, 1);
                player->AddItem(93890, 1);
                player->AddItem(93891, 1);
                player->AddItem(62023, 1);
                player->AddItem(62038, 1);
                player->AddItem(93896, 2);
                player->AddItem(93843, 1);
                player->SendPersonalMessage("Paladin gear added!", CHAT_MSG_SAY, LANG_UNIVERSAL);
                break;
            case CLASS_WARRIOR:
                player->AddItem(23162, 4);
                player->AddItem(50255, 1);
                player->AddItem(61931, 1);
                player->AddItem(93890, 1);
                player->AddItem(93891, 1);
                player->AddItem(62023, 1);
                player->AddItem(62038, 1);
                player->AddItem(93896, 2);
                player->AddItem(93843, 1);
                player->SendPersonalMessage("Warrior gear added!", CHAT_MSG_SAY, LANG_UNIVERSAL);
                break;
            case CLASS_ROGUE:
                player->AddItem(23162, 4);
                player->AddItem(50255, 1);
                player->AddItem(61937, 1);
                player->AddItem(93862, 1);
                player->AddItem(93863, 1);
                player->AddItem(62026, 1);
                player->AddItem(62039, 1);
                player->AddItem(93896, 2);
                player->AddItem(93857, 2);
                player->SendPersonalMessage("Rogue gear added!", CHAT_MSG_SAY, LANG_UNIVERSAL);
                break;
            case CLASS_DRUID:
                player->AddItem(23162, 4);
                player->AddItem(50255, 1);
                player->AddItem(61942, 1);
                player->AddItem(93864, 1);
                player->AddItem(93865, 1);
                player->AddItem(62027, 1);
                player->AddItem(62040, 1);
                player->AddItem(93897, 2);
                player->AddItem(93854, 1);
                player->SendPersonalMessage("Druid gear added!", CHAT_MSG_SAY, LANG_UNIVERSAL);
                break;
            case CLASS_PRIEST:
                player->AddItem(23162, 4);
                player->AddItem(50255, 1);
                player->AddItem(61958, 1);
                player->AddItem(93859, 1);
                player->AddItem(93860, 1);
                player->AddItem(62029, 1);
                player->AddItem(62040, 1);
                player->AddItem(93897, 2);
                player->AddItem(93854, 1);
                player->SendPersonalMessage("Priest gear added!", CHAT_MSG_SAY, LANG_UNIVERSAL);
                break;
            case CLASS_HUNTER:
                player->AddItem(23162, 4);
                player->AddItem(50255, 1);
                player->AddItem(61935, 1);
                player->AddItem(93887, 1);
                player->AddItem(93888, 1);
                player->AddItem(62024, 1);
                player->AddItem(62039, 1);
                player->AddItem(93896, 2);
                player->AddItem(93855, 1);
                player->SendPersonalMessage("Hunter gear added!", CHAT_MSG_SAY, LANG_UNIVERSAL);
                break;
            case CLASS_SHAMAN:
                player->AddItem(23162, 4);
                player->AddItem(50255, 1);
                player->AddItem(61936, 1);
                player->AddItem(93876, 1);
                player->AddItem(93885, 1);
                player->AddItem(62025, 1);
                player->AddItem(62040, 1);
                player->AddItem(93897, 2);
                player->AddItem(93853, 1);
                player->AddItem(93903, 1);
                player->SendPersonalMessage("Shaman gear added!", CHAT_MSG_SAY, LANG_UNIVERSAL);
                break;
            case CLASS_WARLOCK:
                player->AddItem(23162, 4);
                player->AddItem(50255, 1);
                player->AddItem(61958, 1);
                player->AddItem(93859, 1);
                player->AddItem(93860, 1);
                player->AddItem(62029, 1);
                player->AddItem(62040, 1);
                player->AddItem(93897, 2);
                player->AddItem(93854, 1);
                player->SendPersonalMessage("Warlock gear added!", CHAT_MSG_SAY, LANG_UNIVERSAL);
                break;
            case CLASS_MONK:
                player->AddItem(23162, 4);
                player->AddItem(50255, 1);
                player->AddItem(61937, 1);
                player->AddItem(93862, 1);
                player->AddItem(93863, 1);
                player->AddItem(62026, 1);
                player->AddItem(62039, 1);
                player->AddItem(93896, 2);
                player->AddItem(93844, 1);
                player->SendPersonalMessage("Monk gear added!", CHAT_MSG_SAY, LANG_UNIVERSAL);
                break;
            case CLASS_DEATH_KNIGHT:
                player->AddItem(23162, 4);
                player->AddItem(50255, 1);
                player->AddItem(61931, 1);
                player->AddItem(93890, 1);
                player->AddItem(93891, 1);
                player->AddItem(62023, 1);
                player->AddItem(62038, 1);
                player->AddItem(93896, 2);
                player->AddItem(93843, 1);
                player->SendPersonalMessage("DK gear added!", CHAT_MSG_SAY, LANG_UNIVERSAL);
                break;
            default:
                player->SendPersonalMessage("No gear available for your class.", CHAT_MSG_SAY, LANG_UNIVERSAL);
                break;
            }
        }
    }
};

void AddSC_mod_items()
{
    new mod_items();
}