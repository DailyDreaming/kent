/* hgTracks - Human Genome browser main cgi script. */

// Must define IMAGEv2_UI outside of #ifndef IMAGEV2_H
//#define IMAGEv2_UI

#ifndef IMAGEV2_H
#define IMAGEV2_H

extern struct imgBox   *theImgBox;   // Make this global for now to avoid huge rewrite
extern struct image    *theOneImg;   // Make this global for now to avoid huge rewrite
extern struct imgTrack *curImgTrack; // Make this global for now to avoid huge rewrite
extern struct imgSlice *curSlice;    // Make this global for now to avoid huge rewrite
extern struct mapSet   *curMap;      // Make this global for now to avoid huge rewrite
//extern struct mapItem  *curMapItem;  // Make this global for now to avoid huge rewrite

#ifdef IMAGEv2_UI
// IMAGEv2
// The new way to do images
// Terms:
// "image box": The new version of the image on the html page.  It is a table with rows that contain tracks which are made up of parts of images.
//    "imgBox": The C struct that contains all information to render the image box on the html page.
//    "imgTbl": The HTML structure that contains individual track images.  The javascript client controls the imgTbl.
//          Thus cgi knows imgBox while html/js knows imgTbl.
// "image": A single gif.  An image may contain mutilple tracks.
//          Even as a single track and image may contain a data image, sideLabel and centerLabel
// "map" or "mapSet": The image map for providing links to items in a data image.
//          An image and map are a 1 to 1 pair and pixel coordinates are alway image relative.
// "slice" or "imgSlice": The cgi concept of the portion of an image that is sent to the html/js side.
//          Thus, if we are sending a 3X sized image, the "slice" spans the entire 3X.
//          Frequently a subset of an image, but possibly the whole image.
//          Even if the image is of a single track, it might still be cut into data image, sideLabel and centerLabel slices.
// "sliceMap": The portion of a map that belongs to a slice.  The pixel coordinates are always image relative, not sice relative.
// "portal" or "imgPortal": The html/js concept of the portion of a slice that is visible in the browser.
//          Thus, if we are sending a 3X sized data image slice, the "portal" seen in the browser spans only 1X.
//          Frequently a subset of a slice, but possibly the whole image slice.
// "imgTrack" or track image: The cgi side whole shabang for rendering one track.
//          It contains track specific information and three slices: data, sideLabel and centerLabel
//          The imgBox contains N imgTracks.  Typically an ajax/JSON request gets a single imgTrack returned.
// image box support: This can contain all the support variables that are global to the image box (like font size, box width, etc.).
//          At this point, lets not talk structure or cgi vs. html/js but leave this container as a concept that we know needs filling.
// imgBox is the top level and contains all info to draw the image in HTML
//    - contains slList of all imgTrack structs shown
// imgTrack contains all info to display one track/subtrack in the imgBox
//    - contains 3 imgSlice structs (data, centerLabel, sideLabel
// imgSlice contains all info to display a portion of an image file
//    - contains 1 image struct
// image contains all information about an image file and associated map box
//
//struct mapItem // IMAGEv2: single map item in an image map.
//    {
//    struct mapItem *next;     // slList
//    char *linkVar;            // the get variables associated with the map link
//    char *title;              // item title
//    int topLeftX;             // in pixels relative to image
//    int topLeftY;             // in pixels relative to image
//    int bottomRightX;         // in pixels relative to image
//    int bottomRightY;         // in pixels relative to image
//    };
//struct mapSet // IMAGEv2: full map for image OR partial map for slice
//    {
//    char *name;               // to point an image to a map in HTML
//    struct image *parentImg;  // points to the image this map belongs to
//    char *linkRoot;           // the common or static portion of the link for the entire image
//    struct mapItem *items;    // list of items
//    };
//struct image // IMAGEv2: single image which may have multiple imgSlices focused on it
//    {
//    struct image *next;       // slList (Not yet used)
//    char *file;               // name of file that hold the image
//    char *title;              // image wide title
//    int  width;               // in pixels
//    int  height;              // in pixels
//    struct mapSet *map;       // map assocated with this image (may be NULL)
//    };
//enum sliceType // IMAGEv2: currently just the 3
//    {
//    isUnknown=0,              // Invalid
//    isData=1,                 // Data or track slice of an image
//    isCenter=2,               // Top or centerLabel slice of an image
//    isSide=3                  // Side or leftLabel slice of an image
//    };
//struct imgSlice // IMAGEv2: the portion of an image that is displayable for one track
//    {
//    struct imgSlice *next;    // slList
//    enum sliceType type;      // Type of slice (currently only 3)
//    struct image *img;        // the actual image/gif
//    struct mapSet *map;       // A slice specific map.  It contains a subset of the img->map. Coordinates must be img relative NOT slice relative!
//    int  width;               // in pixels (differs from img->width if img contains sideLabel)
//    int  height;              // in pixels (differs from img->height if img contains centerLabel and/or multiple tracks)
//    int  offsetX;             // offset from left (when img->width > slice->width)
//    int  offsetY;             // offset from top (when img->height > slice->height)
//    };
//struct imgTrack // IMAGEv2: imageBox conatins list of displayed imageTracks
//    {
//    struct imgTrack *next;    // slList
//    struct trackDb *tdb;	    // trackDb entry (should this be struct track* entry?)
//    char *db;                 // Image for db (species) (assert imgTrack matches imgBox)
//    char *chrom;              // Image for chrom (assert imgTrack matches imgBox)
//    int  chromStart;          // Image start (absolute, not portal position)
//    int  chromEnd;            // Image end (absolute, not portal position)
//    boolean plusStrand;       // Image covers plus strand, not minus strand
//    boolean showCenterLabel;  // Initially display center label? TODO: Isn't this redundent with vis?
//    enum trackVisibility vis; // Current visibility of track image
//    struct imgSlice *slices;  // Currently there should be three slices for every track: data, centerLabel, sideLabel
//    };
//struct imgBox // IMAGEv2: imageBox conatins all the definitions to draw an image in hgTracks
//    {
//    char *db;                 // database (species)
//    char *chrom;              // chrom
//    int  chromStart;          // Image start (absolute, not portal position)
//    int  chromEnd;            // Image end (absolute, not portal position)
//    boolean plusStrand;       // imgBox currently shows plus strand, not minus strand
//    boolean showSideLabel;    // Initially display side label? (use 'plusStrand' for left/right)
//    struct image *images;     // Contains all images for the imgBox. TEMPORARY: hgTracks creates it's current one image and I'll store it here
//    struct image *bgImg;      // When track images are transparent, bgImage contains blue lines that are db coordinate granularity.
//    int  portalStart;         // initial visible portal within html image table (db coodinates) [May be obsoleted by js client]
//    int  portalEnd;           // initial visible portal within html image table (db coodinates) [May be obsoleted by js client]
//    int  portalWidth;         // in pixels (note that width in visible position within image position (db coodinates)
//    // TODO: I am certain there are more details needed
//    struct imgTrack *imgTracks; // slList of all images to display
//    };

