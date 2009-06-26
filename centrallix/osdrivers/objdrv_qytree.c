#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include "obj.h"
#include "cxlib/mtask.h"
#include "cxlib/xarray.h"
#include "cxlib/xhash.h"
#include "cxlib/xstring.h"
#include "stparse.h"
#include "st_node.h"
#include "expression.h"
#include "cxlib/mtsession.h"

/************************************************************************/
/* Centrallix Application Server System 				*/
/* Centrallix Core       						*/
/* 									*/
/* Copyright (C) 1998-2001 LightSys Technology Services, Inc.		*/
/* 									*/
/* This program is free software; you can redistribute it and/or modify	*/
/* it under the terms of the GNU General Public License as published by	*/
/* the Free Software Foundation; either version 2 of the License, or	*/
/* (at your option) any later version.					*/
/* 									*/
/* This program is distributed in the hope that it will be useful,	*/
/* but WITHOUT ANY WARRANTY; without even the implied warranty of	*/
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the	*/
/* GNU General Public License for more details.				*/
/* 									*/
/* You should have received a copy of the GNU General Public License	*/
/* along with this program; if not, write to the Free Software		*/
/* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  		*/
/* 02111-1307  USA							*/
/*									*/
/* A copy of the GNU General Public License has been included in this	*/
/* distribution in the file "COPYING".					*/
/* 									*/
/* Module: 	objdrv_qytree.c         				*/
/* Author:	Greg Beeley (GRB)					*/
/* Creation:	December 17, 1998					*/
/* Description:	Query Tree object driver.  This driver uses a structure	*/
/*		file (see stparse.[ch]) for its node, which contains	*/
/*		a set of queries which rearrange the appearance of data	*/
/*		contained in other directory structures.  A powerful 	*/
/*		analogy to a database's VIEW.				*/
/************************************************************************/

/**CVSDATA***************************************************************

    $Id: objdrv_qytree.c,v 1.17 2009/06/26 16:39:00 gbeeley Exp $
    $Source: /srv/bld/centrallix-repo/centrallix/osdrivers/objdrv_qytree.c,v $

    $Log: objdrv_qytree.c,v $
    Revision 1.17  2009/06/26 16:39:00  gbeeley
    - (feature) adding a 'known_leaf' option to querytree nodes which have
      recursion enabled, which helps SUBTREE select efficiency among other
      things since queries don't need to be issued to find the nonexistent
      child nodes of a known_leaf (if leaf status can be determined by the
      data in the object itself rather than by the existence/nonexistence of
      other objects)

    Revision 1.16  2008/03/09 08:02:43  gbeeley
    - (bugfix) you can't objUnmanageQuery a query which is NULL...

    Revision 1.15  2006/04/07 06:47:23  gbeeley
    - (bugfix) Don't try to compile the expression if we don't have any objects
      open in the first place.
    - (bugfix) memory_leaks -= 5;

    Revision 1.14  2005/02/26 06:42:39  gbeeley
    - Massive change: centrallix-lib include files moved.  Affected nearly
      every source file in the tree.
    - Moved all config files (except centrallix.conf) to a subdir in /etc.
    - Moved centrallix modules to a subdir in /usr/lib.

    Revision 1.13  2004/12/31 04:25:49  gbeeley
    - allow querytree 'text' objects to have readable content (this helps in
      situations like the index.app sample file).

    Revision 1.12  2004/09/01 02:36:27  gbeeley
    - get rid of last_modification warnings on qyt static elements by setting
      static element last_modification to that of the node itself.

    Revision 1.11  2004/08/30 19:07:07  gbeeley
    - add driver-opened-objects management to qytree to avoid double closes
      on objects when the session times out.

    Revision 1.10  2004/08/30 03:15:56  gbeeley
    - various bugfixes for qytree driver.  This driver needs a security audit
      as well.

    Revision 1.9  2004/06/23 21:33:55  mmcgill
    Implemented the ObjInfo interface for all the drivers that are currently
    a part of the project (in the Makefile, in other words). Authors of the
    various drivers might want to check to be sure that I didn't botch any-
    thing, and where applicable see if there's a neat way to keep track of
    whether or not an object actually has subobjects (I did not set this flag
    unless it was immediately obvious how to test for the condition).

    Revision 1.8  2004/06/12 00:10:15  mmcgill
    Chalk one up under 'didn't understand the build process'. The remaining
    os drivers have been updated, and the prototype for objExecuteMethod
    in obj.h has been changed to match the changes made everywhere it's
    called - param is now of type pObjData, not void*.

    Revision 1.7  2004/02/24 20:11:53  gbeeley
    - better annotation support
    - fixed query-related pathname bug

    Revision 1.6  2003/06/04 19:17:44  gbeeley
    Fixed bug in querytree relating to the use of string query criteria.

    Revision 1.5  2002/11/22 19:29:37  gbeeley
    Fixed some integer return value checking so that it checks for failure
    as "< 0" and success as ">= 0" instead of "== -1" and "!= -1".  This
    will allow us to pass error codes in the return value, such as something
    like "return -ENOMEM;" or "return -EACCESS;".

    Revision 1.4  2002/08/10 02:09:45  gbeeley
    Yowzers!  Implemented the first half of the conversion to the new
    specification for the obj[GS]etAttrValue OSML API functions, which
    causes the data type of the pObjData argument to be passed as well.
    This should improve robustness and add some flexibilty.  The changes
    made here include:

        * loosening of the definitions of those two function calls on a
          temporary basis,
        * modifying all current objectsystem drivers to reflect the new
          lower-level OSML API, including the builtin drivers obj_trx,
          obj_rootnode, and multiquery.
        * modification of these two functions in obj_attr.c to allow them
          to auto-sense the use of the old or new API,
        * Changing some dependencies on these functions, including the
          expSetParamFunctions() calls in various modules,
        * Adding type checking code to most objectsystem drivers.
        * Modifying *some* upper-level OSML API calls to the two functions
          in question.  Not all have been updated however (esp. htdrivers)!

    Revision 1.3  2001/10/16 23:53:02  gbeeley
    Added expressions-in-structure-files support, aka version 2 structure
    files.  Moved the stparse module into the core because it now depends
    on the expression subsystem.  Almost all osdrivers had to be modified
    because the structure file api changed a little bit.  Also fixed some
    bugs in the structure file generator when such an object is modified.
    The stparse module now includes two separate tree-structured data
    structures: StructInf and Struct.  The former is the new expression-
    enabled one, and the latter is a much simplified version.  The latter
    is used in the url_inf in net_http and in the OpenCtl for objects.
    The former is used for all structure files and attribute "override"
    entries.  The methods for the latter have an "_ne" addition on the
    function name.  See the stparse.h and stparse_ne.h files for more
    details.  ALMOST ALL MODULES THAT DIRECTLY ACCESSED THE STRUCTINF
    STRUCTURE WILL NEED TO BE MODIFIED.

    Revision 1.2  2001/09/27 19:26:23  gbeeley
    Minor change to OSML upper and lower APIs: objRead and objWrite now follow
    the same syntax as fdRead and fdWrite, that is the 'offset' argument is
    4th, and the 'flags' argument is 5th.  Before, they were reversed.

    Revision 1.1.1.1  2001/08/13 18:01:07  gbeeley
    Centrallix Core initial import

    Revision 1.2  2001/08/07 19:31:53  gbeeley
    Turned on warnings, did some code cleanup...

    Revision 1.1.1.1  2001/08/07 02:31:06  gbeeley
    Centrallix Core Initial Import


 **END-CVSDATA***********************************************************/


