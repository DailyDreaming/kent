/* cart - stuff to manage variables that persist from 
 * one invocation of a cgi script to another (variables
 * that are carted around).  */

#ifndef CART_H
#define CART_H

#include "jksql.h"
#include "errabort.h"
#include "dystring.h"

typedef struct sqlConnection *(*DbConnector)();  
/* funtion type used to get a connection to database */

typedef void (*DbDisconnect)(struct sqlConnection **pConn); 
/* function type used to cleanup a connection from database */


struct cart
/* A cart of settings that persist. */
   {
   struct cart *next;	/* Next in list. */
   unsigned int userId;	/* User ID in database. */
   unsigned int sessionId;	/* Session ID in database. */
   struct hash *hash;	/* String valued hash. */
   struct hash *exclude;	/* Null valued hash of variables not to save. */
   struct cartDb *userInfo;	/* Info on user. */
   struct cartDb *sessionInfo;	/* Info on session. */
   };

struct cart *cartNew(unsigned int userId, unsigned int sessionId, 
	char **exclude, struct hash *oldVars);
/* Load up cart from user & session id's.  Exclude is a null-terminated list of
 * strings to not include. oldVars is an optional hash to put in values
 * that were just overwritten by cgi-variables. */

void cartCheckout(struct cart **pCart);
/* Save cart to database and free it up. */

void cartEncodeState(struct cart *cart, struct dyString *dy);
/* Add a CGI-encoded var=val&... string of all cart variables to dy. */

void cartParseOverHash(struct cart *cart, char *contents);
/* Parse cgi-style contents into cart's hash table.  This will
 * replace existing members of hash that have same name. */

char *cartSessionVarName();
/* Return name of CGI session ID variable. */

unsigned int cartSessionId(struct cart *cart);
/* Return session id. */

char *cartSidUrlString(struct cart *cart);
/* Return session id string as in hgsid=N . */

unsigned int cartUserId(struct cart *cart);
/* Return session id. */

void cartRemove(struct cart *cart, char *var);
/* Remove variable from cart. */

void cartRemoveExcept(struct cart *cart, char **except);
/* Remove variables except those in null terminated except array
 * from cart.  Except array may be NULL in which case all
 * are removed. */

void cartRemoveLike(struct cart *cart, char *wildCard);
/* Remove all variable from cart that match wildCard. */

void cartRemovePrefix(struct cart *cart, char *prefix);
/* Remove variables with given prefix from cart. */

boolean cartVarExists(struct cart *cart, char *var);
/* Return TRUE if variable is in cart. */

char *cartString(struct cart *cart, char *var);
/* Return string valued cart variable. */

char *cartOptionalString(struct cart *cart, char *var);
/* Return string valued cart variable or NULL if it doesn't exist. */

char *cartNonemptyString(struct cart *cart, char *name);
/* Return string value associated with name.  Return NULL
 * if value doesn't exist or if it is pure white space. */

char *cartUsualString(struct cart *cart, char *var, char *usual);
/* Return variable value if it exists or usual if not. */

char *cartCgiUsualString(struct cart *cart, char *var, char *usual);
/* Look for var in CGI, then in cart, if not found then return usual. */

void cartSetString(struct cart *cart, char *var, char *val);
/* Set string valued cart variable. */

int cartInt(struct cart *cart, char *var);
/* Return int valued variable. */

int cartIntExp(struct cart *cart, char *var);
/* Return integer valued expression in variable. */

int cartUsualInt(struct cart *cart, char *var, int usual);
/* Return variable value if it exists or usual if not. */

int cartUsualIntClipped(struct cart *cart, char *var, int usual,
	int minVal, int maxVal);
/* Return integer variable clipped to lie between minVal/maxVal */

int cartCgiUsualInt(struct cart *cart, char *var, int usual);
/* Look for var in CGI, then in cart, if not found then return usual. */

void cartSetInt(struct cart *cart, char *var, int val);
/* Set integer value. */

double cartDouble(struct cart *cart, char *var);
/* Return double valued variable. */

double cartUsualDouble(struct cart *cart, char *var, double usual);
/* Return variable value if it exists or usual if not. */

double cartCgiUsualDouble(struct cart *cart, char *var, double usual);
/* Look for var in CGI, then in cart, if not found then return usual. */

void cartSetDouble(struct cart *cart, char *var, double val);
/* Set double value. */

boolean cartBoolean(struct cart *cart, char *var);
/* Retrieve cart boolean. */

boolean cartUsualBoolean(struct cart *cart, char *var, boolean usual);
/* Return variable value if it exists or usual if not.  */

boolean cartCgiUsualBoolean(struct cart *cart, char *var, boolean usual);
/* Look for var in CGI, then in cart, if not found then return usual. */

void cartSetBoolean(struct cart *cart, char *var, boolean val);
/* Set boolean value. */

void cartMakeTextVar(struct cart *cart, char *var, char *defaultVal, int charSize);
/* Make a text control filled with value from cart if it exists or
 * default value otherwise.  If charSize is zero it's calculated to fit
 * current value.  Default value may be NULL. */

void cartMakeIntVar(struct cart *cart, char *var, int defaultVal, int maxDigits);
/* Make a text control filled with integer value - from cart if available
 * otherwise default.  */

void cartMakeDoubleVar(struct cart *cart, char *var, double defaultVal, int maxDigits);
/* Make a text control filled with integer value - from cart if available
 * otherwise default.  */

