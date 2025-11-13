#include "ScriptMgr.h"
#include "Player.h"
#include "Map.h"
#include "Group.h"
#include "Log.h"
#include <unordered_map>
#include <algorithm> // for std::min
#include <vector> // added for std::vector
#include <cmath> // for std::pow
#include "SpellScript.h"
#include "Config.h"

struct StatBackup
{
    int32 ap = 0;
    uint32 spellPower = 0;
    int32 stamina = 0;
    int32 strength = 0;
    int32 agility = 0;
    int32 spirit = 0;
    int32 intellect = 0;

    float lastPercentStamina = 0.0f;
    float lastPercentStrength = 0.0f;
    float lastPercentAgility = 0.0f;
    float lastPercentSpirit = 0.0f;
    float lastPercentIntellect = 0.0f;

    float lastFlatStamina = 0.0f;
    float lastFlatStrength = 0.0f;
    float lastFlatAgility = 0.0f;
    float lastFlatSpirit = 0.0f;
    float lastFlatIntellect = 0.0f;

    int32 spApplied = 0;

    uint32 lastAuraCount = 0;
    bool lastWasAlive = true;
    bool valid = false;
};

class mod_balance : public PlayerScript
{
public:
    mod_balance() : PlayerScript("mod_balance")
    {
        TC_LOG_INFO("module.balance", "mod_balance ctor called");
        boostPerMissing = sConfigMgr->GetFloatDefault("DungeonBoost.PerMissing", 0.03f);
        maxTotalBoost = sConfigMgr->GetFloatDefault("DungeonBoost.MaxTotal", 0.50f);
        diminishingExp = sConfigMgr->GetFloatDefault("DungeonBoost.DiminishingExp", 0.5f);
    }

    static std::unordered_map<uint64, StatBackup> statMap;
    static float boostPerMissing;
    static float maxTotalBoost;
    static float diminishingExp;

