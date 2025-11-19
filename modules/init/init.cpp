

#include "Player.h"
#include <bitset>
#include <map>
#include <tuple>
#ifndef _PLAYERBOT_RANDOMITEMMGR_H
#define _PLAYERBOT_RANDOMITEMMGR_H
#include <set>
#include <unordered_set>
#include <vector>
#include "Common.h"
#include "Item.h"
#include "ScriptMgr.h"
#include "Chat.h"
#include <algorithm>
#include <cctype>


#ifndef _PLAYERBOT_PLAYERBOTSPEC_H
#define _PLAYERBOT_PLAYERBOTSPEC_H

using uint32 = unsigned int;

class Player;


#endif
enum item_sub_class : uint32
{
    PLATE = 0,
    MAIL,
    LEATHER,
    CLOTH
};

class RandomItemManager
{
public:
    RandomItemManager();
    void Init();
    virtual ~RandomItemManager();
    static RandomItemManager* instance()
    {
        static RandomItemManager instance;
        return &instance;
    }

    uint32 FindBestItemForLevelAndEquip(Player* bot, InventoryType invType);
    bool IsTestItem(uint32 itemId) { return _itemForTest.find(itemId) != _itemForTest.end(); }

    std::vector<uint32> GetCachedEquipments(uint32 requiredLevel, uint32 inventoryType);

    bool CanEquipArmor(Player* bot, ItemTemplate const* proto);
    bool CanEquipWeapon(Player* bot, ItemTemplate const* proto);
    bool CanEquipSubArmor(Player* bot, ItemTemplate const* proto);
private:
    void AddItemStats(uint32 mod, uint8& sp, uint8& ap, uint8& tank);
    bool CanEquipItem(EquipmentSlots slot, ItemTemplate const* proto);

    bool CheckItemStats(Classes clazz, uint8 sp, uint8 ap, uint8 tank);
    bool ShouldEquipArmorForSpec(uint32 level, Classes playerclass, Specializations spec, ItemTemplate const* proto);
    bool ShouldEquipWeaponForSpec(Classes playerclass, Specializations spec, ItemTemplate const* proto);
private:
    std::map<uint32, std::map<uint32, std::vector<uint32>>> _equipCacheNew;
    std::map<EquipmentSlots, std::set<InventoryType>> _viable_slots;
    std::unordered_set<uint32> _itemForTest;
};

#define sRandomItemMgr RandomItemManager::instance()

#endif

RandomItemManager::RandomItemManager()
{

}
RandomItemManager::~RandomItemManager()
{

}

// Cheat class copy to hack into the loot system
class LootTemplateAccess
{
public:
    class LootGroup;  // A set of loot definitions for items (refs are not allowed inside)
    typedef std::vector<LootGroup> LootGroups;
    LootStoreItemList Entries;  // not grouped only
    LootGroups Groups;          // groups have own (optimized) processing, grouped entries go there
};

void EquipRandomBestGear(Player* player)
{
    if (!player) return;

    // mapping equipment slot -> preferred inventory types to try (order matters)
    static const std::vector<std::pair<EquipmentSlots, std::vector<InventoryType>>> slotInvMap =
    {
        {EQUIPMENT_SLOT_HEAD,      {INVTYPE_HEAD}},
        {EQUIPMENT_SLOT_NECK,      {INVTYPE_NECK}},
        {EQUIPMENT_SLOT_SHOULDERS, {INVTYPE_SHOULDERS}},
        {EQUIPMENT_SLOT_BODY,      {INVTYPE_BODY}},
        {EQUIPMENT_SLOT_CHEST,     {INVTYPE_CHEST, INVTYPE_ROBE}},
        {EQUIPMENT_SLOT_WAIST,     {INVTYPE_WAIST}},
        {EQUIPMENT_SLOT_LEGS,      {INVTYPE_LEGS}},
        {EQUIPMENT_SLOT_FEET,      {INVTYPE_FEET}},
        {EQUIPMENT_SLOT_WRISTS,    {INVTYPE_WRISTS}},
        {EQUIPMENT_SLOT_HANDS,     {INVTYPE_HANDS}},
        {EQUIPMENT_SLOT_FINGER1,   {INVTYPE_FINGER}},
        {EQUIPMENT_SLOT_FINGER2,   {INVTYPE_FINGER}},
        {EQUIPMENT_SLOT_TRINKET1,  {INVTYPE_TRINKET}},
        {EQUIPMENT_SLOT_TRINKET2,  {INVTYPE_TRINKET}},
        {EQUIPMENT_SLOT_BACK,      {INVTYPE_CLOAK}},
        {EQUIPMENT_SLOT_TABARD,    {INVTYPE_TABARD}},
        {EQUIPMENT_SLOT_MAINHAND,  {INVTYPE_WEAPON, INVTYPE_2HWEAPON, INVTYPE_WEAPONMAINHAND, INVTYPE_RANGED}},
        {EQUIPMENT_SLOT_OFFHAND,   {INVTYPE_WEAPON, INVTYPE_2HWEAPON, INVTYPE_SHIELD, INVTYPE_WEAPONMAINHAND, INVTYPE_HOLDABLE}},
        // Ranged slot (if present in codebase) - some cores use EQUIPMENT_SLOT_RANGED
#if defined(EQUIPMENT_SLOT_RANGED)
        {EQUIPMENT_SLOT_RANGED,    {INVTYPE_RANGED, INVTYPE_THROWN, INVTYPE_RANGEDRIGHT}},
#endif
    };

    for (auto const& pr : slotInvMap)
    {
        EquipmentSlots slot = pr.first;
        bool equipped = false;

        for (InventoryType invType : pr.second)
        {
            // pick a candidate item id from the RandomItemManager caches
            uint32 itemId = sRandomItemMgr->FindBestItemForLevelAndEquip(player, invType);
            if (!itemId) continue;

            // ask server if this player can equip this item in this slot
            uint16 destPos = 0;
            InventoryResult canEquip = player->CanEquipNewItem(slot, destPos, itemId, false);
            if (canEquip != EQUIP_ERR_OK)
            {
                // can't equip this item in this slot (bad slot, two-handed conflict, class/race, etc.)
                continue;
            }

            // Equip: EquipNewItem will create the Item and equip it (will handle bag/slot placement)
            Item* newItem = player->EquipNewItem(destPos, itemId, true);
            if (newItem)
            {
                // If item has random properties, preserve that by generating property id
                // (some cores do that inside EquipNewItem - adjust/remove if duplicate)
               
                    int32 randProp = Item::GenerateItemRandomPropertyId(itemId);
                if (randProp != 0)
                    newItem->SetItemRandomProperties(randProp);

                // Reapply player's item bonuses after equipping
                player->ReapplyItemsBonuses();

                equipped = true;
                break; // slot filled, move to next equipment slot
            }
        }
        // continue to next slot whether equipped or not
    }
}

