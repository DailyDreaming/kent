# modified to create grpMgc local groups

DROP TABLE IF EXISTS grpMgc;
CREATE TABLE grpMgc (
    name varchar(255) not null,	# Group name.  Connects with trackDb.grp
    label varchar(255) not null,	# Label to display to user
    priority float not null,	# 0 is top
              #Indices
    PRIMARY KEY(name)
);


INSERT grpMgc VALUES("mgc", "Mammalian Gene Collection Tracks", 0.5);
