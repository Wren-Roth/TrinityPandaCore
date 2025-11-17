
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "Player.h"
#include "ObjectAccessor.h"
#include "GameObject.h"
#include <cmath>

/*
 NPC (Entry: 46245) — fires player from a cannon (GO 205567).
 go_cannon_loader (GO 206679) — clickable cannon that loads & fires player.
 Behavior (both):
  - find nearest cannon / use clicked cannon
  - move player slightly forward & up out of the cannon mouth to avoid camera clipping/falling
  - cannon prep spell (144199) is cast at the moment of launch (synchronized)
  - shrink player model and disable movement/selection while loaded
  - wait LAUNCH_DELAY_MS
  - restore scale/flags immediately when firing, then launch player via MoveJump (smooth arc)
  - cast spell 144157 on the player as soon as they land (with a small buffer to avoid firing early)
*/

// NPC / GO entries
enum SwampOfSorrowsNPCs
{
    NPC_CANNON_SHOOTER = 46245,
    NPC_CRAZY_DAISY = 46503  // new NPC
};

enum SwampOfSorrowsGossip
{
    ACTION_SHOOT = 1
};

static constexpr uint32 GO_CANNON = 205567;      // world cannon (for NPC to find)
static constexpr uint32 GO_CLICK_CANNON = 206679; // clickable cannon ID
static constexpr uint32 SPELL_CANNON_PREP = 144199;
static constexpr uint32 SPELL_ON_LAND = 136214; // cast on player when they land

// Default target location to land at when shot from the cannon.
// The clickable cannon below will use its own CLICK_TARGET_* constants.
static constexpr float TARGET_X = -9962.78f;
static constexpr float TARGET_Y = -4540.62f;
static constexpr float TARGET_Z = 11.83f;

// Clickable GO specific landing target (custom for GO_CLICK_CANNON).
static constexpr float CLICK_TARGET_X = -10172.0f;
static constexpr float CLICK_TARGET_Y = -4183.0f;
static constexpr float CLICK_TARGET_Z = 22.0f;

// Crazy Daisy specific landing target (change to desired coordinates)
static constexpr float CRAZY_DAISY_TARGET_X = -10798.0f;
static constexpr float CRAZY_DAISY_TARGET_Y = -3836.0f;
static constexpr float CRAZY_DAISY_TARGET_Z = 21.5f;

// Jump tuning (defaults)
static constexpr float JUMP_SPEED = 75.0f;
static constexpr float JUMP_ARC_HEIGHT = 25.0f;

// Crazy Daisy specific jump tuning
static constexpr float CRAZY_DAISY_JUMP_SPEED = 200.0f;     // example: faster launch for Daisy
static constexpr float CRAZY_DAISY_JUMP_ARC_HEIGHT = 30.0f; // example different arc

// Search radius to locate the cannon
static constexpr float CANNON_SEARCH_RADIUS = 20.0f;

// Delay (ms) between teleporting the player into the cannon and the actual launch.
static constexpr uint32 LAUNCH_DELAY_MS = 3000;

// How far to move the player along the cannon facing to avoid the camera getting inside.
static constexpr float FORWARD_OFFSET = -2.5f;

// Small vertical offset when teleporting into the cannon to avoid clipping into the ground.
static constexpr float UP_OFFSET = 0.9f;

// Scale to use while player is in the cannon (very small to hide model).
static constexpr float CANNON_HIDDEN_SCALE = 0.01f;

// Buffer after spline duration before any follow-up (ms)
static constexpr uint32 SPLINE_END_BUFFER_MS = 100;

// Extra buffer (ms) to delay the landing spell cast so it fires after the client actually lands.
// Increase if it still triggers too early on your client.
static constexpr uint32 LAND_CAST_BUFFER_MS = 600;

// Crazy Daisy specific landing-spell buffer (ms)
static constexpr uint32 CRAZY_DAISY_LAND_CAST_BUFFER_MS = 1200;

