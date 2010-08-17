/* $xxxterm: xxxterm.c,v 1.106 2010/08/15 22:26:29 marco Exp $ */
/*
 * Copyright (c) 2010 Marco Peereboom <marco@peereboom.us>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * TODO:
 *	inverse color browsing
 *	favs
 *		- add favicon
 *	download files status
 *	multi letter commands
 *	pre and post counts for commands
 *	fav icon
 *	close tab X
 *	autocompletion on various inputs
 *	create privacy browsing
 *		- encrypted local data
 */

#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <pwd.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#include <sys/queue.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <webkit/webkit.h>
#include <libsoup/soup.h>
#include <JavaScriptCore/JavaScript.h>

#include "javascript.h"
/*

javascript.h borrowed from vimprobable2 under the following license:

Copyright (c) 2009 Leon Winter
Copyright (c) 2009 Hannes Schueller
Copyright (c) 2009 Matto Fransen

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

static char		*version = "$xxxterm: xxxterm.c,v 1.106 2010/08/15 22:26:29 marco Exp $";

#define XT_DEBUG
/* #define XT_DEBUG */
#ifdef XT_DEBUG
#define DPRINTF(x...)		do { if (swm_debug) fprintf(stderr, x); } while (0)
#define DNPRINTF(n,x...)	do { if (swm_debug & n) fprintf(stderr, x); } while (0)
#define	XT_D_MOVE		0x0001
#define	XT_D_KEY		0x0002
#define	XT_D_TAB		0x0004
#define	XT_D_URL		0x0008
#define	XT_D_CMD		0x0010
#define	XT_D_NAV		0x0020
#define	XT_D_DOWNLOAD		0x0040
#define	XT_D_CONFIG		0x0080
#define	XT_D_JS			0x0100
u_int32_t		swm_debug = 0
			    | XT_D_MOVE
			    | XT_D_KEY
			    | XT_D_TAB
			    | XT_D_URL
			    | XT_D_CMD
			    | XT_D_NAV
			    | XT_D_DOWNLOAD
			    | XT_D_CONFIG
			    | XT_D_JS
			    ;
#else
#define DPRINTF(x...)
#define DNPRINTF(n,x...)
#endif

#define LENGTH(x)		(sizeof x / sizeof x[0])
#define CLEAN(mask)		(mask & ~(GDK_MOD2_MASK) &	\
				    ~(GDK_BUTTON1_MASK) &	\
				    ~(GDK_BUTTON2_MASK) &	\
				    ~(GDK_BUTTON3_MASK) &	\
				    ~(GDK_BUTTON4_MASK) &	\
				    ~(GDK_BUTTON5_MASK))

struct tab {
	TAILQ_ENTRY(tab)	entry;
	GtkWidget		*vbox;
	GtkWidget		*label;
	GtkWidget		*spinner;
	GtkWidget		*uri_entry;
	GtkWidget		*search_entry;
	GtkWidget		*toolbar;
	GtkWidget		*browser_win;
	GtkWidget		*cmd;
	GtkToolItem		*backward;
	GtkToolItem		*forward;
	GtkToolItem		*stop;
	guint			tab_id;
	WebKitWebView		*wv;

	/* adjustments for browser */
	GtkScrollbar		*sb_h;
	GtkScrollbar		*sb_v;
	GtkAdjustment		*adjust_h;
	GtkAdjustment		*adjust_v;

	/* flags */
	int			focus_wv;
	int			ctrl_click;
	gchar			*hover;

	/* hints */
	int			hints_on;
	int			hint_mode;
#define XT_HINT_NONE		(0)
#define XT_HINT_NUMERICAL	(1)
#define XT_HINT_ALPHANUM	(2)
	char			hint_buf[128];
	char			hint_num[128];

	/* search */
	char			*search_text;
	int			search_forward;

	/* settings */
	WebKitWebSettings	*settings;
	int			font_size;
	gchar			*user_agent;
};
TAILQ_HEAD(tab_list, tab);

struct karg {
	int		i;
	char		*s;
};

/* defines */
#define XT_NAME			("XXXTerm")
#define XT_DIR			(".xxxterm")
#define XT_CONF_FILE		("xxxterm.conf")
#define XT_FAVS_FILE		("favorites")
#define XT_CB_HANDLED		(TRUE)
#define XT_CB_PASSTHROUGH	(FALSE)

/* actions */
#define XT_MOVE_INVALID		(0)
#define XT_MOVE_DOWN		(1)
#define XT_MOVE_UP		(2)
#define XT_MOVE_BOTTOM		(3)
#define XT_MOVE_TOP		(4)
#define XT_MOVE_PAGEDOWN	(5)
#define XT_MOVE_PAGEUP		(6)
#define XT_MOVE_HALFDOWN	(7)
#define XT_MOVE_HALFUP		(8)
#define XT_MOVE_LEFT		(9)
#define XT_MOVE_FARLEFT		(10)
#define XT_MOVE_RIGHT		(11)
#define XT_MOVE_FARRIGHT	(12)

#define XT_TAB_LAST		(-4)
#define XT_TAB_FIRST		(-3)
#define XT_TAB_PREV		(-2)
#define XT_TAB_NEXT		(-1)
#define XT_TAB_INVALID		(0)
#define XT_TAB_NEW		(1)
#define XT_TAB_DELETE		(2)
#define XT_TAB_DELQUIT		(3)
#define XT_TAB_OPEN		(4)

#define XT_NAV_INVALID		(0)
#define XT_NAV_BACK		(1)
#define XT_NAV_FORWARD		(2)
#define XT_NAV_RELOAD		(3)

#define XT_FOCUS_INVALID	(0)
#define XT_FOCUS_URI		(1)
#define XT_FOCUS_SEARCH		(2)

#define XT_SEARCH_INVALID	(0)
#define XT_SEARCH_NEXT		(1)
#define XT_SEARCH_PREV		(2)

#define XT_FONT_SET		(0)

/* globals */
extern char		*__progname;
struct passwd		*pwd;
GtkWidget		*main_window;
GtkNotebook		*notebook;
struct tab_list		tabs;

/* mime types */
struct mime_type {
	char			*mt_type;
	char			*mt_action;
	int			mt_default;
	TAILQ_ENTRY(mime_type)	entry;
};
TAILQ_HEAD(mime_type_list, mime_type);

/* uri aliases */
struct alias {
	char			*a_name;;
	char			*a_uri;
	TAILQ_ENTRY(alias)	 entry;
};
TAILQ_HEAD(alias_list, alias);

/* settings */
int			showtabs = 1;	/* show tabs on notebook */
int			showurl = 1;	/* show url toolbar on notebook */
int			tabless = 0;	/* allow only 1 tab */
int			ctrl_click_focus = 0; /* ctrl click gets focus */
int			cookies_enabled = 1; /* enable cookies */
int			read_only_cookies = 0; /* enable to not write cookies */
int			enable_scripts = 0;
int			enable_plugins = 0;
int			default_font_size = 12;
char			*default_font_family = NULL;
char			*serif_font_family = NULL;
char			*sans_serif_font_family = NULL;
char			*monospace_font_family = NULL;
char			*default_encoding = NULL;
char			*user_stylesheet = NULL;
int			fancy_bar = 1;	/* fancy toolbar */

char			*home = "http://www.peereboom.us";
char			*search_string = NULL;
char			*http_proxy = NULL;
SoupURI			*proxy_uri = NULL;
char			work_dir[PATH_MAX];
char			cookie_file[PATH_MAX];
char			download_dir[PATH_MAX];
SoupSession		*session;
SoupCookieJar		*cookiejar;

struct mime_type_list	mtl;
struct alias_list	aliases;

/* protos */
void			create_new_tab(char *, int);
void			delete_tab(struct tab *);
void			adjustfont_webkit(struct tab *, int);
int			run_script(struct tab *, char *);

struct valid_url_types {
	char		*type;
} vut[] = {
	{ "http://" },
	{ "https://" },
	{ "ftp://" },
	{ "file://" },
};

int
valid_url_type(char *url)
{
	int			i;

	for (i = 0; i < LENGTH(vut); i++)
		if (!strncasecmp(vut[i].type, url, strlen(vut[i].type)))
			return (0);

	return (1);
}

/* http://www.iana.org/domains/root/db */
const char *tlds[] = { "localhost", ".ad",".ae",".aero",".af",".ag",".ai",".al",".am",".an",".ao",".aq",".ar",".arpa",".as",".asia",".at",".au",".aw",".ax",".az",".ba",".bb",".bd",".be",".bf",".bg",".bh",".bi",".biz",".bj",".bl",".bm",".bn",".bo",".br",".bs",".bt",".bv",".bw",".by",".bz",".ca",".cat",".cc",".cd",".cf",".cg",".ch",".ci",".ck",".cl",".cm",".cn",".co",".com",".coop",".cr",".cu",".cv",".cx",".cy",".cz",".de",".dj",".dk",".dm",".do",".dz",".ec",".edu",".ee",".eg",".eh",".er",".es",".et",".eu",".fi",".fj",".fk",".fm",".fo",".fr",".ga",".gb",".gd",".ge",".gf",".gg",".gh",".gi",".gl",".gm",".gn",".gov",".gp",".gq",".gr",".gs",".gt",".gu",".gw",".gy",".hk",".hm",".hn",".hr",".ht",".hu",".id",".ie",".il",".im",".in",".info",".int",".io",".iq",".ir",".is",".it",".je",".jm",".jo",".jobs",".jp",".ke",".kg",".kh",".ki",".km",".kn",".kp",".kr",".kw",".ky",".kz",".la",".lb",".lc",".li",".lk",".lr",".ls",".lt",".lu",".lv",".ly",".ma",".mc",".md",".me",".mf",".mg",".mh",".mil",".mk",".ml",".mm",".mn",".mo",".mobi",".mp",".mq",".mr",".ms",".mt",".mu",".museum",".mv",".mw",".mx",".my",".mz",".na",".name",".nc",".ne",".net",".nf",".ng",".ni",".nl",".no",".np",".nr",".nu",".nz",".om",".org",".pa",".pe",".pf",".pg",".ph",".pk",".pl",".pm",".pn",".pr",".pro",".ps",".pt",".pw",".py",".qa",".re",".ro",".rs",".ru",".rw",".sa",".sb",".sc",".sd",".se",".sg",".sh",".si",".sj",".sk",".sl",".sm",".sn",".so",".sr",".st",".su",".sv",".sy",".sz",".tc",".td",".tel",".tf",".tg",".th",".tj",".tk",".tl",".tm",".tn",".to",".tp",".tr",".travel",".tt",".tv",".tw",".tz",".ua",".ug",".uk",".um",".us",".uy",".uz",".va",".vc",".ve",".vg",".vi",".vn",".vu",".wf",".ws",".xn--0zwm56d",".xn--11b5bs3a9aj6g",".xn--80akhbyknj4f",".xn--9t4b11yi5a",".xn--deba0ad",".xn--g6w251d",".xn--hgbk6aj7f53bba",".xn--hlcj6aya9esc7a",".xn--jxalpdlp",".xn--kgbechtv",".xn--mgbaam7a8h",".xn--mgberp4a5d4ar",".xn--p1ai",".xn--wgbh1c",".xn--zckzah",".ye",".yt",".za",".zm",".zw", NULL };

