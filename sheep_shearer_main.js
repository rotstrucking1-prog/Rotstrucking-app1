/**
 * Sheep Shearer Quest Bot - DeadZone Community Package
 * Shears sheep -> Spins wool at Lumbridge -> Turns in to Fred
 * 
 * Start near Fred's sheep pen with shears in inventory.
 */

// ============ IDs ============
var SHEEP_IDS = [2786, 2699, 2787, 2693, 2694];
var FRED_ID = 732;
var WHEEL_ID = 14889;
var WOOL_ID = 1737;
var BALL_ID = 1759;
var SHEARS_ID = 1735;

// ============ Locations ============
var SHEEP_PEN;
var WHEEL_LOC;
var FRED_LOC;

// ============ States ============
var SHEARING = 0;
var GOING_TO_WHEEL = 1;
var SPINNING = 2;
var SPIN_WAITING = 3;
var GOING_TO_FRED = 4;
var TALKING = 5;
var DONE = 6;

// ============ Variables ============
var state;
var cooldown;
var totalTicksRunning;
var talkTicks;

function OnStart() {
    totalTicksRunning = 0;
    cooldown = 0;
    talkTicks = 0;

    SHEEP_PEN = new WorldPoint(3201, 3268, 0);
    WHEEL_LOC = new WorldPoint(3209, 3213, 1);
    FRED_LOC = new WorldPoint(3190, 3270, 0);

    var wool = Game.info.inventory.getItemCount(WOOL_ID);
    var balls = Game.info.inventory.getItemCount(BALL_ID);

    if (!Game.info.inventory.hasItem(SHEARS_ID, 1) && balls < 20) {
        Game.sendGameMessage("WARNING: No shears found! Pick up shears first.", "SheepBot");
    }

    if (balls >= 20) {
        state = GOING_TO_FRED;
        Game.sendGameMessage("Have 20 balls already! Walking to Fred.", "SheepBot");
        PlayerHelper.webWalkTo(FRED_LOC);
    } else if (wool + balls >= 20) {
        state = GOING_TO_WHEEL;
        Game.sendGameMessage("Have " + wool + " wool + " + balls + " balls. Going to spin!", "SheepBot");
        PlayerHelper.webWalkTo(WHEEL_LOC);
    } else {
        state = SHEARING;
        Game.sendGameMessage("Need " + (20 - wool - balls) + " more wool. Let's shear!", "SheepBot");
    }
}

function OnGameTick() {
    totalTicksRunning++;

    // Respect cooldown
    if (cooldown > 0) {
        cooldown--;
        return;
    }

    // Respect skilling breaks (anti-ban)
    if (Utility.isSkillingBreakActive()) {
        return;
    }

    // Don't interrupt web walking
    if (PlayerHelper.isWebWalking()) {
        return;
    }

    var wool = Game.info.inventory.getItemCount(WOOL_ID);
    var balls = Game.info.inventory.getItemCount(BALL_ID);

    switch (state) {

        // ===== STATE: SHEARING SHEEP =====
        case SHEARING:
            if (wool + balls >= 20) {
                state = GOING_TO_WHEEL;
                Game.sendGameMessage("Got " + wool + " wool + " + balls + " balls! Heading to spin.", "SheepBot");
                Utility.invokeLater(function () {
                    PlayerHelper.webWalkTo(WHEEL_LOC);
                }, Utility.getDelay());
                cooldown = 3;
                return;
            }
            if (!PlayerHelper.isPlayerIdle()) {
                return;
            }
            Utility.invokeLater(function () {
                Game.interact.npc.nearest(MenuAction.NPC_FIRST_OPTION, SHEEP_IDS);
            }, Utility.getDelay());
            cooldown = 5;
            break;

        // ===== STATE: WALKING TO SPINNING WHEEL =====
        case GOING_TO_WHEEL:
            if (PlayerHelper.isPlayerIdle()) {
                state = SPINNING;
                cooldown = 2;
                Game.sendGameMessage("At spinning wheel. Let's spin!", "SheepBot");
            }
            break;

        // ===== STATE: CLICK SPINNING WHEEL =====
        case SPINNING:
            if (wool == 0) {
                if (balls >= 20) {
                    state = GOING_TO_FRED;
                    Game.sendGameMessage("All " + balls + " balls ready! Walking to Fred.", "SheepBot");
                    Utility.invokeLater(function () {
                        PlayerHelper.webWalkTo(FRED_LOC);
                    }, Utility.getDelay());
                    cooldown = 3;
                } else {
                    state = SHEARING;
                    Game.sendGameMessage("Only " + balls + " balls. Need more wool!", "SheepBot");
                    Utility.invokeLater(function () {
                        PlayerHelper.webWalkTo(SHEEP_PEN);
                    }, Utility.getDelay());
                    cooldown = 3;
                }
                return;
            }
            if (!PlayerHelper.isPlayerIdle()) {
                return;
            }
            // Click the spinning wheel
            Utility.invokeLater(function () {
                Game.interact.gameObject.nearest(MenuAction.GAME_OBJECT_FIRST_OPTION, [WHEEL_ID]);
            }, Utility.getDelay());
            // Press space after delay to confirm "Make All"
            Utility.invokeLater(function () {
                Utility.pressSpaceKey();
            }, 2400);
            state = SPIN_WAITING;
            cooldown = 8;
            break;

        // ===== STATE: WAITING FOR SPINNING TO FINISH =====
        case SPIN_WAITING:
            if (PlayerHelper.isPlayerIdle()) {
                // Spinning done or failed - go back to SPINNING to re-evaluate
                state = SPINNING;
                cooldown = 2;
            }
            break;

        // ===== STATE: WALKING TO FRED =====
        case GOING_TO_FRED:
            if (PlayerHelper.isPlayerIdle()) {
                state = TALKING;
                talkTicks = 0;
                cooldown = 2;
                Game.sendGameMessage("Reached Fred. Handing in wool!", "SheepBot");
            }
            break;

        // ===== STATE: TALKING TO FRED =====
        case TALKING:
            talkTicks++;
            if (talkTicks == 1) {
                // Click Fred to start dialogue
                Utility.invokeLater(function () {
                    Game.interact.npc.nearest(MenuAction.NPC_FIRST_OPTION, [FRED_ID]);
                }, Utility.getDelay());
                cooldown = 5;
            } else if (talkTicks <= 20) {
                // Mash space to advance dialogue
                Utility.invokeLater(function () {
                    Utility.pressSpaceKey();
                }, Utility.getDelay());
                cooldown = 2;
            } else {
                // Check if wool was handed in
                if (Game.info.inventory.getItemCount(BALL_ID) == 0) {
                    state = DONE;
                } else {
                    // Retry dialogue
                    Game.sendGameMessage("Retrying dialogue with Fred...", "SheepBot");
                    talkTicks = 0;
                    cooldown = 3;
                }
            }
            break;

        // ===== STATE: QUEST COMPLETE =====
        case DONE:
            Game.sendGameMessage("=== SHEEP SHEARER QUEST COMPLETE! ===", "SheepBot");
            Utility.packages.shutdown();
            break;
    }
}
