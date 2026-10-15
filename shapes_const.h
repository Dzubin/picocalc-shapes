/*
 * shapes_const.h - every constant and data table for picocalc-shapes.c:
 * colours, sizes, the on-screen layout (rows, columns and pixel positions),
 * the picker settings, the key assignments, the sine table and the shape
 * names.
 *
 * It uses macros from the display and keyboard drivers (RGB(), WIDTH,
 * GLYPH_HEIGHT, MAX_ROW, KEY_F2 and so on), so it is included after
 * lcd.h and keyboard.h.
 *
 * Author: Thomas Dzubin
 */
#ifndef SHAPES_CONST_H
#define SHAPES_CONST_H

#define VERSION "V0.2"      /* matches CHANGELOG.md's heading for this round */

#define COL_BG   RGB(  0,   0,   0)
#define COL_TEXT RGB(255, 255, 255)
#define COL_SHAPE_BOX COL_TEXT          /* selection bounding-box colour    */
#define COL_ARROW COL_TEXT              /* colour-picker arrow             */
#define COL_BAR_BOX COL_TEXT            /* outline of the bar being edited */

#define GROW_PER_SIDE (2)              /* pixels added to each of the four
                                         * sides per + press (and removed
                                         * per - press)                  */

/* Sizes are odd-only: with h = size/2 (integer division), an odd size
 * spans exactly [cx-h, cx+h] - a whole pixel sits dead-centre at (cx,cy).
 * An even size spans [cx-h, cx+h-1], one pixel off-centre, so rotating it
 * about (cx,cy) visibly drifts.  GROW_PER_SIDE's per-press step (2 * that,
 * 4px) is even, so parity never changes once these three are all odd.    */
#define MIN_SIZE      (1)              /* degenerate case: a single pixel  */

/* All five shapes are maximised at boot: 49px (smaller than it once was,
 * to leave room for the colour bars under the shapes) is comfortably inside
 * every spacing margin below (the top row's WIDTH/3 columns are the
 * tightest fit, and even that allows up to 96), so it's a fixed cap rather
 * than a layout-derived one.  - can still shrink from here; + just has no
 * further room to grow into.                                              */
#define MAX_SIZE      (49)
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

/* text row under the three pickers that shows the selected shape's
 * colour as its RGB565 fields in hex, two rows above the status line     */
#define COLOUR_ROW    (STATUS_ROW - 2)

/* The three pickers are stacked, hue on top, then saturation, then value
 * at the bottom, with the RGB text row under them.  Each is RAINBOW_W
 * wide and horizontally centred.  The hue picker is the two rainbow strips
 * (RAINBOW_W x RAINBOW_H each).  A strip's column <-> hue mapping is
 * proportional (col * 360 / RAINBOW_W, and the inverse for the arrow, see
 * draw_rainbow_strip() and draw_scene()), not tied to any fixed
 * degrees-per-column, so RAINBOW_W can be any width and the strip still
 * sweeps the full wheel exactly once across it.                          */
#define RAINBOW_W   (300)
#define RAINBOW_H   (20)
#define RAINBOW_X   ((WIDTH - RAINBOW_W) / 2)

/* vertical layout, worked from the bottom up.  VAL_Y is the value bar, a
 * few pixels above the RGB text row (room for its outline).  SAT_GAP is
 * the clear space between bars, enough for the outline and the arrow that
 * points down at the lower bar.  The gap between the hue strips and the
 * saturation bar holds two arrows (the up arrow under the lower strip and
 * the down arrow over the saturation bar), so it is larger.  RAINBOW_Y is
 * the lower (improved) strip.                                            */
#define BAR_LOW_GAP  (4)
#define HUE_SAT_GAP  (22)
#define RAINBOW_Y    (SAT_Y - HUE_SAT_GAP - RAINBOW_H)

/* Two stacked strips, flush against each other.  Wheel 0 is the plain hue
 * wheel (linear channel ramps) and sits on top; wheel 1 is the improved
 * wheel (each ramp passed through sqrt(v * (510 - v)), see
 * hue_to_rgb888()) and sits just above the saturation bar.  Both
 * strips map a column to the same hue, so one hue[] value serves either.  */
#define NUM_WHEELS  (2)
#define WHEEL_PLAIN (0)
#define WHEEL_ROUND (1)
#define WHEEL_Y(w)  (RAINBOW_Y - ((NUM_WHEELS - 1) - (w)) * RAINBOW_H)

/* The modes, numbered in the order UP and DOWN walk them (the top row of
 * shapes, the bottom row, then the pickers as they are stacked on screen),
 * so DOWN is mode + 1 and UP is mode - 1.  F1 steps through the first
 * three: shape selection, then the plain hue strip, then the improved one.
 * F2 (an otherwise unused key) toggles saturation and F3 value, which are
 * outside the F1 cycle.  SHAPES_PER_ROW is how far a shape is from the one
 * directly above or below it.                                            */
#define MODE_SHAPE      (0)
#define MODE_COLOUR_0   (1)
#define MODE_COLOUR_1   (2)
#define NUM_MODES       (3)
#define MODE_SATURATION (3)
#define MODE_VALUE      (4)
#define SHAPES_PER_ROW  (3)
#define SAT_KEY         KEY_F2
#define VAL_KEY         KEY_F3

/* mode_wheel() in the program turns a hue mode into a wheel index with
 * mode - MODE_COLOUR_0, so the hue modes must sit in wheel order           */
_Static_assert(MODE_COLOUR_1 - MODE_COLOUR_0 + 1 == NUM_WHEELS,
               "one hue mode per wheel, in wheel order");
