#include "common.h"
#include "hCommon.h"
#include "hash.h"
#include "jksql.h"
#include "memalloc.h"
#include "dystring.h"
#include "memgfx.h"
#include "vGfx.h"
#include "dnaseq.h"
#include "dnautil.h"
#include "hdb.h"
#include "psl.h"
#include "genePred.h"
#include "cdsColors.h"
#include "cds.h"
#include "genbank.h"
#include "hgTracks.h"

static void drawScaledBoxSampleWithText(struct vGfx *vg, 
					int chromStart, int chromEnd, double 
                                        scale, int xOff, int y, int height, 
                                        Color color, int score, MgFont *font, 
					char *text, boolean zoomed,
                                        boolean zoomedBox, Color *cdsColor, 
					Color textColor, int winStart, int 
                                        maxPixels)
/* Draw a box scaled from chromosome to window coordinates with
   a codon or set of 3 or less bases drawn in the box. */
{

/*first draw the box itself*/
drawScaledBoxSample(vg, chromStart, chromEnd, scale, xOff, y, height, 
		    color, score);

/*draw text in box if space, and align properly for codons or DNA*/
if (zoomed)
    {
    int i;
    int x1, x2, w;
    x1 = round((double)(chromStart-winStart)*scale) + xOff;
    x2 = round((double)(chromEnd-winStart)*scale) + xOff;
    w = x2-x1;
    if (w < 1)
        w = 1;
    if (x2 >= maxPixels)
        x2 = maxPixels - 1;
    w = x2-x1;
    if (w < 1)
        w = 1;

    if (chromEnd - chromStart == 3)
        spreadString(vg,x1,y,w,height,whiteIndex(),
		     font,text,strlen(text));
    else if (chromEnd - chromStart < 3)
        spreadString(vg,x1,y,w,height,cdsColor[CDS_PARTIAL_CODON],font,
		     text,strlen(text));
    else
        {
        int thisX,thisX2;
        char c[2];
        for (i=0; i<strlen(text); i++)
            {
            sprintf(c,"%c",text[i]);
            thisX = round((double)(chromStart+i-winStart)*scale) + xOff;
            thisX2 = round((double)(chromStart+1+i-winStart)*scale) + xOff;
            vgTextCentered(vg,thisX+(thisX2-thisX)/2.0,y,1,height,
			   whiteIndex(),font,c);
            }
        }
    }
}

static int convertCoordUsingPsl( int s, struct psl *psl )
/*return query position corresponding to target position in one 
 * of the coding blocks of a psl file, or return -1 if target position
 * is not in a coding exon of this psl entry or if the psl entry is
 * null*/
{
int i;
int idx = -1;
unsigned *qStarts = psl->qStarts;
unsigned *tStarts = psl->tStarts;
unsigned *blockSizes = psl->blockSizes;
unsigned tStart = 0;
unsigned thisQStart = 0;

if (psl == NULL)
    errAbort("Cannot find psl entry associated with gene %s<br>\n",
	     psl->qName);

if (s < psl->tStart || s >= psl->tEnd)
    errAbort("Position %d is outside of this psl entry (%d-%d).<br>\n",
	     s,psl->qStart,psl->qEnd);

for ( i=0; i<psl->blockCount; i++ )
    {
    unsigned tEnd;
    tStart = psl->tStarts[i];
    tEnd = tStart + psl->blockSizes[i];
    if (psl->strand[1] == '-') 
         reverseIntRange(&tStart, &tEnd, psl->tSize);

    if (s >= tStart-3 && s < tEnd)
	{
	idx = i;
	break;
	}
    }

if (idx < 0)
    return(-1);

if(psl->strand[1] == '-')
    thisQStart = psl->qSize - (qStarts[idx]+psl->blockSizes[idx]);
else
    thisQStart = qStarts[idx];

return(thisQStart + (s - tStart));
}


static void getCodonDna(char *retStr, char *chrom, int n, unsigned *exonStarts,
			unsigned *exonEnds, int exonCount, unsigned cdsStart, 
			unsigned cdsEnd, int startI, boolean reverse, 
			struct dnaSeq *mrnaSeq)
