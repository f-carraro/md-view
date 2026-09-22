/* mdv - minimal markdown viewer for Windows.
   Native GUI window: scrollable, renders tables, clickable minimap.
   Freestanding: no CRT, kernel32 + user32 + gdi32 only. */

#define A __attribute__((dllimport)) __stdcall
#define S __stdcall
typedef void *H;
typedef unsigned U;
typedef unsigned long long Q;
typedef unsigned short C;   /* UTF-16 code unit */

/* ---- kernel32 ---- */
H  A GetModuleHandleW(const C *);
C *A GetCommandLineW(void);
H  A CreateFileW(const C *, U, U, void *, U, U, H);
U  A GetFileSize(H, U *);
int A ReadFile(H, void *, U, U *, void *);
int A WriteFile(H, const void *, U, U *, void *);
void *A VirtualAlloc(void *, Q, U, U);
int A VirtualFree(void *, Q, U);
int A CloseHandle(H);
void A ExitProcess(U);
int A MultiByteToWideChar(U, U, const char *, int, C *, int);
U  A GetEnvironmentVariableW(const C *, C *, U);
int A WideCharToMultiByte(U, U, const C *, int, char *, int, const char *, int *);

/* ---- user32 ---- */
typedef Q (S *WP)(H, U, Q, Q);
typedef struct { U style; WP proc; int ce, we; H inst, icon, cur, bg; const C *menu, *cls; } WCL;
typedef struct { H h; U m, p0; Q w, l; U t; int x, y, p1; } MSG;
typedef struct { H dc; int erase, l, t, r, b, restore, inc; char rsv[32]; } PS;
typedef struct { U cb, mask; int mn, mx; U page; int pos, track; } SI;
typedef struct { int l, t, r, b; } RC;
typedef struct { int cx, cy; } SZ;
C  A RegisterClassW(const WCL *);
H  A CreateWindowExW(U, const C *, const C *, U, int, int, int, int, H, H, H, void *);
int A ShowWindow(H, int);
int A UpdateWindow(H);
int A GetMessageW(MSG *, H, U, U);
int A TranslateMessage(const MSG *);
Q  A DispatchMessageW(const MSG *);
Q  A DefWindowProcW(H, U, Q, Q);
void A PostQuitMessage(int);
H  A BeginPaint(H, PS *);
int A EndPaint(H, const PS *);
int A InvalidateRect(H, const RC *, int);
H  A LoadCursorW(H, const C *);
H  A LoadIconW(H, const C *);
int A SetScrollInfo(H, int, const SI *, int);
int A GetScrollInfo(H, int, SI *);
H  A GetDC(H);
int A ReleaseDC(H, H);
int A FillRect(H, const RC *, H);
H  A SetCapture(H);
int A ReleaseCapture(void);
int A SetWindowTextW(H, const C *);
int A GetWindowRect(H, RC *);
int A DestroyWindow(H);
H  A SetFocus(H);
Q  A SendMessageW(H, U, Q, Q);
int A MoveWindow(H, int, int, int, int, int);
int A MessageBoxW(H, const C *, const C *, U);

/* ---- shell32 ---- */
void A DragAcceptFiles(H, int);
U  A DragQueryFileW(H, U, C *, U);
void A DragFinish(H);

/* ---- gdi32 ---- */
typedef struct { int h, a, d, il, el, acw, mcw, w, ov, ax, ay; C fc, lc, dc, bc;
                 unsigned char it, un, st, pf, cs; } TM;
H  A CreateFontW(int, int, int, int, int, U, U, U, U, U, U, U, U, const C *);
H  A SelectObject(H, H);
int A DeleteObject(H);
U  A SetTextColor(H, U);
U  A SetBkColor(H, U);
int A TextOutW(H, int, int, const C *, int);
H  A CreateSolidBrush(U);
int A GetTextMetricsW(H, TM *);
int A GetTextExtentPoint32W(H, const C *, int, SZ *);
H  A CreateCompatibleDC(H);
H  A CreateCompatibleBitmap(H, int, int);
int A BitBlt(H, int, int, int, int, H, int, int, U);
int A SetBkMode(H, int);

/* LOGFONTW layout, for enumerating installed faces */
typedef struct { int h, w, esc, orient, weight;
                 unsigned char it, un, so, cs, op, cp, q, pf;
                 C face[32]; } LF;
typedef int (S *FEP)(const LF *, const TM *, U, Q);
int A EnumFontFamiliesExW(H, LF *, FEP, Q, U);

/* ---------------- state ---------------- */
typedef struct { int row, col, len, st; C *t; int src; } Run;   /* src: offset of its source line */
typedef struct { C *t; unsigned char *s; int n, src; } Cell;

static C *tx; static int ntx;                 /* document, UTF-16 */
static Run *runs; static int nrun, maxrun, cursrc;
static int *rowidx;
static C *lb; static unsigned char *ls; static int ln;   /* logical-line buffer */
static char *ar; static Q arp, arsz;          /* layout arena */
static int cols, row, nrow, top, vis;
static int cw, chh;
static H hwnd, fnt[3], memdc, membm, mmdc, mmbm;
static H brs[9], bmm, bvp, bbg;
static int mw, mh, mmw, dragging;

#define MMW 112

/* colours are written as 0xRRGGBB and swapped into GDI's 0x00BBGGRR */
#define X(v) ((U)((((v) & 0xffu) << 16) | ((v) & 0xff00u) | (((v) >> 16) & 0xffu)))

/* pal slots: 0 body  1 bold  2 h1  3 h2  4 h3+  5 code  6 link/bullet
              7 dim (rules, quotes, table frame)  8 italic */
typedef struct { U pal[9], bg, mmbg, vp; const C *name; } Theme;