std::vector<uint32> RandomItemManager::GetCachedEquipments(uint32 requiredLevel, uint32 inventoryType)
{
    return _equipCacheNew[requiredLevel][inventoryType];
}

void RandomItemManager::Init()
{
    _viable_slots[EQUIPMENT_SLOT_HEAD].insert(INVTYPE_HEAD);
    _viable_slots[EQUIPMENT_SLOT_NECK].insert(INVTYPE_NECK);
    _viable_slots[EQUIPMENT_SLOT_SHOULDERS].insert(INVTYPE_SHOULDERS);
    _viable_slots[EQUIPMENT_SLOT_BODY].insert(INVTYPE_BODY);
    _viable_slots[EQUIPMENT_SLOT_CHEST].insert(INVTYPE_CHEST);
    _viable_slots[EQUIPMENT_SLOT_CHEST].insert(INVTYPE_ROBE);
    _viable_slots[EQUIPMENT_SLOT_WAIST].insert(INVTYPE_WAIST);
    _viable_slots[EQUIPMENT_SLOT_LEGS].insert(INVTYPE_LEGS);
    _viable_slots[EQUIPMENT_SLOT_FEET].insert(INVTYPE_FEET);
    _viable_slots[EQUIPMENT_SLOT_WRISTS].insert(INVTYPE_WRISTS);
    _viable_slots[EQUIPMENT_SLOT_HANDS].insert(INVTYPE_HANDS);
    _viable_slots[EQUIPMENT_SLOT_FINGER1].insert(INVTYPE_FINGER);
    _viable_slots[EQUIPMENT_SLOT_FINGER2].insert(INVTYPE_FINGER);
    _viable_slots[EQUIPMENT_SLOT_TRINKET1].insert(INVTYPE_TRINKET);
    _viable_slots[EQUIPMENT_SLOT_TRINKET2].insert(INVTYPE_TRINKET);
    _viable_slots[EQUIPMENT_SLOT_MAINHAND].insert(INVTYPE_WEAPON);
    _viable_slots[EQUIPMENT_SLOT_MAINHAND].insert(INVTYPE_2HWEAPON);
    _viable_slots[EQUIPMENT_SLOT_MAINHAND].insert(INVTYPE_WEAPONMAINHAND);
    _viable_slots[EQUIPMENT_SLOT_MAINHAND].insert(INVTYPE_RANGED);
    _viable_slots[EQUIPMENT_SLOT_OFFHAND].insert(INVTYPE_WEAPON);
    _viable_slots[EQUIPMENT_SLOT_OFFHAND].insert(INVTYPE_2HWEAPON);
    _viable_slots[EQUIPMENT_SLOT_OFFHAND].insert(INVTYPE_SHIELD);
    _viable_slots[EQUIPMENT_SLOT_OFFHAND].insert(INVTYPE_WEAPONMAINHAND);
    _viable_slots[EQUIPMENT_SLOT_OFFHAND].insert(INVTYPE_HOLDABLE);
    _viable_slots[EQUIPMENT_SLOT_TABARD].insert(INVTYPE_TABARD);
    _viable_slots[EQUIPMENT_SLOT_BACK].insert(INVTYPE_CLOAK);

    TC_LOG_INFO("playerbots", "Loading equipments cache...");


    ItemTemplateContainer const* itemTemplates = sObjectMgr->GetItemTemplateStore();
    for (auto const& itr : *itemTemplates)
    {
        ItemTemplate const* proto = &itr.second;
        if (!proto)
            continue;
        uint32 itemId = proto->ItemId;

      

        // skip test items
        if (strstr(proto->Name1.c_str(), "(Test)") || strstr(proto->Name1.c_str(), "(TEST)") ||
            strstr(proto->Name1.c_str(), "(test)") || strstr(proto->Name1.c_str(), "(JEFFTEST)") ||
            strstr(proto->Name1.c_str(), "Test ") || strstr(proto->Name1.c_str(), "Test") ||
            strstr(proto->Name1.c_str(), "TEST") || strstr(proto->Name1.c_str(), "TEST ") ||
            strstr(proto->Name1.c_str(), " TEST") || strstr(proto->Name1.c_str(), "2200 ") ||
            strstr(proto->Name1.c_str(), "Deprecated ") || strstr(proto->Name1.c_str(), "Unused ") ||
            strstr(proto->Name1.c_str(), "Monster ") || strstr(proto->Name1.c_str(), "[PH]") ||
            strstr(proto->Name1.c_str(), "(OLD)") || strstr(proto->Name1.c_str(), "QR") ||
            strstr(proto->Name1.c_str(), "zzOLD"))
        {
            _itemForTest.insert(proto->ItemId);
            continue;
        }

        if (proto->Flags & ITEM_PROTO_FLAG_DEPRECATED)
        {
            _itemForTest.insert(proto->ItemId);
            continue;
        }

        if (itemId == 22784)
        {  // Sunwell Orb
            continue;
        }
        _equipCacheNew[proto->RequiredLevel][proto->InventoryType].push_back(itemId);
    }
}