//get n bases from coding exons only.
{
int i, j, thisN;
int theStart, theEnd;

//clear retStr
sprintf(retStr,"   ");

if (!reverse)  /*positive strand*/
    {
   
    thisN = 0;
    //move to next block start
    i = startI+1;
    while (thisN < n)
	{
        struct dnaSeq *codonDna = NULL;

	/*already on last block*/
	if (i >= exonCount) 
	    break;

        //get dna for this exon
        codonDna = hDnaFromSeq( chrom, exonStarts[i], exonEnds[i], dnaUpper );

	theStart = exonStarts[i];
	theEnd = exonEnds[i];
	if (cdsStart > 0 && theStart < cdsStart)
	    theStart = cdsStart;

	if (cdsEnd > 0 && theEnd > cdsEnd)
	    theEnd = cdsEnd;
           
	for (j=0; j<(theEnd - theStart); j++)
	    {
	    if (mrnaSeq != NULL)
		retStr[thisN] = mrnaSeq->dna[theStart+j];
	    else
                retStr[thisN] = codonDna->dna[j+theStart-exonStarts[i]];

	    thisN++;
	    if (thisN >= n) 
		break;
	    }

	i++;
	}
    retStr[thisN] = '\0';
       
    }
else   /*negative strand*/
    {
    retStr[n] = '\0';
    thisN = n-1;
    //move to previous block end
    i = startI-1;
    while(thisN >= 0)
	{
        struct dnaSeq *codonDna = NULL;

	if (i < 0)
	    break;

        //get dna for this exon
        codonDna = hDnaFromSeq( chrom, exonStarts[i], exonEnds[i], dnaUpper );

	theStart = exonStarts[i];
	theEnd = exonEnds[i];
	if (theStart < cdsStart)
	    theStart = cdsStart;

	if (theEnd > cdsEnd)
	    theEnd = cdsEnd;
           
	for (j=0; j<(theEnd - theStart); j++)
	    {
	    if (mrnaSeq != NULL)
		retStr[thisN] = mrnaSeq->dna[theEnd-j-1];
	    else
                retStr[thisN] = codonDna->dna[theEnd-j-1-exonStarts[i]];

	    thisN--;
	    if (thisN < 0) 
		break;
	    }
	i--;
	}
    }
}



static void maskDiffString( char *retStr, char *s1, char *s2, char mask )
/*copies s1, masking off similar characters, and returns result into retStr.
 *if strings are of different size it stops after s1 is done.*/
{
int i;
for (i=0; i<strlen(s1); i++)
    {
    if (s1[i] == s2[i])
	retStr[i] = mask;
    else 
	retStr[i] = s1[i];

    }
retStr[i] = '\0';
}

Color lighterShade(struct vGfx *vg, Color color, double percentLess)
    /* Get lighter shade of a color, with a variable level */ 
{
    struct rgbColor rgbColor =  vgColorIxToRgb(vg, color); 
    rgbColor.r = (int)((rgbColor.r+127)/percentLess);
    rgbColor.g = (int)((rgbColor.g+127)/percentLess);
    rgbColor.b = (int)((rgbColor.b+127)/percentLess);
    return vgFindColorIx(vg, rgbColor.r, rgbColor.g, rgbColor.b);
}



static Color colorFromGrayIx(struct vGfx *vg, char *codon, int grayIx, Color 
        *cdsColor, Color *trackColors, int lfColorIdx, Color ixColor)
/*convert grayIx value to color and codon which
 * are both encoded in the grayIx*/
{
Color color;
if (grayIx == -2)
    {
    color = cdsColor[CDS_ERROR];
    sprintf(codon,"X");
    }
else if (grayIx == -1)
    {
    color = cdsColor[CDS_START];
    sprintf(codon,"M");
    }
else if (grayIx == -3)
    {
    color = cdsColor[CDS_STOP];
    sprintf(codon,"*");
    }
else if (grayIx <= 26)
    {
    color = ixColor;
    sprintf(codon,"%c",grayIx + 'A' - 1);
    }
else if (grayIx > 26)
    {
    color = lighterShade(vg, ixColor,1.5);
    sprintf(codon,"%c",grayIx - 26 + 'A' - 1);
    }
else
    {
    color = cdsColor[CDS_ERROR];
    sprintf(codon,"X");
    }
return color;
}


