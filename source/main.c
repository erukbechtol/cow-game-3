// COW GAME 3 - Nintendo DS/DSi homebrew platformer (libnds / devkitPro)
// D-Pad move, A jump, B or touch LICK = lick attack, START = pause
#include <nds.h>
#include <stdio.h>
#include <string.h>

#define SW 256
#define SH 192
#define TS 16
#define MW 64
#define MH 12
#define PW 20
#define PH 30
#define NE 8
#define C(r,g,b) (RGB15(r,g,b)|BIT(15))

#define SKY    C(12,20,31)
#define WHT    C(31,31,31)
#define BLK    C(2,2,3)
#define YEL    C(31,29,3)
#define DYEL   C(25,22,2)
#define GRN    C(5,20,5)
#define HORN   C(18,15,10)
#define PINK   C(30,18,18)
#define DPINK  C(26,12,12)
#define UDD    C(31,22,22)
#define DIRT   C(18,11,5)
#define GRASS  C(8,22,6)
#define PLAT   C(18,20,27)
#define PLATT  C(26,28,31)
#define GOLD   C(31,24,2)
#define SUBBG  C(5,10,22)
#define BTN    C(8,14,28)
#define LICKC  C(30,12,13)
#define TONGUE C(31,10,12)

static u16 buf[SW*SH];   // top screen back buffer
static u16 sbuf[SW*SH];  // bottom screen back buffer

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
 "...K.K...K.K...."};
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
 ".....yy....."};

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
 {'.',G(0,0,0,0,2)}
};

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
        case 'Y': return YEL;  case 'y': return DYEL; case 'G': return GRN;
        case 'H': return HORN; case 'P': return PINK; case 'p': return DPINK;
        case 'W': return WHT;  case 'K': return BLK;  case 'E': return BLK;
        case 'U': return UDD;  default: return 0;
    }
}

static void sprite(u16 *b, const char **s, int w, int h, int x, int y, int sc, int flip) {
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) {
            char ch = s[j][flip ? w - 1 - i : i];
            if (ch == '.') continue;
            rect(b, x + i * sc, y + j * sc, sc, sc, pal(ch));
        }
}

static void text(u16 *b, const char *s, int x, int y, int sc, u16 col) {
    for (; *s; s++) {
        u16 g = 0;
        for (unsigned k = 0; k < sizeof(font) / sizeof(font[0]); k++)
            if (font[k].c == *s) { g = font[k].g; break; }
        for (int r = 0; r < 5; r++)
            for (int q = 0; q < 3; q++)
                if (g & (1 << ((4 - r) * 3 + (2 - q)))) rect(b, x + q * sc, y + r * sc, sc, sc, col);
        x += 4 * sc;
    }
}

// ---- game state ----
typedef struct { int x, y, dir, alive; } Enemy;
static u8 map[MH][MW];
static Enemy en[NE];
static int ne;
static int x16, y16, vx16, vy16, face, ground, inv, lick;
static int score, health, state, paused, frame;
// state: 0 title, 1 playing, 3 win, 4 game over

static void buildLevel(void) {
    memset(map, 0, sizeof map);
    for (int c = 0; c < MW; c++) {
        if ((c >= 20 && c <= 22) || (c >= 38 && c <= 40)) continue;
        map[10][c] = '#'; map[11][c] = '#';
    }
    static const int plats[][3] = {{8,7,4},{14,5,3},{24,7,4},{29,5,4},{34,7,3},{44,7,3},{48,5,4},{53,7,3}};
    for (unsigned i = 0; i < sizeof(plats) / sizeof(plats[0]); i++)
        for (int l = 0; l < plats[i][2]; l++) map[plats[i][1]][plats[i][0] + l] = '=';
    static const int coins[][2] = {{9,6},{10,6},{15,4},{16,4},{25,6},{26,6},{30,4},{31,4},
                                   {21,8},{39,8},{45,6},{49,4},{50,4},{54,6},{12,9},{36,9}};
    for (unsigned i = 0; i < sizeof(coins) / sizeof(coins[0]); i++) map[coins[i][1]][coins[i][0]] = 'o';
    static const int ecols[] = {12, 18, 27, 32, 45, 50, 56};
    ne = 0;
    for (unsigned i = 0; i < sizeof(ecols) / sizeof(ecols[0]); i++) {
        en[ne].x = ecols[i] * TS; en[ne].y = 10 * TS - 20; en[ne].dir = -1; en[ne].alive = 1; ne++;
    }
}

static void resetPlayer(void) {
    x16 = (2 * TS) << 4; y16 = (10 * TS - PH) << 4;
    vx16 = vy16 = 0; face = 1; inv = 60; lick = 0;
}

static void newGame(void) {
    buildLevel(); score = 0; health = 8; paused = 0; resetPlayer(); state = 1;
}

static int solid(int tx, int ty) {
    if (tx < 0 || tx >= MW) return 1;
    if (ty < 0 || ty >= MH) return 0;
    return map[ty][tx] == '#' || map[ty][tx] == '=';
}