void RandomItemManager::AddItemStats(uint32 mod, uint8& sp, uint8& ap, uint8& tank)
{
    switch (mod)
    {
    case ITEM_MOD_HEALTH:
    case ITEM_MOD_STAMINA:
    case ITEM_MOD_MANA:
    case ITEM_MOD_INTELLECT:
    case ITEM_MOD_SPIRIT:
        ++sp;
        break;
    }

    switch (mod)
    {
    case ITEM_MOD_AGILITY:
    case ITEM_MOD_STRENGTH:
    case ITEM_MOD_HEALTH:
    case ITEM_MOD_STAMINA:
        ++tank;
        break;
    }

    switch (mod)
    {
    case ITEM_MOD_HEALTH:
    case ITEM_MOD_STAMINA:
    case ITEM_MOD_AGILITY:
    case ITEM_MOD_STRENGTH:
        ++ap;
        break;
    }
}

bool RandomItemManager::CheckItemStats(Classes clazz, uint8 sp, uint8 ap, uint8 tank)
{
    switch (clazz)
    {
    case CLASS_PRIEST:
    case CLASS_MAGE:
    case CLASS_WARLOCK:
        if (!sp || ap > sp || tank > sp)
            return false;
        break;
    case CLASS_PALADIN:
    case CLASS_WARRIOR:
        if ((!ap && !tank) || sp > ap || sp > tank)
            return false;
        break;
    case CLASS_HUNTER:
    case CLASS_ROGUE:
    case CLASS_MONK:
        if (!ap || sp > ap || sp > tank)
            return false;
        break;
    }

    return sp || ap || tank;
}

bool RandomItemManager::ShouldEquipArmorForSpec(uint32 level, Classes playerclass, Specializations spec, ItemTemplate const* proto)
{
    if (proto->InventoryType == INVTYPE_TABARD)
        return true;

    std::unordered_set<uint32> resultArmorSubClass;
    switch (playerclass)
    {
    case CLASS_WARRIOR:
    {
        if (proto->InventoryType == INVTYPE_HOLDABLE)
            return false;

        resultArmorSubClass = { ITEM_SUBCLASS_ARMOR_PLATE };
        if (spec != Specializations::SPEC_WARRIOR_PROTECTION)
            resultArmorSubClass = { ITEM_SUBCLASS_ARMOR_MAIL, ITEM_SUBCLASS_ARMOR_PLATE };
        if (level < 40)
            resultArmorSubClass = { ITEM_SUBCLASS_ARMOR_LEATHER, ITEM_SUBCLASS_ARMOR_MAIL, ITEM_SUBCLASS_ARMOR_PLATE };
        break;
    }
    case CLASS_DEATH_KNIGHT:
    {
        if (proto->InventoryType == INVTYPE_HOLDABLE)
            return false;

        resultArmorSubClass = { ITEM_SUBCLASS_ARMOR_PLATE };
        break;
    }
    case CLASS_PALADIN:
    {
        if (spec == Specializations::SPEC_PALADIN_HOLY && proto->InventoryType == INVTYPE_HOLDABLE)
            return false;

        resultArmorSubClass = { ITEM_SUBCLASS_ARMOR_PLATE };
        if (spec != Specializations::SPEC_PALADIN_PROTECTION)
            resultArmorSubClass = { ITEM_SUBCLASS_ARMOR_MAIL, ITEM_SUBCLASS_ARMOR_PLATE };
        if (level < 40)
            resultArmorSubClass = { ITEM_SUBCLASS_ARMOR_LEATHER, ITEM_SUBCLASS_ARMOR_MAIL, ITEM_SUBCLASS_ARMOR_PLATE };

        break;
    }
    case CLASS_HUNTER:
    {
        if (proto->InventoryType == INVTYPE_HOLDABLE)
            return false;

        resultArmorSubClass = { ITEM_SUBCLASS_ARMOR_LEATHER, ITEM_SUBCLASS_ARMOR_MAIL };
        break;
    }
    case CLASS_ROGUE:
    {
        if (proto->InventoryType == INVTYPE_HOLDABLE)
            return false;
        resultArmorSubClass = { ITEM_SUBCLASS_ARMOR_LEATHER };
        break;
    }
    case CLASS_SHAMAN:
    {
        if (spec == Specializations::SPEC_SHAMAN_ENHANCEMENT && proto->InventoryType == INVTYPE_HOLDABLE)
            return false;
        resultArmorSubClass = { ITEM_SUBCLASS_ARMOR_CLOTH, ITEM_SUBCLASS_ARMOR_LEATHER, ITEM_SUBCLASS_ARMOR_MAIL };
        if (spec == Specializations::SPEC_SHAMAN_ENHANCEMENT && level > 40)
            resultArmorSubClass = { ITEM_SUBCLASS_ARMOR_MAIL };

        break;
    }
    case CLASS_PRIEST:
    case CLASS_MAGE:
    case CLASS_WARLOCK:
    {
        resultArmorSubClass = { ITEM_SUBCLASS_ARMOR_CLOTH };
        break;
    }
    case CLASS_DRUID:
    {
        if ((spec == Specializations::SPEC_DRUID_FERAL || spec == Specializations::SPEC_DRUID_GUARDIAN) &&
            proto->InventoryType == INVTYPE_HOLDABLE)
            return false;
        resultArmorSubClass = { ITEM_SUBCLASS_ARMOR_CLOTH, ITEM_SUBCLASS_ARMOR_LEATHER };
        if (spec == Specializations::SPEC_DRUID_FERAL || spec == Specializations::SPEC_DRUID_GUARDIAN)
            resultArmorSubClass = { ITEM_SUBCLASS_ARMOR_LEATHER };

        break;
    }
    case CLASS_MONK:
    {
        if (proto->InventoryType == INVTYPE_HOLDABLE && spec != Specializations::SPEC_MONK_MISTWEAVER)
            return false;
        resultArmorSubClass = { ITEM_SUBCLASS_ARMOR_LEATHER };
        break;
    }
    }

    return (resultArmorSubClass.find(proto->SubClass) != resultArmorSubClass.end());
}