static int setColorByCds(DNA *dna, int codon, boolean *foundStart, 
			 boolean reverse)
{
char codonChar;

if (reverse)
    reverseComplement(dna,strlen(dna));
codonChar = lookupCodon(dna);
if (codonChar == 0)
    return(-3);    //stop codon
else if (codonChar == 'X')
    return(-2);     //bad input to lookupCodon
else if (codonChar == 'M' && !(*foundStart))
    {
    *foundStart = TRUE;
    return(-1);     //start codon
    }
else if (codon == 0)
    return(codonChar - 'A' + 1);       //odd color
else
    return(codonChar - 'A' + 1 + 26);  //even color
}


static void mustGetMrnaStartStop( char *acc, unsigned *cdsStart, 
				  unsigned *cdsEnd )

/* Get cds start and stop from genbank, if available. Otherwise it
 * returns cdsStart == cdsEnd. */
{
char query[256];
struct sqlConnection *conn;
struct sqlResult *sr;
char **row;

conn = hAllocConn();
sprintf(query, "select cds from mrna where acc = '%s'", acc);
sr = sqlGetResult(conn, query);
if((row = sqlNextRow(sr)) == NULL)
    errAbort(
    "cds.c: mushGetMrnaStartStop() - Cannot get cds for %s from table \"mrna\"\n", acc);
sprintf(query, "select name from cds where id = '%d'", atoi(row[0]));
sqlFreeResult(&sr);
sr = sqlGetResult(conn, query);
if((row = sqlNextRow(sr)) == NULL)
    errAbort(
    "cds.c: mushGetMrnaStartStop() - Cannot get cds for %s from table \"cds\"\n", acc);
genbankParseCds(row[0], cdsStart, cdsEnd);
sqlFreeResult(&sr);
hFreeConn(&conn);
}

static void getHiddenGaps(struct psl *psl, unsigned *gaps)
/*return the gaps in the query sequence of a psl that are 'hidden' 
  in the browser. This lets these insertions be indicated with the
  color orange.*/
{
int i;
gaps[0] = psl->qStarts[0] - psl->qStart;
for (i=1; i<psl->blockCount; i++)
    {
    gaps[i] = psl->qStarts[i] - 
	(psl->qStarts[i-1] + psl->blockSizes[i-1]);
    }
gaps[psl->blockCount] = psl->qSize - 
    (psl->qStarts[psl->blockCount-1] + psl->blockSizes[psl->blockCount-1]);
}




static struct simpleFeature *splitPslByCodon(char *chrom, 
					     struct linkedFeatures *lf, struct 
                                             psl *psl, int sizeMul, boolean 
                                             isXeno, int maxShade, 
                                             int displayOption)
{
struct simpleFeature *sfList = NULL, *sf;
struct genePred *gp = NULL;
unsigned cdsStart, cdsEnd;
unsigned *retGaps = NULL;
boolean extraInfo = (displayOption != CDS_DRAW_GENOMIC_CODONS);

struct genbankCds cds;

/*get CDS from genBank*/
mustGetMrnaStartStop(psl->qName, &cdsStart, &cdsEnd);
cds.start = cdsStart;
cds.end = cdsEnd;
if(extraInfo)
        gp = genePredFromPsl2(psl, 12, &cds, -1);
else
        gp = genePredFromPsl2(psl, 12, &cds, 0);

/*cds not in genbank - revert to normal*/
if (gp->cdsStartStat != cdsComplete || gp->cdsEndStat != cdsComplete)
{
    int grayIx = pslGrayIx(psl, isXeno, maxShade);
    sfList = sfFromPslX(psl, grayIx, sizeMul);
}
else
    {
    lf->tallStart = gp->cdsStart;
    lf->tallEnd = gp->cdsEnd;
    sfList = splitGenePredByCodon(chrom, lf, gp, retGaps, extraInfo);
    }

return(sfList);
}



void lfSplitByCodonFromPslX(char *chromName, struct linkedFeatures *lf, 
			    struct psl *psl, int sizeMul, boolean
                            isXeno, int maxShade, int displayOption)
/*divide a pslX record into a linkedFeature, where each simple feature
 * is a 3-base codon (or a partial codon if on a boundary). Uses
 * GenBank to get the CDS start/stop of the psl record and also grabs
 * the sequence. This takes care of hidden gaps in the alignment, that
 * alter the frame. Therefore this function relies on the mRNA
 * sequence (rather than the genomic) to determine the frame.*/
{
struct simpleFeature *sfList = splitPslByCodon(chromName, lf, psl, sizeMul,
                                               isXeno, maxShade, displayOption);
slReverse(&sfList);
lf->components = sfList;
}


