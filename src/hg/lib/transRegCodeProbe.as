table transRegCodeProbe
"CHIP/CHIP Probe and Transcription Factor Binding Info"
    (
    string chrom;	"Chromosome binding site is on"
    uint chromStart;    "Start position in chromosome"
    uint chromEnd;      "End position in chromosome"
    string name;        "Name of probe"
    uint tfCount;	"Count of bound transcription factors_conditions"
    string[tfCount] tfList;    "List of bound transcription factors_conditions"
    float[tfCount] bindVals;   "E values for factor binding (lower is better)"
    )