int
guess_domainname(char *input)
{
	char *str;
	char *p;
	int i, ret = 0;

	if (!input)
		return 0;

	if (!valid_url_type(input))
		return 1;

	if ((str = strdup(input)) == NULL)
		err(1, "strdup");
	if ((p = strchr(str, '/')) != NULL)
		*p = '\0';
	/* port? */
	if ((p = strchr(str, ':')) != NULL)
		*p = '\0';

	for (i = 0; tlds[i]; i++) {
		if ((p = strstr(str, tlds[i])) != NULL) {
			if (strlen(p) == strlen(tlds[i])) {
				ret = 1;
				break;
			}
		}
	}
	free(str);
	return ret;
}

char *
match_alias(char *url_in)
{
	struct alias		*a;
	char			*arg;
	char			*url_out = NULL;

	arg = url_in;
	if (strsep(&arg, ":") == NULL)
		errx(1, "match_alias: NULL URL");

	TAILQ_FOREACH(a, &aliases, entry) {
		if (!strcmp(url_in, a->a_name))
			break;
	}

	if (a != NULL) {
		DNPRINTF(XT_D_URL, "match_alias: matched alias %s\n",
			a->a_name);
		if (arg != NULL)
			url_out = g_strdup_printf(a->a_uri, arg);
		else
			url_out = g_strdup(a->a_uri);
	}

	return (url_out);
}

char *
guess_url_type(char *url_in)
{
	struct stat		sb;
	char			*url_out = NULL;

	url_out = match_alias(url_in);
	if (url_out != NULL)
		return (url_out);

	/* XXX not sure about this heuristic */
	if (stat(url_in, &sb) == 0)
		url_out = g_strdup_printf("file://%s", url_in);
	else {
		/* guess search */
		if (!guess_domainname(url_in))
			url_out = g_strdup_printf(search_string, url_in);
		else {
			char *p, *url;

			if ((p = strdup(url_in)) == NULL)
				err(1, "strdup http");
			url = strtok(p, " \t");
			url_out = g_strdup_printf("http://%s", url); /* guess http */
			free(p);
		}
	}

	if (url_out == NULL)
		err(1, "asprintf pointer");

	DNPRINTF(XT_D_URL, "guess_url_type: guessed %s\n", url_out);

	return (url_out);
}

void
add_alias(char *line)
{
	char			*l, *alias;
	struct alias		*a;

	if (line == NULL)
		errx(1, "add_alias");
	l = line;

	a = g_malloc(sizeof(*a));

	if ((alias = strsep(&l, " \t,")) == NULL || l == NULL)
		errx(1, "add_alias: incomplete alias definition");

	if (strlen(alias) == 0 || strlen(l) == 0)
		errx(1, "add_alias: invalid alias definition");

	a->a_name = g_strdup(alias);
	a->a_uri = g_strdup(l);

	DNPRINTF(XT_D_CONFIG, "add_alias: %s for %s\n", a->a_name, a->a_uri);

	TAILQ_INSERT_TAIL(&aliases, a, entry);
}

void
add_mime_type(char *line)
{
	char			*mime_type;
	char			*l = NULL;
	struct mime_type	*m;

	/* XXX this could be smarter */

	if (line == NULL)
		errx(1, "add_mime_type");
	l = line;

	m = g_malloc(sizeof(*m));

	if ((mime_type = strsep(&l, " \t,")) == NULL || l == NULL)
		errx(1, "add_mime_type: invalid mime_type");

	if (mime_type[strlen(mime_type) - 1] == '*') {
		mime_type[strlen(mime_type) - 1] = '\0';
		m->mt_default = 1;
	} else
		m->mt_default = 0;

	if (strlen(mime_type) == 0 || strlen(l) == 0)
		errx(1, "add_mime_type: invalid mime_type");

	m->mt_type = g_strdup(mime_type);
	m->mt_action = g_strdup(l);

	DNPRINTF(XT_D_CONFIG, "add_mime_type: type %s action %s default %d\n",
	    m->mt_type, m->mt_action, m->mt_default);

	TAILQ_INSERT_TAIL(&mtl, m, entry);
}

struct mime_type *
find_mime_type(char *mime_type)
{
	struct mime_type	*m, *def = NULL, *rv = NULL;

	TAILQ_FOREACH(m, &mtl, entry) {
		if (m->mt_default &&
		    !strncmp(mime_type, m->mt_type, strlen(m->mt_type)))
			def = m;

		if (m->mt_default == 0 && !strcmp(mime_type, m->mt_type)) {
			rv = m;
			break;
		}
	}

	if (rv == NULL)
		rv = def;

	return (rv);
}

#define	WS	"\n= \t"
void
config_parse(char *filename)
{
	FILE			*config;
	char			*line, *cp, *var, *val;
	size_t			len, lineno = 0;

	DNPRINTF(XT_D_CONFIG, "config_parse: filename %s\n", filename);

	TAILQ_INIT(&mtl);
	TAILQ_INIT(&aliases);

	if (filename == NULL)
		return;

	if ((config = fopen(filename, "r")) == NULL) {
		warn("config_parse: cannot open %s", filename);
		return;
	}

	for (;;) {
		if ((line = fparseln(config, &len, &lineno, NULL, 0)) == NULL)
			if (feof(config) || ferror(config))
				break;

		cp = line;
		cp += (long)strspn(cp, WS);
		if (cp[0] == '\0') {
			/* empty line */
			free(line);
			continue;
		}

		if ((var = strsep(&cp, WS)) == NULL || cp == NULL)
			break;

		cp += (long)strspn(cp, WS);

		if ((val = strsep(&cp, "\0")) == NULL)
			break;

		DNPRINTF(XT_D_CONFIG, "config_parse: %s=%s\n",var ,val);

		/* get settings */
		if (!strcmp(var, "home"))
			home = g_strdup(val);
		else if (!strcmp(var, "ctrl_click_focus"))
			ctrl_click_focus = atoi(val);
		else if (!strcmp(var, "read_only_cookies"))
			read_only_cookies = atoi(val);
		else if (!strcmp(var, "cookies_enabled"))
			cookies_enabled = atoi(val);
		else if (!strcmp(var, "enable_scripts"))
			enable_scripts = atoi(val);
		else if (!strcmp(var, "enable_plugins"))
			enable_plugins = atoi(val);
		else if (!strcmp(var, "default_font_size"))
			default_font_size = atoi(val);
		else if (!strcmp(var, "default_font_family"))
			default_font_family = strdup(val);
		else if (!strcmp(var, "serif_font_family"))
			serif_font_family = strdup(val);
		else if (!strcmp(var, "sans_serif_font_family"))
			sans_serif_font_family = strdup(val);
		else if (!strcmp(var, "monospace_font_family"))
			monospace_font_family = strdup(val);
		else if (!strcmp(var, "default_encoding"))
			default_encoding = strdup(val);
		else if (!strcmp(var, "user_stylesheet"))
			user_stylesheet = strdup(val);
		else if (!strcmp(var, "fancy_bar"))
			fancy_bar = atoi(val);
		else if (!strcmp(var, "mime_type"))
			add_mime_type(val);
		else if (!strcmp(var, "alias"))
			add_alias(val);
		else if (!strcmp(var, "http_proxy")) {
			if (http_proxy)
				g_free(http_proxy);
			http_proxy = g_strdup(val);
		} else if (!strcmp(var, "search_string")) {
			if (search_string)
				g_free(search_string);
			search_string = g_strdup(val);
		} else if (!strcmp(var, "download_dir")) {
			if (val[0] == '~')
				snprintf(download_dir, sizeof download_dir,
				    "%s/%s", pwd->pw_dir, &val[1]);
			else
				strlcpy(download_dir, val, sizeof download_dir);
		} else
			errx(1, "invalid conf file entry: %s=%s", var, val);

		free(line);
	}

	fclose(config);
}

char *
js_ref_to_string(JSContextRef context, JSValueRef ref)
{
	char			*s = NULL;
	size_t			l;
	JSStringRef		jsref;

	jsref = JSValueToStringCopy(context, ref, NULL);
	if (jsref == NULL)
		return (NULL);

	l = JSStringGetMaximumUTF8CStringSize(jsref);
	s = g_malloc(l);
	if (s)
		JSStringGetUTF8CString(jsref, s, l);
	JSStringRelease(jsref);

	return (s);
}

void
disable_hints(struct tab *t)
{
	bzero(t->hint_buf, sizeof t->hint_buf);
	bzero(t->hint_num, sizeof t->hint_num);
	run_script(t, "vimprobable_clear()");
	t->hints_on = 0;
	t->hint_mode = XT_HINT_NONE;
}

void
enable_hints(struct tab *t)
{
	bzero(t->hint_buf, sizeof t->hint_buf);
	run_script(t, "vimprobable_show_hints()");
	t->hints_on = 1;
	t->hint_mode = XT_HINT_NONE;
}

#define XT_JS_OPEN	("open;")
#define XT_JS_OPEN_LEN	(strlen(XT_JS_OPEN))
#define XT_JS_FIRE	("fire;")
#define XT_JS_FIRE_LEN	(strlen(XT_JS_FIRE))
#define XT_JS_FOUND	("found;")
#define XT_JS_FOUND_LEN	(strlen(XT_JS_FOUND))

int
run_script(struct tab *t, char *s)
{
	JSGlobalContextRef	ctx;
	WebKitWebFrame		*frame;
	JSStringRef		str;
	JSValueRef		val, exception;
	char			*es, buf[128];

	DNPRINTF(XT_D_JS, "run_script: tab %d %s\n",
	    t->tab_id, s == (char *)JS_HINTING ? "JS_HINTING" : s);

	frame = webkit_web_view_get_main_frame(t->wv);
	ctx = webkit_web_frame_get_global_context(frame);

	str = JSStringCreateWithUTF8CString(s);
	val = JSEvaluateScript(ctx, str, JSContextGetGlobalObject(ctx),
	    NULL, 0, &exception);
	JSStringRelease(str);

	DNPRINTF(XT_D_JS, "run_script: val %p\n", val);
	if (val == NULL) {
		es = js_ref_to_string(ctx, exception);
		DNPRINTF(XT_D_JS, "run_script: exception %s\n", es);
		g_free(es);
		return (1);
	} else {
		es = js_ref_to_string(ctx, val);
		DNPRINTF(XT_D_JS, "run_script: val %s\n", es);

		/* handle return value right here */
		if (!strncmp(es, XT_JS_OPEN, XT_JS_OPEN_LEN)) {
			disable_hints(t);
			webkit_web_view_load_uri(t->wv, &es[XT_JS_OPEN_LEN]);
		}

		if (!strncmp(es, XT_JS_FIRE, XT_JS_FIRE_LEN)) {
			snprintf(buf, sizeof buf, "vimprobable_fire(%s)",
			    &es[XT_JS_FIRE_LEN]);
			run_script(t, buf);
			disable_hints(t);
		}

		if (!strncmp(es, XT_JS_FOUND, XT_JS_FOUND_LEN)) {
			if (atoi(&es[XT_JS_FOUND_LEN]) == 0)
				disable_hints(t);
		}

		g_free(es);
	}

	return (0);
}