int getCdsDrawOptionNum(struct track *tg)
    /*query the cart for the current track's CDS coloring option. See
     * cdsColors.h for return value meanings*/
{
    char *defaultDrawOption;
    char optionStr[128];
    if(tg == NULL) return(0);
    if (tg->tdb != NULL)
        defaultDrawOption = trackDbSettingOrDefault(tg->tdb, "cdsDrawDefault", 
                                                    CDS_DRAW_DEFAULT);
    else
        defaultDrawOption = CDS_DRAW_DEFAULT;
    safef(optionStr, 128,"%s.cds.draw", tg->mapName);
    return(cdsColorStringToEnum(cartUsualString(cart, optionStr,
                                                defaultDrawOption)));
}


static struct psl *genePredLookupPsl(char *db, char *chrom, 
        struct linkedFeatures* lf, char *pslTable ) 
/*get the psl entry associated with this genePred entry (NOT a
  conversion, must be in pslTable. Complains if psl cannot be
  found but it doesn't abort.*/
{
    char rest[64];
    char **row;
    int rowOffset;
    struct psl *psl = NULL;
    struct sqlResult *sr = NULL;
    struct sqlConnection *conn2 = sqlConnect(db);

    safef(rest, 64, "qName='%s'", lf->name );
    sr = hRangeQuery(conn2, pslTable, chromName, lf->start, lf->end, 
            rest, &rowOffset);
    if ((row = sqlNextRow(sr)) != NULL)
        psl = pslLoad(row+rowOffset);
    sqlFreeResult(&sr);
    sqlDisconnect(&conn2);

    if (psl == NULL)
        uglyf("Cannot Find psl for %s\n<br>", lf->name );
    return psl;
}


static struct dnaSeq *mustGetSeqUpper(char *name, char *tableName)
{
    struct dnaSeq *mrnaSeq = NULL;
    if (sameString(tableName,"refGene"))
        mrnaSeq = hGenBankGetMrna(name, "refMrna");
    else
        mrnaSeq = hGenBankGetMrna(name, NULL);

    if (mrnaSeq == NULL)
        printf("Cannot find refGene mRNA sequence for %s<br>\n", name );
    touppers(mrnaSeq->dna);
    return mrnaSeq;
}



static void makeCdsShades(struct vGfx *vg, Color *cdsColor )
{
    cdsColor[CDS_ERROR] = vgFindColorIx(vg,0,0,0); 
    cdsColor[CDS_ODD] = vgFindColorIx(vg,CDS_ODD_R,CDS_ODD_G,CDS_ODD_B);
    cdsColor[CDS_EVEN] = vgFindColorIx(vg,CDS_EVEN_R,CDS_EVEN_G,CDS_EVEN_B);
    cdsColor[CDS_START] = vgFindColorIx(vg,CDS_START_R,CDS_START_G,CDS_START_B);
    cdsColor[CDS_STOP] = vgFindColorIx(vg,CDS_STOP_R,CDS_STOP_G,CDS_STOP_B);
    cdsColor[CDS_SPLICE] = vgFindColorIx(vg,CDS_SPLICE_R,CDS_SPLICE_G,
                                         CDS_SPLICE_B);
    cdsColor[CDS_PARTIAL_CODON] = vgFindColorIx(vg,CDS_PARTIAL_CODON_R,
                                                CDS_PARTIAL_CODON_G, 
                                                CDS_PARTIAL_CODON_B);
    cdsColor[CDS_GENOMIC_INSERTION] = vgFindColorIx(vg,CDS_GENOMIC_INSERTION_R, 
                                                       CDS_GENOMIC_INSERTION_G,
                                                       CDS_GENOMIC_INSERTION_B);
}



static void updatePartialCodon(char *retStr, char *chrom, int start,
        int end, boolean reverse, struct dnaSeq *codonDna, int base)
{
    char tmpStr[5];
    char tmpDna[5];

    snprintf(tmpDna, min(3-strlen(retStr)+1,abs(end-start+1)), "%s",
            &codonDna->dna[start-base]);
    if (!reverse)
        safef(tmpStr, 4, "%s%s", retStr, tmpDna);
    else
        safef(tmpStr, 4, "%s%s", tmpDna, retStr);

    strncpy( retStr, tmpStr, 4 );
}


