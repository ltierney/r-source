/*
 *  R : A Computer Language for Statistical Data Analysis
 *  Copyright (C) 1998--1999  Guido Masarotto
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
   A version of drawing.c without current/state.
   More safe in case of multiple redrawing
 */

#include "internal.h"

#define GETHDC(a) (((a->kind==PrinterObject)||(a->kind==MetafileObject))?\
                        ((HDC)a->handle):get_context(a))

/*
 *  Some clipping functions.
 */
rect ggetcliprect(drawing d)
{
    RECT R;
    rect r;
    HDC dc = GETHDC(d);
    GetClipBox(dc, &R);
    r.x = R.left;
    r.y = R.top;
    r.width = R.right - R.left;
    r.height = R.bottom - R.top;
    return r;
}

void gsetcliprect(drawing d,rect r)
{
    HRGN rgn;
    HDC dc = GETHDC(d);
    rgn = CreateRectRgn(r.x, r.y, r.x + r.width, r.y + r.height);
    SelectClipRgn(dc, rgn);
    DeleteObject(rgn);
}


void gbitblt(drawing db, drawing sb, point p, rect r)
{
    HDC src;
    HDC dst;

    dst = GETHDC(db);
    src = GETHDC(sb);
    BitBlt(dst, p.x, p.y, r.width, r.height, src, r.x, r.y, SRCCOPY);
}


void gscroll(drawing d, point dp, rect r)
{
    rect cliprect = getrect(d);
    HDC dc = GETHDC(d);

    ScrollDC(dc, dp.x - r.x, dp.y - r.y,
	     (RECT *)&r, (RECT *) &cliprect, 0, NULL);
}

void ginvert(drawing d, rect r)
{
    HDC dc = GETHDC(d);

    PatBlt(dc, r.x, r.y, r.width, r.height, DSTINVERT);
}

rgb ggetpixel(drawing d, point p)
{
    rgb c;
    HDC dc = GETHDC(d);

    c = GetPixel(dc, p.x, p.y);
    c = ((c&0x000000FFL)<<16) | (c&0x0000FF00L) |
	((c&0x00FF0000L)>>16);
    return c;
}

static COLORREF getwinrgb(drawing d, rgb c)
{
    int r, g, b;
    long luminance;
    int depth;

    r = (int) ((c >> 16) & 0x000000FFL);
    g = (int) ((c >>  8) & 0x000000FFL);
    b = (int) ((c >>  0) & 0x000000FFL);
    depth = getdepth(d);
    if (depth <= 2)	/* map to black or white, or grey if c == Grey */
    {
	luminance = (r*3 + g*5 + b) / 9;
	if (luminance > 0x0087)		r = g = b = 0x00FF;
	else if (luminance <= 0x0077)	r = g = b = 0x0000;
	else				r = g = b = 0x0080;
	c = rgb(r, g, b);
    }
    return RGB(r, g, b);
}

void gsetpixel(drawing d, point p, rgb c)
{
    HDC dc = GETHDC(d);
    HBRUSH br = CreateSolidBrush(getwinrgb(d, c));

    fix_brush(dc, d, br);
    SelectObject(dc, br);
    PatBlt(dc, p.x, p.y, 1, 1, PATCOPY);
    SelectObject(dc, GetStockObject(NULL_BRUSH));
    DeleteObject(br);
}

static void CALLBACK  gLineHelper(int x, int y, LPARAM aa) 
{
    int i, *a;
    a = (int *)aa;
    if (a[0]) {
	for (i = 0; i < a[6]; i++)
	    PatBlt((HDC)a[8], x + i*a[4], y + i*a[5], 1, 1, PATCOPY);
    }
    a[1] += 1;
    if (a[1] >= a[2]) {
	a[1] = 0;
	a[2] = 0;
	while (!a[2]) {
	    double pd;
	    a[3] = (a[3] + 4) % 32;
	    pd = ((a[7] >> a[3]) & 15) * a[6];
	    if (pd > 0) {
		a[2] = (pd * GetDeviceCaps((HDC)a[8], LOGPIXELSX)) / 72.0 + 0.5;
		if (a[2] < 1) a[2] = 1;
	    }
	    else
		a[2] = 0;
	    a[0] = a[0] ? 0 : 1;
	}
    }
}