int
hint(struct tab *t, struct karg *args)
{

	DNPRINTF(XT_D_JS, "hint: tab %d\n", t->tab_id);

	if (t->hints_on == 0)
		enable_hints(t);
	else
		disable_hints(t);

	return (0);
}

int
quit(struct tab *t, struct karg *args)
{
	gtk_main_quit();

	return (1);
}

int
focus(struct tab *t, struct karg *args)
{
	if (t == NULL || args == NULL)
		errx(1, "focus");

	if (args->i == XT_FOCUS_URI)
		gtk_widget_grab_focus(GTK_WIDGET(t->uri_entry));
	else if (args->i == XT_FOCUS_SEARCH)
		gtk_widget_grab_focus(GTK_WIDGET(t->search_entry));

	return (0);
}

int
help(struct tab *t, struct karg *args)
{
	if (t == NULL)
		errx(1, "help");

	webkit_web_view_load_string(t->wv,
	    "<html><body><h1>XXXTerm</h1></body></html>",
	    NULL,
	    NULL,
	    NULL);

	return (0);
}

int
favorites(struct tab *t, struct karg *args)
{
	char			file[PATH_MAX];
	FILE			*f, *h;
	char			*uri = NULL, *title = NULL;
	size_t			len, lineno = 0;
	int			i, failed = 0;

	if (t == NULL)
		errx(1, "favorites");

	/* XXX run a digest over the favorites file instead of always generating it */

	/* open favorites */
	snprintf(file, sizeof file, "%s/%s/%s",
	    pwd->pw_dir, XT_DIR, XT_FAVS_FILE);
	if ((f = fopen(file, "r")) == NULL) {
		warn("favorites");
		return (1);
	}

	/* open favorites html */
	snprintf(file, sizeof file, "%s/%s/%s.html",
	    pwd->pw_dir, XT_DIR, XT_FAVS_FILE);
	if ((h = fopen(file, "w+")) == NULL) {
		warn("favorites.html");
		return (1);
	}

	fprintf(h, "<html><body>Favorites:<p>\n<ol>\n");

	for (i = 1;;) {
		if ((title = fparseln(f, &len, &lineno, NULL, 0)) == NULL)
			if (feof(f) || ferror(f))
				break;
		if (len == 0) {
			free(title);
			title = NULL;
			continue;
		}

		if ((uri = fparseln(f, &len, &lineno, NULL, 0)) == NULL)
			if (feof(f) || ferror(f)) {
				failed = 1;
				break;
			}

		fprintf(h, "<li><a href=\"%s\">%s</a><br>\n", uri, title);

		free(uri);
		uri = NULL;
		free(title);
		title = NULL;
		i++;
	}

	if (uri)
		free(uri);
	if (title)
		free(title);

	fprintf(h, "</ol></body></html>");
	fclose(f);
	fclose(h);

	if (failed) {
		webkit_web_view_load_string(t->wv,
		    "<html><body>Invalid favorites file</body></html>",
		    NULL,
		    NULL,
		    NULL);
	} else {
		snprintf(file, sizeof file, "file://%s/%s/%s.html",
		    pwd->pw_dir, XT_DIR, XT_FAVS_FILE);
		webkit_web_view_load_uri(t->wv, file);
	}

	return (0);
}

int
favadd(struct tab *t, struct karg *args)
{
	char			file[PATH_MAX];
	FILE			*f;
	char			*line = NULL;
	size_t			urilen, linelen;
	WebKitWebFrame		*frame;
	const gchar		*uri, *title;

	if (t == NULL)
		errx(1, "favadd");

	snprintf(file, sizeof file, "%s/%s/%s",
	    pwd->pw_dir, XT_DIR, XT_FAVS_FILE);
	if ((f = fopen(file, "r+")) == NULL) {
		warn("favorites");
		return (1);
	}

	title = webkit_web_view_get_title(t->wv);
	frame = webkit_web_view_get_main_frame(t->wv);
	uri = webkit_web_frame_get_uri(frame);
	if (title == NULL)
		title = uri;

	if (title == NULL || uri == NULL) {
		webkit_web_view_load_string(t->wv,
		    "<html><body>can't add page to favorites</body></html>",
		    NULL,
		    NULL,
		    NULL);
		goto done;
	}

	urilen = strlen(uri);

	while (!feof(f)) {
		line = fparseln(f, &linelen, NULL, NULL, 0);
		if (linelen == urilen && !strcmp(line, uri))
			goto done;
		free(line);
		line = NULL;
	}

	fprintf(f, "\n%s\n%s", title, uri);
done:
	if (line)
		free(line);
	fclose(f);

	return (0);
}

int
navaction(struct tab *t, struct karg *args)
{
	DNPRINTF(XT_D_NAV, "navaction: tab %d opcode %d\n",
	    t->tab_id, args->i);

	switch (args->i) {
	case XT_NAV_BACK:
		webkit_web_view_go_back(t->wv);
		break;
	case XT_NAV_FORWARD:
		webkit_web_view_go_forward(t->wv);
		break;
	case XT_NAV_RELOAD:
		webkit_web_view_reload(t->wv);
		break;
	}
	return (XT_CB_PASSTHROUGH);
}

int
move(struct tab *t, struct karg *args)
{
	GtkAdjustment		*adjust;
	double			pi, si, pos, ps, upper, lower, max;

	switch (args->i) {
	case XT_MOVE_DOWN:
	case XT_MOVE_UP:
	case XT_MOVE_BOTTOM:
	case XT_MOVE_TOP:
	case XT_MOVE_PAGEDOWN:
	case XT_MOVE_PAGEUP:
	case XT_MOVE_HALFDOWN:
	case XT_MOVE_HALFUP:
		adjust = t->adjust_v;
		break;
	default:
		adjust = t->adjust_h;
		break;
	}

	pos = gtk_adjustment_get_value(adjust);
	ps = gtk_adjustment_get_page_size(adjust);
	upper = gtk_adjustment_get_upper(adjust);
	lower = gtk_adjustment_get_lower(adjust);
	si = gtk_adjustment_get_step_increment(adjust);
	pi = gtk_adjustment_get_page_increment(adjust);
	max = upper - ps;

	DNPRINTF(XT_D_MOVE, "move: opcode %d %s pos %f ps %f upper %f lower %f "
	    "max %f si %f pi %f\n",
	    args->i, adjust == t->adjust_h ? "horizontal" : "vertical",
	    pos, ps, upper, lower, max, si, pi);

	switch (args->i) {
	case XT_MOVE_DOWN:
	case XT_MOVE_RIGHT:
		pos += si;
		gtk_adjustment_set_value(adjust, MIN(pos, max));
		break;
	case XT_MOVE_UP:
	case XT_MOVE_LEFT:
		pos -= si;
		gtk_adjustment_set_value(adjust, MAX(pos, lower));
		break;
	case XT_MOVE_BOTTOM:
	case XT_MOVE_FARRIGHT:
		gtk_adjustment_set_value(adjust, max);
		break;
	case XT_MOVE_TOP:
	case XT_MOVE_FARLEFT:
		gtk_adjustment_set_value(adjust, lower);
		break;
	case XT_MOVE_PAGEDOWN:
		pos += pi;
		gtk_adjustment_set_value(adjust, MIN(pos, max));
		break;
	case XT_MOVE_PAGEUP:
		pos -= pi;
		gtk_adjustment_set_value(adjust, MAX(pos, lower));
		break;
	case XT_MOVE_HALFDOWN:
		pos += pi / 2;
		gtk_adjustment_set_value(adjust, MIN(pos, max));
		break;
	case XT_MOVE_HALFUP:
		pos -= pi / 2;
		gtk_adjustment_set_value(adjust, MAX(pos, lower));
		break;
	default:
		return (XT_CB_PASSTHROUGH);
	}

	DNPRINTF(XT_D_MOVE, "move: new pos %f %f\n", pos, MIN(pos, max));

	return (XT_CB_HANDLED);
}

char *
getparams(char *cmd, char *cmp)
{
	char			*rv = NULL;

	if (cmd && cmp) {
		if (!strncmp(cmd, cmp, strlen(cmp))) {
			rv = cmd + strlen(cmp);
			while (*rv == ' ')
				rv++;
			if (strlen(rv) == 0)
				rv = NULL;
		}
	}

	return (rv);
}

int
tabaction(struct tab *t, struct karg *args)
{
	int			rv = XT_CB_HANDLED;
	char			*url = NULL, *newuri = NULL;

	DNPRINTF(XT_D_TAB, "tabaction: %p %d %d\n", t, args->i, t->focus_wv);

	if (t == NULL)
		return (XT_CB_PASSTHROUGH);

	switch (args->i) {
	case XT_TAB_NEW:
		if ((url = getparams(args->s, "tabnew")))
			create_new_tab(url, 1);
		else
			create_new_tab(NULL, 1);
		break;
	case XT_TAB_DELETE:
		delete_tab(t);
		break;
	case XT_TAB_DELQUIT:
		if (gtk_notebook_get_n_pages(notebook) > 1)
			delete_tab(t);
		else
			quit(t, args);
		break;
	case XT_TAB_OPEN:
		if ((url = getparams(args->s, "open")) ||
		    ((url = getparams(args->s, "op"))) ||
		    ((url = getparams(args->s, "o"))))
			;
		else {
			rv = XT_CB_PASSTHROUGH;
			goto done;
		}

		if (valid_url_type(url)) {
			newuri = guess_url_type(url);
			url = newuri;
		}
		webkit_web_view_load_uri(t->wv, url);
		if (newuri)
			g_free(newuri);
		break;
	default:
		rv = XT_CB_PASSTHROUGH;
		goto done;
	}

done:
	if (args->s) {
		g_free(args->s);
		args->s = NULL;
	}

	return (rv);
}

int
resizetab(struct tab *t, struct karg *args)
{
	if (t == NULL || args == NULL)
		errx(1, "resizetab");

	DNPRINTF(XT_D_TAB, "resizetab: tab %d %d\n",
	    t->tab_id, args->i);

	adjustfont_webkit(t, args->i);

	return (XT_CB_HANDLED);
}