static const Theme themes[] = {
	/* -------- light -------- */
	{ { X(0x424a53), X(0x1f2328), X(0x0a3069), X(0xbf8700), X(0x8250df),
	    X(0x0550ae), X(0x116329), X(0x8c959f), X(0xcf222e) },
	  X(0xffffff), X(0xf4f4f5), X(0xafb8c1), L"github" },

	{ { X(0x5e6687), X(0x202746), X(0x9b8833), X(0xad7d2b), X(0x3d8fd1),
	    X(0xc76b29), X(0x1f92b5), X(0x898ea4), X(0x6679cc) },
	  X(0xf5f7ff), X(0xeceef8), X(0x979db4), L"atelier sulphurpool light" },

	{ { X(0x6d828e), X(0x485867), X(0x6e916d), X(0x7c8262), X(0x797ea1),
	    X(0x8e7a6a), X(0x628282), X(0x6f828c), X(0xa179a0) },
	  X(0xe3efef), X(0xdce8e9), X(0xb0c5c8), L"brush trees" },

	{ { X(0x000000), X(0x000000), X(0x730073), X(0x755b00), X(0x007300),
	    X(0x006565), X(0x755b00), X(0x707070), X(0x000090) },
	  X(0xe0e0e0), X(0xd3d3d3), X(0xd0d0d0), L"dirtysea" },

	{ { X(0x464646), X(0x101010), X(0x8e8e8e), X(0x828282), X(0x686868),
	    X(0x8a8a8a), X(0x868686), X(0x898989), X(0x747474) },
	  X(0xf7f7f7), X(0xececec), X(0xb9b9b9), L"grayscale light" },

	/* -------- dark -------- */
	{ { X(0xc0c5ce), X(0xd8dee9), X(0x99c794), X(0xfac863), X(0x6699cc),
	    X(0xf99157), X(0x5fb3b3), X(0x6e7b86), X(0xc594c5) },
	  X(0x1b2b34), X(0x25343d), X(0x4f5b66), L"oceanicnext" },

	{ { X(0xb7b7b7), X(0xd5d5d5), X(0x5f8787), X(0xffc799), X(0x8eaaaa),
	    X(0xdab083), X(0x60a592), X(0x646464), X(0xd69094) },
	  X(0x101010), X(0x1a1a1a), X(0x222222), L"vesper" },

	{ { X(0xd5c7a1), X(0xfbf1c7), X(0xb8bb26), X(0xffcc1b), X(0x458588),
	    X(0xfe8019), X(0xa4638c), X(0x928374), X(0xfabd2f) },
	  X(0x282828), X(0x32322f), X(0x504945), L"ascendancy" },

	{ { X(0xcdc8d0), X(0xe1dee3), X(0x4b8686), X(0xedb392), X(0xd27f91),
	    X(0xe99d90), X(0xe99d90), X(0x6f6373), X(0xc285b2) },
	  X(0x1d1920), X(0x28242b), X(0x2e2533), L"corduroy" },

	{ { X(0xd3c6aa), X(0xfffbef), X(0xa7c080), X(0xdbbc7f), X(0x7fbbb3),
	    X(0xe69875), X(0x83c092), X(0x859289), X(0xd699b6) },
	  X(0x272e33), X(0x31373a), X(0x414b50), L"everforest dark hard" },

	{ { X(0xb9b9b9), X(0xf7f7f7), X(0x8e8e8e), X(0xa0a0a0), X(0x686868),
	    X(0x999999), X(0x868686), X(0x656565), X(0x747474) },
	  X(0x101010), X(0x1a1a1a), X(0x464646), L"grayscale dark" },

	{ { X(0xb9b5b8), X(0xffffff), X(0x8fc13e), X(0xfdcc59), X(0x1290bf),
	    X(0xfd8b19), X(0x149b93), X(0x797379), X(0xc85e7c) },
	  X(0x322931), X(0x3a3139), X(0x5c545b), L"hopscotch" },

	{ { X(0xcbced0), X(0xe3e6ee), X(0xefaf8e), X(0xefb993), X(0xdf5273),
	    X(0xe58d7d), X(0x24a8b4), X(0x6f6f70), X(0xb072d1) },
	  X(0x1c1e26), X(0x262930), X(0x2e303e), L"horizon dark" },

	{ { X(0xe4dee9), X(0xffffff), X(0xb1f2a7), X(0xebde76), X(0x95a6f4),
	    X(0xf4b870), X(0xb3f4f3), X(0x6e6780), X(0xff79c6) },
	  X(0x211e2a), X(0x2d2a35), X(0x3f3951), L"soft server" },
};
#define NTHEME ((int)(sizeof themes / sizeof themes[0]))

/* fixed-pitch faces discovered on this machine, via EnumFontFamiliesExW */
#define MAXFONT 256
static C fontnames[MAXFONT][32];
static int nfontnames;
static C curfont[32];
static int fontsize;

/* font-picker popup state */
static H pkhwnd, ginst;
static int pksel, pktop, pkrows, pkw;

static int theme;
static const U *pal;
static const unsigned char sfont[9] = { 0, 1, 1, 1, 1, 0, 0, 0, 2 };

void *memset(void *d, int c, Q n) { char *p = d; while (n--) *p++ = (char)c; return d; }
void *memcpy(void *d, const void *s, Q n) { char *p = d; const char *q = s; while (n--) *p++ = *q++; return d; }

/* ---------------- document loading ---------------- */
static C tpath[600];                         /* document path, shown in the caption */
static char *doc;                            /* one block holds every document-sized buffer */
static int bom, crlf;                        /* remembered so a save keeps them */

/* reads path as UTF-8 and (re)builds the buffers, all sized from the file
   length n, which bounds the UTF-16 length:
   ar | runs | rowidx | tx | lb | ls | raw */
static int loadfile(const C *path)
{
	H f;
	U rd = 0, n;
	char *raw;

	f = CreateFileW(path, 0x80000000u, 1, 0, 3, 0, 0);
	if (f == (H)-1) return 0;

	if (doc) VirtualFree(doc, 0, 0x8000);
	ntx = 0; nrun = 0; nrow = 0; top = 0;

	n = GetFileSize(f, 0);
	maxrun = (int)n + 16384;
	arsz = (Q)n * 96 + (1 << 21);
	doc = VirtualAlloc(0, arsz + (Q)maxrun * (sizeof(Run) + 8) + (Q)n * 6 + 8192, 0x3000, 4);
	if (doc) {
		ar = doc;
		runs = (Run *)(ar + arsz);
		rowidx = (int *)(runs + maxrun);      /* rows <= runs + newlines < 2 * maxrun */
		tx = (C *)(rowidx + maxrun * 2 + 16);
		lb = tx + n + 2048;
		ls = (unsigned char *)(lb + n + 1024);
		raw = (char *)ls + n + 1024;
		ReadFile(f, raw, n, &rd, 0);
		ntx = rd ? MultiByteToWideChar(65001, 0, raw, (int)rd, tx, (int)rd + 8) : 0;
		bom = ntx > 0 && tx[0] == 0xFEFF;
		tx += bom; ntx -= bom;
	}
	CloseHandle(f);
	if (!doc) tpath[0] = 0;             /* old text is gone: never save it over this path */
	return doc != 0;
}

static void *aa(Q n)
{
	void *p;
	n = (n + 7) & ~7ull;
	if (arp + n > arsz) return ar;      /* clamp instead of faulting */
	p = ar + arp; arp += n; return p;
}

static void put(int col, int len, int st, C *t)
{
	Run *r;
	if (len <= 0 || nrun >= maxrun) return;
	r = runs + nrun++;
	r->row = row; r->col = col; r->len = len; r->st = st; r->t = t; r->src = cursrc;
}

/* ---------------- inline markup -> lb/ls ---------------- */
static void ap(C c, int st) { if (ln < ntx + 512) { lb[ln] = c; ls[ln] = (unsigned char)st; ln++; } }

