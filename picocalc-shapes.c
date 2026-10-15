/*
 *  Shapes Demo
 *  ===========
 *
 *  Standalone PicoCalc demo: six filled shapes on the 320x320 screen, in
 *  two rows of three, left to right: a thin (fixed 5px) rectangle, the
 *  original square and a filled equilateral triangle (apex up) across the
 *  top row; a filled regular pentagon (point up), a filled five-pointed
 *  star and a filled circle across the bottom row - see the SHAPE_* enum
 *  and shape_center() for that exact order.  Each shape has its own
 *  `size[NUM_SHAPES]` entry - its bounding-box dimension (the rectangle's
 *  long side, the square's side, the triangle's base, the pentagon/star's
 *  circumscribed diameter, the circle's diameter) - all starting
 *  maximised at INIT_SIZE (== MAX_SIZE, 49).  + and - grow or
 *  shrink only the *selected* shape's size[], each press moving the (not
 *  displayed) bounding box that inscribes its pixels by 2 pixels in every
 *  direction (left, right, top and bottom) - stopping at the 1x1-pixel
 *  degenerate case on -.  MIN_SIZE, MAX_SIZE and INIT_SIZE are all odd,
 *  and every + / - step is by 4 (even), so a size can never become even:
 *  with h = size/2 (integer division), an odd size spans exactly
 *  [cx-h, cx+h] about its centre, so one pixel sits dead-centre and a
 *  rotation about (cx,cy) doesn't drift; an even size spans one pixel
 *  off-centre.
 *
 *  The only drawing primitive the display driver gives us is a solid
 *  rectangle (lcd_solid_rectangle / our fill() wrapper) - there is no
 *  set-pixel or circle/polygon call.  So the circle is rasterised as a
 *  stack of 1-pixel-tall horizontal rows (half-width per row from
 *  Pythagoras, isqrt(radius^2 - dy^2)) and the thin rectangle / square /
 *  triangle / pentagon / star are each a small set of corners
 *  (regular_polygon() places the pentagon's and star's; the star
 *  alternates an outer and inner radius every other vertex) run through
 *  fill_polygon() - a generic scanline polygon fill (same "one row at a
 *  time" idea, but from edge/scanline intersections instead of a per-
 *  shape formula, so it works for any vertex list, convex or not - the
 *  star is concave and still fills correctly).
 *
 *  Rotation: draw_thin_rect(), draw_square(), draw_triangle(),
 *  draw_pentagon() and draw_star() each take an angle_deg parameter - 0
 *  uses the shape's original unrotated corners, any other value rotates
 *  them with rotate_point() (fixed-point x1000 sine/cosine from
 *  SIN_TABLE_Q1000, so no <math.h> or pico_float) before running them
 *  through fill_polygon(), which needs no changes for a rotated polygon
 *  since it works from the
 *  (now-rotated) corner coordinates alone.  draw_circle() takes no angle:
 *  a circle is rotationally symmetric about its own centre, so there's
 *  nothing for a rotation to change.  main() keeps one angle per shape in
 *  `angle[NUM_SHAPES]`, so each shape remembers its own rotation across
 *  LEFT/RIGHT selection changes; F4/F5 turn the *selected* shape's entry
 *  by -/+ ROTATE_STEP (5) degrees, wrapped to 0..359.
 *
 *  One shape at a time is "selected" (index `selected`, SHAPE_THIN_RECT
 *  first) and gets a hollow white box drawn BOX_PAD pixels outside its own
 *  size x size box (draw_box_outline() - four thin fill() strips, since
 *  there's no unfilled-rectangle primitive either).  F4/F5 always rotate
 *  whichever shape is currently selected.  shape_center() is the one
 *  place each shape's (cx,cy) is defined, so draw_scene()'s drawing loop
 *  and the selection box always agree on where a shape is.
 *
 *  `mode` (MODE_SHAPE, or one of the pickers below) decides what LEFT and
 *  RIGHT do.  In shape-select mode they step `selected` forward/back
 *  through SHAPE_* / reading order, wrapping at both ends (circle -> thin
 *  rectangle going forward, and back).  In a picker mode they change the
 *  selected shape's hue, saturation or value.  F1 steps shape -> hue (plain
 *  strip) -> hue (improved strip) -> shape, F2 toggles saturation and F3
 *  toggles value; UP and DOWN walk the same modes as one vertical chain
 *  (see the Controls list).
 *
 *  The pickers are stacked above the RGB text row (COLOUR_ROW): hue on
 *  top, then saturation, then value.  Hue is two RAINBOW_W x RAINBOW_H
 *  strips flush against each other: the plain hue wheel on top and an
 *  improved wheel below it (each linear channel ramp v is remapped to
 *  sqrt(v * (510 - v)), so the colours between the primaries are not
 *  dimmer than the primaries).  Both map a column to the same hue
 *  (col * 360 / RAINBOW_W), so all RAINBOW_W columns sweep the full 360
 *  degrees exactly once, whatever RAINBOW_W is.  LEFT/RIGHT in a hue mode
 *  step the selected shape's hue[] entry by HUE_STEP (2) degrees, wrapped
 *  to 0..359, and record which wheel the shape is coloured from
 *  (wheel[NUM_SHAPES]).  The saturation bar runs from white at the left
 *  end to the full colour at the right, and the value bar from black to
 *  the full colour.  Each shape keeps its own hue, wheel, sat[] and val[]
 *  (DEFAULT_HUE, pure green, at full saturation and value at boot), the
 *  same way angle[] is remembered.  Small arrows (draw_colour_arrow() and
 *  draw_bar_arrow()) always show the selected shape's hue, saturation and
 *  value; a white outline (draw_bar_outline()) marks the picker being
 *  edited.  The selected shape's RGB565 fields are shown in hex on the text
 *  row under the value bar.
 *
 *  The status text row (STATUS_ROW, one row above MAX_ROW - the physical
 *  PicoCalc's screen/bezel makes the very bottom row hard to see, so it's
 *  reserved for a possible future debug line and nothing else uses it)
 *  shows the selected shape's name, size and rotation via put_row() /
 *  show_status() and the built-in 8x10 font, e.g.
 *  "SQUARE  SIZE: 49px  ROTATION: 35 DEG", updated on every
 *  + / - / LEFT / RIGHT / F4 / F5 press.
 *
 *  The strips are built once into static RAINBOW_W x RAINBOW_H buffers and
 *  sent with a single lcd_blit() each, not one fill() per column (see
 *  draw_rainbow_strip()'s own comment for why that used to be visibly
 *  slow).  The saturation and value bars depend on the selected shape, so
 *  draw_bar() rebuilds one only when the hue, wheel or the other setting
 *  changed.  The colour math is hue_to_rgb565(): it walks a pure hue
 *  (0..359 degrees, matching the ROTATION display's units) around the
 *  standard six-segment colour wheel, applies the wheel, the saturation
 *  (blend toward white) and the value (scale toward black), and packs the
 *  result into the RGB565 that lcd_solid_rectangle() etc. expect (via the
 *  driver's own RGB() macro).  draw_shape() gets its colour from the same
 *  function (through draw_scene()), so a shape's fill always matches the
 *  arrows.  rgb565_to_hue(uint16_t) is the inverse (unpack 5/6/5 back to
 *  8 bits per channel, then the standard max/min-channel HSV hue formula);
 *  nothing needs it yet, so expect a harmless "defined but not used" -Wall
 *  warning on it alone.  Both are integer-only, like the rest of this
 *  file's math.
 *
 *  Built on the drivers from "picocalc-text-starter" by Blair Leduc
 *  (drivers/lcd.c, drivers/southbridge.c): lcd_init(), lcd_solid_rectangle(),
 *  lcd_putstr() for drawing and sb_init() / sb_read_keyboard() for raw
 *  keyboard-FIFO polling.  We do NOT call picocalc_init(), which would start
 *  a background poll draining the same FIFO.
 *
 *  Controls:
 *      + / -  - grow / shrink the selected shape (2px per side, 4px per
 *               dimension; stops at the 1x1 degenerate case)
 *      DOWN   - one step down a vertical chain: top-row shape, the shape
 *               below it, then the hue strips, saturation bar and value bar
 *               (the same modes F1, F2 and F3 pick directly)
 *      UP     - one step back up the same chain, ending on the selected
 *               shape in the bottom row, then the shape above it
 *      F1     - step shape-select -> hue (plain wheel) -> hue (improved
 *               wheel) -> shape-select
 *      F2     - toggle saturation mode (0 to 100 percent in steps of 2)
 *      F3     - toggle value (brightness) mode (0 to 100 percent in steps
 *               of 2)
 *      RIGHT  - shape-select mode: move the white box to the next shape;
 *               picker modes: step the selected setting up (a hue moves its
 *               arrow right)
 *      LEFT   - shape-select mode: move the white box to the previous
 *               shape; picker modes: step the selected setting down
 *      F5     - rotate the selected shape 5 degrees clockwise
 *      F4     - rotate the selected shape 5 degrees counter-clockwise
 *      ?      - show the help screen (also shown at the start); any key
 *               continues
 *      ESC or Q - leave for the PicoCalc UF2 Loader menu
 *      ~ (SHIFT + backtick) - reboot into BOOTSEL (USB drive) mode
 */

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/bootrom.h"   /* reset_usb_boot() - BOOTSEL reboot ('~') */
#include "hardware/watchdog.h" /* watchdog_hw, watchdog_reboot() - exit to the UF2 Loader */

