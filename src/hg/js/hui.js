// JavaScript Especially for hui.c
// $Header: /projects/compbio/cvsroot/kent/src/hg/js/hui.js,v 1.51 2010/04/27 00:17:23 tdreszer Exp $

var compositeName = "";
//var now = new Date();
//var start = now.getTime();
//$(window).load(function () {
//    if(start != null) {
//        now = new Date();
//        alert("Loading took "+(now.getTime() - start)+" msecs.");
//    }
//});


// The 'mat*' functions are especially designed to support subtrack configuration by 2D matrix of controls

function matSelectViewForSubTracks(obj,view)
{
// viewDD:onchange Handle any necessary changes to subtrack checkboxes when the view changes
// views are "select" drop downs on a subtrack configuration page
    if( obj.selectedIndex == 0) { // hide
        matSubCBsEnable(false,view);
        hideConfigControls(view);

        // fix 3-way matCBs if necessary
        var matCBs = matCBsWhichAreComplete(false);
        if(matCBs.length > 0) {
            if($("select.viewDD[selectedIndex]").length == 0) {// No views visible so nothing is inconsistent
                $( matCBs ).each( function (i) { matCbComplete( this, true ); });
            } else {
                var classes = matViewClasses('hidden');
                classes = classes.concat( matAbcCBclasses('unchecked') );
                $( matCBs ).each( function (i) { matChkBoxNormalize( this, classes ); });
            }
        }
    } else {
        // Make main display dropdown show full if currently hide
        compositeName = obj.name.substring(0,obj.name.indexOf(".")); // {trackName}.{view}.vis
        exposeComposite(compositeName);
        // if matrix used then: essentially reclick all 'checked' matrix checkboxes
        var CBs = $("input.matCB").filter(":checked");
        if(CBs.length > 0) {
            var classSets = new Array();
            CBs.each( function (i) { classSets.push( $(this).attr("class") ); } );
            if(classSets.length > 0) {
                // Now it would be good to create a list of all subtrack CBs that match view,unchecked, and a class set (pair or triplet!)
                CBs = $("input.subCB").filter("."+view).not(":checked");
                if(CBs.length > 0) {
                    while(classSets.length > 0) {
                        var OneOrMoreClasses = classSets.pop();
                        var JustTheseCBs = CBs;
                        if(OneOrMoreClasses.length > 0) {
                            OneOrMoreClasses = OneOrMoreClasses.replace("matCB ",""); // "matCB K562 CTCF" to "K562 CTCF"
                            var classes = OneOrMoreClasses.split(" ");
                            while(classes.length > 0) {
                                JustTheseCBs = JustTheseCBs.filter("."+classes.pop());
                            }
                            JustTheseCBs.each( function (i) {
                                this.checked = true;
                                matSubCBsetShadow(this);
                                hideOrShowSubtrack(this);
                            });
                        }
                    }
                }
            }
        }
        matSubCBsEnable(true,view);
    }
    matSubCBsSelected();
}

function exposeComposite(compositeName)
{
    // Make main display dropdown show full if currently hide
    var compositeDD = $("select[name='"+compositeName+"']");
    if($(compositeDD).attr('selectedIndex') < 1) { // Composite vis display is HIDE
        var maxVis = ($(compositeDD).children('option').length - 1);
        $(compositeDD).attr('selectedIndex',maxVis);
    }
}

function matSubCbClick(subCB)
{
// subCB:onclick  When a subtrack checkbox is clicked, it may result in
// Clicking/unclicking the corresponding matrix CB.  Also the
// subtrack may be hidden as a result.
    matSubCBsetShadow(subCB);
    hideOrShowSubtrack(subCB);
    // When subCBs are clicked, 3-state matCBs may need to be set
    var classes = matViewClasses('hidden');
    classes = classes.concat( matAbcCBclasses('unchecked') );
    var matCB = matCbFindFromSubCb( subCB );
    if( matCB != undefined ) {
        matChkBoxNormalize( matCB, classes );
    }
    //var abcCB = matAbcCbFindFromSubCb( subCB );
    //if( abcCB != undefined ) {
    //    matChkBoxNormalize( abcCB, classes );
    //}
    matSubCBsSelected();
}

function matCbClick(matCB)
{
// matCB:onclick  When a matrix CB is clicked, the set of subtracks checked may change
// Also called indirectly by matButton:onclick via matSetMatrixCheckBoxes

    var classList = $( matCB ).attr("class").split(" ");
    var isABC = (aryFind(classList,"abc") != -1);
    classList = aryRemove(classList,"matCB","halfVis","abc");
    if(classList.length == 0 )
       matSubCBsCheck(matCB.checked);
    else if(classList.length == 1 )
       matSubCBsCheck(matCB.checked,classList[0]);               // dimX or dimY or dim ABC
    else if(classList.length == 2 )
       matSubCBsCheck(matCB.checked,classList[0],classList[1]);  // dimX and dimY
    else
        alert("ASSERT in matCbClick(): There should be no more than 2 entries in list:"+classList)

    if(!isABC)
        matCbComplete(matCB,true); // No longer partially checked

    if(isABC) {  // if dim ABC then we may have just made indeterminate X and Ys determinate
        if(matCB.checked == false) { // checking new dim ABCs cannot change indeterminate state.   IS THIS TRUE ?  So far.
            var matCBs = matCBsWhichAreComplete(false);
            if(matCBs.length > 0) {
                if($("input.matCB.abc:checked").length == 0) // No dim ABC checked, so leave X&Y checked but determined
                    $( matCBs ).each( function (i) { matCbComplete( this, true ); });
                else {
                    var classes = matViewClasses('hidden');
                    classes = classes.concat( matAbcCBclasses('unchecked') );
                    $( matCBs ).each( function (i) { matChkBoxNormalize( this, classes ); });
                }
            }
        }
    }
    matSubCBsSelected();
}