static void inl(const C *p, const C *e, int base)
{
	const C *q, *r;
	C c;
	while (p < e) {
		c = *p;
		if (c == 92 && p + 1 < e) { ap(p[1], base); p += 2; continue; }
		if (c == '`') {
			for (q = p + 1; q < e && *q != '`'; q++);
			if (q < e) { while (++p < q) ap(*p, 5); p = q + 1; continue; }
		}
		if (c == '*' && p + 1 < e && p[1] == '*') {
			for (q = p + 2; q + 1 < e && !(*q == '*' && q[1] == '*'); q++);
			if (q + 1 < e) { inl(p + 2, q, base ? base : 1); p = q + 2; continue; }
		}
		if (c == '*') {
			for (q = p + 1; q < e && *q != c; q++);
			if (q < e && q > p + 1) { inl(p + 1, q, base ? base : 8); p = q + 1; continue; }
		}
		if (c == '[') {
			for (q = p + 1; q < e && *q != ']'; q++);
			if (q + 1 < e && q[1] == '(') {
				for (r = q + 2; r < e && *r != ')'; r++);
				if (r < e) { inl(p + 1, q, 6); p = r + 1; continue; }
			}
		}
		ap(c, base); p++;
	}
}

/* ---------------- wrapping / emit ---------------- */
static void span(int col, int from, int to)
{
	int i = from, j, st, k;
	C *t;
	while (i < to) {
		st = ls[i]; j = i;
		while (j < to && ls[j] == st) j++;
		t = aa((Q)(j - i) * 2);
		for (k = 0; k < j - i; k++) t[k] = lb[i + k];
		put(col + (i - from), j - i, st, t);
		i = j;
	}
}

static void flush(int ind)
{
	int w = cols - ind, i = 0, rem, take, b;
	if (w < 4) w = 4;
	if (ln == 0) { row++; return; }
	while (i < ln) {
		rem = ln - i;
		if (rem <= w) take = rem;
		else {
			b = i + w;
			while (b > i && lb[b] != ' ') b--;
			take = b > i ? b - i : w;
		}
		span(ind, i, i + take);
		row++;
		i += take;
		while (i < ln && lb[i] == ' ') i++;
	}
	ln = 0;
}

static void rule(void)
{
	int i, n = cols - 2;
	C *t;
	if (n < 1) n = 1;
	t = aa((Q)n * 2);
	for (i = 0; i < n; i++) t[i] = 0x2500;
	put(0, n, 7, t);
	row++;
}

/* ---------------- tables ---------------- */
static int issep(const C *l, const C *e)
{
	int dash = 0;
	const C *p;
	for (p = l; p < e; p++) {
		if (*p == '-') dash = 1;
		else if (*p != '|' && *p != ':' && *p != ' ' && *p != 9) return 0;
	}
	return dash;
}

static int cells(const C *l, const C *e, const C **st, const C **en)
{
	int n = 0;
	const C *p = l, *s, *q;
	while (p < e && (*p == ' ' || *p == 9)) p++;
	if (p < e && *p == '|') p++;
	while (p <= e && n < 64) {
		s = p;
		while (p < e && *p != '|') { if (*p == 92 && p + 1 < e) p++; p++; }
		if (s == p && p >= e) break;
		while (s < p && *s == ' ') s++;
		q = p;
		while (q > s && q[-1] == ' ') q--;
		st[n] = s; en[n] = q; n++;
		if (p >= e) break;
		p++;
		if (p >= e) break;
	}
	return n;
}

static int wrapcell(Cell *c, int w, int *seg)
{
	int i = 0, k = 0, rem, take, b;
	if (c->n == 0) { seg[0] = 0; seg[1] = 0; return 1; }
	while (i < c->n && k < 255) {
		rem = c->n - i;
		if (rem <= w) take = rem;
		else {
			b = i + w;
			while (b > i && c->t[b] != ' ') b--;
			take = b > i ? b - i : w;
		}
		seg[k * 2] = i; seg[k * 2 + 1] = i + take; k++;
		i += take;
		while (i < c->n && c->t[i] == ' ') i++;
	}
	return k;
}

static void bar(int nc, int *wd, int lft, int mid, int rgt)
{
	int i, j, x = 0, tot = 1;
	C *t;
	for (i = 0; i < nc; i++) tot += wd[i] + 1;
	t = aa((Q)tot * 2 + 8);
	t[x++] = (C)lft;
	for (i = 0; i < nc; i++) {
		for (j = 0; j < wd[i]; j++) t[x++] = 0x2500;
		t[x++] = (C)(i == nc - 1 ? rgt : mid);
	}
	put(0, x, 7, t);
	row++;
}

/* advances past one line; *le receives its end, CR stripped */
static const C *nextline(const C *p, const C *end, const C **le)
{
	const C *e = p;
	while (e < end && *e != '\n') e++;
	*le = (e > p && e[-1] == '\r') ? e - 1 : e;
	return e < end ? e + 1 : end;
}