_Static_assert(MODE_COLOUR_0 == MODE_SHAPE + 1 && MODE_COLOUR_1 == MODE_COLOUR_0 + 1
               && MODE_SATURATION == MODE_COLOUR_1 + 1 && MODE_VALUE == MODE_SATURATION + 1,
               "UP and DOWN step through the modes in numeric order");

/* saturation selection: a bar under the rainbow strips, left end white
 * (0 percent) to right end the full colour (100 percent) at the selected
 * shape's hue.  Each shape keeps its own saturation.                     */
#define SAT_MAX      (100)
#define SAT_STEP     (2)
#define DEFAULT_SAT  (100)
#define SAT_H        (20)
#define SAT_GAP      (14)
#define SAT_Y        (VAL_Y - SAT_GAP - SAT_H)

/* value (brightness) selection: a bar under the saturation bar, black at
 * the left end to the full colour at the right.  Each shape keeps its own
 * value.                                                                 */

/* growing and shrinking a shape (UP and DOWN move the selection instead) */
#define KEY_GROW         '+'
#define KEY_SHRINK       '-'

#define VAL_MAX      (100)
#define VAL_STEP     (2)
#define DEFAULT_VAL  (100)
#define VAL_H        (20)
#define VAL_Y        (COLOUR_ROW * GLYPH_HEIGHT - BAR_LOW_GAP - VAL_H)

/* draw_bar() builds both bars in buffers sized by SAT_H                   */
_Static_assert(SAT_H == VAL_H, "the saturation and value bars share a height");

/* colour selection (F1 toggles it; see the header comment).  HUE_STEP is
 * purely how far LEFT/RIGHT move the selected shape's hue per press -
 * independent of RAINBOW_W, since the strip's column mapping is now
 * proportional rather than one-column-per-HUE_STEP.  DEFAULT_HUE (pure
 * green) is every shape's starting colour.                               */
#define HUE_STEP     (2)
#define DEFAULT_HUE  (120)

/* the small arrow drawn beside the rainbow strip in colour-selection mode
 * (ARROW_GAP leaves room for the white outline that marks the active bar,
 * BOX_THICK pixels wide), ARROW_GAP pixels from it, ARROW_HALF_W*2+1
 * pixels wide and ARROW_H tall (its apex is ARROW_GAP pixels from the bar) */
#define ARROW_HALF_W (3)
#define ARROW_H      (6)
#define ARROW_GAP    (4)

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

/* The top row sits as high as it can without a rotated shape leaving the
 * screen.  The farthest any shape reaches from its centre is a square's
 * half diagonal at MAX_SIZE (MAX_SIZE * 0.7071, rounded up), and
 * TOP_MARGIN pixels stay clear above that.  The bottom row keeps the same
 * distance below the top row as before (2 * ROW_OFFSET).                 */
#define ROT_REACH        ((MAX_SIZE * 7072 + 9999) / 10000)
#define TOP_MARGIN       (6)
#define TOP_ROW_CY       (ROT_REACH + TOP_MARGIN)
#define BOT_ROW_CY       (TOP_ROW_CY + 2 * ROW_OFFSET)

/* ===================================================================== */
/*  Rotation math tables (see the header comment of picocalc-shapes.c)    */
/* ===================================================================== */

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

#define THIN_WIDTH (5)  /* the thin rectangle's fixed short side - odd, for
                         * the same centre-pixel-on-rotation reason as the
                         * size[] values (see the MIN_SIZE comment)        */
_Static_assert(THIN_WIDTH % 2 == 1,
               "THIN_WIDTH must be odd so its centre pixel doesn't drift on rotation");

/* the {5/2} star's inner-point radius as a fraction (x1000) of its outer
 * (point) radius - the ratio that makes a regular five-pointed star       */
#define STAR_INNER_RATIO_Q1000 (382)

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

/* ===================================================================== */
/*  Help screen (shown at the start and when ? is pressed)                */
/* ===================================================================== */

#define KEY_HELP          '?'

#define COL_HELP_TEXT     RGB(255, 255, 255)   /* full white               */
#define COL_HELP_TITLE    RGB(  0, 255, 255)   /* cyan                     */
#define COL_HELP_FOOTER   RGB(  0, 255,   0)   /* green                    */

/* The help screen is drawn in the driver's bold text (each stroke two pixels
 * wide) and in pure white, since the normal one-pixel strokes look dim on
 * the real screen.
 *
 * text rows: the title, the author and the version are centred; the key
 * list starts at HELP_FIRST_ROW, one row per line of HELP_LINES, with the
 * line's own spacing putting the key names in a column.  Every line is at
 * most TCOLS - HELP_LEFT_COL characters long.                            */
#define HELP_TITLE_ROW    (1)
#define HELP_AUTHOR_ROW   (3)
#define HELP_VERSION_ROW  (4)
#define HELP_FIRST_ROW    (7)
#define HELP_LEFT_COL     (1)
#define HELP_FOOTER_ROW   (28)

#define HELP_TITLE        "PICOCALC SHAPES"
#define HELP_AUTHOR       "Author: Thomas Dzubin"
#define HELP_FOOTER       "PRESS ANY KEY TO CONTINUE"

static const char *const HELP_LINES[] = {
    "LEFT/RIGHT  previous / next shape,",
    "            or change the picker",
    "UP/DOWN     move between the shapes",
    "            and the three pickers",
    "+ / -       grow / shrink the shape",
    "F1          hue (plain, improved)",
    "F2          saturation",
    "F3          value (brightness)",
    "F4 / F5     rotate left / right",
    "?           this help screen",
    "ESC or Q    back to the UF2 Loader",
    "~           reboot to BOOTSEL mode",
};
#define HELP_LINE_COUNT   ((int)(sizeof HELP_LINES / sizeof HELP_LINES[0]))

#endif /* SHAPES_CONST_H */