function matSetMatrixCheckBoxes(state)
{
// matButtons:onclick Set all Matrix checkboxes to state.  If additional arguments are passed in, the list of CBs will be narrowed by the classes
    //jQuery(this).css('cursor', 'wait');
    var matCBs = $("input.matCB").not(".abc");
    for(var vIx=1;vIx<arguments.length;vIx++) {
        matCBs = $( matCBs ).filter("."+arguments[vIx]);  // Successively limit list by additional classes.
    }
    $( matCBs ).each( function (i) {
        this.checked = state;
        matCbComplete(this,true);
    });
    subCDs = $("input.subCB");
    for(var vIx=1;vIx<arguments.length;vIx++) {
        subCDs = $( subCDs ).filter("."+arguments[vIx]);  // Successively limit list by additional classes.
    }
    if(state) { // If clicking [+], further limit to only checked ABCs
        var classes = matAbcCBclasses('unchecked');
        subCDs = objsFilterByClasses(subCDs,false,classes);  // remove unchecked abcCB classes
    }
    $( subCDs ).each( function (i) {
        this.checked = state;
        matSubCBsetShadow(this);
    });
    showOrHideSelectedSubtracks();
    matSubCBsSelected();
    //jQuery(this).css('cursor', '');
    return true;
}

///////////// CB support routines ///////////////
// Terms:
// viewDD - view drop-down control
// matButton: the [+][-] button controls associated with the matrix
// matCB - matrix dimX and dimY CB controls (in some cases this set contains abcCBs as well because they are part of the matrix)
// abcCB - matrix dim (ABC) CB controls
// subCB - subtrack CB controls
// What does work
// 1) 4 state subCBs: checked/unchecked enabled/disabled (which is visible/hidden)
// 2) 3 state matCBs for dimX and Y but not for Z (checked,unchecked,indeterminate (incomplete set of subCBs for this matCB))
// 3) cart vars for viewDD, abcCBs and subCBs but matCBs set by the state of those 3
// What is awkward or does not work
// A) Awkward: matCB could be 5 state (all,none,subset,superset,excusive non-matching set)
function matSubCBsCheck(state)
{
// Set all subtrack checkboxes to state.  If additional arguments are passed in, the list of CBs will be narrowed by the classes
// called by matCB clicks (matCbClick()) !
    var subCBs = $("input.subCB");
    for(var vIx=1;vIx<arguments.length;vIx++) {
        subCBs = subCBs.filter("."+arguments[vIx]);  // Successively limit list by additional classes.
    }

    if(state) { // If checking subCBs, then make sure up to 3 dimensions of matCBs agree with each other on subCB verdict
        var classes = matAbcCBclasses('unchecked');
        subCBs = objsFilterByClasses(subCBs,false,classes);  // remove unchecked abcCB classes
        if(arguments.length == 1 || arguments.length == 3) { // Requested dimX&Y: check dim ABC state
            $( subCBs ).each( function (i) { matSubCBcheckOne(this,state); });
        } else {//if(arguments.length == 2) { // Requested dim ABC (or only 1 dimension so this code is harmless)
            var matXY = $("input.matCB").not(".abc");  // check X&Y state
            matXY = $( matXY ).filter(":checked");
            for(var mIx=0;mIx<matXY.length;mIx++) {
                var classes = $(matXY[mIx]).attr("class").split(' ');
                classes = aryRemove(classes,"matCB","halfVis");
                $( subCBs ).filter('.'+classes.join(".")).each( function (i) { matSubCBcheckOne(this,state); });
            }
        }
    } else  // state not checked so no filtering by other matCBs needed
        subCBs.each( function (i) { matSubCBcheckOne(this,state); });

    return true;
}

function matSubCBsEnable(state)
{
// Enables/Disables subtracks checkboxes.  If additional arguments are passed in, the list of CBs will be narrowed by the classes
    var subCBs = $("input.subCB");
    for(var vIx=1;vIx<arguments.length;vIx++) {
        if(arguments[vIx].length > 0)
            subCBs = subCBs.filter("."+arguments[vIx]);  // Successively limit list by additional classes.
    }
    subCBs.each( function (i) {
        if(state) {
            $(this).parent().attr('title','');
            $(this).parent().attr('cursor','pointer');
        } else {
            $(this).parent().attr('title','view is hidden');
            $(this).parent().attr('cursor','pointer');
        }
        this.disabled = !state;
        matSubCBsetShadow(this);
        hideOrShowSubtrack(this);
    });

    return true;
}