/* bs..be spans the consecutive '|' lines forming one table */
static void table(const C *bs, const C *be)
{
	int nc = 0, i, j, k, m, r, nr = 0, tot;
	int *wd, *al, *xs, *nseg, *segs;
	Cell *cl;
	const C *cs[64], *ce[64];
	const C *p, *l, *le;

	p = bs;
	while (p < be) {
		l = p; p = nextline(p, be, &le);
		while (l < le && (*l == ' ' || *l == 9)) l++;
		if (l >= le) continue;
		k = cells(l, le, cs, ce);
		if (k > nc) nc = k;
		if (!issep(l, le)) nr++;
	}
	if (nr == 0 || nc == 0) return;

	cl = aa(sizeof(Cell) * (Q)nr * nc);
	wd = aa(sizeof(int) * (Q)nc);
	al = aa(sizeof(int) * (Q)nc);
	xs = aa(sizeof(int) * (Q)(nc + 1));
	nseg = aa(sizeof(int) * (Q)nc);
	for (i = 0; i < nc; i++) { wd[i] = 1; al[i] = 0; }
	for (i = 0; i < nr * nc; i++) { cl[i].t = 0; cl[i].s = 0; cl[i].n = 0; }

	r = 0;
	p = bs;
	while (p < be) {
		l = p; p = nextline(p, be, &le);
		while (l < le && (*l == ' ' || *l == 9)) l++;
		if (l >= le) continue;
		k = cells(l, le, cs, ce);
		if (issep(l, le)) {
			for (j = 0; j < k && j < nc; j++) {
				int lc = cs[j] < ce[j] && cs[j][0] == ':';
				int rc = cs[j] < ce[j] && ce[j][-1] == ':';
				al[j] = (lc && rc) ? 2 : rc ? 1 : 0;
			}
			continue;
		}
		cl[r * nc].src = (int)(l - tx);         /* each row maps to its own line */
		for (j = 0; j < k && j < nc; j++) {
			Cell *c = cl + r * nc + j;
			ln = 0;
			inl(cs[j], ce[j], r == 0 ? 1 : 0);
			c->n = ln;
			c->t = aa((Q)ln * 2 + 2);
			c->s = aa((Q)ln + 1);
			for (m = 0; m < ln; m++) { c->t[m] = lb[m]; c->s[m] = ls[m]; }
			if (c->n > wd[j]) wd[j] = c->n;
		}
		r++;
	}
	ln = 0;

	tot = 1;
	for (i = 0; i < nc; i++) tot += wd[i] + 1;
	while (tot > cols) {
		int bi = 0;
		for (i = 1; i < nc; i++) if (wd[i] > wd[bi]) bi = i;
		if (wd[bi] <= 4) break;
		wd[bi]--; tot--;
	}
	xs[0] = 0;
	for (i = 0; i < nc; i++) xs[i + 1] = xs[i] + wd[i] + 1;

	bar(nc, wd, 0x250C, 0x252C, 0x2510);
	for (r = 0; r < nr; r++) {
		int hmax = 1;
		cursrc = cl[r * nc].src;
		segs = aa(sizeof(int) * 512 * (Q)nc);
		for (j = 0; j < nc; j++) {
			nseg[j] = wrapcell(cl + r * nc + j, wd[j], segs + j * 512);
			if (nseg[j] > hmax) hmax = nseg[j];
		}
		for (k = 0; k < hmax; k++) {
			C *v = aa(4); v[0] = 0x2502;
			for (j = 0; j <= nc; j++) put(xs[j], 1, 7, v);
			for (j = 0; j < nc; j++) {
				Cell *c = cl + r * nc + j;
				int *sg = segs + j * 512;
				int f, t2, len, off, x2, i2, stt, e2;
				if (k >= nseg[j]) continue;
				f = sg[k * 2]; t2 = sg[k * 2 + 1];
				len = t2 - f;
				if (len > wd[j]) len = wd[j];
				off = al[j] == 1 ? wd[j] - len : al[j] == 2 ? (wd[j] - len) / 2 : 0;
				x2 = xs[j] + 1 + off;
				i2 = f;
				while (i2 < f + len) {
					C *t3;
					stt = c->s[i2]; e2 = i2;
					while (e2 < f + len && c->s[e2] == stt) e2++;
					t3 = aa((Q)(e2 - i2) * 2);
					for (m = 0; m < e2 - i2; m++) t3[m] = c->t[i2 + m];
					put(x2 + (i2 - f), e2 - i2, stt, t3);
					i2 = e2;
				}
			}
			row++;
		}
		if (r < nr - 1) bar(nc, wd, 0x251C, 0x253C, 0x2524);
	}
	bar(nc, wd, 0x2514, 0x2534, 0x2518);
}

/* ---------------- block layout ---------------- */

static void layout(int ncols)
{
	const C *p = tx, *end = tx + ntx, *l, *e, *le;
	int fence = 0, h, i, n;

	cols = ncols < 12 ? 12 : ncols;
	arp = 0; nrun = 0; row = 0; ln = 0; nrow = 0;
	if (!doc) return;                          /* no document loaded */

	while (p < end) {
		l = p;
		cursrc = (int)(l - tx);
		for (e = l; e < end && *e != '\n'; e++);
		le = (e > l && e[-1] == '\r') ? e - 1 : e;
		p = e < end ? e + 1 : end;

		if (le - l >= 3 && l[0] == '`' && l[1] == '`' && l[2] == '`') { fence = !fence; row++; continue; }
		if (fence) {
			ln = 0;
			for (i = 0; l + i < le; i++) ap(l[i], 5);
			flush(2);
			continue;
		}

		for (i = 0; l + i < le && (l[i] == ' ' || l[i] == 9); i++);
		n = i;

		if (l + n < le && l[n] == '|') {          /* table block */
			const C *bs = l + n, *be = le;
			while (p < end) {
				const C *l2 = p, *le2, *nx;
				int n2;
				nx = nextline(l2, end, &le2);
				for (n2 = 0; l2 + n2 < le2 && (l2[n2] == ' ' || l2[n2] == 9); n2++);
				if (!(l2 + n2 < le2 && l2[n2] == '|')) break;
				be = le2;
				p = nx;
			}
			table(bs, be);
			continue;
		}

		for (h = 0; l + h < le && l[h] == '#'; h++);
		if (h && h < 7 && l + h < le && l[h] == ' ') {
			int st = h == 1 ? 2 : h == 2 ? 3 : 4;
			row++;
			ln = 0;
			inl(l + h + 1, le, st);
			flush(0);
			continue;
		}
		if (le - l >= 3 && (*l == '-' || *l == '*' || *l == '_')) {
			for (i = 0; l + i < le && l[i] == *l; i++);
			if (l + i == le) { rule(); continue; }
		}
		if (l + n < le && l[n] == '>') {
			C *v = aa(4);
			v[0] = 0x2502;
			ln = 0;
			i = n + 1;
			if (l + i < le && l[i] == ' ') i++;
			inl(l + i, le, 7);
			put(0, 1, 7, v);
			flush(2);
			continue;
		}
		if (l + n + 1 < le && (l[n] == '-' || l[n] == '*' || l[n] == '+') && l[n + 1] == ' ') {
			C *v = aa(4);
			v[0] = 0x2022;
			put(n, 1, 6, v);
			ln = 0;
			inl(l + n + 2, le, 0);
			flush(n + 2);
			continue;
		}
		for (i = n; l + i < le && l[i] >= '0' && l[i] <= '9'; i++);
		if (i > n && l + i + 1 < le && (l[i] == '.' || l[i] == ')') && l[i + 1] == ' ') {
			int k, m = i + 1 - n;
			C *v = aa((Q)m * 2);
			for (k = 0; k < m; k++) v[k] = l[n + k];
			put(n, m, 6, v);
			ln = 0;
			inl(l + i + 2, le, 0);
			flush(n + m + 1);
			continue;
		}
		ln = 0;
		inl(l, le, 0);
		flush(0);
	}
	nrow = row;

	for (i = 0; i <= nrow; i++) rowidx[i] = nrun;
	for (i = nrun - 1; i >= 0; i--) if (runs[i].row <= nrow) rowidx[runs[i].row] = i;
	for (i = nrow - 1; i >= 0; i--) if (rowidx[i] > rowidx[i + 1]) rowidx[i] = rowidx[i + 1];
}

