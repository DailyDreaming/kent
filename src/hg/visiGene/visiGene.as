
table fileLocation
"Location of image, typically a file directory"
    (
    int id;	"ID of location"
    lstring name;	"Directory path usually"
    )

table strain
"Name of strain (eg C57BL for a mouse)"
    (
    int id;	"ID of strain"
    int taxon;	"NCBI taxon of organism"
    string name;	"Name of strain"
    )


table bodyPart
"Brain, eye, kidney, etc.  Use 'whole' for whole body"
    (
    int id;	"ID of body part"
    string name;	"Name of body part"
    )

table sliceType
"Horizontal, coronal, whole mount, etc."
    (
    int id;	"ID of section"
    string name;	"Name of horizontal/whole mount, etc"
    )

table fixation
"Fixation conditions - 3% formaldehyde or the like"
    (
    int id;	"ID of fixation"
    string description;	"Text string describing fixation"
    )

table embedding
"Embedding media for slices - paraffin, etc."
    (
    int id;	"ID of embedding"
    string description; "Text string describing embedding"
    )

table permeablization
"Permeablization conditions"
    (
    int id;	"ID of treatment"
    string description;	"Text string describing conditions"
    )

table contributor
"Info on contributor"
    (
    int id;	"ID of contributor"
    string name;	"Name in format like Kent W.J."
    )

table journal
"Information on a journal"
    (
    int id;	"ID of journal"
    string name;	"Name of journal"
    string url;	"Journal's main url"
    )

table copyright
"Copyright information"
    (
    int id;	"ID of copyright"
    lstring notice;	"Text of copyright notice."
    )

table submissionSet
"Info on a batch of images submitted at once"
    (
    int id;			"ID of submission set"
    string name;		"Name of submission set"
    lstring contributors;	"Comma separated list of contributors in format Kent W.J., Wu F.Y."
    lstring publication;	"Name of publication"
    lstring pubUrl;		"Publication URL"
    int journal;		"Journal for publication"
    int copyright;		"Copyright notice"
    lstring setUrl;		"URL for whole set"
    lstring itemUrl;		"URL for item.  Put %s where image.submitId should go"
    )

table submissionContributor
"Association between contributors and submissionSets"
    (
    int submissionSet;	"ID in submissionSet table"
    int contributor;	"ID in contributor table"
    )

table sectionSet
"Info on a bunch of sections through same sample"
    (
    int id;	"Section id"
    )

table antibody
"Information on an antibody"
    (
    int id;	"Antibody ID"
    string name;	"Name of antibody"
    lstring description;	"Description of antibody"
    int taxon;		"NCBI Taxon of animal antibody is from"
    )

table gene
"Info on a gene"
    (
    int id;		"ID of gene"
    string name;	"Gene symbol (HUGO if available)"
    string locusLink;	"NCBI locus link ID or blank if none"
    string refSeq;	"RefSeq ID or blank if none"
    string genbank;	"Genbank/EMBL accession or blank if none"
    string uniProt;	"SwissProt/Uniprot accession or blank if none"
    int taxon;		"NCBI taxon ID of organism"
    )

table geneSynonym
"A synonym for a gene"
    (
    int gene;	"ID in gene table"
    string name;	"Synonymous name for gene"
    )

table allele
"Name of a gene allele"
    (
    int id;	"ID of allele"
    int gene;	"ID of gene"
    string name;	"Allele name, + for wild type"
    )

table genotype
"How different from wild type.  Associated with genotypeAllele table"
    (
    int id;	"Genotype id"
    int taxon;	"NCBI Taxon of organism"
    int strain;	"Strain of organism"
    lstring alleles; "Comma separated list of gene:allele in alphabetical order"
    )

table genotypeAllele
"Association between genotype and alleles"
    (
    int genotype;	"Associated genotype"
    int allele;		"Associated allele"
    )

table sex
"Sex of a specimen"
    (
    int id;	"Sex ID"
    string name;	"Name of sex - male, female, hermaphrodite, mixed"
    )

table specimen
"A biological specimen - something mounted, possibly sliced up"
    (
    int id;	  "Specimen ID"
    string name;	"Name of specimen, frequently blank"
    int taxon;		"NCBI Taxon of organism"
    int genotype; 	"Genotype of specimen"
    int bodyPart;	"Body part of specimen"
    int sex;		"Sex - male, female or hermaphrodite"
    float age;		"Age in days since conception"
    float minAge;	"Minimum age"
    float maxAge;	"Maximum age - may differ from minAge if uncertain of age"
    lstring notes;	"Any notes on specimen"
    )

table preparation
"How a specimen is prepared"
    (
    int id;	"Preparation ID"
    int fixation;	"How fixed"
    int embedding;	"How embedded"
    int permeablization; "How permeablized"
    int sliceType;	"How it was sliced"
    lstring notes;	"Any other notes on preparation"
    )

table probeType
"Type of probe - RNA, antibody, etc."
    (
    int id;		"ID of probe type"
    string name;	"Name of probe type"
    )

table probe
"Info on a probe"
    (
    int id;		"ID of probe"
    int gene;		"Associated gene if any"
    int antibody;	"Associated antibody if any"
    int probeType;	"Type of probe - antibody, RNA, etc.."
    string fPrimer;	"Forward PCR primer if any"
    string rPrimer;	"Reverse PCR primer if any"
    lstring seq;	"Associated sequence if any"
    )

table probeColor
"Color - what color probe is in"
    (
    int id;		"Id of color"
    string name;	"Color name"
    )

table imageFile
"A biological image file"
    (
    int id;		"ID of imageFile"
    string fileName;	"Image file name not including directory"
    float priority;	"Lower priorities are displayed first"
    int imageWidth;     "width of image in pixels"
    int imageHeight;    "height of image in pixels"
    int fullLocation;	"Location of full-size image"
    int thumbLocation;	"Location of thumbnail-sized image"
    int submissionSet;	"Submission set this is part of"
    string submitId;	"ID within submission set"
    )

table image
"An image.  There may be multiple images within an imageFile"
    (
    int id;		"ID of image"
    int submissionSet;	"Submission set this is part of"
    int imageFile;	"ID of image file"
    int imagePos;	"Position in image file, starting with 0"
    string paneLabel;   "Label of this pane in image file"
    int sectionSet;	"Set of sections this is part of or 0 if none"
    int sectionIx;	"Position (0 based) within set of sections"
    int specimen;	"Pointer to info on specimen"
    int preparation;	"Pointer to info on how specimen prepared"
    )

table imageProbe
"Associate probe and image"
    (
    int id;	"ID of imageProbe combination"
    int image;	"ID of image"
    int probe;	"ID of probe"
    int probeColor;	"ID of probeColor"
    )

table expressionLevel
"Annotated expression level if any"
    (
    int imageProbe;	"Image and probe"
    int bodyPart;	"Location of expression"
    float level;	"Expression level (0.0 to 1.0)"
    )

table lifeTime
"Information of ages critical points in life cycle"
    (
    int taxon;	"NCBI taxon"
    float birth;	"Typical number of days from conception to birth/hatching"
    float adult;	"Typical number of days from conception to adulthood"
    float death;	"Typical number of days from conception to death"
    )

table lifeStageScheme
"List of schemes for developmental stages"
    (
    int id;	"ID of scheme"
    int taxon;	"NCBI taxon"
    string name;	"Theiler, or whatever"
    string description; "Description of stage"
    )

table lifeStage
"List of life stages according to a particular scheme"
    (
    int lifeStageScheme; "Which staging scheme this is"
    string name;	 "Name of this stage"
    float age;		 "Start age of this stage measured in days since conception"
    )