/*** Structure used by this driver internally. ***/
typedef struct 
    {
    char	Pathname[256];
    int		Flags;
    pObject	Obj;
    int		Mask;
    pStructInf	NodeData;
    pSnNode	BaseNode;
    pObject	LLObj;
    int		Offset;
    }
    QytData, *pQytData;

#define QYT_F_ISTEXT	1

#define QYT(x) ((pQytData)(x))

#define QYT_MAX_CACHE	32


/*** Structure used by queries for this driver. ***/
typedef struct
    {
    pQytData	ObjInf;
    pObjQuery	Query;
    char	NameBuf[256];
    char*	QyText;
    pObject	LLQueryObj;
    pObjQuery	LLQuery;
    int		NextSubInfID;
    char*	ItemText;
    char*	ItemSrc;
    char*	ItemWhere;
    XHashTable	StructTable;
    }
    QytQuery, *pQytQuery;


/*** Structure used for attr/obj cache ***/
typedef struct
    {
    char	ObjPathname[256];
    XString	AttrNameBuf;
    XString	AttrValueBuf;
    XArray	AttrNames;
    XArray	AttrValues;
    time_t	CacheTime;
    int		LinkCnt;
    }
    QytObjAttr, *pQytObjAttr;


/*** Globals - including object attribute cache. ***/
struct
    {
    XHashTable	ObjAttrCache;
    XArray	ObjAttrCacheList;
    XHashTable	NodeCache;
    }
    QYT_INF;


/*** qyt_internal_ReadObject - reads an object, with its attributes (blobs
 *** limited to 255 bytes), and stores the data in the object-attr cache,
 *** or looks the object up there if it has already been cached.
 ***/
pQytObjAttr
qyt_internal_ReadObject(char* path, pObjSession s)
    {
    pQytObjAttr objattr;
    pObject obj;
    char* attrname;
    int type;
    int ival;
    char* sval;

    	/** Check the cache first. **/
	objattr = (pQytObjAttr)xhLookup(&QYT_INF.ObjAttrCache, path);
	if (objattr) 
	    {
	    objattr->CacheTime = time(NULL);
	    objattr->LinkCnt++;
	    return objattr;
	    }

	/** Otherwise, open the object & read its attrs. **/
	obj = objOpen(s, path, O_RDONLY, 0600, "system/file");
	if (!obj) return NULL;

	/** Allocate... **/
	objattr = (pQytObjAttr)nmMalloc(sizeof(QytObjAttr));
	if (!objattr)
	    {
	    objClose(obj);
	    return NULL;
	    }
	memccpy(objattr->ObjPathname,path,0,255);
	objattr->ObjPathname[255]=0;
	xsInit(&objattr->AttrNameBuf);
	xsInit(&objattr->AttrValueBuf);
	xaInit(&objattr->AttrNames,32);
	xaInit(&objattr->AttrValues,32);

	/** Loop through attributes. **/
	for(attrname=objGetFirstAttr(obj);attrname;attrname=objGetNextAttr(obj))
	    {
	    /** Copy attrname to names listing.  Be sure to copy the '\0'. **/
	    xaAddItem(&objattr->AttrNames, xsStringEnd(&objattr->AttrNameBuf));
	    xsConcatenate(&objattr->AttrNameBuf, attrname, strlen(attrname)+1);

	    /** Get type and copy to attrvalues. **/
	    type = objGetAttrType(obj,attrname);
	    switch(type)
	        {
		case DATA_T_INTEGER:
		    if (objGetAttrValue(obj,attrname,DATA_T_INTEGER,POD(&ival)) == 0)
		        {
		        xaAddItem(&objattr->AttrValues, xsStringEnd(&objattr->AttrValueBuf));
			xsConcatenate(&objattr->AttrValueBuf, (char*)&ival, 4);
			}
		    else
		        {
		        xaAddItem(&objattr->AttrValues, NULL);
			}
		    break;

		case DATA_T_STRING:
		    if (objGetAttrValue(obj,attrname,DATA_T_INTEGER,POD(&sval)) == 0)
		        {
		        xaAddItem(&objattr->AttrValues, xsStringEnd(&objattr->AttrValueBuf));
		        xsConcatenate(&objattr->AttrValueBuf,sval, strlen(sval)+1);
			}
		    else
		        {
		        xaAddItem(&objattr->AttrValues, NULL);
			}
		    break;

		default:
		    break;
		}
	    }

	/** Close up the object **/
	objClose(obj);

	/** Cache the objattr structure **/
	xhAdd(&QYT_INF.ObjAttrCache, objattr->ObjPathname, (void*)objattr);
	xaAddItem(&QYT_INF.ObjAttrCacheList, (void*)objattr);

    return objattr;
    }


/*** qyt_internal_AttrValue - retrieves an attribute value from the objattr
 *** structure, and returns a pointer to that value, whether a 4-byte int or 
 *** an array of char.
 ***/
void*
qyt_internal_AttrValue(pQytObjAttr objattr, char* attrname)
    {
    int i;
    char* name;

    	/** Scan through the attrnames. **/
	for(i=0;i<objattr->AttrNames.nItems;i++)
	    {
	    name = (char*)(objattr->AttrNames.Items[i]);
	    if (!strcmp(name,attrname)) return (void*)(objattr->AttrValues.Items[i]);
	    }

    return NULL;
    }


#if 0
/*** qyt_internal_SubstWhere - perform parameterized substitution on a WHERE 
 *** clause '::' elements (for parent references)
 ***/