//////// Maps
struct mapItem // IMAGEv2: single map item in an image map.
    {
    struct mapItem *next;     // slList
    char *linkVar;            // the get variables associated with the map link
    char *title;              // item title
    int topLeftX;             // in pixels relative to image
    int topLeftY;             // in pixels relative to image
    int bottomRightX;         // in pixels relative to image
    int bottomRightY;         // in pixels relative to image
    };
struct mapSet // IMAGEv2: full map for image OR partial map for slice
    {
    char *name;               // to point an image to a map in HTML
    struct image *parentImg;  // points to the image this map belongs to
    char *linkRoot;           // the common or static portion of the link for the entire image
    struct mapItem *items;    // list of items
    };
struct mapSet *mapSetStart(char *name,struct image *img,char *linkRoot);
/* Starts a map (aka mapSet) which is the seet of links and image locations used in HTML.
   Complete a map by adding items with mapItemAdd() */
struct mapSet *mapSetUpdate(struct mapSet *map,char *name,struct image *img,char *linkRoot);
/* Updates an existing map (aka mapSet) */
struct mapItem *mapSetItemAdd(struct mapSet *map,char *link,char *title,int topLeftX,int topLeftY,int bottomRightX,int bottomRightY);
/* Added a single mapItem to a growing mapSet */
void mapItemFree(struct mapItem **pItem);
/* frees all memory assocated with a single mapItem */
boolean mapItemConsistentWithImage(struct mapItem *item,struct image *img,boolean verbose);
/* Test whether a map item is consistent with the image it is supposed to be for */
boolean mapSetIsComplete(struct mapSet *map,boolean verbose);
/* Tests the completeness and consistency of this map (mapSet) */
void mapSetFree(struct mapSet **pMap);
/* frees all memory (including items) assocated with a single mapSet */

