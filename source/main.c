// COW GAME 3 - Nintendo DS/DSi homebrew platformer (BlocksDS / libnds)
// D-Pad: move | A: jump | B / Y / touch LICK: lick attack | SELECT / touch RADIO: toggle radio | START / touch PAUSE: pause
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

#define COYOTE_FRAMES 6
#define JUMP_BUFFER_FRAMES 6

// Audio synthesis settings (8-bit signed PCM loop, ~11025 Hz, 4.0s loop)
#define AUDIO_RATE 11025
#define AUDIO_SECONDS 4
#define AUDIO_SAMPLES (AUDIO_RATE * AUDIO_SECONDS)

#define C(r,g,b) (RGB15(r,g,b) | BIT(15))

// Palette
#define SKY_TOP   C(8, 16, 30)
#define SKY_BOT   C(18, 25, 31)
#define CLOUD_HI  C(31, 31, 31)
#define CLOUD_SH  C(23, 26, 31)
#define WHT       C(31, 31, 31)
#define BLK       C(2, 2, 3)
#define YEL       C(31, 29, 3)
#define DYEL      C(24, 20, 2)
#define LEMON_FL  C(31, 31, 18)
#define GRN       C(5, 20, 5)
#define HORN      C(20, 17, 12)
#define PINK      C(30, 18, 18)
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
#define LICKC     C(30, 12, 13)
#define TONGUE    C(31, 10, 12)

static u16 *buf  = NULL; // top screen back buffer (256x192)
static u16 *sbuf = NULL; // bottom screen back buffer (256x192)
static s8  *radioPcm = NULL;
static int  radioChannel = -1;
static int  radioMuted = 0;
static int  dsiMode = 0;

static const char *COW[16] = {
    "..GG.......H....",
    ".GGG.YYYYYYYH...",
    ".GGYYYYYYYYYY...",
    "..HYYYYYYEYYYY..",
    "..YYYYYYYYYYYPP.",
    ".YYYYYYYYYYYPPP.",
    ".YYYYYYYYYYYPpP.",
    ".YYYYYYYYYYYYPP.",
    "..YYYYYYYYYYYP..",
    "..YYYYYYYYYYY...",
    "...YYYYYYYYY....",
    "....UU...UU.....",
    "...W.W...W.W....",
    "...W.W...W.W....",
    "...W.W...W.W....",
    "...K.K...K.K...."
};

static const char *LEMON[10] = {
    ".....GG.....",
    "...YYYYYY...",
    "..YYYYYYYY..",
    ".YYEYYYYEYYY",
    ".YYYYYYYYYY.",
    ".YYYEEEEYYY.",
    ".YYYYYYYYYY.",
    "..YYYYYYYY..",
    "...YYYYYY...",
    ".....yy....."
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
        case 'Y': return YEL;   case 'y': return DYEL;  case 'G': return GRN;
        case 'H': return HORN;  case 'P': return PINK;  case 'p': return LICKC;
        case 'W': return WHT;   case 'K': return BLK;   case 'E': return BLK;
        case 'U': return UDD;   default: return 0;
    }
}

