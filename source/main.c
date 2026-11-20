// COW GAME 3 - Nintendo DS/DSi homebrew platformer (BlocksDS / libnds)
// D-Pad: move | L: sprint | A: jump (or return to title when paused)
// B / Y / touch LICK: lick | R / touch SPIT: spit (recharging)
// X: toggle Hard Mode | SELECT / touch RADIO: toggle radio | START / touch PAUSE: pause
#include <nds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "gfx/splash.h"
#include "gfx/cow.h"
#include "gfx/lemon.h"
#include "gfx/cloud.h"
#include "gfx/coin.h"
#include "gfx/btn_pause.h"
#include "gfx/btn_lick.h"
#include "gfx/btn_radio.h"
#include "gfx/btn_spit.h"
#include "gfx/sky.h"
#include "gfx/dirt.h"
#include "gfx/logo.h"

#define SW 256
#define SH 192
#define TS 16
#define MW 64
#define MH 12
#define PW 18
#define PH 28
#define NE 8
#define MAX_SPITS 4
#define MAX_ENEMY_SPITS 6

#define BOOT_FRAMES 120          // ~2 seconds at 60 FPS
#define COYOTE_FRAMES 6
#define JUMP_BUFFER_FRAMES 6
#define SPIT_MAX_CHARGE 84       // ~1.4 seconds full recharge at 60 FPS
#define SPIT_COOLDOWN_FRAMES 10

// Hard mode enemy timers (at 60 FPS)
#define ENEMY_GRACE_FRAMES 120   // 2.0s grace period so enemies cannot spawn-camp
#define ENEMY_LICK_REGEN 150     // 2.5s lick recharge
#define ENEMY_LICK_ACTIVE 14
#define ENEMY_SPIT_REGEN 180     // 3.0s spit recharge

// Audio synthesis settings (8-bit signed PCM loop, ~11025 Hz, 4.0s loop)
#define AUDIO_RATE 11025
#define AUDIO_SECONDS 4
#define AUDIO_SAMPLES (AUDIO_RATE * AUDIO_SECONDS)

#define C(r,g,b) (RGB15(r,g,b) | BIT(15))

// Palette
#define SKY_TOP   C(8, 16, 30)
#define SKY_BOT   C(18, 25, 31)
#define WHT       C(31, 31, 31)
#define BLK       C(2, 2, 3)
#define YEL       C(31, 29, 3)
#define LEMON_FL  C(31, 31, 19)
#define PINK      C(31, 19, 20)
#define DIRT_HI   C(19, 12, 6)
#define DIRT_MD   C(16, 10, 5)
#define DIRT_DK   C(12, 7, 3)
#define GRASS_HI  C(12, 26, 8)
#define GRASS_MD  C(8, 21, 6)
#define PLAT_HI   C(26, 28, 31)
#define PLAT_MD   C(18, 20, 27)
#define PLAT_DK   C(12, 14, 20)
#define GOLD      C(31, 24, 2)
#define SUBBG     C(4, 8, 18)
#define LICKC     C(30, 12, 13)
#define TONGUE    C(31, 10, 12)
#define HARD_RED  C(31, 7, 7)

static u16 *buf  = NULL;
static u16 *sbuf = NULL;
static s8  *radioPcm = NULL;
static int  radioChannel = -1;
static int  radioMuted = 0;
static int  dsiMode = 0;

// ---- tiny 3x5 font (each row is an octal digit = 3 pixels) ----
#define G(a,b,c,d,e) (((a)<<12)|((b)<<9)|((c)<<6)|((d)<<3)|(e))
typedef struct { char c; u16 g; } Glyph;
static const Glyph font[] = {
    {'A',G(2,5,7,5,5)},{'B',G(6,5,6,5,6)},{'C',G(3,4,4,4,3)},{'D',G(6,5,5,5,6)},
    {'E',G(7,4,6,4,7)},{'F',G(7,4,6,4,4)},{'G',G(3,4,5,5,3)},{'H',G(5,5,7,5,5)},
    {'I',G(7,2,2,2,7)},{'J',G(1,1,1,5,2)},{'K',G(5,5,6,5,5)},{'L',G(4,4,4,4,7)},
    {'M',G(5,7,7,5,5)},{'N',G(6,5,5,5,5)},{'O',G(2,5,5,5,2)},{'P',G(6,5,6,4,4)},
    {'R',G(6,5,6,5,5)},{'S',G(3,4,2,1,6)},{'T',G(7,2,2,2,2)},{'U',G(5,5,5,5,7)},
    {'V',G(5,5,5,5,2)},{'W',G(5,5,7,7,5)},{'X',G(5,5,2,5,5)},{'Y',G(5,5,2,2,2)},
    {'0',G(7,5,5,5,7)},{'1',G(2,6,2,2,7)},{'2',G(6,1,2,4,7)},{'3',G(6,1,2,1,6)},
    {'4',G(5,5,7,1,1)},{'5',G(7,4,6,1,6)},{'6',G(3,4,7,5,7)},{'7',G(7,1,2,2,2)},
    {'8',G(7,5,7,5,7)},{'9',G(7,5,7,1,6)},
    {':',G(0,2,0,2,0)},{'-',G(0,0,7,0,0)},{'/',G(1,1,2,4,4)},{'!',G(2,2,2,0,2)},
    {'.',G(0,0,0,0,2)},{' ',G(0,0,0,0,0)}
};