//////// Images
struct image // IMAGEv2: single image which may have multiple imgSlices focused on it
    {
    struct image *next;       // slList (Not yet used)
    char *file;               // name of file that hold the image
    char *title;              // image wide title
    int  width;               // in pixels
    int  height;              // in pixels
    struct mapSet *map;       // map assocated with this image (may be NULL)
    };
struct image *imgCreate(char *gif,char *title,int width,int height);
/* Creates a single image container.
   A map map be added with imgMapStart(),mapSetItemAdd() */
struct mapSet *imgMapStart(struct image *img,char *name,char *linkRoot);
/* Starts a map associated with an image. Map items can then be added to the returned pointer with mapSetItemAdd() */
struct mapSet *imgGetMap(struct image *img);
/* Gets the map associated with this image. Map items can then be added to the map with mapSetItemAdd() */
void imgFree(struct image **pImg);
/* frees all memory assocated with an image (including a map) */

//////// Slices
enum sliceType // IMAGEv2: currently just the 3
    {
    isUnknown=0,              // Invalid
    isData=1,                 // Data or track slice of an image
    isCenter=2,               // Top or centerLabel slice of an image
    isSide=3,                 // Side or leftLabel slice of an image
    isInvalid=4               // Invalid
    };
#define isMaxSliceTypes isInvalid
struct imgSlice // IMAGEv2: the portion of an image that is displayable for one track
    {
    struct imgSlice *next;    // slList
    enum sliceType type;      // Type of slice (currently only 3)
    struct image *parentImg;  // the actual image/gif
    char *title;              // slice wide title
    struct mapSet *map;       // A slice specific map.  It contains a subset of the img->map. Coordinates must be img relative NOT slice relative!
    int  width;               // in pixels (differs from img->width if img contains sideLabel)
    int  height;              // in pixels (differs from img->height if img contains centerLabel and/or multiple tracks)
    int  offsetX;             // offset from left (when img->width > slice->width)
    int  offsetY;             // offset from top (when img->height > slice->height)
    };
struct imgSlice *sliceCreate(enum sliceType type,struct image *img,char *title,int width,int height,int offsetX,int offsetY);
/* Creates of a slice which is a portion of an image.
   A slice specific map map be added with sliceMapStart(),mapSetItemAdd() */
struct imgSlice *sliceUpdate(struct imgSlice *slice,enum sliceType type,struct image *img,char *title,int width,int height,int offsetX,int offsetY);
/* updates an already created slice */
char *sliceTypeToString(enum sliceType type);
/* Translate enum slice type to string */
struct mapSet *sliceMapStart(struct imgSlice *slice,char *name,char *linkRoot);
/* Adds a slice specific map to a slice of an image. Map items can then be added to the returned pointer with mapSetItemAdd()*/
struct mapSet *sliceGetMap(struct imgSlice *slice,boolean sliceSpecific);
/* Gets the map associate with a slice which male be sliceSpecific or it map belong to the slices' image.
   Map items can then be added to the map returned with mapSetItemAdd() */