    void OnMapChanged(Player* player) override
    {
        if (!player) return;

        Map* map = player->GetMap();
        bool isGhost = player->HasAura(8326);

        uint64 guid = player->GetGUID();
        auto it = statMap.find(guid);

        if ((!map || (!map->IsDungeon() && !map->IsRaid())) && it != statMap.end() && it->second.valid)
        {
            StatBackup& bk = it->second;
            bool oldCanModify = player->CanModifyStats();
            if (!oldCanModify)
                player->SetCanModifyStats(true);

            removePercentMod(player, STAT_STAMINA, bk.lastPercentStamina);
            removePercentMod(player, STAT_STRENGTH, bk.lastPercentStrength);
            removePercentMod(player, STAT_AGILITY, bk.lastPercentAgility);
            removePercentMod(player, STAT_SPIRIT, bk.lastPercentSpirit);
            removePercentMod(player, STAT_INTELLECT, bk.lastPercentIntellect);

            removeDirectFlat(player, UNIT_FIELD_STAT_POS_BUFF + STAT_STAMINA, bk.lastFlatStamina);
            removeDirectFlat(player, UNIT_FIELD_STAT_POS_BUFF + STAT_STRENGTH, bk.lastFlatStrength);
            removeDirectFlat(player, UNIT_FIELD_STAT_POS_BUFF + STAT_AGILITY, bk.lastFlatAgility);
            removeDirectFlat(player, UNIT_FIELD_STAT_POS_BUFF + STAT_SPIRIT, bk.lastFlatSpirit);
            removeDirectFlat(player, UNIT_FIELD_STAT_POS_BUFF + STAT_INTELLECT, bk.lastFlatIntellect);

            if (bk.spApplied != 0)
            {
                player->ApplySpellPowerBonus(-bk.spApplied, true);
                bk.spApplied = 0;
            }

            auto clampNonNeg = [](int32 v) -> int32 { return v < 0 ? 0 : v; };

            player->SetStat(STAT_STAMINA, clampNonNeg(bk.stamina));
            player->SetStat(STAT_STRENGTH, clampNonNeg(bk.strength));
            player->SetStat(STAT_AGILITY, clampNonNeg(bk.agility));
            player->SetStat(STAT_SPIRIT, clampNonNeg(bk.spirit));
            player->SetStat(STAT_INTELLECT, clampNonNeg(bk.intellect));

            player->InitStatsForLevel(true);

            player->UpdateAllStats();
            player->UpdateMaxHealth();
            player->UpdateAttackPowerAndDamage(false);
            player->UpdateAllRatings();
            player->UpdatePowerRegeneration();
            player->UpdatePvpPower();

            if (int32(player->GetStat(STAT_STAMINA)) < 0) player->SetStat(STAT_STAMINA, 0);
            if (int32(player->GetStat(STAT_STRENGTH)) < 0) player->SetStat(STAT_STRENGTH, 0);
            if (int32(player->GetStat(STAT_AGILITY)) < 0) player->SetStat(STAT_AGILITY, 0);
            if (int32(player->GetStat(STAT_SPIRIT)) < 0) player->SetStat(STAT_SPIRIT, 0);
            if (int32(player->GetStat(STAT_INTELLECT)) < 0) player->SetStat(STAT_INTELLECT, 0);

            bk.lastPercentStamina = bk.lastPercentStrength = bk.lastPercentAgility = bk.lastPercentSpirit = bk.lastPercentIntellect = 0.0f;
            bk.lastFlatStamina = bk.lastFlatStrength = bk.lastFlatAgility = bk.lastFlatSpirit = bk.lastFlatIntellect = 0.0f;
            bk.lastAuraCount = 0;
            bk.valid = false;
            bk.lastWasAlive = (player->GetHealth() > 0 && !isGhost);

            if (!oldCanModify)
                player->SetCanModifyStats(false);

            player->UpdateAllStats();
            player->UpdateMaxHealth();
            player->UpdateAttackPowerAndDamage(false);
            player->UpdateAllRatings();
            player->UpdatePowerRegeneration();
            player->UpdatePvpPower();

            TC_LOG_INFO("module.balance", "Restored stats and cleared boost for playerGuidLow=%u", player->GetGUIDLow());
        }

        EnsureBoost(player);
    }

    void OnUpdate(Player* player, uint32 /*diff*/) override
    {
        if (!player) return;

        Map* map = player->GetMap();
        if (!map || (!map->IsDungeon() && !map->IsRaid()))
            return;

        uint64 guid = player->GetGUID();
        StatBackup& bk = statMap[guid];

        uint32 auraCount = uint32(player->GetAppliedAuras().size());
        bool isAliveNow = (player->GetHealth() > 0 && !player->HasAura(8326));

        if (bk.valid)
        {
            if (auraCount != bk.lastAuraCount)
            {
                bk.lastAuraCount = auraCount;
                EnsureBoost(player);
            }

            if (!bk.lastWasAlive && isAliveNow)
            {
                player->InitStatsForLevel(true);
                player->UpdateAllStats();
                player->UpdateMaxHealth();
                player->UpdateAttackPowerAndDamage(false);
                player->UpdateAllRatings();
                player->UpdatePowerRegeneration();
                player->UpdatePvpPower();

                EnsureBoost(player);

                TC_LOG_INFO("module.balance", "Player %u revived in-instance: reinit stats and reapplied boosts", player->GetGUIDLow());
            }

            bk.lastAuraCount = auraCount;
            bk.lastWasAlive = isAliveNow;
        }
        else
        {
            bk.lastAuraCount = auraCount;
            bk.lastWasAlive = isAliveNow;
        }
    }