#include "loader_const.h"   /* the UF2 Loader hand-off values           */

#include "lcd.h"            /* WIDTH, HEIGHT, RGB(), lcd_*, GLYPH_HEIGHT,
                             * ROWS, MAX_ROW                            */
#include "southbridge.h"    /* sb_init(), sb_read_keyboard()           */
#include "keyboard.h"       /* KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
                             * KEY_F1 to KEY_F5, KEY_STATE_*          */

#include "shapes_const.h"   /* every constant and data table            */



/* the wheel (strip) a hue mode edits, or -1 when `mode` is not a hue mode */
static int mode_wheel(int mode)
{
    return (mode >= MODE_COLOUR_0 && mode <= MODE_COLOUR_1)
         ? mode - MODE_COLOUR_0 : -1;
}

static inline uint16_t key_event(void)      { return sb_read_keyboard(); }
static inline uint8_t  ev_state(uint16_t e) { return (e >> 8) & 0xFF; }
static inline uint8_t  ev_code(uint16_t e)  { return e & 0xFF; }

/* clipped solid rectangle (off-screen edges are trimmed, not skipped) */
static void fill(uint16_t colour, int x, int y, int w, int h)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > WIDTH)  w = WIDTH  - x;
    if (y + h > HEIGHT) h = HEIGHT - y;
    if (w > 0 && h > 0)
        lcd_solid_rectangle(colour, (uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h);
}

/* integer square root (Newton's method) - avoids pulling in libm/pico_float
 * for the one sqrt the circle rasteriser needs                            */
static int isqrt(int n)
{
    if (n <= 0)
        return 0;
    int x = n;
    int y = (x + 1) / 2;
    while (y < x) {
        x = y;
        y = (x + n / x) / 2;
    }
    return x;
}

/* ===================================================================== */
/*  Colour: rainbow hue <-> RGB565 (see the colour-selection header note) */
/* ===================================================================== */

/*
 *  hue_to_rgb888() walks a pure (full saturation, full brightness) hue
 *  around the classic six-segment colour wheel - red -> yellow -> green
 *  -> cyan -> blue -> magenta -> red - the same construction as the
 *  ubiquitous embedded "Wheel()" rainbow function (e.g. Adafruit's
 *  NeoPixel examples), just parameterised in degrees (0..359) instead of
 *  a 0..255 byte, to match this file's ROTATION-in-degrees style.  Each
 *  60-degree segment ramps one channel linearly up or down between 0 and
 *  255 while the other two hold at 0 and 255; that ramp is `rising` /
 *  `falling` below.  Integer-only, like the rest of this file's math.
 */
