// COW GAME 3 - Nintendo DS/DSi homebrew platformer (BlocksDS / libnds)
// D-Pad: move | A: jump | B / Y / touch LICK: lick | L / R / touch SPIT: spit (recharging)
// SELECT / touch RADIO: toggle radio | START / touch PAUSE: pause
#include <nds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define SW 256
#define SH 192
#define TS 16
#define MW 64
#define MH 12
#define PW 18
#define PH 28
#define NE 8
#define MAX_SPITS 4

#define BOOT_FRAMES 120          // ~2 seconds at 60 FPS
#define COYOTE_FRAMES 6
#define JUMP_BUFFER_FRAMES 6
#define SPIT_MAX_CHARGE 180      // ~3 seconds full recharge at 60 FPS
#define SPIT_COOLDOWN_FRAMES 12

// Audio synthesis settings (8-bit signed PCM loop, ~11025 Hz, 4.0s loop)
#define AUDIO_RATE 11025
#define AUDIO_SECONDS 4
#define AUDIO_SAMPLES (AUDIO_RATE * AUDIO_SECONDS)

#define C(r,g,b) (RGB15(r,g,b) | BIT(15))

// Palette
#define SKY_TOP   C(8, 16, 30)
#define SKY_BOT   C(18, 25, 31)
#define CLOUD_HI  C(31, 31, 31)
#define CLOUD_MD  C(27, 29, 31)
#define CLOUD_SH  C(21, 24, 30)
#define WHT       C(31, 31, 31)
#define BLK       C(2, 2, 3)
#define YEL       C(31, 29, 3)
#define DYEL      C(23, 18, 2)
#define LEMON_FL  C(31, 31, 19)
#define GRN       C(6, 22, 6)
#define LGRN      C(13, 27, 10)
#define DGRN      C(3, 13, 4)
#define HORN      C(21, 18, 13)
#define DHORN     C(14, 11, 8)
#define PINK      C(31, 19, 20)
#define DPINK     C(25, 11, 13)
#define UDD       C(31, 22, 22)
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
#define BTN       C(8, 15, 29)
#define BTN_ON    C(7, 22, 13)
#define BTN_OFF   C(16, 10, 12)
#define SPIT_BTN  C(31, 18, 4)
#define LICKC     C(30, 12, 13)
#define TONGUE    C(31, 10, 12)

static u16 *buf  = NULL;
static u16 *sbuf = NULL;
static s8  *radioPcm = NULL;
static int  radioChannel = -1;
static int  radioMuted = 0;
static int  dsiMode = 0;

// Detailed 24x20 Lemon-Cow sprite (facing right by default)
static const char *COW_SPRITE[20] = {
    ".......LL..HH......HH...",
    "......LLL.H..H....H..H..",
    ".....LLLL.HH..H..H..HH..",
    "....LLGLL..HHHH..HHHH...",
    ".....LG..YYYYYYYYYYYY...",
    "........YYYYYYYYYYYYYY..",
    ".......YYLYYYYYYYYYYYY..",
    "......YYYYYYYYYYYYYPPP..",
    "......YYYYYYYYYYYYPPPPPP",
    "......YYDYYYYYYYYYPPKPPP",
    ".......YYYYYYYYYYYPPPPPP",
    ".......YYYYYYYYYYYYYDPPP",
    "........YYYYYYYYYYYYY...",
    ".........YYYYYYYYYY.....",
    ".........YYYYYYYYYY.....",
    "..........UU....UU......",
    ".........WW.W..WW.W.....",
    ".........WW.W..WW.W.....",
    ".........WW.W..WW.W.....",
    ".........KK.K..KK.K....."
};