static inline u32 hash2d(int x, int y) {
    u32 h = (u32)(x * 374761393u + y * 668265263u);
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static inline void px_set(u16 *b, int x, int y, u16 c) {
    if ((unsigned)x < SW && (unsigned)y < SH) {
        b[y * SW + x] = c;
    }
}

static void rect(u16 *b, int x, int y, int w, int h, u16 c) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SW) w = SW - x;
    if (y + h > SH) h = SH - y;
    if (w <= 0 || h <= 0) return;
    for (int j = 0; j < h; j++) {
        u16 *p = b + (y + j) * SW + x;
        for (int i = 0; i < w; i++) p[i] = c;
    }
}

static void drawBmp16(u16 *dst, const u16 *src, int w, int h, int x, int y, int flip) {
    for (int j = 0; j < h; j++) {
        int dy = y + j;
        if ((unsigned)dy >= SH) continue;
        for (int i = 0; i < w; i++) {
            int dx = x + i;
            if ((unsigned)dx >= SW) continue;
            int sx = flip ? (w - 1 - i) : i;
            u16 col = src[j * w + sx];
            if (col & BIT(15)) {
                dst[dy * SW + dx] = col;
            }
        }
    }
}

static void text(u16 *b, const char *s, int x, int y, int sc, u16 col) {
    for (; *s; s++) {
        u16 g = 0;
        for (unsigned k = 0; k < sizeof(font) / sizeof(font[0]); k++) {
            if (font[k].c == *s) { g = font[k].g; break; }
        }
        for (int r = 0; r < 5; r++) {
            for (int q = 0; q < 3; q++) {
                if (g & (1 << ((4 - r) * 3 + (2 - q)))) {
                    rect(b, x + q * sc, y + r * sc, sc, sc, col);
                }
            }
        }
        x += 4 * sc;
    }
}

// ---- 1D Perlin-style gradient noise for seeded individual levels ----
static float smoothstep_f(float t) {
    return t * t * (3.0f - 2.0f * t);
}

static float grad1d(int ix, u32 seed) {
    u32 h = (u32)ix * 374761393u ^ (seed * 668265263u + 0x9E3779B9u);
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= (h >> 16);
    return ((float)(h & 0xFFFFu) / 32767.5f) - 1.0f;
}

static float perlin1d(float x, u32 seed) {
    int x0 = (int)floorf(x);
    int x1 = x0 + 1;
    float frac = x - (float)x0;
    float u = smoothstep_f(frac);

    float n0 = grad1d(x0, seed) * frac;
    float n1 = grad1d(x1, seed) * (frac - 1.0f);
    return n0 + u * (n1 - n0);
}

static float fbm1d(float x, u32 seed) {
    float sum = 0.0f;
    float amp = 0.65f;
    float freq = 1.0f;
    float norm = 0.0f;
    for (int o = 0; o < 2; o++) {
        sum += perlin1d(x * freq, seed + (u32)(o * 7919)) * amp;
        norm += amp;
        amp *= 0.45f;
        freq *= 2.05f;
    }
    return sum / norm;
}

// ---- Bit-crushed Portal-radio chiptune generator ----
static float midi_to_freq(int midi) {
    return 440.0f * powf(2.0f, (midi - 69) / 12.0f);
}

static void buildRadioLoop(void) {
    radioPcm = (s8 *)malloc(AUDIO_SAMPLES);
    if (!radioPcm) return;

    static const int lead_notes[16] = {
        62, 65, 69, 74,  72, 69, 67, 65,
        62, 65, 70, 74,  76, 74, 69, 65
    };
    static const int bass_notes[16] = {
        38, 38, 38, 38,  41, 41, 43, 43,
        38, 38, 46, 46,  45, 45, 43, 41
    };

    const int steps = 16;
    const int samples_per_step = AUDIO_SAMPLES / steps;
    const int hold_factor = 7;
    const int bit_levels = 32;

    float lead_phase = 0.0f;
    float bass_phase = 0.0f;
    float held_sample = 0.0f;
    u32 rng = 0x504F5254u;

    for (int n = 0; n < AUDIO_SAMPLES; n++) {
        int step = (n / samples_per_step) % steps;
        int step_pos = n % samples_per_step;
        float env = 1.0f - 0.55f * ((float)step_pos / (float)samples_per_step);

        float f_lead = midi_to_freq(lead_notes[step]);
        float f_bass = midi_to_freq(bass_notes[step]);

        lead_phase += f_lead / (float)AUDIO_RATE;
        if (lead_phase >= 1.0f) lead_phase -= 1.0f;

        bass_phase += f_bass / (float)AUDIO_RATE;
        if (bass_phase >= 1.0f) bass_phase -= 1.0f;

        float pulse = (lead_phase < 0.25f) ? 0.65f : -0.65f;
        float tri = (bass_phase < 0.5f)
            ? (-1.0f + 4.0f * bass_phase)
            : (3.0f - 4.0f * bass_phase);

        rng = rng * 1664525u + 1013904223u;
        float noise = (((int)(rng >> 24)) - 128) / 128.0f;

        float raw = (pulse * 0.42f * env) + (tri * 0.35f) + (noise * 0.07f);

        if ((n % hold_factor) == 0) {
            float crushed = floorf(raw * (float)bit_levels + 0.5f) / (float)bit_levels;
            held_sample = crushed / (1.0f + 0.35f * fabsf(crushed));
        }

        int s = (int)(held_sample * 96.0f);
        if (s > 110) s = 110;
        if (s < -110) s = -110;
        radioPcm[n] = (s8)s;
    }
}