int
movetab(struct tab *t, struct karg *args)
{
	struct tab		*tt;
	int			x;

	if (t == NULL || args == NULL)
		errx(1, "movetab");

	DNPRINTF(XT_D_TAB, "movetab: tab %d opcode %d\n",
	    t->tab_id, args->i);

	if (args->i == XT_TAB_INVALID)
		return (XT_CB_PASSTHROUGH);

	if (args->i < XT_TAB_INVALID) {
		/* next or previous tab */
		if (TAILQ_EMPTY(&tabs))
			return (XT_CB_PASSTHROUGH);

		switch (args->i) {
		case XT_TAB_NEXT:
			gtk_notebook_next_page(notebook);
			break;
		case XT_TAB_PREV:
			gtk_notebook_prev_page(notebook);
			break;
		case XT_TAB_FIRST:
			gtk_notebook_set_current_page(notebook, 0);
			break;
		case XT_TAB_LAST:
			gtk_notebook_set_current_page(notebook, -1);
			break;
		default:
			return (XT_CB_PASSTHROUGH);
		}

		return (XT_CB_HANDLED);
	}

	/* jump to tab */
	x = args->i - 1;
	if (t->tab_id == x) {
		DNPRINTF(XT_D_TAB, "movetab: do nothing\n");
		return (XT_CB_HANDLED);
	}

	TAILQ_FOREACH(tt, &tabs, entry) {
		if (tt->tab_id == x) {
			gtk_notebook_set_current_page(notebook, x);
			DNPRINTF(XT_D_TAB, "movetab: going to %d\n", x);
			if (tt->focus_wv)
				gtk_widget_grab_focus(GTK_WIDGET(tt->wv));
		}
	}

	return (XT_CB_HANDLED);
}

int
command(struct tab *t, struct karg *args)
{
	char			*s = NULL;
	GdkColor		color;

	if (t == NULL || args == NULL)
		errx(1, "command");

	if (args->i == '/')
		s = "/";
	else if (args->i == '?')
		s = "?";
	else if (args->i == ':')
		s = ":";
	else {
		warnx("invalid command %c\n", args->i);
		return (XT_CB_PASSTHROUGH);
	}

	DNPRINTF(XT_D_CMD, "command: type %s\n", s);

	gtk_entry_set_text(GTK_ENTRY(t->cmd), s);
	gdk_color_parse("white", &color);
	gtk_widget_modify_base(t->cmd, GTK_STATE_NORMAL, &color);
	gtk_widget_show(t->cmd);
	gtk_widget_grab_focus(GTK_WIDGET(t->cmd));
	gtk_editable_set_position(GTK_EDITABLE(t->cmd), -1);

	return (XT_CB_HANDLED);
}

int
search(struct tab *t, struct karg *args)
{
	gboolean		d;

	if (t == NULL || args == NULL)
		errx(1, "search");
	if (t->search_text == NULL)
		return (XT_CB_PASSTHROUGH);

	DNPRINTF(XT_D_CMD, "search: tab %d opc %d forw %d text %s\n",
	    t->tab_id, args->i, t->search_forward, t->search_text);

	switch (args->i) {
	case  XT_SEARCH_NEXT:
		d = t->search_forward;
		break;
	case  XT_SEARCH_PREV:
		d = !t->search_forward;
		break;
	default:
		return (XT_CB_PASSTHROUGH);
	}

	webkit_web_view_search_text(t->wv, t->search_text, FALSE, d, TRUE);

	return (XT_CB_HANDLED);
}

int
mnprintf(char **buf, int *len, char *fmt, ...)
{
	int			x, old_len;
	va_list			ap;

	va_start(ap, fmt);

	old_len = *len;
	x = vsnprintf(*buf, *len, fmt, ap);
	if (x == -1)
		err(1, "mnprintf");
	if (old_len < x)
		errx(1, "mnprintf: buffer overflow");

	*buf += x;
	*len -= x;

	va_end(ap);

	return (0);
}

int
set(struct tab *t, struct karg *args)
{
	struct mime_type	*m;
	char			b[16 * 1024], *s, *pars;
	int			l;

	if (t == NULL || args == NULL)
		errx(1, "set");

	DNPRINTF(XT_D_CMD, "set: tab %d\n",
	    t->tab_id);

	s = b;
	l = sizeof b;

	if ((pars = getparams(args->s, "set")) == NULL) {
		mnprintf(&s, &l, "<html><body><pre>");
		mnprintf(&s, &l, "ctrl_click_focus\t= %d<br>", ctrl_click_focus);
		mnprintf(&s, &l, "cookies_enabled\t\t= %d<br>", cookies_enabled);
		mnprintf(&s, &l, "default_font_size\t= %d<br>", default_font_size);
		mnprintf(&s, &l, "default_font_family\t= %d<br>", default_font_family);
		mnprintf(&s, &l, "serif_font_family\t= %d<br>", serif_font_family);
		mnprintf(&s, &l, "sans_serif_font_family\t= %d<br>", sans_serif_font_family);
		mnprintf(&s, &l, "monospace_font_family\t= %d<br>", monospace_font_family);
		mnprintf(&s, &l, "default_encoding\t= %s<br>", default_encoding);
		mnprintf(&s, &l, "user_stylesheet\t= %d<br>", user_stylesheet);
		mnprintf(&s, &l, "enable_plugins\t\t= %d<br>", enable_plugins);
		mnprintf(&s, &l, "enable_scripts\t\t= %d<br>", enable_scripts);
		mnprintf(&s, &l, "fancy_bar\t\t= %d<br>", fancy_bar);
		mnprintf(&s, &l, "home\t\t\t= %s<br>", home);
		TAILQ_FOREACH(m, &mtl, entry) {
			mnprintf(&s, &l, "mime_type\t\t= %s%s,%s<br>",
			    m->mt_type, m->mt_default ? "*" : "", m->mt_action);
		}
		mnprintf(&s, &l, "proxy_uri\t\t= %s<br>", proxy_uri);
		mnprintf(&s, &l, "read_only_cookies\t= %d<br>", read_only_cookies);
		mnprintf(&s, &l, "search_string\t\t= %s<br>", search_string);
		mnprintf(&s, &l, "showurl\t\t\t= %d<br>", showurl);
		mnprintf(&s, &l, "showtabs\t\t= %d<br>", showtabs);
		mnprintf(&s, &l, "tabless\t\t\t= %d<br>", tabless);
		mnprintf(&s, &l, "download_dir\t\t= %s<br>", download_dir);
		mnprintf(&s, &l, "</pre></body></html>");

		webkit_web_view_load_string(t->wv,
		    b,
		    NULL,
		    NULL,
		    "about:config");
		goto done;
	}

	/* XXX this sucks donkey balls and is a POC only */
	int			x;
	char			*e;
	if (!strncmp(pars, "enable_scripts ", strlen("enable_scripts"))) {
		s = pars + strlen("enable_scripts");
		x = strtol(s, &e, 10);
		if (s[0] == '\0' || *e != '\0')
			webkit_web_view_load_string(t->wv,
			    "<html><body>invalid value</body></html>",
			    NULL,
			    NULL,
			    "about:error");

		enable_scripts = x;
		g_object_set((GObject *)t->settings,
		    "enable-scripts", enable_scripts, (char *)NULL);
		webkit_web_view_set_settings(t->wv, t->settings);
	}

done:
	if (args->s) {
		g_free(args->s);
		args->s = NULL;
	}

	return (XT_CB_PASSTHROUGH);
}

/* inherent to GTK not all keys will be caught at all times */
struct key {
	guint		mask;
	guint		modkey;
	guint		key;
	int		(*func)(struct tab *, struct karg *);
	struct karg	arg;
} keys[] = {
	{ 0,			0,	GDK_slash,	command,	{.i = '/'} },
	{ GDK_SHIFT_MASK,	0,	GDK_question,	command,	{.i = '?'} },
	{ GDK_SHIFT_MASK,	0,	GDK_colon,	command,	{.i = ':'} },
	{ GDK_CONTROL_MASK,	0,	GDK_q,		quit,		{0} },

	/* search */
	{ 0,			0,	GDK_n,		search,		{.i = XT_SEARCH_NEXT} },
	{ GDK_SHIFT_MASK,	0,	GDK_N,		search,		{.i = XT_SEARCH_PREV} },

	/* focus */
	{ 0,			0,	GDK_F6,		focus,		{.i = XT_FOCUS_URI} },
	{ 0,			0,	GDK_F7,		focus,		{.i = XT_FOCUS_SEARCH} },

	/* hinting */
	{ 0,			0,	GDK_f,		hint,		{.i = 0} },

	/* navigation */
	{ 0,			0,	GDK_BackSpace,	navaction,	{.i = XT_NAV_BACK} },
	{ GDK_MOD1_MASK,	0,	GDK_Left,	navaction,	{.i = XT_NAV_BACK} },
	{ GDK_SHIFT_MASK,	0,	GDK_BackSpace,	navaction,	{.i = XT_NAV_FORWARD} },
	{ GDK_MOD1_MASK,	0,	GDK_Right,	navaction,	{.i = XT_NAV_FORWARD} },
	{ 0,			0,	GDK_F5,		navaction,	{.i = XT_NAV_RELOAD} },
	{ GDK_CONTROL_MASK,	0,	GDK_r,		navaction,	{.i = XT_NAV_RELOAD} },
	{ GDK_CONTROL_MASK,	0,	GDK_l,		navaction,	{.i = XT_NAV_RELOAD} },

	/* vertical movement */
	{ 0,			0,	GDK_j,		move,		{.i = XT_MOVE_DOWN} },
	{ 0,			0,	GDK_Down,	move,		{.i = XT_MOVE_DOWN} },
	{ 0,			0,	GDK_Up,		move,		{.i = XT_MOVE_UP} },
	{ 0,			0,	GDK_k,		move,		{.i = XT_MOVE_UP} },
	{ GDK_SHIFT_MASK,	0,	GDK_G,		move,		{.i = XT_MOVE_BOTTOM} },
	{ 0,			0,	GDK_End,	move,		{.i = XT_MOVE_BOTTOM} },
	{ 0,			0,	GDK_Home,	move,		{.i = XT_MOVE_TOP} },
	{ 0,			GDK_g,	GDK_g,		move,		{.i = XT_MOVE_TOP} }, /* XXX make this work */
	{ 0,			0,	GDK_space,	move,		{.i = XT_MOVE_PAGEDOWN} },
	{ GDK_CONTROL_MASK,	0,	GDK_f,		move,		{.i = XT_MOVE_PAGEDOWN} },
	{ GDK_CONTROL_MASK,	0,	GDK_d,		move,		{.i = XT_MOVE_HALFDOWN} },
	{ 0,			0,	GDK_Page_Down,	move,		{.i = XT_MOVE_PAGEDOWN} },
	{ 0,			0,	GDK_Page_Up,	move,		{.i = XT_MOVE_PAGEUP} },
	{ GDK_CONTROL_MASK,	0,	GDK_b,		move,		{.i = XT_MOVE_PAGEUP} },
	{ GDK_CONTROL_MASK,	0,	GDK_u,		move,		{.i = XT_MOVE_HALFUP} },
	/* horizontal movement */
	{ 0,			0,	GDK_l,		move,		{.i = XT_MOVE_RIGHT} },
	{ 0,			0,	GDK_Right,	move,		{.i = XT_MOVE_RIGHT} },
	{ 0,			0,	GDK_Left,	move,		{.i = XT_MOVE_LEFT} },
	{ 0,			0,	GDK_h,		move,		{.i = XT_MOVE_LEFT} },
	{ GDK_SHIFT_MASK,	0,	GDK_dollar,	move,		{.i = XT_MOVE_FARRIGHT} },
	{ 0,			0,	GDK_0,		move,		{.i = XT_MOVE_FARLEFT} },

	/* tabs */
	{ GDK_CONTROL_MASK,	0,	GDK_t,		tabaction,	{.i = XT_TAB_NEW} },
	{ GDK_CONTROL_MASK,	0,	GDK_w,		tabaction,	{.i = XT_TAB_DELETE} },
	{ GDK_CONTROL_MASK,	0,	GDK_1,		movetab,	{.i = 1} },
	{ GDK_CONTROL_MASK,	0,	GDK_2,		movetab,	{.i = 2} },
	{ GDK_CONTROL_MASK,	0,	GDK_3,		movetab,	{.i = 3} },
	{ GDK_CONTROL_MASK,	0,	GDK_4,		movetab,	{.i = 4} },
	{ GDK_CONTROL_MASK,	0,	GDK_5,		movetab,	{.i = 5} },
	{ GDK_CONTROL_MASK,	0,	GDK_6,		movetab,	{.i = 6} },
	{ GDK_CONTROL_MASK,	0,	GDK_7,		movetab,	{.i = 7} },
	{ GDK_CONTROL_MASK,	0,	GDK_8,		movetab,	{.i = 8} },
	{ GDK_CONTROL_MASK,	0,	GDK_9,		movetab,	{.i = 9} },
	{ GDK_CONTROL_MASK,	0,	GDK_0,		movetab,	{.i = 10} },
	{ GDK_CONTROL_MASK|GDK_SHIFT_MASK, 0, GDK_less, movetab,	{.i = XT_TAB_FIRST} },
	{ GDK_CONTROL_MASK|GDK_SHIFT_MASK, 0, GDK_greater, movetab,	{.i = XT_TAB_LAST} },
	{ GDK_CONTROL_MASK,	0,	GDK_minus,	resizetab,	{.i = -1} },
	{ GDK_CONTROL_MASK|GDK_SHIFT_MASK, 0, GDK_plus,	resizetab,	{.i = 1} },
	{ GDK_CONTROL_MASK,	0,	GDK_equal,	resizetab,	{.i = 1} },
};