// Detailed 16x14 Grumpy Lemon enemy sprite
static const char *LEMON_SPRITE[14] = {
    ".......LL.......",
    "......LGGL......",
    ".......GD.......",
    "....YYYYYYYY....",
    "..YYLYYYYYYYYY..",
    ".YYYYYYYYYYYYYY.",
    ".YYKKYYYYYYKKYY.",
    ".YYYYKYYYYKYYYY.",
    ".YYYYYYYYYYYYYY.",
    ".YYDYYYYYYYYDYY.",
    "..YYYYDYYYDYYY..",
    "...YYYYDDDDYY...",
    "....YYYYYYYY....",
    "......DDDD......"
};

// Detailed 32x14 Puffy Cloud sprite
static const char *CLOUD_SPRITE[14] = {
    ".........CCCCCCCCC..............",
    ".......CCCHHHHHHHHCC............",
    "......CHHHHHHHHHHHHHCC..........",
    "....CCCHHHHHHHHHHHHHHHCC........",
    "...CHHHHHHHHHHHHHHHHHHHCCC......",
    "..CHHHHHHHHHHHHHHHHHHHHHHHCC....",
    ".CHHHHHHHHHHHHHHHHHHHHHHHHHHCC..",
    ".CHHHHHHHHHHHHHHHHHHHHHHHHHHHHC.",
    ".CHHHHHHHHHHHHHHHHHHHHHHHHHHHHC.",
    ".CMMMMMMMMMMMMMMMMMMMMMMMMMMMMC.",
    "..CCCCCCCCCCCCCCCCCCCCCCCCCCCC..",
    "...SSSSSSSMMMMMMMMMMMMMMSSSSSS..",
    ".....SSSSSSSSSSSSSSSSSSSSSSSS...",
    ".........SSSSSSSSSSSSSSSS......."
};