static int boxhit(int x, int y, int w, int h) {
    if (x < 0 || x + w > MW * TS) return 1;
    if (y + h - 1 < 0) return 0;
    int y0 = y < 0 ? 0 : y / TS, y1 = (y + h - 1) / TS;
    for (int ty = y0; ty <= y1; ty++)
        for (int tx = x / TS; tx <= (x + w - 1) / TS; tx++)
            if (solid(tx, ty)) return 1;
    return 0;
}

static int overlap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
    return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}

static void hurt(void) {
    if (inv > 0) return;
    health--; inv = 90; vy16 = -60;
    if (health <= 0) state = 4;
}

static void updatePlay(int held, int down, int touched, int tx, int ty) {
    if (touched && tx >= 10 && tx < 70 && ty >= 150 && ty < 176) paused = !paused;
    else if (down & KEY_START) paused = !paused;
    if (paused) return;

    int want = 0;
    if (held & KEY_LEFT)  { want = -28; face = -1; }
    if (held & KEY_RIGHT) { want = 28;  face = 1; }
    vx16 = want;
    if ((down & KEY_A) && ground) vy16 = -110;
    if (!(held & KEY_A) && vy16 < -40) vy16 = -40;
    vy16 += 5; if (vy16 > 140) vy16 = 140;

    if (lick == 0 && ((down & (KEY_B | KEY_Y)) || (touched && tx >= 76 && tx < 180 && ty >= 140 && ty < 184))) lick = 14;
    if (lick > 0) lick--;

    int nx = x16 + vx16;
    if (!boxhit(nx >> 4, y16 >> 4, PW, PH)) x16 = nx;
    int ny = y16 + vy16;
    ground = 0;
    if (!boxhit(x16 >> 4, ny >> 4, PW, PH)) y16 = ny;
    else {
        if (vy16 > 0) { int t = (((ny >> 4) + PH - 1)) / TS; y16 = (t * TS - PH) << 4; ground = 1; }
        else { int t = (ny >> 4) / TS; y16 = ((t + 1) * TS) << 4; }
        vy16 = 0;
    }
    if (inv > 0) inv--;

    int px = x16 >> 4, py = y16 >> 4;

    // lemon slices (coins)
    for (int r = (py < 0 ? 0 : py / TS); r <= (py + PH - 1) / TS && r < MH; r++)
        for (int c = px / TS; c <= (px + PW - 1) / TS && c < MW; c++)
            if (map[r][c] == 'o') { map[r][c] = 0; score += 50; }

    // enemies
    int tonx = face > 0 ? px + 22 : px - 30;
    for (int i = 0; i < ne; i++) {
        Enemy *e = &en[i];
        if (!e->alive) continue;
        int nex = e->x + e->dir;
        int ahead = e->dir > 0 ? nex + 24 : nex;
        if (boxhit(nex, e->y, 24, 20) || !solid(ahead / TS, (e->y + 20) / TS)) e->dir = -e->dir;
        else e->x = nex;
        if (lick > 0 && overlap(tonx, py + 12, 28, 6, e->x, e->y, 24, 20)) { e->alive = 0; score += 100; continue; }
        if (overlap(px, py, PW, PH, e->x, e->y, 24, 20)) {
            if (vy16 > 0 && (py + PH) - e->y < 14) { e->alive = 0; score += 100; vy16 = -80; }
            else hurt();
        }
    }

    if (py > SH + 20) {
        health--;
        if (health <= 0) state = 4; else resetPlayer();
    }
    if (px >= 61 * TS) state = 3;
}

