/* PostScript graphics - 
 * This provides a bit of a shell around writing graphics to
 * a postScript file.  Perhaps the most important thing it
 * does is convert 0,0 from being at the bottom left to
 * being at the top left. */

#ifndef PSGFX_H
#define PSGFX_H

struct psGfx 
/* A postScript output file. */
    {
    FILE *f;                    /* File to write to. */
    int pixWidth, pixHeight;	/* Size of image in virtual pixels. */
    double ptWidth, ptHeight;   /* Size of image in points (1/72 of an inch) */
    double xScale, yScale;      /* Conversion from pixels to points. */
    double xOff, yOff;          /* Offset from pixels to points. */
    double fontHeight;		/* Height of current font. */
    struct psClipRect *clipStack; /* Clipping region. */
    };

struct psClipRect
/* A clipping region. */
    {
    struct psClipRect *next;
    int x1, y1;	/* upper left corner. */
    int x2, y2; /* lower right, not inclusive */
    };

struct psGfx *psOpen(char *fileName, 
	int pixWidth, int pixHeight, 	 /* Dimension of image in pixels. */
	double ptWidth, double ptHeight, /* Dimension of image in points. */
	double ptMargin);                /* Image margin in points. */
/* Open up a new postscript file.  If ptHeight is 0, it will be
 * calculated to keep pixels square. */

void psClose(struct psGfx **pPs);
/* Close out postScript file. */

void psDrawBox(struct psGfx *ps, double x, double y, 
	double width, double height);
/* Draw a filled box in current color. */

void psDrawLine(struct psGfx *ps, double x1, double y1, 
	double x2, double y2);
/* Draw a line from x1/y1 to x2/y2 */

void psFillUnder(struct psGfx *ps, double x1, double y1, 
	double x2, double y2, double bottom);
/* Draw a 4 sided filled figure that has line x1/y1 to x2/y2 at
 * it's top, a horizontal line at bottom at it's bottom,  and
 * vertical lines from the bottom to y1 on the left and bottom to
 * y2 on the right. */

void psXyOut(struct psGfx *ps, double x, double y);
/* Output x,y position transformed into PostScript space. 
 * Useful if you're mixing direct PostScript with psGfx
 * functions. */

void psWhOut(struct psGfx *ps, double width, double height);
/* Output width/height transformed into PostScript space. */

void psMoveTo(struct psGfx *ps, double x, double y);
/* Move PostScript position to given point. */

void psTextAt(struct psGfx *ps, double x, double y, char *text);
/* Output text in current font at given position. */

void psTextDown(struct psGfx *ps, double x, double y, char *text);
/* Output text going downwards rather than across at position. */

void psTextRight(struct psGfx *mg, double x, double y, 
	double width, double height, char *text);
/* Draw a line of text right justified in box defined by x/y/width/height */

void psTextCentered(struct psGfx *mg, double x, double y, 
	double width, double height, char *text);
/* Draw a line of text centered in box defined by x/y/width/height */

void psTimesFont(struct psGfx *ps, double size);
/* Set font to times of a certain size. */

void psSetColor(struct psGfx *ps, int r, int g, int b);
/* Set current color. r/g/b values are between 0 and 255. */

void psSetGray(struct psGfx *ps, double grayVal);
/* Set gray value (between 0.0 and 1.0. */

void psPushG(struct psGfx *ps);
/* Save graphics state on stack. */

void psPopG(struct psGfx *ps);
/* Pop off saved graphics state. */

void psPushClipRect(struct psGfx *ps, double x, double y, 
	double width, double height);
/* Push clipping rectangle onto graphics stack. */

void psPopClipRect(struct psGfx *ps);
/* Get rid of clipping. Beware that this does a psPopG, so
 * other graphic variables will be reset to the time of
 * the corresponding psPushClipRect. */

#endif /* PSGFX_H */