/* Improved wheel: a linear ramp v (0..255) is remapped to the quarter
 * circle sqrt(v * (510 - v)).  0 stays 0 and 255 stays 255, but the values
 * in between rise faster, so the colours between the primaries are not
 * dimmer than the primaries themselves (integer square root, no libm).    */
static int round_ramp(int v)
{
    return isqrt(v * (510 - v));
}

static void hue_to_rgb888(int hue_deg, int wheel, int sat, int val,
                          uint8_t *r, uint8_t *g, uint8_t *b)
{
    hue_deg = ((hue_deg % 360) + 360) % 360;   /* normalise to 0..359 */
    int region  = hue_deg / 60;                /* which 60-degree segment  */
    int rem     = hue_deg % 60;                /* position within it, 0..59*/
    int rising  = (rem * 255) / 59;
    int falling = 255 - rising;

    if (wheel == WHEEL_ROUND) {
        rising  = round_ramp(rising);
        falling = round_ramp(falling);
    }

    switch (region) {
    case 0: *r = 255;     *g = rising;  *b = 0;       break; /* red    -> yellow  */
    case 1: *r = falling; *g = 255;     *b = 0;       break; /* yellow -> green   */
    case 2: *r = 0;       *g = 255;     *b = rising;  break; /* green  -> cyan    */
    case 3: *r = 0;       *g = falling; *b = 255;     break; /* cyan   -> blue    */
    case 4: *r = rising;  *g = 0;       *b = 255;     break; /* blue   -> magenta */
    default:*r = 255;     *g = 0;       *b = falling; break; /* magenta-> red     */
    }

    /* saturation: blend each channel toward white (100 = the pure colour) */
    if (sat < SAT_MAX) {
        *r = (uint8_t)(255 - (255 - *r) * sat / SAT_MAX);
        *g = (uint8_t)(255 - (255 - *g) * sat / SAT_MAX);
        *b = (uint8_t)(255 - (255 - *b) * sat / SAT_MAX);
    }

    /* value: scale each channel toward black (100 = full brightness)      */
    if (val < VAL_MAX) {
        *r = (uint8_t)(*r * val / VAL_MAX);
        *g = (uint8_t)(*g * val / VAL_MAX);
        *b = (uint8_t)(*b * val / VAL_MAX);
    }
}

/* hue (0..359 degrees, wraps) -> packed RGB565, via the display driver's
 * own RGB() macro (lcd.h) so the 8-bit-per-channel -> 5/6/5 truncation is
 * exactly what lcd_solid_rectangle() etc. already expect.                */
static uint16_t hue_to_rgb565(int hue_deg, int wheel, int sat, int val)
{
    uint8_t r, g, b;
    hue_to_rgb888(hue_deg, wheel, sat, val, &r, &g, &b);
    return RGB(r, g, b);
}

/* unpack RGB565 back to 8-bit-per-channel RGB888, expanding each 5/6-bit
 * channel by replicating its own high bits into the new low bits (e.g.
 * r5<<3 | r5>>2) rather than a plain left-shift, so 0x1F -> 255 exactly
 * instead of 0xF8 - the usual accurate 5/6-bit -> 8-bit expansion.        */
static void rgb565_to_rgb888(uint16_t colour, uint8_t *r, uint8_t *g, uint8_t *b)
{
    uint8_t r5 = (colour >> 11) & 0x1F;
    uint8_t g6 = (colour >> 5)  & 0x3F;
    uint8_t b5 =  colour        & 0x1F;
    *r = (uint8_t)((r5 << 3) | (r5 >> 2));
    *g = (uint8_t)((g6 << 2) | (g6 >> 4));
    *b = (uint8_t)((b5 << 3) | (b5 >> 2));
}

/* RGB888 -> hue in degrees (0..359); the standard max/min-channel HSV hue
 * formula, kept in integer arithmetic throughout.  A grey/black/white
 * pixel (r==g==b, delta 0) has no defined hue - returns 0 (red) rather
 * than dividing by zero, since a colour picker needs some starting point.*/
static int rgb888_to_hue(uint8_t r, uint8_t g, uint8_t b)
{
    int maxc = r; if (g > maxc) maxc = g; if (b > maxc) maxc = b;
    int minc = r; if (g < minc) minc = g; if (b < minc) minc = b;
    int delta = maxc - minc;
    if (delta == 0)
        return 0;

    int hue6;
    if (maxc == r)
        hue6 = ((g - b) * 60) / delta;             /* -60..60   (wraps below) */
    else if (maxc == g)
        hue6 = ((b - r) * 60) / delta + 120;        /*  60..180                */
    else
        hue6 = ((r - g) * 60) / delta + 240;        /* 180..300                */

    return ((hue6 % 360) + 360) % 360;
}

/* packed RGB565 -> hue in degrees (0..359) - the inverse of hue_to_rgb565(),
 * e.g. to find where a shape's already-chosen colour sits on the rainbow
 * stripe so a picker can start its cursor there.                          */
static int rgb565_to_hue(uint16_t colour)
{
    uint8_t r, g, b;
    rgb565_to_rgb888(colour, &r, &g, &b);
    return rgb888_to_hue(r, g, b);
}

/* one RAINBOW_W x RAINBOW_H hue strip (there are NUM_WHEELS of them), each
 * column's hue proportional to its position (col * 360 / RAINBOW_W) so
 * the RAINBOW_W columns together sweep the full 360-degree wheel exactly
 * once, at whatever width RAINBOW_W is.  Doubles as the hue picker:
 * draw_colour_arrow() points at whichever column the selected shape's
 * hue[] currently matches (the same formula, inverted - see
 * draw_scene()).
 *
 * Built and sent as ONE lcd_blit() of the whole RAINBOW_W x RAINBOW_H
 * buffer, not RAINBOW_W separate fill() calls: lcd_solid_rectangle() (and
 * so fill()) issues one lcd_blit() - one SPI window-set + data-write -
 * per row regardless of width, so drawing this one column at a time used
 * to cost RAINBOW_W * RAINBOW_H (300 * 20 = 6000) tiny SPI transactions
 * every redraw and was visibly slow.  `buf` is `static` (not a ~12KB
 * stack array - the RP2040/2350 stack is nowhere near that big) and
 * built only once, the first call (`built` latches that): the strip's
 * pixels never change at runtime, only the arrow drawn above it does, so
 * every call after the first is just the one blit.                       */