struct cmd {
	char		*cmd;
	int		params;
	int		(*func)(struct tab *, struct karg *);
	struct karg	arg;
} cmds[] = {
	{ "q!",			0,	quit,			{0} },
	{ "qa",			0,	quit,			{0} },
	{ "qa!",		0,	quit,			{0} },
	{ "help",		0,	help,			{0} },

	/* favorites */
	{ "fav",		0,	favorites,		{0} },
	{ "favadd",		0,	favadd,			{0} },

	{ "1",			0,	move,			{.i = XT_MOVE_TOP} },

	/* tabs */
	{ "o",			1,	tabaction,		{.i = XT_TAB_OPEN} },
	{ "op",			1,	tabaction,		{.i = XT_TAB_OPEN} },
	{ "open",		1,	tabaction,		{.i = XT_TAB_OPEN} },
	{ "tabnew",		1,	tabaction,		{.i = XT_TAB_NEW} },
	{ "tabedit",		1,	tabaction,		{.i = XT_TAB_NEW} },
	{ "tabe",		1,	tabaction,		{.i = XT_TAB_NEW} },
	{ "tabclose",		0,	tabaction,		{.i = XT_TAB_DELETE} },
	{ "tabc",		0,	tabaction,		{.i = XT_TAB_DELETE} },
	{ "quit",		0,	tabaction,		{.i = XT_TAB_DELQUIT} },
	{ "q",			0,	tabaction,		{.i = XT_TAB_DELQUIT} },
	/* XXX add count to these commands */
	{ "tabfirst",		0,	movetab,		{.i = XT_TAB_FIRST} },
	{ "tabfir",		0,	movetab,		{.i = XT_TAB_FIRST} },
	{ "tabrewind",		0,	movetab,		{.i = XT_TAB_FIRST} },
	{ "tabr",		0,	movetab,		{.i = XT_TAB_FIRST} },
	{ "tablast",		0,	movetab,		{.i = XT_TAB_LAST} },
	{ "tabl",		0,	movetab,		{.i = XT_TAB_LAST} },
	{ "tabprevious",	0,	movetab,		{.i = XT_TAB_PREV} },
	{ "tabp",		0,	movetab,		{.i = XT_TAB_PREV} },
	{ "tabnext",		0,	movetab,		{.i = XT_TAB_NEXT} },
	{ "tabn",		0,	movetab,		{.i = XT_TAB_NEXT} },

	/* settings */
	{ "set",		1,	set,			{0} },
};

gboolean
tab_close_cb(GtkWidget *button, struct tab *t)
{
	DNPRINTF(XT_D_TAB, "tab_close_cb: tab %d\n", t->tab_id);

	delete_tab(t);

	return (FALSE);
}

void
focus_uri_entry_cb(GtkWidget* w, GtkDirectionType direction, struct tab *t)
{
	DNPRINTF(XT_D_URL, "focus_uri_entry_cb: tab %d focus_wv %d\n",
	    t->tab_id, t->focus_wv);

	if (t == NULL)
		errx(1, "focus_uri_entry_cb");

	/* focus on wv instead */
	if (t->focus_wv)
		gtk_widget_grab_focus(GTK_WIDGET(t->wv));
}

void
activate_uri_entry_cb(GtkWidget* entry, struct tab *t)
{
	const gchar		*uri = gtk_entry_get_text(GTK_ENTRY(entry));
	char			*newuri = NULL;

	DNPRINTF(XT_D_URL, "activate_uri_entry_cb: %s\n", uri);

	if (t == NULL)
		errx(1, "activate_uri_entry_cb");

	if (uri == NULL)
		errx(1, "uri");

	uri += strspn(uri, "\t ");

	if (valid_url_type((char *)uri)) {
		newuri = guess_url_type((char *)uri);
		uri = newuri;
	}

	webkit_web_view_load_uri(t->wv, uri);
	gtk_widget_grab_focus(GTK_WIDGET(t->wv));

	if (newuri)
		g_free(newuri);
}

void
activate_search_entry_cb(GtkWidget* entry, struct tab *t)
{
	const gchar		*search = gtk_entry_get_text(GTK_ENTRY(entry));
	char			*newuri = NULL;

	DNPRINTF(XT_D_URL, "activate_search_entry_cb: %s\n", search);

	if (t == NULL)
		errx(1, "activate_search_entry_cb");

	if (search_string == NULL) {
		warnx("no search_string");
		return;
	}

	newuri = g_strdup_printf(search_string, search);

	webkit_web_view_load_uri(t->wv, newuri);
	gtk_widget_grab_focus(GTK_WIDGET(t->wv));

	if (newuri)
		g_free(newuri);
}

void
notify_load_status_cb(WebKitWebView* wview, GParamSpec* pspec, struct tab *t)
{
	GdkColor		color;
	WebKitWebFrame		*frame;
	const gchar		*uri;

	if (t == NULL)
		errx(1, "notify_load_status_cb");

	switch (webkit_web_view_get_load_status(wview)) {
	case WEBKIT_LOAD_COMMITTED:
		gtk_widget_show(t->spinner);
		gtk_spinner_start(t->spinner);
		frame = webkit_web_view_get_main_frame(wview);
		uri = webkit_web_frame_get_uri(frame);
		if (uri)
			gtk_entry_set_text(GTK_ENTRY(t->uri_entry), uri);

		gtk_widget_set_sensitive(GTK_WIDGET(t->stop), TRUE);
		t->focus_wv = 1;

		/* take focus if we are visible */
		if (gtk_notebook_get_current_page(notebook) == t->tab_id)
			gtk_widget_grab_focus(GTK_WIDGET(t->wv));

		/* color uri_entry */
		if (uri && !strncmp(uri, "https://", strlen("https://")))
			gdk_color_parse("green", &color);
		else
			gdk_color_parse("white", &color);
		gtk_widget_modify_base(t->uri_entry, GTK_STATE_NORMAL, &color);

		break;

	case WEBKIT_LOAD_FIRST_VISUALLY_NON_EMPTY_LAYOUT:
		uri = webkit_web_view_get_title(wview);
		if (uri == NULL) {
			frame = webkit_web_view_get_main_frame(wview);
			uri = webkit_web_frame_get_uri(frame);
		}
		gtk_label_set_text(GTK_LABEL(t->label), uri);
		gtk_window_set_title(GTK_WINDOW(main_window), uri);

		break;

	case WEBKIT_LOAD_PROVISIONAL:
	case WEBKIT_LOAD_FINISHED:
#if WEBKIT_CHECK_VERSION(1, 1, 18)
	case WEBKIT_LOAD_FAILED:
#endif
		gtk_spinner_stop(t->spinner);
		gtk_widget_hide(t->spinner);
	default:
		gtk_widget_set_sensitive(GTK_WIDGET(t->stop), FALSE);
		break;
	}

	gtk_widget_set_sensitive(GTK_WIDGET(t->backward),
	    webkit_web_view_can_go_back(wview));

	gtk_widget_set_sensitive(GTK_WIDGET(t->forward),
	    webkit_web_view_can_go_forward(wview));
}

void
webview_load_finished_cb(WebKitWebView *wv, WebKitWebFrame *wf, struct tab *t)
{
	run_script(t, JS_HINTING);
}

void
webview_progress_changed_cb(WebKitWebView *wv, int progress, struct tab *t)
{
	gtk_entry_set_progress_fraction(GTK_ENTRY(t->uri_entry),
	    progress == 100 ? 0 : (double)progress / 100);
}

int
webview_nw_cb(WebKitWebView *wv, WebKitWebFrame *wf,
    WebKitNetworkRequest *request, WebKitWebNavigationAction *na,
    WebKitWebPolicyDecision *pd, struct tab *t)
{
	char			*uri;

	if (t == NULL)
		errx(1, "webview_nw_cb");

	DNPRINTF(XT_D_NAV, "webview_nw_cb: %s\n",
	    webkit_network_request_get_uri(request));

	/* open in current tab */
	uri = (char *)webkit_network_request_get_uri(request);
	webkit_web_view_load_uri(t->wv, uri);
	webkit_web_policy_decision_ignore(pd);

	return (TRUE); /* we made the decission */
}

int
webview_npd_cb(WebKitWebView *wv, WebKitWebFrame *wf,
    WebKitNetworkRequest *request, WebKitWebNavigationAction *na,
    WebKitWebPolicyDecision *pd, struct tab *t)
{
	char			*uri;

	if (t == NULL)
		errx(1, "webview_npd_cb");

	DNPRINTF(XT_D_NAV, "webview_npd_cb: ctrl_click %d %s\n",
	    t->ctrl_click,
	    webkit_network_request_get_uri(request));

	uri = (char *)webkit_network_request_get_uri(request);
	if (t->ctrl_click) {
		t->ctrl_click = 0;
		create_new_tab(uri, ctrl_click_focus);
		webkit_web_policy_decision_ignore(pd);
		return (TRUE); /* we made the decission */
	}

	webkit_web_policy_decision_use(pd);
	return (TRUE); /* we made the decission */
}