bool RandomItemManager::ShouldEquipWeaponForSpec(Classes playerclass, Specializations spec, ItemTemplate const* proto)
{
    EquipmentSlots slot_mh = EQUIPMENT_SLOT_START;
    EquipmentSlots slot_oh = EQUIPMENT_SLOT_START;
    for (auto i = _viable_slots.begin(); i != _viable_slots.end(); ++i)
    {
        std::set<InventoryType> slots = _viable_slots[(EquipmentSlots)i->first];
        if (slots.find((InventoryType)proto->InventoryType) != slots.end())
        {
            if (i->first == EQUIPMENT_SLOT_MAINHAND)
                slot_mh = i->first;
            if (i->first == EQUIPMENT_SLOT_OFFHAND)
                slot_oh = i->first;
        }
    }


    if (slot_mh == EQUIPMENT_SLOT_START && slot_oh == EQUIPMENT_SLOT_START)
        return false;

    std::unordered_set<uint32> mh_weapons;
    std::unordered_set<uint32> oh_weapons;

    switch (playerclass)
    {
    case CLASS_WARRIOR:
    {
        if (spec == Specializations::SPEC_WARRIOR_PROTECTION)
        {
            mh_weapons = { ITEM_SUBCLASS_WEAPON_SWORD, ITEM_SUBCLASS_WEAPON_AXE, ITEM_SUBCLASS_WEAPON_MACE,
                          ITEM_SUBCLASS_WEAPON_DAGGER, ITEM_SUBCLASS_WEAPON_FIST_WEAPON };
            oh_weapons = { ITEM_SUBCLASS_ARMOR_SHIELD };
        }
        else
        {
            mh_weapons = { ITEM_SUBCLASS_WEAPON_SWORD2, ITEM_SUBCLASS_WEAPON_AXE2, ITEM_SUBCLASS_WEAPON_MACE2,
                          ITEM_SUBCLASS_WEAPON_STAFF, ITEM_SUBCLASS_WEAPON_POLEARM };
        }
        break;
    }
    case CLASS_PALADIN:
    {
        if (spec == Specializations::SPEC_PALADIN_PROTECTION)
        {
            mh_weapons = { ITEM_SUBCLASS_WEAPON_SWORD, ITEM_SUBCLASS_WEAPON_AXE, ITEM_SUBCLASS_WEAPON_MACE };
            oh_weapons = { ITEM_SUBCLASS_ARMOR_SHIELD };
        }
        else if (spec == Specializations::SPEC_PALADIN_HOLY)
        {
            mh_weapons = { ITEM_SUBCLASS_WEAPON_SWORD, ITEM_SUBCLASS_WEAPON_AXE, ITEM_SUBCLASS_WEAPON_MACE };
            oh_weapons = { ITEM_SUBCLASS_ARMOR_SHIELD, ITEM_SUBCLASS_ARMOR_MISCELLANEOUS };
        }
        else
        {
            mh_weapons = { ITEM_SUBCLASS_WEAPON_SWORD2, ITEM_SUBCLASS_WEAPON_AXE2, ITEM_SUBCLASS_WEAPON_MACE2,
                          ITEM_SUBCLASS_WEAPON_POLEARM };
        }
        break;
    }
    case CLASS_DEATH_KNIGHT:
    {
        // Prefer two-handed weapon subclasses for Death Knights (Blood preference).
        mh_weapons = {
            ITEM_SUBCLASS_WEAPON_MACE2,
            ITEM_SUBCLASS_WEAPON_POLEARM,
            ITEM_SUBCLASS_WEAPON_SWORD2,
            ITEM_SUBCLASS_WEAPON_AXE2
        };
        break;
    }
    case CLASS_HUNTER:
    {
        mh_weapons = { ITEM_SUBCLASS_WEAPON_BOW, ITEM_SUBCLASS_WEAPON_CROSSBOW, ITEM_SUBCLASS_WEAPON_GUN };
        break;
    }
    case CLASS_ROGUE:
    {
        mh_weapons = { ITEM_SUBCLASS_WEAPON_DAGGER };
        oh_weapons = { ITEM_SUBCLASS_WEAPON_DAGGER };
        break;
    }
    case CLASS_PRIEST:
    {
        mh_weapons = { ITEM_SUBCLASS_WEAPON_STAFF, ITEM_SUBCLASS_WEAPON_DAGGER, ITEM_SUBCLASS_WEAPON_MACE };
        oh_weapons = { ITEM_SUBCLASS_ARMOR_MISCELLANEOUS };
        break;
    }
    case CLASS_SHAMAN:
    {
        if (spec == Specializations::SPEC_SHAMAN_RESTORATION)
        {
            mh_weapons = { ITEM_SUBCLASS_WEAPON_DAGGER, ITEM_SUBCLASS_WEAPON_AXE, ITEM_SUBCLASS_WEAPON_MACE,
                          ITEM_SUBCLASS_WEAPON_FIST_WEAPON };
            oh_weapons = { ITEM_SUBCLASS_ARMOR_MISCELLANEOUS, ITEM_SUBCLASS_ARMOR_SHIELD };
        }
        else if (spec == Specializations::SPEC_SHAMAN_ENHANCEMENT)
        {
            mh_weapons = { ITEM_SUBCLASS_WEAPON_MACE2, ITEM_SUBCLASS_WEAPON_AXE2, ITEM_SUBCLASS_WEAPON_DAGGER,
                          ITEM_SUBCLASS_WEAPON_AXE,   ITEM_SUBCLASS_WEAPON_MACE, ITEM_SUBCLASS_WEAPON_FIST_WEAPON };
            oh_weapons = { ITEM_SUBCLASS_ARMOR_SHIELD };
        }
        else
        {
            mh_weapons = { ITEM_SUBCLASS_WEAPON_STAFF };
        }
        break;
    }
    case CLASS_MAGE:
    case CLASS_WARLOCK:
    {
        mh_weapons = { ITEM_SUBCLASS_WEAPON_STAFF, ITEM_SUBCLASS_WEAPON_DAGGER, ITEM_SUBCLASS_WEAPON_SWORD };
        oh_weapons = { ITEM_SUBCLASS_ARMOR_MISCELLANEOUS };
        break;
    }
    case CLASS_DRUID:
    {
        if (spec == Specializations::SPEC_DRUID_GUARDIAN)
        {
            mh_weapons = { ITEM_SUBCLASS_WEAPON_STAFF, ITEM_SUBCLASS_WEAPON_MACE2 };
        }
        else if (spec == Specializations::SPEC_DRUID_RESTORATION)
        {
            mh_weapons = { ITEM_SUBCLASS_WEAPON_STAFF, ITEM_SUBCLASS_WEAPON_DAGGER, ITEM_SUBCLASS_WEAPON_FIST_WEAPON,
                          ITEM_SUBCLASS_WEAPON_MACE };
            oh_weapons = { ITEM_SUBCLASS_ARMOR_MISCELLANEOUS };
        }
        else if (spec == Specializations::SPEC_DRUID_FERAL)
        {
            mh_weapons = { ITEM_SUBCLASS_WEAPON_STAFF, ITEM_SUBCLASS_WEAPON_MACE2 };
        }
        else
        {
            mh_weapons = { ITEM_SUBCLASS_WEAPON_STAFF };
        }
        break;
    }
    case CLASS_MONK:
    {
        mh_weapons = { ITEM_SUBCLASS_WEAPON_STAFF, ITEM_SUBCLASS_WEAPON_AXE , ITEM_SUBCLASS_WEAPON_SWORD , ITEM_SUBCLASS_WEAPON_MACE , ITEM_SUBCLASS_WEAPON_FIST_WEAPON };
        oh_weapons = { ITEM_SUBCLASS_WEAPON_AXE , ITEM_SUBCLASS_WEAPON_SWORD , ITEM_SUBCLASS_WEAPON_MACE , ITEM_SUBCLASS_WEAPON_FIST_WEAPON };
        break;
    }
    }

    if (slot_mh == EQUIPMENT_SLOT_MAINHAND && mh_weapons.find(proto->SubClass) != mh_weapons.end())
        return true;

    if (slot_oh == EQUIPMENT_SLOT_OFFHAND && oh_weapons.find(proto->SubClass) != oh_weapons.end())
        return true;

    return false;
}