void cartMakeCheckBox(struct cart *cart, char *var, boolean defaultVal);
/* Make a check box filled with value from cart if it exists or
 * default value otherwise.  */

void cartMakeRadioButton(struct cart *cart, char *var, char *val, char *defaultVal);
/* Make a radio button that is selected if cart variable exists and matches
 * value (or value matches default val if cart var doesn't exist). */

void cartSaveSession(struct cart *cart);
/* Save session in a hidden variable. This needs to be called
 * somewhere inside of form or bad things will happen. */

void cartDump(struct cart *cart);
/* Dump contents of cart. */

void cartDumpList(struct hashEl *elList);
/* Dump list of cart variables. */

void cartDumpPrefix(struct cart *cart, char *prefix);
/* Dump all cart variables with prefix */

void cartDumpLike(struct cart *cart, char *wildcard);
/* Dump all cart variables matching wildcard */

struct hashEl *cartFindPrefix(struct cart *cart, char *prefix);
/* Return list of name/val pairs from cart where name starts with 
 * prefix.  Free when done with hashElFreeList. */

struct hashEl *cartFindLike(struct cart *cart, char *wildCard);
/* Return list of name/val pairs from cart where name matches 
 * wildcard.  Free when done with hashElFreeList. */

char *cartFindFirstLike(struct cart *cart, char *wildCard);
/* Find name of first variable that matches wildCard in cart. 
 * Return NULL if none. */

void cartResetInDb(char *cookieName);
/* Clear cart in database. */

void cartEarlyWarningHandler(char *format, va_list args);
/* Write an error message so user can see it before page is really started */

void cartWarnCatcher(void (*doMiddle)(struct cart *cart), struct cart *cart, WarnHandler warner);
/* Wrap error and warning handlers around doMiddl. */

void cartEmptyShell(void (*doMiddle)(struct cart *cart), char *cookieName, 
	char **exclude, struct hash *oldVars);
/* Get cart and cookies and set up error handling, but don't start writing any
 * html yet. The doMiddleFunction has to call cartHtmlStart(title), and
 * cartHtmlEnd(), as well as writing the body of the HTML. 
 * oldVars - those in cart that are overlayed by cgi-vars are
 * put in optional hash oldVars. */

void cartHtmlStart(char *title);
/* Write HTML header and put in normal error handler. Needed with cartEmptyShell,
 * but not cartHtmlShell. */

void cartHtmlEnd();
/* Write out HTML footer and get rid or error handler. Needed with cartEmptyShell,
 * but not cartHtmlShell. */

void cartWebStart(struct cart *theCart, char *format, ...);
/* Print out pretty wrapper around things when working
 * from cart. Balance this with cartWebEnd. */

void cartVaWebStart(struct cart *cart, char *format, va_list args);
/* Print out pretty wrapper around things when working
 * from cart. */

void cartWebEnd();
/* End out pretty wrapper around things when working
 * from cart. */

void cartHtmlShell(char *title, void (*doMiddle)(struct cart *cart), 
	char *cookieName, char **exclude, struct hash *oldVars);
/* Load cart from cookie and session cgi variable.  Write web-page preamble, call doMiddle
 * with cart, and write end of web-page.   Exclude may be NULL.  If it exists it's a
 * comma-separated list of variables that you don't want to save in the cart between
 * invocations of the cgi-script. oldVars is an optional hash that will get values
 * of things in the cart that were overwritten by cgi-variables. */

void cartHtmlShellPB(char *title, void (*doMiddle)(struct cart *cart),
        char *cookieName, char **exclude, struct hash *oldVars);
/* For Proteome Browser, load cart from cookie and session cgi variable.  Write web-page
 * preamble, call doMiddle with cart, and write end of web-page.
 * Exclude may be NULL.  If it exists it's a comma-separated list of
 * variables that you don't want to save in the cart between
 * invocations of the cgi-script. */

void cartHtmlShellPbGlobal(char *title, void (*doMiddle)(struct cart *cart),
        char *cookieName, char **exclude, struct hash *oldVars);
/* For Proteome Browser, load cart from cookie and session cgi variable.  Write web-page
 * preamble, call doMiddle with cart, and write end of web-page.
 * Exclude may be NULL.  If it exists it's a comma-separated list of
 * variables that you don't want to save in the cart between
 * invocations of the cgi-script. */

struct cart *cartAndCookie(char *cookieName, char **exclude, 
	struct hash *oldVars);
/* Load cart from cookie and session cgi variable.  Write cookie and 
 * content-type part HTTP preamble to web page.  Don't write any HTML though. */

struct cart *cartAndCookieNoContent(char *cookieName, char **exclude, 
	struct hash *oldVars);
/* Load cart from cookie and session cgi variable. Don't write out
 * content type or any HTML. */

struct cart *cartAndCookieWithHtml(char *cookieName, char **exclude, 
	struct hash *oldVars, boolean doContentType);
/* Load cart from cookie and session cgi variable.  Write cookie 
 * and optionally content-type part HTTP preamble to web page.  Don't 
 * write any HTML though. */

void cartSetDbConnector(DbConnector connector);
/* Set the connector that will be used by the cart to connect to the
 * database. Default connector is hConnectCart */

void cartSetDbDisconnector(DbDisconnect disconnector);
/* Set the connector that will be used by the cart to disconnect from the
 * database. Default disconnector is hDisconnectCart */


#endif /* CART_H */