static void startRadioLoop(void) {
    if (!radioPcm || radioMuted) return;
    if (radioChannel >= 0) {
        soundKill(radioChannel);
        radioChannel = -1;
    }
    radioChannel = soundPlaySample(
        radioPcm,
        SoundFormat_8Bit,
        AUDIO_SAMPLES,
        AUDIO_RATE,
        58,
        64,
        true,
        0
    );
}

static void toggleRadio(void) {
    radioMuted = !radioMuted;
    if (radioMuted) {
        if (radioChannel >= 0) {
            soundKill(radioChannel);
            radioChannel = -1;
        }
    } else {
        startRadioLoop();
    }
}

// ---- Game state ----
typedef struct {
    int x, y, dir, alive, anim;
    int graceTimer;
    int lickCooldown;
    int lickTimer;
    int spitCooldown;
} Enemy;

typedef struct { int x, y, vx, active; } SpitProj;

static u8 map[MH][MW];
static Enemy en[NE];
static SpitProj spits[MAX_SPITS];
static SpitProj enemySpits[MAX_ENEMY_SPITS];
static int ne;
static int x16, y16, vx16, vy16, face, ground, inv, lick;
static int spitCharge, spitCooldown;
static int coyoteTimer, jumpBufferTimer;
static int score, health, state, paused, frame, bootTimer;
static int levelNum;
static u32 baseSeed, levelSeed;
static int hardMode = 0;
// state: -1 boot splash (2s), 0 title, 1 playing, 3 level clear, 4 game over

static void clearSpits(void) {
    for (int i = 0; i < MAX_SPITS; i++) {
        spits[i].active = 0;
    }
    for (int i = 0; i < MAX_ENEMY_SPITS; i++) {
        enemySpits[i].active = 0;
    }
}

static u32 rollNewBaseSeed(void) {
    u32 s = (u32)(frame * 2654435761u) ^ (u32)(score * 2246822519u) ^ 0x5EED1234u;
    if (s == 0) s = 0xC0FFEE42u;
    return s;
}

static void buildLevel(int targetLevel, u32 seed) {
    memset(map, 0, sizeof map);
    clearSpits();

    int groundRow[MW];
    int gapRun = 0;

    for (int c = 0; c < MW; c++) {
        float n = fbm1d((float)c * 0.11f, seed);
        int isGap = 0;

        if (c >= 6 && c <= 57) {
            if (n < -0.34f && gapRun < 2) {
                isGap = 1;
                gapRun++;
            } else {
                gapRun = 0;
            }
        } else {
            gapRun = 0;
        }

        if (isGap) {
            groundRow[c] = MH;
        } else {
            int h = 9 + (int)roundf(-n * 1.6f);
            if (h < 8) h = 8;
            if (h > 10) h = 10;
            if (c <= 4 || c >= 59) h = 10;
            groundRow[c] = h;

            for (int r = h; r < MH; r++) {
                map[r][c] = '#';
            }
        }
    }

    ne = 0;
    int lastEnemyCol = 4;

    for (int c = 7; c <= 55; c++) {
        float pNoise = perlin1d((float)c * 0.19f + 13.7f, seed ^ 0xA5A5A5A5u);
        float cNoise = perlin1d((float)c * 0.27f + 5.1f, seed ^ 0x3C3C3C3Cu);

        if (pNoise > 0.26f && c + 3 <= 56) {
            int platRow = 5 + (c % 3);
            int platLen = 3 + ((c + targetLevel) & 1);
            int canPlace = 1;
            for (int l = 0; l < platLen; l++) {
                if (map[platRow][c + l] != 0) canPlace = 0;
            }
            if (canPlace) {
                for (int l = 0; l < platLen; l++) {
                    map[platRow][c + l] = '=';
                    if (cNoise > 0.05f && l < platLen) {
                        map[platRow - 1][c + l] = 'o';
                    }
                }
                c += platLen;
                continue;
            }
        }

        if (groundRow[c] < MH && cNoise > 0.18f && map[groundRow[c] - 1][c] == 0) {
            map[groundRow[c] - 1][c] = 'o';
        }

        if (ne < NE && c >= lastEnemyCol + 6 && c <= 54 && groundRow[c] < MH &&
            groundRow[c + 1] < MH && map[groundRow[c] - 1][c] == 0) {
            en[ne].x = c * TS;
            en[ne].y = groundRow[c] * TS - 20;
            en[ne].dir = ((c + targetLevel) & 1) ? 1 : -1;
            en[ne].alive = 1;
            en[ne].anim = ne * 7;
            en[ne].graceTimer = ENEMY_GRACE_FRAMES;
            en[ne].lickCooldown = 30 + (ne * 18);
            en[ne].lickTimer = 0;
            en[ne].spitCooldown = 45 + (ne * 24);
            ne++;
            lastEnemyCol = c;
        }
    }
}

