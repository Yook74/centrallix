#ifdef HAVE_CONFIG_H
#include "cxlibconfig-internal.h"
#endif
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include "xstring.h"
#include "mtask.h"
#include "newmalloc.h"

/************************************************************************/
/* Centrallix Application Server System 				*/
/* Centrallix Base Library						*/
/* 									*/
/* Copyright (C) 1998-2001 LightSys Technology Services, Inc.		*/
/* 									*/
/* You may use these files and this library under the terms of the	*/
/* GNU Lesser General Public License, Version 2.1, contained in the	*/
/* included file "COPYING".						*/
/* 									*/
/* Module: 	xstring.c, xstring.h					*/
/* Author:	Greg Beeley (GRB)					*/
/* Creation:	December 17, 1998					*/
/* Description:	Extensible string support module.  Implements an auto-	*/
/*		realloc'ing string data structure.			*/
/************************************************************************/

/**CVSDATA***************************************************************

    $Id: xstring.c,v 1.13 2003/04/25 15:57:18 gbeeley Exp $
    $Source: /srv/bld/centrallix-repo/centrallix-lib/src/xstring.c,v $

    $Log: xstring.c,v $
    Revision 1.13  2003/04/25 15:57:18  gbeeley
    Minor tweak to cxsec implementation in xstring.

    Revision 1.12  2003/04/04 03:18:20  gbeeley
    Added CXSEC_ENTRY and CXSEC_EXIT examples to xstring.

    Revision 1.11  2003/04/03 21:38:31  gbeeley
    Adding xsSubst() to the xstring library.  Does a replacement of a
    string with another based on position, not content.

    Revision 1.10  2003/04/03 04:32:39  gbeeley
    Added new cxsec module which implements some optional-use security
    hardening measures designed to protect data structures and stack
    return addresses.  Updated build process to have hardening and
    optimization options.  Fixed some build-related dependency checking
    problems.  Updated mtask to put some variables in registers even
    when not optimizing with -O.  Added some security hardening features
    to xstring as an example.

    Revision 1.9  2002/11/22 20:56:59  gbeeley
    Added xsGenPrintf(), fdPrintf(), and supporting logic.  These routines
    basically allow printf() style functionality on top of any xxxWrite()
    type of routine (such as fdWrite, objWrite, etc).

    Revision 1.8  2002/10/15 22:01:30  gbeeley
    A few minor tweaks.  This commit was originally going to fix the xstring
    null termination problems but evidently I hadn't done a cvs update in
    a while since jorupp already fixed em...

    Revision 1.7  2002/09/28 01:08:07  jorupp
     * added xsFindRev()
     * fixed a couple bugs that pop up when using those functions I added
        the other day on null-terminated strings -- I may not have them all yet...

    Revision 1.6  2002/09/21 01:59:33  jorupp
     * added some functions to xstring -- helps quite a bit for string manipulation :)

    Revision 1.5  2002/08/06 16:39:25  lkehresman
    Fixed a bug (that greg noticed) in the xsRTrim function that was
    inaccurately setting the string length.

    Revision 1.4  2002/08/06 16:00:29  lkehresman
    Added some xstring manipulation functions:
      xsRTrim - right trim whitespace
      xsLTrim - left trim whitespace
      xsTrim - xsRTrim && xsLTrim

    Revision 1.3  2001/10/03 15:48:09  gbeeley
    Added xsWrite() function to mimic fdWrite/objWrite for XStrings.

    Revision 1.2  2001/10/03 15:31:32  gbeeley
    Added xsPrintf and xsConcatPrintf functions to the xstring library.
    They currently support %s and %d with field width and precision.

    Revision 1.1.1.1  2001/08/13 18:04:23  gbeeley
    Centrallix Library initial import

    Revision 1.1.1.1  2001/07/03 01:02:57  gbeeley
    Initial checkin of centrallix-lib


 **END-CVSDATA***********************************************************/

#define XS_FN_KEY	(MGK_XSTRING & 0xFFFF00FF)

/*** xsInit - initialize an existing XString structure.  The structure
 *** MUST be allocated first.
 ***/
int 
xsInit(pXString this)
    {
    CXSEC_ENTRY(XS_FN_KEY);
    	
	/** Initialize the various fields. **/
	this->String = this->InitBuf;
	this->String[0] = '\0';
	this->AllocLen = XS_BLK_SIZ;
	this->Length = 0;
	SETMAGIC(this, MGK_XSTRING);
	CXSEC_INIT(*this);

    CXSEC_EXIT(XS_FN_KEY);
    return 0;
    }