function matSubCBcheckOne(subCB,state)
{
// setting a single subCB may cause it to appear/disappear
    subCB.checked = state;
    matSubCBsetShadow(subCB);
    hideOrShowSubtrack(subCB);
}

function matSubCBsetShadow(subCB)
{
// Since CBs only get into cart when enabled/checked, the shadow control enables cart to know other states
    var shadowState = 0;
    if(subCB.checked)
        shadowState = 1;
    if(subCB.disabled)
        shadowState -= 2;
    $("#"+subCB.name+"_4way").val(shadowState);
}

function matChkBoxNormalize(matCB)
{
// Makes sure matCBs are in one of 3 states (checked,unchecked,indeterminate) based on matching set of subCBs
    var classList = $( matCB ).attr("class").split(" ");
    var isABC = (aryFind(classList,"abc") != -1);
    if(isABC)
        alert("ASSERT: matChkBoxNormalize() called for dim ABC!");
    classList = aryRemove(classList,"matCB","halfVis");

    var classes = '.' + classList.join(".");// created string filter of classes converting "matCB K562 H3K4me1" as ".K562.H3K4me1"
    var subCBs = $("input.subCB").filter(classes); // All subtrack CBs that match matrix CB

    if(arguments.length > 1 && arguments[1].length > 0) { // dim ABC NOT classes
        subCBs = objsFilterByClasses(subCBs,false,arguments[1]);
    }

    if(subCBs.length > 0) {
        var CBsChecked = subCBs.filter(":checked");
        if(!isABC) {
            if(CBsChecked.length == subCBs.length) {
                matCbComplete(matCB,true);
                $(matCB).attr('checked',true);
            } else if(CBsChecked.length == 0) {
                matCbComplete(matCB,true);
                $(matCB).attr('checked',false);
            } else {
                matCbComplete(matCB,false);
                $(matCB).attr('checked',true);
            }
        }
    }
    else
        matCbComplete(matCB,true); // If no subs match then this is determined !
}

function matCbComplete(matCB,complete)
{
// Makes or removes the 3rd (indeterminate) matCB state
    // Too many options:
    // 1) addClass()/removeClass() (which does not directly support title)
    // 2) wrap div which could contain border, color, content.  content is not on one line: size is difficult
    // 3) wrap font which could contain border, color, content.  content is on one line: size is difficult
    // 4) *[ ]* ?[ ]?  *[ ]*  No text seems right;  borders? colors? opacity? Yes, minimum
    if(complete) {
        $(matCB).css('opacity', '1');  // For some reason IE accepts direct change but isn't happy with simply adding class!
        $(matCB).removeClass('halfVis');
        $(matCB).attr("title","");
    } else {
        $(matCB).css('opacity', '0.5');
        $(matCB).addClass('halfVis');
        $(matCB).attr("title","Not all associated subtracks have been selected");
        $('.halfVis').css('opacity', '0.5');
    }
}

function matCBsWhichAreComplete(complete)
{
// Returns a list of currently indeterminate matCBs.  This is encapsulated to keep consistent with matCbComplete()
    if(complete)
        return $("input.matCB").not(".halfVis");
    else
        return $("input.matCB.halfVis");
}

function matCbFindFromSubCb(subCB)
{
// returns the one matCB associated with a subCB (or undefined)
    var classList =  $( subCB ).attr("class").split(" ");
    classes = '.' + classList.slice(1,3).join('.');   // How to get only X and Y classes?  Assume they are the first 2 ("subCB GM12878 H3K4me3 rep1 p1" we only want ".GM12878.H3K4me3")
    // At this point classes has been converted from "subCB 1GM12878 CTCF rep1 cHot" to ".1GM12878.CTCF"
    var matCB = $("input.matCB"+classes); // NOte, this works for filtering multiple classes because we want AND
    if(matCB.length == 1)
        return matCB;
    else
        return undefined;
}

function matAbcCBfindFromSubCb(subCB)
{
// returns the abcCBs associated with a subCB (or undefined)
    var abcCBs = $("input.matCB.abc");
    if( abcCBs.length > 0 ) {
        var classList =  $( subCB ).attr("class").split(" ");
        classList = aryRemove(classList,"subCB");
        classList.shift(); // Gets rid of X and Y associated classes (first 2 after subCB)
        classList.shift();
        classList.pop();   // gets rid of view associated class (at end)
        if(classList.length >= 1) {
            var abcCB = $(abcCBs).filter('.'+classList.join("."));
            if(abcCB.length >= 1)
                return abcCB;
        }
    }
    return undefined;
}

function objsFilterByClasses(objs,keep,classes)
{
// Accepts an obj list and an array of classes, then filters successively by that list
    if( classes != undefined && classes.length > 0 ) {
        if(keep) {
            objs = $( objs ).filter( '.' + classes.join('.') ); // filter('class1.class2') is same as filter('.class1').filter('.class2')
        } else {
            for(var cIx=classes.length-1;cIx>-1;cIx--) {
                objs = $( objs ).not( '.' + classes[cIx] );   // not('class1.class2') is different from not('.class1').not('.class2')
            }
        }
    }
    return objs;
}