    static void EnsureBoost(Player* player)
    {
        if (!player) return;

        Map* map = player->GetMap();
        uint64 guid = player->GetGUID();

        bool inInstance = map && (map->IsDungeon() || map->IsRaid());
        StatBackup& bk = statMap[guid];

        if (inInstance)
        {
            if (!bk.valid)
            {
                bk.ap = player->GetTotalAttackPowerValue(BASE_ATTACK);
                bk.spellPower = player->GetBaseSpellPowerBonus();
                bk.stamina = static_cast<int32>(player->GetCreateStat(STAT_STAMINA));
                bk.strength = static_cast<int32>(player->GetCreateStat(STAT_STRENGTH));
                bk.agility = static_cast<int32>(player->GetCreateStat(STAT_AGILITY));
                bk.spirit = static_cast<int32>(player->GetCreateStat(STAT_SPIRIT));
                bk.intellect = static_cast<int32>(player->GetCreateStat(STAT_INTELLECT));

                bk.lastPercentStamina = bk.lastPercentStrength = bk.lastPercentAgility = bk.lastPercentSpirit = bk.lastPercentIntellect = 0.0f;
                bk.lastFlatStamina = bk.lastFlatStrength = bk.lastFlatAgility = bk.lastFlatSpirit = bk.lastFlatIntellect = 0.0f;
                bk.spApplied = 0;
                bk.lastAuraCount = uint32(player->GetAppliedAuras().size());
                bk.lastWasAlive = (player->GetHealth() > 0 && !player->HasAura(8326));

                bk.valid = true;

                TC_LOG_INFO("module.balance", "Saved base stats for playerGuidLow=%u STAM=%d STR=%d AGI=%d SPI=%d INT=%d SP=%u",
                    player->GetGUIDLow(), bk.stamina, bk.strength, bk.agility, bk.spirit, bk.intellect, bk.spellPower);
            }

            if (player->GetHealth() == 0 || player->HasAura(8326))
            {
                bk.lastWasAlive = false;
                return;
            }

            bool oldCanModify = player->CanModifyStats();
            if (!oldCanModify)
                player->SetCanModifyStats(true);

            removePercentMod(player, STAT_STAMINA, bk.lastPercentStamina);
            removePercentMod(player, STAT_STRENGTH, bk.lastPercentStrength);
            removePercentMod(player, STAT_AGILITY, bk.lastPercentAgility);
            removePercentMod(player, STAT_SPIRIT, bk.lastPercentSpirit);
            removePercentMod(player, STAT_INTELLECT, bk.lastPercentIntellect);

            removeDirectFlat(player, UNIT_FIELD_STAT_POS_BUFF + STAT_STAMINA, bk.lastFlatStamina);
            removeDirectFlat(player, UNIT_FIELD_STAT_POS_BUFF + STAT_STRENGTH, bk.lastFlatStrength);
            removeDirectFlat(player, UNIT_FIELD_STAT_POS_BUFF + STAT_AGILITY, bk.lastFlatAgility);
            removeDirectFlat(player, UNIT_FIELD_STAT_POS_BUFF + STAT_SPIRIT, bk.lastFlatSpirit);
            removeDirectFlat(player, UNIT_FIELD_STAT_POS_BUFF + STAT_INTELLECT, bk.lastFlatIntellect);

            Group* group = player->GetGroup();
            uint32 groupSize = group ? group->GetMembersCount() : 1;

            uint32 maxGroupCap = 5;
            if (map->IsRaid())
            {
                Difficulty diff = map->GetDifficulty();
                switch (diff)
                {
                case RAID_DIFFICULTY_10MAN_NORMAL:
                case RAID_DIFFICULTY_10MAN_HEROIC: maxGroupCap = 10; break;
                case RAID_DIFFICULTY_25MAN_NORMAL:
                case RAID_DIFFICULTY_25MAN_HEROIC:
                case RAID_DIFFICULTY_25MAN_LFR:
                case RAID_DIFFICULTY_1025MAN_FLEX: maxGroupCap = 25; break;
                case RAID_DIFFICULTY_40MAN: maxGroupCap = 40; break;
                default: maxGroupCap = MAXRAIDSIZE; break;
                }
            }

            if (groupSize > maxGroupCap) groupSize = maxGroupCap;
            uint32 missing = (groupSize < maxGroupCap) ? (maxGroupCap - groupSize) : 0;

            float effectiveMissing = (missing == 0) ? 0.0f : std::pow(static_cast<float>(missing), diminishingExp);
            float rawBoost = effectiveMissing * boostPerMissing;
            float ratio = std::min(rawBoost, maxTotalBoost);
            float multiplier = 1.0f + ratio;

            applyPercentMod(player, STAT_STAMINA, ratio);
            applyPercentMod(player, STAT_STRENGTH, ratio);
            applyPercentMod(player, STAT_AGILITY, ratio);
            applyPercentMod(player, STAT_SPIRIT, ratio);
            applyPercentMod(player, STAT_INTELLECT, ratio);

            float percentPoints = ratio * 100.0f;
            bk.lastPercentStamina = percentPoints;
            bk.lastPercentStrength = percentPoints;
            bk.lastPercentAgility = percentPoints;
            bk.lastPercentSpirit = percentPoints;
            bk.lastPercentIntellect = percentPoints;

            uint32 baseSP = bk.spellPower;
            uint32 newSP = static_cast<uint32>(static_cast<float>(baseSP) * multiplier);
            int32 newSpDelta = static_cast<int32>(newSP) - static_cast<int32>(baseSP);

            if (bk.spApplied != 0)
                player->ApplySpellPowerBonus(-bk.spApplied, true);

            if (newSpDelta != 0)
            {
                player->ApplySpellPowerBonus(newSpDelta, true);
                TC_LOG_INFO("module.balance", "Applied spellpower delta %+d for playerGuidLow=%u", newSpDelta, player->GetGUIDLow());
            }

            bk.spApplied = newSpDelta;

            player->UpdateAllStats();

            {
                float totalStam = player->GetTotalStatValue(STAT_STAMINA);
                float visibleStam = float(player->GetStat(STAT_STAMINA));
                float deltaStam = totalStam - visibleStam;

                float totalStr = player->GetTotalStatValue(STAT_STRENGTH);
                float visibleStr = float(player->GetStat(STAT_STRENGTH));
                float deltaStr = totalStr - visibleStr;

                float totalAgi = player->GetTotalStatValue(STAT_AGILITY);
                float visibleAgi = float(player->GetStat(STAT_AGILITY));
                float deltaAgi = totalAgi - visibleAgi;

                float totalSpi = player->GetTotalStatValue(STAT_SPIRIT);
                float visibleSpi = float(player->GetStat(STAT_SPIRIT));
                float deltaSpi = totalSpi - visibleSpi;

                float totalInt = player->GetTotalStatValue(STAT_INTELLECT);
                float visibleInt = float(player->GetStat(STAT_INTELLECT));
                float deltaInt = totalInt - visibleInt;

                applyDirectFlat(player, UNIT_FIELD_STAT_POS_BUFF + STAT_STAMINA, deltaStam);
                applyDirectFlat(player, UNIT_FIELD_STAT_POS_BUFF + STAT_STRENGTH, deltaStr);
                applyDirectFlat(player, UNIT_FIELD_STAT_POS_BUFF + STAT_AGILITY, deltaAgi);
                applyDirectFlat(player, UNIT_FIELD_STAT_POS_BUFF + STAT_SPIRIT, deltaSpi);
                applyDirectFlat(player, UNIT_FIELD_STAT_POS_BUFF + STAT_INTELLECT, deltaInt);

                bk.lastFlatStamina = deltaStam;
                bk.lastFlatStrength = deltaStr;
                bk.lastFlatAgility = deltaAgi;
                bk.lastFlatSpirit = deltaSpi;
                bk.lastFlatIntellect = deltaInt;
            }

            player->UpdateMaxHealth();
            player->UpdateAttackPowerAndDamage(false);
            player->UpdateAllRatings();
            player->UpdatePowerRegeneration();
            player->UpdatePvpPower();

            if (!oldCanModify)
                player->SetCanModifyStats(false);

            TC_LOG_INFO("module.balance", "Applied stat boosts for playerGuidLow=%u (ratio=%.2f%%)", player->GetGUIDLow(), percentPoints);

            bk.lastAuraCount = uint32(player->GetAppliedAuras().size());
            bk.lastWasAlive = true;
        }
        else
        {
            if (!bk.valid)
                return;

            TC_LOG_INFO("module.balance", "Removing applied boosts for playerGuidLow=%u", player->GetGUIDLow());

            bool oldCanModify = player->CanModifyStats();
            if (!oldCanModify)
                player->SetCanModifyStats(true);

            removePercentMod(player, STAT_STAMINA, bk.lastPercentStamina);
            removePercentMod(player, STAT_STRENGTH, bk.lastPercentStrength);
            removePercentMod(player, STAT_AGILITY, bk.lastPercentAgility);
            removePercentMod(player, STAT_SPIRIT, bk.lastPercentSpirit);
            removePercentMod(player, STAT_INTELLECT, bk.lastPercentIntellect);

            removeDirectFlat(player, UNIT_FIELD_STAT_POS_BUFF + STAT_STAMINA, bk.lastFlatStamina);
            removeDirectFlat(player, UNIT_FIELD_STAT_POS_BUFF + STAT_STRENGTH, bk.lastFlatStrength);
            removeDirectFlat(player, UNIT_FIELD_STAT_POS_BUFF + STAT_AGILITY, bk.lastFlatAgility);
            removeDirectFlat(player, UNIT_FIELD_STAT_POS_BUFF + STAT_SPIRIT, bk.lastFlatSpirit);
            removeDirectFlat(player, UNIT_FIELD_STAT_POS_BUFF + STAT_INTELLECT, bk.lastFlatIntellect);

            if (bk.spApplied != 0)
            {
                player->ApplySpellPowerBonus(-bk.spApplied, true);
                TC_LOG_INFO("module.balance", "Removed spellpower delta -%d for playerGuidLow=%u", bk.spApplied, player->GetGUIDLow());
                bk.spApplied = 0;
            }

            bk.lastPercentStamina = bk.lastPercentStrength = bk.lastPercentAgility = bk.lastPercentSpirit = bk.lastPercentIntellect = 0.0f;
            bk.lastFlatStamina = bk.lastFlatStrength = bk.lastFlatAgility = bk.lastFlatSpirit = bk.lastFlatIntellect = 0.0f;
            bk.lastAuraCount = 0;
            bk.lastWasAlive = (player->GetHealth() > 0 && !player->HasAura(8326));

            if (!oldCanModify)
                player->SetCanModifyStats(false);

            player->UpdateAllStats();
            player->UpdateMaxHealth();
            player->SetHealth(player->GetMaxHealth());
            player->UpdateAttackPowerAndDamage(false);
            player->UpdateAllRatings();
            player->UpdatePowerRegeneration();
            player->UpdatePvpPower();

            bk.valid = false;
            TC_LOG_INFO("module.balance", "Removed applied mods for playerGuidLow=%u", player->GetGUIDLow());
        }
    }

private:
    static void removePercentMod(Player* player, Stats stat, float prevPercent)
    {
        if (prevPercent == 0.0f) return;
        player->ApplyStatPercentBuffMod(stat, prevPercent, false);

        UnitMods fallbackMod = UNIT_MOD_STAT_STRENGTH;
        switch (stat)
        {
        case STAT_STRENGTH:  fallbackMod = UNIT_MOD_STAT_STRENGTH; break;
        case STAT_AGILITY:   fallbackMod = UNIT_MOD_STAT_AGILITY;  break;
        case STAT_STAMINA:   fallbackMod = UNIT_MOD_STAT_STAMINA;  break;
        case STAT_INTELLECT: fallbackMod = UNIT_MOD_STAT_INTELLECT; break;
        case STAT_SPIRIT:    fallbackMod = UNIT_MOD_STAT_SPIRIT;   break;
        default: fallbackMod = UNIT_MOD_STAT_STRENGTH; break;
        }

        float current = player->GetModifierValue(fallbackMod, TOTAL_PCT);
        float newVal = current - prevPercent;
        if (newVal < 0.0f)
            newVal = 0.0f;
        player->SetModifierValue(fallbackMod, TOTAL_PCT, newVal);
    }

