#include "raylib.h"
#include <cmath>
#include <cstdio>

const int SCREEN_W = 1100;
const int SCREEN_H = 680;
const int WIN_SCORE = 5;
const int MAX_PARTICLES = 180;
const int STAR_COUNT = 90;

enum GameState { MENU, PLAYING, PAUSED, SETTINGS, GAME_OVER };

struct Particle {
    Vector2 pos;
    Vector2 vel;
    float life;
    Color color;
};

struct Star {
    float x, y, speed, size;
};
// Detect one tap instead of repeating while the finger is held.
bool IsScreenTapped()
{
    static bool wasTouching = false;

    bool touching = GetTouchPointCount() > 0;
    bool tapped = touching && !wasTouching;

    wasTouching = touching;
    return tapped;
}
enum PlayerMode {
    SINGLE_PLAYER,
    TWO_PLAYERS
};

enum Difficulty {
    EASY,
    MEDIUM,
    HARD
};

PlayerMode selectedMode = SINGLE_PLAYER;
Difficulty selectedDifficulty = MEDIUM;
int main()
{
    InitWindow(SCREEN_W, SCREEN_H, "NEON PONG - Ultimate Edition");
    InitAudioDevice();
    SetTargetFPS(60);

    Color bg       = {7, 10, 25, 255};
    Color cyan     = {0, 235, 255, 255};
    Color pink     = {255, 55, 150, 255};
    Color purple   = {155, 90, 255, 255};
    Color muted    = {85, 105, 145, 255};
    Color panel    = {15, 23, 48, 245};

    // Generate sound effects in memory. No external audio files needed.
    auto makeTone = [](float frequency, float duration, float volume) {
        Wave wave = {0};
        wave.frameCount = (unsigned int)(44100 * duration);
        wave.sampleRate = 44100;
        wave.sampleSize = 16;
        wave.channels = 1;

        short *samples = (short *)MemAlloc(
                wave.frameCount * sizeof(short));

        for (unsigned int i = 0; i < wave.frameCount; i++) {
            float t = (float)i / 44100.0f;
            float envelope = 1.0f - (float)i / wave.frameCount;
            float sample = sinf(2.0f * PI * frequency * t)
                           * volume * envelope;
            samples[i] = (short)(sample * 32767.0f);
        }

        wave.data = samples;
        Sound result = LoadSoundFromWave(wave);
        UnloadWave(wave);
        return result;
    };

    Sound hitSound   = makeTone(620, 0.09f, 0.55f);
    Sound wallSound  = makeTone(330, 0.07f, 0.35f);
    Sound scoreSound = makeTone(880, 0.24f, 0.55f);
    Sound winSound   = makeTone(1100, 0.40f, 0.55f);

    bool soundOn = true;
    bool twoPlayer = false;
    float speedMultiplier = 1.0f;

    GameState state = MENU;

    Rectangle player = {60, 285, 20, 110};
    Rectangle enemy  = {1020, 285, 20, 110};

    Vector2 ball = {SCREEN_W / 2.0f, SCREEN_H / 2.0f};
    Vector2 ballVel = {430, 160};
    float radius = 11;
    float rallySpeed = 460;

    int playerScore = 0;
    int enemyScore = 0;
    int highScore = 0;

    // Load the saved high score, if it exists.
    FILE *file = fopen("highscore.txt", "r");
    if (file) {
        fscanf(file, "%d", &highScore);
        fclose(file);
    }

    Particle particles[MAX_PARTICLES] = {};
    Star stars[STAR_COUNT] = {};

    for (int i = 0; i < STAR_COUNT; i++) {
        stars[i].x = (float)GetRandomValue(0, SCREEN_W);
        stars[i].y = (float)GetRandomValue(0, SCREEN_H);
        stars[i].speed = (float)GetRandomValue(8, 35);
        stars[i].size = (float)GetRandomValue(1, 3);
    }

    auto play = [&](Sound sound) {
        if (soundOn && IsAudioDeviceReady())
            PlaySound(sound);
    };

    auto saveHighScore = [&]() {
        int score = playerScore > enemyScore
                    ? playerScore : enemyScore;

        if (score > highScore) {
            highScore = score;
            FILE *f = fopen("highscore.txt", "w");
            if (f) {
                fprintf(f, "%d\n", highScore);
                fclose(f);
            }
        }
    };

    auto resetBall = [&](float direction) {
        ball = {(float)SCREEN_W / 2, (float)SCREEN_H / 2};
        rallySpeed = 460 * speedMultiplier;
        ballVel = {rallySpeed * direction, 130};
    };

    auto resetMatch = [&]() {
        playerScore = 0;
        enemyScore = 0;
        player.y = SCREEN_H / 2.0f - player.height / 2;
        enemy.y = SCREEN_H / 2.0f - enemy.height / 2;
        resetBall(1);
    };

    auto burst = [&](Vector2 pos, Color color) {
        for (int n = 0; n < 24; n++) {
            for (int i = 0; i < MAX_PARTICLES; i++) {
                if (particles[i].life <= 0) {
                    float a = GetRandomValue(0, 359) * DEG2RAD;
                    float s = (float)GetRandomValue(70, 290);

                    particles[i].pos = pos;
                    particles[i].vel = {
                            cosf(a) * s, sinf(a) * s
                    };
                    particles[i].life = 0.75f;
                    particles[i].color = color;
                    break;
                }
            }
        }
    };

    // Menu button rectangles use the game's 1100 x 680 coordinate system.
    Rectangle singleButton = {260, 275, 260, 58};
    Rectangle twoPlayerButton = {550, 275, 260, 58};
    Rectangle easyButton = {260, 355, 150, 55};
    Rectangle mediumButton = {475, 355, 150, 55};
    Rectangle hardButton = {690, 355, 150, 55};
    Rectangle startButton = {400, 445, 300, 62};
    Rectangle settingsButton = {450, 525, 200, 42};
    Rectangle pauseButton = {SCREEN_W - 150, 18, 125, 48};
    Rectangle resumeButton = {400, 320, 300, 60};
    Rectangle pauseMenuButton = {400, 400, 300, 55};
    Rectangle restartButton = {400, 350, 300, 55};
    Rectangle gameOverMenuButton = {400, 425, 300, 55};

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        // Animated background
        for (int i = 0; i < STAR_COUNT; i++) {
            stars[i].x -= stars[i].speed * dt;
            if (stars[i].x < 0) {
                stars[i].x = SCREEN_W;
                stars[i].y = GetRandomValue(0, SCREEN_H);
            }
        }

        // Update particles
        for (int i = 0; i < MAX_PARTICLES; i++) {
            if (particles[i].life > 0) {
                particles[i].pos.x += particles[i].vel.x * dt;
                particles[i].pos.y += particles[i].vel.y * dt;
                particles[i].life -= dt;
            }
        }

        // ================= MENU =================
        if (state == MENU) {
            if (IsKeyPressed(KEY_ONE)) {
                selectedMode = SINGLE_PLAYER;
                twoPlayer = false;
            }
            if (IsKeyPressed(KEY_TWO)) {
                selectedMode = TWO_PLAYERS;
                twoPlayer = true;
            }
            if (IsKeyPressed(KEY_F1)) selectedDifficulty = EASY;
            if (IsKeyPressed(KEY_F2)) selectedDifficulty = MEDIUM;
            if (IsKeyPressed(KEY_F3)) selectedDifficulty = HARD;

            bool mouseTap = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
            bool tapped = mouseTap || IsScreenTapped();
            Vector2 tapPosition = mouseTap ? GetMousePosition() : GetTouchPosition(0);

            if (tapped) {
                if (CheckCollisionPointRec(tapPosition, singleButton)) {
                    selectedMode = SINGLE_PLAYER;
                    twoPlayer = false;
                } else if (CheckCollisionPointRec(tapPosition, twoPlayerButton)) {
                    selectedMode = TWO_PLAYERS;
                    twoPlayer = true;
                } else if (CheckCollisionPointRec(tapPosition, easyButton)) {
                    selectedDifficulty = EASY;
                } else if (CheckCollisionPointRec(tapPosition, mediumButton)) {
                    selectedDifficulty = MEDIUM;
                } else if (CheckCollisionPointRec(tapPosition, hardButton)) {
                    selectedDifficulty = HARD;
                } else if (CheckCollisionPointRec(tapPosition, startButton)) {
                    twoPlayer = (selectedMode == TWO_PLAYERS);
                    resetMatch();
                    state = PLAYING;
                } else if (CheckCollisionPointRec(tapPosition, settingsButton)) {
                    state = SETTINGS;
                }
            }

            if (IsKeyPressed(KEY_ENTER)) {
                twoPlayer = (selectedMode == TWO_PLAYERS);
                resetMatch();
                state = PLAYING;
            }
            if (IsKeyPressed(KEY_S)) state = SETTINGS;
        }

            // ================= SETTINGS =================
        else if (state == SETTINGS) {
            if (IsKeyPressed(KEY_LEFT))
                speedMultiplier -= 0.1f;

            if (IsKeyPressed(KEY_RIGHT))
                speedMultiplier += 0.1f;

            if (speedMultiplier < 0.6f)
                speedMultiplier = 0.6f;

            if (speedMultiplier > 1.6f)
                speedMultiplier = 1.6f;

            if (IsKeyPressed(KEY_M))
                soundOn = !soundOn;

            if (IsKeyPressed(KEY_ESCAPE) ||
                IsKeyPressed(KEY_ENTER))
                state = MENU;
        }

            // ================= PLAYING =================
        else if (state == PLAYING) {
            bool mouseTap = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
            bool tapped = mouseTap || IsScreenTapped();
            Vector2 tapPosition = mouseTap ? GetMousePosition() : GetTouchPosition(0);
            if (tapped && CheckCollisionPointRec(tapPosition, pauseButton))
                state = PAUSED;
            if (IsKeyPressed(KEY_P)) state = PAUSED;

            if (state == PLAYING) {
                float playerSpeed = 500 * speedMultiplier;

                // Mobile touch controls: in 1-player mode, any touch controls
                // the left paddle. In 2-player mode, touch the left/right half
                // of the screen to control the corresponding paddle.
                if (GetTouchPointCount() > 0) {
                    Vector2 touch = GetTouchPosition(0);
                    if (twoPlayer && touch.x >= SCREEN_W / 2) {
                        enemy.y = touch.y - enemy.height / 2;
                    } else {
                        player.y = touch.y - player.height / 2;
                    }
                } else {
                    // Optional desktop keyboard controls.
                    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))
                        player.y -= playerSpeed * dt;
                    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))
                        player.y += playerSpeed * dt;
                    if (twoPlayer) {
                        if (IsKeyDown(KEY_I)) enemy.y -= playerSpeed * dt;
                        if (IsKeyDown(KEY_K)) enemy.y += playerSpeed * dt;
                    }
                }

                // Keep both paddles inside the screen.
                if (player.y < 0) player.y = 0;
                if (player.y + player.height > SCREEN_H)
                    player.y = SCREEN_H - player.height;

                else {
                    // AI with limited reaction and speed.


                    float aiSpeed = 330.0f;

                    if (selectedDifficulty == EASY)
                        aiSpeed = 180.0f;
                    else if (selectedDifficulty == MEDIUM)
                        aiSpeed = 330.0f;
                    else if (selectedDifficulty == HARD)
                        aiSpeed = 480.0f;
                    float target = ball.y - enemy.height / 2;

                    if (selectedDifficulty == EASY)
                        target += GetRandomValue(-70, 70);

                    float center = enemy.y + enemy.height / 2;

                    if (center < target - 10)
                        enemy.y += aiSpeed * dt;
                    else if (center > target + 10)
                        enemy.y -= aiSpeed * dt;
                }

                if (enemy.y < 0) enemy.y = 0;
                if (enemy.y + enemy.height > SCREEN_H)
                    enemy.y = SCREEN_H - enemy.height;

                // Move the ball.
                ball.x += ballVel.x * dt;
                ball.y += ballVel.y * dt;

                // Top/bottom walls.
                if (ball.y - radius <= 0) {
                    ball.y = radius;
                    ballVel.y = fabsf(ballVel.y);
                    play(wallSound);
                    burst(ball, purple);
                }

                if (ball.y + radius >= SCREEN_H) {
                    ball.y = SCREEN_H - radius;
                    ballVel.y = -fabsf(ballVel.y);
                    play(wallSound);
                    burst(ball, purple);
                }

                // Player paddle collision.
                if (CheckCollisionCircleRec(
                        ball, radius, player) && ballVel.x < 0) {
                    ball.x = player.x + player.width + radius;

                    float offset =
                            (ball.y -
                             (player.y + player.height / 2)) /
                            (player.height / 2);

                    rallySpeed = fminf(
                            rallySpeed + 18, 850 * speedMultiplier);

                    float angle = offset * 1.0f;
                    ballVel.x = fabsf(rallySpeed * cosf(angle));
                    ballVel.y = rallySpeed * sinf(angle);

                    play(hitSound);
                    burst(ball, cyan);
                }

                // Computer or second-player collision.
                if (CheckCollisionCircleRec(
                        ball, radius, enemy) && ballVel.x > 0) {
                    ball.x = enemy.x - radius;

                    float offset =
                            (ball.y -
                             (enemy.y + enemy.height / 2)) /
                            (enemy.height / 2);

                    rallySpeed = fminf(
                            rallySpeed + 18, 850 * speedMultiplier);

                    float angle = offset * 1.0f;
                    ballVel.x = -fabsf(rallySpeed * cosf(angle));
                    ballVel.y = rallySpeed * sinf(angle);

                    play(hitSound);
                    burst(ball, pink);
                }

                // Computer/second player scores.
                if (ball.x + radius < 0) {
                    enemyScore++;
                    play(scoreSound);
                    burst(
                            {(float)SCREEN_W / 2,
                             (float)SCREEN_H / 2}, pink);

                    saveHighScore();

                    if (enemyScore >= WIN_SCORE) {
                        state = GAME_OVER;
                        play(winSound);
                    }
                    else resetBall(-1);
                }

                // Player scores.
                if (ball.x - radius > SCREEN_W) {
                    playerScore++;
                    play(scoreSound);
                    burst(
                            {(float)SCREEN_W / 2,
                             (float)SCREEN_H / 2}, cyan);

                    saveHighScore();

                    if (playerScore >= WIN_SCORE) {
                        state = GAME_OVER;
                        play(winSound);
                    }
                    else resetBall(1);
                }
            }
        }

            // ================= PAUSED =================
        else if (state == PAUSED) {
            bool mouseTap = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
            bool tapped = mouseTap || IsScreenTapped();
            Vector2 tapPosition = mouseTap ? GetMousePosition() : GetTouchPosition(0);
            if (tapped) {
                if (CheckCollisionPointRec(tapPosition, resumeButton)) state = PLAYING;
                else if (CheckCollisionPointRec(tapPosition, pauseMenuButton)) state = MENU;
            }
            if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ENTER)) state = PLAYING;
            if (IsKeyPressed(KEY_M)) state = MENU;
        }

            // ================= GAME OVER =================
        else if (state == GAME_OVER) {
            bool mouseTap = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
            bool tapped = mouseTap || IsScreenTapped();
            Vector2 tapPosition = mouseTap ? GetMousePosition() : GetTouchPosition(0);
            if (tapped) {
                if (CheckCollisionPointRec(tapPosition, restartButton)) {
                    resetMatch();
                    state = PLAYING;
                } else if (CheckCollisionPointRec(tapPosition, gameOverMenuButton)) {
                    state = MENU;
                }
            }
            if (IsKeyPressed(KEY_R)) {
                resetMatch();
                state = PLAYING;
            }
            if (IsKeyPressed(KEY_M)) state = MENU;
        }
        // ================= DRAW =================
        BeginDrawing();
        ClearBackground(bg);

        // Moving stars
        for (int i = 0; i < STAR_COUNT; i++)
            DrawCircleV(
                    {stars[i].x, stars[i].y},
                    stars[i].size, muted);

        // Decorative center line.
        for (int y = 0; y < SCREEN_H; y += 28)
            DrawRectangle(
                    SCREEN_W / 2 - 2, y, 4, 14, muted);

        // Scoreboard
        DrawText(TextFormat("%d", playerScore),
                 425, 28, 56, cyan);

        DrawText(TextFormat("%d", enemyScore),
                 625, 28, 56, pink);

        DrawText("PLAYER 1", 45, 105, 16, cyan);
        DrawText(twoPlayer ? "PLAYER 2" : "CPU",
                 970, 105, 16, pink);

        DrawText(TextFormat("BEST: %d", highScore),
                 15, 145, 17, RAYWHITE);

        // Neon paddles.
        DrawRectangle(
                player.x - 4, player.y - 4,
                player.width + 8, player.height + 8,
                Fade(cyan, 0.22f));

        DrawRectangleRec(player, cyan);

        DrawRectangle(
                enemy.x - 4, enemy.y - 4,
                enemy.width + 8, enemy.height + 8,
                Fade(pink, 0.22f));

        DrawRectangleRec(enemy, pink);

        // Ball glow.
        DrawCircleV(ball, radius + 9, Fade(purple, 0.3f));
        DrawCircleV(ball, radius + 4, purple);
        DrawCircleV(ball, radius, RAYWHITE);

        // Particles.
        for (int i = 0; i < MAX_PARTICLES; i++) {
            if (particles[i].life > 0) {
                DrawCircleV(
                        particles[i].pos,
                        4 * particles[i].life,
                        Fade(particles[i].color,
                             particles[i].life / 0.75f));
            }
        }

        if (state == PLAYING) {
            DrawRectangleRec(pauseButton, panel);
            DrawText("PAUSE", (int)pauseButton.x + 27, (int)pauseButton.y + 14, 20, RAYWHITE);
            if (twoPlayer)
                DrawText("TOUCH LEFT/RIGHT HALF TO MOVE EACH PADDLE", 270, SCREEN_H - 32, 17, muted);
            else
                DrawText("DRAG FINGER UP/DOWN TO MOVE", 360, SCREEN_H - 32, 17, muted);
        }

        // Menu: touch-friendly player and difficulty selection.
        if (state == MENU) {
            DrawRectangle(160, 100, 780, 500, panel);

            DrawText("NEON PONG", 365, 125, 58, cyan);
            DrawText("ULTIMATE EDITION", 425, 190, 20, pink);
            DrawText("SELECT PLAYERS", 445, 235, 22, RAYWHITE);

            DrawRectangleRec(singleButton,
                             selectedMode == SINGLE_PLAYER ? DARKGREEN : DARKGRAY);
            DrawText("1 PLAYER", 325, 293, 22, RAYWHITE);

            DrawRectangleRec(twoPlayerButton,
                             selectedMode == TWO_PLAYERS ? DARKGREEN : DARKGRAY);
            DrawText("2 PLAYERS", 620, 293, 22, RAYWHITE);

            DrawText("DIFFICULTY", 475, 335, 20, YELLOW);

            DrawRectangleRec(easyButton,
                             selectedDifficulty == EASY ? GREEN : DARKGRAY);
            DrawText("EASY", 310, 373, 20, RAYWHITE);

            DrawRectangleRec(mediumButton,
                             selectedDifficulty == MEDIUM ? ORANGE : DARKGRAY);
            DrawText("MEDIUM", 495, 373, 20, RAYWHITE);

            DrawRectangleRec(hardButton,
                             selectedDifficulty == HARD ? RED : DARKGRAY);
            DrawText("HARD", 735, 373, 20, RAYWHITE);

            DrawRectangleRec(startButton, purple);
            DrawText("TAP TO START", 455, 465, 24, RAYWHITE);

            DrawRectangleRec(settingsButton, panel);
            DrawText("SETTINGS", 505, 535, 18, muted);
            DrawText("Keyboard: 1/2 players, F1/F2/F3 difficulty",
                     285, 570, 16, muted);
        }

        // Settings.
        if (state == SETTINGS) {
            DrawRectangle(200, 180, 700, 320, panel);

            DrawText("SETTINGS", 410, 205, 42, cyan);

            DrawText(TextFormat("GAME SPEED: %.1fx",
                                speedMultiplier),
                     350, 280, 26, RAYWHITE);

            DrawText("LEFT / RIGHT: CHANGE SPEED",
                     310, 330, 20, YELLOW);

            DrawText(TextFormat("SOUND: %s",
                                soundOn ? "ON" : "OFF"),
                     390, 375, 24, pink);

            DrawText("M: TOGGLE SOUND",
                     385, 415, 20, RAYWHITE);

            DrawText("ENTER / ESC: BACK",
                     375, 460, 20, muted);
        }

        // Touch-friendly pause overlay.
        if (state == PAUSED) {
            DrawRectangle(250, 210, 600, 280, panel);
            DrawText("PAUSED", 425, 235, 48, YELLOW);
            DrawRectangleRec(resumeButton, purple);
            DrawText("RESUME", 490, 339, 24, RAYWHITE);
            DrawRectangleRec(pauseMenuButton, panel);
            DrawText("MAIN MENU", 475, 416, 22, cyan);
        }

        // Touch-friendly game-over overlay.
        if (state == GAME_OVER) {
            DrawRectangle(200, 180, 700, 340, panel);
            if (playerScore >= WIN_SCORE)
                DrawText("PLAYER 1 WINS!", 330, 205, 45, cyan);
            else
                DrawText(twoPlayer ? "PLAYER 2 WINS!" : "CPU WINS!", 330, 205, 45, pink);
            DrawText(TextFormat("FINAL SCORE: %d - %d", playerScore, enemyScore),
                     380, 275, 26, RAYWHITE);
            DrawRectangleRec(restartButton, purple);
            DrawText("PLAY AGAIN", 485, 367, 23, RAYWHITE);
            DrawRectangleRec(gameOverMenuButton, panel);
            DrawText("MAIN MENU", 480, 443, 22, cyan);
        }
        EndDrawing();
    }

    UnloadSound(hitSound);
    UnloadSound(wallSound);
    UnloadSound(scoreSound);
    UnloadSound(winSound);

    CloseAudioDevice();
    CloseWindow();

    return 0;
}