function matViewClasses(limitTo)
{
// returns an array of classes from the ViewDd: converts "viewDD normalText SIG"[]s to "SIG","zRAW"
    var classes = new Array;
    var viewDDs = $("select.viewDD");//.filter("[selectedIndex='0']");
    if(limitTo == 'hidden') {
        viewDDs = $(viewDDs).not("[selectedIndex]");
    } else if(limitTo == 'visible') {
        viewDDs = $(viewDDs).filter("[selectedIndex]");
    }
    $(viewDDs).each( function (i) {
        var classList = $( this ).attr("class").split(" ");
        classList = aryRemove(classList,"viewDD","normalText");
        classes.push( classList[0] );
    });
    return classes;
}

function matAbcCBclasses(limitTo)
{// returns an array of classes from the dim ABC CB classes: converts "matCB abc rep1"[]s to "rep1","rep2"
    var classes = new Array;
    var abcCBs = $("input.matCB.abc");
    if(abcCBs.length > 0) {
        if(limitTo == 'unchecked') {
            abcCBs = abcCBs.not(":checked");
        } else if(limitTo == 'checked') {
            abcCBs = abcCBs.filter(":checked");
        }
        $(abcCBs).each( function (i) {
            var classList = $( this ).attr("class").split(" ");
            classList = aryRemove(classList,"matCB","abc");
            classes.push( classList[0] );
        });
    }
    return classes;
}

function matSubCBsSelected()
{
// Displays visible and checked track count
    var counter = $('#subCBcount');
    if(counter != undefined) {
        var subCBs =  $("input.subCB");
        $(counter).text($(subCBs).filter(":enabled:checked").length + " of " +$(subCBs).length+ " selected");
    }
}

/////////////////// subtrack configuration support ////////////////

function compositeCfgUpdateSubtrackCfgs(inp)
{
// Updates all subtrack configuration values when the composite cfg is changed
    var suffix = inp.name.substring(inp.name.indexOf("."));  // Includes '.'
    //if(suffix.length==0)
    //    suffix = inp.name.substring(inp.name.indexOf("_"));
    if(suffix.length==0) {
        alert("Unable to parse '"+inp.name+"'");
        return true;
    }
    if(inp.type.indexOf("select") == 0) {
        var list = $("select[name$='"+suffix+"']").not("[name='"+inp.name+"']"); // Exclude self from list
        if($(list).length>0) {
            if(inp.multiple != true)
                $(list).attr('selectedIndex',inp.selectedIndex);
            else {
                $(list).each(function() {  // for all dependent (subtrack) multi-selects
                    sel = this;
                    $(this).children('option').each(function() {  // for all options of dependent mult-selects
                        $(this).attr('selected',$(inp).children('option:eq('+this.index+')').attr('selected')); // set selected state to independent (parent) selected state
                    });
                    $(this).attr('size',$(inp).attr('size'));
                });
            }
        }
    }
    else if(inp.type.indexOf("checkbox") == 0) {
        var list = $("checkbox[name$='"+suffix+"']").not("[name='"+inp.name+"']"); // Exclude self from list
        if($(list).length>0)
            $(list).attr("checked",$(inp).attr("checked"));
    }
    else if(inp.type.indexOf("radio") == 0) {
        var list = $("input:radio[name$='"+suffix+"']").not("[name='"+inp.name+"']");
        list = $(list).filter("[value='"+inp.value+"']")
        if($(list).length>0)
            $(list).attr("checked",true);
    }
    else {  // Various types of inputs
        var list = $("input[name$='"+suffix+"']").not("[name='"+inp.name+"']");//.not("[name^='boolshad.']"); // Exclude self from list
        if($(list).length>0)
            $(list).val(inp.value);
        //else {
        //    alert("Unsupported type of multi-level cfg setting type='"+inp.type+"'");
        //    return false;
        //}
    }
    return true;
}

function compositeCfgRegisterOnchangeAction(prefix)
{
// After composite level cfg settings written to HTML it is necessary to go back and
// make sure that each time they change, any matching subtrack level cfg setting are changed.
    var list = $("input[name^='"+prefix+".']").not("[name$='.vis']");
    $(list).change(function(){compositeCfgUpdateSubtrackCfgs(this);});

    var list = $("select[name^='"+prefix+".']").not("[name$='.vis']");
    $(list).change(function(){compositeCfgUpdateSubtrackCfgs(this);});
}


function subtrackCfgHideAll(table)
{
// hide all the subtrack configuration stuff
    $("div[id $= '_cfg']").each( function (i) {
        $( this ).css('display','none');
        $( this ).children("input[name$='.childShowCfg']").val("off");
    });
}

function subtrackCfgShow(tableName)
{
// Will show subtrack specific configuration controls
// Config controls not matching name will be hidden
    var divit = $("#div_"+tableName+"_cfg");
    if($(divit).css('display') == 'none')
        $("#div_"+tableName+"_meta").hide();
    // Could have all inputs commented out, then uncommented when clicked:
    // But would need to:
    // 1) be able to find composite view level input
    // 2) know if subtrack input is non-default (if so then subtrack value overrides composite view level value)
    // 3) know whether so composite view level value has changed since hgTrackUi displayed (if so composite view level value overrides)
    $(divit).toggle();
    return false;
}