static void resetPlayer(void) {
    x16 = (2 * TS) << 4;
    y16 = (10 * TS - PH) << 4;
    vx16 = 0;
    vy16 = 0;
    face = 1;
    ground = 0;
    coyoteTimer = 0;
    jumpBufferTimer = 0;
    inv = 60;
    lick = 0;
    spitCharge = SPIT_MAX_CHARGE;
    spitCooldown = 0;
    clearSpits();
    for (int i = 0; i < ne; i++) {
        if (en[i].alive && en[i].graceTimer < ENEMY_GRACE_FRAMES) {
            en[i].graceTimer = ENEMY_GRACE_FRAMES;
        }
    }
}

static void startLevel(int num, u32 seed, int resetScore) {
    levelNum = num;
    levelSeed = seed;
    if (resetScore) {
        score = 0;
        health = 8;
    }
    paused = 0;
    buildLevel(levelNum, levelSeed);
    resetPlayer();
    state = 1;
}

static void newGame(void) {
    baseSeed = rollNewBaseSeed();
    startLevel(1, baseSeed + 1u, 1);
}

static int solid(int tx, int ty) {
    if (tx < 0 || tx >= MW) return 1;
    if (ty < 0 || ty >= MH) return 0;
    return map[ty][tx] == '#' || map[ty][tx] == '=';
}

static int boxhit(int x, int y, int w, int h) {
    if (x < 0 || x + w > MW * TS) return 1;
    if (y + h - 1 < 0) return 0;
    int y0 = y < 0 ? 0 : y / TS;
    int y1 = (y + h - 1) / TS;
    int x0 = x / TS;
    int x1 = (x + w - 1) / TS;
    for (int ty = y0; ty <= y1; ty++) {
        for (int tx = x0; tx <= x1; tx++) {
            if (solid(tx, ty)) return 1;
        }
    }
    return 0;
}

static int overlap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
    return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}

static void hurt(void) {
    if (inv > 0) return;
    health--;
    inv = 90;
    vy16 = -68;
    ground = 0;
    coyoteTimer = 0;
    if (health <= 0) state = 4;
}

static void tryFireSpit(int px, int py) {
    if (spitCooldown > 0 || spitCharge < SPIT_MAX_CHARGE) return;
    for (int i = 0; i < MAX_SPITS; i++) {
        if (!spits[i].active) {
            spits[i].active = 1;
            spits[i].x = (face > 0) ? (px + PW + 2) : (px - 10);
            spits[i].y = py + 11;
            spits[i].vx = (face > 0) ? 6 : -6;
            spitCharge = 0;
            spitCooldown = SPIT_COOLDOWN_FRAMES;
            break;
        }
    }
}

static void tryEnemyFireSpit(const Enemy *e) {
    for (int i = 0; i < MAX_ENEMY_SPITS; i++) {
        if (!enemySpits[i].active) {
            enemySpits[i].active = 1;
            enemySpits[i].x = (e->dir > 0) ? (e->x + 26) : (e->x - 8);
            enemySpits[i].y = e->y + 8;
            enemySpits[i].vx = (e->dir > 0) ? 4 : -4;
            break;
        }
    }
}

static void toggleHardMode(void) {
    hardMode = !hardMode;
    if (hardMode) {
        for (int i = 0; i < ne; i++) {
            if (en[i].alive && en[i].graceTimer < ENEMY_GRACE_FRAMES) {
                en[i].graceTimer = ENEMY_GRACE_FRAMES;
            }
        }
    }
}

