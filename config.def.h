#define TITLE_HEIGHT       18
#define BUTTON_WIDTH       18
#define BORDER_EXT          4

#define BAR_HEIGHT TITLE_HEIGHT + BORDER_EXT
#define BORDER_INNER (BORDER_EXT - 1)

#define EDGE_PADDING       10
#define DEFAULT_X EDGE_PADDING
#define DEFAULT_Y BAR_HEIGHT + EDGE_PADDING

#define TITLE_COLOR  0x282828
#define BAR_TEXT     0xFBF1C7
#define BORDER_COLOR 0x282828
#define BORDER_DARK  0x3C3836
#define WINDOW_BACK  0x282828

#define MOD_MASK     Mod1Mask

static char* font_name = "Liberation Mono:pixelsize=12:antialias=true:autohint=true";

static silly_exec launch_apps[] = {
	{ "st", (char* []){ "st", NULL } }
};
