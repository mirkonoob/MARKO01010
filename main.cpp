
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <random>
#include <algorithm>
#include <cmath>
#include <deque>
#include <iomanip>
#include <cctype>
#include <windows.h>
#include <conio.h>

/*
========================================================================================
   ASCII DUNGEON CRAWLER - TURN-BASED ROGUELIKE
   Language: Modern C++ (C++17 / C++20)
   Platform: Windows Console

   COMPILATION INSTRUCTIONS:
   -------------------------------------------------------------------------------------
   Using MinGW / GCC:
     g++ -std=c++17 -O2 main.cpp -o dungeon.exe
     .\dungeon.exe

     
C:\msys64\mingw64\bin\g++.exe -std=c++17 -O2 main.cpp -o dungeon.exe
.\dungeon.exe

   Using Clang:
     clang++ -std=c++17 -O2 main.cpp -o dungeon.exe
     .\dungeon.exe

   Using Microsoft Visual C++ (MSVC via Developer Command Prompt / vcvars64.bat):
     cl /std:c++17 /O2 /EHsc main.cpp /Fe:dungeon.exe
     .\dungeon.exe
========================================================================================
*/

constexpr int MAP_WIDTH  = 68;
constexpr int MAP_HEIGHT = 16;
constexpr int MAX_LOG_LINES = 3;
constexpr int GOBLIN_DETECTION_RADIUS = 7;

enum ConsoleColor : WORD {
    COLOR_DEFAULT   = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
    COLOR_WALL      = FOREGROUND_INTENSITY,
    COLOR_FLOOR     = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
    COLOR_PLAYER    = FOREGROUND_GREEN | FOREGROUND_INTENSITY,
    COLOR_GOBLIN    = FOREGROUND_RED | FOREGROUND_INTENSITY,
    COLOR_POTION    = FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
    COLOR_WEAPON    = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
    COLOR_GOLD      = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
    COLOR_STAIRS    = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
    COLOR_TITLE     = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
    COLOR_ACCENT    = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
    COLOR_DANGER    = FOREGROUND_RED | FOREGROUND_INTENSITY,
    COLOR_SUCCESS   = FOREGROUND_GREEN | FOREGROUND_INTENSITY
};

enum class Tile : char {
    Void   = ' ',
    Wall   = '#',
    Floor  = '.',
    Stairs = '>'
};

struct Point {
    int x{0};
    int y{0};

    bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }
};

struct Room {
    int x{0};
    int y{0};
    int w{0};
    int h{0};

    Point center() const {
        return { x + w / 2, y + h / 2 };
    }

    bool intersects(const Room& other, int buffer = 1) const {
        return (x - buffer < other.x + other.w) &&
               (x + w + buffer > other.x) &&
               (y - buffer < other.y + other.h) &&
               (y + h + buffer > other.y);
    }
};

enum class ItemType {
    Gold,
    Potion,
    Weapon
};

struct Item {
    Point pos;
    ItemType type;
    std::string name;
    int value;
};

struct Player {
    Point pos;
    int hp{100};
    int maxHp{100};
    int baseAttack{6};
    std::string weaponName{"Rusty Dagger"};
    int weaponBonus{2};
    int gold{0};
    int kills{0};

    int getAttackDamage(std::mt19937& rng) const {
        std::uniform_int_distribution<int> varDist(-1, 2);
        int total = baseAttack + weaponBonus + varDist(rng);
        return (total < 1) ? 1 : total;
    }
};

struct Goblin {
    int id{0};
    Point pos;
    int hp{20};
    int maxHp{20};
    int attackDamage{4};
    bool alive{true};

    int getAttackDamage(std::mt19937& rng) const {
        std::uniform_int_distribution<int> varDist(-1, 2);
        int total = attackDamage + varDist(rng);
        return (total < 1) ? 1 : total;
    }
};

class DungeonGame {
public:
    DungeonGame()
        : rng(std::random_device{}())
    {
        hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        initConsole();
        generateDungeon();
    }