static struct simpleFeature *splitByCodon( char *chrom, 
        struct linkedFeatures *lf, 
        unsigned *starts, unsigned *ends, 
        int blockCount, unsigned
        cdsStart, unsigned cdsEnd,
        unsigned *gaps, int *exonFrames)
{
    int codon = 0;
    int frame = 0;
    int currentStart = 0, currentEnd = 0;
    struct dnaSeq *codonDna = NULL;
    char partialCodonSeq[4];
    char theRestOfCodon[4];
    char tempCodonSeq[4];
    int currentSize, base;
    int i;

    boolean foundStart = FALSE;
    struct simpleFeature *sfList = NULL, *sf = NULL;
    struct simpleFeature *tmp = NULL;

    int i0, iN, iInc;
    boolean posStrand;
    partialCodonSeq[0] = '\0';

    if (lf->orientation > 0) //positive strand
    {
        i0 = 0; iN = blockCount; iInc = 1;
        posStrand = TRUE;
    }
    else
    {
        i0 = blockCount-1; iN=-1; iInc = -1;
        posStrand = FALSE;
    }

    for (i=i0; (iInc*i)<(iInc*iN); i=i+iInc)
    {
        unsigned thisStart, thisEnd, cdsLine;
        if(exonFrames != NULL)
        {
            if(exonFrames[i] > 0)
                frame = 3 - exonFrames[i];
            else
                frame = 0;
        }

        if(frame == 0)
            strcpy(partialCodonSeq,"");

        thisStart = starts[i];
        thisEnd = ends[i];
        cdsLine = posStrand?cdsStart:cdsEnd;

        // 3' block or 5' block
        if ((thisStart < cdsStart && thisEnd <= cdsStart) ||
                (thisStart >= cdsEnd && thisEnd > cdsEnd))
        {
            AllocVar(sf);
            sf->start = thisStart;
            sf->end = thisEnd; 
            slAddHead(&sfList, sf);
            continue;
        }
        //3' to coding block
        else if (thisEnd > cdsLine && thisStart < cdsLine)
        {
            AllocVar(sf);
            if(posStrand)
            {
                sf->start = thisStart;
                sf->end = cdsStart;
                currentStart = cdsStart;
            }
            else
            {
                sf->start = cdsEnd;
                sf->end = thisEnd;
                currentEnd = cdsEnd;
            }
            slAddHead(&sfList, sf);
        }
        else
            if(posStrand)
                currentStart = thisStart;
            else 
                currentEnd = thisEnd;


        /*get dna for entire block. this is faster than
          getting it for each codon, but suprisingly not faster
          than getting it for each linked feature*/
        codonDna = hDnaFromSeq( chrom, thisStart, thisEnd, dnaUpper );
        base = thisStart;

        //break each block by codon and set color code to denote codon
        while (TRUE)
        {
            int codonInc = frame;
            if (frame == 0)  
            {
                codon++; codon %= 2; codonInc = 3;
            }
            
            if(posStrand)
                currentEnd = currentStart + codonInc;
            else
                currentStart = currentEnd - codonInc; 

            AllocVar(sf);
            sf->start = currentStart;
            sf->end = currentEnd;

            //we've gone off the end of the current block
            if ((posStrand && currentEnd > thisEnd) ||
                    (!posStrand && currentStart < thisStart ))
            {

                if (posStrand)
                {
                    frame = currentEnd - thisEnd;
                    sf->end = currentEnd = thisEnd;
                }
                else
                {
                    frame = thisStart - currentStart;
                    sf->start = currentStart = thisStart;
                }

                if(frame == 3) 
                {
                    frame = 0;
                    break;
                }

                /*accumulate partial codon in case of 
                  one base exon, or start a new one.*/
                updatePartialCodon(partialCodonSeq, chrom, sf->start,
                                sf->end, !posStrand, codonDna, base);

                /*get next 'frame' nt's to see what codon will be 
                  (skipping intron sequence)*/
                getCodonDna(theRestOfCodon, chrom, frame, starts, ends, 
                        blockCount, cdsStart, cdsEnd, i, !posStrand, NULL);

                if(posStrand)
                    safef(tempCodonSeq, 4, "%s%s", partialCodonSeq, 
                            theRestOfCodon);
                else
                    safef(tempCodonSeq, 4, "%s%s", theRestOfCodon, 
                            partialCodonSeq );

                sf->grayIx = ((posStrand && currentEnd <= cdsEnd) || 
                             (!posStrand && currentStart >= cdsStart))?
                             setColorByCds( tempCodonSeq,codon,&foundStart, 
                             !posStrand):-2;
                slAddHead(&sfList, sf);
                break;
            }

            currentSize = sf->end - sf->start;
            /*inside a coding block (with 3 bases)*/
            if (currentSize == 3)
            {
                char currentCodon[5];
                char *thisDna = &codonDna->dna[sf->start - base];
                snprintf(currentCodon, currentSize+1, "%s", thisDna);
                sf->grayIx = ((posStrand && currentEnd <= cdsEnd) || 
                             (!posStrand && currentStart >= cdsStart))?
                             setColorByCds(currentCodon,codon,&foundStart, 
                                     !posStrand):-2;
            }
            /*start of a coding block with less than 3 bases*/
            else if (currentSize < 3)
            {
                updatePartialCodon(partialCodonSeq,chrom,sf->start,
                        sf->end,!posStrand,codonDna, base);
                if (strlen(partialCodonSeq) == 3) 
                    sf->grayIx = setColorByCds(partialCodonSeq,codon,
                            &foundStart, !posStrand);
                else
                    sf->grayIx = -2;
                strcpy(partialCodonSeq,"" );

                /*update frame based on bases appended*/
                frame -= (sf->end - sf->start);     
            }
            else
                errAbort("%s: Too much dna (%d,%s)<br>\n", lf->name, 
                        codonDna->size, codonDna->dna );

            slAddHead(&sfList, sf);
            if(posStrand)
                currentStart = currentEnd;
            else
                currentEnd = currentStart;
        }
    }

    if(posStrand)
        slReverse(&sfList);
    return(sfList);
}

