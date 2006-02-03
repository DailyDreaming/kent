table hgMut
"track for human mutation data"
    (
    ushort  bin;            "A field to speed indexing"
    string  chrom;          "Reference sequence chromosome or scaffold"
    uint    chromStart;     "Start position in chrom"
    uint    chromEnd;       "End position in chrom"
    string  name;           "HGVS description of mutation."
    string  mutId;          "unique ID for this mutation"
    ushort  srcId;          "source ID for this mutation"
    char[1] hasPhenData;    "y or n, does this have phenotype data linked"
    string  baseChangeType; "enum('insertion', 'deletion', 'substitution','duplication','complex','unknown')."
    string  location;       "enum('intron', 'exon', '5'' UTR', '3'' UTR', 'not within known transcription unit')."
    )

table hgMutSrc
"sources for human mutation track"
    (
    ushort srcId;	    "key into hgMut table"
    string src;		    "name of genome wide source or LSDB"
    string details; 	    "for LSDB name of actual source DB"
    )

table hgMutExtLink
"accessions and sources for links"
    (
    string mutId;           "mutation ID"
    string acc;             "accession or ID used by link"
    int linkId;             "link ID, foreign key into hgMutLink"
    )

table hgMutLink 
"links for human mutation detail page"
    (
    int linkId;             "ID for this source, links to hgMutRef table."
    string linkDisplayName; "Display name for this link."
    string url;             "url to substitute ID in for links."
    )

table hgMutAlias
"aliases for mutations"
    (
    string mutId;           "mutation ID from hgMut table."
    string name;            "Another name for the mutation."
    string nameType;	    "common, or ?"
    )

table hgMutAttr
"attributes asssociated with the mutation"
    (
    string mutId;	    "mutation ID."
    int mutAttrClassId;     "id for attribute class or category, foreign key."
    int mutAttrNameId;      "id for attribute name, foreign key."
    string mutAttrVal;      "value for this attribute"
    )

table hgMutAttrClass
"classes or categories of attributes"
    (
    int mutAttrClassId;     "id for attribute class."
    string mutAttrClass;    "class"
    int displayOrder;       "order to display the classes in on the detail page."
    )

table hgMutAttrName
"Names of attributes"
    (
    int mutAttrNameId;     "id for attribute name."
    int mutAttrClassId;    "id for class this name belongs to."
    string mutAttrName;    "name"
    )