/*
* stat_comb
* 108 armor / agi / stam / dodge / parry
* 105 armor / stam / stren / dodge / parry
* 104 armor / stam / dodge / expert / parry
* 96 armor / dodge / parry
* 77 armor / stren / agi / stam / parry
* 76 armor / stam / agi / parry
* 73 armor / stren / stam / parry
* 72 armor / stam / parry
* 65 armor / stren / parry
* 64 armor / parry / fire resi / heal.s / dodge / parry
* 58 armor / stam / intell / spirit / dodge
* 56 armor / starm / spirit / dodge
* 48 armor / spirit / dodge
* 46 armor / stam / intell / agi / hit / dodge
* 45 armor / agi / stren / stam / dodge / frost res
* 44 armor / stam / agi / dodge
* 42 armor / stam / intel / dodge / crit
* 41 armor / stren / stam / dodge / mastery
* 40 armor / stam / dodge / exprt
* 36 armor / agi / crit / dodge
* 33 armor / stren / dodge / crit
* 32 armor / dodge / health
* 31 armor / stren / stam / intell / agi / spirit
* 28 armor / agi / stam / spirit / attack power
* 26 armor / intell / stam / spirit / crit
* 25 armor / stam / spirit / hit / pvp power / pvp resi / rage cost
* 24 armor / stam / spirit / spell power / haste
* 18 armor / spirit / intell / crit
* 16 armor / spirit / pvp power / pvp resi
* 14 armor / agi / intel / stam / hit
* 13 armor / stren / stam / agi / haste
* 12 armor / agi / stam / expert / crit / mastery
* 11 armor / stren / intell / stam
* 10 armor / intell / stam / haste
* 9 armor / stren / stam / haste / mastery
* 8 armor / stam / crit / pvp resi / attack power / haste
* 6 armor / intell / agi / haste
* 5 armor / stren / agi / crit
* 4 armor / agi / hit / haste
* 3 armor / intell / stren (staff unique)
* 2 armor / intell / haste /
* 1 armor / stren / mastery / crit / hit
* 0 any
*/