static void updatePlay(int held, int down, int touched, int tx, int ty) {
    if (down & KEY_SELECT) {
        toggleRadio();
    }
    if (down & KEY_X) {
        toggleHardMode();
    }
    if (touched && tx >= 178 && tx < 242 && ty >= 150 && ty < 182) {
        toggleRadio();
    }
    if ((touched && tx >= 10 && tx < 74 && ty >= 150 && ty < 182) || (down & KEY_START)) {
        paused = !paused;
        return;
    }
    if (paused) {
        if (down & KEY_A) {
            paused = 0;
            clearSpits();
            state = 0;
        }
        return;
    }

    if (spitCharge < SPIT_MAX_CHARGE) {
        spitCharge++;
    }
    if (spitCooldown > 0) {
        spitCooldown--;
    }

    // L button sprints; walk & jump speeds are brisk
    int sprinting = (held & KEY_L) ? 1 : 0;
    int moveSpeed = sprinting ? 54 : 38;
    int want = 0;
    if (held & KEY_LEFT)  { want = -moveSpeed; face = -1; }
    if (held & KEY_RIGHT) { want =  moveSpeed; face =  1; }
    vx16 = want;

    int nx = x16 + vx16;
    if (!boxhit(nx >> 4, y16 >> 4, PW, PH)) {
        x16 = nx;
    } else {
        if (vx16 > 0) {
            int wall_tx = ((nx >> 4) + PW - 1) / TS;
            x16 = (wall_tx * TS - PW) << 4;
        } else if (vx16 < 0) {
            int wall_tx = (nx >> 4) / TS;
            x16 = ((wall_tx + 1) * TS) << 4;
        }
        vx16 = 0;
    }

    if (down & KEY_A) {
        jumpBufferTimer = JUMP_BUFFER_FRAMES;
    } else if (jumpBufferTimer > 0) {
        jumpBufferTimer--;
    }

    if (ground) {
        coyoteTimer = COYOTE_FRAMES;
    } else if (coyoteTimer > 0) {
        coyoteTimer--;
    }

    if (jumpBufferTimer > 0 && coyoteTimer > 0) {
        vy16 = sprinting ? -136 : -130;
        ground = 0;
        coyoteTimer = 0;
        jumpBufferTimer = 0;
    }

    if (!(held & KEY_A) && vy16 < -52) {
        vy16 = -52;
    }

    vy16 += 5;
    if (vy16 > 144) vy16 = 144;

    int px = x16 >> 4, py = y16 >> 4;

    if (lick == 0 && ((down & (KEY_B | KEY_Y)) || (touched && tx >= 78 && tx < 174 && ty >= 144 && ty < 184))) {
        lick = 14;
    }
    if (lick > 0) lick--;

    if ((down & (KEY_R | KEY_Y)) || (touched && tx >= 132 && tx < 244 && ty >= 96 && ty < 128)) {
        if (down & KEY_R) {
            tryFireSpit(px, py);
        }
    }
    if (touched && tx >= 132 && tx < 244 && ty >= 96 && ty < 128) {
        tryFireSpit(px, py);
    }

    int ny = y16 + vy16;
    ground = 0;
    if (!boxhit(x16 >> 4, ny >> 4, PW, PH)) {
        y16 = ny;
    } else {
        if (vy16 > 0) {
            int t = (((ny >> 4) + PH - 1)) / TS;
            y16 = (t * TS - PH) << 4;
            ground = 1;
            coyoteTimer = COYOTE_FRAMES;
        } else if (vy16 < 0) {
            int t = (ny >> 4) / TS;
            y16 = ((t + 1) * TS) << 4;
        }
        vy16 = 0;
    }

    if (inv > 0) inv--;

    px = x16 >> 4;
    py = y16 >> 4;

    for (int r = (py < 0 ? 0 : py / TS); r <= (py + PH - 1) / TS && r < MH; r++) {
        for (int c = px / TS; c <= (px + PW - 1) / TS && c < MW; c++) {
            if (map[r][c] == 'o') {
                map[r][c] = 0;
                score += 50;
            }
        }
    }

    for (int s = 0; s < MAX_SPITS; s++) {
        if (!spits[s].active) continue;
        spits[s].x += spits[s].vx;
        if (spits[s].x < 0 || spits[s].x > MW * TS || boxhit(spits[s].x, spits[s].y, 8, 6)) {
            spits[s].active = 0;
            continue;
        }
        for (int i = 0; i < ne; i++) {
            if (en[i].alive && overlap(spits[s].x, spits[s].y, 8, 6, en[i].x, en[i].y, 24, 20)) {
                en[i].alive = 0;
                spits[s].active = 0;
                score += hardMode ? 150 : 100;
                break;
            }
        }
    }

    for (int s = 0; s < MAX_ENEMY_SPITS; s++) {
        if (!enemySpits[s].active) continue;
        if (!hardMode) {
            enemySpits[s].active = 0;
            continue;
        }
        enemySpits[s].x += enemySpits[s].vx;
        if (enemySpits[s].x < 0 || enemySpits[s].x > MW * TS ||
            boxhit(enemySpits[s].x, enemySpits[s].y, 8, 6)) {
            enemySpits[s].active = 0;
            continue;
        }
        if (overlap(enemySpits[s].x, enemySpits[s].y, 8, 6, px, py, PW, PH)) {
            enemySpits[s].active = 0;
            hurt();
        }
    }

    int tonx = face > 0 ? px + PW : px - 28;
    int enemySpeed = hardMode ? 2 : 1;

    for (int i = 0; i < ne; i++) {
        Enemy *e = &en[i];
        if (!e->alive) continue;
        e->anim++;

        if (e->graceTimer > 0) e->graceTimer--;
        if (e->lickTimer > 0) e->lickTimer--;
        if (e->lickCooldown > 0) e->lickCooldown--;
        if (e->spitCooldown > 0) e->spitCooldown--;

        for (int step = 0; step < enemySpeed; step++) {
            int nex = e->x + e->dir;
            int ahead = e->dir > 0 ? nex + 24 : nex;
            if (boxhit(nex, e->y, 24, 20) || !solid(ahead / TS, (e->y + 20) / TS)) {
                e->dir = -e->dir;
                break;
            } else {
                e->x = nex;
            }
        }

        if (hardMode && e->graceTimer == 0) {
            int dx = px - e->x;
            int dy = abs((py + PH / 2) - (e->y + 10));
            if (abs(dx) < 150 && dy < 36) {
                e->dir = (dx >= 0) ? 1 : -1;
            }

            if (e->lickCooldown == 0 && e->lickTimer == 0 && abs(dx) <= 44 && dy <= 20) {
                e->lickTimer = ENEMY_LICK_ACTIVE;
                e->lickCooldown = ENEMY_LICK_REGEN;
            }

            if (e->spitCooldown == 0 && abs(dx) <= 136 && dy <= 26) {
                tryEnemyFireSpit(e);
                e->spitCooldown = ENEMY_SPIT_REGEN;
            }
        }

        if (lick > 0 && overlap(tonx, py + 10, 28, 8, e->x, e->y, 24, 20)) {
            e->alive = 0;
            score += hardMode ? 150 : 100;
            continue;
        }

        if (hardMode && e->graceTimer == 0 && e->lickTimer > 0) {
            int elx = (e->dir > 0) ? (e->x + 24) : (e->x - 24);
            if (overlap(elx, e->y + 9, 24, 6, px, py, PW, PH)) {
                hurt();
            }
        }

        if (overlap(px, py, PW, PH, e->x, e->y, 24, 20)) {
            if (vy16 > 0 && (py + PH) - e->y < 15) {
                e->alive = 0;
                score += hardMode ? 150 : 100;
                vy16 = -88;
                ground = 0;
            } else if (e->graceTimer == 0) {
                hurt();
            }
        }
    }

    if (py > SH + 20) {
        health--;
        if (health <= 0) state = 4;
        else resetPlayer();
    }
    if (px >= 61 * TS) {
        score += 250 * levelNum;
        state = 3;
    }
}

