/* javascript - some little helper routines  to manage our javascript.  
 * We don't do much javascript - just occassionally use it so that
 * when they select something from a pull-down, it will go hit the server to 
 * figure out how to reload other control options based on the choice.
 * (For instance if they change the group, which items in the track
 * drop-down need to change).
 *
 * We accomplish this by maintaining two forms - a mainForm and a
 * hiddenForm.  The hiddenForm maintains echo's of all the variables
 * in the main form, which get updated onChange of controls that need
 * to 'ripple' to other controls.  The onChange also submits the
 * control. */

#include "common.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "cart.h"
#include "hPrint.h"
#include "jsHelper.h"

void jsWriteFunctions()
/* Write out Javascript functions. */
{
hPrintf("\n<SCRIPT>\n");
hPrintf("%s\n",
"function setRadioCheck(varName, value) \n"
"{\n"
"var len = document.mainForm.elements.length;\n"
"var i = 0;\n"
"for (i = 0; i < len; i++) \n"
"    {\n"
"    if (document.mainForm.elements[i].name == varName) \n"
"	{\n"
"	if (document.mainForm.elements[i].value == value)\n"
"	    document.mainForm.elements[i].checked = true;\n"
"	else\n"
"	    document.mainForm.elements[i].checked = false;\n"
"	}\n"
"    }\n"
"}\n"
"\n"
"function getKeyCode(e)\n"
"{\n"
"if (window.event) // IE\n"
"    {\n"
"    return e.keyCode;\n"
"    }\n"
"else \n"
"    {\n"
"    return e.which;\n"
"    }\n"
"}\n"
"\n"
"function getKey(e)\n"
"{\n"
"return String.fromCharCode(getKeyCode(e));\n"
"}\n"
"\n"
"function gotEnterKey(e)\n"
"{\n"
"return getKeyCode(e) == 13;\n"
"}\n"
"\n"
"function submitOnEnter(e,f)\n"
"{\n"
"if(gotEnterKey(e))\n"
"   {\n"
"   f.submit();\n"
"   return false;\n"
"   }\n"
"else\n"
"   return true;\n"
"}\n"
"\n"
"function noSubmitOnEnter(e)\n"
"{\n"
"return !gotEnterKey(e);\n"
"}\n"
);
hPrintf("</SCRIPT>\n");
}

struct dyString *jsOnChangeStart()
/* Start up an onChange string */
{
struct dyString *dy = dyStringNew(1024);
dyStringAppend(dy, "onChange=\"");
return dy;
}

char *jsOnChangeEnd(struct dyString **pDy)
/* Finish up javascript onChange command. */
{
dyStringAppend(*pDy, "document.hiddenForm.submit();\"");
return dyStringCannibalize(pDy);
}

void jsDropDownCarryOver(struct dyString *dy, char *var)
/* Add statement to carry-over drop-down item to dy. */
{
dyStringPrintf(dy, "document.hiddenForm.%s.value=", var);
dyStringPrintf(dy, "document.mainForm.%s.options", var);
dyStringPrintf(dy, "[document.mainForm.%s.selectedIndex].value; ", var);
}

void jsTextCarryOver(struct dyString *dy, char *var)
/* Add statement to carry-over text item to dy. */
{
dyStringPrintf(dy, 
    "document.hiddenForm.%s.value=document.mainForm.%s.value; ",
    var, var);
}

void jsTrackingVar(char *jsVar, char *val)
/* Emit a little Javascript to keep track of a variable. 
 * This helps especially with radio buttons. */
{
hPrintf("<SCRIPT>\n");
hPrintf("var %s='%s';\n", jsVar, val);
hPrintf("</SCRIPT>\n");
}

void jsMakeTrackingRadioButton(char *cgiVar, char *jsVar, 
	char *val, char *selVal)
/* Make a radio button that also sets tracking variable
 * in javascript. */
{
hPrintf("<INPUT TYPE=RADIO NAME=\"%s\"", cgiVar);
hPrintf(" VALUE=\"%s\"", val);
hPrintf(" onClick=\"%s='%s';\"", jsVar, val);
if (sameString(val, selVal))
    hPrintf(" CHECKED");
hPrintf(">");
}

void jsMakeTrackingCheckBox(struct cart *cart,
	char *cgiVar, char *jsVar, boolean usualVal)
/* Make a check box filling in with existing value and
 * putting a javascript tracking variable on it. */
{
char buf[256];
boolean oldVal = cartUsualBoolean(cart, cgiVar, usualVal);
hPrintf("<SCRIPT>var %s=%d;</SCRIPT>\n", jsVar, oldVal);
hPrintf("<INPUT TYPE=CHECKBOX NAME=%s VALUE=1", cgiVar);
if (oldVal)
    hPrintf(" CHECKED");
hPrintf(" onClick=\"%s=%d;\"", jsVar, !oldVal);
hPrintf(">");
sprintf(buf, "%s%s", cgiBooleanShadowPrefix(), cgiVar);
cgiMakeHiddenVar(buf, "1");
}

void jsTrackedVarCarryOver(struct dyString *dy, char *cgiVar, char *jsVar)
/* Carry over tracked variable (radio button?) to hidden form. */
{
dyStringPrintf(dy, "document.hiddenForm.%s.value=%s; ", cgiVar, jsVar);
}

char *jsRadioUpdate(char *cgiVar, char *jsVar, char *val)
/* Make a little javascript to check and uncheck radio buttons
 * according to new value.  To use this you must have called
 * jsWriteFunctions somewhere, and also must use jsMakeTrackingRadioButton
 * to make the buttons. */
{
static char buf[256];
safef(buf, sizeof(buf),
    "setRadioCheck('%s', '%s'); %s='%s'", cgiVar, val, jsVar, val);
return buf;
}

void jsCreateHiddenForm(struct cart *cart, char *scriptName,
	char **vars, int varCount)
/* Create a hidden form with the given variables */
{
int i;
hPrintf(
    "<FORM ACTION=\"..%s\" "
    "METHOD=\"GET\" NAME=\"hiddenForm\">\n", scriptName);
cartSaveSession(cart);
for (i=0; i<varCount; ++i)
    hPrintf("<input type=\"hidden\" name=\"%s\" value=\"\">\n", vars[i]);
puts("</FORM>");
}