static void draw_rainbow_strip(int x0, int y0, int wheel)
{
    static uint16_t buf[NUM_WHEELS][RAINBOW_W * RAINBOW_H];
    static bool built[NUM_WHEELS];

    if (!built[wheel]) {
        for (int col = 0; col < RAINBOW_W; col++) {
            uint16_t colour = hue_to_rgb565(col * 360 / RAINBOW_W, wheel, SAT_MAX, VAL_MAX);
            for (int row = 0; row < RAINBOW_H; row++)
                buf[wheel][row * RAINBOW_W + col] = colour;
        }
        built[wheel] = true;
    }

    lcd_blit(buf[wheel], x0, y0, RAINBOW_W, RAINBOW_H);
}

/* The saturation bar (vary_sat true: white at the left to the colour at
 * the right) and the value bar (vary_sat false: black at the left to the
 * colour at the right): RAINBOW_W columns each, at `hue` on `wheel`.  The
 * other setting (value for the saturation bar, saturation for the value
 * bar) is the selected shape's own, so each bar shows the colours the
 * shape can have.  A bar is rebuilt (one row of colours, copied down the
 * bar) only when the hue, the wheel or that other setting differs from the
 * last build; otherwise the saved pixels are just sent again.              */
static void draw_bar(int y0, int hue, int wheel, int sat, int val,
                     bool vary_sat)
{
    static uint16_t buf[2][RAINBOW_W * SAT_H];
    static int built_for[2][3];            /* hue, wheel, other setting   */
    static bool built[2];

    int i = vary_sat ? 0 : 1;
    int other = vary_sat ? val : sat;

    if (!built[i] || built_for[i][0] != hue || built_for[i][1] != wheel
        || built_for[i][2] != other) {
        for (int col = 0; col < RAINBOW_W; col++) {
            int pct = col * SAT_MAX / (RAINBOW_W - 1);
            buf[i][col] = vary_sat ? hue_to_rgb565(hue, wheel, pct, val)
                                   : hue_to_rgb565(hue, wheel, sat, pct);
        }
        for (int row = 1; row < SAT_H; row++)
            memcpy(&buf[i][row * RAINBOW_W], buf[i], RAINBOW_W * sizeof buf[i][0]);
        built_for[i][0] = hue;
        built_for[i][1] = wheel;
        built_for[i][2] = other;
        built[i] = true;
    }

    lcd_blit(buf[i], RAINBOW_X, y0, RAINBOW_W, SAT_H);
}

/* ===================================================================== */
/*  Rotation math (see header comment)                                    */
/* ===================================================================== */

typedef struct { int x, y; } point_t;

static int sin1000(int deg)
{
    deg = ((deg % 360) + 360) % 360;      /* normalise to 0..359 */
    if (deg <= 90)  return  SIN_TABLE_Q1000[deg];
    if (deg <= 180) return  SIN_TABLE_Q1000[180 - deg];
    if (deg <= 270) return -SIN_TABLE_Q1000[deg - 180];
    return -SIN_TABLE_Q1000[360 - deg];
}

static int cos1000(int deg) { return sin1000(deg + 90); }

/* rotate (px,py) by deg degrees about (cx,cy); screen y grows downward,
 * so a positive angle turns clockwise as drawn.                          */
static point_t rotate_point(int px, int py, int cx, int cy, int deg)
{
    int s = sin1000(deg), c = cos1000(deg);
    int dx = px - cx, dy = py - cy;
    point_t r;
    r.x = cx + (dx * c - dy * s) / 1000;
    r.y = cy + (dx * s + dy * c) / 1000;
    return r;
}

/* Filled convex polygon via horizontal scanline fill: for each row inside
 * the polygon's y-range, intersect every edge with that scanline, sort
 * the x hits and fill the span(s) between them.  Takes any vertex list -
 * in particular, rotate_point()'s output - so it's the one rasteriser a
 * rotated shape needs; no per-shape scanline math like draw_circle() has.*/
static void fill_polygon(const point_t *pts, int n, uint16_t colour)
{
    int ymin = pts[0].y, ymax = pts[0].y;
    for (int i = 1; i < n; i++) {
        if (pts[i].y < ymin) ymin = pts[i].y;
        if (pts[i].y > ymax) ymax = pts[i].y;
    }

    for (int y = ymin; y <= ymax; y++) {
        int xs[8];                    /* plenty for the small shapes here */
        int count = 0;
        for (int i = 0; i < n; i++) {
            point_t a = pts[i], b = pts[(i + 1) % n];
            if (a.y == b.y)
                continue;                       /* horizontal edge: skip */
            if ((y >= a.y && y < b.y) || (y >= b.y && y < a.y)) {
                int x = a.x + (int)((long)(y - a.y) * (b.x - a.x) / (b.y - a.y));
                if (count < (int)(sizeof xs / sizeof xs[0]))
                    xs[count++] = x;
            }
        }
        for (int i = 1; i < count; i++) {        /* insertion sort: count is tiny */
            int key = xs[i], j = i - 1;
            while (j >= 0 && xs[j] > key) { xs[j + 1] = xs[j]; j--; }
            xs[j + 1] = key;
        }
        for (int i = 0; i + 1 < count; i += 2)
            fill(colour, xs[i], y, xs[i + 1] - xs[i] + 1, 1);
    }
}

/* ===================================================================== */
/*  Shapes                                                                */
/* ===================================================================== */

/* size x size square, centred at (cx, cy), rotated angle_deg degrees
 * (0 = axis-aligned - the original single fill() call, unchanged output),
 * filled with `colour` (each shape now has its own - see DEFAULT_HUE)    */