/* ---------------- minimap ---------------- */
static void drawmm(void)
{
	RC r;
	int i, ph;
	if (mmw <= 0) return;              /* mmw > 0 only once WM_SIZE made mmdc */
	r.l = 0; r.t = 0; r.r = mmw; r.b = mh;
	FillRect(mmdc, &r, bmm);
	if (nrow <= 0 || nrun <= 0) return;
	ph = mh / nrow;
	if (ph < 1) ph = 1;
	if (ph > 3) ph = 3;
	for (i = 0; i < nrun; i++) {
		Run *rn = runs + i;
		int y = (int)((Q)rn->row * (Q)mh / (Q)nrow);
		int x1 = 2 + rn->col * (mmw - 4) / cols;
		int x2 = 2 + (rn->col + rn->len) * (mmw - 4) / cols;
		if (x2 <= x1) x2 = x1 + 1;
		if (x2 > mmw - 2) x2 = mmw - 2;
		r.l = x1; r.r = x2; r.t = y; r.b = y + ph;
		FillRect(mmdc, &r, brs[rn->st]);
	}
}

/* ---------------- themes ---------------- */

static void cpy(C *d, const C *s, int max)
{
	int i = 0;
	while (s[i] && i < max - 1) { d[i] = s[i]; i++; }
	d[i] = 0;
}

static int wlen(const C *s) { int i = 0; while (s[i]) i++; return i; }

static int weq(const C *a, const C *b)
{
	int i = 0;
	while (a[i] && a[i] == b[i]) i++;
	return a[i] == b[i];
}

static int wless(const C *a, const C *b)
{
	int i = 0;
	while (a[i] && a[i] == b[i]) i++;
	return a[i] < b[i];
}

/* ---------------- persisted settings (theme + window rect) ---------------- */
static C cfgpath[640];

static void buildcfgpath(void)
{
	U n = GetEnvironmentVariableW(L"APPDATA", cfgpath, 600);
	if (n == 0 || n > 550) { cfgpath[0] = 0; return; }
	cpy(cfgpath + n, L"\\mdv.cfg", 40);
}

typedef struct { int theme, x, y, cx, cy, fontsize; C font[32]; } Cfg;

/* returns 1 and fills *th/*x/*y/*cx/*cy/curfont/fontsize on success */
static int loadcfg(int *th, int *x, int *y, int *cx, int *cy)
{
	H f;
	U rd = 0;
	Cfg cf;
	if (!cfgpath[0]) return 0;
	f = CreateFileW(cfgpath, 0x80000000u, 1, 0, 3, 0, 0);
	if (f == (H)-1) return 0;
	ReadFile(f, &cf, sizeof cf, &rd, 0);
	CloseHandle(f);
	if (rd != sizeof cf) return 0;
	if (cf.cx < 200 || cf.cy < 150 || cf.cx > 10000 || cf.cy > 10000) return 0;
	*th = cf.theme; *x = cf.x; *y = cf.y; *cx = cf.cx; *cy = cf.cy;
	cf.font[31] = 0;
	cpy(curfont, cf.font, 32);
	if (cf.fontsize >= 6 && cf.fontsize <= 72) fontsize = cf.fontsize;
	return 1;
}

static void savecfg(void)
{
	H f;
	U wr;
	RC r;
	Cfg cf;
	if (!cfgpath[0] || !hwnd) return;
	GetWindowRect(hwnd, &r);
	cf.theme = theme; cf.x = r.l; cf.y = r.t; cf.cx = r.r - r.l; cf.cy = r.b - r.t;
	cf.fontsize = fontsize;
	cpy(cf.font, curfont, 32);
	f = CreateFileW(cfgpath, 0x40000000u, 0, 0, 2, 0x80, 0);
	if (f == (H)-1) return;
	WriteFile(f, &cf, sizeof cf, &wr, 0);
	CloseHandle(f);
}

static C *app(C *d, const C *s) { while (*s) *d++ = *s++; return d; }

/* path (< 600) + theme name + font (< 32) always fit in b */
static void setcap(void)
{
	C b[720], *q;
	if (!hwnd) return;
	q = app(b, tpath[0] ? tpath : (const C *)L"mdv \x2014 drop a .md file here");
	q = app(q, L" \x00b7 ");
	q = app(q, themes[theme].name);
	q = app(q, L" \x00b7 ");
	*app(q, curfont) = 0;
	SetWindowTextW(hwnd, b);
}

/* rebuilds every brush; wraps around in both directions */
static void settheme(int t)
{
	int i;
	theme = (t % NTHEME + NTHEME) % NTHEME;
	pal = themes[theme].pal;
	for (i = 0; i < 9; i++) {
		if (brs[i]) DeleteObject(brs[i]);
		brs[i] = CreateSolidBrush(pal[i]);
	}
	if (bmm) DeleteObject(bmm);
	if (bvp) DeleteObject(bvp);
	if (bbg) DeleteObject(bbg);
	bmm = CreateSolidBrush(themes[theme].mmbg);
	bvp = CreateSolidBrush(themes[theme].vp);
	bbg = CreateSolidBrush(themes[theme].bg);
	drawmm();
	setcap();
	if (hwnd) InvalidateRect(hwnd, 0, 0);
}

/* ---------------- window ---------------- */
static H ed;                        /* EDIT control, only while in edit mode */

static void setbar(void)
{
	SI si;
	si.cb = sizeof si; si.mask = 0x17;
	si.mn = 0; si.mx = nrow > 0 && !ed ? nrow - 1 : 0;   /* hidden while editing */
	si.page = (U)vis; si.pos = top; si.track = 0;
	SetScrollInfo(hwnd, 1, &si, 1);
}

static void clamp(void)
{
	int mx = nrow - vis;
	if (mx < 0) mx = 0;
	if (top > mx) top = mx;
	if (top < 0) top = 0;
}

static void relayout(void)
{
	int tw;
	mmw = mw >= 420 ? MMW : 0;
	tw = mw - mmw;
	vis = chh ? mh / chh : 1;
	if (vis < 1) vis = 1;
	layout(cw ? tw / cw - 2 : 60);
	clamp();
	setbar();
	drawmm();
}

/* after any change to top */
static void scrolled(void)
{
	clamp();
	setbar();
	InvalidateRect(hwnd, 0, 0);
}

static void mmclick(int y)
{
	if (nrow <= 0) return;
	top = (int)((Q)y * (Q)nrow / (Q)mh) - vis / 2;
	scrolled();
}

/* (re)creates fnt[0..2] for curfont and recomputes cell metrics */
static void mkfont(void)
{
	H dc;
	TM tm;
	SZ sz;
	int i;
	for (i = 0; i < 3; i++) {       /* regular, bold, italic */
		if (fnt[i]) DeleteObject(fnt[i]);
		fnt[i] = CreateFontW(-fontsize, 0, 0, 0, i == 1 ? 700 : 400, i == 2, 0, 0, 1, 4, 0, 5, 49, curfont);
	}
	dc = GetDC(0);
	SelectObject(dc, fnt[0]);
	GetTextMetricsW(dc, &tm);
	GetTextExtentPoint32W(dc, L"MMMMMMMMMM", 10, &sz);
	cw = sz.cx / 10;
	if (cw < 1) cw = 8;
	chh = tm.h;                     /* EDIT's line height too, so edit mode lines up */
	if (chh < 1) chh = 16;
	ReleaseDC(0, dc);
	if (memdc) SelectObject(memdc, fnt[0]);
}