/*** xsDeInit - deinitialize the XString structure, freeing any added
 *** memory occupied by the string data.
 ***/
int 
xsDeInit(pXString this)
    {
    CXSEC_ENTRY(XS_FN_KEY);

	ASSERTMAGIC(this, MGK_XSTRING);
	CXSEC_VERIFY(*this);

    	/** If AllocLen > XS_BLK_SIZ, we allocated the memory. **/
	if (this->AllocLen > XS_BLK_SIZ) nmSysFree(this->String);
	this->AllocLen = 256;
	this->String = this->InitBuf;

    CXSEC_EXIT(XS_FN_KEY);
    return 0;
    }


/*** xsCheckAlloc - optionally add more memory to the data if needed,
 *** given the currently occupied data area and the additional data
 *** required.
 ***/
int 
xsCheckAlloc(pXString this, int addl_needed)
    {
    CXSEC_ENTRY(XS_FN_KEY);
    int	new_cnt = 0;
    char* ptr;

	ASSERTMAGIC(this, MGK_XSTRING);
	CXSEC_VERIFY(*this);

    	/** See if more memory is needed. **/
	if (addl_needed > 0 && this->AllocLen < this->Length + addl_needed + 1)
	    {
	    new_cnt = (this->Length + addl_needed + XS_BLK_SIZ) & ~(XS_BLK_SIZ-1);

	    /** If only XS_BLK_SIZ alloc'd, we're using internal buffer. **/
	    if (this->AllocLen == XS_BLK_SIZ)
	        {
		ptr = (char*)nmSysMalloc(new_cnt);
		if (!ptr) 
		    {
		    CXSEC_EXIT(XS_FN_KEY);
		    return -1;
		    }
		memcpy(ptr,this->InitBuf,XS_BLK_SIZ);
		CXSEC_VERIFY(*this);
		this->String = ptr;
		this->AllocLen = new_cnt;
		CXSEC_UPDATE(*this);
		}
	    else
	        {
		ptr = (char*)nmSysRealloc(this->String, new_cnt);
		if (!ptr)
		    {
		    CXSEC_EXIT(XS_FN_KEY);
		    return -1;
		    }
		CXSEC_VERIFY(*this);
		this->String = ptr;
		this->AllocLen = new_cnt;
		CXSEC_UPDATE(*this);
		}
	    }

    CXSEC_EXIT(XS_FN_KEY);
    return 0;
    }


/*** xsConcatenate - adds text data to the end of the existing string, and
 *** allocs more memory as needed.  If 'len' is -1, then the length is 
 *** calculated using strlen(), otherwise the given length is enforced.
 ***/
int 
xsConcatenate(pXString this, char* text, int len)
    {
    CXSEC_ENTRY(XS_FN_KEY);

	ASSERTMAGIC(this, MGK_XSTRING);
	CXSEC_VERIFY(*this);

    	/** Determine length. **/
	if (len == -1) len = strlen(text);

    	/** Check memory **/
	if (xsCheckAlloc(this,len) < 0) 
	    {
	    CXSEC_EXIT(XS_FN_KEY);
	    return -1;
	    }

	/** Copy to end of string. **/
	CXSEC_VERIFY(*this);
	memcpy(this->String + this->Length, text, len);
	this->Length += len;
	this->String[this->Length] = '\0';
	CXSEC_UPDATE(*this);

    CXSEC_EXIT(XS_FN_KEY);
    return 0;
    }


/*** xsCopy - copies text data to the given string, overwriting anything
 *** that is already there.  As before, 'len' can be -1.  Semantics of 'len'
 *** are same as that of xsConcatenate.
 ***/
int 
xsCopy(pXString this, char* text, int len)
    {
    CXSEC_ENTRY(XS_FN_KEY);

	ASSERTMAGIC(this, MGK_XSTRING);
	CXSEC_VERIFY(*this);

	/** Reset length to 0 and concatenate. **/
	this->Length = 0;
	CXSEC_UPDATE(*this);
	if (xsConcatenate(this,text,len) < 0) 
	    {
	    CXSEC_EXIT(XS_FN_KEY);
	    return -1;
	    }

    CXSEC_EXIT(XS_FN_KEY);
    return 0;
    }