// ---- Regenerating bar inside 112x32 SPIT button sprite ----
// 8px narrower than previous (bw - 28), 1px thinner (barH = 3), 3px lower (by + bh - 9),
// and uses rounded/clean fill without a single-pixel stray green strand.
static void drawSpitBarFill(u16 *dst, int bx, int by, int charge, int maxCharge) {
    const int bw = 112;
    const int bh = 32;

    if (maxCharge <= 0) return;
    if (charge < 0) charge = 0;
    if (charge > maxCharge) charge = maxCharge;

    const int barX = bx + 14;
    const int barW = bw - 28;
    const int barY = by + bh - 9;
    const int barH = 3;

    int fillW = (charge * barW) / maxCharge;
    if (fillW <= 0) return;
    if (fillW > barW) fillW = barW;

    int full = (charge >= maxCharge);

    for (int j = 0; j < barH; j++) {
        for (int i = 0; i < fillW; i++) {
            u16 col;
            if (j == 0) {
                col = full ? C(16, 28, 12) : C(31, 28, 10);
            } else if (j == 1) {
                col = full ? C(8, 24, 8) : C(31, 18, 3);
            } else {
                col = full ? C(5, 18, 5) : C(27, 8, 4);
            }
            px_set(dst, barX + i, barY + j, col);
        }
    }
}

// ---- Multi-layer 3D depth parallax (far sky -> painted clouds -> near clouds -> player) ----
static void drawSkyAndClouds(int camx) {
    int skyScroll = (camx / 5) & 255;
    for (int y = 0; y < SH; y++) {
        const u16 *srcRow = skyBitmap + y * SW;
        u16 *dstRow = buf + y * SW;
        int firstPart = SW - skyScroll;
        memcpy(dstRow, srcRow + skyScroll, (size_t)firstPart * sizeof(u16));
        if (skyScroll > 0) {
            memcpy(dstRow + firstPart, srcRow, (size_t)skyScroll * sizeof(u16));
        }
    }

    int midScroll = (camx / 3) & 255;
    for (int y = 12; y < 76; y++) {
        const u16 *srcRow = skyBitmap + y * SW;
        u16 *dstRow = buf + y * SW;
        for (int x = 0; x < SW; x++) {
            u16 c = srcRow[(x + midScroll) & 255];
            int r = c & 31;
            int g = (c >> 5) & 31;
            int b = (c >> 10) & 31;
            if (r >= 24 && g >= 25 && b >= 26) {
                dstRow[x] = c | BIT(15);
            }
        }
    }

    for (int i = 0; i < 4; i++) {
        int span = SW + 140;
        int cx = (((i * 210 - (camx * 2) / 3) % span) + span) % span - 70;
        int cy = 12 + (i * 25) % 34;
        drawBmp16(buf, (const u16 *)cloudBitmap, 64, 32, cx, cy, (i % 3 == 0));
    }
}

static void drawTileTextured(int sx, int sy, int r, int c, u8 t) {
    if (t == '#') {
        int hasTop = (r == 0 || map[r - 1][c] != '#');
        for (int j = 0; j < TS; j++) {
            for (int i = 0; i < TS; i++) {
                u32 h = hash2d(c * TS + i, r * TS + j);
                u16 col;
                if (hasTop && j < 4) {
                    if (j == 0) col = GRASS_HI;
                    else col = ((h & 3) == 0) ? GRASS_HI : GRASS_MD;
                } else {
                    if ((h & 15) == 0) col = DIRT_DK;
                    else if ((h & 15) == 1) col = DIRT_HI;
                    else col = DIRT_MD;
                }
                px_set(buf, sx + i, sy + j, col);
            }
        }
    } else if (t == '=') {
        rect(buf, sx, sy, TS, TS, PLAT_MD);
        rect(buf, sx, sy, TS, 3, PLAT_HI);
        rect(buf, sx, sy + TS - 3, TS, 3, PLAT_DK);
        rect(buf, sx + 2, sy + 6, 2, 2, PLAT_HI);
        rect(buf, sx + TS - 4, sy + 6, 2, 2, PLAT_HI);
    } else if (t == 'o') {
        drawBmp16(buf, (const u16 *)coinBitmap, 16, 16, sx, sy, 0);
    }
}