int
qyt_internal_SubstWhere()
    {

	/** Perform any needed substitution on the parameterized where clause **/
	if (*where_clause)
	    {
	    n_subst = 0;
	    ptr = where_clause;
	    while(ptr = strstr(ptr,"::"))
	        {
		ptr += 2;

		/** Get the attribute name after the :: **/
		i = 0;
		while(((*ptr >= 'A' && *ptr <= 'Z') || (*ptr >= 'a' && *ptr <= 'z') || 
		      (*ptr >= '0' && *ptr <= '9') || *ptr == '_') && i < 32)
		    {
		    attrname[i++] = *(ptr++);
		    }
		attrname[i] = 0;

		/** Get the attr type and string -or- integer value **/
		if (qy->ObjInf->LLObj && (subst_types[n_subst] = objGetAttrType(qy->ObjInf->LLObj, attrname)) >= 0)
		    {
		    if (subst_types[n_subst] == DATA_T_INTEGER) 
		        {
			objGetAttrValue(qy->ObjInf->LLObj, attrname, DATA_T_INTEGER, POD(&(subst_int[n_subst])));
			len += 12;
			}
		    else if (subst_types[n_subst] == DATA_T_STRING)
		        {
			objGetAttrValue(qy->ObjInf->LLObj, attrname, DATA_T_INTEGER, POD(&(subst_str[n_subst])));
			len += strlen(subst_str[n_subst]);
			}
		    n_subst++;
		    }
		else
		    {
		    mssError(0, "QYT", "Parent attribute in .qyt file where clause does not exist.");
		    return NULL;
		    }
		}
	    }

	/** Allocate memory for the new where clause **/
	if (len) qy->QyText = (char*)nmMalloc(len);
	if (len && !(qy->QyText)) 
	    {
	    return NULL;
	    }

	/** Build the "final" where clause to use **/
	ptr = qy->QyText;
	if (*usr_where_clause)
	    {
	    ptr = memccpy(ptr, "(", '\0', len)-1;
	    ptr = memccpy(ptr, usr_where_clause, '\0', len)-1;
	    ptr = memccpy(ptr, ")", '\0', len)-1;
	    }
	if (*where_clause)
            {
            if (*usr_where_clause)
	        ptr = memccpy(ptr, " AND (", '\0', len)-1;
	    else
	        ptr = memccpy(ptr, "(", '\0', len)-1;
            sptr = where_clause;
            i = 0;
            while(*sptr)
                {
                if (*sptr == ':' && *(sptr+1) == ':')
                    {
                    if (subst_types[i] == DATA_T_INTEGER)
                        {
                        sprintf(nbuf,"%d",subst_int[i]);
                        ptr = memccpy(ptr, nbuf, '\0', 16)-1;
                        }
                    else
                        {
                        *(ptr++) = '\'';
                        ptr = memccpy(ptr, subst_str[i], '\0', len)-1;
                        *(ptr++) = '\'';
                        }
                    sptr += 2;
                    while(((*sptr >= 'A' && *sptr <= 'Z') || (*sptr >= 'a' && *sptr <= 'z') || 
                           (*sptr >= '0' && *sptr <= '9') || *sptr == '_')) sptr++;
                    }
                else
                    {
                    *(ptr++) = *(sptr++);
                    }
                }
            ptr = memccpy(ptr, ")", '\0', len)-1;
            }
	*ptr = '\0';

    return 0;
    }
#endif


/*** qyt_internal_ProcessPath - handles the lookup of the snNode from the given
 *** path in the querytree, and the optional creation of a new entity, as needed,
 *** manipulating the WHERE sql in the snNode to assign parent foreign key links
 *** and so forth.
 ***/
pQytData
qyt_internal_ProcessPath(pObjSession s, pPathname path, pSnNode node, int subref, pStructInf dptr, int create, int no_open)
    {
    pQytData inf;
    pStructInf lookup_inf, find_inf, next_inf;
    char* strval;
    char* exprval;
    char* ptr;
    int i,v;
    pExpression expr;
    pParamObjects objlist;
    pObject test_obj;
    XHashTable struct_table;

    	/** Setup the pathname into its subparts **/
	for(i=1;i<path->nElements;i++) path->Elements[i][-1] = 0;

    	/** Allocate the data info first **/
	inf = (pQytData)nmMalloc(sizeof(QytData));
	if (!inf) return NULL;
	memset(inf,0,sizeof(QytData));
	inf->BaseNode = node;
	inf->LLObj = NULL;
	inf->Offset = 0;

	/** Create the object parameter list... **/
        objlist = expCreateParamList();

	/** Initialize the listing of query tree elements **/
	xhInit(&struct_table, 17, 0);

	/** Look for the part of the structinf which this path references. **/
	while(subref < path->nElements)
	    {
	    /** Add a slot to the param object list for this tree level **/
	    objlist->ParentID = objlist->CurrentID;
	    expAddParamToList(objlist,"",NULL,EXPR_O_CURRENT);

	    /** Look for text and source items in the querytree definition. **/
	    next_inf = NULL;
	    for(i=0;i<dptr->nSubInf;i++) 
	        {
		find_inf = dptr->SubInf[i];
		if (stStructType(find_inf) == ST_T_SUBGROUP)
                    {
	    PROCESS_SUBGROUP:
                    inf->Pathname[0] = '\0';
                    if ((lookup_inf = stLookup(find_inf, "text")))
                        {
                        strval = "";
                        stAttrValue(lookup_inf,NULL,&strval,0);
                        if (!strcmp(path->Elements[subref],strval))
                            {
                            next_inf = find_inf;
                            break;
                            }
                        if (subref == path->nElements - 1)
                            inf->LLObj = NULL;
                        }
                    else if ((lookup_inf = stLookup(find_inf, "source")))
                        {
                        strval = "";
                        stAttrValue(lookup_inf,NULL,&strval,0);
                        expr = NULL;
                        exprval = NULL;
                        stAttrValue(stLookup(find_inf, "where"),NULL,&exprval,0);
                        if (exprval && !no_open) 
			    {
			    objlist->Names[(signed char)(objlist->CurrentID)] = find_inf->Name;
			    expr = (pExpression)expCompileExpression(exprval, objlist, MLX_F_ICASE | MLX_F_FILENAMES, 0);
			    }
                        snprintf(inf->Pathname,sizeof(inf->Pathname),"%s/%s?ls__type=application%%2foctet-stream",strval,path->Elements[subref]);

			/** Setup this querytree struct entry for lookups **/
			xhAdd(&struct_table, find_inf->Name, (void*)find_inf);

			/** Not opening any objects?  Skip open attempt if so. **/
			if (no_open)
			    {
			    next_inf = find_inf;
			    break;
			    }
    
                        /** Open the object, or create if need be.  If create, convert any WHERE to setattrs **/
                        test_obj = objOpen(s, inf->Pathname, O_RDONLY, 0600, "system/file");
			if (test_obj) 
			    {
			    expModifyParam(objlist, NULL, test_obj);
			    }
                        if (!test_obj && subref == path->nElements - 1 && create)
                            {
                            test_obj = objOpen(s, inf->Pathname, O_RDWR | O_CREAT | O_TRUNC, 0600, "system/file");
                            if (!test_obj) break;
			    expModifyParam(objlist, NULL, test_obj);
			    objlist->Flags[(signed char)(objlist->CurrentID)] |= EXPR_O_UPDATE;
                            if (expr)
                                {
				/** Reverse eval the expression to set the object attributes **/
				expr->Integer = 1;
				v = expReverseEvalTree(expr, objlist);
				expFreeExpression(expr);
				expr = NULL;
				if (v < 0)
				    {
				    objClose(test_obj);
				    break;
				    }
                                }
                            }
                        else if (!test_obj) break;
    
                        /** Validate the where clause expression if need be. **/
			objlist->Session = s;
                        v = !expr || (expEvalTree(expr,objlist) >= 0 && expr->Integer != 0);
                        if (expr) expFreeExpression(expr);
			expr = NULL;
                        if (subref == path->nElements - 1) 
			    {
			    objUnmanageObject(test_obj->Session, test_obj);
                            inf->LLObj = test_obj;
			    }
                        else 
			    {
                            if (!v) objClose(test_obj);
			    }
			if (v)
			    {
			    next_inf = find_inf;
			    break;
			    }
                        }
                    }
		else if (!strcmp(find_inf->Name,"recurse"))
		    {
		    expr = NULL;
		    exprval = NULL;
		    stAttrValue(stLookup(dptr, "known_leaf"),NULL,&exprval,0);
		    if (exprval) 
			{
			objlist->Names[(signed char)(objlist->CurrentID)] = find_inf->Name;
			expr = (pExpression)expCompileExpression(exprval, objlist, MLX_F_ICASE | MLX_F_FILENAMES, 0);
			if (!expr)
			    {
			    mssError(0,"QYT","Error in known_leaf expression");
			    break;
			    }
			else
			    {
			    v = (expEvalTree(expr,objlist) >= 0 && expr->Integer != 0);
			    expFreeExpression(expr);
			    expr = NULL;
			    if (v) break;
			    }
			}
		    stGetAttrValue(find_inf, DATA_T_STRING, POD(&ptr), 0);
		    find_inf = (pStructInf)xhLookup(&struct_table, ptr);
		    if (find_inf) goto PROCESS_SUBGROUP;
		    }
		}

	    /** Didn't find? **/
	    if (!next_inf) 
	        {
		/** Ok, close up the structure table. **/
		xhClear(&struct_table, NULL, NULL);
		xhDeInit(&struct_table);
		nmFree(inf,sizeof(QytData));
		inf->LLObj = NULL;

		/** Close the objects and free the param list. **/
		for(i=0;i<objlist->nObjects-1;i++) if (objlist->Objects[i]) objClose(objlist->Objects[i]);
		expFreeParamList(objlist);
		mssError(0,"QYT","Could not find object via access through querytree");
		return NULL;
		}
	    dptr = next_inf;
	    subref++;
	    }
	inf->NodeData = dptr;
	xhClear(&struct_table, NULL, NULL);
	xhDeInit(&struct_table);
	for(i=0;i<objlist->nObjects-1;i++) if (objlist->Objects[i]) objClose(objlist->Objects[i]);
	expFreeParamList(objlist);

    return inf;
    }


