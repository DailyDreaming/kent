/* 
checkAgpAndFa - take a .agp file and a .fa file and validate
that they are in synch.
*/

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "fa.h"
#include "agpFrag.h"
#include "agpGap.h"

static char const rcsid[] = "$Id: checkAgpAndFa.c,v 1.5 2004/02/28 02:43:50 angie Exp $";

void usage()
/* 
Explain usage and exit. 
*/
{
    fflush(stdout);
    errAbort(
      "\ncheckAgpAndFa - takes a .agp file and .fa file and ensures that they are in synch\n"
      "usage:\n\n"
      "   checkAgpAndFa in.agp in.fa\n"
      "\n");
}

boolean containsOnlyChar(char *string, int offset, int strLength, char searchChar)
/* 
Ensure that the searched string only contains a certain set char, like 'n', for example.

param string - the string to examine
param offset - the starting offset at which to begin examination
param strLength - the number of chars to examine up from the offset
param searchChar - the char to search for in the string

returns true if this string contains only the searchChar
returns false if any other char is in this string 
*/
{
int charIndex = 0;
int stringEnd = offset + strLength;

for (charIndex = offset; charIndex < stringEnd; charIndex++)
    {
    if (searchChar != string[charIndex])
	{
        printf("Bad char %c found at index %d\n", string[charIndex], charIndex);
	return FALSE;
	}
    }
printf("Contains %d Ns\n", strLength);
return TRUE;
}

boolean containsOnlyChars(char *string, int offset, int strLength, char *searchCharList, int numSearchChars)
/* 
Ensure that the searched string only contains a certain set of chars, like 'acgtACGT',
for example.

param string - the string to examine
param offset - the starting offset at which to begin examination
param strLength - the number of chars to examine up from the offset
param searchCharList - the set of chars to search for in the string
param numSearchChars - the size of the searchCharList containing the searched-for chars

returns true if this string contains only the searchChars
returns false if any other char is in this string 
*/
{
int charIndex = 0;
int currentChar = 0;
int numMismatches = 0;
int stringEnd =  offset + strLength;

for(charIndex = offset; charIndex < stringEnd; charIndex++)
    {
    numMismatches = 0;
    for (currentChar = 0; currentChar < numSearchChars; currentChar++)
        {
        if (searchCharList[currentChar] != string[charIndex])
 	    {
	    ++numMismatches;
	    }
        /* If there was not even a single match then return FALSE */
	if(numMismatches == numSearchChars)
	    {
	    printf("Bad char %c found at index %d\n", string[charIndex], charIndex);
	    return FALSE;
	    }
	}
    }

return TRUE;
}

boolean agpMatchesFaEntry(struct agpFrag *agp, int offset, char *dna, int seqSize, char *seqName)
/* 
Ensure that the metadata about the entry in the agp file agrees with the data
   in the fa file.

param *agp - pointer to incoming agp structure already containing 
agp data that *should* agree with the fasta entry.
param offset - the starting offset at which to begin validating in the dna string.
param *dna - pointer to the fasta data already populated.
param seqSize - the number of chars in the dna string to be checked, starting at the
offset.
param seqName - the name of the sequence to be checked.

return TRUE - if the agp and fasta entries agree, FALSE otherwise
*/
{
int fragSize = (agp->chromEnd - agp->chromStart);
boolean result = FALSE;

printf("In agpMatchesFaEntry()\n");

if (sameString(agp->chrom, seqName))
    {
    if (sameString(agp->type, "N"))
        {
        printf("FASTA gap entry\n");
        result =  containsOnlyChar(dna, offset, fragSize, 'n');
        }
    else 
	{
        printf("FASTA sequence entry\n");
result = TRUE; /*containsOnlyChars(dna, offset, fragSize, "acgt", 4);*/
	}
    }

printf("Returning %d from agpMatchesFaEntry()\n", result);
return result;
}