struct mapSet *sliceMapFindOrStart(struct imgSlice *slice,char *name,char *linkRoot);
/* Finds the slice specific map or starts it */
struct mapSet *sliceMapUpdateOrStart(struct imgSlice *slice,char *name,char *linkRoot);
/* Updates the slice specific map or starts it */
boolean sliceIsConsistent(struct imgSlice *slice,boolean verbose);
/* Test whether the slice and it's associated image and map are consistent with each other */
void sliceFree(struct imgSlice **pSlice);
/* frees all memory assocated with a slice (not including the image or a map belonging to the image) */

//////// imgTracks
struct imgTrack // IMAGEv2: imageBox conatins list of displayed imageTracks
    {
    struct imgTrack *next;    // slList
    struct trackDb *tdb;	  // trackDb entry (should this be struct track* entry?)
    char *name;	              // It is possible to have an imgTrack without a tdb, but then it mist have a name
    char *db;                 // Image for db (species) (assert imgTrack matches imgBox)
    char *chrom;              // Image for chrom (assert imgTrack matches imgBox)
    int  chromStart;          // Image start (absolute, not portal position)
    int  chromEnd;            // Image end (absolute, not portal position)
    boolean plusStrand;       // Image covers plus strand, not minus strand
    boolean showCenterLabel;  // Initially display center label? TODO: Isn't this redundent with vis?
    boolean reorderable;      // Is this track reorderable (by drag and drop) ?
    int order;                // Image order: This keeps track of dragReorder
    enum trackVisibility vis; // Current visibility of track image
    struct imgSlice *slices;  // Currently there should be three slices for every track: data, centerLabel, sideLabel
    };
#define IMG_ANYORDER  -2
#define IMG_FIXEDPOS  -1
#define IMG_ORDER_VAR "imgOrd"
struct imgTrack *imgTrackStart(struct trackDb *tdb,char *name,char *db,char *chrom,int chromStart,int chromEnd,boolean plusStrand,boolean showCenterLabel,enum trackVisibility vis,int order);
/* Starts an image track which will contain all image slices needed to render one track
   Must completed by adding slices with imgTrackAddSlice() */
struct imgTrack *imgTrackUpdate(struct imgTrack *imgTrack,struct trackDb *tdb,char *name,char *db,char *chrom,int chromStart,int chromEnd,boolean plusStrand,boolean showCenterLabel,enum trackVisibility vis,int order);
/* Updates an already existing image track */
int imgTrackOrderCmp(const void *va, const void *vb);
/* Compare to sort on label. */
struct imgSlice *imgTrackSliceAdd(struct imgTrack *imgTrack,enum sliceType type, struct image *img,char *title,int width,int height,int offsetX,int offsetY);
/* Adds slices to an image track.  Expected are types: isData, isSide and isCenter */
struct imgSlice *imgTrackSliceGetByType(struct imgTrack *imgTrack,enum sliceType type);
/* Gets a specific slice already added to an image track.  Expected are types: isData, isSide and isCenter */
struct imgSlice *imgTrackSliceFindOrAdd(struct imgTrack *imgTrack,enum sliceType type, struct image *img,char *title,int width,int height,int offsetX,int offsetY);
/* Find the slice or adds it */
struct imgSlice *imgTrackSliceUpdateOrAdd(struct imgTrack *imgTrack,enum sliceType type, struct image *img,char *title,int width,int height,int offsetX,int offsetY);
/* Updates the slice or adds it */
struct mapSet *imgTrackGetMapByType(struct imgTrack *imgTrack,enum sliceType type);
/* Gets the map assocated with a specific slice belonging to the imgTrack */
boolean imgTrackIsComplete(struct imgTrack *imgTrack,boolean verbose);
/* Tests the completeness and consistency of this imgTrack (not including slices) */
void imgTrackFree(struct imgTrack **pImgTrack);
/* frees all memory assocated with an imgTrack (including slices) */