/*** qytOpen - open a file or directory.
 ***/
void*
qytOpen(pObject obj, int mask, pContentType systype, char* usrtype, pObjTrxTree* oxt)
    {
    pQytData inf;
    char* node_path;
    pSnNode node = NULL;

	/** Determine node path **/
	node_path = obj_internal_PathPart(obj->Pathname, 0, obj->SubPtr);

	/** If CREAT and EXCL, we only create, failing if already exists. **/
	if ((obj->Mode & O_CREAT) && (obj->Mode & O_EXCL) && (obj->SubPtr == obj->Pathname->nElements))
	    {
	    node = snNewNode(obj->Prev, usrtype);
	    if (!node)
	        {
		mssError(0,"QYT","Could not create new querytree node object");
		return NULL;
		}
	    }
	
	/** Otherwise, try to open it first. **/
	if (!node)
	    {
	    node = snReadNode(obj->Prev);
	    }

	/** If no node, and user said CREAT ok, try that. **/
	if (!node && (obj->Mode & O_CREAT) && (obj->SubPtr == obj->Pathname->nElements))
	    {
	    node = snNewNode(obj->Prev, usrtype);
	    }

	/** If _still_ no node, quit out. **/
	if (!node)
	    {
	    mssError(0,"QYT","Could not open querytree node object");
	    return NULL;
	    }

	/** Allocate the structure **/
	inf = qyt_internal_ProcessPath(obj->Session, obj->Pathname, node, obj->SubPtr, node->Data, obj->Mode & O_CREAT, 0);
	obj_internal_PathPart(obj->Pathname,0,0);
	if (!inf) return NULL;
	inf->Obj = obj;
	inf->Mask = mask;
	obj->SubCnt = obj->Pathname->nElements - obj->SubPtr + 1;

	/** Set object params. **/
	inf->BaseNode = node;
	inf->BaseNode->OpenCnt++;
	inf->Offset = 0;

    return (void*)inf;
    }


/*** qytClose - close an open file or directory.
 ***/
int
qytClose(void* inf_v, pObjTrxTree* oxt)
    {
    pQytData inf = QYT(inf_v);

    	/** Write the node first **/
	snWriteNode(inf->Obj->Prev, inf->BaseNode);

	/** Close the lowlevel-object **/
	if (inf->LLObj) objClose(inf->LLObj);
	
	/** Release the memory **/
	inf->BaseNode->OpenCnt --;
	nmFree(inf,sizeof(QytData));

    return 0;
    }


/*** qytCreate - create a new file without actually opening that 
 *** file.
 ***/
int
qytCreate(pObject obj, int mask, pContentType systype, char* usrtype, pObjTrxTree* oxt)
    {
    void* inf;

    	/** Call open() then close() **/
	obj->Mode = O_CREAT | O_EXCL;
	inf = qytOpen(obj, mask, systype, usrtype, oxt);
	if (!inf) return -1;
	qytClose(inf, oxt);

    return 0;
    }


/*** qytDelete - delete an existing file or directory.
 ***/
int
qytDelete(pObject obj, pObjTrxTree* oxt)
    {
    pQytData inf;
    char* node_path;
    pSnNode node;

	/** Determine node path **/
	node_path = obj_internal_PathPart(obj->Pathname, 0, obj->SubPtr);
	node = snReadNode(obj->Prev);
	if (!node) 
	    {
	    mssError(0,"QYT","Could not access node object for querytree");
	    return -1;
	    }

	/** Process path to determine actual path of object. **/
	inf = qyt_internal_ProcessPath(obj->Session, obj->Pathname, node, obj->SubPtr, node->Data, 0, 0);
	if (inf->LLObj) objClose(inf->LLObj);
	obj_internal_PathPart(obj->Pathname, 0,0);

	/** Call delete on it, using the actual path determined by process_path. **/
	if (inf->Pathname[0]) 
	    {
	    if (objDelete(obj->Session, inf->Pathname) < 0)
	        {
		nmFree(inf, sizeof(QytData));
		mssError(0,"QYT","Could not delete object referenced by querytree");
		return -1;
		}
	    }
	else
	    {
	    nmFree(inf,sizeof(QytData));
	    mssError(0,"QYT","Could not determine referenced querytree object");
	    return -1;
	    }

	/** Release, don't call close because that might write data to a deleted object **/
	nmFree(inf,sizeof(QytData));

    return 0;
    }


/*** qyt_internal_ReadText() - attempt to read data from a 'text' type object
 *** that might have a 'content' property giving the object's content.
 ***/