static void draw_square(int cx, int cy, int size, int angle_deg, uint16_t colour)
{
    int h = size / 2;
    if (angle_deg == 0) {
        fill(colour, cx - h, cy - h, size, size);
        return;
    }
    point_t corners[4] = {
        rotate_point(cx - h, cy - h, cx, cy, angle_deg),
        rotate_point(cx + h, cy - h, cx, cy, angle_deg),
        rotate_point(cx + h, cy + h, cx, cy, angle_deg),
        rotate_point(cx - h, cy + h, cx, cy, angle_deg),
    };
    fill_polygon(corners, 4, colour);
}

/* size x THIN_WIDTH rectangle, centred at (cx, cy), rotated angle_deg
 * degrees - `size` is the long side, same as every other shape's shared
 * size[]; THIN_WIDTH (5px) never changes.  Same construction as
 * draw_square(): a single fast fill() when unrotated, else four rotated
 * corners through fill_polygon().                                        */
static void draw_thin_rect(int cx, int cy, int size, int angle_deg, uint16_t colour)
{
    int hw = size / 2;
    int hh = THIN_WIDTH / 2;
    if (angle_deg == 0) {
        fill(colour, cx - hw, cy - hh, size, THIN_WIDTH);
        return;
    }
    point_t corners[4] = {
        rotate_point(cx - hw, cy - hh, cx, cy, angle_deg),
        rotate_point(cx + hw, cy - hh, cx, cy, angle_deg),
        rotate_point(cx + hw, cy + hh, cx, cy, angle_deg),
        rotate_point(cx - hw, cy + hh, cx, cy, angle_deg),
    };
    fill_polygon(corners, 4, colour);
}

/* `size`-diameter filled circle, centred at (cx, cy): one 1px-tall row per
 * scanline, each row's half-width from Pythagoras (isqrt), clamped to at
 * least 1px so no row - including the top/bottom cap - vanishes.  A
 * circle is rotationally symmetric about its own centre, so unlike the
 * square / triangle it takes no angle_deg - there's nothing to rotate.   */
static void draw_circle(int cx, int cy, int size, uint16_t colour)
{
    int radius = size / 2;

    for (int dy = -radius; dy <= radius; dy++) {
        int hw = isqrt(radius * radius - dy * dy);
        if (hw < 1)
            hw = 1;
        fill(colour, cx - hw, cy + dy, 2 * hw, 1);
    }
}

/* `size`-base filled equilateral triangle, apex up, centred at (cx, cy),
 * rotated angle_deg degrees (0 = the original per-row apex-to-base fill,
 * unchanged output).  height = size * sqrt(3)/2 via fixed-point (866/1000).*/
static void draw_triangle(int cx, int cy, int size, int angle_deg, uint16_t colour)
{
    int height = (size * 866 + 500) / 1000;
    if (height < 1)
        height = 1;
    int top_y     = cy - height / 2;
    int half_base = size / 2;

    if (angle_deg == 0) {
        for (int row = 0; row < height; row++) {
            int hw = (height <= 1) ? half_base : (row * half_base) / (height - 1);
            if (hw < 1)
                hw = 1;
            fill(colour, cx - hw, top_y + row, 2 * hw, 1);
        }
        return;
    }

    point_t corners[3] = {
        rotate_point(cx,             top_y,              cx, cy, angle_deg),
        rotate_point(cx + half_base, top_y + height - 1,  cx, cy, angle_deg),
        rotate_point(cx - half_base, top_y + height - 1,  cx, cy, angle_deg),
    };
    fill_polygon(corners, 3, colour);
}

/* n evenly spaced points on a circle of `radius` around (cx, cy), the
 * first one straight up - the shared "point up, then clockwise" base
 * orientation every polygon shape here starts from before any rotation.  */
static void regular_polygon(point_t *out, int n, int cx, int cy, int radius)
{
    for (int k = 0; k < n; k++) {
        int deg = -90 + (k * 360) / n;
        out[k].x = cx + radius * cos1000(deg) / 1000;
        out[k].y = cy + radius * sin1000(deg) / 1000;
    }
}

/* `size`-diameter filled regular pentagon, point up, centred at (cx, cy),
 * rotated angle_deg degrees (0 = the base regular_polygon() corners).     */
static void draw_pentagon(int cx, int cy, int size, int angle_deg, uint16_t colour)
{
    point_t corners[5];
    regular_polygon(corners, 5, cx, cy, size / 2);
    if (angle_deg != 0)
        for (int i = 0; i < 5; i++)
            corners[i] = rotate_point(corners[i].x, corners[i].y, cx, cy, angle_deg);
    fill_polygon(corners, 5, colour);
}

/* `size`-diameter (outer points) filled five-pointed star, point up,
 * centred at (cx, cy), rotated angle_deg degrees.  10 vertices alternating
 * an outer (point) and inner radius every 36 degrees; concave, but
 * fill_polygon() fills any simple polygon, not just convex ones.          */
static void draw_star(int cx, int cy, int size, int angle_deg, uint16_t colour)
{
    int r_out = size / 2;
    int r_in  = r_out * STAR_INNER_RATIO_Q1000 / 1000;
    if (r_in < 1)
        r_in = 1;

    point_t pts[10];
    for (int k = 0; k < 10; k++) {
        int deg = -90 + k * 36;
        int r   = (k % 2 == 0) ? r_out : r_in;
        pts[k].x = cx + r * cos1000(deg) / 1000;
        pts[k].y = cy + r * sin1000(deg) / 1000;
    }
    if (angle_deg != 0)
        for (int i = 0; i < 10; i++)
            pts[i] = rotate_point(pts[i].x, pts[i].y, cx, cy, angle_deg);
    fill_polygon(pts, 10, colour);
}

/* ===================================================================== */
/*  Shape selection (the LEFT/RIGHT keys step through these)              */
/* ===================================================================== */

/* every shape's centre - the one place the row/column layout lives, so
 * draw_scene() and the selection box below always agree on where a shape
 * is.  Three shapes per row, sharing the same WIDTH/6-based columns as
 * the top row always has, so top and bottom line up.                     */