WebKitWebView *
webview_cwv_cb(WebKitWebView *wv, WebKitWebFrame *wf, struct tab *t)
{
	if (t == NULL)
		errx(1, "webview_cwv_cb");

	DNPRINTF(XT_D_NAV, "webview_cwv_cb: %s\n",
	    webkit_web_view_get_uri(wv));

	return (wv);
}

int
webview_event_cb(GtkWidget *w, GdkEventButton *e, struct tab *t)
{
	/* we can not eat the event without throwing gtk off so defer it */

	/* catch middle click */
	if (e->type == GDK_BUTTON_RELEASE && e->button == 2) {
		t->ctrl_click = 1;
		goto done;
	}

	/* catch ctrl click */
	if (e->type == GDK_BUTTON_RELEASE &&
	    CLEAN(e->state) == GDK_CONTROL_MASK)
		t->ctrl_click = 1;
	else
		t->ctrl_click = 0;
done:
	return (XT_CB_PASSTHROUGH);
}

int
run_mimehandler(struct tab *t, char *mime_type, WebKitNetworkRequest *request)
{
	struct mime_type	*m;

	m = find_mime_type(mime_type);
	if (m == NULL)
		return (1);

	switch (fork()) {
	case -1:
		err(1, "fork");
		/* NOTREACHED */
	case 0:
		break;
	default:
		return (0);
	}

	/* child */
	execlp(m->mt_action, m->mt_action,
	    webkit_network_request_get_uri(request), (void *)NULL);

	_exit(0);

	/* NOTREACHED */
	return (0);
}

int
webview_mimetype_cb(WebKitWebView *wv, WebKitWebFrame *frame,
    WebKitNetworkRequest *request, char *mime_type,
    WebKitWebPolicyDecision *decision, struct tab *t)
{
	if (t == NULL)
		errx(1, "webview_mimetype_cb");

	DNPRINTF(XT_D_DOWNLOAD, "webview_mimetype_cb: tab %d mime %s\n",
	    t->tab_id, mime_type);

	if (run_mimehandler(t, mime_type, request) == 0) {
		webkit_web_policy_decision_ignore(decision);
		gtk_widget_grab_focus(GTK_WIDGET(t->wv));
		return (TRUE);
	}

	if (webkit_web_view_can_show_mime_type(wv, mime_type) == FALSE) {
		webkit_web_policy_decision_download(decision);
		return (TRUE);
	}

	return (FALSE);
}

int
webview_download_cb(WebKitWebView *wv, WebKitDownload *download, struct tab *t)
{
	const gchar		*filename;
	char			*uri = NULL;

	if (download == NULL || t == NULL)
		errx(1, "webview_download_cb: invalid pointers");

	filename = webkit_download_get_suggested_filename(download);
	if (filename == NULL)
		return (FALSE); /* abort download */

	uri = g_strdup_printf("file://%s/%s", download_dir, filename);

	DNPRINTF(XT_D_DOWNLOAD, "webview_download_cb: tab %d filename %s "
	    "local %s\n",
	    t->tab_id, filename, uri);

	webkit_download_set_destination_uri(download, uri);
	webkit_download_start(download);

	if (uri)
		g_free(uri);

	return (TRUE); /* start download */
}

/* XXX currently unused */
void
webview_hover_cb(WebKitWebView *wv, gchar *title, gchar *uri, struct tab *t)
{
	DNPRINTF(XT_D_KEY, "webview_hover_cb: %s %s\n", title, uri);

	if (t == NULL)
		errx(1, "webview_hover_cb");

	if (uri) {
		if (t->hover) {
			g_free(t->hover);
			t->hover = NULL;
		}
		t->hover = g_strdup(uri);
	} else if (t->hover) {
		g_free(t->hover);
		t->hover = NULL;
	}
}

int
webview_keypress_cb(GtkWidget *w, GdkEventKey *e, struct tab *t)
{
	int			i;
	char			s[2], buf[128];
	const char		*errstr = NULL;
	long long		link;

	/* don't use w directly; use t->whatever instead */

	if (t == NULL)
		errx(1, "webview_keypress_cb");

	DNPRINTF(XT_D_KEY, "webview_keypress_cb: keyval 0x%x mask 0x%x t %p\n",
	    e->keyval, e->state, t);

	if (t->hints_on) {
		/* ESC */
		if (CLEAN(e->state) == 0 && e->keyval == GDK_Escape) {
			disable_hints(t);
			return (XT_CB_HANDLED);
		}

		/* RETURN */
		if (CLEAN(e->state) == 0 && e->keyval == GDK_Return) {
			link = strtonum(t->hint_num, 1, 1000, &errstr);
			if (errstr) {
				/* we have a string */
			} else {
				/* we have a number */
				snprintf(buf, sizeof buf, "vimprobable_fire(%s)",
				    t->hint_num);
				run_script(t, buf);
			}
			disable_hints(t);
		}

		/* BACKSPACE */
		/* XXX unfuck this */
		if (CLEAN(e->state) == 0 && e->keyval == GDK_BackSpace) {
			if (t->hint_mode == XT_HINT_NUMERICAL) {
				/* last input was numerical */
				int		l;
				l = strlen(t->hint_num);
				if (l > 0) {
					l--;
					if (l == 0) {
						disable_hints(t);
						enable_hints(t);
					} else {
						t->hint_num[l] = '\0';
						goto num;
					}
				}
			} else if (t->hint_mode == XT_HINT_ALPHANUM) {
				/* last input was alphanumerical */
				int		l;
				l = strlen(t->hint_buf);
				if (l > 0) {
					l--;
					if (l == 0) {
						disable_hints(t);
						enable_hints(t);
					} else {
						t->hint_buf[l] = '\0';
						goto anum;
					}
				}
			} else {
				/* bogus */
				disable_hints(t);
			}
		}

		/* numerical input */
		if (CLEAN(e->state) == 0 &&
		    ((e->keyval >= GDK_0 && e->keyval <= GDK_9) || (e->keyval >= GDK_KP_0 && e->keyval <= GDK_KP_9))) {
			snprintf(s, sizeof s, "%c", e->keyval);
			strlcat(t->hint_num, s, sizeof t->hint_num);
			DNPRINTF(XT_D_JS, "webview_keypress_cb: numerical %s\n",
			    t->hint_num);
num:
			link = strtonum(t->hint_num, 1, 1000, &errstr);
			if (errstr) {
				DNPRINTF(XT_D_JS, "webview_keypress_cb: invalid link number\n");
				disable_hints(t);
			} else {
				snprintf(buf, sizeof buf, "vimprobable_update_hints(%s)",
				    t->hint_num);
				t->hint_mode = XT_HINT_NUMERICAL;
				run_script(t, buf);
			}

			/* empty the counter buffer */
			bzero(t->hint_buf, sizeof t->hint_buf);
			return (XT_CB_HANDLED);
		}

		/* alphanumerical input */
		if (
		    (CLEAN(e->state) == 0 && e->keyval >= GDK_a && e->keyval <= GDK_z) ||
		    (CLEAN(e->state) == GDK_SHIFT_MASK && e->keyval >= GDK_A && e->keyval <= GDK_Z) ||
		    (CLEAN(e->state) == 0 && ((e->keyval >= GDK_0 && e->keyval <= GDK_9) ||
		    ((e->keyval >= GDK_KP_0 && e->keyval <= GDK_KP_9) && (t->hint_mode != XT_HINT_NUMERICAL))))) {
			snprintf(s, sizeof s, "%c", e->keyval);
			strlcat(t->hint_buf, s, sizeof t->hint_buf);
			DNPRINTF(XT_D_JS, "webview_keypress_cb: alphanumerical %s\n",
			    t->hint_buf);
anum:
			snprintf(buf, sizeof buf, "vimprobable_cleanup()");
			run_script(t, buf);

			snprintf(buf, sizeof buf, "vimprobable_show_hints('%s')",
			    t->hint_buf);
			t->hint_mode = XT_HINT_ALPHANUM;
			run_script(t, buf);

			/* empty the counter buffer */
			bzero(t->hint_num, sizeof t->hint_num);
			return (XT_CB_HANDLED);
		}

		return (XT_CB_HANDLED);
	}

	for (i = 0; i < LENGTH(keys); i++)
		if (e->keyval == keys[i].key && CLEAN(e->state) ==
		    keys[i].mask) {
			keys[i].func(t, &keys[i].arg);
			return (XT_CB_HANDLED);
		}

	return (XT_CB_PASSTHROUGH);
}

int
cmd_keyrelease_cb(GtkEntry *w, GdkEventKey *e, struct tab *t)
{
	const gchar		*c = gtk_entry_get_text(w);
	GdkColor		color;
	int			forward = TRUE;

	DNPRINTF(XT_D_CMD, "cmd_keyrelease_cb: keyval 0x%x mask 0x%x t %p\n",
	    e->keyval, e->state, t);

	if (t == NULL)
		errx(1, "cmd_keyrelease_cb");

	DNPRINTF(XT_D_CMD, "cmd_keyrelease_cb: keyval 0x%x mask 0x%x t %p\n",
	    e->keyval, e->state, t);

	if (c[0] == ':')
		goto done;
	if (strlen(c) == 1)
		goto done;

	if (c[0] == '/')
		forward = TRUE;
	else if (c[0] == '?')
		forward = FALSE;
	else
		goto done;

	/* search */
	if (webkit_web_view_search_text(t->wv, &c[1], FALSE, forward, TRUE) ==
	    FALSE) {
		/* not found, mark red */
		gdk_color_parse("red", &color);
		gtk_widget_modify_base(t->cmd, GTK_STATE_NORMAL, &color);
		/* unmark and remove selection */
		webkit_web_view_unmark_text_matches(t->wv);
		/* my kingdom for a way to unselect text in webview */
	} else {
		/* found, highlight all */
		webkit_web_view_unmark_text_matches(t->wv);
		webkit_web_view_mark_text_matches(t->wv, &c[1], FALSE, 0);
		webkit_web_view_set_highlight_text_matches(t->wv, TRUE);
		gdk_color_parse("white", &color);
		gtk_widget_modify_base(t->cmd, GTK_STATE_NORMAL, &color);
	}
done:
	return (XT_CB_PASSTHROUGH);
}

#if 0
int
cmd_complete(struct tab *t, char *s)
{
	int			i;
	GtkEntry		*w = GTK_ENTRY(t->cmd);

	DNPRINTF(XT_D_CMD, "cmd_keypress_cb: complete %s\n", s);

	for (i = 0; i < LENGTH(cmds); i++) {
		if (!strncasecmp(cmds[i].cmd, s, strlen(s))) {
			fprintf(stderr, "match %s %d\n", cmds[i].cmd, strcasecmp(cmds[i].cmd, s));
#if 0
			gtk_entry_set_text(w, ":");
			gtk_entry_append_text(w, cmds[i].cmd);
			gtk_editable_set_position(GTK_EDITABLE(w), -1);
#endif
		}
	}

	return (0);
}
#endif