int
qyt_internal_ReadText(pQytData inf, char* buffer, int maxcnt, int offset, int flags, pObjTrxTree* oxt)
    {
    pStructInf content_inf;
    int len;
    char* ptr;

	/** do we have content? **/
	content_inf = stLookup(inf->NodeData, "content");
	if (!content_inf)
	    {
	    mssError(1,"QYT","Text querytree object has no content");
	    return -1;
	    }

	/** ok, get our bearings... **/
	if (stGetAttrValue(content_inf, DATA_T_STRING, POD(&ptr), 0) != 0)
	    {
	    mssError(1,"QYT","Could not read content from text querytree object");
	    return -1;
	    }
	len = strlen(ptr);
	if (flags & OBJ_U_SEEK)
	    inf->Offset = offset;
	if (inf->Offset < 0) inf->Offset = 0;
	if (inf->Offset > len) inf->Offset = len;
	if (inf->Offset + maxcnt > len) maxcnt = len - inf->Offset;
	if (maxcnt <= 0) return 0;
	memcpy(buffer, ptr+inf->Offset, maxcnt);
	inf->Offset += maxcnt;

    return maxcnt;
    }


/*** qytRead - Attempt to read from the underlying object.
 ***/
int
qytRead(void* inf_v, char* buffer, int maxcnt, int offset, int flags, pObjTrxTree* oxt)
    {
    pQytData inf = QYT(inf_v);
    int rcnt;

    	/** If no actual pathname (presented path was a text="") ... **/
	if (inf->LLObj == NULL) 
	    {
	    return qyt_internal_ReadText(inf, buffer, maxcnt, offset, flags, oxt);
	    }

	/** Otherwise, attempt the read operation. **/
	rcnt = objRead(inf->LLObj, buffer, maxcnt, offset, flags);

    return rcnt;
    }


/*** qytWrite - As above, attempt to read if we found an actual object.
 ***/
int
qytWrite(void* inf_v, char* buffer, int cnt, int offset, int flags, pObjTrxTree* oxt)
    {
    pQytData inf = QYT(inf_v);
    int wcnt;

    	/** No write if the object didn't map to an actual one. **/
	if (inf->LLObj == NULL) 
	    {
	    mssError(1,"QYT","Cannot write content to a text='' querytree object");
	    return -1;
	    }

	/** Ok, attempt the write operation **/
	wcnt = objWrite(inf->LLObj, buffer, cnt, offset, flags);

    return wcnt;
    }


/*** qyt_internal_GetQueryItem - finds the first(next) inf item in the .qyt
 *** file to include in this query.  Items to include are system/querytree
 *** type subitems that either have a source= or text= property.  Source=
 *** type subitems will result in zero or more items for the query.  Text=
 *** subitems will result in exactly one returned object for the query.
 *** Text= types cannot be modified/etc by the enduser.  The .qyt must be
 *** opened as a structure file (via objdrv_struct) to modify Text=items.
 *** modifications/addidions/deletions of query results from a Source=
 *** item will be passed on to the source driver (such as objdrv_sybase).
 ***/
int
qyt_internal_GetQueryItem(pQytQuery qy)
    {
    pStructInf main_inf;
    pStructInf find_inf;
    char* val;
    char* ptr;

    	/** Already hit end of query? **/
	if (qy->NextSubInfID == -1) return -1;

	/** Need to 'prime' the struct table? **/
	if (qy->StructTable.nItems == 0)
	    {
	    find_inf = qy->ObjInf->NodeData;
	    while(find_inf)
	        {
		if (stStructType(find_inf) == ST_T_SUBGROUP) 
		    xhAdd(&qy->StructTable, find_inf->Name, (void*)find_inf);
		find_inf = find_inf->Parent;
		}
	    }

    	/** Search for a SOURCE= or TEXT= subgroup inf **/
	main_inf = qy->ObjInf->NodeData;
	while(qy->NextSubInfID < main_inf->nSubInf)
	    {
	    find_inf = main_inf->SubInf[qy->NextSubInfID++];
	    if (stStructType(find_inf) == ST_T_ATTRIB && !strcmp(find_inf->Name, "recurse"))
	        {
		stGetAttrValue(find_inf, DATA_T_STRING, POD(&ptr), 0);
		find_inf = (pStructInf)xhLookup(&qy->StructTable, ptr);
		if (!find_inf) continue;
		}
	    if (stStructType(find_inf) == ST_T_SUBGROUP)
	        {
		xhAdd(&qy->StructTable, find_inf->Name, (void*)find_inf);
		val = NULL;
		stAttrValue(stLookup(find_inf,"text"),NULL,&val,0);
		if (val)
		    {
		    qy->ItemText = val;
		    qy->ItemSrc = NULL;
		    qy->ItemWhere = NULL;
		    return qy->NextSubInfID - 1;
		    }
		stAttrValue(stLookup(find_inf,"source"),NULL,&val,0);
		if (val)
		    {
		    qy->ItemText = NULL;
		    qy->ItemSrc = val;
		    qy->ItemWhere = NULL;
		    stAttrValue(stLookup(find_inf,"where"),NULL,&(qy->ItemWhere),0);
		    return qy->NextSubInfID - 1;
		    }
		}
	    }

	/** End of query. **/
	qy->NextSubInfID = -1;

    return -1;
    }


/*** qyt_internal_StartQuery - starts a query on the low-level object on which the
 *** querytree translation is being performed.  Returns a query object from 
 *** objOpenQuery, which can be used for objQueryFetch()ing.
 ***/