/*** xsStringEnd - returns a pointer to the end of the string so that
 *** routines like 'sprintf' can be used (assuming that the caller made
 *** sure there was enough room with xsCheckAlloc()...)
 ***/
char* 
xsStringEnd(pXString this)
    {
    CXSEC_ENTRY(XS_FN_KEY);
    ASSERTMAGIC(this, MGK_XSTRING);
    CXSEC_VERIFY(*this);
    CXSEC_EXIT(XS_FN_KEY);
    return this->String + this->Length;
    }


int
xs_internal_Printf(pXString this, char* fmt, va_list vl)
    {
    CXSEC_ENTRY(XS_FN_KEY);
    char* cur_pos;
    char* ptr;
    int n;
    char* str;
    char nbuf[32];
    int i;
    int field_width= -999;
    int precision= -999;
    char* nptr;
    int found_field_width=0;

	ASSERTMAGIC(this, MGK_XSTRING);
	CXSEC_VERIFY(*this);

	/** Go through the format string one snippet at a time **/
	cur_pos = fmt;
	ptr = strchr(cur_pos, '%');
	if (ptr)
	    {
	    n = ptr - cur_pos;
	    xsConcatenate(this, cur_pos, n);
	    }
	while(ptr)
	    {
	    switch(ptr[1])
	        {
		case '\0':
		    xsCheckAlloc(this,1);
		    CXSEC_VERIFY(*this);
		    this->String[this->Length++] = '%';
		    CXSEC_UPDATE(*this);
		    cur_pos = ptr+1;
		    break;
		case '*':
		    if (field_width == -999 && !found_field_width)
			{
			field_width = va_arg(vl, int);
			found_field_width = 1;
			}
		    else
			precision = va_arg(vl, int);
		    ptr++;
		    continue; /* loop continue */
		case '.':
		    ptr++;
		    found_field_width=1;
		    continue;
		case '-':
		case '+':
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		    if (field_width == -999 && !found_field_width)
			{
			field_width = strtol(ptr+1,&nptr,10);
			found_field_width=1;
			}
		    else
			precision = strtol(ptr+1,&nptr,10);
		    ptr = nptr-1;
		    continue;
		case '%':
		    xsCheckAlloc(this,1);
		    CXSEC_VERIFY(*this);
		    this->String[this->Length++] = '%';
		    CXSEC_UPDATE(*this);
		    cur_pos = ptr+2;
		    break;
		case 's':
		    /** Get the string value **/
		    str = va_arg(vl, char*);
		    if (!str) str = "(null)";

		  do_as_string:	    /* from int handler, below */
		    n = strlen(str);

		    /** Need to pad beginning of string with spaces? **/
		    if (field_width > precision && precision >= 0)
			field_width = precision;
		    if (field_width > 0 && field_width > n)
			{
			xsCheckAlloc(this,field_width-n);
			CXSEC_VERIFY(*this);
			memset(this->String+this->Length,' ',field_width-n);
			this->Length += (field_width-n);
			CXSEC_UPDATE(*this);
			}

		    /** Add the string.  Need to make it less than given length? **/
		    if (n > precision && precision >= 0)
			n = precision;
		    xsConcatenate(this, str, n);

		    /** Add trailing blanks? **/
		    if (field_width < 0 && field_width != -999 && (-field_width) > n)
			{
			CXSEC_VERIFY(*this);
			xsCheckAlloc(this,(-field_width)-n);
			memset(this->String+this->Length,' ',(-field_width)-n);
			this->Length += ((-field_width)-n);
			CXSEC_UPDATE(*this);
			}
		    cur_pos = ptr+2;
		    found_field_width=0;
		    field_width = -999;
		    precision = -999;
		    break;
		case 'd':
		    i = va_arg(vl, int);
		    if (precision > 0)
			{
			if (precision >= 31) precision=30;
			sprintf(nbuf,"%.*d",precision,i);
			precision=-999;
			}
		    else
			{
			sprintf(nbuf,"%d",i);
			}
		    str = nbuf;
		    goto do_as_string; /* next section up */
		default:
		    cur_pos = ptr+2;
		    break;
		}
	    ptr = strchr(cur_pos, '%');
	    if (ptr)
		{
		n = ptr - cur_pos;
		xsConcatenate(this, cur_pos, n);
		}
	    }
	if (*cur_pos) xsConcatenate(this, cur_pos, -1);
	CXSEC_VERIFY(*this);
	this->String[this->Length] = '\0';
	CXSEC_UPDATE(*this);

    CXSEC_EXIT(XS_FN_KEY);
    return 0;
    }


