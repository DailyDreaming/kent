table jaxQTL2
"Quantitative Trait Loci from Jackson Labs / Mouse Genome Informatics"
    (
    string chrom;      "chromosome"
    uint   chromStart; "Start position in chromosome"
    uint   chromEnd;   "End position in chromosome"
    string name;       "Name of item"
    uint   score;      "Score from 0-1000 (bed6 compat.)"
    char[1] strand;    "+ or - (bed6 compat.)"
    uint   thickStart; "start of thick region"
    uint   thickEnd; "start of thick region"
    string marker;     "MIT SSLP Marker w/highest correlation"
    string mgiID;      "MGI ID"
    string description; "MGI description"
    float  cMscore;    "cM position of marker associated with peak LOD score"
    )