pObjQuery
qyt_internal_StartQuery(pQytQuery qy)
    {
    pObjQuery qyinf = NULL;
    char* where_clause;
    char* usr_where_clause;
    int len;
    void* subst_ptr[8];
    int alloc_ptr[8];
    int subst_int[8];
    int subst_types[8];
    int n_subst;
    char* ptr;
    char* sptr;
    char attrname[33];
    char nbuf[16];
    int i;

    	/** Get the where clause restrictions **/
	where_clause = qy->ItemWhere;
	usr_where_clause = qy->Query->QyText;
	if (!usr_where_clause) usr_where_clause = "";
	if (*usr_where_clause || *where_clause) 
	    len = strlen(usr_where_clause) + strlen(where_clause) + 12;
	else
	    len = 0;

	/** Perform any needed substitution on the parameterized where clause **/
	if (*where_clause)
	    {
	    n_subst = 0;
	    ptr = where_clause;
	    while((ptr = strstr(ptr,"::")))
	        {
		ptr += 2;

		/** Get the attribute name after the :: **/
		i = 0;
		while(((*ptr >= 'A' && *ptr <= 'Z') || (*ptr >= 'a' && *ptr <= 'z') || 
		      (*ptr >= '0' && *ptr <= '9') || *ptr == '_') && i < 32)
		    {
		    attrname[i++] = *(ptr++);
		    }
		attrname[i] = 0;

		/** Get the attr type and string -or- integer value **/
		if (qy->ObjInf->LLObj && (subst_types[n_subst] = objGetAttrType(qy->ObjInf->LLObj, attrname)) >= 0)
		    {
		    alloc_ptr[n_subst] = 0;
		    subst_ptr[n_subst] = NULL;
		    switch(subst_types[n_subst])
		        {
			case DATA_T_INTEGER:
			    if (objGetAttrValue(qy->ObjInf->LLObj, attrname, DATA_T_INTEGER,POD(&(subst_int[n_subst]))) == 1)
			        {
			        subst_types[n_subst] = -1;
			        len += 6;
			        }
			    else
			        {
			        len += 12;
			        }
			    break;

			case DATA_T_STRING:
			    if (objGetAttrValue(qy->ObjInf->LLObj, attrname, DATA_T_STRING,POD(&sptr)) == 1)
			        {
				subst_types[n_subst] = -1;
				len += 6;
				}
			    else
			        {
			        subst_ptr[n_subst] = (void*)nmSysStrdup(sptr);
				alloc_ptr[n_subst] = 1;
			        len += strlen(sptr);
				}
			    break;
			}
		    n_subst++;
		    }
		else
		    {
		    mssError(0, "QYT", "Parent attribute in .qyt file where clause does not exist.");
		    return NULL;
		    }
		}
	    }

	/** Allocate memory for the new where clause **/
	if (len) qy->QyText = (char*)nmMalloc(len);
	if (len && !(qy->QyText)) 
	    {
	    return NULL;
	    }

	/** Build the "final" where clause to use **/
	ptr = qy->QyText;
	if (*usr_where_clause)
	    {
	    ptr = memccpy(ptr, "(", '\0', len)-1;
	    ptr = memccpy(ptr, usr_where_clause, '\0', len)-1;
	    ptr = memccpy(ptr, ")", '\0', len)-1;
	    }
	if (*where_clause)
            {
            if (*usr_where_clause)
	        ptr = memccpy(ptr, " AND (", '\0', len)-1;
	    else
	        ptr = memccpy(ptr, "(", '\0', len)-1;
            sptr = where_clause;
            i = 0;
            while(*sptr)
                {
                if (*sptr == ':' && *(sptr+1) == ':')
                    {
		    switch(subst_types[i])
		        {
			case -1: /* NULL */
			    ptr = memccpy(ptr, "NULL", '\0', len)-1;
			    break;

			case DATA_T_INTEGER:
                            sprintf(nbuf,"%d",subst_int[i]);
                            ptr = memccpy(ptr, nbuf, '\0', 16)-1;
			    break;

			case DATA_T_STRING:
                            *(ptr++) = '"';
                            ptr = memccpy(ptr, subst_ptr[i], '\0', len)-1;
                            *(ptr++) = '"';
			    break;
                        }
		    if (alloc_ptr[i] && subst_ptr[i]) 
		        {
			nmSysFree(subst_ptr[i]);
			alloc_ptr[i] = 0;
			}
                    sptr += 2;
                    while(((*sptr >= 'A' && *sptr <= 'Z') || (*sptr >= 'a' && *sptr <= 'z') || 
                           (*sptr >= '0' && *sptr <= '9') || *sptr == '_')) sptr++;
		    i++;
                    }
                else
                    {
                    *(ptr++) = *(sptr++);
                    }
                }
            ptr = memccpy(ptr, ")", '\0', len)-1;
            }
	*ptr = '\0';

	/** Issue the query. **/
	qy->LLQueryObj = objOpen(qy->ObjInf->Obj->Session, qy->ItemSrc, O_RDONLY, 0600, "system/directory");
	if (qy->LLQueryObj) 
	    {
	    objUnmanageObject(qy->LLQueryObj->Session, qy->LLQueryObj);
	    qyinf = objOpenQuery(qy->LLQueryObj, qy->QyText, NULL,NULL,NULL);
	    if (!qyinf)
		{
		objClose(qy->LLQueryObj);
		qy->LLQueryObj = NULL;
		}
	    else
		{
		objUnmanageQuery(qy->LLQueryObj->Session, qyinf);
		}
	    }

	/** Failed to open source object or issue query? **/
	if (!(qyinf)) 
	    {
	    if (len) nmFree(qy->QyText, len);
	    mssError(0,"QYT","Could not open querytree source object/query");
	    return NULL;
	    }

	/** Free the memory our where clause used. **/
	if (len) nmFree(qy->QyText, len);
    
    return qyinf;
    }


/*** qytOpenQuery - open a directory query.
 ***/
void*
qytOpenQuery(void* inf_v, pObjQuery query, pObjTrxTree* oxt)
    {
    pQytData inf = QYT(inf_v);
    pQytQuery qy;

	/** Allocate the query structure **/
	qy = (pQytQuery)nmMalloc(sizeof(QytQuery));
	if (!qy) return NULL;
	memset(qy, 0, sizeof(QytQuery));
	qy->ObjInf = inf;
	qy->NextSubInfID = 0;
	qy->Query = query;
	qy->ObjInf = inf;
	xhInit(&qy->StructTable,17,0);

	/** Get the next subinf ready for retrieval. **/
	if (qyt_internal_GetQueryItem(qy) < 0)
	    {
	    xhClear(&qy->StructTable, NULL, NULL);
	    xhDeInit(&qy->StructTable);
	    nmFree(qy, sizeof(QytQuery));
	    return NULL;
	    }

	/** If a source= query, start it now. **/
	if (qy->ItemSrc != NULL) qy->LLQuery = qyt_internal_StartQuery(qy);

    return (void*)qy;
    }


/*** qytQueryFetch - get the next directory entry as an open object.
 ***/
void*
qytQueryFetch(void* qy_v, pObject obj, int mode, pObjTrxTree* oxt)
    {
    pQytQuery qy = ((pQytQuery)(qy_v));
    pQytData inf;
    pObject llobj = NULL;
    char* objname = NULL;
    int cur_id = -1;

	/** If text item, return it and go to next item, else look at sub query **/
	while(!objname && qy->NextSubInfID != -1)
	    {
	    cur_id = qy->NextSubInfID - 1;
	    if (qy->ItemText != NULL)
	        {
		qy->LLQuery = NULL;
	        objname = qy->ItemText;
	        qyt_internal_GetQueryItem(qy);
	        if (qy->NextSubInfID != -1 && qy->ItemSrc != NULL) qy->LLQuery = qyt_internal_StartQuery(qy);
	        }
	    else
	        {
	        if (qy->LLQuery) 
		    {
		    llobj = objQueryFetch(qy->LLQuery, mode);
		    if (!llobj) 
		        {
			objQueryClose(qy->LLQuery);
			qy->LLQuery = NULL;
			objClose(qy->LLQueryObj);
			qy->LLQueryObj = NULL;
			}
		    }
		if (!llobj)
		    {
	            qyt_internal_GetQueryItem(qy);
	            if (qy->NextSubInfID != -1 && qy->ItemSrc != NULL) qy->LLQuery = qyt_internal_StartQuery(qy);
		    }
		else
		    {
		    objGetAttrValue(llobj, "name", DATA_T_STRING,POD(&objname));
		    objUnmanageObject(llobj->Session, llobj);
		    }
	        }
	    }

    	/** If end of query, return NULL. **/
	if (cur_id == -1 || !objname) return NULL;

	/** Build the filename. **/
	if (strlen(objname) + 1 + strlen(qy->ObjInf->Obj->Pathname->Pathbuf) > 255) 
	    {
	    mssError(1,"QYT","Object pathname in query result exceeds internal representation");
	    return NULL;
	    }
	sprintf(obj->Pathname->Pathbuf,"%s/%s",qy->ObjInf->Obj->Pathname->Pathbuf,objname);
	obj->Pathname->Elements[obj->Pathname->nElements++] = strrchr(obj->Pathname->Pathbuf,'/')+1;

	/** Alloc the structure **/
	inf = qyt_internal_ProcessPath(obj->Session, obj->Pathname, qy->ObjInf->BaseNode, 
		qy->ObjInf->Obj->SubPtr, qy->ObjInf->BaseNode->Data, 0, 1);
	if (!inf) return NULL;
	/*strcpy(inf->Pathname, llobj->Pathname->Pathbuf);*/
	strcpy(inf->Pathname, obj_internal_PathPart(obj->Pathname, 0, 0));
	if (inf->LLObj) objClose(inf->LLObj);
	inf->LLObj = llobj;
	inf->BaseNode = qy->ObjInf->BaseNode;
	inf->NodeData = qy->ObjInf->NodeData->SubInf[cur_id];
	inf->BaseNode->OpenCnt++;
	inf->Obj = obj;
	inf->Offset = 0;
	obj_internal_PathPart(obj->Pathname,0,0);

    return (void*)inf;
    }


