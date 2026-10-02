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
 *  maximised at INIT_SIZE (== MAX_SIZE, 59).  UP/DOWN grow or
 *  shrink only the *selected* shape's size[], each press moving the (not
 *  displayed) bounding box that inscribes its pixels by 2 pixels in every
 *  direction (left, right, top and bottom) - stopping at the 1x1-pixel
 *  degenerate case on DOWN.  MIN_SIZE, MAX_SIZE and INIT_SIZE are all odd,
 *  and every UP/DOWN step is by 4 (even), so a size can never become even:
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
 *  first) and gets a hollow blue box drawn BOX_PAD pixels outside its own
 *  size x size box (draw_box_outline() - four thin fill() strips, since
 *  there's no unfilled-rectangle primitive either).  F4/F5 always rotate
 *  whichever shape is currently selected.  shape_center() is the one
 *  place each shape's (cx,cy) is defined, so draw_scene()'s drawing loop
 *  and the selection box always agree on where a shape is.
 *
 *  F1 toggles `colour_mode` between two meanings for LEFT/RIGHT (bool
 *  colour_mode in main()):
 *    - shape-select mode (the default): RIGHT/LEFT step `selected`
 *      forward/back through SHAPE_* / reading order, wrapping at both
 *      ends (circle -> thin rectangle going forward, and back).
 *    - colour-select mode: RIGHT/LEFT instead step the *selected* shape's
 *      own `hue[NUM_SHAPES]` entry +/- HUE_STEP (2) degrees, wrapped to
 *      0..359, and a small downward-pointing arrow (draw_colour_arrow())
 *      appears above the rainbow strip at that hue's column (the inverse
 *      of the strip's own col -> hue mapping, rounded to the nearest
 *      column - see draw_scene()), so the arrow always points straight
 *      down at the exact stripe of rainbow the shape is now coloured
 *      with.  Every shape keeps its own hue (DEFAULT_HUE, pure green, at
 *      boot), remembered the same way angle[] is.
 *
 *  The status text row (STATUS_ROW, one row above MAX_ROW - the physical
 *  PicoCalc's screen/bezel makes the very bottom row hard to see, so it's
 *  reserved for a possible future debug line and nothing else uses it)
 *  shows the selected shape's name, size and rotation via put_row() /
 *  show_status() and the built-in 8x10 font, e.g.
 *  "SQUARE  SIZE: 59px  ROTATION: 35 DEG", updated on every
 *  UP / DOWN / LEFT / RIGHT / F4 / F5 press.
 *
 *  Just above the status line sits a RAINBOW_W x RAINBOW_H (300x20)
 *  rainbow strip (draw_rainbow_strip()), each column's hue proportional
 *  to its position (col * 360 / RAINBOW_W), so all RAINBOW_W columns
 *  together sweep the full 360 degrees exactly once regardless of
 *  RAINBOW_W's value - it's not tied to HUE_STEP or any other fixed
 *  degrees-per-column.  It's built once into a static RAINBOW_W x
 *  RAINBOW_H pixel buffer and sent with a single lcd_blit(), not one
 *  fill() per column - see draw_rainbow_strip()'s own comment for why
 *  that used to be visibly slow.  The colour math is hue_to_rgb565(int
 *  hue_deg) - walks a pure
 *  hue (0..359 degrees, matching the ROTATION display's units) around the
 *  standard six-segment colour wheel and packs it straight into the
 *  RGB565 lcd_solid_rectangle() etc. already expect (via the driver's own
 *  RGB() macro) - the same function draw_shape() calls (through
 *  draw_scene()) to turn each shape's hue[] entry into the colour it's
 *  actually drawn in, so a shape's fill always matches the rainbow column
 *  its arrow points at.  rgb565_to_hue(uint16_t) is the inverse - unpack
 *  5/6/5 back to 8-bit-per-channel, then the standard max/min-channel HSV
 *  hue formula; nothing needs it yet (hue[] is stored directly, never
 *  reconstructed from a packed colour), so expect a harmless "defined but
 *  not used" -Wall warning on it alone.  Both are integer-only, like the
 *  rest of this file's math.
 *
 *  Built on the drivers from "picocalc-text-starter" by Blair Leduc
 *  (drivers/lcd.c, drivers/southbridge.c): lcd_init(), lcd_solid_rectangle(),
 *  lcd_putstr() for drawing and sb_init() / sb_read_keyboard() for raw
 *  keyboard-FIFO polling.  We do NOT call picocalc_init(), which would start
 *  a background poll draining the same FIFO.
 *
 *  Controls:
 *      UP     - grow the selected shape (2px per side, 4px per dimension)
 *      DOWN   - shrink the selected shape (stops at the 1x1 degenerate case)
 *      F1     - toggle shape-select mode <-> colour-select mode
 *      RIGHT  - shape-select mode: move the blue box to the next shape;
 *               colour-select mode: step the selected shape's colour
 *               forward around the rainbow (moves its arrow right)
 *      LEFT   - shape-select mode: move the blue box to the previous
 *               shape; colour-select mode: step the selected shape's
 *               colour back around the rainbow (moves its arrow left)
 *      F5     - rotate the selected shape 5 degrees clockwise
 *      F4     - rotate the selected shape 5 degrees counter-clockwise
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
                             * KEY_F1, KEY_F4, KEY_F5, KEY_STATE_*     */

#define VERSION "V0.1"      /* matches CHANGELOG.md's heading for this round */

#define COL_BG   RGB(  0,   0,   0)
#define COL_TEXT RGB(235, 235, 235)
#define COL_BLUE RGB( 60, 140, 255)    /* selection bounding-box colour    */
#define COL_ARROW COL_TEXT              /* colour-picker arrow             */

#define GROW_PER_SIDE (2)              /* pixels added to each of the four
                                         * sides per UP press (and removed
                                         * per DOWN press)                  */

/* Sizes are odd-only: with h = size/2 (integer division), an odd size
 * spans exactly [cx-h, cx+h] - a whole pixel sits dead-centre at (cx,cy).
 * An even size spans [cx-h, cx+h-1], one pixel off-centre, so rotating it
 * about (cx,cy) visibly drifts.  GROW_PER_SIDE's per-press step (2 * that,
 * 4px) is even, so parity never changes once these three are all odd.    */
#define MIN_SIZE      (1)              /* degenerate case: a single pixel  */

/* All five shapes are maximised at boot: 59px (the largest odd size not
 * exceeding 60) is comfortably inside every spacing margin below (the top
 * row's WIDTH/3 columns are the tightest fit, and even that allows up to
 * 96), so it's a fixed cap rather than a layout-derived one.  DOWN can
 * still shrink from here; UP just has no further room to grow into.      */
#define MAX_SIZE      (59)
#define INIT_SIZE     (MAX_SIZE)

_Static_assert(MIN_SIZE % 2 == 1 && MAX_SIZE % 2 == 1 && INIT_SIZE % 2 == 1,
               "sizes must stay odd so a shape's centre pixel doesn't drift on rotation");

#define ROTATE_STEP   (5)              /* degrees per F4/F5 press          */

#define TCOLS         (40)             /* 320 / 8px-wide glyphs, built-in
                                         * 8x10 font (see put_row)          */

/* The physical PicoCalc's bezel/screen makes MAX_ROW (the very bottom
 * text row) hard to see, so it's off-limits for anything but a possible
 * future debug line - the status line lives one row above it instead.    */
#define STATUS_ROW    (MAX_ROW - 1)

/* the rainbow strip above the status line: RAINBOW_W x RAINBOW_H,
 * horizontally centred, its bottom edge RAINBOW_GAP pixels above the
 * status row's top (clear of it, rather than flush against it).  Its
 * column <-> hue mapping is proportional (col * 360 / RAINBOW_W, and the
 * inverse for the arrow - see draw_rainbow_strip() / draw_scene()), not
 * tied to any fixed degrees-per-column, so RAINBOW_W can be any width
 * and the strip still sweeps the full wheel exactly once across it.      */
#define RAINBOW_W   (300)
#define RAINBOW_H   (20)
#define RAINBOW_GAP (20)
#define RAINBOW_X   ((WIDTH - RAINBOW_W) / 2)
#define RAINBOW_Y   (STATUS_ROW * GLYPH_HEIGHT - RAINBOW_H - RAINBOW_GAP)

/* colour selection (F1 toggles it; see the header comment).  HUE_STEP is
 * purely how far LEFT/RIGHT move the selected shape's hue per press -
 * independent of RAINBOW_W, since the strip's column mapping is now
 * proportional rather than one-column-per-HUE_STEP.  DEFAULT_HUE (pure
 * green) is every shape's starting colour.                               */
#define HUE_STEP     (2)
#define DEFAULT_HUE  (120)

/* the small downward-pointing arrow drawn above the rainbow strip in
 * colour-selection mode, ARROW_GAP pixels above it, ARROW_HALF_W*2+1
 * pixels wide and ARROW_H tall (apex at the bottom, touching the strip
 * would be ARROW_GAP 0 - kept a couple of pixels clear instead)          */
#define ARROW_HALF_W (3)
#define ARROW_H      (6)
#define ARROW_GAP    (2)

#define BOX_PAD       (4)              /* gap between a shape's own size x
                                         * size box and the selection outline
                                         * drawn around it                  */
#define BOX_THICK     (2)              /* selection outline thickness      */

/* How far each row sits from vertical centre.  ROW_HALF_EXTENT is the
 * farthest any shape ever reaches from its own centre - MAX_SIZE/2 for
 * the shape itself, plus BOX_PAD for its selection box outline (the
 * outline's own BOX_THICK is drawn inward from that edge, not beyond it).
 * Placing each row's centre ROW_HALF_EXTENT + ROW_GAP/2 pixels out from
 * HEIGHT/2 leaves exactly ROW_GAP of clear space between the two rows
 * even when every shape (and its selection box) is at MAX_SIZE at once -
 * they can never overlap - while keeping the rows as close to centre as
 * that allows, freeing the most room above and below for anything else.  */
#define ROW_GAP          (10)
#define ROW_HALF_EXTENT  (MAX_SIZE / 2 + BOX_PAD)
#define ROW_OFFSET       (ROW_HALF_EXTENT + ROW_GAP / 2)

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
static void hue_to_rgb888(int hue_deg, uint8_t *r, uint8_t *g, uint8_t *b)
{
    hue_deg = ((hue_deg % 360) + 360) % 360;   /* normalise to 0..359 */
    int region  = hue_deg / 60;                /* which 60-degree segment  */
    int rem     = hue_deg % 60;                /* position within it, 0..59*/
    int rising  = (rem * 255) / 59;
    int falling = 255 - rising;

    switch (region) {
    case 0: *r = 255;     *g = rising;  *b = 0;       break; /* red    -> yellow  */
    case 1: *r = falling; *g = 255;     *b = 0;       break; /* yellow -> green   */
    case 2: *r = 0;       *g = 255;     *b = rising;  break; /* green  -> cyan    */
    case 3: *r = 0;       *g = falling; *b = 255;     break; /* cyan   -> blue    */
    case 4: *r = rising;  *g = 0;       *b = 255;     break; /* blue   -> magenta */
    default:*r = 255;     *g = 0;       *b = falling; break; /* magenta-> red     */
    }
}

/* hue (0..359 degrees, wraps) -> packed RGB565, via the display driver's
 * own RGB() macro (lcd.h) so the 8-bit-per-channel -> 5/6/5 truncation is
 * exactly what lcd_solid_rectangle() etc. already expect.                */
static uint16_t hue_to_rgb565(int hue_deg)
{
    uint8_t r, g, b;
    hue_to_rgb888(hue_deg, &r, &g, &b);
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

/* the RAINBOW_W x RAINBOW_H strip just above the status line, each
 * column's hue proportional to its position (col * 360 / RAINBOW_W) so
 * the RAINBOW_W columns together sweep the full 360-degree wheel exactly
 * once, at whatever width RAINBOW_W is.  Doubles as the colour picker in
 * colour-selection mode: draw_colour_arrow() points at whichever column
 * the selected shape's hue[] currently matches (the same formula,
 * inverted - see draw_scene()).
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
static void draw_rainbow_strip(int x0, int y0)
{
    static uint16_t buf[RAINBOW_W * RAINBOW_H];
    static bool built = false;

    if (!built) {
        for (int col = 0; col < RAINBOW_W; col++) {
            uint16_t colour = hue_to_rgb565(col * 360 / RAINBOW_W);
            for (int row = 0; row < RAINBOW_H; row++)
                buf[row * RAINBOW_W + col] = colour;
        }
        built = true;
    }

    lcd_blit(buf, x0, y0, RAINBOW_W, RAINBOW_H);
}

/* ===================================================================== */
/*  Rotation math (see header comment)                                    */
/* ===================================================================== */

typedef struct { int x, y; } point_t;

/* sin(0..90 degrees) x1000, one entry per integer degree; sin1000() /
 * cos1000() below derive every other degree from this by symmetry, so
 * there's no <math.h> dependency for the rotation math.                  */
static const int16_t SIN_TABLE_Q1000[91] = {
      0,  17,  35,  52,  70,  87, 105, 122, 139, 156,
    174, 191, 208, 225, 242, 259, 276, 292, 309, 326,
    342, 358, 375, 391, 407, 423, 438, 454, 469, 485,
    500, 515, 530, 545, 559, 574, 588, 602, 616, 629,
    643, 656, 669, 682, 695, 707, 719, 731, 743, 755,
    766, 777, 788, 799, 809, 819, 829, 839, 848, 857,
    866, 875, 883, 891, 899, 906, 914, 921, 927, 934,
    940, 946, 951, 956, 961, 966, 970, 974, 978, 981,
    985, 987, 990, 992, 994, 996, 997, 998, 999, 999,
   1000,
};

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

#define THIN_WIDTH (5)  /* the thin rectangle's fixed short side - odd, for
                         * the same centre-pixel-on-rotation reason as the
                         * size[] values (see the MIN_SIZE comment)        */
_Static_assert(THIN_WIDTH % 2 == 1,
               "THIN_WIDTH must be odd so its centre pixel doesn't drift on rotation");

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

/* the {5/2} star's inner-point radius as a fraction (x1000) of its outer
 * (point) radius - the ratio that makes a regular five-pointed star       */
#define STAR_INNER_RATIO_Q1000 (382)

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

/* left-to-right, top row then bottom row */
enum { SHAPE_THIN_RECT, SHAPE_TRIANGLE, SHAPE_SQUARE,
       SHAPE_PENTAGON, SHAPE_STAR, SHAPE_CIRCLE,
       NUM_SHAPES };

/* SHAPE_THIN_RECT displays as "LINE" - not technically accurate (it's
 * still a thin filled rectangle, draw_thin_rect() and THIN_WIDTH are
 * unchanged), but that's the name it goes by on screen and in
 * conversation from here on.                                             */
static const char *const SHAPE_NAME[NUM_SHAPES] = {
    "LINE", "TRIANGLE", "SQUARE", "PENTAGON", "STAR", "CIRCLE",
};

/* every shape's centre - the one place the row/column layout lives, so
 * draw_scene() and the selection box below always agree on where a shape
 * is.  Three shapes per row, sharing the same WIDTH/6-based columns as
 * the top row always has, so top and bottom line up.                     */
static void shape_center(int shape, int *cx, int *cy)
{
    int top_cy = HEIGHT / 2 - ROW_OFFSET;
    int bot_cy = HEIGHT / 2 + ROW_OFFSET;

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

/* small downward-pointing arrow (apex at the bottom) shown above the
 * rainbow strip in colour-selection mode, marking the selected shape's
 * current hue - hue_x is the screen x of that hue's rainbow-strip column
 * (caller computes it: RAINBOW_X + the hue's column, the inverse of
 * draw_rainbow_strip()'s col -> hue mapping), so the arrow always points
 * straight down at the exact column the shape's colour comes from.       */
static void draw_colour_arrow(int hue_x)
{
    int apex_y = RAINBOW_Y - ARROW_GAP;
    point_t tri[3] = {
        { hue_x,               apex_y },
        { hue_x + ARROW_HALF_W, apex_y - ARROW_H },
        { hue_x - ARROW_HALF_W, apex_y - ARROW_H },
    };
    fill_polygon(tri, 3, COL_ARROW);
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
 * "SQUARE  SIZE: 59px  ROTATION: 35 DEG"                                  */
static void show_status(int selected, int size, int angle_deg)
{
    char buf[40];
    snprintf(buf, sizeof buf, "%s  SIZE: %dpx  ROTATION: %d DEG",
             SHAPE_NAME[selected], size, angle_deg);
    put_row(STATUS_ROW, buf, COL_TEXT);
}

/* clear the screen, draw all six shapes at their own size, rotation and
 * colour (size[] / angle[] / hue[] each have one entry per SHAPE_*
 * index), the selection box around `selected`, the rainbow strip, the
 * colour-selection arrow (only in colour_mode) and the status line       */
static void draw_scene(const int *size, int selected, const int *angle,
                        const int *hue, bool colour_mode)
{
    fill(COL_BG, 0, 0, WIDTH, HEIGHT);

    for (int s = 0; s < NUM_SHAPES; s++) {
        int cx = 0, cy = 0;
        shape_center(s, &cx, &cy);
        draw_shape(s, cx, cy, size[s], angle[s], hue_to_rgb565(hue[s]));
    }

    int scx = 0, scy = 0;
    shape_center(selected, &scx, &scy);
    draw_box_outline(scx, scy, size[selected], COL_BLUE);

    draw_rainbow_strip(RAINBOW_X, RAINBOW_Y);
    if (colour_mode) {
        /* inverse of draw_rainbow_strip()'s col * 360 / RAINBOW_W, rounded
         * to the nearest column instead of truncated                      */
        int col = (hue[selected] * RAINBOW_W + 180) / 360;
        draw_colour_arrow(RAINBOW_X + col);
    }
    show_status(selected, size[selected], angle[selected]);
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
    for (int s = 0; s < NUM_SHAPES; s++) {
        size[s] = INIT_SIZE;
        hue[s]  = DEFAULT_HUE;
    }
    int selected  = SHAPE_THIN_RECT;  /* the first shape, per the blue box */
    int angle[NUM_SHAPES] = { 0 };    /* each shape keeps its own rotation,
                                       * remembered across selection changes */
    bool colour_mode = false;         /* F1 toggles shape- vs colour-select */
    draw_scene(size, selected, angle, hue, colour_mode);

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

        if (code == KEY_F1) {                     /* toggle shape / colour mode */
            colour_mode = !colour_mode;
            draw_scene(size, selected, angle, hue, colour_mode);
        } else if (code == KEY_RIGHT) {
            if (colour_mode) {                     /* step the arrow / hue forward */
                hue[selected] = (hue[selected] + HUE_STEP) % 360;
            } else {                               /* move the selection box forward */
                selected = (selected + 1) % NUM_SHAPES;
            }
            draw_scene(size, selected, angle, hue, colour_mode);
        } else if (code == KEY_LEFT) {
            if (colour_mode) {                     /* step the arrow / hue back */
                hue[selected] = (hue[selected] - HUE_STEP + 360) % 360;
            } else {                               /* move the selection box back */
                selected = (selected - 1 + NUM_SHAPES) % NUM_SHAPES;
            }
            draw_scene(size, selected, angle, hue, colour_mode);
        } else if (code == KEY_UP) {              /* grow only the selected shape */
            if (size[selected] < MAX_SIZE) {
                size[selected] += 2 * GROW_PER_SIDE;
                if (size[selected] > MAX_SIZE) size[selected] = MAX_SIZE;
                draw_scene(size, selected, angle, hue, colour_mode);
            }
        } else if (code == KEY_DOWN) {            /* shrink only the selected shape */
            if (size[selected] > MIN_SIZE) {
                size[selected] -= 2 * GROW_PER_SIDE;
                if (size[selected] < MIN_SIZE) size[selected] = MIN_SIZE;
                draw_scene(size, selected, angle, hue, colour_mode);
            }
        } else if (code == KEY_F5) {              /* rotate selected CW    */
            angle[selected] = (angle[selected] + ROTATE_STEP) % 360;
            draw_scene(size, selected, angle, hue, colour_mode);
        } else if (code == KEY_F4) {              /* rotate selected CCW   */
            angle[selected] = (angle[selected] - ROTATE_STEP + 360) % 360;
            draw_scene(size, selected, angle, hue, colour_mode);
        }
    }
}