//////// Image Box
struct imgBox // IMAGEv2: imageBox conatins all the definitions to draw an image in hgTracks
    {
    char *db;                 // database (species)
    char *chrom;              // chrom
    int  chromStart;          // Image start (absolute, not portal position)
    int  chromEnd;            // Image end (absolute, not portal position)
    boolean plusStrand;       // imgBox currently shows plus strand, not minus strand
    boolean showSideLabel;    // Initially display side label? (use 'plusStrand' for left/right)
    struct image *images;     // Contains all images for the imgBox. TEMPORARY: hgTracks creates it's current one image and I'll store it here
    struct image *bgImg;      // When track images are transparent, bgImage contains blue lines that are db coordinate granularity.
    int  portalStart;         // initial visible portal within html image table (db coodinates) [May be obsoleted by js client]
    int  portalEnd;           // initial visible portal within html image table (db coodinates) [May be obsoleted by js client]
    int  portalWidth;         // in pixels (note that width in visible position within image position (db coodinates)
    // TODO: I am certain there are more details needed
    struct imgTrack *imgTracks; // slList of all images to display
    };

struct imgBox *imgBoxStart(char *db,char *chrom,int chromStart,int chromEnd,boolean plusStrand,boolean showSideLabel,int portalWidth);
/* Starts an imgBox which should contain all info needed to draw the hgTracks image with multiple tracks
   The image box must be completed using imgBoxImageAdd() and imgBoxTrackAdd() */
struct image *imgBoxImageAdd(struct imgBox *imgBox,char *gif,char *title,int width,int height,boolean backGround);
/* Adds an image to an imgBox.  The image may be extended with imgMapStart(),mapSetItemAdd() */
struct image *imgBoxImageFind(struct imgBox *imgBox,char *gif);
/* Finds a specific image already added to this imgBox */
struct imgTrack *imgBoxTrackAdd(struct imgBox *imgBox,struct trackDb *tdb,char *name,enum trackVisibility vis,boolean showCenterLabel,int order);
/* Adds an imgTrack to an imgBox.  The imgTrack needs to be extended with imgTrackAddSlice() */
struct imgTrack *imgBoxTrackFind(struct imgBox *imgBox,struct trackDb *tdb,char *name);
/* Finds a specific imgTrack already added to this imgBox */
struct imgTrack *imgBoxTrackFindOrAdd(struct imgBox *imgBox,struct trackDb *tdb,char *name,enum trackVisibility vis,boolean showCenterLabel,int order);
/* Find the imgTrack, or adds it if not found */
struct imgTrack *imgBoxTrackUpdateOrAdd(struct imgBox *imgBox,struct trackDb *tdb,char *name,enum trackVisibility vis,boolean showCenterLabel,int order);
/* Updates the imgTrack, or adds it if not found */
void imgBoxTracksNormalizeOrder(struct imgBox *imgBox);
/* This routine sorts the imgTracks then forces tight ordering, so new tracks wil go to the end */
boolean imgBoxIsComplete(struct imgBox *imgBox,boolean verbose);
/* Tests the completeness and consistency of an imgBox. */
void imgBoxFree(struct imgBox **pImgBox);
/* frees all memory assocated with an imgBox (including images and imgTracks) */

// Now the real work:
void imageMapDraw(struct mapSet *map,char *name);
/* writes an image map as HTML */
void sliceAndMapDraw(struct imgSlice *slice,char *name);
/* writes a slice of an image and any assocated image map as HTML */
void imageBoxDraw(struct imgBox *imgBox);
/* writes a entire imgBox including all tracksas HTML */
#endif//def IMAGEv2_UI

#endif//ndef IMAGEV2_H
