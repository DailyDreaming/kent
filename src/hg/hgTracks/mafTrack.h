/* mafTrack.h - MAF track display */

#ifndef MAFTRACK_H
#define MAFTRACK_H

/* zoom level where summary file is used */
#define MAF_SUMMARY_VIEW 1000000

/* zoom level that displays synteny breaks and nesting brackets */
#define MAF_DETAIL_VIEW 30000

void drawMafRegionDetails(struct mafAli *mafList, int height,
        int seqStart, int seqEnd, struct vGfx *vg, int xOff, int yOff,
        int width, MgFont *font, Color color, Color altColor,
        enum trackVisibility vis, boolean isAxt, boolean chainBreaks);
/* Draw wiggle/density plot based on scoring things on the fly. */

void drawMafChain(struct vGfx *vg, int xOff, int yOff, int width, int height,
                        Color color, boolean isDouble);
/* draw single or double chain line between alignments in MAF display */

#endif