struct simpleFeature *splitGenePredByCodon( char *chrom, struct linkedFeatures 
        *lf, struct genePred *gp, unsigned *gaps, boolean extraInfo)
/*divide a genePred record into a linkedFeature, where each simple
  feature is a 3-base codon (or a partial codon if on a gap boundary).
  It starts at the cdsStarts position on the genome and goes to 
  cdsEnd. It only relies on the genomic sequence to determine the
  frame so it works with any gene prediction track*/
{
    if(extraInfo)
        return(splitByCodon(chrom,lf,gp->exonStarts,gp->exonEnds,gp->exonCount,
                    gp->cdsStart,gp->cdsEnd,gaps,gp->exonFrames));
    else
        return(splitByCodon(chrom,lf,gp->exonStarts,gp->exonEnds,gp->exonCount,
                    gp->cdsStart,gp->cdsEnd,gaps,NULL));
}


static void drawDiffTextBox(struct vGfx *vg, int xOff, int y, 
        double scale, int heightPer, MgFont *font, Color color, 
        char *chrom, unsigned s, unsigned e, struct psl *psl, 
        struct dnaSeq *mrnaSeq, struct linkedFeatures *lf, Color *cdsColor,
        int grayIx, boolean *foundStart, int displayOption, int
        maxPixels, Color *trackColors, Color ixColor)
{
struct dyString *ds = newDyString(256);
struct dyString *ds2 = newDyString(256);
char *retStrDy = NULL; 
int mrnaS;
int startI = -1;
int size, i;
char retStr[4];
unsigned *ends = needMem(sizeof(unsigned)*psl->blockCount);
static char saveStr[4];
char tempStr[4];
char codon[4];
char mrnaCodon[4]; 
int genomicColor;
Color textColor = whiteIndex();
boolean isDiff = (displayOption == CDS_DRAW_DIFF_CODONS ||
                  displayOption == CDS_DRAW_DIFF_BASES );

if (isDiff)
        dyStringAppend( ds, (char*)hDnaFromSeq(chromName,s,e,dnaUpper)->dna);
mrnaS = convertCoordUsingPsl( s, psl ); 
if(mrnaS >= 0)
    {
    boolean genomicInsertion = FALSE;
    dyStringAppendN( ds2, (char*)&mrnaSeq->dna[mrnaS], e-s );
    if(isDiff)
        retStrDy = needMem(sizeof(char)*(ds->stringSize+1));

    //get rest of mrna bases if we are on a block boundary
    //so we can determine if codons are different
    size = e-s;
    if(size < 3)
	{

        if( mrnaSeq != NULL && mrnaS-(3-size)>0 )
        {

            int idx = -1;
            int newIdx = -1;
            int *gaps = NULL;
            boolean appendAtStart = FALSE;
            AllocArray(gaps, psl->blockCount+2);

            for(i=0; i<psl->blockCount; i++)
                {
                unsigned tStart = psl->tStarts[i];
                unsigned tEnd = tStart + psl->blockSizes[i];
                if (psl->strand[1] == '-') 
                        reverseIntRange(&tStart, &tEnd, psl->tSize);

                if (s == tStart)
                        {
                        idx = i;
                        appendAtStart = TRUE;
                        break;
                        }
                else if (e == tEnd)
                        {
                        idx = i+1;
                        appendAtStart = FALSE;
                        break;
                        }
                }

            getHiddenGaps( psl, gaps);
            if(idx >= 0 && gaps[idx] > 0)
                genomicInsertion = TRUE;

            if (!appendAtStart)
            {
                newIdx = mrnaS+(3-size)-1;
	        snprintf(saveStr,4,"%s%s", ds2->string, &mrnaSeq->dna[newIdx]);
            }
            else
            {
                newIdx = mrnaS-(3-size);
	        snprintf(saveStr,4,"%s%s", &mrnaSeq->dna[newIdx], ds2->string);
            }
        }
	strncpy(tempStr,saveStr,4);
        }
    else
	strncpy(tempStr,ds2->string,4);

    if(isDiff)
        maskDiffString(retStrDy, ds2->string, ds->string,  ' ' );

    if (e <= lf->tallEnd)
	{

	int mrnaGrayIx;
        boolean trueBool = TRUE;
        boolean *nullFoundStart = &trueBool;
        boolean startColor = FALSE;

	//compute mrna codon and get genomic codon
	//by decoding grayIx.
	if (lf->orientation == -1)
	    reverseComplement(tempStr,strlen(tempStr));

        if (isDiff)
	        genomicColor = colorFromGrayIx( vg, codon, grayIx,
                        cdsColor, trackColors, lf->grayIx, ixColor );

	//re-set color of this block based on mrna seq rather tha
	//than genomic, but keep the odd/even cycle of dark/light
	//blue.
        mrnaGrayIx = setColorByCds(tempStr,(grayIx > 26),nullFoundStart, FALSE);
        if(color == cdsColor[CDS_START])
                startColor = TRUE;
	color = colorFromGrayIx(vg, mrnaCodon, mrnaGrayIx, cdsColor,
                trackColors, lf->grayIx, ixColor);
        if(startColor && sameString(mrnaCodon,"M"))
                color = cdsColor[CDS_START];
        if(genomicInsertion)
                textColor = color = cdsColor[CDS_GENOMIC_INSERTION];
	}


    if (displayOption == CDS_DRAW_MRNA_BASES)
	drawScaledBoxSampleWithText(vg, s, e, scale, xOff, y, heightPer, 
				    color, lf->score, font, ds2->string,
				    zoomedToBaseLevel, TRUE, cdsColor, textColor,
				    winStart, maxPixels );
    else if (displayOption == CDS_DRAW_MRNA_CODONS)
	drawScaledBoxSampleWithText(vg, s, e, scale, xOff, y, heightPer, 
				    color, lf->score, font, mrnaCodon,
				    zoomedToCodonLevel, TRUE, cdsColor, textColor,
				    winStart, maxPixels );
    else if (displayOption == CDS_DRAW_DIFF_BASES)
	drawScaledBoxSampleWithText(vg, s, e, scale, xOff, y, heightPer, 
				    color, lf->score, font, retStrDy, zoomedToBaseLevel,
				    TRUE, cdsColor, textColor, winStart, maxPixels );
 
    else if (displayOption == CDS_DRAW_DIFF_CODONS)
	{
	if (mrnaCodon[0] != codon[0])
	    drawScaledBoxSampleWithText(vg, s, e, scale, xOff, y, 
					heightPer, color, lf->score, font, mrnaCodon,
					zoomedToCodonLevel, TRUE, cdsColor, textColor,
					winStart, maxPixels );
	else
	    drawScaledBoxSample(vg, s, e, scale, xOff, y, heightPer, 
				color, lf->score );
	}
    else
        errAbort("Unknown displayOption: %s<br>\n", displayOption);

    if(isDiff)        
        freeMem(retStrDy);
    freeMem(ends);
    }
else
    {
    /*show we have an error by coloring entire exon block yellow*/
    drawScaledBoxSample(vg, s, e, scale, xOff, y, heightPer, 
			MG_YELLOW, lf->score );
    }

dyStringFree(&ds);
dyStringFree(&ds2);
}

