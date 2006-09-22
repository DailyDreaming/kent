/* hPrint - turning html printing on and off, which is useful
 * when postscript and PDF images are being drawn  */

#include "hPrint.h"

boolean suppressHtml = FALSE;
/* If doing PostScript output we'll suppress most of HTML output. */

void hPrintDisable()
/* turn html printing off */
{
suppressHtml = TRUE;
}

void hPrintEnable()
/* turn html printing on */
{
suppressHtml = FALSE;
}

void hvPrintf(char *format, va_list args)
/* Suppressable variable args printf. Check for write error so we can
 * terminate if http connection breaks. */
{
if (suppressHtml)
    return;
vprintf(format, args);
if (ferror(stdout))
    noWarnAbort();
}

void hPrintf(char *format, ...)
/* Printf that can be suppressed if not making html. */
{
va_list(args);
va_start(args, format);
hvPrintf(format, args);
va_end(args);
}

void hPrintNonBreak(char *s)
/* Print out string but replace spaces with &nbsp; */
{
char c;

if (suppressHtml)
    return;
while ((c = *s++) != '\0')
    {
    if (c == ' ')
	fputs("&nbsp;", stdout);
    else
        putchar(c);
    }
}

void hPuts(char *string)
/* Puts that can be suppressed if not making
 * html. */
{
if (!suppressHtml)
    puts(string);
}

void hPutc(char c)
/* putc that can be suppressed if not making html. */
{
if (!suppressHtml)
    fputc(c, stdout);
}

void hWrites(char *string)
/* Write string with no '\n' if not suppressed. */
{
if (!suppressHtml)
    fputs(string, stdout);
}

void hButton(char *name, char *label)
/* Write out button if not suppressed. */
{
if (!suppressHtml)
    cgiMakeButton(name, label);
}

void hOnClickButton(char *command, char *label)
/* Write out push button if not suppressed. */
{
if (!suppressHtml)
    cgiMakeOnClickButton(command, label);
}

void hTextVar(char *varName, char *initialVal, int charSize)
/* Write out text entry field if not suppressed. */
{
if (!suppressHtml)
    cgiMakeTextVar(varName, initialVal, charSize);
}

void hIntVar(char *varName, int initialVal, int maxDigits)
/* Write out numerical entry field if not supressed. */
{
if (!suppressHtml)
    cgiMakeIntVar(varName, initialVal, maxDigits);
}

void hDoubleVar(char *varName, double initialVal, int maxDigits)
/* Write out numerical entry field if not supressed. */
{
if (!suppressHtml)
    cgiMakeDoubleVar(varName, initialVal, maxDigits);
}

void hCheckBox(char *varName, boolean checked)
/* Make check box if not suppressed. */
{
if (!suppressHtml)
    cgiMakeCheckBox(varName, checked);
}

void hDropList(char *name, char *menu[], int menuSize, char *checked)
/* Make a drop-down list with names if not suppressed. */
{
if (!suppressHtml)
    cgiMakeDropList(name, menu, menuSize, checked);
}

void printHtmlComment(char *format, ...)
/* Function to print output as a comment so it is not seen in the HTML
 * output but only in the HTML source. */
{
va_list(args);
va_start(args, format);
hWrites("\n<!-- DEBUG: ");
hvPrintf(format, args);
hWrites(" -->\n");
fflush(stdout); /* USED ONLY FOR DEBUGGING BECAUSE THIS IS SLOW - MATT */
va_end(args);
}