static void drawPlay(void) {
    int px = x16 >> 4, py = y16 >> 4;
    int camx = px - 112;
    if (camx < 0) camx = 0;
    if (camx > MW * TS - SW) camx = MW * TS - SW;

    rect(buf, 0, 0, SW, SH, SKY);
    for (int i = 0; i < 5; i++) {
        int cx = (((i * 180 - camx / 3) % (SW + 120)) + (SW + 120)) % (SW + 120) - 60;
        int cy = 24 + (i * 37) % 60;
        rect(buf, cx, cy, 50, 12, WHT); rect(buf, cx + 8, cy - 6, 30, 8, WHT);
    }
    int c0 = camx / TS, c1 = c0 + SW / TS + 1;
    if (c1 >= MW) c1 = MW - 1;
    for (int r = 0; r < MH; r++)
        for (int c = c0; c <= c1; c++) {
            u8 t = map[r][c]; int sx = c * TS - camx, sy = r * TS;
            if (t == '#') {
                rect(buf, sx, sy, TS, TS, DIRT);
                if (r == 0 || map[r - 1][c] != '#') rect(buf, sx, sy, TS, 4, GRASS);
            } else if (t == '=') {
                rect(buf, sx, sy, TS, TS, PLAT); rect(buf, sx, sy, TS, 3, PLATT);
            } else if (t == 'o') {
                rect(buf, sx + 4, sy + 3, 8, 10, GOLD); rect(buf, sx + 6, sy + 5, 2, 6, YEL);
            }
        }
    // goal flag
    rect(buf, 62 * TS + 6 - camx, 80, 3, 80, WHT);
    rect(buf, 62 * TS + 9 - camx, 80, 22, 14, LICKC);

    for (int i = 0; i < ne; i++)
        if (en[i].alive) sprite(buf, LEMON, 12, 10, en[i].x - camx, en[i].y, 2, 0);
    if (inv == 0 || (inv % 6) < 3) sprite(buf, COW, 16, 16, px - camx - 6, py - 2, 2, face < 0);
    if (lick > 0) rect(buf, (face > 0 ? px + 22 : px - 30) - camx, py + 12, 28, 6, TONGUE);

    // HUD
    char s[40];
    sprintf(s, "SCORE: %06d / WORLD: 1-1", score);
    text(buf, s, 7, 5, 2, BLK); text(buf, s, 6, 4, 2, WHT);
    rect(buf, 238, 18, 14, 70, BLK);
    for (int i = 0; i < health; i++) {
        u16 col = i < 3 ? C(28,5,4) : (i < 5 ? C(30,24,3) : C(6,24,6));
        rect(buf, 240, 84 - i * 8 - 6, 10, 6, col);
    }
    if (paused) text(buf, "PAUSED", 64, 80, 5, WHT);
}

static void drawTitle(void) {
    rect(buf, 0, 0, SW, SH, 0x8000);
    sprite(buf, COW, 16, 16, 96, 30, 4, 0);
    text(buf, "COW GAME 3", 50, 120, 4, WHT);
    rect(sbuf, 0, 0, SW, SH, SUBBG);
    if ((frame / 30) % 2 == 0) text(sbuf, "PRESS START", 85, 90, 2, WHT);
}

static void drawSub(void) {
    int px = x16 >> 4, py = y16 >> 4;
    int camx = px - 112;
    if (camx < 0) camx = 0;
    if (camx > MW * TS - SW) camx = MW * TS - SW;
    rect(sbuf, 0, 0, SW, SH, SUBBG);
    text(sbuf, "LEVEL 1-1", 92, 6, 2, WHT);
    for (int i = 0; i < health; i++) rect(sbuf, 8 + i * 14, 24, 10, 8, LICKC);
    rect(sbuf, 30, 42, 196, 40, BLK);
    rect(sbuf, 32, 44, 192, 36, SKY);
    for (int r = 0; r < MH; r++)
        for (int c = 0; c < MW; c++) {
            u8 t = map[r][c];
            if (t) rect(sbuf, 32 + c * 3, 44 + r * 3, 3, 3, t == '#' ? DIRT : (t == '=' ? PLAT : GOLD));
        }
    for (int i = 0; i < ne; i++)
        if (en[i].alive) rect(sbuf, 32 + en[i].x * 3 / TS, 44 + en[i].y * 3 / TS, 3, 3, LICKC);
    if ((frame / 8) % 2 == 0) rect(sbuf, 32 + px * 3 / TS, 44 + py * 3 / TS, 3, 4, YEL);
    rect(sbuf, 32 + camx * 3 / TS, 42, SW * 3 / TS, 1, WHT);
    rect(sbuf, 32 + camx * 3 / TS, 81, SW * 3 / TS, 1, WHT);
    text(sbuf, "DPAD MOVE  A JUMP  B LICK", 8, 100, 1, WHT);
    rect(sbuf, 10, 150, 60, 26, BTN);
    text(sbuf, "PAUSE", 21, 158, 2, WHT);
    rect(sbuf, 76, 140, 104, 44, LICKC);
    text(sbuf, "LICK", 98, 152, 4, WHT);
}

int main(void) {
    videoSetMode(MODE_5_2D | DISPLAY_BG3_ACTIVE);
    videoSetModeSub(MODE_5_2D | DISPLAY_BG3_ACTIVE);
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankC(VRAM_C_SUB_BG);
    int bg  = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    int sbg = bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    u16 *fb  = bgGetGfxPtr(bg);
    u16 *sfb = bgGetGfxPtr(sbg);
         irqEnable(IRQ_VBLANK);
    for (int i = 0; i < SW * SH; i++) { fb[i] = C(31,0,0); sfb[i] = C(0,0,31); }
    buildLevel(); state = 0;

    while (1) {
        scanKeys();
        int down = keysDown(), held = keysHeld();
        int touched = 0, tx = 0, ty = 0;
        touchPosition tp;
        if (down & KEY_TOUCH) { touchRead(&tp); touched = 1; tx = tp.px; ty = tp.py; }
        frame++;

        if (state == 0) {
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
        DC_FlushRange(buf, sizeof buf);
        DC_FlushRange(sbuf, sizeof sbuf);
        dmaCopy(buf, fb, sizeof buf);
        dmaCopy(sbuf, sfb, sizeof sbuf);
    }
    return 0;
}