static void setfontsize(int sz)
{
	if (sz < 6) sz = 6;
	if (sz > 72) sz = 72;
	fontsize = sz;
	mkfont();
	if (hwnd) { relayout(); InvalidateRect(hwnd, 0, 0); }
}

static void setfontname(const C *name)
{
	cpy(curfont, name, 32);
	setcap();
	setfontsize(fontsize);
}

/* ---------------- edit mode: a plain EDIT control over the view ---------------- */
static void editon(void)
{
	C *b, pv = 0;
	int i, j = 0, c = 0, at = 0, off = 0;
	if (ed || !tpath[0]) return;
	if (nrow > 0 && (i = rowidx[top]) < nrun) {  /* first source line starting in view, */
		while (i > 0 && i < nrun - 1 && runs[i].src == runs[i - 1].src) i++;
		at = runs[i].src;                   /* and how far below the top it is shown */
		off = top - runs[i].row;
	}
	b = VirtualAlloc(0, (Q)ntx * 4 + 8, 0x3000, 4);
	if (!b) return;
	crlf = 0;
	for (i = 0; i < ntx; i++) {             /* the control wants CRLF */
		if (i == at) c = j;
		if (tx[i] == '\r') crlf = 1;
		else if (tx[i] == '\n' && pv != '\r') b[j++] = '\r';
		b[j++] = pv = tx[i];
	}
	ed = CreateWindowExW(0, L"EDIT", 0, 0x50201144u, 0, 0, mw, mh, hwnd, 0, ginst, 0);
	setbar();                               /* hides our scrollbar: resize before wrapping */
	SendMessageW(ed, 0x30, (Q)fnt[0], 0);                   /* WM_SETFONT */
	SendMessageW(ed, 0xC5, 0, 0);                           /* EM_SETLIMITTEXT: lift 32K cap */
	SendMessageW(ed, 0xD3, 3, (Q)(U)(cw | (cw + mmw) << 16));  /* EM_SETMARGINS: wrap like the view */
	SetWindowTextW(ed, b);
	/* EM_LINESCROLL from the top so that line sits where the view showed it;
	   only then EM_SETSEL the caret onto it, as that scrolls it into view */
	SendMessageW(ed, 0xB6, 0, SendMessageW(ed, 0xC9, (Q)c, 0) + off);
	SendMessageW(ed, 0xB1, (Q)c, (Q)c);
	VirtualFree(b, 0, 0x8000);
	SetFocus(ed);
}

/* 1 when the edit buffer may be thrown away: unmodified, or the user agrees */
static int candrop(void)
{
	return !SendMessageW(ed, 0xB8, 0, 0) ||                 /* EM_GETMODIFY */
	       MessageBoxW(hwnd, L"Discard unsaved changes?", L"mdv", 0x31) == 1;
}

/* Ctrl+S writes the text back as UTF-8 and re-renders; Esc discards it */
static void editdone(int save)
{
	int n, i, j, t = top;
	C *b;
	char *u;
	H f;
	U wr = 0;
	if (!save && !candrop()) return;
	if (save) {
		n = (int)SendMessageW(ed, 0x0E, 0, 0);          /* WM_GETTEXTLENGTH */
		b = VirtualAlloc(0, (Q)n * 5 + 16, 0x3000, 4);  /* n UTF-16 + 3n UTF-8 */
		if (!b) return;
		SendMessageW(ed, 0x0D, (Q)n + 1, (Q)b);         /* WM_GETTEXT */
		for (i = j = 0; i < n; i++) if (crlf || b[i] != '\r') b[j++] = b[i];
		u = (char *)(b + n + 1);
		u[0] = (char)0xEF; u[1] = (char)0xBB; u[2] = (char)0xBF;
		n = WideCharToMultiByte(65001, 0, b, j, u + 3, n * 3 + 1, 0, 0);
		f = CreateFileW(tpath, 0x40000000u, 0, 0, 2, 0x80, 0);
		if (f != (H)-1) { WriteFile(f, u + 3 - bom * 3, (U)(n + bom * 3), &wr, 0); CloseHandle(f); }
		VirtualFree(b, 0, 0x8000);
		if (wr != (U)(n + bom * 3)) {                   /* keep editing, nothing lost */
			MessageBoxW(hwnd, L"Save failed", L"mdv", 0x10);
			return;
		}
		loadfile(tpath);
	}
	DestroyWindow(ed);
	ed = 0;
	top = t;
	relayout();
	SetFocus(hwnd);
	InvalidateRect(hwnd, 0, 0);
}

/* collects installed fixed-pitch face names into fontnames[], sorted */
static int S enumproc(const LF *lf, const TM *tm, U ft, Q lp)
{
	int i;
	(void)tm; (void)ft; (void)lp;
	if ((lf->pf & 3) != 1) return 1;        /* fixed pitch only */
	if (lf->face[0] == '@') return 1;       /* vertical-writing variant */
	for (i = 0; i < nfontnames; i++) if (weq(fontnames[i], lf->face)) return 1;
	if (nfontnames >= MAXFONT) return 1;
	i = nfontnames++;
	cpy(fontnames[i], lf->face, 32);
	while (i > 0 && wless(fontnames[i], fontnames[i - 1])) {
		C tmp[32];
		cpy(tmp, fontnames[i], 32);
		cpy(fontnames[i], fontnames[i - 1], 32);
		cpy(fontnames[i - 1], tmp, 32);
		i--;
	}
	return 1;
}

static void enumfonts(void)
{
	H dc;
	LF lf;
	memset(&lf, 0, sizeof lf);
	lf.cs = 1;                              /* DEFAULT_CHARSET: all families */
	dc = GetDC(0);
	EnumFontFamiliesExW(dc, &lf, enumproc, 0, 0);
	ReleaseDC(0, dc);
}