/*** xsConcatPrintf - prints to an XString using a printf-style format string,
 *** though the format string only allows %s and %d, no field widths or
 *** anything of that sort.
 ***/
int
xsConcatPrintf(pXString this, char* fmt, ...)
    {
    CXSEC_ENTRY(XS_FN_KEY);
    va_list vl;

	ASSERTMAGIC(this, MGK_XSTRING);
	CXSEC_VERIFY(*this);

	va_start(vl, fmt);
	xs_internal_Printf(this, fmt, vl);
	va_end(vl);

    CXSEC_EXIT(XS_FN_KEY);
    return 0;
    }


/*** xsPrintf - prints to an XString using a printf-style format string,
 *** though the format string only allows %s and %d, no field widths or
 *** anything of that sort.
 ***/
int
xsPrintf(pXString this, char* fmt, ...)
    {
    CXSEC_ENTRY(XS_FN_KEY);
    va_list vl;

	ASSERTMAGIC(this, MGK_XSTRING);
	CXSEC_VERIFY(*this);

	va_start(vl, fmt);
	this->Length = 0;
	CXSEC_UPDATE(*this);
	xs_internal_Printf(this, fmt, vl);
	va_end(vl);

    CXSEC_EXIT(XS_FN_KEY);
    return 0;
    }


/*** xsWrite - writes data into an XString using the standard fdWrite/
 *** objWrite type of API.  This function can thus be used as a 
 *** write_fn for those functions that require it (such as the 
 *** expGenerateText() function).  WARNING: XStrings can hold
 *** more or less unlimited data.  Don't use this function if the
 *** calling API might write nearly unlimited data to it.
 ***/
int
xsWrite(pXString this, char* buf, int len, int offset, int flags)
    {
    CXSEC_ENTRY(XS_FN_KEY);

	ASSERTMAGIC(this, MGK_XSTRING);
	CXSEC_VERIFY(*this);

	/** If offset not specified, just a simple concat. **/
	if (!(flags & XS_U_SEEK))
	    {
	    xsConcatenate(this, buf, len);
	    }
	else
	    {
	    /** Check to see if end is overflowed. **/
	    if (offset+len > this->Length)
		{
		xsCheckAlloc(this, (offset+len) - this->Length);
		this->Length = offset+len;
		this->String[this->Length] = '\0';
		}
	    
	    /** Copy the data to the appropriate string position **/
	    memcpy(this->String + offset, buf, len);
	    CXSEC_UPDATE(*this);
	    }

    CXSEC_EXIT(XS_FN_KEY);
    return len;
    }

/*** xsRTrim - Trims all the white space off of the right side
 *** of a given XString.  This is accomplished by moving the
 *** \0 character up to the desired location.
 ***/
int
xsRTrim(pXString this)
    {
    CXSEC_ENTRY(XS_FN_KEY);
    int i;

	ASSERTMAGIC(this, MGK_XSTRING);
	CXSEC_VERIFY(*this);

	for (i=this->Length-1; i >= 0 && 
		   (this->String[i] == '\r' ||
		    this->String[i] == '\n' ||
		    this->String[i] == '\t' ||
		    this->String[i] == ' '); i--);
	this->String[i+1] = '\0';
	this->Length = i+1;
	CXSEC_UPDATE(*this);

    CXSEC_EXIT(XS_FN_KEY);
    return 0;
    }

/*** xsLTrim - Trims all the white space off of the left side
 *** of a given XString.  This is accomplished by using memmove.
 ***/
int
xsLTrim(pXString this)
    {
    CXSEC_ENTRY(XS_FN_KEY);
    int i;

	ASSERTMAGIC(this, MGK_XSTRING);
	CXSEC_VERIFY(*this);

	for (i=0; i < this->Length && 
		   (this->String[i] == '\r' ||
		    this->String[i] == '\n' ||
		    this->String[i] == '\t' ||
		    this->String[i] == ' '); i++);
	memmove(this->String, this->String+i, this->Length-i);
	this->Length -= i;
	this->String[this->Length] = '\0';
	CXSEC_UPDATE(*this);

    CXSEC_EXIT(XS_FN_KEY);
    return 0;
    }

/*** xsTrim - Does both xsLTrim and xsRTrim on the given string
 ***/