    ~DungeonGame() {
        restoreConsole();
    }

    void run() {
        while (!shouldQuit) {
            render();

            if (gameOver) {
                Sleep(1200);
                break;
            }

            if (nextLevel) {
                advanceLevel();
                continue;
            }

            handleInput();

            if (playerActed) {
                updateGoblins();
                playerActed = false;
            }
        }

        renderGameOver();
    }

private:
    HANDLE hConsole;
    CONSOLE_CURSOR_INFO originalCursorInfo{};
    std::mt19937 rng;

    std::vector<std::vector<Tile>> map;
    std::vector<Room> rooms;
    Player player;
    std::vector<Goblin> goblins;
    std::vector<Item> items;
    Point stairsPos{-1, -1};

    std::deque<std::string> messageLog;
    int level{1};
    bool playerActed{false};
    bool gameOver{false};
    bool nextLevel{false};
    bool shouldQuit{false};

    void initConsole() {
        SetConsoleTitleA("ASCII Dungeon Crawler (Turn-Based Roguelike)");

        GetConsoleCursorInfo(hConsole, &originalCursorInfo);
        CONSOLE_CURSOR_INFO newCursorInfo = originalCursorInfo;
        newCursorInfo.bVisible = FALSE;
        SetConsoleCursorInfo(hConsole, &newCursorInfo);

        system("cls");
    }

    void restoreConsole() {
        SetConsoleCursorInfo(hConsole, &originalCursorInfo);
        SetConsoleTextAttribute(hConsole, COLOR_DEFAULT);
    }

    void setColor(WORD color) const {
        SetConsoleTextAttribute(hConsole, color);
    }

    void resetColor() const {
        SetConsoleTextAttribute(hConsole, COLOR_DEFAULT);
    }

    void addLog(const std::string& msg) {
        if (messageLog.size() >= MAX_LOG_LINES) {
            messageLog.pop_front();
        }
        messageLog.push_back(msg);
    }