function enableViewCfgLink(enable,view)
{
// Enables or disables a single configuration link.
    var link = $('#a_cfg_'+view);
    if(enable)
        $(link).attr('href','#'+$(link).attr('id'));
    else
        $(link).removeAttr('href');
}

function enableAllViewCfgLinks()
{
    $( ".viewDD").each( function (i) {
        var view = this.name.substring(this.name.indexOf(".") + 1,lastIndexOf(".vis"));
        enableViewCfgLink((this.selectedIndex > 0),view);
    });
}

function hideConfigControls(view)
{
// Will hide the configuration controls associated with one name
    $("input[name$='"+view+".showCfg']").val("off");      // Set cart variable
    $("tr[id^='tr_cfg_"+view+"']").css('display','none'); // Hide controls
}

function showConfigControls(name)
{
// Will show configuration controls for name= {tableName}.{view}
// Config controls not matching name will be hidden
    var trs  = $("tr[id^='tr_cfg_']")
    $("input[name$='.showCfg']").val("off"); // Turn all off
    $( trs ).each( function (i) {
        if( this.id == 'tr_cfg_'+name && this.style.display == 'none') {
            $( this ).css('display','');
	    $("input[name$='."+name+".showCfg']").val("on");
        }
        else if( this.style.display == '') {
            $( this ).css('display','none');
        }
    });

    // Close the cfg controls in the subtracks
    $("table[id^='subtracks.']").each( function (i) { subtrackCfgHideAll(this);} );
    return true;
}

function trAlternateColors(table,cellIx)
{
// Will alternate colors each time the contents of the column(s) change
    var lastContent = "not";
    var bgColor1 = "#FFFEE8";
    var bgColor2 = "#FFF9D2";
    var curColor = bgColor1;
    var lastContent = "start";
    var cIxs = new Array();

    for(var aIx=1;aIx<arguments.length;aIx++) {   // multiple columns
        cIxs[aIx-1] = arguments[aIx];
    }
    if (document.getElementsByTagName)
    {
        for (var trIx=0;trIx<table.rows.length;trIx++) {
            var curContent = "";
            if(table.rows[trIx].style.display == 'none')
                continue;
            for(var ix=0;ix<cIxs.length;ix++) {
                if(table.rows[trIx].cells[cIxs[ix]]) {
                    curContent = (table.rows[trIx].cells[cIxs[ix]].abbr != "" ?
                                  table.rows[trIx].cells[cIxs[ix]].abbr       :
                                  table.rows[trIx].cells[cIxs[ix]].innerHTML  );
                }
            }
            if( lastContent != curContent ) {
                lastContent  = curContent;
                if( curColor == bgColor1)
                    curColor =  bgColor2;
                else
                    curColor =  bgColor1;
            }
            table.rows[trIx].bgColor = curColor;
        }
    }
}

//////////// Sorting ////////////

function tableSort(table,fnCompare)
{
// Sorts table based upon rules passed in by function reference
    //alert("tableSort("+table.id+") is beginning.");
    subtrackCfgHideAll(table);
    var trs=0,moves=0;
    var colOrder = new Array();
    var cIx=0;
    var trTopIx,trCurIx,trBottomIx=table.rows.length - 1;
    for(trTopIx=0;trTopIx < trBottomIx;trTopIx++) {
        trs++;
        var topRow = table.rows[trTopIx];
        for(trCurIx = trTopIx + 1; trCurIx <= trBottomIx; trCurIx++) {
            var curRow = table.rows[trCurIx];
            var compared = fnCompare(topRow,curRow,arguments[2]);
            if(compared < 0) {
                table.insertBefore(table.removeChild(curRow), topRow);
                topRow = curRow; // New top!
                moves++;
            }
        }
    }
    if(fnCompare != trComparePriority)
        tableSetPositions(table);
    //alert("tableSort("+table.id+") examined "+trs+" rows and made "+moves+" moves.");
}

// Sorting a table by columns relies upon the sortColumns structure
// The sortColumns structure looks like:
//{
//    char *  tags[];     // a list of trackDb.subGroupN tags in sort order
//    boolean reverse[];  // the sort direction for that subGroup
//    int     cellIxs[];  // The indexes of the columns in the table to be sorted
//}
///// Following functions are for Sorting by columns:
function trCompareColumnAbbr(tr1,tr2,sortColumns)
{
// Compares a set of columns based upon the contents of their abbr
    for(var ix=0;ix < sortColumns.cellIxs.length;ix++) {
        //if(tr1.cells[sortColumns.cellIxs[ix]].abbr == undefined) {
        //    if(tr1.cells[sortColumns.cellIxs[ix]].value < tr2.cells[sortColumns.cellIxs[ix]].value)
        //        return (sortColumns.reverse[ix] ? -1: 1);
        //    else if(tr1.cells[sortColumns.cellIxs[ix]].value > tr2.cells[sortColumns.cellIxs[ix]].value)
        //        return (sortColumns.reverse[ix] ? 1: -1);
        //} else {
            if(tr1.cells[sortColumns.cellIxs[ix]].abbr < tr2.cells[sortColumns.cellIxs[ix]].abbr)
                return (sortColumns.reverse[ix] ? -1: 1);
            else if(tr1.cells[sortColumns.cellIxs[ix]].abbr > tr2.cells[sortColumns.cellIxs[ix]].abbr)
                return (sortColumns.reverse[ix] ? 1: -1);
        //}
    }
    return 0;
}