static void shape_center(int shape, int *cx, int *cy)
{
    int top_cy = TOP_ROW_CY;
    int bot_cy = BOT_ROW_CY;

    switch (shape) {
    case SHAPE_THIN_RECT: *cx = WIDTH / 6;         *cy = top_cy; break;
    case SHAPE_TRIANGLE:  *cx = WIDTH / 2;         *cy = top_cy; break;
    case SHAPE_SQUARE:    *cx = WIDTH - WIDTH / 6; *cy = top_cy; break;
    case SHAPE_PENTAGON:  *cx = WIDTH / 6;         *cy = bot_cy; break;
    case SHAPE_STAR:      *cx = WIDTH / 2;         *cy = bot_cy; break;
    case SHAPE_CIRCLE:    *cx = WIDTH - WIDTH / 6; *cy = bot_cy; break;
    }
}

/* angle_deg is ignored for the circle - it's rotationally symmetric about
 * its own centre, so there's nothing for a rotation to change (see the
 * header comment).  `colour` is that shape's own RGB565 (from its hue[]
 * entry, via hue_to_rgb565() - see the colour-selection header comment). */
static void draw_shape(int shape, int cx, int cy, int size, int angle_deg, uint16_t colour)
{
    switch (shape) {
    case SHAPE_THIN_RECT: draw_thin_rect(cx, cy, size, angle_deg, colour); break;
    case SHAPE_TRIANGLE:  draw_triangle (cx, cy, size, angle_deg, colour); break;
    case SHAPE_SQUARE:    draw_square   (cx, cy, size, angle_deg, colour); break;
    case SHAPE_PENTAGON:  draw_pentagon (cx, cy, size, angle_deg, colour); break;
    case SHAPE_STAR:      draw_star     (cx, cy, size, angle_deg, colour); break;
    case SHAPE_CIRCLE:    draw_circle   (cx, cy, size,            colour); break;
    }
}

/* hollow BOX_THICK-wide square outline, BOX_PAD bigger on every side than
 * the shape's own size x size box, centred at (cx, cy) - built from four
 * thin fill() strips since the driver has no unfilled-rectangle primitive */
static void draw_box_outline(int cx, int cy, int size, uint16_t colour)
{
    int half = size / 2 + BOX_PAD;
    int x0 = cx - half, y0 = cy - half;
    int side = 2 * half;

    fill(colour, x0, y0, side, BOX_THICK);                    /* top    */
    fill(colour, x0, y0 + side - BOX_THICK, side, BOX_THICK);  /* bottom */
    fill(colour, x0, y0, BOX_THICK, side);                    /* left   */
    fill(colour, x0 + side - BOX_THICK, y0, BOX_THICK, side);  /* right  */
}

/* filled triangle with its apex at (x, apex_y): pointing down (apex at the
 * bottom, base above it) when `down`, otherwise pointing up                */
static void draw_arrow(int x, int apex_y, bool down)
{
    int base_y = down ? apex_y - ARROW_H : apex_y + ARROW_H;
    point_t tri[3] = {
        { x,                apex_y },
        { x + ARROW_HALF_W, base_y },
        { x - ARROW_HALF_W, base_y },
    };
    fill_polygon(tri, 3, COL_ARROW);
}

/* arrow marking the selected shape's current hue - hue_x is the screen x
 * of that hue's strip column (caller computes it: RAINBOW_X + the hue's
 * column, the inverse of draw_rainbow_strip()'s col -> hue mapping), so
 * the arrow always points straight at the exact column the shape's colour
 * comes from.  The plain wheel's arrow sits above its strip and points
 * down; the improved wheel's sits below its strip and points up, because
 * the strips touch and there is no room between them.                    */
static void draw_colour_arrow(int hue_x, int wheel)
{
    if (wheel == WHEEL_PLAIN)
        draw_arrow(hue_x, WHEEL_Y(wheel) - ARROW_GAP, true);
    else
        draw_arrow(hue_x, WHEEL_Y(wheel) + RAINBOW_H + ARROW_GAP, false);
}

/* white outline just outside a bar, BOX_THICK wide, marking the bar the
 * current mode edits.  The two rainbow strips touch, so the edge they
 * share is left out (skip_top for the lower strip, skip_bottom for the
 * upper one) instead of being drawn over the neighbouring strip.         */
static void draw_bar_outline(int y0, int h, bool skip_top, bool skip_bottom)
{
    int x0 = RAINBOW_X - BOX_THICK;
    int w  = RAINBOW_W + 2 * BOX_THICK;

    if (!skip_top)
        fill(COL_BAR_BOX, x0, y0 - BOX_THICK, w, BOX_THICK);
    if (!skip_bottom)
        fill(COL_BAR_BOX, x0, y0 + h, w, BOX_THICK);
    fill(COL_BAR_BOX, x0, y0, BOX_THICK, h);
    fill(COL_BAR_BOX, RAINBOW_X + RAINBOW_W, y0, BOX_THICK, h);
}

/* downward arrow above a bar whose top edge is bar_y, at the column for
 * `pct` (0..100)                                                          */
static void draw_bar_arrow(int bar_y, int pct)
{
    draw_arrow(RAINBOW_X + pct * (RAINBOW_W - 1) / SAT_MAX,
               bar_y - ARROW_GAP, true);
}

/* ===================================================================== */
/*  Bottom status line                                                    */
/* ===================================================================== */

static void clear_row(int row)
{
    fill(COL_BG, 0, row * GLYPH_HEIGHT, WIDTH, GLYPH_HEIGHT);
}

/* clear a text row and print a string centred on it */
static void put_row(int row, const char *str, uint16_t fg)
{
    clear_row(row);
    int len = (int)strlen(str);
    int col = (TCOLS - len) / 2;
    if (col < 0) col = 0;
    lcd_set_foreground(fg);
    lcd_putstr((uint8_t)col, (uint8_t)row, str);
}

/* shows the selected shape's name, size and rotation, e.g.
 * "SQUARE  SIZE: 49px  ROTATION: 35 DEG"                                  */
static void show_status(int selected, int size, int angle_deg)
{
    char buf[40];
    snprintf(buf, sizeof buf, "%s  SIZE: %dpx  ROTATION: %d DEG",
             SHAPE_NAME[selected], size, angle_deg);
    put_row(STATUS_ROW, buf, COL_TEXT);
}

