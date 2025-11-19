
#include "ScriptMgr.h"
#include "Player.h"
#include "Chat.h"
#include "Item.h"
#include <algorithm>
#include <cctype>
#include <string>

class DeleteGearScript : public PlayerScript
{
public:
    DeleteGearScript() : PlayerScript("delete_gear_script") {}

    void OnChat(Player* player, uint32 type, uint32 /*lang*/, std::string& msg) override
    {
        if (type != CHAT_MSG_SAY || !player)
            return;

        std::string lower = msg;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });

        if (lower == "delete player gear")
        {
            bool deletedAny = false;

            for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
            {
                if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                {
                    // Use the item's stored bag/slot for safe destruction
                    uint8 bag = item->GetBagSlot();
                    uint8 slotpos = item->GetSlot();

                    // Destroy the item and remove it from the player's inventory/equipment
                    player->DestroyItem(bag, slotpos, true);
                    deletedAny = true;
                }
            }

            // Reapply bonuses after removing items
            player->ReapplyItemsBonuses();

            if (deletedAny)
                player->SendPersonalMessage("All equipped gear removed and deleted.", CHAT_MSG_SAY, LANG_UNIVERSAL);
            else
                player->SendPersonalMessage("No equipped gear found to delete.", CHAT_MSG_SAY, LANG_UNIVERSAL);
        }
    }
};

void AddSC_delete_gear()
{
    new DeleteGearScript();
}