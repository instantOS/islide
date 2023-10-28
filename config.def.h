/* See LICENSE file for copyright and license details. */
/* Default settings; can be overriden by command line. */

static int topbar = 1;                      /* -b  option; if 0, islide appears at bottom     */
/* -fn option overrides fonts[0]; default X11 font or font set */
static const char *fonts[] = {
	"Inter-Regular:size=12"
};
static const char *prompt      = NULL;      /* -p  option; prompt to the left of input field */
static const char *command      = NULL;      
static const char *suffix      = NULL;
static unsigned int maxvalue      = 100;
static unsigned int keyboardvalue      = 0;

static const char *colors[SchemeLast][2] = {
	/*     fg         bg       */
	[SchemeNorm] = { "#bbbbbb", "#121212" },
	[SchemeSel] = { "#eeeeee", "#88B2F6" },
    [SchemeDarkSel] = { "#121212", "#88B2F6" },
	[SchemeOut] = { "#000000", "#00ffff" },
};
/* -l option; if nonzero, islide uses vertical list with given number of lines */
static unsigned int lines      = 0;
static unsigned int startvalue      = 0;


/*
 * Characters not considered part of a word while deleting words
 * for example: " /?\"&[]"
 */
static const char worddelimiters[] = " ";