/* ---------------- font picker (custom popup, no listbox control) ---------------- */
static Q S pickproc(H w, U m, Q wp, Q lp)
{
	PS ps;
	switch (m) {
	case 15: {                      /* WM_PAINT */
		H dc = BeginPaint(w, &ps);
		RC r;
		int i;
		SetBkMode(dc, 1);
		SelectObject(dc, fnt[0]);
		for (i = 0; i < pkrows; i++) {
			int idx = pktop + i;
			r.l = 0; r.t = i * chh; r.r = pkw; r.b = (i + 1) * chh;
			FillRect(dc, &r, idx == pksel ? bvp : bbg);
			if (idx < nfontnames) {
				SetTextColor(dc, pal[0]);
				TextOutW(dc, 4, i * chh, fontnames[idx], wlen(fontnames[idx]));
			}
		}
		EndPaint(w, &ps);
		return 0;
	}
	case 0x0100: {                  /* WM_KEYDOWN */
		switch (wp) {
		case 0x26: pksel--; break;                     /* up */
		case 0x28: pksel++; break;                     /* down */
		case 0x21: pksel -= pkrows; break;              /* page up */
		case 0x22: pksel += pkrows; break;              /* page down */
		case 0x24: pksel = 0; break;                    /* home */
		case 0x23: pksel = nfontnames - 1; break;       /* end */
		case 0x0D:                                      /* enter */
			if (pksel >= 0 && pksel < nfontnames) setfontname(fontnames[pksel]);
			DestroyWindow(w);
			return 0;
		case 0x1B:                                      /* escape */
			DestroyWindow(w);
			return 0;
		default:
			return 0;
		}
		if (pksel < 0) pksel = 0;
		if (pksel > nfontnames - 1) pksel = nfontnames - 1;
		if (pksel < pktop) pktop = pksel;
		if (pksel >= pktop + pkrows) pktop = pksel - pkrows + 1;
		InvalidateRect(w, 0, 0);
		return 0;
	}
	case 0x0201:                    /* WM_LBUTTONDOWN */
		{
			int idx = pktop + (int)(short)((lp >> 16) & 0xffff) / (chh ? chh : 1);
			if (idx >= 0 && idx < nfontnames) setfontname(fontnames[idx]);
		}
		DestroyWindow(w);
		return 0;
	case 0x020A: {                  /* WM_MOUSEWHEEL */
		int d = (int)(short)((wp >> 16) & 0xffff);
		pktop -= (d / 120) * 3;
		if (pktop > nfontnames - pkrows) pktop = nfontnames - pkrows;
		if (pktop < 0) pktop = 0;
		InvalidateRect(w, 0, 0);
		return 0;
	}
	case 0x0008:                    /* WM_KILLFOCUS: dismiss like a dropdown */
		DestroyWindow(w);
		return 0;
	case 2:                         /* WM_DESTROY */
		pkhwnd = 0;
		return 0;
	case 0x0014:                    /* WM_ERASEBKGND */
		return 1;
	}
	return DefWindowProcW(w, m, wp, lp);
}

static void openpicker(void)
{
	RC r;
	int i, maxw = 0, w, h, x, y;
	if (pkhwnd) { SetFocus(pkhwnd); return; }
	if (nfontnames == 0) return;
	pksel = 0;
	for (i = 0; i < nfontnames; i++) if (weq(fontnames[i], curfont)) { pksel = i; break; }
	pkrows = nfontnames < 16 ? nfontnames : 16;
	if (pkrows < 1) pkrows = 1;
	pktop = pksel - pkrows / 2;
	if (pktop > nfontnames - pkrows) pktop = nfontnames - pkrows;
	if (pktop < 0) pktop = 0;
	for (i = 0; i < nfontnames; i++) { int l = wlen(fontnames[i]); if (l > maxw) maxw = l; }
	w = (maxw + 3) * cw;
	h = pkrows * chh;
	pkw = w;
	GetWindowRect(hwnd, &r);
	x = r.l + ((r.r - r.l) - w) / 2;
	y = r.t + ((r.b - r.t) - h) / 2;
	pkhwnd = CreateWindowExW(0, L"mdvpick", L"", 0x90800000u, x, y, w, h, hwnd, 0, ginst, 0);
	SetFocus(pkhwnd);
}