// Helper shared logic used by both NPC and GO handlers.
// destX/Y/Z default to the NPC target; pass CLICK_TARGET_* or CRAZY_DAISY_TARGET_* when calling from the clickable GO or Crazy Daisy.
// speedXY and speedZ default to JUMP_SPEED / JUMP_ARC_HEIGHT; pass CRAZY_DAISY_JUMP_SPEED/CRAZY_DAISY_JUMP_ARC_HEIGHT for Daisy.
// landCastBuffer default uses LAND_CAST_BUFFER_MS; Daisy can pass CRAZY_DAISY_LAND_CAST_BUFFER_MS.
static void HandlePlayerLoadAndLaunch(Player* player, WorldObject* source, uint64 sourceGUID,
    float destX = TARGET_X, float destY = TARGET_Y, float destZ = TARGET_Z,
    float speedXY = JUMP_SPEED, float speedZ = JUMP_ARC_HEIGHT,
    uint32 landCastBuffer = LAND_CAST_BUFFER_MS)
{
    if (!player || !source)
        return;

    // Save original scale so we can restore it later.
    const float originalScale = player->GetObjectScale();

    // Use source position/orientation (source can be Creature or GameObject)
    float sx = source->GetPositionX();
    float sy = source->GetPositionY();
    float sz = source->GetPositionZ();
    float so = source->GetOrientation();

    // Compute a small forward offset along the source orientation and a small upward offset.
    float nx = sx + FORWARD_OFFSET * std::cos(so);
    float ny = sy + FORWARD_OFFSET * std::sin(so);
    float nz = sz + UP_OFFSET;

    // Teleport player to the adjusted position.
    player->NearTeleportTo(nx, ny, nz, so);

    // Immediately disable movement & selection and shrink the player model to hide it.
    player->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_DISABLE_MOVE);
    player->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);

    // Instantly update the player's scale and push visibility update.
    player->SetObjectScale(CANNON_HIDDEN_SCALE);
    player->UpdateVisibilityForPlayer();

    // Capture GUIDs and the live Player*/source* for resolution inside lambdas.
    const uint64 playerGUID = player->GetGUID();
    const uint64 srcGUID = sourceGUID;
    Player* capturedPlayer = player;         // safe: scheduling on player's m_Events keeps player valid
    WorldObject* capturedSource = source;    // will be checked before use (may be nullptr or removed)

    // Schedule launch on the player's event queue (safe because we scheduled on player->m_Events)
    capturedPlayer->m_Events.Schedule(LAUNCH_DELAY_MS, 1, [playerGUID, srcGUID, originalScale, capturedPlayer, capturedSource, destX, destY, destZ, speedXY, speedZ, landCastBuffer]()
        {
            // Use the captured player pointer directly (safe: lambda scheduled on player's event queue).
            Player* p = capturedPlayer;
            if (!p)
                return;

            // If player disconnected or died, restore and return.
            if (!p->IsAlive())
            {
                p->SetObjectScale(originalScale);
                p->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_DISABLE_MOVE);
                p->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                p->UpdateVisibilityForPlayer();
                return;
            }

            // Restore scale and flags immediately so player is visible during flight
            p->SetObjectScale(originalScale);
            p->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_DISABLE_MOVE);
            p->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
            p->UpdateVisibilityForPlayer();

            // If we captured a source and it still exists in world, cast the prep/visual spell now so it lines up with launch.
            if (capturedSource && capturedSource->IsInWorld())
            {
                if (GameObject* gc = capturedSource->ToGameObject())
                    gc->CastSpell(p, SPELL_CANNON_PREP);
                else if (Creature* cr = capturedSource->ToCreature())
                    cr->CastSpell(p, SPELL_CANNON_PREP);
            }

            // Launch with a smooth arc to the provided destination using provided speeds
            p->GetMotionMaster()->MoveJump(destX, destY, destZ, speedXY, speedZ);

            // Schedule the landing spell at spline end (landing moment) plus a small buffer to avoid early firing.
            uint32 dur = p->GetSplineDuration();
            if (dur > 0)
            {
                // schedule cast of SPELL_ON_LAND slightly after landing time using provided buffer
                p->m_Events.Schedule(dur + landCastBuffer, 1, [capturedPlayer]()
                    {
                        Player* pLand = capturedPlayer;
                        if (!pLand || !pLand->IsAlive())
                            return;
                        pLand->CastSpell(pLand, SPELL_ON_LAND, true);
                    });
            }

            // Safety: ensure restore after spline as fallback.
            p->m_Events.Schedule(dur + SPLINE_END_BUFFER_MS, 1, [playerGUID, originalScale, capturedPlayer]()
                {
                    Player* p2 = capturedPlayer;
                    if (!p2)
                        return;

                    if (p2->GetObjectScale() < originalScale)
                        p2->SetObjectScale(originalScale);
                    p2->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_DISABLE_MOVE);
                    p2->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                    p2->UpdateVisibilityForPlayer();
                });
        });
}