void drawCdsColoredBox(struct track *tg,  struct linkedFeatures *lf, 
		       int grayIx, Color *cdsColor, struct vGfx *vg, int xOff, int y, 
		       double scale, MgFont *font, int s, int e, int heightPer, 
		       boolean zoomedToCodonLevel, struct dnaSeq *mrnaSeq, struct psl *psl, 
		       int drawOptionNum, boolean errorColor, boolean *foundStart, 
		       int maxPixels, int winStart, Color originalColor)
/*draw a box that is colored by the bases inside it and its
 * orientation. Stop codons are red, start are green, otherwise they
 * alternate light/dark blue colors. These are defined in
 * cdsColors.h*/
{

boolean isGenomic = (drawOptionNum == CDS_DRAW_GENOMIC_CODONS);
char codon[2] = " ";
Color color = colorFromGrayIx(vg, codon, grayIx, cdsColor,
        tg->colorShades, lf->grayIx, originalColor);

if (drawOptionNum == CDS_DRAW_GENOMIC_CODONS)
    /*any track in the genes category, with the genomic
     * coloring turned on in the options menu*/
    drawScaledBoxSampleWithText(vg, s, e, scale, xOff, y, heightPer, 
				color, lf->score, font, codon, zoomedToCodonLevel, FALSE,
				cdsColor, whiteIndex(), winStart, maxPixels );
else if (sameString(tg->mapName,"mrna") || 
	 sameString(tg->mapName,"xenoMrna")||
         sameString(tg->mapName,"mrnaBlastz"))
    /*one of the chosen psl tracks with associated
     * sequences in a database*/

    if (errorColor == TRUE)
	/*make it yellow to show we have a problem*/
	drawScaledBoxSample(vg, s, e, scale, xOff, y, 
                            heightPer, MG_YELLOW, lf->score );
    else if (mrnaSeq != NULL && psl != NULL)
	drawDiffTextBox(vg, xOff, y, scale, heightPer, font, 
			color, chromName, s, e, psl, mrnaSeq, lf, cdsColor,
			grayIx, foundStart, drawOptionNum, maxPixels,
                        tg->colorShades, originalColor);
    else
	/*revert to normal coloring*/
	drawScaledBoxSample(vg, s, e, scale, xOff, y,
                            heightPer, color, lf->score );
else
    /*again revert to normal coloring*/
    drawScaledBoxSample(vg, s, e, scale, xOff, y, heightPer, 
			color, lf->score );
}