    static void applyPercentMod(Player* player, Stats stat, float ratio)
    {
        if (ratio == 0.0f) return;

        float percentPoints = ratio * 100.0f;
        player->ApplyStatPercentBuffMod(stat, percentPoints, true);

        UnitMods fallbackMod = UNIT_MOD_STAT_STRENGTH;
        switch (stat)
        {
        case STAT_STRENGTH:  fallbackMod = UNIT_MOD_STAT_STRENGTH; break;
        case STAT_AGILITY:   fallbackMod = UNIT_MOD_STAT_AGILITY;  break;
        case STAT_STAMINA:   fallbackMod = UNIT_MOD_STAT_STAMINA;  break;
        case STAT_INTELLECT: fallbackMod = UNIT_MOD_STAT_INTELLECT; break;
        case STAT_SPIRIT:    fallbackMod = UNIT_MOD_STAT_SPIRIT;   break;
        default: fallbackMod = UNIT_MOD_STAT_STRENGTH; break;
        }

        float current = player->GetModifierValue(fallbackMod, TOTAL_PCT);
        float newVal = current + percentPoints;
        player->SetModifierValue(fallbackMod, TOTAL_PCT, newVal);
    }
    static void removeDirectFlat(Player* player, int16 fieldIndex, float prev)
    {
        if (prev != 0.0f)
            player->ApplyModPositiveFloatValue(fieldIndex, prev, false);
    }