int
cmd_keypress_cb(GtkEntry *w, GdkEventKey *e, struct tab *t)
{
	int			rv = XT_CB_HANDLED;
	const gchar		*c = gtk_entry_get_text(w);

	if (t == NULL)
		errx(1, "cmd_keypress_cb");

	DNPRINTF(XT_D_CMD, "cmd_keypress_cb: keyval 0x%x mask 0x%x t %p\n",
	    e->keyval, e->state, t);

	/* sanity */
	if (c == NULL)
		e->keyval = GDK_Escape;
	else if (!(c[0] == ':' || c[0] == '/' || c[0] == '?'))
		e->keyval = GDK_Escape;

	switch (e->keyval) {
#if 0
	case GDK_Tab:
		if (c[0] != ':')
			goto done;

		if (strchr (c, ' ')) {
			/* par completion */
			fprintf(stderr, "completeme par\n");
			goto done;
		}

		cmd_complete(t, (char *)&c[1]);

		goto done;
#endif
	case GDK_BackSpace:
		if (!(!strcmp(c, ":") || !strcmp(c, "/") || !strcmp(c, "?")))
			break;
		/* FALLTHROUGH */
	case GDK_Escape:
		gtk_widget_hide(t->cmd);
		gtk_widget_grab_focus(GTK_WIDGET(t->wv));
		goto done;
	}

	rv = XT_CB_PASSTHROUGH;
done:
	return (rv);
}

int
cmd_focusout_cb(GtkWidget *w, GdkEventFocus *e, struct tab *t)
{
	if (t == NULL)
		errx(1, "cmd_focusout_cb");

	DNPRINTF(XT_D_CMD, "cmd_focusout_cb: tab %d focus_wv %d\n",
	    t->tab_id, t->focus_wv);

	/* abort command when losing focus */
	gtk_widget_hide(t->cmd);
	if (t->focus_wv)
		gtk_widget_grab_focus(GTK_WIDGET(t->wv));
	else
		gtk_widget_grab_focus(GTK_WIDGET(t->uri_entry));

	return (XT_CB_PASSTHROUGH);
}

void
cmd_activate_cb(GtkEntry *entry, struct tab *t)
{
	int			i;
	char			*s;
	const gchar		*c = gtk_entry_get_text(entry);

	if (t == NULL)
		errx(1, "cmd_activate_cb");

	DNPRINTF(XT_D_CMD, "cmd_activate_cb: tab %d %s\n", t->tab_id, c);

	/* sanity */
	if (c == NULL)
		goto done;
	else if (!(c[0] == ':' || c[0] == '/' || c[0] == '?'))
		goto done;
	if (strlen(c) < 2)
		goto done;
	s = (char *)&c[1];

	if (c[0] == '/' || c[0] == '?') {
		if (t->search_text) {
			g_free(t->search_text);
			t->search_text = NULL;
		}

		t->search_text = g_strdup(s);
		t->search_forward = c[0] == '/';

		goto done;
	}

	for (i = 0; i < LENGTH(cmds); i++)
		if (cmds[i].params) {
			if (!strncmp(s, cmds[i].cmd, strlen(cmds[i].cmd))) {
				cmds[i].arg.s = g_strdup(s);
				cmds[i].func(t, &cmds[i].arg);
			}
		} else {
			if (!strcmp(s, cmds[i].cmd))
				cmds[i].func(t, &cmds[i].arg);
		}

done:
	gtk_widget_hide(t->cmd);
}

void
backward_cb(GtkWidget *w, struct tab *t)
{
	if (t == NULL)
		errx(1, "backward_cb");

	DNPRINTF(XT_D_NAV, "backward_cb: tab %d\n", t->tab_id);

	webkit_web_view_go_back(t->wv);
}

void
forward_cb(GtkWidget *w, struct tab *t)
{
	if (t == NULL)
		errx(1, "forward_cb");

	DNPRINTF(XT_D_NAV, "forward_cb: tab %d\n", t->tab_id);

	webkit_web_view_go_forward(t->wv);
}

void
stop_cb(GtkWidget *w, struct tab *t)
{
	WebKitWebFrame		*frame;

	if (t == NULL)
		errx(1, "stop_cb");

	DNPRINTF(XT_D_NAV, "stop_cb: tab %d\n", t->tab_id);

	frame = webkit_web_view_get_main_frame(t->wv);
	if (frame == NULL) {
		warnx("stop_cb: no frame");
		return;
	}

	webkit_web_frame_stop_loading(frame);
}

void
setup_webkit(struct tab *t)
{
	g_object_set((GObject *)t->settings,
	    "user-agent", t->user_agent, (char *)NULL);
	g_object_set((GObject *)t->settings,
	    "enable-scripts", enable_scripts, (char *)NULL);
	g_object_set((GObject *)t->settings,
	    "enable-plugins", enable_plugins, (char *)NULL);
	adjustfont_webkit(t, XT_FONT_SET);
	g_object_set((GObject *)t->settings,
	    "default-font-family", default_font_family, NULL);
	g_object_set((GObject *)t->settings,
	    "serif-font-family", serif_font_family, NULL);
	g_object_set((GObject *)t->settings,
	    "sans-serif-font-family", sans_serif_font_family, NULL);
	g_object_set((GObject *)t->settings,
	    "monospace-font-family", monospace_font_family, NULL);
	g_object_set((GObject *)t->settings,
	    "default-encoding", default_encoding, NULL);
	g_object_set((GObject *)t->settings,
	    "user-stylesheet-uri", user_stylesheet, NULL);

	webkit_web_view_set_settings(t->wv, t->settings);
}

GtkWidget *
create_browser(struct tab *t)
{
	GtkWidget		*w;
	gchar			*strval;

	if (t == NULL)
		errx(1, "create_browser");

	t->sb_h = GTK_SCROLLBAR(gtk_hscrollbar_new(NULL));
	t->sb_v = GTK_SCROLLBAR(gtk_vscrollbar_new(NULL));
	t->adjust_h = gtk_range_get_adjustment(GTK_RANGE(t->sb_h));
	t->adjust_v = gtk_range_get_adjustment(GTK_RANGE(t->sb_v));

	w = gtk_scrolled_window_new(t->adjust_h, t->adjust_v);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(w),
	    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	t->wv = WEBKIT_WEB_VIEW(webkit_web_view_new());
	gtk_container_add(GTK_CONTAINER(w), GTK_WIDGET(t->wv));

	/* set defaults */
	t->settings = webkit_web_settings_new();

	g_object_get((GObject *)t->settings, "user-agent", &strval, (char *)NULL);
	t->user_agent = g_strdup_printf("%s %s+", strval, version);
	g_free(strval);

	setup_webkit(t);

	return (w);
}

GtkWidget *
create_window(void)
{
	GtkWidget		*w;

	w = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size(GTK_WINDOW(w), 1024, 768);
	gtk_widget_set_name(w, "xxxterm");
	gtk_window_set_wmclass(GTK_WINDOW(w), "xxxterm", "XXXTerm");
	g_signal_connect(G_OBJECT(w), "delete_event",
	    G_CALLBACK (gtk_main_quit), NULL);

	return (w);
}

GtkWidget *
create_toolbar(struct tab *t)
{
	GtkWidget		*toolbar = gtk_toolbar_new();
	GtkToolItem		*i;

#if GTK_CHECK_VERSION(2,15,0)
	gtk_orientable_set_orientation(GTK_ORIENTABLE(toolbar),
	    GTK_ORIENTATION_HORIZONTAL);
#else
	gtk_toolbar_set_orientation(GTK_TOOLBAR(toolbar),
	    GTK_ORIENTATION_HORIZONTAL);
#endif
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_BOTH_HORIZ);

	if (fancy_bar) {
		/* backward button */
		t->backward = gtk_tool_button_new_from_stock(GTK_STOCK_GO_BACK);
		gtk_widget_set_sensitive(GTK_WIDGET(t->backward), FALSE);
		g_signal_connect(G_OBJECT(t->backward), "clicked",
		    G_CALLBACK(backward_cb), t);
		gtk_toolbar_insert(GTK_TOOLBAR(toolbar), t->backward, -1);

		/* forward button */
		t->forward =
		    gtk_tool_button_new_from_stock(GTK_STOCK_GO_FORWARD);
		gtk_widget_set_sensitive(GTK_WIDGET(t->forward), FALSE);
		g_signal_connect(G_OBJECT(t->forward), "clicked",
		    G_CALLBACK(forward_cb), t);
		gtk_toolbar_insert(GTK_TOOLBAR(toolbar), t->forward, -1);

		/* stop button */
		t->stop = gtk_tool_button_new_from_stock(GTK_STOCK_STOP);
		gtk_widget_set_sensitive(GTK_WIDGET(t->stop), FALSE);
		g_signal_connect(G_OBJECT(t->stop), "clicked",
		    G_CALLBACK(stop_cb), t);
		gtk_toolbar_insert(GTK_TOOLBAR(toolbar), t->stop, -1);
	}

	/* uri entry */
	i = gtk_tool_item_new();
	gtk_tool_item_set_expand(i, TRUE);
	t->uri_entry = gtk_entry_new();
	gtk_container_add(GTK_CONTAINER(i), t->uri_entry);
	g_signal_connect(G_OBJECT(t->uri_entry), "activate",
	    G_CALLBACK(activate_uri_entry_cb), t);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), i, -1);

	/* search entry */
	if (fancy_bar && search_string) {
		i = gtk_tool_item_new();
		t->search_entry = gtk_entry_new();
		gtk_entry_set_width_chars(GTK_ENTRY(t->search_entry), 30);
		gtk_container_add(GTK_CONTAINER(i), t->search_entry);
		g_signal_connect(G_OBJECT(t->search_entry), "activate",
		    G_CALLBACK(activate_search_entry_cb), t);
		gtk_toolbar_insert(GTK_TOOLBAR(toolbar), i, -1);
	}

	return (toolbar);
}

void
delete_tab(struct tab *t)
{
	DNPRINTF(XT_D_TAB, "delete_tab: %p\n", t);

	if (t == NULL)
		return;

	TAILQ_REMOVE(&tabs, t, entry);
	if (TAILQ_EMPTY(&tabs))
		create_new_tab(NULL, 1);

	gtk_widget_destroy(t->vbox);

	g_free(t->user_agent);
	g_free(t);

	TAILQ_FOREACH(t, &tabs, entry)
		t->tab_id = gtk_notebook_page_num(notebook, t->vbox);
}

void
adjustfont_webkit(struct tab *t, int adjust)
{
	if (t == NULL)
		errx(1, "adjustfont_webkit");

	if (adjust == XT_FONT_SET)
		t->font_size = default_font_size;

	t->font_size += adjust;
	g_object_set((GObject *)t->settings, "default-font-size",
	    t->font_size, (char *)NULL);
	g_object_get((GObject *)t->settings, "default-font-size",
	    &t->font_size, (char *)NULL);
}