/*** qytQueryClose - close the query.
 ***/
int
qytQueryClose(void* qy_v, pObjTrxTree* oxt)
    {
    pQytQuery qy = ((pQytQuery)(qy_v));

    	/** Close any pending low-level query **/
	if (qy->LLQuery) objQueryClose(qy->LLQuery);
	if (qy->LLQueryObj) objClose(qy->LLQueryObj);

	/** Free the structure **/
	xhClear(&qy->StructTable, NULL, NULL);
	xhDeInit(&qy->StructTable);
	nmFree(qy_v,sizeof(QytQuery));

    return 0;
    }


/*** qytGetAttrType - get the type (DATA_T_xxx) of an attribute by name.
 ***/
int
qytGetAttrType(void* inf_v, char* attrname, pObjTrxTree* oxt)
    {
    pQytData inf = QYT(inf_v);

    	/** If name, it's a string **/
	if (!strcmp(attrname,"name")) return DATA_T_STRING;

	/** If 'content-type', it's also a string. **/
	if (!strcmp(attrname,"content_type") || !strcmp(attrname, "inner_type") ||
	    !strcmp(attrname,"outer_type")) return DATA_T_STRING;

	/** If there is a low-level object, lookup within it **/
	if (inf->LLObj) return objGetAttrType(inf->LLObj, attrname);

	mssError(1,"QYT","Invalid attribute '%s' for querytree object", attrname);

    return -1;
    }


/*** qytGetAttrValue - get the value of an attribute by name.  The 'val'
 *** pointer must point to an appropriate data type.
 ***/
int
qytGetAttrValue(void* inf_v, char* attrname, int datatype, pObjData val, pObjTrxTree* oxt)
    {
    pQytData inf = QYT(inf_v);
    pStructInf content_inf;

	/** Choose the attr name **/
	if (!strcmp(attrname,"name"))
	    {
	    if (datatype != DATA_T_STRING)
		{
		mssError(1,"QYT","Type mismatch accessing attribute '%s' (should be string)", attrname);
		return -1;
		}
	    val->String = obj_internal_PathPart(inf->Obj->Pathname, inf->Obj->Pathname->nElements - 1, 0);
	    obj_internal_PathPart(inf->Obj->Pathname,0,0);
	    return 0;
	    }

	/** annotation? **/
	if (!strcmp(attrname,"annotation"))
	    {
	    if (datatype != DATA_T_STRING)
		{
		mssError(1,"QYT","Type mismatch accessing attribute '%s' (should be string)", attrname);
		return -1;
		}

	    /** If object associated, get annot from it **/
	    if (inf->LLObj) return objGetAttrValue(inf->LLObj, attrname, datatype, val);

	    /** Otherwise, get annot from node if it has it **/
	    if (stGetAttrValue(stLookup(inf->NodeData,attrname), DATA_T_STRING, val, 0) == 0)
		{
		return 0;
		}
	    else
		{
		val->String = "";
		return 0;
		}
	    }

	/** If content-type, return as appropriate **/
	if (!strcmp(attrname,"outer_type") && !(inf->LLObj))
	    {
	    if (datatype != DATA_T_STRING)
		{
		mssError(1,"QYT","Type mismatch accessing attribute '%s' (should be string)", attrname);
		return -1;
		}
	    val->String = inf->NodeData->UsrType;
	    return 0;
	    }
	else if ((!strcmp(attrname,"content_type") || !strcmp(attrname,"inner_type")) && !(inf->LLObj))
	    {
	    if (datatype != DATA_T_STRING)
		{
		mssError(1,"QYT","Type mismatch accessing attribute '%s' (should be string)", attrname);
		return -1;
		}
	    if (stLookup(inf->NodeData, "content"))
		{
		if ((content_inf = stLookup(inf->NodeData, "content_type")) != NULL)
		    stGetAttrValue(content_inf, DATA_T_STRING, val, 0);
		else
		    val->String = "text/plain";
		}
	    else
		{
		val->String = "system/void";
		}
	    return 0;
	    }

	/** Low-level object?  Lookup the attribute in it **/
	if (inf->LLObj) return objGetAttrValue(inf->LLObj, attrname, datatype, val);

	/** last_modification?  Lookup from node **/
	if (!strcmp(attrname,"last_modification"))
	    {
	    val->DateTime = snGetLastModification(inf->BaseNode);
	    return 0;
	    }

	mssError(1,"QYT","Invalid attribute '%s' for querytree object", attrname);

    return -1;
    }


/*** qytGetNextAttr - get the next attribute name for this object.
 ***/
char*
qytGetNextAttr(void* inf_v, pObjTrxTree oxt)
    {
    pQytData inf = QYT(inf_v);

    	/** If low-level object, pass this through to it. **/
	if (inf->LLObj) return objGetNextAttr(inf->LLObj);

    return NULL;
    }


/*** qytGetFirstAttr - get the first attribute name for this object.
 ***/
char*
qytGetFirstAttr(void* inf_v, pObjTrxTree oxt)
    {
    pQytData inf = QYT(inf_v);

    	/** If low-level object, pass this through to it. **/
	if (inf->LLObj) return objGetFirstAttr(inf->LLObj);

    return NULL;
    }


/*** qytSetAttrValue - sets the value of an attribute.  'val' must
 *** point to an appropriate data type.
 ***/
int
qytSetAttrValue(void* inf_v, char* attrname, int datatype, pObjData val, pObjTrxTree oxt)
    {
    pQytData inf = QYT(inf_v);

	/** Choose the attr name **/
	if (!strcmp(attrname,"name"))
	    {
	    if (datatype != DATA_T_STRING)
		{
		mssError(1,"QYT","Type mismatch setting attribute '%s' (should be string)", attrname);
		return -1;
		}
	    if (inf->NodeData == inf->BaseNode->Data)
	        {
	        if (!strcmp(inf->Obj->Pathname->Pathbuf,".")) return -1;
	        if (strlen(inf->Obj->Pathname->Pathbuf) - 
	            strlen(strrchr(inf->Obj->Pathname->Pathbuf,'/')) + 
		    strlen(val->String) + 1 > 255)
		    {
		    mssError(1,"QYT","SetAttr 'name': name too long for internal representation");
		    return -1;
		    }
	        strcpy(inf->Pathname, inf->Obj->Pathname->Pathbuf);
	        strcpy(strrchr(inf->Pathname,'/')+1,val->String);
	        if (rename(inf->Obj->Pathname->Pathbuf, inf->Pathname) < 0) return -1;
	        strcpy(inf->Obj->Pathname->Pathbuf, inf->Pathname);
		}
	    strcpy(inf->NodeData->Name,val->String);
	    return 0;
	    }

	/** Otherwise, attempt to pass setattr through to lowlevel obj **/
	if (inf->LLObj) return objSetAttrValue(inf->LLObj, attrname, datatype, val);

	mssError(1,"QYT","Invalid attribute for querytree object");

    return -1;
    }


