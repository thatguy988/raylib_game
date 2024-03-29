    #include <iostream>
    #include <utility>
    #include <random>
    #include "raylib.h"
    #include "game.h"
    
    
 
    
    #include "../maze/predefinedlevels.h"
    #include "../maze/maze_generator.h"
    #include "../collision/collision.h"
    #include "../camera/player_camera.h"
    #include "../entities/bullet/bullet.h"
    #include "../entities/enemy/enemymanager.h"
    #include "../entities/ammobox/ammobox.h"
    #include "../entities/healthbox/healthbox.h"
    
    #define LIGHTRED        (Color){ 230, 41, 55, 128 }     




   

    namespace GameScreen {
        
        BulletSystem::WeaponType currentWeapon = BulletSystem::WeaponType::PISTOL;
        
        bool isRedFlashActive = false;
        float redFlashDuration = 0.2f; // Duration of the flash in seconds
        float redFlashTimer = 0.0f;    // Timer to track the flash duration
        
        int playerHealth = 100; //health of player
        int maxPlayerHealth = 100; //max health of player

        int level = 1;
        int n;
        int m;
        int maze[MAX][MAX];
        int dist[MAX][MAX];
        
        std::vector<Vector3> openPositions; // Vector to store open positions

        std::vector<Vector3> openPositionsForItems;
        
        
        std::vector<BoundingBox> wallBoundingBoxes;
        BoundingBox endpointBoundingBox;
        
        BoundingBox outOfBoundsBox;



        float wallHeight = 3.0f; // Height of the walls
        float floorThickness = 0.1f; // Thickness of the floor

     
        
        
        
        PlayerCamera playerCamera(0.1f, 4.0f, 2.0f, 1.0f);
        std::pair<int, int> endCoords;
        std::pair<int, int> startCoords; 

        
        BulletSystem::BulletManager bulletManager;
        EnemySystem::EnemyManager enemyManager;
        AmmoSystem::AmmoBoxManager ammoBoxManager;
        HealthSystem::HealthBoxManager healthBoxManager;
        
        
        


        std::pair<int, int> InitializeMaze() {
            auto startEndCoords = MazeGenerator::GenerateMaze(n, m, 70, maze);
            endCoords = startEndCoords.second;
            
            
            // Clear and update openPositions vector
            openPositions.clear();
            for (int i = 0; i < n; ++i) {
                for (int j = 0; j < m; ++j) {
                    if (maze[i][j] == 0) {
                        openPositions.push_back(Vector3{static_cast<float>(j) * playerCamera.blockSize, 0.0f, static_cast<float>(i) * playerCamera.blockSize});
                    }
                }
            }
            return startEndCoords.first;
        }
     
        
        
        
        void updateMazeSize() {
            const int initialBaseSize = 5; // Initial base size
            const int minSize = 5; // Minimum size for 'n' and 'm'
            const int maxSize = 25; // Maximum size for 'n' and 'm'
            const double growthFactor = 1.1; // Growth factor for the maze size per level
            const int levelThreshold = 10; // Level threshold to increase base size
            const int changeRange = 3; // Maximum change (increase or decrease) per level

            static std::mt19937 rng(std::random_device{}()); // Random number generator
            std::uniform_int_distribution<int> sizeChangeDist(-changeRange, changeRange); // Distribution for size change

            int baseSize = initialBaseSize + ((level - 1) / levelThreshold);

            // Independent size calculation for 'n' and 'm'
            int calculatedSizeN = static_cast<int>(baseSize * pow(growthFactor, (level - 1) % levelThreshold));
            int calculatedSizeM = static_cast<int>(baseSize * pow(growthFactor, (level - 1) % levelThreshold));

            // Random size change
            int sizeChangeN = sizeChangeDist(rng);
            int sizeChangeM = sizeChangeDist(rng);

            int newN = std::clamp(calculatedSizeN + sizeChangeN, minSize, maxSize);
            int newM = std::clamp(calculatedSizeM + sizeChangeM, minSize, maxSize);

            n = newN;
            m = newM;

            std::cout << "New maze size: " << n << "x" << m << std::endl;
            std::cout << "Level " << level << std::endl;
        }


        
        
        
        // Function to initialize the out-of-bounds bounding box
        void InitializeOutOfBoundsBox(float mazeWidth, float mazeHeight, float buffer) {
            Vector3 center = {mazeWidth / 2.0f, 0.0f, mazeHeight / 2.0f};
            Vector3 size = {mazeWidth + buffer, wallHeight, mazeHeight + buffer}; // Buffer on each side
            outOfBoundsBox = {
                Vector3Subtract(center, Vector3Scale(size, 0.5f)), 
                Vector3Add(center, Vector3Scale(size, 0.5f))
            };
        }
        
        
        void GenerateWallBoundingBoxes() {
            GameScreen::wallBoundingBoxes.clear();
            for (int i = 0; i < GameScreen::n; ++i) {
                for (int j = 0; j < GameScreen::m; ++j) {
                    if (GameScreen::maze[i][j] == 1 || GameScreen::maze[i][j] == 2) {
                        Vector3 wallPosition = {(float)j * GameScreen::playerCamera.blockSize, 
                                                 GameScreen::wallHeight / 2, 
                                                 (float)i * GameScreen::playerCamera.blockSize};
                        Vector3 halfSize = {GameScreen::playerCamera.blockSize / 2, 
                                            GameScreen::wallHeight / 2, 
                                            GameScreen::playerCamera.blockSize / 2};
                        BoundingBox box = {Vector3Subtract(wallPosition, halfSize), 
                                           Vector3Add(wallPosition, halfSize)};

                        // Distinguish between regular walls and the endpoint
                        if (GameScreen::maze[i][j] == 1) {
                            GameScreen::wallBoundingBoxes.push_back(box);
                        } else if (GameScreen::maze[i][j] == 2) {
                            GameScreen::endpointBoundingBox = box;
                        }
                    }
                }
            }
        }


        std::pair<std::pair<int, int>, std::pair<int, int>> ConvertPredefinedLevelToMaze(const std::vector<std::vector<int>>& predefinedLevel, int maze[MAX][MAX], int& n, int& m) {
            n = predefinedLevel.size();    // Number of rows
            m = predefinedLevel[0].size(); // Number of columns

            std::pair<int, int> startCoords;
            std::pair<int, int> endCoords;

            for (int i = 0; i < n; ++i) {
                for (int j = 0; j < m; ++j) {
                    maze[i][j] = predefinedLevel[i][j];
                    if (maze[i][j] == 3) { // Assuming 3 represents the start point
                        startCoords = {i, j};
                    }
                    if (maze[i][j] == 2) { // Assuming 2 represents the end point
                        endCoords = {i, j};
                    }
                }
            }
            return {startCoords, endCoords};
        }
        
        
        // Method to update and draw the UI elements
        void DrawUI() {
            // Draw the red flash if active
            if (isRedFlashActive) {
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), LIGHTRED);
            }

            // Draw game text
            DrawText("Game Screen", 20, 20, 40, WHITE);
            
            // Display ammo information
            char ammoText[50]; // Buffer to hold the formatted ammo text
            switch (currentWeapon) {
                case BulletSystem::WeaponType::PISTOL:
                    sprintf(ammoText, "Pistol Ammo: %d/%d", bulletManager.PistolAmmo, bulletManager.PistolAmmoCapacity);
                    break;
                case BulletSystem::WeaponType::SHOTGUN:
                    sprintf(ammoText, "Shotgun Ammo: %d/%d", bulletManager.ShotgunAmmo, bulletManager.ShotgunAmmoCapacity);
                    break;
                case BulletSystem::WeaponType::MACHINE_GUN:
                    sprintf(ammoText, "Machine Gun Ammo: %d/%d", bulletManager.MachineGunAmmo, bulletManager.MachineGunAmmoCapacity);
                    break;
                default:
                    // Handling for NONE or other cases, if necessary
                    break;
            }
            DrawText(ammoText, 20, 70, 20, WHITE); // Position the text below "Game Screen"

            // Display health information
            char healthText[50];
            sprintf(healthText, "Health: %d/%d", playerHealth, maxPlayerHealth);
            DrawText(healthText, 20, 100, 20, WHITE);
        }
        
        
        void DrawMaze() {
            for (int i = 0; i < n; ++i) {
                for (int j = 0; j < m; ++j) {
                    if (maze[i][j] == 1) { 
                        // Draw walls
                        DrawCube(Vector3 { (float)j * playerCamera.blockSize, wallHeight / 2, (float)i * playerCamera.blockSize }, 
                                 playerCamera.blockSize, wallHeight, playerCamera.blockSize, DARKGRAY);
                    } else if(maze[i][j] == 0 || maze[i][j] == 3) {
                        // Draw floor
                        DrawCube(Vector3 { (float)j * playerCamera.blockSize, -floorThickness / 2, (float)i * playerCamera.blockSize }, 
                                 playerCamera.blockSize, floorThickness, playerCamera.blockSize, LIGHTGRAY);
                    } else if(maze[i][j] == 2){
                        //Draw endpoint
                        DrawCube(Vector3 { (float)j * playerCamera.blockSize, wallHeight / 2, (float)i * playerCamera.blockSize }, 
                                 playerCamera.blockSize, wallHeight, playerCamera.blockSize, GREEN);
                        // Draw floor
                        DrawCube(Vector3 { (float)j * playerCamera.blockSize, -floorThickness / 2, (float)i * playerCamera.blockSize }, 
                                 playerCamera.blockSize, floorThickness, playerCamera.blockSize, GREEN);
                    }
                }
            }
        }
        
        
        
        void HandleWeaponInputAndShooting(BulletSystem::BulletManager& bulletManager, const PlayerCamera& playerCamera, BulletSystem::WeaponType& currentWeapon) {
            if (IsKeyPressed(KEY_ONE)) currentWeapon = BulletSystem::WeaponType::PISTOL;
            if (IsKeyPressed(KEY_TWO)) currentWeapon = BulletSystem::WeaponType::SHOTGUN;
            if (IsKeyPressed(KEY_THREE)) currentWeapon = BulletSystem::WeaponType::MACHINE_GUN;

            Vector3 bulletDirection = Vector3Normalize(Vector3Subtract(playerCamera.camera.target, playerCamera.camera.position));

            float deltaTime = GetFrameTime(); // Get the time elapsed since the last frame

            // Update timers for all weapons
            bulletManager.machineGunLastShotTime += deltaTime;
            bulletManager.pistolGunLastShotTime += deltaTime;
            bulletManager.shotGunLastShotTime += deltaTime;

            // Handle machine gun shooting
            if (currentWeapon == BulletSystem::WeaponType::MACHINE_GUN && IsKeyDown(KEY_SPACE)) {
                if (bulletManager.machineGunLastShotTime >= bulletManager.machineGunShotDelay) {
                    bulletManager.Shoot(playerCamera.camera.position, bulletDirection, 1.0f, currentWeapon);
                    bulletManager.machineGunLastShotTime = 0.0f; // Reset the timer
                }
            }

            // Handle pistol shooting
            if (currentWeapon == BulletSystem::WeaponType::PISTOL && IsKeyPressed(KEY_SPACE)) {
                if (bulletManager.pistolGunLastShotTime >= bulletManager.pistolGunShotDelay) {
                    bulletManager.Shoot(playerCamera.camera.position, bulletDirection, 0.1f, currentWeapon);
                    bulletManager.pistolGunLastShotTime = 0.0f; // Reset the timer
                }
            }

            // Handle shotgun shooting
            if (currentWeapon == BulletSystem::WeaponType::SHOTGUN && IsKeyPressed(KEY_SPACE)) {
                if (bulletManager.shotGunLastShotTime >= bulletManager.shotGunShotDelay) {
                    bulletManager.Shoot(playerCamera.camera.position, bulletDirection, 1.0f, currentWeapon);
                    bulletManager.shotGunLastShotTime = 0.0f; // Reset the timer
                }
            }
        }
            
        

        

        void InitGame() {
            enemyManager.Reset();
            bulletManager.Reset();
            ammoBoxManager.Reset();
            healthBoxManager.Reset();
            enemyManager.SetRandomMaxEnemies(level);
            ammoBoxManager.SetMaxAmmoBoxes();
            healthBoxManager.SetMaxHealthBoxes();
            
            
            
            
            if (level == 50) {
                auto predefinedLevel = PredefinedLevels::GetLevel(level);
                auto coords = ConvertPredefinedLevelToMaze(predefinedLevel, maze, n, m);
                startCoords = coords.first;
                
                
                // Clear and update openPositions vector
                openPositions.clear();
                for (int i = 0; i < n; ++i) {
                    for (int j = 0; j < m; ++j) {
                        if (maze[i][j] == 0) {
                            openPositions.push_back(Vector3{static_cast<float>(j) * playerCamera.blockSize, 0.0f, static_cast<float>(i) * playerCamera.blockSize});
                        }
                    }
                }
                
                
                playerCamera.InitializeCamera(startCoords);
                openPositionsForItems = openPositions;
                enemyManager.InitializeEnemies(openPositions,level);
                ammoBoxManager.InitializeAmmoBoxes(openPositionsForItems);
                healthBoxManager.InitializeHealthBoxes(openPositionsForItems); 

                GenerateWallBoundingBoxes();
                
                enemyManager.SetWallBoundingBoxes(GameScreen::wallBoundingBoxes);
                
                float mazeWidth = m * playerCamera.blockSize;
                float mazeHeight = n * playerCamera.blockSize;
                InitializeOutOfBoundsBox(mazeWidth, mazeHeight, 10.0f); // 10 units buffer around the maze
                // Additional setup after converting the level
            } else {
                updateMazeSize();
                auto startCoords = InitializeMaze();
                playerCamera.InitializeCamera(startCoords);
                openPositionsForItems = openPositions;
                enemyManager.InitializeEnemies(openPositions, level);
                ammoBoxManager.InitializeAmmoBoxes(openPositionsForItems);
                healthBoxManager.InitializeHealthBoxes(openPositionsForItems); 
                


                GenerateWallBoundingBoxes();
                
                enemyManager.SetWallBoundingBoxes(GameScreen::wallBoundingBoxes);
                
                float mazeWidth = m * playerCamera.blockSize;
                float mazeHeight = n * playerCamera.blockSize;
                InitializeOutOfBoundsBox(mazeWidth, mazeHeight, 10.0f); // 10 units buffer around the maze
            }


        }

        void UpdateGame() {
            
            
            auto exitpoint = playerCamera.UpdateCamera(wallBoundingBoxes, endpointBoundingBox, enemyManager.enemies);
            // Check if level is 50 and if the enemy (Cyberdemon) is defeated
            bool canExitLevel = true;
            if (level == 50) {
                // Assuming there's only one enemy and it's a Cyberdemon
                if (!enemyManager.enemies.empty() && enemyManager.enemies[0]->active) {
                    canExitLevel = false; // Can't exit if the Cyberdemon is still active
                }
            }

            if (exitpoint == 2 && IsKeyPressed(KEY_R) && canExitLevel) {
                level += 1;
                InitGame();
            }
            
            
            
            
            
            HandleWeaponInputAndShooting(bulletManager, playerCamera, currentWeapon);
            
            bulletManager.UpdateBullets(wallBoundingBoxes, endpointBoundingBox);
            
            bulletManager.CheckBulletOutOfBounds(outOfBoundsBox);

            
            CollisionHandling::CheckBulletEnemyCollision(bulletManager.bullets, bulletManager.MAX_BULLETS, enemyManager.enemies);


            
            // Check for bullet-player collision
            if (CollisionHandling::CheckBulletPlayerCollision(bulletManager.bullets, bulletManager.MAX_BULLETS, playerCamera.playerBody, playerHealth)) {
                isRedFlashActive = true; // Activate the red flash
                redFlashTimer = redFlashDuration; // Reset the timer
                
            }
            
                

            
            // check collision between player and ammo box
            CollisionHandling::CheckPlayerAmmoBoxCollision(playerCamera.playerBody, ammoBoxManager.ammoBoxes, 
            bulletManager.PistolAmmo, bulletManager.PistolAmmoCapacity, 
            bulletManager.ShotgunAmmo, bulletManager.ShotgunAmmoCapacity, 
            bulletManager.MachineGunAmmo, bulletManager.MachineGunAmmoCapacity);
            
            

            //check collision between player and health box
            CollisionHandling:: CheckPlayerHealthBoxCollision(playerCamera.playerBody, healthBoxManager.healthBoxes, playerHealth, maxPlayerHealth);
            
                


            // Update the red flash timer
            if (isRedFlashActive) {
                redFlashTimer -= GetFrameTime();
                if (redFlashTimer <= 0) {
                    isRedFlashActive = false; // Deactivate the red flash
                }
            }
            
            enemyManager.UpdateEnemies(playerCamera.camera.position, maze, n, m, GameScreen::playerCamera.blockSize, openPositions, bulletManager, playerCamera.playerBody);
            
            
            
            
        }
        
        

    void DrawGame() {
        ClearBackground(BLACK);
        
        BeginMode3D(GameScreen::playerCamera.camera);
        
        
        
        DrawMaze();
        enemyManager.DrawEnemies();
        bulletManager.DrawBullets();
        ammoBoxManager.DrawAmmoBoxes();
        healthBoxManager.DrawHealthBoxes();
        EndMode3D();
        
        DrawUI();
 
    }
    

    void UnloadGame() {
        enemyManager.Reset();
        bulletManager.Reset();
        ammoBoxManager.Reset();
        healthBoxManager.Reset();
        level = 1;
       

    }
    

} // namespace GameScreen