void
create_new_tab(char *title, int focus)
{
	struct tab		*t;
	int			load = 1;
	char			*newuri = NULL;
	GtkWidget		*image, *b, *button;

	DNPRINTF(XT_D_TAB, "create_new_tab: title %s focus %d\n", title, focus);

	if (tabless && !TAILQ_EMPTY(&tabs)) {
		DNPRINTF(XT_D_TAB, "create_new_tab: new tab rejected\n");
		return;
	}

	t = g_malloc0(sizeof *t);
	TAILQ_INSERT_TAIL(&tabs, t, entry);

	if (title == NULL) {
		title = "(untitled)";
		load = 0;
	} else {
		if (valid_url_type(title)) {
			newuri = guess_url_type(title);
			title = newuri;
		}
	}

	t->vbox = gtk_vbox_new(FALSE, 0);

	/* label + button for tab */
	b = gtk_hbox_new(FALSE, 0);
	t->spinner = gtk_spinner_new ();
	t->label = gtk_label_new(title);
	image = gtk_image_new_from_stock(GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
	button = gtk_button_new();
	gtk_button_set_image(GTK_BUTTON(button), image);
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	gtk_button_set_focus_on_click(GTK_BUTTON(button), FALSE);
	gtk_widget_set_size_request(t->label, 100, -1);
	gtk_box_pack_start(GTK_BOX(b), t->spinner, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(b), t->label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(b), button, FALSE, FALSE, 0);

	/* toolbar */
	t->toolbar = create_toolbar(t);
	gtk_box_pack_start(GTK_BOX(t->vbox), t->toolbar, FALSE, FALSE, 0);

	/* browser */
	t->browser_win = create_browser(t);
	gtk_box_pack_start(GTK_BOX(t->vbox), t->browser_win, TRUE, TRUE, 0);

	/* command entry */
	t->cmd = gtk_entry_new();
	gtk_entry_set_inner_border(GTK_ENTRY(t->cmd), NULL);
	gtk_entry_set_has_frame(GTK_ENTRY(t->cmd), FALSE);
	gtk_box_pack_end(GTK_BOX(t->vbox), t->cmd, FALSE, FALSE, 0);

	/* and show it all */
	gtk_widget_show_all(b);
	gtk_widget_show_all(t->vbox);
	t->tab_id = gtk_notebook_append_page(notebook, t->vbox, b);

	/* hide spinner for now */
	gtk_spinner_stop(t->spinner);
	gtk_widget_hide(t->spinner);

	/* make notebook tabs reorderable */
	gtk_notebook_set_tab_reorderable(notebook, t->vbox, TRUE);

	g_object_connect((GObject*)t->cmd,
	    "signal::key-press-event", (GCallback)cmd_keypress_cb, t,
	    "signal::key-release-event", (GCallback)cmd_keyrelease_cb, t,
	    "signal::focus-out-event", (GCallback)cmd_focusout_cb, t,
	    "signal::activate", (GCallback)cmd_activate_cb, t,
	    (char *)NULL);

	g_object_connect((GObject*)t->wv,
	    "signal-after::key-press-event", (GCallback)webview_keypress_cb, t,
	    /* "signal::hovering-over-link", (GCallback)webview_hover_cb, t, */
	    "signal::download-requested", (GCallback)webview_download_cb, t,
	    "signal::mime-type-policy-decision-requested", (GCallback)webview_mimetype_cb, t,
	    "signal::navigation-policy-decision-requested", (GCallback)webview_npd_cb, t,
	    "signal::new-window-policy-decision-requested", (GCallback)webview_nw_cb, t,
	    "signal::create-web-view", (GCallback)webview_cwv_cb, t,
	    "signal::event", (GCallback)webview_event_cb, t,
	    "signal::load-finished", (GCallback)webview_load_finished_cb, t,
	    "signal::load-progress-changed", (GCallback)webview_progress_changed_cb, t,
	    (char *)NULL);
	g_signal_connect(t->wv, "notify::load-status",
	    G_CALLBACK(notify_load_status_cb), t);

	/* hijack the unused keys as if we were the browser */
	g_object_connect((GObject*)t->toolbar,
	    "signal-after::key-press-event", (GCallback)webview_keypress_cb, t,
	    (char *)NULL);

	g_signal_connect(G_OBJECT(t->uri_entry), "focus",
	    G_CALLBACK(focus_uri_entry_cb), t);

	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(tab_close_cb), t);

	/* hide stuff */
	gtk_widget_hide(t->cmd);
	if (showurl == 0)
		gtk_widget_hide(t->toolbar);

	if (focus) {
		gtk_notebook_set_current_page(notebook, t->tab_id);
		DNPRINTF(XT_D_TAB, "create_new_tab: going to tab: %d\n",
		    t->tab_id);
	}

	if (load)
		webkit_web_view_load_uri(t->wv, title);
	else
		gtk_widget_grab_focus(GTK_WIDGET(t->uri_entry));

	if (newuri)
		g_free(newuri);
}

void
notebook_switchpage_cb(GtkNotebook *nb, GtkNotebookPage *nbp, guint pn,
    gpointer *udata)
{
	struct tab		*t;
	const gchar		*uri;

	DNPRINTF(XT_D_TAB, "notebook_switchpage_cb: tab: %d\n", pn);

	TAILQ_FOREACH(t, &tabs, entry) {
		if (t->tab_id == pn) {
			DNPRINTF(XT_D_TAB, "notebook_switchpage_cb: going to "
			    "%d\n", pn);

			uri = webkit_web_view_get_title(t->wv);
			if (uri == NULL)
				uri = XT_NAME;
			gtk_window_set_title(GTK_WINDOW(main_window), uri);

			gtk_widget_hide(t->cmd);
		}
	}
}

void
create_canvas(void)
{
	GtkWidget		*vbox;

	vbox = gtk_vbox_new(FALSE, 0);
	notebook = GTK_NOTEBOOK(gtk_notebook_new());
	if (showtabs == 0)
		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook), FALSE);
	gtk_notebook_set_scrollable(notebook, TRUE);

	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(notebook), TRUE, TRUE, 0);

	g_object_connect((GObject*)notebook,
	    "signal::switch-page", (GCallback)notebook_switchpage_cb, NULL,
	    (char *)NULL);

	main_window = create_window();
	gtk_container_add(GTK_CONTAINER(main_window), vbox);
	gtk_window_set_title(GTK_WINDOW(main_window), XT_NAME);
	gtk_widget_show_all(main_window);
}

void
setup_cookies(char *file)
{
	if (cookiejar) {
		soup_session_remove_feature(session,
		    (SoupSessionFeature*)cookiejar);
		g_object_unref(cookiejar);
		cookiejar = NULL;
	}

	if (cookies_enabled == 0)
		return;

	cookiejar = soup_cookie_jar_text_new(file, read_only_cookies);
	soup_session_add_feature(session, (SoupSessionFeature*)cookiejar);
}

void
setup_proxy(char *uri)
{
	if (proxy_uri) {
		g_object_set(session, "proxy_uri", NULL, (char *)NULL);
		soup_uri_free(proxy_uri);
		proxy_uri = NULL;
	}
	if (http_proxy) {
		if (http_proxy != uri) {
			g_free(http_proxy);
			http_proxy = NULL;
		}
	}

	if (uri) {
		http_proxy = g_strdup(uri);
		DNPRINTF(XT_D_CONFIG, "setup_proxy: %s\n", uri);
		proxy_uri = soup_uri_new(http_proxy);
		g_object_set(session, "proxy-uri", proxy_uri, (char *)NULL);
	}
}

void
usage(void)
{
	fprintf(stderr,
	    "%s [-STVt][-f file] url ...\n", __progname);
	exit(0);
}

int
main(int argc, char *argv[])
{
	struct stat		sb;
	int			c, focus = 1;
	char			conf[PATH_MAX] = { '\0' };
	char			file[PATH_MAX];
	char			*env_proxy = NULL;
	FILE			*f = NULL;

	while ((c = getopt(argc, argv, "STVf:t")) != -1) {
		switch (c) {
		case 'S':
			showurl = 0;
			break;
		case 'T':
			showtabs = 0;
			break;
		case 'V':
			errx(0 , "Version: %s", version);
			break;
		case 'f':
			strlcpy(conf, optarg, sizeof(conf));
			break;
		case 't':
			tabless = 1;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	TAILQ_INIT(&tabs);

	/* prepare gtk */
	gtk_init(&argc, &argv);
	if (!g_thread_supported())
		g_thread_init(NULL);

	pwd = getpwuid(getuid());
	if (pwd == NULL)
		errx(1, "invalid user %d", getuid());

	/* set download dir */
	strlcpy(download_dir, pwd->pw_dir, sizeof download_dir);

	/* read config file */
	if (strlen(conf) == 0)
		snprintf(conf, sizeof conf, "%s/.%s",
		    pwd->pw_dir, XT_CONF_FILE);
	config_parse(conf);

	/* download dir */
	if (!strcmp(download_dir, pwd->pw_dir))
		strlcat(download_dir, "/downloads", sizeof download_dir);

	if (stat(download_dir, &sb)) {
		if (mkdir(download_dir, S_IRWXU) == -1)
			err(1, "mkdir download_dir");
	}
	if (S_ISDIR(sb.st_mode) == 0)
		errx(1, "%s not a dir", download_dir);
	if (((sb.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO))) != S_IRWXU) {
		warnx("fixing invalid permissions on %s", download_dir);
		if (chmod(download_dir, S_IRWXU) == -1)
			err(1, "chmod");
	}

	/* working directory */
	snprintf(work_dir, sizeof work_dir, "%s/%s", pwd->pw_dir, XT_DIR);
	if (stat(work_dir, &sb)) {
		if (mkdir(work_dir, S_IRWXU) == -1)
			err(1, "mkdir work_dir");
	}
	if (S_ISDIR(sb.st_mode) == 0)
		errx(1, "%s not a dir", work_dir);
	if (((sb.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO))) != S_IRWXU) {
		warnx("fixing invalid permissions on %s", work_dir);
		if (chmod(work_dir, S_IRWXU) == -1)
			err(1, "chmod");
	}

	/* favorites file */
	snprintf(file, sizeof file, "%s/%s", work_dir, XT_FAVS_FILE);
	if (stat(file, &sb)) {
		warnx("favorites file doesn't exist, creating it");
		if ((f = fopen(file, "w")) == NULL)
			err(1, "favorites");
		fclose(f);
	}

	/* cookies */
	session = webkit_get_default_session();
	snprintf(file, sizeof file, "%s/cookies.txt", work_dir);
	setup_cookies(file);

	/* proxy */
	env_proxy = getenv("http_proxy");
	if (env_proxy)
		setup_proxy(env_proxy);
	else
		setup_proxy(http_proxy);

	create_canvas();

	while (argc) {
		create_new_tab(argv[0], focus);
		focus = 0;

		argc--;
		argv++;
	}
	if (focus == 1)
		create_new_tab(home, 1);

	gtk_main();

	return (0);
}