static Q S wndproc(H w, U m, Q wp, Q lp)
{
	PS ps;
	int d, x, y;

	switch (m) {
	case 5: {                       /* WM_SIZE */
		H dc;
		hwnd = w;               /* arrives during CreateWindowExW */
		if (!memdc) return 0;
		dc = GetDC(w);
		mw = (int)(lp & 0xffff);
		mh = (int)((lp >> 16) & 0xffff);
		if (mw < 1) mw = 1;
		if (mh < 1) mh = 1;
		if (membm) DeleteObject(membm);
		if (mmbm) DeleteObject(mmbm);
		membm = CreateCompatibleBitmap(dc, mw, mh);
		mmbm = CreateCompatibleBitmap(dc, MMW, mh);
		SelectObject(memdc, membm);
		SelectObject(mmdc, mmbm);
		ReleaseDC(w, dc);
		if (ed) MoveWindow(ed, 0, 0, mw, mh, 1);
		relayout();
		InvalidateRect(w, 0, 0);
		return 0;
	}
	case 15: {                      /* WM_PAINT */
		H dc = BeginPaint(w, &ps);
		int lim = top + vis + 1, i, cf = -1;
		RC r;
		r.l = 0; r.t = 0; r.r = mw; r.b = mh;
		FillRect(memdc, &r, bbg);
		SetBkMode(memdc, 1);
		if (nrow <= 0) {
			SelectObject(memdc, fnt[0]);
			SetTextColor(memdc, pal[7]);
			TextOutW(memdc, cw * 2, chh * 2, L"Drop a .md file onto this window", 32);
			TextOutW(memdc, cw * 2, chh * 4, L"t / T \x2014 cycle theme    f \x2014 choose font    +/- \x2014 font size    q \x2014 quit", 69);
			TextOutW(memdc, cw * 2, chh * 5, L"e \x2014 edit    Ctrl+S \x2014 save    Esc \x2014 cancel edit", 46);
		}
		if (nrow > 0) {
			for (i = rowidx[top < nrow ? top : nrow]; i < nrun; i++) {
				Run *rn = runs + i;
				if (rn->row >= lim) break;
				if (rn->row < top) continue;
				if (sfont[rn->st] != cf) { cf = sfont[rn->st]; SelectObject(memdc, fnt[cf]); }
				SetTextColor(memdc, pal[rn->st]);
				TextOutW(memdc, cw + rn->col * cw, (rn->row - top) * chh, rn->t, rn->len);
			}
		}
		if (mmw) {
			int x0 = mw - mmw, y1, y2;
			BitBlt(memdc, x0, 0, mmw, mh, mmdc, 0, 0, 0x00CC0020u);
			if (nrow > 0) {
				y1 = (int)((Q)top * (Q)mh / (Q)nrow);
				y2 = (int)((Q)(top + vis) * (Q)mh / (Q)nrow);
				if (y2 < y1 + 4) y2 = y1 + 4;
				if (y2 > mh) y2 = mh;
				r.l = x0; r.r = mw; r.t = y1; r.b = y1 + 2; FillRect(memdc, &r, brs[6]);
				r.t = y2 - 2; r.b = y2; FillRect(memdc, &r, brs[6]);
				r.t = y1; r.b = y2; r.r = x0 + 2; FillRect(memdc, &r, brs[6]);
				r.l = mw - 2; r.r = mw; FillRect(memdc, &r, brs[6]);
			}
		}
		BitBlt(dc, 0, 0, mw, mh, memdc, 0, 0, 0x00CC0020u);
		EndPaint(w, &ps);
		return 0;
	}
	case 0x0233: {                  /* WM_DROPFILES */
		C buf[600];
		buf[0] = 0;
		DragQueryFileW((H)wp, 0, buf, 600);
		DragFinish((H)wp);
		if (!ed && buf[0] && loadfile(buf)) {
			cpy(tpath, buf, 600);
			setcap();
			top = 0;
			relayout();
		}
		InvalidateRect(w, 0, 0);
		return 0;
	}
	case 7:                         /* WM_SETFOCUS: keys belong to the editor */
		if (ed) SetFocus(ed);
		return 0;
	case 0x0010:                    /* WM_CLOSE */
		if (ed && !candrop()) return 0;
		break;
	case 0x0011:                    /* WM_QUERYENDSESSION: logoff/shutdown */
		return !ed || candrop();
	case 2:                         /* WM_DESTROY */
		savecfg();
		PostQuitMessage(0);
		return 0;
	case 0x020A:                    /* WM_MOUSEWHEEL */
		d = (int)(short)((wp >> 16) & 0xffff);
		top -= (d / 120) * 3;
		scrolled();
		return 0;
	case 0x0201:                    /* WM_LBUTTONDOWN */
		x = (int)(short)(lp & 0xffff);
		y = (int)(short)((lp >> 16) & 0xffff);
		if (mmw && x >= mw - mmw) { dragging = 1; SetCapture(w); mmclick(y); }
		return 0;
	case 0x0200:                    /* WM_MOUSEMOVE */
		if (dragging) mmclick((int)(short)((lp >> 16) & 0xffff));
		return 0;
	case 0x0202:                    /* WM_LBUTTONUP */
		if (dragging) { dragging = 0; ReleaseCapture(); }
		return 0;
	case 0x0100:                    /* WM_KEYDOWN: space pgup pgdn end home - up - down */
		if (wp - 0x20 > 8 || (d = "\3\2\3\7\6\10\0\10\1"[wp - 0x20]) > 7)
			return DefWindowProcW(w, m, wp, lp);
		wp = (Q)d;                  /* now the matching SB_ code */
		/* fall through */
	case 0x0115: {                  /* WM_VSCROLL */
		SI si;
		int code = (int)(wp & 0xffff);
		si.cb = sizeof si; si.mask = 0x17;
		GetScrollInfo(w, 1, &si);
		switch (code) {
		case 0: top--; break;
		case 1: top++; break;
		case 2: top -= vis - 1; break;
		case 3: top += vis - 1; break;
		case 4: case 5: top = si.track; break;
		case 6: top = 0; break;
		case 7: top = nrow; break;
		default: return 0;
		}
		scrolled();
		return 0;
	}
	case 0x0102:                    /* WM_CHAR */
		if (wp == 't') { settheme(theme + 1); return 0; }
		if (wp == 'T') { settheme(theme - 1); return 0; }
		if ((wp | 32) == 'f') { openpicker(); return 0; }
		if ((wp | 32) == 'e') { editon(); return 0; }
		if (wp == '+' || wp == '=') { setfontsize(fontsize + 1); return 0; }
		if (wp == '-' || wp == '_') { setfontsize(fontsize - 1); return 0; }
		if ((wp | 32) == 'q') PostQuitMessage(0);
		return 0;
	case 0x0133:                    /* WM_CTLCOLOREDIT: theme the editor */
		SetTextColor((H)wp, pal[0]);
		SetBkColor((H)wp, themes[theme].bg);
		return (Q)bbg;
	case 0x0014:                    /* WM_ERASEBKGND */
		return 1;
	}
	return DefWindowProcW(w, m, wp, lp);
}

/* ---------------- entry ---------------- */
__attribute__((force_align_arg_pointer))
void _start(void)
{
	C *c = GetCommandLineW(), *path, *title;
	H dc, inst;
	WCL wc;
	MSG msg;
	int q = 0, i;
	int cfgth = 0, cfgx = 0x80000000, cfgy = 0x80000000, cfgw = 1040, cfgh = 800;

	while (*c == ' ') c++;
	if (*c == '"') { q = 1; c++; }
	while (*c && !(q ? *c == '"' : *c == ' ')) c++;
	if (*c) c++;
	while (*c == ' ') c++;
	if (*c == '"') { c++; for (i = 0; c[i] && c[i] != '"'; i++); c[i] = 0; }
	path = c;

	if (*path) { cpy(tpath, path, 600); loadfile(path); }   /* a missing file can still be created with e */

	cpy(curfont, L"Consolas", 32);
	fontsize = 15;
	buildcfgpath();
	loadcfg(&cfgth, &cfgx, &cfgy, &cfgw, &cfgh);

	inst = GetModuleHandleW(0);
	ginst = inst;
	enumfonts();
	mkfont();
	settheme(cfgth);

	/* memdc/mmdc must exist before CreateWindowExW,
	   because WM_SIZE is delivered synchronously during creation */
	dc = GetDC(0);
	memdc = CreateCompatibleDC(dc);
	mmdc = CreateCompatibleDC(dc);
	ReleaseDC(0, dc);
	SelectObject(memdc, fnt[0]);

	memset(&wc, 0, sizeof wc);
	wc.style = 3;
	wc.proc = wndproc;
	wc.inst = inst;
	wc.cur = LoadCursorW(0, (const C *)32512);
	wc.icon = LoadIconW(inst, (const C *)1);   /* 16+32 px, see mdv.rc */
	wc.cls = L"mdv";
	RegisterClassW(&wc);

	wc.proc = pickproc;
	wc.icon = 0;
	wc.cls = L"mdvpick";
	RegisterClassW(&wc);

	title = *path ? path : (C *)L"mdv \x2014 drop a .md file here";
	hwnd = CreateWindowExW(0, L"mdv", title, 0x02CF0000u | 0x00200000u,  /* +CLIPCHILDREN */
		cfgx, cfgy, cfgw, cfgh, 0, 0, inst, 0);

	setcap();
	DragAcceptFiles(hwnd, 1);
	ShowWindow(hwnd, 1);
	UpdateWindow(hwnd);

	while (GetMessageW(&msg, 0, 0, 0) > 0) {
		if (ed && msg.m == 0x0102 && (msg.w == 19 || msg.w == 27)) {   /* Ctrl+S / Esc */
			editdone(msg.w == 19);
			continue;
		}
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
	ExitProcess(0);
}