function tableSortByColumns(table,sortColumns)
{
// Will sort the table based on the abbr values on a et of <TH> colIds
    if (document.getElementsByTagName)
    {
        tableSort(table,trCompareColumnAbbr,sortColumns);//cellIxs,columns.colRev);
                        var columns = new sortColumnsGetFromTable(table);
        if(sortColumns.tags.length>1)
            trAlternateColors(table,sortColumns.cellIxs[sortColumns.tags.length-2]);

    }
}

function sortOrderFromColumns(sortColumns)
{// Creates the trackDB setting entry sortOrder subGroup1=+ ... from a sortColumns structure
    var sortOrder ="";
    for(ix=0;ix<sortColumns.tags.length;ix++) {
        sortOrder += sortColumns.tags[ix] + "=" + (sortColumns.reverse[ix] ? "-":"+") + " ";
    }
    if(sortOrder.length > 0)
        sortOrder = sortOrder.substr(0,sortOrder.length-1);
    return sortOrder;
}

function sortOrderFromTr(tr)
{// Looks up the sortOrder input value from a *.sortTr header row of a sortable table
    var inp = tr.getElementsByTagName('input');
    for(var ix=0;ix<inp.length;ix++) {
        var offset = inp[ix].id.lastIndexOf(".sortOrder");
        if(offset > 0 && offset == inp[ix].id.length - 10)
            return inp[ix].value;
    }
    return "";
}
function sortColumnsGetFromSortOrder(sortOrder)
{// Creates sortColumns struct (without cellIxs[]) from a trackDB.sortOrder setting string
    this.tags = new Array();
    this.reverse = new Array();
    var order = sortOrder;
    for(var ix=0;ix<12;ix++) {
        if(order.indexOf("=") <= 0)
            break;
        this.tags[ix]    = order.substring(0,order.indexOf("="));
        this.reverse[ix] = (order.substr(this.tags[ix].length+1,1) != '+');
        if(order.length < (this.tags[ix].length+2))
            break;
        order = order.substring(this.tags[ix].length+3);
    }
}
function sortColumnsGetFromTr(tr)
{// Creates a sortColumns struct from the entries in the '*.sortTr' heading row of a sortable table
    this.inheritFrom = sortColumnsGetFromSortOrder;
    var inp = tr.getElementsByTagName('input');
    var ix;
    for(ix=0;ix<inp.length;ix++) {
        var offset = inp[ix].id.lastIndexOf(".sortOrder");
        if(offset > 0 && offset == inp[ix].id.length - 10) {
            this.inheritFrom(inp[ix].value);
            break;
        }
    }
    if(ix == inp.length)
        return;

    // Add an additional array
    this.cellIxs = new Array();
    var cols = tr.getElementsByTagName('th');
    for(var tIx=0;tIx<this.tags.length;tIx++) {
        var colIdTag = this.tags[tIx] + ".sortTh";
        for(ix=0; ix<cols.length; ix++) {
            var offset = cols[ix].id.lastIndexOf(colIdTag);
            if(offset > 0 && offset == cols[ix].id.length - colIdTag.length) {
                this.cellIxs[tIx] = cols[ix].cellIndex;
            }
        }
    }
}
function sortColumnsGetFromTable(table)
{// Creates a sortColumns struct from the contents of a '*.sortable' table
    this.inheritNow = sortColumnsGetFromTr;
    var ix;
    for(ix=0;ix<table.rows.length;ix++) {
        var offset = table.rows[ix].id.lastIndexOf(".sortTr");
        if(offset > 0 && offset == table.rows[ix].id.length - 7) {
            this.inheritNow(table.rows[ix]);
            break;
        }
    }
}


function tableSortUsingSortColumns(table)
{// Sorts a table body based upon the marked columns
    var columns = new sortColumnsGetFromTable(table);
    tbody = table.getElementsByTagName("tbody")[0];
    tableSortByColumns(tbody,columns);
}