int cdsColorSetup(struct vGfx *vg, struct track *tg, Color *cdsColor, 
		  struct dnaSeq **mrnaSeq, struct psl **psl, boolean *errorColor, 
		  struct linkedFeatures *lf, boolean cdsColorsMade)
/*gets the CDS coloring option, allocates colors, and returns the
 * sequence and psl record for the given lf->name (only returns
 * sequence and psl for mRNA, EST, or xenoMrna.
 * returns drawOptionNum, mrnaSeq, psl, errorColor*/
{
//get coloring options
int drawOptionNum;
drawOptionNum = getCdsDrawOptionNum(tg);
if(drawOptionNum == 0) return(0);

/*allocate colors for coding coloring*/
if (!cdsColorsMade)
    { 
    makeCdsShades(vg,cdsColor);
    cdsColorsMade = TRUE;
    }

   
if(drawOptionNum>0 && zoomedToCodonLevel)
    {
    if (sameString(tg->mapName,"mrna")   ||
	sameString(tg->mapName,"xenoMrna") ||
        sameString(tg->mapName,"mrnaBlastz"))
	{
        char *database = cartUsualString(cart, "db", hGetDb());
	*mrnaSeq = mustGetSeqUpper(lf->name,tg->mapName);
	*psl = genePredLookupPsl(database, chromName, lf, tg->mapName);
	if (*mrnaSeq != NULL && *psl != NULL)
                {
                if ((*psl)->strand[0] == '-' || (*psl)->strand[1] == '-')
	                reverseComplement((*mrnaSeq)->dna,strlen((*mrnaSeq)->dna));
                }
        else
                return(0);

	}
    }

return(drawOptionNum);
}