int
xsTrim(pXString this)
    {
    CXSEC_ENTRY(XS_FN_KEY);

	ASSERTMAGIC(this, MGK_XSTRING);
	CXSEC_VERIFY(*this);

	xsLTrim(this);
	xsRTrim(this);

    CXSEC_EXIT(XS_FN_KEY);
    return 0;
    }

/*** xsFind - searches an xString for a string from an offset and returns the offset it was found at
 ***   returns -1 if not found
 ***/
int
xsFind(pXString this,char* find,int findlen, int offset)
    {
    CXSEC_ENTRY(XS_FN_KEY);
    ASSERTMAGIC(this, MGK_XSTRING);
    CXSEC_VERIFY(*this);
    if(findlen==-1) findlen=strlen(find);
    for(;offset<this->Length;offset++)
	{
	if(this->String[offset]==find[0])
	    {
	    int i;
	    for(i=1;i<findlen;i++)
		{
		if(this->String[offset+i]!=find[i])
		    break;
		}
	    if(i==findlen)
		{
		CXSEC_EXIT(XS_FN_KEY);
		return offset;
		}
	    }
	}
    CXSEC_EXIT(XS_FN_KEY);
    return -1;
    }

/*** xsFindReverse - searches an xString for a string from an offset (from the end) and returns the offset it was found at
 ***   returns -1 if not found
 ***/
int
xsFindRev(pXString this,char* find,int findlen, int offset)
    {
    CXSEC_ENTRY(XS_FN_KEY);
    ASSERTMAGIC(this, MGK_XSTRING);
    CXSEC_VERIFY(*this);
    if(findlen==-1) findlen=strlen(find);
    offset=this->Length-offset-1;
    for(;offset>=0;offset--)
	{
	if(this->String[offset]==find[0])
	    {
	    int i;
	    for(i=1;i<findlen;i++)
		{
		if(this->String[offset+i]!=find[i])
		    break;
		}
	    if(i==findlen)
		{
		CXSEC_EXIT(XS_FN_KEY);
		return offset;
		}
	    }
	}
    CXSEC_EXIT(XS_FN_KEY);
    return -1;
    }


/*** xsSubst - substitutes a string in a given position in an xstring.  does not
 *** search for matches like xsreplace does - you have to tell it the position
 *** and length to replace.  Length of -1 indicates length is unknown.
 ***/
int
xsSubst(pXString this, int offset, int len, char* rep, int replen)
    {
    CXSEC_ENTRY(XS_FN_KEY);

	ASSERTMAGIC(this, MGK_XSTRING);
	CXSEC_VERIFY(*this);

	/** Figure some default lengths **/
	if (offset > this->Length || offset < 0) 
	    {
	    CXSEC_EXIT(XS_FN_KEY);
	    return -1;
	    }
	if (len == -1) len = strlen(this->String + offset);
	if (replen == -1) replen = strlen(rep);

	/** Make sure we have enough room **/
	if (len < replen) xsCheckAlloc(this, replen - len);

	/** Move the tail of the string, and plop the replacement in there **/
	memmove(this->String+offset+replen, this->String+offset+len, this->Length + 1 - (offset+len));
	memcpy(this->String+offset, rep, replen);
	this->Length += (replen - len);
	CXSEC_UPDATE(*this);

    CXSEC_EXIT(XS_FN_KEY);
    return 0;
    }


/*** xsReplace - searches an xString for a string and replaces that string with another
 ***   returns the starting offset of the replace if successful, and -1 if not found
 ***/
int
xsReplace(pXString this, char* find, int findlen, int offset, char* rep, int replen)
    {
    CXSEC_ENTRY(XS_FN_KEY);
    ASSERTMAGIC(this, MGK_XSTRING);
    CXSEC_VERIFY(*this);
    if(findlen==-1) findlen=strlen(find);
    if(replen==-1) replen=strlen(rep);
    offset=xsFind(this,find,findlen,offset);
    if(offset < 0) 
	{
	CXSEC_EXIT(XS_FN_KEY);
	return -1;
	}
    if(findlen>=replen)
	{
	memcpy(&(this->String[offset]),rep,replen);
	if(replen!=findlen)
	    {
	    memmove(this->String+offset+replen,this->String+offset+findlen,this->Length-offset-findlen+1);
	    this->Length-=findlen-replen;
	    }
	}
    else
	{
	/** warning: untested code :) **/
	xsCheckAlloc(this,replen-findlen);
	memmove(this->String+offset+replen,this->String+offset+findlen,this->Length-offset-findlen+1);
	memcpy(&(this->String[offset]),rep,replen);
	this->Length+=replen-findlen;
	}
    this->String[this->Length] = '\0';
    CXSEC_UPDATE(*this);
    CXSEC_EXIT(XS_FN_KEY);
    return offset;
    }

