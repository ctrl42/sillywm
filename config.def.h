#define TITLE_HEIGHT       18
#define BUTTON_WIDTH       18
#define BORDER_EXT          4

#define EDGE_PADDING       10
#define DEFAULT_X EDGE_PADDING
#define DEFAULT_Y BAR_HEIGHT + EDGE_PADDING

#define TITLE_COLOR  0x282828
#define BAR_TEXT     0xEBDBB2
#define BORDER_COLOR 0x282828
#define BORDER_DARK  0x3C3836
#define WINDOW_BACK  0x282828

#define MOD_MASK     Mod1Mask
#define CURSOR       XC_X_cursor

static char* font_name = "Liberation Mono:pixelsize=12:antialias=true:autohint=true";

static silly_exec launch_apps[] = {
	{ "st", (char* []){ "st", NULL } }
};

static silly_bind binds[] = {
	{XK_Return, { "st", (char* []){ "st", NULL } } },
	{XK_q, {NULL, NULL} }
};

static char* close_button_xpm[] = {
	"18 18 2 1",
	"X c #EBDBB2",
	". c #3C3836",
	"..................",
	"..................",
	"..................",
	"..................",
	"....XX......XX....",
	"....XXX....XXX....",
	".....XXX..XXX.....",
	"......XXXXXX......",
	".......XXXX.......",
	".......XXXX.......",
	"......XXXXXX......",
	".....XXX..XXX.....",
	"....XXX....XXX....",
	"....XX......XX....",
	"..................",
	"..................",
	"..................",
	".................."
};

static char* minimize_button_xpm[] = {
	"18 18 2 1",
	"X c #EBDBB2",
	". c #3C3836",
	"..................",
	"..................",
	"..................",
	"..................",
	"..................",
	"..................",
	"..................",
	".....XXXXXXXX.....",
	"......XXXXXX......",
	".......XXXX.......",
	"........XX........",
	"..................",
	"..................",
	"..................",
	"..................",
	"..................",
	"..................",
	".................."
};