static void sprite(u16 *b, const char **s, int w, int h, int x, int y, int sc, int flip) {
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            char ch = s[j][flip ? w - 1 - i : i];
            if (ch == '.') continue;
            u16 col = pal(ch);
            for (int sy = 0; sy < sc; sy++) {
                for (int sx = 0; sx < sc; sx++) {
                    u16 out = col;
                    if (ch == 'Y' && j < 4 && sy == 0) out = LEMON_FL;
                    else if (ch == 'Y' && j > 10 && sy == sc - 1) out = DYEL;
                    px_set(b, x + i * sc + sx, y + j * sc + sy, out);
                }
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

// ---- game state ----
typedef struct { int x, y, dir, alive, anim; } Enemy;
static u8 map[MH][MW];
static Enemy en[NE];
static int ne;
static int x16, y16, vx16, vy16, face, ground, inv, lick;
static int coyoteTimer, jumpBufferTimer;
static int score, health, state, paused, frame;

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

static void updatePlay(int held, int down, int touched, int tx, int ty) {
    if (down & KEY_SELECT) {
        toggleRadio();
    }
    if (touched && tx >= 182 && tx < 246 && ty >= 150 && ty < 176) {
        toggleRadio();
    }
    if ((touched && tx >= 10 && tx < 70 && ty >= 150 && ty < 176) || (down & KEY_START)) {
        paused = !paused;
    }
    if (paused) return;

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

    if (lick == 0 && ((down & (KEY_B | KEY_Y)) || (touched && tx >= 76 && tx < 176 && ty >= 140 && ty < 184))) {
        lick = 14;
    }
    if (lick > 0) lick--;

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

    int px = x16 >> 4, py = y16 >> 4;

    for (int r = (py < 0 ? 0 : py / TS); r <= (py + PH - 1) / TS && r < MH; r++) {
        for (int c = px / TS; c <= (px + PW - 1) / TS && c < MW; c++) {
            if (map[r][c] == 'o') {
                map[r][c] = 0;
                score += 50;
            }
        }
    }

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

static void drawSkyAndClouds(int camx) {
    for (int y = 0; y < SH; y++) {
        u16 skyCol = (y < 96) ? SKY_TOP : SKY_BOT;
        rect(buf, 0, y, SW, 1, skyCol);
    }

    for (int i = 0; i < 5; i++) {
        int span = SW + 140;
        int cx = (((i * 180 - camx / 3) % span) + span) % span - 70;
        int cy = 24 + (i * 37) % 52;
        rect(buf, cx, cy, 52, 12, CLOUD_HI);
        rect(buf, cx + 8, cy - 6, 32, 8, CLOUD_HI);
        rect(buf, cx + 2, cy + 9, 48, 3, CLOUD_SH);
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
        int bob = ((en[i].anim / 6) % 2 == 0) ? 0 : -1;
        sprite(buf, LEMON, 12, 10, en[i].x - camx, en[i].y + bob, 2, en[i].dir > 0);
    }

    if (inv == 0 || (inv % 6) < 3) {
        int drawX = px - camx - ((32 - PW) / 2);
        int drawY = py - (32 - PH);
        sprite(buf, COW, 16, 16, drawX, drawY, 2, face < 0);
    }

    if (lick > 0) {
        int lx = (face > 0 ? px + PW : px - 28) - camx;
        int ly = py + 11;
        rect(buf, lx, ly, 28, 6, TONGUE);
        rect(buf, lx, ly, 28, 2, PINK);
    }

    char s[40];
    snprintf(s, sizeof s, "SCORE:%06d  %s", score, dsiMode ? "DSI 133" : "DS 67");
    text(buf, s, 7, 5, 2, BLK);
    text(buf, s, 6, 4, 2, WHT);

    rect(buf, 238, 18, 14, 70, BLK);
    for (int i = 0; i < health; i++) {
        u16 col = i < 3 ? C(28,5,4) : (i < 5 ? C(30,24,3) : C(6,24,6));
        rect(buf, 240, 84 - i * 8 - 6, 10, 6, col);
    }

    if (paused) {
        rect(buf, 54, 72, 148, 38, BLK);
        text(buf, "PAUSED", 64, 80, 4, WHT);
    }
}

static void drawTitle(void) {
    rect(buf, 0, 0, SW, SH, SKY_TOP);
    rect(buf, 0, 96, SW, SH - 96, SKY_BOT);
    sprite(buf, COW, 16, 16, 96, 26, 4, 0);
    text(buf, "COW GAME 3", 50, 112, 4, WHT);
    text(buf, dsiMode ? "DSI MODE ACTIVE" : "NDS MODE", 66, 148, 2, YEL);

    rect(sbuf, 0, 0, SW, SH, SUBBG);
    text(sbuf, "PORTAL RADIO LOOP", 46, 44, 2, WHT);
    if ((frame / 30) % 2 == 0) {
        text(sbuf, "PRESS START", 80, 96, 2, WHT);
    }
}

static void drawSub(void) {
    int px = x16 >> 4, py = y16 >> 4;
    int camx = px - 112;
    if (camx < 0) camx = 0;
    if (camx > MW * TS - SW) camx = MW * TS - SW;

    rect(sbuf, 0, 0, SW, SH, SUBBG);
    text(sbuf, "LEVEL 1-1", 92, 6, 2, WHT);

    for (int i = 0; i < health; i++) {
        rect(sbuf, 8 + i * 14, 24, 10, 8, LICKC);
    }

    rect(sbuf, 30, 42, 196, 40, BLK);
    rect(sbuf, 32, 44, 192, 36, SKY_TOP);
    for (int r = 0; r < MH; r++) {
        for (int c = 0; c < MW; c++) {
            u8 t = map[r][c];
            if (t) {
                u16 mc = (t == '#') ? DIRT_HI : ((t == '=') ? PLAT_HI : GOLD);
                rect(sbuf, 32 + c * 3, 44 + r * 3, 3, 3, mc);
            }
        }
    }
    for (int i = 0; i < ne; i++) {
        if (en[i].alive) {
            rect(sbuf, 32 + en[i].x * 3 / TS, 44 + en[i].y * 3 / TS, 3, 3, LICKC);
        }
    }
    if ((frame / 8) % 2 == 0) {
        rect(sbuf, 32 + px * 3 / TS, 44 + py * 3 / TS, 3, 4, YEL);
    }
    rect(sbuf, 32 + camx * 3 / TS, 42, SW * 3 / TS, 1, WHT);
    rect(sbuf, 32 + camx * 3 / TS, 81, SW * 3 / TS, 1, WHT);

    text(sbuf, "DPAD MOVE A JUMP B LICK", 16, 98, 1, WHT);
    text(sbuf, "SELECT OR TAP RADIO", 36, 112, 1, YEL);

    rect(sbuf, 10, 150, 60, 26, BTN);
    text(sbuf, "PAUSE", 18, 159, 2, WHT);

    rect(sbuf, 76, 140, 98, 44, LICKC);
    text(sbuf, "LICK", 95, 154, 3, WHT);

    rect(sbuf, 182, 150, 64, 26, radioMuted ? BTN_OFF : BTN_ON);
    text(sbuf, radioMuted ? "MUTE" : "RADIO", 190, 159, 2, WHT);
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

    buildLevel();
    state = 0;

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

        if (state == 0) {
            if (down & KEY_SELECT) toggleRadio();
            if (down & KEY_START) newGame();
            else drawTitle();
        } else {
            if (state == 1) updatePlay(held, down, touched, tx, ty);
            drawPlay();
            drawSub();
            if (state == 3) text(buf, "YOU WIN!", 48, 70, 5, WHT);
            if (state == 4) text(buf, "GAME OVER", 40, 70, 5, WHT);
            if (state >= 3) {
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