void gdrawline(drawing d, int width, int style, rgb c, point p1, point p2)
{
    int a[9];
    HDC dc = GETHDC(d);
    COLORREF winrgb = getwinrgb(d, c);
    if ((p1.x == p2.x) && (p1.y == p2.y))
	return;
    if (!style) {
	HPEN gpen = CreatePen(PS_INSIDEFRAME, width, winrgb);
	SelectObject(dc, gpen);
	SetROP2(dc, R2_COPYPEN);
	MoveToEx(dc, p1.x, p1.y, NULL);
	LineTo(dc, p2.x, p2.y);
	SelectObject(dc, GetStockObject(NULL_PEN));
	DeleteObject(gpen);
    }
     else {
	point pshift;
	double pd;
	HBRUSH br = CreateSolidBrush(getwinrgb(d,c));
	fix_brush(dc, d, br);
	SelectObject(dc, br);	
	a[0] = 1;
	a[1] = 0;
	pd =(style & 15)*width;
	a[2] = (pd * GetDeviceCaps(dc,LOGPIXELSX)) / 72.0 + 0.5;
	if (a[2] < 1) a[2] = 1;
	a[3] = 0;
	if (p1.x != p2.x) {
	    a[4] = 0;
	    a[5] = 1;
	    pshift = pt(0, width/2);
	}
	else {
	    a[4] = 1;
	    a[5] = 0;
	    pshift = pt(width/2,0);
	}
	a[6] = width;
	a[7] = style;
	a[8] = (int) dc;
	LineDDA(p1.x, p1.y, p2.x, p2.y, gLineHelper, (LPARAM) a);
	SelectObject(dc, GetStockObject(NULL_BRUSH));
	DeleteObject(br);
    }
}

void gdrawrect(drawing d, int width, int style, rgb c, rect r)
{
    int x0 = r.x;
    int y0 = r.y;
    int x1 = r.x + r.width;
    int y1 = r.y + r.height;
    gdrawline(d, width, style, c, pt(x0,y0), pt(x1,y0));
    gdrawline(d, width, style, c, pt(x1,y0), pt(x1,y1));
    gdrawline(d, width, style, c, pt(x1,y1), pt(x0,y1));
    gdrawline(d, width, style, c, pt(x0,y1), pt(x0,y0));
}

void gfillrect(drawing d, rgb fill, rect r)
{
    HDC dc = GETHDC(d);
    HBRUSH br = CreateSolidBrush(getwinrgb(d, fill));
    fix_brush(dc, d, br);
    SelectObject(dc, br);	      
    PatBlt(dc, r.x, r.y, r.width, r.height, PATCOPY);
    SelectObject(dc, GetStockObject(NULL_BRUSH));
    DeleteObject(br);
}

void gdrawellipse(drawing d,int width,rgb border,rect r)
{
    HDC dc = GETHDC(d);
    HPEN gpen = CreatePen(PS_INSIDEFRAME, width, getwinrgb(d,border));
    SelectObject(dc, gpen);
    SetROP2(dc, R2_COPYPEN);
    Ellipse(dc, r.x, r.y, r.x+r.width, r.y+r.height);
    SelectObject(dc, GetStockObject(NULL_PEN));
    DeleteObject(gpen);
}

void goldfillellipse(drawing d, rgb fill, rect r)
{
    HDC dc = GETHDC(d);
    HBRUSH br = CreateSolidBrush(getwinrgb(d, fill));
    fix_brush(dc, d, br);
    SelectObject(dc, br);
    Ellipse(dc, r.x, r.y, r.x+r.width, r.y+r.height);
    SelectObject(dc, GetStockObject(NULL_BRUSH));
    DeleteObject(br);
}


#ifndef fastfillrect
#define fastfillrect(x, y, w, h) PatBlt(dc, (x), (y), (w), (h), mode)
#endif