    void endLinePadded(int totalWidth = MAP_WIDTH) {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(hConsole, &csbi)) {
            if (csbi.dwCursorPosition.X < totalWidth) {
                std::cout << std::string(totalWidth - csbi.dwCursorPosition.X, ' ');
            }
        }
        std::cout << "\n";
    }

    void printLinePadded(const std::string& line, int totalWidth = MAP_WIDTH) {
        std::cout << line;
        endLinePadded(totalWidth);
    }

    bool tryDodge() {
        return (rng() % 100) < 5;
    }

    void generateDungeon() {
        map.assign(MAP_HEIGHT, std::vector<Tile>(MAP_WIDTH, Tile::Void));
        rooms.clear();
        goblins.clear();
        items.clear();
        messageLog.clear();

        std::uniform_int_distribution<int> wDist(7, 13);
        std::uniform_int_distribution<int> hDist(4, 6);
        std::uniform_int_distribution<int> xDist(2, MAP_WIDTH - 15);
        std::uniform_int_distribution<int> yDist(2, MAP_HEIGHT - 8);

        constexpr int MAX_ROOM_ATTEMPTS = 150;
        int TARGET_ROOM_COUNT = 6 + level;

        for (int i = 0; i < MAX_ROOM_ATTEMPTS && static_cast<int>(rooms.size()) < TARGET_ROOM_COUNT; ++i) {
            Room newRoom;
            newRoom.w = wDist(rng);
            newRoom.h = hDist(rng);
            newRoom.x = xDist(rng);
            newRoom.y = yDist(rng);

            bool overlaps = false;
            for (const auto& r : rooms) {
                if (newRoom.intersects(r, 2)) {
                    overlaps = true;
                    break;
                }
            }

            if (!overlaps) {
                rooms.push_back(newRoom);
            }
        }

        for (const auto& room : rooms) {
            for (int y = room.y; y < room.y + room.h; ++y) {
                for (int x = room.x; x < room.x + room.w; ++x) {
                    map[y][x] = Tile::Floor;
                }
            }
        }

        for (size_t i = 1; i < rooms.size(); ++i) {
            Point p1 = rooms[i - 1].center();
            Point p2 = rooms[i].center();
            carveCorridor(p1, p2);
        }

        for (int y = 0; y < MAP_HEIGHT; ++y) {
            for (int x = 0; x < MAP_WIDTH; ++x) {
                if (map[y][x] == Tile::Void) {
                    bool adjacentToFloor = false;
                    for (int dy = -1; dy <= 1 && !adjacentToFloor; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            int ny = y + dy;
                            int nx = x + dx;
                            if (nx >= 0 && nx < MAP_WIDTH && ny >= 0 && ny < MAP_HEIGHT) {
                                if (map[ny][nx] == Tile::Floor) {
                                    adjacentToFloor = true;
                                    break;
                                }
                            }
                        }
                    }
                    if (adjacentToFloor) {
                        map[y][x] = Tile::Wall;
                    }
                }
            }
        }

        player.pos = rooms.front().center();

        stairsPos = rooms.back().center();
        map[stairsPos.y][stairsPos.x] = Tile::Stairs;

        const std::vector<std::pair<std::string, int>> weaponTiers = {
            { "Iron Broadsword", 5 },
            { "Spiked Morningstar", 8 },
            { "Battle Axe", 11 },
            { "Mythril Blade", 15 },
            { "Steel Longsword", 19 },
            { "Warhammer", 23 },
            { "Dragonslayer", 28 },
            { "Frostbrand", 34 },
            { "Thunderfury", 41 },
            { "Demon's Bane", 49 },
            { "Excalibur", 58 },
            { "Celestial Edge", 70 }
        };
        size_t weaponIndex = 0;

        int goblinIdCounter = 1;

        for (size_t i = 1; i < rooms.size(); ++i) {
            const auto& r = rooms[i];

            std::uniform_int_distribution<int> countDist(1, 2 + level / 2);
            int goblinCount = countDist(rng);
            for (int g = 0; g < goblinCount; ++g) {
                Point gPos = getRandomFloorInRoom(r);
                if (isTileFree(gPos)) {
                    Goblin gob;
                    gob.id = goblinIdCounter++;
                    gob.pos = gPos;
                    gob.hp = 18 + level * 3 + (rng() % 6);
                    gob.maxHp = gob.hp;
                    gob.attackDamage = 3 + level + (rng() % 3);
                    gob.alive = true;
                    goblins.push_back(gob);
                }
            }

            std::uniform_int_distribution<int> itemTypeDist(0, 99);
            int choice = itemTypeDist(rng);

            Point itemPos = getRandomFloorInRoom(r);
            if (isTileFree(itemPos)) {
                if (choice < 35) {
                    // Gold (35%)
                    std::uniform_int_distribution<int> goldDist(15, 45);
                    items.push_back({ itemPos, ItemType::Gold, "Gold Pouch", goldDist(rng) });
                } else if (choice < 80) {
                    // Potion (45%)
                    std::uniform_int_distribution<int> healDist(25, 40);
                    items.push_back({ itemPos, ItemType::Potion, "Health Potion", healDist(rng) });
                } else {
                    // Weapon (20%)
                    if (weaponIndex < weaponTiers.size()) {
                        const auto& w = weaponTiers[weaponIndex++];
                        items.push_back({ itemPos, ItemType::Weapon, w.first, w.second });
                    } else {
                        items.push_back({ itemPos, ItemType::Gold, "Large Gold Chest", 60 });
                    }
                }
            }
        }

        if (level == 1) {
            addLog("Welcome, Adventurer! Reach stairs (>) or defeat all goblins.");
            addLog("Keys: [W,A,S,D] Move/Attack | [Space] Wait | [Q] Quit");
        } else {
            addLog("Level " + std::to_string(level) + " generated.");
        }
    }

    void advanceLevel() {
        level++;
        nextLevel = false;

        generateDungeon();

        int heal = 30;
        int oldHp = player.hp;
        player.hp = std::min(player.maxHp, player.hp + heal);
        int actualHeal = player.hp - oldHp;

        messageLog.clear();
        addLog("Descending to level " + std::to_string(level) + "...");
        if (actualHeal > 0) {
            addLog("You rested and recovered " + std::to_string(actualHeal) + " HP.");
        }
        addLog("Stronger goblins await on this level!");
    }

    void carveCorridor(Point p1, Point p2) {
        std::uniform_int_distribution<int> coin(0, 1);
        if (coin(rng) == 0) {
            for (int x = std::min(p1.x, p2.x); x <= std::max(p1.x, p2.x); ++x) {
                map[p1.y][x] = Tile::Floor;
            }
            for (int y = std::min(p1.y, p2.y); y <= std::max(p1.y, p2.y); ++y) {
                map[y][p2.x] = Tile::Floor;
            }
        } else {
            for (int y = std::min(p1.y, p2.y); y <= std::max(p1.y, p2.y); ++y) {
                map[y][p1.x] = Tile::Floor;
            }
            for (int x = std::min(p1.x, p2.x); x <= std::max(p1.x, p2.x); ++x) {
                map[p2.y][x] = Tile::Floor;
            }
        }
    }

    Point getRandomFloorInRoom(const Room& r) {
        std::uniform_int_distribution<int> rx(r.x, r.x + r.w - 1);
        std::uniform_int_distribution<int> ry(r.y, r.y + r.h - 1);
        for (int attempts = 0; attempts < 50; ++attempts) {
            Point p{ rx(rng), ry(rng) };
            if (map[p.y][p.x] == Tile::Floor && !(p == player.pos) && !(p == stairsPos)) {
                return p;
            }
        }
        return r.center();
    }

    bool isTileFree(Point p) const {
        if (p == player.pos || p == stairsPos) return false;
        for (const auto& g : goblins) {
            if (g.alive && g.pos == p) return false;
        }
        for (const auto& item : items) {
            if (item.pos == p) return false;
        }
        return true;
    }

    void handleInput() {
        int ch = _getch();

        int dx = 0;
        int dy = 0;
        bool waitTurn = false;

        if (ch == 0 || ch == 224) {
            int arrow = _getch();
            switch (arrow) {
                case 72: dy = -1; break;
                case 80: dy =  1; break;
                case 75: dx = -1; break;
                case 77: dx =  1; break;
                default: break;
            }
        } else {
            char key = static_cast<char>(std::tolower(ch));
            switch (key) {
                case 'w': dy = -1; break;
                case 's': dy =  1; break;
                case 'a': dx = -1; break;
                case 'd': dx =  1; break;
                case ' ':
                case '.': waitTurn = true; break;
                case 'q': shouldQuit = true; return;
                default: break;
            }
        }

        if (waitTurn) {
            addLog("You hold position and ready your guard.");
            playerActed = true;
            return;
        }

        if (dx != 0 || dy != 0) {
            Point target{ player.pos.x + dx, player.pos.y + dy };

            if (target.x < 0 || target.x >= MAP_WIDTH || target.y < 0 || target.y >= MAP_HEIGHT) {
                return;
            }

            auto goblinIt = findGoblinAt(target);
            if (goblinIt != goblins.end()) {
                attackGoblin(*goblinIt);
                playerActed = true;
                return;
            }

            if (map[target.y][target.x] == Tile::Wall || map[target.y][target.x] == Tile::Void) {
                addLog("A solid stone wall blocks your path.");
                return;
            }

            player.pos = target;
            playerActed = true;

            checkItemPickup(player.pos);

            if (player.pos == stairsPos) {
                nextLevel = true;
                addLog("You found the stairs! Descend to the next level?");
            }
        }
    }

    std::vector<Goblin>::iterator findGoblinAt(Point p) {
        return std::find_if(goblins.begin(), goblins.end(), [&](const Goblin& g) {
            return g.alive && g.pos == p;
        });
    }

    void attackGoblin(Goblin& target) {
        if (tryDodge()) {
            addLog("Goblin dodged your attack!");
            return;
        }

        int damage = player.getAttackDamage(rng);
        target.hp -= damage;

        std::string msg = "You strike Goblin with " + player.weaponName + " for " +
                          std::to_string(damage) + " DMG!";
        addLog(msg);

        if (target.hp <= 0) {
            target.alive = false;
            player.kills++;
            std::uniform_int_distribution<int> rewardDist(12, 22);
            int reward = rewardDist(rng);
            player.gold += reward;
            addLog("-> Goblin collapsed! You found " + std::to_string(reward) + " gold coins.");

            int aliveCount = 0;
            for (const auto& g : goblins) {
                if (g.alive) aliveCount++;
            }
            if (aliveCount == 0) {
                nextLevel = true;
                addLog("*** All goblins vanquished! Descend to the next level? ***");
            }
        } else {
            addLog("   (Goblin HP: " + std::to_string(target.hp) + "/" + std::to_string(target.maxHp) + ")");
        }
    }

    void checkItemPickup(Point p) {
        auto it = std::find_if(items.begin(), items.end(), [&](const Item& item) {
            return item.pos == p;
        });

        if (it != items.end()) {
            switch (it->type) {
                case ItemType::Gold: {
                    player.gold += it->value;
                    addLog("Picked up " + it->name + "! (+" + std::to_string(it->value) + " Gold)");
                    break;
                }
                case ItemType::Potion: {
                    int heal = std::min(it->value, player.maxHp - player.hp);
                    player.hp += heal;
                    addLog("Drank " + it->name + "! Restored " + std::to_string(heal) +
                           " HP (" + std::to_string(player.hp) + "/" + std::to_string(player.maxHp) + ")");
                    break;
                }
                case ItemType::Weapon: {
                    if (it->value > player.weaponBonus) {
                        player.weaponName = it->name;
                        player.weaponBonus = it->value;
                        addLog("Equipped " + it->name + "! Attack increased to +" +
                               std::to_string(it->value) + "!");
                    } else {
                        addLog("Found " + it->name + ", but your equipped weapon is stronger.");
                    }
                    break;
                }
            }
            items.erase(it);
        }
    }

    void updateGoblins() {
        for (auto& gob : goblins) {
            if (!gob.alive) continue;

            int manhattanDist = std::abs(gob.pos.x - player.pos.x) + std::abs(gob.pos.y - player.pos.y);
            int chebyshevDist = std::max(std::abs(gob.pos.x - player.pos.x), std::abs(gob.pos.y - player.pos.y));

            if (manhattanDist == 1) {
                if (tryDodge()) {
                    addLog("You dodged the goblin's attack!");
                } else {
                    int damage = gob.getAttackDamage(rng);
                    player.hp -= damage;
                    addLog("Goblin slashes you for " + std::to_string(damage) + " DMG!");

                    if (player.hp <= 0) {
                        player.hp = 0;
                        gameOver = true;
                        addLog("You collapse from your wounds... Defeated!");
                        return;
                    }
                }
            } else if (chebyshevDist <= GOBLIN_DETECTION_RADIUS) {
                stepGoblinTowards(gob, player.pos);
            }
        }
    }

    void stepGoblinTowards(Goblin& gob, Point target) {
        const Point directions[4] = { {0, -1}, {0, 1}, {-1, 0}, {1, 0} };

        Point bestStep = gob.pos;
        int minDistance = std::abs(gob.pos.x - target.x) + std::abs(gob.pos.y - target.y);

        for (const auto& d : directions) {
            Point nextPos{ gob.pos.x + d.x, gob.pos.y + d.y };

            if (nextPos.x < 0 || nextPos.x >= MAP_WIDTH || nextPos.y < 0 || nextPos.y >= MAP_HEIGHT)
                continue;

            if (map[nextPos.y][nextPos.x] != Tile::Floor && map[nextPos.y][nextPos.x] != Tile::Stairs)
                continue;

            if (nextPos == player.pos)
                continue;

            bool occupiedByGoblin = false;
            for (const auto& other : goblins) {
                if (other.alive && other.id != gob.id && other.pos == nextPos) {
                    occupiedByGoblin = true;
                    break;
                }
            }
            if (occupiedByGoblin) continue;

            int newDist = std::abs(nextPos.x - target.x) + std::abs(nextPos.y - target.y);
            if (newDist < minDistance) {
                minDistance = newDist;
                bestStep = nextPos;
            }
        }

        gob.pos = bestStep;
    }

    void render() {
        COORD coord{ 0, 0 };
        SetConsoleCursorPosition(hConsole, coord);

        setColor(COLOR_TITLE);
        printLinePadded("====================================================================");
        printLinePadded("                ASCII DUNGEON CRAWLER (TURN-BASED)                  ");
        resetColor();

        for (int y = 0; y < MAP_HEIGHT; ++y) {
            for (int x = 0; x < MAP_WIDTH; ++x) {
                Point p{ x, y };

                if (player.pos == p) {
                    setColor(COLOR_PLAYER);
                    std::cout << '@';
                    continue;
                }

                bool goblinPrinted = false;
                for (const auto& gob : goblins) {
                    if (gob.alive && gob.pos == p) {
                        setColor(COLOR_GOBLIN);
                        std::cout << 'g';
                        goblinPrinted = true;
                        break;
                    }
                }
                if (goblinPrinted) continue;

                bool itemPrinted = false;
                for (const auto& item : items) {
                    if (item.pos == p) {
                        switch (item.type) {
                            case ItemType::Gold:
                                setColor(COLOR_GOLD);
                                std::cout << '$';
                                break;
                            case ItemType::Potion:
                                setColor(COLOR_POTION);
                                std::cout << '!';
                                break;
                            case ItemType::Weapon:
                                setColor(COLOR_WEAPON);
                                std::cout << '/';
                                break;
                        }
                        itemPrinted = true;
                        break;
                    }
                }
                if (itemPrinted) continue;

                Tile t = map[y][x];
                switch (t) {
                    case Tile::Wall:
                        setColor(COLOR_WALL);
                        std::cout << '#';
                        break;
                    case Tile::Floor:
                        setColor(COLOR_FLOOR);
                        std::cout << '.';
                        break;
                    case Tile::Stairs:
                        setColor(COLOR_STAIRS);
                        std::cout << '>';
                        break;
                    case Tile::Void:
                    default:
                        std::cout << ' ';
                        break;
                }
            }
            std::cout << '\n';
        }

        setColor(COLOR_DEFAULT);
        printLinePadded("====================================================================");

        renderStatsHUD();
        renderActionLog();
        renderLegend();

        setColor(COLOR_DEFAULT);
        printLinePadded("====================================================================");
    }

    void renderStatsHUD() {
        int barLength = 14;
        int filled = (player.hp * barLength) / player.maxHp;
        if (filled < 0) filled = 0;
        if (filled > barLength) filled = barLength;

        std::cout << " HP: [";
        if (player.hp > 50) setColor(COLOR_SUCCESS);
        else if (player.hp > 25) setColor(COLOR_GOLD);
        else setColor(COLOR_DANGER);

        for (int i = 0; i < filled; ++i) std::cout << '=';
        setColor(COLOR_DEFAULT);
        for (int i = filled; i < barLength; ++i) std::cout << ' ';
        std::cout << "] ";

        std::cout << std::setw(3) << player.hp << "/" << std::setw(3) << player.maxHp;
        std::cout << " | Gold: ";
        setColor(COLOR_GOLD);
        std::cout << "$" << std::setw(4) << std::left << player.gold;
        resetColor();

        int aliveGoblins = 0;
        for (const auto& g : goblins) if (g.alive) aliveGoblins++;

        std::cout << " | Goblins Left: " << std::setw(2) << aliveGoblins;
        endLinePadded();

        std::cout << " ATK: " << (player.baseAttack + player.weaponBonus)
                  << " (Base " << player.baseAttack << ", Weapon: ";
        setColor(COLOR_WEAPON);
        std::cout << player.weaponName;
        resetColor();
        std::cout << " [+" << player.weaponBonus << "]) | Slain: " << player.kills
                  << " | Level: " << level;
        endLinePadded();
    }

    void renderActionLog() {
        printLinePadded("--------------------------------------------------------------------");
        setColor(COLOR_ACCENT);
        printLinePadded(" RECENT LOG:");
        resetColor();

        for (int i = 0; i < MAX_LOG_LINES; ++i) {
            if (i < static_cast<int>(messageLog.size())) {
                std::cout << "  * ";
                std::string msg = messageLog[i];
                if (static_cast<int>(msg.length()) > MAP_WIDTH - 6) {
                    msg = msg.substr(0, MAP_WIDTH - 6);
                }

                if (msg.find("strike") != std::string::npos || msg.find("conquered") != std::string::npos) {
                    setColor(COLOR_SUCCESS);
                } else if (msg.find("slashes") != std::string::npos || msg.find("Defeated") != std::string::npos) {
                    setColor(COLOR_DANGER);
                } else if (msg.find("Picked up") != std::string::npos || msg.find("Drank") != std::string::npos || msg.find("gold") != std::string::npos) {
                    setColor(COLOR_GOLD);
                } else if (msg.find("Equipped") != std::string::npos) {
                    setColor(COLOR_WEAPON);
                } else {
                    setColor(COLOR_DEFAULT);
                }
                std::cout << msg;
                resetColor();
            }
            endLinePadded();
        }
    }

    void renderLegend() {
        printLinePadded("--------------------------------------------------------------------");
        printLinePadded(" CONTROLS: [W/A/S/D / Arrows] Move/Attack | [Space] Wait | [Q] Quit");
        printLinePadded(" LEGEND:   [@] Hero  [g] Goblin  [!] Potion  [/] Weapon  [$] Gold  [>] Exit");
    }

    void renderGameOver() {
        system("cls");
        std::cout << "\n\n";
        if (gameOver) {
            setColor(COLOR_DANGER);
            std::cout << "  ******************************************************************\n";
            std::cout << "  *                                                                *\n";
            std::cout << "  *                          GAME OVER                             *\n";
            std::cout << "  *                YOU WERE SLAIN IN THE DUNGEON!                  *\n";
            std::cout << "  *                                                                *\n";
            std::cout << "  ******************************************************************\n";
        } else {
            setColor(COLOR_TITLE);
            std::cout << "  ******************************************************************\n";
            std::cout << "  *                   YOU RETIRED FROM ADVENTURING                 *\n";
            std::cout << "  ******************************************************************\n";
        }

        resetColor();
        std::cout << "\n";
        std::cout << "  ====================== FINAL ADVENTURE STATS =====================\n";
        std::cout << "    Final Level Reached: " << level << "\n";
        std::cout << "    Final Health:        " << player.hp << " / " << player.maxHp << "\n";
        std::cout << "    Weapon Equipped:     " << player.weaponName << " (Bonus: +" << player.weaponBonus << ")\n";
        std::cout << "    Total Gold Looted:   $" << player.gold << "\n";
        std::cout << "    Goblins Slain:       " << player.kills << "\n";
        std::cout << "  ==================================================================\n\n";

        std::cout << "  Press any key to exit...";
        _getch();
        std::cout << "\n";
    }
};

int main() {
    DungeonGame game;
    game.run();
    return 0;
}