void checkAgpAndFa(char *agpFile, char *faFile)
/* 
checkAgpAndFa - read the .agp file and make sure that it agrees with the .fa file. 

param agpFile - The pathname of the agp file to check.
param faFile - The pathname of the fasta file to check.

exceptions - this function aborts if it detects an entry where the agp and fasta
files do not agree
*/
{
struct lineFile *lfAgp = lineFileOpen(agpFile, TRUE);
struct lineFile *lfFa = lineFileOpen(faFile, TRUE);
char *line = NULL;
char *words[9];
int lineSize = 0;
int wordCount = 0;
struct agpFrag *agpFrag = NULL;
struct agpGap *agpGap = NULL;
int dnaOffset = -1;
int retSize = 0;
char *retName = NULL;
DNA *retDna = NULL;

while (faSpeedReadNext(lfFa, &retDna, &retSize, &retName))
    {
    printf("\n\nAnalyzing data for Chromosome: %s, size: %d, dnaOffset = %d\n\n", retName, retSize, dnaOffset);

    if (dnaOffset >= retSize)
	{
	printf("dnaOffset >= retSize\n");
	}
    dnaOffset = 0;
    while (dnaOffset < retSize)
        {
        printf("\nLoop with dnaOffset of %d\nTotal chromosome size of %d\n", dnaOffset, retSize);
        lineFileNext(lfAgp, &line, &lineSize);

        if (line[0] == '#' || line[0] == '\n')
            {
            continue;
            }

        wordCount = chopLine(line, words);
        if (wordCount < 5)
            {
            fflush(stdout);
            errAbort("Bad line %d of %s\n", lfAgp->lineIx, lfAgp->fileName);
            }
   
        if (words[4][0] != 'N')
            {
            lineFileExpectWords(lfAgp, 9, wordCount);
            agpFrag = agpFragLoad(words);
	    // file is 1-based but agpFragLoad() now assumes 0-based:
	    agpFrag->chromStart -= 1;
	    agpFrag->fragStart  -= 1;
            agpFragOutput(agpFrag, stdout, ' ', '\n');
            if (agpFrag->chromEnd - agpFrag->chromStart != agpFrag->fragEnd - agpFrag->fragStart)
               {
               fflush(stdout);
               errAbort("1Sizes don't match in %s and %s line %d of %s\n",
                    agpFrag->chrom, agpFrag->frag, lfAgp->lineIx, lfAgp->fileName);
               }
            if (agpFrag->chromEnd - agpFrag->chromStart <= 0)
               {
               fflush(stdout);
               errAbort("Size is %d in %s and %s line %d of %s\n",
			agpFrag->chromEnd - agpFrag->chromStart,
			agpFrag->chrom, agpFrag->frag, lfAgp->lineIx,
			lfAgp->fileName);
               }
            }
        else
            {
            int gapSize = -1;
            agpGap = agpGapLoad(words);
            agpGap->chromStart--;
            gapSize = (agpGap->chromEnd - agpGap->chromStart);

            if (gapSize != agpGap->size)
               {
               fflush(stdout);
               errAbort("2Sizes don't match in %s, calculated size %d, size %d, line %d of %s\n",
                    agpGap->chrom, gapSize, agpGap->size, lfAgp->lineIx, lfAgp->fileName);
               }   
            agpFrag = (struct agpFrag*) agpGap;
            }

        if (dnaOffset != 0 && agpFrag->chromStart != dnaOffset)
            {
            fflush(stdout);
            errAbort("3Start %d, doesn't match previous end %d, line %d of %s\n", agpFrag->chromStart, dnaOffset, lfAgp->lineIx, lfAgp->fileName);
            }
   
        /* If the agp entry is ignoring gaps at the start of the chromosome 
            then we need to fake an agp entry and check the fake entry first against the
            fasta entry */
        if (0 == dnaOffset && 0 != agpFrag->chromStart)
            {
            int origStart = agpFrag->chromStart;
            int origEnd = agpFrag->chromEnd;
            char origType[2];
            agpFrag->chromStart = 0;
            agpFrag->chromEnd = origStart;	
            strcpy(origType, agpFrag->type);
	    strcpy(agpFrag->type, "N");
 
            if (!agpMatchesFaEntry(agpFrag, dnaOffset, retDna, retSize, retName))
                {
                fflush(stdout);
                errAbort("Invalid Fasta file entry\n");
                }

            agpFrag->chromStart = origStart;
            agpFrag->chromEnd = origEnd;
            strcpy(agpFrag->type, origType);
            }

        dnaOffset = agpFrag->chromStart;                

        printf("agpFrag->chromStart: %d, agpFrag->chromEnd: %d, dnaOffset: %d\n", agpFrag->chromStart, agpFrag->chromEnd, dnaOffset);
        if (!agpMatchesFaEntry(agpFrag, dnaOffset, retDna, retSize, retName))
            {
            printf("Invalid Agp or Fasta file entry for chromsome %s\n", agpFrag->chrom);
            fflush(stdout);
	    errAbort("Exiting\n");
	    }
	else 
	    {
            printf("Valid Fasta file entry\n");
	    }

        dnaOffset = agpFrag->chromEnd;
        }  
    }

printf("All AGP and FASTA entries agree - both files are valid\n");
}

int main(int argc, char *argv[])
/* 
Process command line then delegate  main work to checkAgpAndFa().
*/
{
cgiSpoof(&argc, argv);
if (argc != 3)
    usage();
checkAgpAndFa(argv[1], argv[2]);
return 0;
}