bool RandomItemManager::CanEquipItem(EquipmentSlots slot, ItemTemplate const* proto)
{
    if (proto->Duration & 0x80000000)
        return false;

    if (proto->Bonding == BIND_QUEST || proto->Bonding == BIND_ON_USE)
        return false;

    if (proto->Class == ITEM_CLASS_CONTAINER)
        return true;

    std::set<InventoryType> slots = _viable_slots[slot];
    if (slots.find((InventoryType)proto->InventoryType) == slots.end())
        return false;

    return true;
}

bool RandomItemManager::CanEquipSubArmor(Player* bot, ItemTemplate const* proto)
{
    Classes clazz = (Classes)bot->getClass();
    Specializations spec = bot->GetSpecialization();

    if (proto->InventoryType != InventoryType::INVTYPE_SHIELD && proto->InventoryType != InventoryType::INVTYPE_HOLDABLE)
        return true;

    switch (clazz)
    {
    case CLASS_HUNTER:
    case CLASS_ROGUE:
    case CLASS_DEATH_KNIGHT:
    {
        return false;
    }
    case CLASS_PRIEST:
    case CLASS_MAGE:
    case CLASS_WARLOCK:
    {
        if (proto->SubClass == ITEM_SUBCLASS_ARMOR_MISCELLANEOUS)
            return true;
        return false;
    }
    case CLASS_WARRIOR:
    {
        if (spec == Specializations::SPEC_WARRIOR_PROTECTION && proto->SubClass == ITEM_SUBCLASS_ARMOR_SHIELD)
            return true;
        return false;
    }
    case CLASS_PALADIN:
    {
        if (spec == Specializations::SPEC_PALADIN_PROTECTION && proto->SubClass == ITEM_SUBCLASS_ARMOR_SHIELD)
            return true;
        if (spec == Specializations::SPEC_PALADIN_HOLY && proto->SubClass == ITEM_SUBCLASS_ARMOR_MISCELLANEOUS)
            return true;
        return false;
    }
    case CLASS_SHAMAN:
    {
        if ((spec == Specializations::SPEC_SHAMAN_ELEMENTAL || spec == Specializations::SPEC_SHAMAN_RESTORATION)
            && (proto->SubClass == ITEM_SUBCLASS_ARMOR_SHIELD || proto->SubClass == ITEM_SUBCLASS_ARMOR_MISCELLANEOUS))
            return true;
        return false;
    }
    case CLASS_DRUID:
    {
        if ((spec == Specializations::SPEC_DRUID_BALANCE || spec == Specializations::SPEC_DRUID_FERAL)
            && proto->SubClass == ITEM_SUBCLASS_ARMOR_MISCELLANEOUS)
            return true;
        return false;
    }
    case CLASS_MONK:
    {
        if (spec == Specializations::SPEC_MONK_MISTWEAVER && proto->SubClass != ITEM_SUBCLASS_ARMOR_MISCELLANEOUS)
            return true;
        return false;
    }
    }

    return false;
}

bool RandomItemManager::CanEquipArmor(Player* bot, ItemTemplate const* proto)
{
    Classes clazz = (Classes)bot->getClass();
    uint32 level = (uint32)bot->getLevel();

    if (proto->InventoryType == INVTYPE_TABARD)
        return true;

    if (!ShouldEquipArmorForSpec(level, clazz, bot->GetSpecialization(), proto))
        return false;

    uint8 sp = 0, ap = 0, tank = 0;
    for (uint8 j = 0; j < MAX_ITEM_PROTO_STATS; ++j)
    {
        // for ItemStatValue != 0
        if (!proto->ItemStat[j].ItemStatValue)
            continue;

        AddItemStats(proto->ItemStat[j].ItemStatType, sp, ap, tank);
    }

    return CheckItemStats(clazz, sp, ap, tank);
}

