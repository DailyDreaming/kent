table knownGene
"Protein coding genes based on proteins from SWISS-PROT, TrEMBL, and TrEMBL-NEW and their corresponding mRNAs from GenBank"
(
string  name;               "Name of gene"
string  chrom;              "Chromosome name"
char[1] strand;             "+ or - for strand"
uint    txStart;            "Transcription start position"
uint    txEnd;              "Transcription end position"
uint    cdsStart;           "Coding region start"
uint    cdsEnd;             "Coding region end"
uint    exonCount;          "Number of exons"
uint[exonCount] exonStarts; "Exon start positions"
uint[exonCount] exonEnds;   "Exon end positions"
string  proteinID;          "SWISS-PROT ID" 
string  alignID;            "Unique identifier for each (known gene, alignment position) pair"
)