/*** qytAddAttr - add an attribute to an object.  Passthrough to lowlevel.
 ***/
int
qytAddAttr(void* inf_v, char* attrname, int type, pObjData val, pObjTrxTree oxt)
    {
    pQytData inf = QYT(inf_v);

	/** Attempt to pass this through... **/
	if (inf->LLObj) return objAddAttr(inf->LLObj, attrname, type, val);

    return -1;
    }


/*** qytOpenAttr - open an attribute as an object.  Passthrough.
 ***/
void*
qytOpenAttr(void* inf_v, char* attrname, int mode, pObjTrxTree oxt)
    {
    pQytData inf = QYT(inf_v);

	/** Attempt to pass this through... **/
	if (inf->LLObj) return objOpenAttr(inf->LLObj, attrname, mode);

    return NULL;
    }


/*** qytGetFirstMethod -- passthrough.
 ***/
char*
qytGetFirstMethod(void* inf_v, pObjTrxTree oxt)
    {
    pQytData inf = QYT(inf_v);

	/** Attempt to pass this through... **/
	if (inf->LLObj) return objGetFirstMethod(inf->LLObj);

    return NULL;
    }


/*** qytGetNextMethod -- passthrough.
 ***/
char*
qytGetNextMethod(void* inf_v, pObjTrxTree oxt)
    {
    pQytData inf = QYT(inf_v);

	/** Attempt to pass this through... **/
	if (inf->LLObj) return objGetNextMethod(inf->LLObj);

    return NULL;
    }


/*** qytExecuteMethod - passthrough.
 ***/
int
qytExecuteMethod(void* inf_v, char* methodname, pObjData param, pObjTrxTree oxt)
    {
    pQytData inf = QYT(inf_v);

	/** Attempt to pass this through... **/
	if (inf->LLObj) return objExecuteMethod(inf->LLObj, methodname, param);

    return -1;
    }

/*** qytInfo - Return the capabilities of the object.
 ***/
int
qytInfo(void* inf_v, pObjectInfo info)
    {
    pQytData inf = QYT(inf_v);
    pObjectInfo ll_info;
    pStructInf find_inf;
    int n_groups = 0;
    int is_recurse = 0;
    int is_leaf = 0;
    int i;
    pExpression exp;
    pParamObjects objlist;
    char* expstr;

    if (inf->LLObj) 
	{
	/** Get basic character of object from underlying object **/
	ll_info = objInfo(inf->LLObj);
	if (ll_info)
	    memcpy(info, ll_info, sizeof(ObjectInfo));

	/** Now add what we know, structurally, from the querytree **/
	for(i=0;i<inf->NodeData->nSubInf;i++)
	    {
	    find_inf = inf->NodeData->SubInf[i];
	    if (stStructType(find_inf) == ST_T_SUBGROUP)
		n_groups++;
	    else if (!strcmp(find_inf->Name,"recurse"))
		is_recurse = 1;
	    else if (!strcmp(find_inf->Name,"known_leaf"))
		{
		objlist = expCreateParamList();
		if (objlist)
		    {
		    expstr = NULL;
		    stAttrValue(find_inf, NULL, &expstr, 0);
		    if (expstr)
			{
			exp = (pExpression)expCompileExpression(expstr, objlist, MLX_F_ICASE | MLX_F_FILENAMES, 0);
			if (exp)
			    {
			    expAddParamToList(objlist,"",inf->LLObj,EXPR_O_CURRENT);
			    if (expEvalTree(exp, objlist) >= 0 && exp->Integer != 0 && !(exp->Flags & EXPR_F_NULL))
				is_leaf = 1;
			    expFreeExpression(exp);
			    }
			}
		    expFreeParamList(objlist);
		    }
		}
	    }
	if ((is_recurse || n_groups > 0) && !is_leaf)
	    {
	    info->Flags &= ~( OBJ_INFO_F_NO_SUBOBJ | OBJ_INFO_F_CANT_HAVE_SUBOBJ );
	    info->Flags |= ( OBJ_INFO_F_CAN_HAVE_SUBOBJ );
	    }

	return 0;
	}
    return -1;
    }

/*** qytInitialize - initialize this driver, which also causes it to 
 *** register itself with the objectsystem.
 ***/
int
qytInitialize()
    {
    pObjDriver drv;

	/** Allocate the driver **/
	drv = (pObjDriver)nmMalloc(sizeof(ObjDriver));
	if (!drv) return -1;
	memset(drv, 0, sizeof(ObjDriver));

	/** Initialize globals **/
	memset(&QYT_INF,0,sizeof(QYT_INF));
	xhInit(&QYT_INF.ObjAttrCache,255,0);
	xaInit(&QYT_INF.ObjAttrCacheList,QYT_MAX_CACHE);

	/** Setup the structure **/
	strcpy(drv->Name,"QYT - QueryTree Translation Driver");
	drv->Capabilities = OBJDRV_C_FULLQUERY;
	xaInit(&(drv->RootContentTypes),16);
	xaAddItem(&(drv->RootContentTypes),"system/querytree");

	/** Setup the function references. **/
	drv->Open = qytOpen;
	drv->Close = qytClose;
	drv->Create = qytCreate;
	drv->Delete = qytDelete;
	drv->OpenQuery = qytOpenQuery;
	drv->QueryDelete = NULL;
	drv->QueryFetch = qytQueryFetch;
	drv->QueryClose = qytQueryClose;
	drv->Read = qytRead;
	drv->Write = qytWrite;
	drv->GetAttrType = qytGetAttrType;
	drv->GetAttrValue = qytGetAttrValue;
	drv->GetFirstAttr = qytGetFirstAttr;
	drv->GetNextAttr = qytGetNextAttr;
	drv->SetAttrValue = qytSetAttrValue;
	drv->AddAttr = qytAddAttr;
	drv->OpenAttr = qytOpenAttr;
	drv->GetFirstMethod = qytGetFirstMethod;
	drv->GetNextMethod = qytGetNextMethod;
	drv->ExecuteMethod = qytExecuteMethod;
	drv->Info = qytInfo;

	/** Register some structures **/
	nmRegister(sizeof(QytData),"QytData");
	nmRegister(sizeof(QytQuery),"QytQuery");

	/** Register the driver **/
	if (objRegisterDriver(drv) < 0) return -1;

    return 0;
    }