    static void applyDirectFlat(Player* player, int16 fieldIndex, float val)
    {
        if (val != 0.0f)
            player->ApplyModPositiveFloatValue(fieldIndex, val, true);
    }
};

std::unordered_map<uint64, StatBackup> mod_balance::statMap;
float mod_balance::boostPerMissing = 0.03f;
float mod_balance::maxTotalBoost = 0.50f;
float mod_balance::diminishingExp = 0.5f;

/*
 * Spell/Aura hook: attach this AuraScript to the specific buff spell(s) you care about.
 * To bind this script to a spell, add a spell_script_map entry in your DB that maps
 * the spell id(s) to this script name ("spell_balance_hook"), or use existing tooling
 * that assigns script loaders to spell ids.
 *
 * The AuraScript calls `mod_balance::EnsureBoost(Player*)` on apply/remove.
 */
class spell_balance_hook : public SpellScriptLoader
{
public:
    spell_balance_hook() : SpellScriptLoader("spell_balance_hook") {}

    class spell_balance_hook_AuraScript : public AuraScript
    {
        PrepareAuraScript(spell_balance_hook_AuraScript);

        void OnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
        {
            if (Unit* target = GetTarget())
                if (Player* player = target->ToPlayer())
                    mod_balance::EnsureBoost(player);
        }

        void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
        {
            if (Unit* target = GetTarget())
                if (Player* player = target->ToPlayer())
                    mod_balance::EnsureBoost(player);
        }

        void Register() override
        {
            OnEffectApply += AuraEffectApplyFn(spell_balance_hook_AuraScript::OnApply, EFFECT_ALL, SPELL_AURA_ANY, AURA_EFFECT_HANDLE_REAL);
            OnEffectRemove += AuraEffectRemoveFn(spell_balance_hook_AuraScript::OnRemove, EFFECT_ALL, SPELL_AURA_ANY, AURA_EFFECT_HANDLE_REAL);
        }
    };

    AuraScript* GetAuraScript() const override
    {
        return new spell_balance_hook_AuraScript();
    }
};

void AddSC_mod_balance()
{
    TC_LOG_INFO("module.balance", "AddSC_mod_balance called");
    new mod_balance();
    new spell_balance_hook();
}