static void drawPlay(void) {
    int px = x16 >> 4, py = y16 >> 4;
    int camx = px - 112;
    if (camx < 0) camx = 0;
    if (camx > MW * TS - SW) camx = MW * TS - SW;

    drawSkyAndClouds(camx);

    int c0 = camx / TS;
    int c1 = c0 + SW / TS + 1;
    if (c1 >= MW) c1 = MW - 1;

    for (int r = 0; r < MH; r++) {
        for (int c = c0; c <= c1; c++) {
            u8 t = map[r][c];
            if (!t) continue;
            drawTileTextured(c * TS - camx, r * TS, r, c, t);
        }
    }

    rect(buf, 62 * TS + 6 - camx, 80, 3, 80, WHT);
    rect(buf, 62 * TS + 9 - camx, 80, 22, 14, LICKC);
    rect(buf, 62 * TS + 9 - camx, 80, 22, 3, PINK);

    for (int i = 0; i < ne; i++) {
        if (!en[i].alive) continue;
        if (en[i].graceTimer > 0 && ((en[i].graceTimer / 4) % 2) == 1) continue;
        int bob = ((en[i].anim / 6) % 2 == 0) ? 0 : -1;
        drawBmp16(buf, (const u16 *)lemonBitmap, 32, 32, en[i].x - camx - 4, en[i].y - 12 + bob, en[i].dir > 0);

        if (hardMode && en[i].lickTimer > 0) {
            int elx = ((en[i].dir > 0) ? (en[i].x + 24) : (en[i].x - 24)) - camx;
            int ely = en[i].y + 9;
            rect(buf, elx, ely, 24, 6, HARD_RED);
            rect(buf, elx, ely, 24, 2, YEL);
        }
    }

    for (int s = 0; s < MAX_SPITS; s++) {
        if (!spits[s].active) continue;
        int sx = spits[s].x - camx;
        int sy = spits[s].y;
        rect(buf, sx, sy, 8, 6, LEMON_FL);
        rect(buf, sx + 1, sy + 1, 6, 4, YEL);
    }

    for (int s = 0; s < MAX_ENEMY_SPITS; s++) {
        if (!enemySpits[s].active) continue;
        int sx = enemySpits[s].x - camx;
        int sy = enemySpits[s].y;
        rect(buf, sx, sy, 8, 6, HARD_RED);
        rect(buf, sx + 1, sy + 1, 6, 4, YEL);
    }

    if (inv == 0 || (inv % 6) < 3) {
        int drawX = px - camx - ((48 - PW) / 2);
        int drawY = py - (40 - PH);
        drawBmp16(buf, (const u16 *)cowBitmap, 48, 40, drawX, drawY, face < 0);
    }

    if (lick > 0) {
        int lx = (face > 0 ? px + PW : px - 28) - camx;
        int ly = py + 11;
        rect(buf, lx, ly, 28, 6, TONGUE);
        rect(buf, lx, ly, 28, 2, PINK);
    }

    char s[48];
    snprintf(s, sizeof s, "L%d %06d %s", levelNum, score, hardMode ? "HARD" : "NORM");
    text(buf, s, 7, 5, 2, BLK);
    text(buf, s, 6, 4, 2, hardMode ? HARD_RED : WHT);

    rect(buf, 238, 18, 14, 70, BLK);
    for (int i = 0; i < health; i++) {
        u16 col = i < 3 ? C(28,5,4) : (i < 5 ? C(30,24,3) : C(6,24,6));
        rect(buf, 240, 84 - i * 8 - 6, 10, 6, col);
    }

    if (paused) {
        rect(buf, 36, 66, 184, 56, BLK);
        text(buf, "PAUSED", 76, 74, 3, WHT);
        text(buf, "A:TITLE  START:RESUME", 46, 104, 1, YEL);
    }
}

static void drawBootSplash(void) {
    dmaCopy(splashBitmap, buf, SW * SH * sizeof(u16));
    rect(sbuf, 0, 0, SW, SH, BLK);
    text(sbuf, "COW GAME 3", 48, 84, 4, WHT);
}

static void drawTitle(void) {
    dmaCopy(skyBitmap, buf, SW * SH * sizeof(u16));
    drawBmp16(buf, (const u16 *)logoBitmap, 208, 48, 24, 34, 0);
    drawBmp16(buf, (const u16 *)cowBitmap, 48, 40, 104, 102, 0);
    text(buf, hardMode ? "HARD MODE: ON" : "HARD MODE: OFF", 48, 152, 2, hardMode ? HARD_RED : YEL);
    text(buf, "X TO TOGGLE HARD", 64, 170, 1, WHT);

    rect(sbuf, 0, 0, SW, SH, SUBBG);
    if ((frame / 30) % 2 == 0) {
        text(sbuf, "PRESS START", 80, 88, 2, WHT);
    }
}