/* NPC script (existing) */
class npc_cannon_shooter_swamp : public CreatureScript
{
public:
    npc_cannon_shooter_swamp() : CreatureScript("npc_cannon_shooter_swamp") {}

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Fire me out of the cannon!", GOSSIP_SENDER_MAIN, ACTION_SHOOT);
        player->SEND_GOSSIP_MENU(player->GetGossipTextId(creature), creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        player->PlayerTalkClass->ClearMenus();

        if (action != ACTION_SHOOT)
        {
            player->CLOSE_GOSSIP_MENU();
            return false;
        }

        player->CLOSE_GOSSIP_MENU();

        // If player is in a vehicle, remove them first.
        if (player->IsOnVehicle())
            player->ExitVehicle();

        // Find nearest cannon GameObject (entry GO_CANNON) around the player; fallback to NPC area.
        GameObject* cannon = player->FindNearestGameObject(GO_CANNON, CANNON_SEARCH_RADIUS);
        if (!cannon)
            cannon = creature->FindNearestGameObject(GO_CANNON, CANNON_SEARCH_RADIUS);

        // Use helper to load and schedule the launch. Provide the cannon (or NPC) as the source.
        // NPC uses default target (TARGET_X/Y/Z) and default speed.
        HandlePlayerLoadAndLaunch(player, cannon ? (WorldObject*)cannon : (WorldObject*)creature, (cannon ? cannon->GetGUID() : creature->GetGUID()));

        return true;
    }
};

/* Crazy Daisy NPC script */
class npc_crazy_daisy : public CreatureScript
{
public:
    npc_crazy_daisy() : CreatureScript("npc_crazy_daisy") {}

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Shoot me, Crazy Daisy!", GOSSIP_SENDER_MAIN, ACTION_SHOOT);
        player->SEND_GOSSIP_MENU(player->GetGossipTextId(creature), creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        player->PlayerTalkClass->ClearMenus();

        if (action != ACTION_SHOOT)
        {
            player->CLOSE_GOSSIP_MENU();
            return false;
        }

        player->CLOSE_GOSSIP_MENU();

        if (player->IsOnVehicle())
            player->ExitVehicle();

        // Crazy Daisy uses the same world cannon (GO_CANNON). Find nearest and launch to CRAZY_DAISY_TARGET_* with Daisy speed and Daisy buffer.
        GameObject* cannon = player->FindNearestGameObject(GO_CANNON, CANNON_SEARCH_RADIUS);
        if (!cannon)
            cannon = creature->FindNearestGameObject(GO_CANNON, CANNON_SEARCH_RADIUS);

        HandlePlayerLoadAndLaunch(player, cannon ? (WorldObject*)cannon : (WorldObject*)creature, (cannon ? cannon->GetGUID() : creature->GetGUID()),
            CRAZY_DAISY_TARGET_X, CRAZY_DAISY_TARGET_Y, CRAZY_DAISY_TARGET_Z, CRAZY_DAISY_JUMP_SPEED, CRAZY_DAISY_JUMP_ARC_HEIGHT, CRAZY_DAISY_LAND_CAST_BUFFER_MS);

        return true;
    }
};

/* GameObject script — clickable cannon (GO_CLICK_CANNON) */
class go_cannon_loader : public GameObjectScript
{
public:
    go_cannon_loader() : GameObjectScript("go_cannon_loader") {}

    bool OnGossipHello(Player* player, GameObject* go) override
    {
        // Offer the same gossip menu as the NPC
        player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Load me into the cannon", GOSSIP_SENDER_MAIN, ACTION_SHOOT);
        player->SEND_GOSSIP_MENU(player->GetGossipTextId(go), go->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, GameObject* go, uint32 /*sender*/, uint32 action) override
    {
        player->PlayerTalkClass->ClearMenus();

        if (action != ACTION_SHOOT)
        {
            player->CLOSE_GOSSIP_MENU();
            return false;
        }

        player->CLOSE_GOSSIP_MENU();

        if (player->IsOnVehicle())
            player->ExitVehicle();

        // Use the clicked gameobject as the source of the load+launch.
        // Pass the GO-specific landing target (CLICK_TARGET_*)
        HandlePlayerLoadAndLaunch(player, go, go->GetGUID(), CLICK_TARGET_X, CLICK_TARGET_Y, CLICK_TARGET_Z);

        return true;
    }
};

void AddSC_swamp_of_sorrows()
{
    new npc_cannon_shooter_swamp();
    new npc_crazy_daisy();
    new go_cannon_loader();
}