/*** xsInsertAfter - inserts the supplied string at offset -- returns new offset
 ***/
int
xsInsertAfter(pXString this, char* ins, int inslen, int offset)
    {
    CXSEC_ENTRY(XS_FN_KEY);
    ASSERTMAGIC(this, MGK_XSTRING);
    CXSEC_VERIFY(*this);
    if(inslen==-1) inslen=strlen(ins);
    if(xsCheckAlloc(this,inslen)==-1) 
	{
	CXSEC_EXIT(XS_FN_KEY);
	return -1;
	}
    memmove(this->String+offset+inslen,this->String+offset,this->Length-offset+1);
    memcpy(this->String+offset,ins,inslen);
    this->Length+=inslen;
    CXSEC_UPDATE(*this);
    CXSEC_EXIT(XS_FN_KEY);
    return offset+inslen;
    }


/*** xsGenPrintf - generic printf() operation to a xxxWrite() style function.
 *** This routine isn't really all that closely tied to the XString module,
 *** but this seemed to be the best place for it.  If a 'buf' and 'buf_size'
 *** are supplied (NULL otherwise), then buf MUST be allocated with the
 *** nmSysMalloc() routine.  Otherwise, kaboom!  This routine will grow
 *** 'buf' if it is too small, and will update 'buf_size' accordingly.
 ***
 *** Returns -(errno) on failure and the printed length (>= 0) on success.
 ***/
int
xsGenPrintf_va(int (*write_fn)(), void* write_arg, char** buf, int* buf_size, const char* fmt, va_list va)
    {
    CXSEC_ENTRY(XS_FN_KEY);
    va_list orig_va;
    char* mybuf = NULL;
    int mybuf_size = 0;
    char* new_buf;
    int new_buf_size;
    int rval=0,len;

	/** Init the varargs **/
	orig_va = va;

	/** Allocate a buffer, if caller didn't **/
	if (!buf)
	    {
	    buf = &mybuf;
	    buf_size = &mybuf_size;
	    *buf_size = XS_PRINTF_BUFSIZ;
	    *buf = (char*)nmSysMalloc(*buf_size);
	    if (!*buf) 
		{
		CXSEC_EXIT(XS_FN_KEY);
		return -ENOMEM;
		}
	    }

	/** Try to print the formatted string. **/
	while(1)
	    {
	    len = vsnprintf(*buf, *buf_size, fmt, va);

	    /** Truncated? **/
	    if (len == -1 || len > ((*buf_size) - 1))
		{
		/** Grab a bigger box **/
		nmSysFree(*buf);
		new_buf_size = (*buf_size)*2;
		while(new_buf_size < len) new_buf_size *= 2;
		new_buf = (char*)nmSysMalloc(new_buf_size);
		if (!new_buf) 
		    {
		    CXSEC_EXIT(XS_FN_KEY);
		    return -ENOMEM;
		    }
		*buf = new_buf;
		*buf_size = new_buf_size;
		va = orig_va;
		}
	    else
		{
		break;
		}
	    }

	/** Ok, got it.  Now send it to the output function **/
	CXSEC_EXIT(XS_FN_KEY); /* do it here too to protect the write_fn argument on the stack */
	rval = write_fn(write_arg, *buf, len, 0, FD_U_PACKET);
	if (mybuf) nmSysFree(mybuf);
	if (rval != len) rval = -EIO; /* oops!!! routine ignored FD_U_PACKET! */

    CXSEC_EXIT(XS_FN_KEY);
    return rval;
    }

int
xsGenPrintf(int (*write_fn)(), void* write_arg, char** buf, int* buf_size, const char* fmt, ...)
    {
    CXSEC_ENTRY(XS_FN_KEY);
    va_list va;
    int rval;

	va_start(va, fmt);
	rval = xsGenPrintf_va(write_fn, write_arg, buf, buf_size, fmt, va);
	va_end(va);

    CXSEC_EXIT(XS_FN_KEY);
    return rval;
    }