static void drawSub(void) {
    int px = x16 >> 4, py = y16 >> 4;
    int camx = px - 112;
    if (camx < 0) camx = 0;
    if (camx > MW * TS - SW) camx = MW * TS - SW;

    rect(sbuf, 0, 0, SW, SH, SUBBG);
    char hdr[36];
    snprintf(hdr, sizeof hdr, "WORLD %d SEED:%u", levelNum, (unsigned)(levelSeed % 100000u));
    text(sbuf, hdr, 28, 6, 2, WHT);

    for (int i = 0; i < health; i++) {
        rect(sbuf, 8 + i * 14, 20, 10, 7, LICKC);
    }

    rect(sbuf, 30, 32, 196, 38, BLK);
    rect(sbuf, 32, 34, 192, 34, SKY_TOP);
    for (int r = 0; r < MH; r++) {
        for (int c = 0; c < MW; c++) {
            u8 t = map[r][c];
            if (t) {
                u16 mc = (t == '#') ? DIRT_HI : ((t == '=') ? PLAT_HI : GOLD);
                rect(sbuf, 32 + c * 3, 34 + r * 3, 3, 3, mc);
            }
        }
    }
    for (int i = 0; i < ne; i++) {
        if (en[i].alive) {
            rect(sbuf, 32 + en[i].x * 3 / TS, 34 + en[i].y * 3 / TS, 3, 3, LICKC);
        }
    }
    if ((frame / 8) % 2 == 0) {
        rect(sbuf, 32 + px * 3 / TS, 34 + py * 3 / TS, 3, 4, YEL);
    }
    rect(sbuf, 32 + camx * 3 / TS, 32, SW * 3 / TS, 1, WHT);
    rect(sbuf, 32 + camx * 3 / TS, 69, SW * 3 / TS, 1, WHT);

    text(sbuf, "A JUMP L RUN B LICK R SPIT", 8, 78, 1, WHT);
    text(sbuf, hardMode ? "X HARD:ON SEL RADIO" : "X HARD:OFF SEL RADIO", 24, 88, 1, hardMode ? HARD_RED : YEL);

    int spitX = 132;
    int spitY = 96;
    drawBmp16(sbuf, (const u16 *)btn_spitBitmap, 112, 32, spitX, spitY, 0);
    drawSpitBarFill(sbuf, spitX, spitY, spitCharge, SPIT_MAX_CHARGE);

    drawBmp16(sbuf, (const u16 *)btn_pauseBitmap, 64, 32, 10, 150, 0);
    drawBmp16(sbuf, (const u16 *)btn_lickBitmap,  96, 40, 78, 144, 0);
    drawBmp16(sbuf, (const u16 *)btn_radioBitmap, 64, 32, 178, 150, 0);
}

int main(void) {
#ifdef ARM9
    dsiMode = isDSiMode() ? 1 : 0;
#endif
    if (dsiMode) {
        setCpuClock(true);
    }

    buf  = (u16 *)malloc(SW * SH * sizeof(u16));
    sbuf = (u16 *)malloc(SW * SH * sizeof(u16));
    if (!buf || !sbuf) {
        while (1) swiWaitForVBlank();
    }

    videoSetMode(MODE_5_2D | DISPLAY_BG3_ACTIVE);
    videoSetModeSub(MODE_5_2D | DISPLAY_BG3_ACTIVE);
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankC(VRAM_C_SUB_BG);

    int bg  = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    int sbg = bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    u16 *fb  = bgGetGfxPtr(bg);
    u16 *sfb = bgGetGfxPtr(sbg);

    irqEnable(IRQ_VBLANK);
    soundEnable();

    buildRadioLoop();
    startRadioLoop();

    for (int i = 0; i < SW * SH; i++) {
        fb[i] = C(0, 0, 0);
        sfb[i] = C(0, 0, 0);
    }

    baseSeed = 0x1337BEEFu;
    buildLevel(1, baseSeed + 1u);
    bootTimer = BOOT_FRAMES;
    state = -1;

    while (1) {
        scanKeys();
        int down = keysDown(), held = keysHeld();
        int touched = 0, tx = 0, ty = 0;
        touchPosition tp;
        if (down & KEY_TOUCH) {
            touchRead(&tp);
            touched = 1;
            tx = tp.px;
            ty = tp.py;
        }
        frame++;

        if (state == -1) {
            drawBootSplash();
            if (--bootTimer <= 0 || (down & KEY_START)) {
                state = 0;
            }
        } else if (state == 0) {
            if (down & KEY_SELECT) toggleRadio();
            if (down & KEY_X) toggleHardMode();
            if (down & KEY_START) newGame();
            else drawTitle();
        } else {
            if (state == 1) updatePlay(held, down, touched, tx, ty);
            drawPlay();
            drawSub();
            if (state == 3) {
                text(buf, "LEVEL CLEAR!", 28, 70, 4, WHT);
                text(buf, "PRESS START NEXT SEED", 34, 120, 2, YEL);
                if (down & KEY_START) {
                    startLevel(levelNum + 1, baseSeed + (u32)((levelNum + 1) * 7919u), 0);
                }
            }
            if (state == 4) {
                text(buf, "GAME OVER", 40, 70, 5, WHT);
                text(buf, "PRESS START", 85, 120, 2, WHT);
                if (down & KEY_START) state = 0;
            }
        }

        swiWaitForVBlank();
        DC_FlushRange(buf, SW * SH * sizeof(u16));
        DC_FlushRange(sbuf, SW * SH * sizeof(u16));
        dmaCopy(buf, fb, SW * SH * sizeof(u16));
        dmaCopy(sbuf, sfb, SW * SH * sizeof(u16));
    }

    return 0;
}