function tableSortAtButtonPress(anchor,tagId)
{// Updates the sortColumns struct and sorts the table when a column header has been pressed
 // If the current primary sort column is pressed, its direction is toggled then the table is sorted
 // If a secondary sort column is pressed, it is moved to the primary spot and sorted in fwd direction
    var th=anchor.parentNode;
    var sup=th.getElementsByTagName("sup")[0];
    var tr=th.parentNode;
    var inp = tr.getElementsByTagName('input');
    var iIx;
    for(iIx=0;iIx<inp.length;iIx++) {
        var offset = inp[iIx].id.lastIndexOf(".sortOrder");
        if(offset > 0 && offset == inp[iIx].id.length - 10)
            break;
    }
    var theOrder = new sortColumnsGetFromTr(tr);
    var oIx;
    for(oIx=0;oIx<theOrder.tags.length;oIx++) {
        if(theOrder.tags[oIx] == tagId)
            break;
    }
    if(oIx > 0) { // Need to reorder
        var newOrder = new sortColumnsGetFromTr(tr);
        var nIx=0;
        newOrder.tags[nIx] = theOrder.tags[oIx];
        newOrder.reverse[nIx] = false;  // When moving to the first position sort forward
        newOrder.cellIxs[nIx] = theOrder.cellIxs[ oIx];
        sups = tr.getElementsByTagName("sup");
        sups[newOrder.cellIxs[nIx]-1].innerHTML = "&darr;1";
        for(var ix=0;ix<theOrder.tags.length;ix++) {
            if(ix != oIx) {
                nIx++;
                newOrder.tags[nIx]    = theOrder.tags[ix];
                newOrder.reverse[nIx] = theOrder.reverse[ix];
                newOrder.cellIxs[nIx] = theOrder.cellIxs[ix];
                var dir = sups[newOrder.cellIxs[nIx]-1].innerHTML.substring(0,1);
                sups[newOrder.cellIxs[nIx]-1].innerHTML = dir + (nIx+1);
            }
        }
        theOrder = newOrder;
    } else { // need to reverse directions
        theOrder.reverse[oIx] = (theOrder.reverse[oIx] == false);
        var ord = sup.innerHTML.substring(1);
        sup.innerHTML = (theOrder.reverse[oIx] == false ? "&darr;":"&uarr;");
        if(theOrder.tags.length>1)
            sup.innerHTML += ord;
    }
    //alert("tableSortAtButtonPress(): count:"+theOrder.tags.length+" tag:"+theOrder.tags[0]+"="+(theOrder.reverse[0]?"-":"+"));
    var newSortOrder = sortOrderFromColumns(theOrder);
    inp[iIx].value = newSortOrder;
    var thead=tr.parentNode;
    var table=thead.parentNode;
    tbody = table.getElementsByTagName("tbody")[0];
    tableSortByColumns(tbody,theOrder);
    return;

}
function tableSortAtStartup()
{
    //alert("tableSortAtStartup() called");
    var list = document.getElementsByTagName('table');
    for(var ix=0;ix<list.length;ix++) {
        var offset = list[ix].id.lastIndexOf(".sortable");  // TODO: replace with class and jQuery
        if(offset > 0 && offset == list[ix].id.length - 9) {
            tableSortUsingSortColumns(list[ix]);
        }
    }
}

function hintOverSortableColumnHeader(th)
{// Upodates the sortColumns struct and sorts the table when a column headder has been pressed
    //th.title = "Click to make this the primary sort column, or toggle direction";
    //var tr=th.parentNode;
    //th.title = "Current Sort Order: " + sortOrderFromTr(tr);
}

///// Following functions are for Sorting by priority
function tableSetPositions(table)
{
// Sets the value for the *.priority input element of a table row
// This gets called by sort or dradgndrop in order to allow the new order to affect hgTracks display
    if (table.getElementsByTagName)
    {
        for(var trIx=0;trIx<table.rows.length;trIx++) {
            if(table.rows[trIx].id.indexOf("tr_cb_") == 0) {
                var inp = table.rows[trIx].getElementsByTagName('input');
                for(var ix=0;ix<inp.length;ix++) {
                    var offset = inp[ix].name.lastIndexOf(".priority");
                    if( offset > 0 && offset == (inp[ix].name.length - 9)) {
                        inp[ix].value = table.rows[trIx].rowIndex;
                        break;
                    }
                }
            }
        }
    }
}
function trFindPosition(tr)
{
// returns the position (*.priority) of a sortable table row
    var inp = tr.getElementsByTagName('input');
    for(var ix=0;ix<inp.length;ix++) {
        var offset = inp[ix].name.indexOf(".priority");
        if(offset > 0 && offset == (inp[ix].name.length - 9)) {
            return inp[ix].value;
        }
    }
    return "unknown";
}

function trComparePriority(tr1,tr2)
{
// Compare routine for sorting by *.priority
    var priority1 = 999999;
    var priority2 = 999999;
    var inp1 = tr1.getElementsByTagName('input');
    var inp2 = tr2.getElementsByTagName('input');
    for(var ix=0;ix<inp1.length;ix++) { // should be same length
        if(inp1[ix].name.indexOf(".priority") == (inp1[ix].name.length - 9))
            priority1 = inp1[ix].value;
        if(inp2[ix].name.indexOf(".priority") == (inp2[ix].name.length - 9))
            priority2 = inp2[ix].value;
        if(priority1 < 999999 && priority2 < 999999)
            break;
    }
    return priority2 - priority1;
}