bool RandomItemManager::CanEquipWeapon(Player* bot, ItemTemplate const* proto)
{
    Classes clazz = (Classes)bot->getClass();
    uint32 level = (uint32)bot->getLevel();

    switch (clazz)
    {
    case CLASS_PRIEST:
        if (proto->SubClass != ITEM_SUBCLASS_WEAPON_STAFF && proto->SubClass != ITEM_SUBCLASS_WEAPON_WAND &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE && proto->SubClass != ITEM_SUBCLASS_WEAPON_DAGGER)
            return false;
        break;
    case CLASS_MAGE:
    case CLASS_WARLOCK:
        if (proto->SubClass != ITEM_SUBCLASS_WEAPON_STAFF && proto->SubClass != ITEM_SUBCLASS_WEAPON_WAND &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_DAGGER && proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD)
            return false;
        break;
    case CLASS_WARRIOR:
        if (proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE2 && proto->SubClass != ITEM_SUBCLASS_WEAPON_AXE &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_POLEARM && proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD2 &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE && proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_AXE2 && proto->SubClass != ITEM_SUBCLASS_WEAPON_FIST_WEAPON &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_DAGGER && proto->SubClass != ITEM_SUBCLASS_WEAPON_STAFF)
            return false;
        break;
    case CLASS_PALADIN:
    case CLASS_DEATH_KNIGHT:
        if (proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE2 && proto->SubClass != ITEM_SUBCLASS_WEAPON_POLEARM &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD2 && proto->SubClass != ITEM_SUBCLASS_WEAPON_AXE2 &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_AXE && proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD)
            return false;
        break;
    case CLASS_SHAMAN:
        if (proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE && proto->SubClass != ITEM_SUBCLASS_WEAPON_AXE &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_FIST_WEAPON && proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE2 &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_AXE2 && proto->SubClass != ITEM_SUBCLASS_WEAPON_DAGGER &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_STAFF)
            return false;
        break;
    case CLASS_DRUID:
        if (proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE && proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE2 &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_DAGGER && proto->SubClass != ITEM_SUBCLASS_WEAPON_STAFF &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_POLEARM)
            return false;
        break;
    case CLASS_HUNTER:
        if (proto->SubClass != ITEM_SUBCLASS_WEAPON_BOW && proto->SubClass != ITEM_SUBCLASS_WEAPON_GUN &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_CROSSBOW)
            return false;
        break;
    case CLASS_ROGUE:
        if (proto->SubClass != ITEM_SUBCLASS_WEAPON_DAGGER && proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_FIST_WEAPON && proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_AXE)
            return false;
        break;
    case CLASS_MONK:
    {
        if (proto->SubClass != ITEM_SUBCLASS_WEAPON_AXE && proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_STAFF && proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_FIST_WEAPON)
            return false;
        break;
    }
    }

    if (!ShouldEquipWeaponForSpec((Classes)bot->getClass(), bot->GetSpecialization(), proto))
        return false;

    return true;
}

// Définition des seuils de qualité avec une valeur de repli
static const std::map<uint32, std::tuple<ItemQualities, ItemQualities, uint32, uint32>> qualityRules =
{
    {1,   std::make_tuple(ItemQualities::ITEM_QUALITY_NORMAL,   ItemQualities::ITEM_QUALITY_POOR,       20, 5)},
    {10,  std::make_tuple(ItemQualities::ITEM_QUALITY_UNCOMMON, ItemQualities::ITEM_QUALITY_NORMAL,     20, 5)},
    {35,  std::make_tuple(ItemQualities::ITEM_QUALITY_RARE,     ItemQualities::ITEM_QUALITY_UNCOMMON,   30, 10)},
    {60,  std::make_tuple(ItemQualities::ITEM_QUALITY_EPIC,     ItemQualities::ITEM_QUALITY_RARE,       40, 15)},
    {61,  std::make_tuple(ItemQualities::ITEM_QUALITY_UNCOMMON, ItemQualities::ITEM_QUALITY_NORMAL,     20, 5)},
    {65,  std::make_tuple(ItemQualities::ITEM_QUALITY_RARE,     ItemQualities::ITEM_QUALITY_UNCOMMON,   30, 10)},
    {70,  std::make_tuple(ItemQualities::ITEM_QUALITY_EPIC,     ItemQualities::ITEM_QUALITY_RARE,       40, 15)},
    {71,  std::make_tuple(ItemQualities::ITEM_QUALITY_UNCOMMON, ItemQualities::ITEM_QUALITY_NORMAL,     20, 5)},
    {75,  std::make_tuple(ItemQualities::ITEM_QUALITY_RARE,     ItemQualities::ITEM_QUALITY_UNCOMMON,   30, 10)},
    {80,  std::make_tuple(ItemQualities::ITEM_QUALITY_EPIC,     ItemQualities::ITEM_QUALITY_RARE,       40, 15)},
    {81,  std::make_tuple(ItemQualities::ITEM_QUALITY_UNCOMMON, ItemQualities::ITEM_QUALITY_NORMAL,     20, 5)},
    {83,  std::make_tuple(ItemQualities::ITEM_QUALITY_RARE,     ItemQualities::ITEM_QUALITY_UNCOMMON,   30, 10)},
    {85,  std::make_tuple(ItemQualities::ITEM_QUALITY_EPIC,     ItemQualities::ITEM_QUALITY_RARE,       40, 15)},
    {86,  std::make_tuple(ItemQualities::ITEM_QUALITY_UNCOMMON, ItemQualities::ITEM_QUALITY_NORMAL,     20, 5)},
    {88,  std::make_tuple(ItemQualities::ITEM_QUALITY_RARE,     ItemQualities::ITEM_QUALITY_UNCOMMON,   30, 10)},
    {90,  std::make_tuple(ItemQualities::ITEM_QUALITY_EPIC,     ItemQualities::ITEM_QUALITY_RARE,       40, 15)}
};

/*
* Know issues:
* Not always wear wearable (plate / mail etc)
* Hunt have incorrect weapon
* Random items stat not supposed to be assigned (ex shaman enhance with intellect spell power etc)
*/