// ---- tiny 3x5 font (each row is an octal digit = 3 pixels) ----
#define G(a,b,c,d,e) (((a)<<12)|((b)<<9)|((c)<<6)|((d)<<3)|(e))
typedef struct { char c; u16 g; } Glyph;
static const Glyph font[] = {
    {'A',G(2,5,7,5,5)},{'B',G(6,5,6,5,6)},{'C',G(3,4,4,4,3)},{'D',G(6,5,5,5,6)},
    {'E',G(7,4,6,4,7)},{'F',G(7,4,6,4,4)},{'G',G(3,4,5,5,3)},{'H',G(5,5,7,5,5)},
    {'I',G(7,2,2,2,7)},{'K',G(5,5,6,5,5)},{'L',G(4,4,4,4,7)},{'M',G(5,7,7,5,5)},
    {'N',G(6,5,5,5,5)},{'O',G(2,5,5,5,2)},{'P',G(6,5,6,4,4)},{'R',G(6,5,6,5,5)},
    {'S',G(3,4,2,1,6)},{'T',G(7,2,2,2,2)},{'U',G(5,5,5,5,7)},{'V',G(5,5,5,5,2)},
    {'W',G(5,5,7,7,5)},{'X',G(5,5,2,5,5)},{'Y',G(5,5,2,2,2)},
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

static u16 pal(char ch) {
    switch (ch) {
        case 'Y': return YEL;
        case 'D': return DYEL;
        case 'L': return LEMON_FL;
        case 'G': return GRN;
        case 'l': return LGRN;
        case 'd': return DGRN;
        case 'H': return HORN;
        case 'h': return DHORN;
        case 'P': return PINK;
        case 'p': return DPINK;
        case 'W': return WHT;
        case 'K': return BLK;
        case 'U': return UDD;
        case 'C': return CLOUD_HI;
        case 'M': return CLOUD_MD;
        case 'S': return CLOUD_SH;
        default:  return 0; // transparent
    }
}

static void drawSpriteGrid(u16 *b, const char **s, int w, int h, int x, int y, int sc, int flip) {
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            char ch = s[j][flip ? (w - 1 - i) : i];
            u16 col = pal(ch);
            if (!col) continue;
            if (sc == 1) {
                px_set(b, x + i, y + j, col);
            } else {
                rect(b, x + i * sc, y + j * sc, sc, sc, col);
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
typedef struct { int x, y, dir, alive, anim; } Enemy;
typedef struct { int x, y, vx, active; } SpitProj;

static u8 map[MH][MW];
static Enemy en[NE];
static SpitProj spits[MAX_SPITS];
static int ne;
static int x16, y16, vx16, vy16, face, ground, inv, lick;
static int spitCharge, spitCooldown;
static int coyoteTimer, jumpBufferTimer;
static int score, health, state, paused, frame, bootTimer;
// state: -1 boot splash (2s), 0 title, 1 playing, 3 win, 4 game over

static void clearSpits(void) {
    for (int i = 0; i < MAX_SPITS; i++) {
        spits[i].active = 0;
    }
}

static void buildLevel(void) {
    memset(map, 0, sizeof map);
    for (int c = 0; c < MW; c++) {
        if ((c >= 20 && c <= 22) || (c >= 38 && c <= 40)) continue;
        map[10][c] = '#';
        map[11][c] = '#';
    }
    static const int plats[][3] = {
        {8,7,4},{14,5,3},{24,7,4},{29,5,4},{34,7,3},{44,7,3},{48,5,4},{53,7,3}
    };
    for (unsigned i = 0; i < sizeof(plats) / sizeof(plats[0]); i++) {
        for (int l = 0; l < plats[i][2]; l++) {
            map[plats[i][1]][plats[i][0] + l] = '=';
        }
    }
    static const int coins[][2] = {
        {9,6},{10,6},{15,4},{16,4},{25,6},{26,6},{30,4},{31,4},
        {21,8},{39,8},{45,6},{49,4},{50,4},{54,6},{12,9},{36,9}
    };
    for (unsigned i = 0; i < sizeof(coins) / sizeof(coins[0]); i++) {
        map[coins[i][1]][coins[i][0]] = 'o';
    }
    static const int ecols[] = {12, 18, 27, 32, 45, 50, 56};
    ne = 0;
    for (unsigned i = 0; i < sizeof(ecols) / sizeof(ecols[0]); i++) {
        en[ne].x = ecols[i] * TS;
        en[ne].y = 10 * TS - 20;
        en[ne].dir = -1;
        en[ne].alive = 1;
        en[ne].anim = i * 7;
        ne++;
    }
    clearSpits();
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
}

static void newGame(void) {
    buildLevel();
    score = 0;
    health = 8;
    paused = 0;
    resetPlayer();
    state = 1;
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
    vy16 = -64;
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
            spits[i].vx = (face > 0) ? 5 : -5;
            spitCharge = 0;
            spitCooldown = SPIT_COOLDOWN_FRAMES;
            break;
        }
    }
}

static void updatePlay(int held, int down, int touched, int tx, int ty) {
    if (down & KEY_SELECT) {
        toggleRadio();
    }
    if (touched && tx >= 182 && tx < 246 && ty >= 152 && ty < 180) {
        toggleRadio();
    }
    if ((touched && tx >= 10 && tx < 70 && ty >= 152 && ty < 180) || (down & KEY_START)) {
        paused = !paused;
    }
    if (paused) return;

    // Recharge spit meter over time
    if (spitCharge < SPIT_MAX_CHARGE) {
        spitCharge++;
    }
    if (spitCooldown > 0) {
        spitCooldown--;
    }

    // Horizontal movement
    int want = 0;
    if (held & KEY_LEFT)  { want = -28; face = -1; }
    if (held & KEY_RIGHT) { want = 28;  face = 1; }
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

    // Jump buffering & coyote time
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
        vy16 = -114;
        ground = 0;
        coyoteTimer = 0;
        jumpBufferTimer = 0;
    }

    if (!(held & KEY_A) && vy16 < -42) {
        vy16 = -42;
    }

    vy16 += 5;
    if (vy16 > 140) vy16 = 140;

    int px = x16 >> 4, py = y16 >> 4;

    // Lick attack (always available, zero cooldown)
    if (lick == 0 && ((down & (KEY_B | KEY_Y)) || (touched && tx >= 76 && tx < 176 && ty >= 146 && ty < 184))) {
        lick = 14;
    }
    if (lick > 0) lick--;

    // Spit attack via L or R triggers, or bottom-screen SPIT button
    if ((down & (KEY_L | KEY_R)) || (touched && tx >= 132 && tx < 246 && ty >= 104 && ty < 138)) {
        tryFireSpit(px, py);
    }

    // Vertical movement & collision
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

    // Collect lemon slices
    for (int r = (py < 0 ? 0 : py / TS); r <= (py + PH - 1) / TS && r < MH; r++) {
        for (int c = px / TS; c <= (px + PW - 1) / TS && c < MW; c++) {
            if (map[r][c] == 'o') {
                map[r][c] = 0;
                score += 50;
            }
        }
    }

    // Update spit projectiles
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
                score += 100;
                break;
            }
        }
    }

    // Update enemies + lick / stomp / body collisions
    int tonx = face > 0 ? px + PW : px - 28;
    for (int i = 0; i < ne; i++) {
        Enemy *e = &en[i];
        if (!e->alive) continue;
        e->anim++;

        int nex = e->x + e->dir;
        int ahead = e->dir > 0 ? nex + 24 : nex;
        if (boxhit(nex, e->y, 24, 20) || !solid(ahead / TS, (e->y + 20) / TS)) {
            e->dir = -e->dir;
        } else {
            e->x = nex;
        }

        if (lick > 0 && overlap(tonx, py + 10, 28, 8, e->x, e->y, 24, 20)) {
            e->alive = 0;
            score += 100;
            continue;
        }

        if (overlap(px, py, PW, PH, e->x, e->y, 24, 20)) {
            if (vy16 > 0 && (py + PH) - e->y < 15) {
                e->alive = 0;
                score += 100;
                vy16 = -84;
                ground = 0;
            } else {
                hurt();
            }
        }
    }

    if (py > SH + 20) {
        health--;
        if (health <= 0) state = 4;
        else resetPlayer();
    }
    if (px >= 61 * TS) state = 3;
}