void gfillellipse(drawing d, rgb fill, rect r)
{			/* e(x,y) = b*b*x*x + a*a*y*y - a*a*b*b */
    register long mode = PATCOPY;
    int w_odd = (r.width & 0x0001);
    int h_odd = (r.height & 0x0001);
    int a = r.width >> 1;
    int b = r.height >> 1;
    point c = pt(r.x+a,r.y+b);
    int x = 0;
    int y = b;
    long a2 = a*a;
    long b2 = b*b;
    long xcrit = ((a2+a2+a2) >> 2) + 1;
    long ycrit = ((b2+b2+b2) >> 2) + 1;
    long t = b2 + a2 - (a2+a2)*b;	/* t = e(x+1,y-1) */
    long dxt = b2*(3+x+x);
    long dyt = a2*(3-y-y);
    int d2xt = b2+b2;
    int d2yt = a2+a2;
    int stored = 0;
    int sx = 0, sy = 0, sh = 0; /* stored values of x, y, height */
    HDC dc;
    HBRUSH br;

    if ((r.width > 31) && (r.height > 31)) {
	goldfillellipse(d, fill, r);
	return;
    }
    if ((r.width < 3) || (r.height < 3)) {
	gfillrect(d, fill, r);
	return;
    }
        
    dc = GETHDC(d);
    br = CreateSolidBrush(getwinrgb(d, fill));
    fix_brush(dc, d, br);
    SelectObject(dc, br);

    if (w_odd == 0) {
	fastfillrect(c.x-1,c.y-b,2,r.height);
    }

    while (y > 0) {
	if (stored) {
	    if (sx != x) { /* output stored rect */
		fastfillrect(c.x-sx,c.y-sy, sx+sx+w_odd,sh);
		fastfillrect(c.x-sx,c.y+sy+h_odd-sh, sx+sx+w_odd,sh);
		stored = 0;
	    }
	    else /* increment height of stored rect */
		sh++;
	}
	if (t + a2*y < xcrit) { /* e(x+1,y-1/2) <= 0 */
	    /* move left and right to encounter edge */
	    x += 1;
	    t += dxt;
	    dxt += d2xt;
	} else if (t - b2*x >= ycrit) { /* e(x+1/2,y-1) > 0 */
	    /* drop down one line */
	    if (!stored) {
		sx = x;
		sy = y;
		sh = 1;
		stored = 1;
	    }
	    y -= 1;
	    t += dyt;
	    dyt += d2yt;
	} else {
	    /* drop diagonally down and out */
	    if (!stored) {
		sx = x;
		sy = y;
		sh = 1;
		stored = 1;
	    }

	    x += 1;
	    y -= 1;
	    t += dxt + dyt;
	    dxt += d2xt;
	    dyt += d2yt;
	}
    }
    if (stored) { /* output stored rectangle */
	fastfillrect(c.x-sx, c.y-sy, sx+sx+w_odd, sh);
	fastfillrect(c.x-sx, c.y+sy+h_odd-sh, sx+sx+w_odd, sh);
	stored = 0;
    }
    if (x <= a){
	fastfillrect(c.x-a, c.y-y, a+a+w_odd, 1);
	fastfillrect(c.x-a, c.y+y-1+h_odd, a+a+w_odd, 1);
    }
    SelectObject(dc, GetStockObject(NULL_BRUSH));
}

void gdrawpolygon(drawing d, int width, int style, rgb c, point *p, int n)
{
    int i;

    for (i = 1; i < n; i++) 
	gdrawline(d, width, style, c, p[i-1], p[i]);
    gdrawline(d, width, style, c, p[n-1], p[0]);
}

void gfillpolygon(drawing d,rgb fill,point *p, int n)
{
   HDC dc = GETHDC(d);
   HBRUSH br = CreateSolidBrush(getwinrgb(d,fill));
   fix_brush(dc, d, br);
   SelectObject(dc, br);	   
   Polygon(dc, (POINT FAR *) p, n);
   SelectObject(dc, GetStockObject(NULL_BRUSH));
   DeleteObject(br);
}


int gdrawstr(drawing d,font f,rgb c,point p, char *s)
{
    POINT curr_pos;
    int width;
    HFONT old;
    HDC dc = GETHDC(d);

    SetTextColor(dc, getwinrgb(d,c));
    old = SelectObject(dc, f->handle);
    MoveToEx(dc, p.x, p.y, NULL);
    SetBkMode(dc, TRANSPARENT);
    SetTextAlign(dc, TA_LEFT | TA_TOP | TA_UPDATECP);

    TextOut(dc, p.x, p.y, s, strlen(s));

    GetCurrentPositionEx(dc, &curr_pos);
    width = curr_pos.x - p.x;
    SelectObject(dc, old);

    return width;
}

rect gstrrect(drawing d,font f, char *s)
{
    SIZE size;
    HFONT old;
    HDC dc;
    if (! f)
	f = SystemFont;
    if (d)
	dc = GETHDC(d);
    else
	dc = GetDC(0);
    old = SelectObject(dc, f->handle);
    GetTextExtentPoint(dc, (LPSTR) s, strlen(s), &size);
    SelectObject(dc, old);
    if (!d) ReleaseDC(0,dc);
    return rect(0,0,size.cx, size.cy);
}