uint32 RandomItemManager::FindBestItemForLevelAndEquip(Player* bot, InventoryType invType)
{
    uint32 level = (uint32)bot->getLevel();
    uint32 bot_class = bot->getClass();
    uint32 bot_race = bot->getRace();
    int32 delta = std::min(level, 10u);

    for (uint32 requiredLevel = bot->getLevel(); requiredLevel > std::max((int32)bot->getLevel() - delta, 0); requiredLevel--)
    {
        std::mt19937 gen(std::chrono::system_clock::now().time_since_epoch().count());
        auto& items = _equipCacheNew[requiredLevel][invType];
        std::shuffle(items.begin(), items.end(), gen);

        for (auto it = items.end(); it != items.begin(); )
        {
            --it;

            uint32 itemID = *it;

            if (IsTestItem(itemID)) continue;

            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemID);
            if (proto->RequiredLevel > level) continue;
            if ((proto->AllowableClass & bot->getClassMask()) == 0 || (proto->AllowableRace & bot->getRaceMask()) == 0) continue;
            if (level < 10 && proto->ItemLevel > 100) continue;

            // NEW: explicitly skip epic quality items
            if (proto->Quality == ItemQualities::ITEM_QUALITY_EPIC)
                continue;

            // Item Quality
            {
                auto quality_iterator = qualityRules.upper_bound(bot->getLevel());
                if (quality_iterator != qualityRules.begin()) --quality_iterator; // Récupérer la tranche correcte
                // Extraire les valeurs correspondantes
                ItemQualities minQuality = std::get<0>(quality_iterator->second);
                ItemQualities fallbackQuality = std::get<1>(quality_iterator->second); // Qualité inférieure en cas d'absence
                uint32 rareChance = std::get<2>(quality_iterator->second);
                uint32 uncommonChance = std::get<3>(quality_iterator->second);
                // Vérification de la qualité de l'objet
                if (proto->Quality >= minQuality) {
                    // Accepter immédiatement l'objet
                }
                else if (proto->Quality >= fallbackQuality) {
                    // Accepter un objet de qualité inférieure en dernier recours
                }
                else if (proto->Quality == ItemQualities::ITEM_QUALITY_RARE && urand(1, 100) <= rareChance) {
                    // Accepter avec un % pour Rare
                }
                else if (proto->Quality == ItemQualities::ITEM_QUALITY_UNCOMMON && urand(1, 100) <= uncommonChance) {
                    // Accepter avec un % pour Uncommon
                }
                else {
                    continue; // Sinon, ignorer l'objet
                }
            }

            if (proto->Class != ITEM_CLASS_WEAPON && proto->Class != ITEM_CLASS_ARMOR &&
                proto->Class != ITEM_CLASS_CONTAINER && proto->Class != ITEM_CLASS_PROJECTILE)
                continue;

            if (proto->Class == ITEM_CLASS_ARMOR &&
                (proto->InventoryType == InventoryType::INVTYPE_HEAD || proto->InventoryType == InventoryType::INVTYPE_SHOULDERS ||
                    proto->InventoryType == InventoryType::INVTYPE_CHEST || proto->InventoryType == InventoryType::INVTYPE_WAIST ||
                    proto->InventoryType == InventoryType::INVTYPE_LEGS || proto->InventoryType == InventoryType::INVTYPE_FEET ||
                    proto->InventoryType == InventoryType::INVTYPE_WRISTS || proto->InventoryType == InventoryType::INVTYPE_HANDS ||
                    proto->InventoryType == InventoryType::INVTYPE_ROBE) &&
                !CanEquipArmor(bot, proto))
                continue;

            else if (proto->Class == ITEM_CLASS_ARMOR &&
                (proto->SubClass == ItemSubclassArmor::ITEM_SUBCLASS_ARMOR_MISCELLANEOUS ||
                    proto->SubClass == ItemSubclassArmor::ITEM_SUBCLASS_ARMOR_SHIELD) &&
                !CanEquipSubArmor(bot, proto)) continue;

            else if ((proto->Class == ITEM_CLASS_WEAPON) &&
                !CanEquipWeapon(bot, proto))
                continue;

            else
            {
                uint8 sp = 0, ap = 0, tank = 0;
                for (uint8 j = 0; j < MAX_ITEM_PROTO_STATS; ++j)
                {
                    // for ItemStatValue != 0
                    if (!proto->ItemStat[j].ItemStatValue)
                        continue;

                    AddItemStats(proto->ItemStat[j].ItemStatType, sp, ap, tank);
                }

                if (!CheckItemStats((Classes)bot_class, sp, ap, tank)) continue;
            }

            // ok ?
            return proto->ItemId;
        }
    }
    return 0;
}

class InitGearScript : public PlayerScript
{
public:
    InitGearScript() : PlayerScript("init_gear_script") {}

    void OnChat(Player* player, uint32 type, uint32 lang, std::string& msg) override
    {
        if (type != CHAT_MSG_SAY || !player)
            return;

        std::string lower = msg;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });

        if (lower == "init gear")
        {
            EquipRandomBestGear(player);

            // confirmation back to player (use CHAT_MSG_SAY so it appears as say)
            player->SendPersonalMessage("init gear", CHAT_MSG_SAY, LANG_UNIVERSAL);
        }
    }
};

void AddSC_init()
{
    // Initialize the RandomItemManager singleton and build its cache at module load
    sRandomItemMgr->Init();

    // Register the player chat script
    new InitGearScript();

    TC_LOG_INFO("init_gear", "RandomItemManager initialized and init_gear script registered.");
}