// ---- Environment & Sprite Rendering ----
static void drawSkyAndClouds(int camx) {
    for (int y = 0; y < SH; y++) {
        u16 skyCol = (y < 96) ? SKY_TOP : SKY_BOT;
        rect(buf, 0, y, SW, 1, skyCol);
    }

    for (int i = 0; i < 5; i++) {
        int span = SW + 140;
        int cx = (((i * 180 - camx / 3) % span) + span) % span - 70;
        int cy = 18 + (i * 31) % 46;
        int sc = (i % 2 == 0) ? 2 : 1;
        drawSpriteGrid(buf, CLOUD_SPRITE, 32, 14, cx, cy, sc, (i % 3 == 0));
    }
}

static void drawTileTextured(int sx, int sy, int r, int c, u8 t) {
    if (t == '#') {
        int hasTop = (r == 0 || map[r - 1][c] != '#');
        for (int j = 0; j < TS; j++) {
            for (int i = 0; i < TS; i++) {
                u32 h = hash2d(c * TS + i, r * TS + j);
                u16 col = DIRT_MD;
                if (hasTop && j < 5) {
                    col = (j == 0 || (h & 7) == 0) ? GRASS_HI : GRASS_MD;
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
        rect(buf, sx + 4, sy + 3, 8, 10, GOLD);
        rect(buf, sx + 5, sy + 4, 6, 8, YEL);
        rect(buf, sx + 6, sy + 5, 4, 6, LEMON_FL);
        rect(buf, sx + 7, sy + 4, 2, 8, YEL);
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
    if (c1 >= MW) 