point gstrsize(drawing d,font f, char *s)
{
    rect r = gstrrect(d,f,s);
    return pt(r.width, r.height);
}

int gstrwidth(drawing d,font f, char *s)
{
    rect r = gstrrect(d,f,s);
    return r.width;
}

int ghasfixedwidth(font f) 
{
    TEXTMETRIC tm;
    HFONT old;
    HDC dc = GetDC(0);
    old = SelectObject(dc, (HFONT)f->handle);
    GetTextMetrics(dc, &tm);
    SelectObject(dc, old);
    ReleaseDC(0,dc);
    return !(tm.tmPitchAndFamily & TMPF_FIXED_PITCH);
}

void gcharmetric(drawing d, font f, int c, int *ascent, int *descent, 
		 int *width) 
{
    int first, last;
    TEXTMETRIC tm;
    HFONT old;
    HDC dc = GETHDC(d);
    old = SelectObject(dc, (HFONT)f->handle);
    GetTextMetrics(dc, &tm);
    first = tm.tmFirstChar;
    last = tm.tmLastChar;
    *ascent = tm.tmAscent;
    *descent = tm.tmDescent;
    if(c == 0)
	*width = tm.tmMaxCharWidth;
    else if(first <= c && c <= last) {
	char xx[2];
	xx[0]=c;
	xx[1]='\0';
	*width = strwidth(f,xx);
    }
    else {
	*ascent = 0;
	*descent = 0;
	*width = 0;
    }
    SelectObject(dc, old);
}

font gnewfont(drawing d, char *face, int style, int size, double rot)
{
    font obj;
    HFONT hf;
    LOGFONT lf;
    double pixs;
    /* the 'magic' 1.1 comes from V */
    pixs = size/72.0;
    if ((rot<=45.0) || ((rot>135) && (rot<=225)) || (rot>315))
	pixs = pixs * devicepixelsy(d);
    else
	pixs = pixs * devicepixelsx(d);

    lf.lfHeight = - (pixs + 0.5);

    lf.lfWidth = lf.lfEscapement = lf.lfOrientation = 0;
    lf.lfEscapement = 10*rot;
    lf.lfWeight = FW_NORMAL;
    lf.lfItalic = lf.lfUnderline = lf.lfStrikeOut = 0;
    if ((! strcmp(face, "Symbol")) ||
	(! strcmp(face, "Wingdings")))
	lf.lfCharSet = SYMBOL_CHARSET;
    else
	lf.lfCharSet = ANSI_CHARSET;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    strncpy(lf.lfFaceName, face, LF_FACESIZE-1);

    if (style & Italic)
	lf.lfItalic = 1;
    if (style & Bold)
	lf.lfWeight = FW_BOLD;
    if (style & FixedWidth)
	lf.lfPitchAndFamily |= FIXED_PITCH;
    if (style & SansSerif)
	lf.lfPitchAndFamily |= FF_SWISS;

    if ((hf = CreateFontIndirect(&lf)) == 0)
	return NULL;

    obj = new_font_object(hf);
    if (obj)
	obj->text = new_string(face);
    if (d && ((d->kind==PrinterObject) ||
	      (d->kind==MetafileObject))) {
	TEXTMETRIC tm;
	HFONT old = SelectObject((HDC)d->handle,hf);
	GetTextMetrics((HDC)d->handle, &tm);
	obj->rect.width = tm.tmAveCharWidth;
	obj->rect.height = tm.tmHeight;
	obj->rect.x = tm.tmAscent - tm.tmInternalLeading;
	obj->rect.y = tm.tmDescent;
	SelectObject(dc, old);
    }

    return (font) obj;
}


static int measuredev(drawing dev,int what)
{
    HDC hDC;
    int n;
    if (dev)
	hDC=GETHDC(dev);
    else
	hDC=GetDC(NULL);
    n = GetDeviceCaps(hDC,what);
    if (!dev) ReleaseDC(NULL,hDC);
    return n;
}

#define MEASUREDEV(a) {return measuredev(dev,a);}

int devicewidth(drawing dev) MEASUREDEV(HORZRES)
int deviceheight(drawing dev) MEASUREDEV(VERTRES)
int devicewidthmm(drawing dev) MEASUREDEV(HORZSIZE)
int deviceheightmm(drawing dev) MEASUREDEV(VERTSIZE)
int devicepixelsx(drawing dev) MEASUREDEV(LOGPIXELSX)
int devicepixelsy(drawing dev) MEASUREDEV(LOGPIXELSY)






