# This creates the table holding the relationship between
# ensemble genes, transcripts, and peptides.
CREATE TABLE ensGtp (
  gene char(20) NOT NULL,
  transcript char(20) NOT NULL,
  protein char(20) NOT NULL,
# INDICES
  INDEX(gene),
  UNIQUE(transcript),
  UNIQUE(protein)
) 