/* shows the selected shape's colour as the three RGB565 fields in hex:
 * R and B are 5 bits (00..1F), G is 6 bits (00..3F), e.g. "R:00 G:3F B:00" */
static void show_colour(uint16_t colour)
{
    char buf[24];
    snprintf(buf, sizeof buf, "R:%02X G:%02X B:%02X",
             (colour >> 11) & 0x1F, (colour >> 5) & 0x3F, colour & 0x1F);
    put_row(COLOUR_ROW, buf, COL_TEXT);
}

/* clear the screen, draw all six shapes at their own size, rotation and
 * colour (size[] / angle[] / hue[] / wheel[] each have one entry per SHAPE_*
 * index), the selection box around `selected`, the two rainbow strips, the
 * colour-selection arrow (only in a colour mode), the saturation bar and
 * its arrow (only in saturation mode) and the status line                */
static void draw_scene(const int *size, int selected, const int *angle,
                        const int *hue, const int *wheel, const int *sat,
                        const int *val, int mode)
{
    fill(COL_BG, 0, 0, WIDTH, HEIGHT);

    for (int s = 0; s < NUM_SHAPES; s++) {
        int cx = 0, cy = 0;
        shape_center(s, &cx, &cy);
        draw_shape(s, cx, cy, size[s], angle[s], hue_to_rgb565(hue[s], wheel[s], sat[s], val[s]));
    }

    int scx = 0, scy = 0;
    shape_center(selected, &scx, &scy);
    draw_box_outline(scx, scy, size[selected], COL_SHAPE_BOX);

    for (int w = 0; w < NUM_WHEELS; w++)
        draw_rainbow_strip(RAINBOW_X, WHEEL_Y(w), w);
    draw_bar(SAT_Y, hue[selected], wheel[selected], sat[selected],
             val[selected], true);
    draw_bar(VAL_Y, hue[selected], wheel[selected], sat[selected],
             val[selected], false);
    /* the arrows are always shown for the selected shape's own hue,
     * saturation and value; the white outline marks the picker the current
     * mode edits.  In a hue mode the hue arrow and the outline are on the
     * same strip (the one being edited); otherwise the arrow is on the
     * strip of the wheel the shape is coloured from.                      */
    int edit_wheel = mode_wheel(mode);
    int arrow_wheel = (edit_wheel >= 0) ? edit_wheel : wheel[selected];

    /* inverse of draw_rainbow_strip()'s col * 360 / RAINBOW_W, rounded to
     * the nearest column instead of truncated                             */
    int col = (hue[selected] * RAINBOW_W + 180) / 360;
    draw_colour_arrow(RAINBOW_X + col, arrow_wheel);
    draw_bar_arrow(SAT_Y, sat[selected]);
    draw_bar_arrow(VAL_Y, val[selected]);

    if (mode == MODE_SATURATION)
        draw_bar_outline(SAT_Y, SAT_H, false, false);
    else if (mode == MODE_VALUE)
        draw_bar_outline(VAL_Y, VAL_H, false, false);
    else if (edit_wheel >= 0)
        draw_bar_outline(WHEEL_Y(edit_wheel), RAINBOW_H,
                         edit_wheel == WHEEL_ROUND, edit_wheel == WHEEL_PLAIN);
    show_colour(hue_to_rgb565(hue[selected], wheel[selected], sat[selected],
                              val[selected]));
    show_status(selected, size[selected], angle[selected]);
}

/* print a string starting at text column `col` of `row` */
static void put_text(int col, int row, const char *str, uint16_t fg)
{
    lcd_set_foreground(fg);
    lcd_putstr((uint8_t)col, (uint8_t)row, str);
}

/* The help screen: the title, author and version, the key list and a line
 * saying any key continues.  It waits for a key press (releases are
 * ignored) and the caller redraws the scene afterwards.                   */
static void show_help(void)
{
    char buf[TCOLS + 1];

    lcd_set_bold(true);
    fill(COL_BG, 0, 0, WIDTH, HEIGHT);
    put_row(HELP_TITLE_ROW, HELP_TITLE, COL_HELP_TITLE);
    put_row(HELP_AUTHOR_ROW, HELP_AUTHOR, COL_HELP_TEXT);
    snprintf(buf, sizeof buf, "Version: %s", VERSION);
    put_row(HELP_VERSION_ROW, buf, COL_HELP_TEXT);

    for (int i = 0; i < HELP_LINE_COUNT; i++)
        put_text(HELP_LEFT_COL, HELP_FIRST_ROW + i, HELP_LINES[i], COL_HELP_TEXT);

    put_row(HELP_FOOTER_ROW, HELP_FOOTER, COL_HELP_FOOTER);

    for (;;) {
        uint16_t e = key_event();
        if (e && ev_state(e) == KEY_STATE_PRESSED)
            break;
        sleep_ms(2);
    }
    lcd_set_bold(false);
}

/* Leaves the program for the PicoCalc UF2 Loader's menu: shows a short
 * message, asks the loader for its menu (see LOADER_COMMAND_MAGIC in
 * loader_const.h) and reboots with the watchdog. If the program was flashed
 * straight to the chip with no loader, nothing reads the request and it
 * simply restarts. The Windows / Linux build uses a stand-in watchdog that
 * closes the program.                                                     */
static void exit_to_loader(void)
{
    put_row(STATUS_ROW, "BACK TO THE LOADER...", COL_TEXT);
    sleep_ms(LOADER_EXIT_MSG_MS);

    watchdog_hw->scratch[LOADER_SCRATCH_MODE] = LOADER_BOOT_MODE_SD;
    watchdog_hw->scratch[LOADER_SCRATCH_ARGUMENT] = 0;
    watchdog_hw->scratch[LOADER_SCRATCH_MAGIC] = LOADER_COMMAND_MAGIC;
    watchdog_reboot(0, 0, LOADER_REBOOT_DELAY_MS);

    for (;;)
        tight_loop_contents();      /* the reboot comes in a few milliseconds */
}