///// Following functions support column reorganization
function trReOrderCells(tr,cellIxFrom,cellIxTo)
{
// Reorders cells in a table row: removes cell from one spot and inserts it before another
    //alert("tableSort("+table.id+") is beginning.");
    if(cellIxFrom == cellIxTo)
        return;

    var tdFrom = tr.cells[cellIxFrom];
    var tdTo   = tr.cells[cellIxTo];
    if((cellIxTo - cellIxFrom) == 1) {
        tdFrom = tr.cells[cellIxTo];
        tdTo   = tr.cells[cellIxFrom];
    } else if((cellIxTo - cellIxFrom) > 1)
        tdTo   = tr.cells[cellIxTo + 1];

    tr.insertBefore(tr.removeChild(tdFrom), tdTo);
}

function tableReOrderColumns(table,cellIxFrom,cellIxTo)
{
// Reorders cells in all the rows of a table row, thus reordering columns
    if (table.getElementsByTagName) {
        for(var ix=0;ix<table.rows.length;ix++) {
            var offset = table.rows[ix].id.lastIndexOf(".sortTr");
            if(offset > 0 && offset == table.rows[ix].id.length - 7) {
                trReOrderCells(table.rows[ix],cellIxFrom,cellIxTo);
                break;
            }
        }
        tbody = table.getElementsByTagName('tbody');
        for(var ix=0;ix<tbody[0].rows.length;ix++) {
            trReOrderCells(tbody[0].rows[ix],cellIxFrom,cellIxTo);
        }
    }
}

function showOrHideSelectedSubtracks(inp)
{
// Show or Hide subtracks based upon radio toggle
    var showHide;
    if(arguments.length > 0)
        showHide=inp;
    else {
        var onlySelected = $("input[name='displaySubtracks']");
        if(onlySelected.length > 0)
            showHide = onlySelected[0].checked;
        else
            return;
    }
    showSubTrackCheckBoxes(showHide);
    var list = $("table.tableSortable")
    for(var ix=0;ix<list.length;ix++) {
        var columns = new sortColumnsGetFromTable(list[ix]);
        var tbody = list[ix].getElementsByTagName('tbody');
        if(columns.tags.length>1) {
            if(columns.tags.length==2)
                trAlternateColors(tbody[0],columns.cellIxs[0]);
            else if(columns.tags.length==3)
                trAlternateColors(tbody[0],columns.cellIxs[0],columns.cellIxs[1]);
            else
                trAlternateColors(tbody[0],columns.cellIxs[0],columns.cellIxs[1],columns.cellIxs[2]);
        }
    }
}

///// Following functions called on page load
function matInitializeMatrix()
{
// Called at Onload to coordinate all subtracks with the matrix of check boxes
//var start = startTiming();
jQuery('body').css('cursor', 'wait');
    if (document.getElementsByTagName) {
        matSubCBsSelected();
        showOrHideSelectedSubtracks();
    }
jQuery('body').css('cursor', '');
//showTiming(start,"matInitializeMatrix()");
}

function multiSelectLoad(div,sizeWhenOpen)
{
    //var div = $(obj);//.parent("div.multiSelectContainer");
    var sel = $(div).children("select:first");
    if(div != undefined && sel != undefined && sizeWhenOpen <= $(sel).length) {
        $(div).css('width', ( $(sel).clientWidth ) +"px");
        $(div).css('overflow',"hidden");
        $(div).css('borderRight',"2px inset");
    }
    $(sel).show();
}

function multiSelectBlur(obj)
{
    if($(obj).val() == undefined || $(obj).val() == "") {
        $(obj).val("All");
        $(obj).attr('selectedIndex',0);
    }
    //if(obj.value == "All") // Close if selected index is 1
    if($(obj).attr('selectedIndex') == 0) // Close if selected index is 1
        $(obj).attr('size',1);
    /*else if($.browser.msie == false && $(obj).children('option[selected]').length==1) {
        var ix;
        for(ix=0;ix<obj.options.length;ix++) {
            if(obj.options[ix].value == obj.value) {
                //obj.options[ix].selected = true;
                obj.selectedIndex = ix;
                obj.size=1;
                $(obj).trigger('change');
                break;
            }
        }
    }*/
}

function multiSelectClick(obj,sizeWhenOpen)
{
    if($(obj).attr('size') == 1)
        $(obj).attr('size',sizeWhenOpen);
    else if($(obj).attr('selectedIndex') == 0)
        $(obj).attr('size',1);
}

// The following js depends upon the jQuery library
$(document).ready(function()
{
    //matInitializeMatrix();
    //$("div.multiSelectContainer").each( function (i) {
    //    var sel = $(this).children("select:first");
    //    multiSelectLoad(this,sel.openSize);
    //});

    // Allows rows to have their positions updated after a drag event
    if($(".tableWithDragAndDrop").length > 0) {
        $(".tableWithDragAndDrop").tableDnD({
            onDragClass: "trDrag",
            onDrop: function(table, row) {
                    if(tableSetPositions) {
                        tableSetPositions(table);
                    }
                }
            });
        $(".trDraggable").hover(
            function(){if($(this).hasClass('trDrag') == false) $(this).addClass('pale');},
            function(){$(this).removeClass('pale');}
        );
    }
    $('.halfVis').css('opacity', '0.5'); // The 1/2 opacity just doesn't get set from cgi!
});