int main(void)
{
    stdio_init_all();

    sb_init();                /* keyboard / south-bridge I2C (no bg poll) */
    lcd_init();                /* ST7365P LCD                              */
    lcd_enable_cursor(false);  /* no text cursor on this screen            */

    int size[NUM_SHAPES];
    int hue[NUM_SHAPES];
    int wheel[NUM_SHAPES];            /* which strip each hue is read from */
    int sat[NUM_SHAPES];              /* each shape's saturation, 0..SAT_MAX */
    int val[NUM_SHAPES];              /* each shape's value, 0..VAL_MAX      */
    for (int s = 0; s < NUM_SHAPES; s++) {
        size[s] = INIT_SIZE;
        hue[s]  = DEFAULT_HUE;
        wheel[s] = WHEEL_PLAIN;
        sat[s]   = DEFAULT_SAT;
        val[s]   = DEFAULT_VAL;
    }
    int selected  = SHAPE_THIN_RECT;  /* the first shape, per the white box */
    int angle[NUM_SHAPES] = { 0 };    /* each shape keeps its own rotation,
                                       * remembered across selection changes */
    int mode = MODE_SHAPE;            /* F1 steps shape / colour 0 / colour 1 */
    show_help();                      /* any key continues */
    draw_scene(size, selected, angle, hue, wheel, sat, val, mode);

    for (;;) {
        uint16_t e = key_event();
        if (!e) {
            sleep_ms(2);
            continue;
        }

        uint8_t code = ev_code(e);
        uint8_t st   = ev_state(e);

        if (st != KEY_STATE_PRESSED)
            continue;

        if (code == '~') {                      /* BOOTSEL reboot */
            reset_usb_boot(0, 0);
            while (1)
                tight_loop_contents();
        }

        if (code == KEY_ESC || code == 'q' || code == 'Q')
            exit_to_loader();                   /* back to the UF2 Loader */

        if (code == KEY_HELP) {
            show_help();
            draw_scene(size, selected, angle, hue, wheel, sat, val, mode);
        } else if (code == KEY_F1) {                     /* toggle shape / colour mode */
            mode = (mode >= NUM_MODES) ? MODE_SHAPE : (mode + 1) % NUM_MODES;
            draw_scene(size, selected, angle, hue, wheel, sat, val, mode);
        } else if (code == SAT_KEY) {               /* toggle saturation mode */
            mode = (mode == MODE_SATURATION) ? MODE_SHAPE : MODE_SATURATION;
            draw_scene(size, selected, angle, hue, wheel, sat, val, mode);
        } else if (code == VAL_KEY) {               /* toggle value mode */
            mode = (mode == MODE_VALUE) ? MODE_SHAPE : MODE_VALUE;
            draw_scene(size, selected, angle, hue, wheel, sat, val, mode);
        } else if (code == KEY_RIGHT) {
            if (mode == MODE_VALUE) {
                if (val[selected] < VAL_MAX) val[selected] += VAL_STEP;
                if (val[selected] > VAL_MAX) val[selected] = VAL_MAX;
            } else if (mode == MODE_SATURATION) {
                if (sat[selected] < SAT_MAX) sat[selected] += SAT_STEP;
                if (sat[selected] > SAT_MAX) sat[selected] = SAT_MAX;
            } else if (mode != MODE_SHAPE) {       /* step the arrow / hue forward */
                wheel[selected] = mode_wheel(mode);
                hue[selected] = (hue[selected] + HUE_STEP) % 360;
            } else {                               /* move the selection box forward */
                selected = (selected + 1) % NUM_SHAPES;
            }
            draw_scene(size, selected, angle, hue, wheel, sat, val, mode);
        } else if (code == KEY_LEFT) {
            if (mode == MODE_VALUE) {
                val[selected] -= VAL_STEP;
                if (val[selected] < 0) val[selected] = 0;
            } else if (mode == MODE_SATURATION) {
                sat[selected] -= SAT_STEP;
                if (sat[selected] < 0) sat[selected] = 0;
            } else if (mode != MODE_SHAPE) {       /* step the arrow / hue back */
                wheel[selected] = mode_wheel(mode);
                hue[selected] = (hue[selected] - HUE_STEP + 360) % 360;
            } else {                               /* move the selection box back */
                selected = (selected - 1 + NUM_SHAPES) % NUM_SHAPES;
            }
            draw_scene(size, selected, angle, hue, wheel, sat, val, mode);
        } else if (code == KEY_DOWN) {            /* next row / picker down */
            if (mode != MODE_SHAPE) {
                if (mode < MODE_VALUE)
                    mode++;
            } else if (selected < SHAPES_PER_ROW) {
                selected += SHAPES_PER_ROW;
            } else {
                mode = MODE_COLOUR_0;
            }
            draw_scene(size, selected, angle, hue, wheel, sat, val, mode);
        } else if (code == KEY_UP) {              /* previous row / picker up */
            if (mode != MODE_SHAPE)
                mode--;
            else if (selected >= SHAPES_PER_ROW)
                selected -= SHAPES_PER_ROW;
            draw_scene(size, selected, angle, hue, wheel, sat, val, mode);
        } else if (code == KEY_GROW) {            /* grow only the selected shape */
            if (size[selected] < MAX_SIZE) {
                size[selected] += 2 * GROW_PER_SIDE;
                if (size[selected] > MAX_SIZE) size[selected] = MAX_SIZE;
                draw_scene(size, selected, angle, hue, wheel, sat, val, mode);
            }
        } else if (code == KEY_SHRINK) {          /* shrink only the selected shape */
            if (size[selected] > MIN_SIZE) {
                size[selected] -= 2 * GROW_PER_SIDE;
                if (size[selected] < MIN_SIZE) size[selected] = MIN_SIZE;
                draw_scene(size, selected, angle, hue, wheel, sat, val, mode);
            }
        } else if (code == KEY_F5) {              /* rotate selected CW    */
            angle[selected] = (angle[selected] + ROTATE_STEP) % 360;
            draw_scene(size, selected, angle, hue, wheel, sat, val, mode);
        } else if (code == KEY_F4) {              /* rotate selected CCW   */
            angle[selected] = (angle[selected] - ROTATE_STEP + 360) % 360;
            draw_scene(size, selected, angle, hue, wheel, sat, val, mode);
        }
    }
}
