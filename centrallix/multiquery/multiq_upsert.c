#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include "obj.h"
#include "cxlib/mtlexer.h"
#include "expression.h"
#include "cxlib/xstring.h"
#include "multiquery.h"
#include "cxlib/mtsession.h"
#include "cxlib/util.h"


/************************************************************************/
/* Centrallix Application Server System 				*/
/* Centrallix Core       						*/
/* 									*/
/* Copyright (C) 1999-2008 LightSys Technology Services, Inc.		*/
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
/* Module: 	multiq_upsert.c  	 				*/
/* Author:	Greg Beeley (GRB)					*/
/* Creation:	October 16, 2013 					*/
/* Description:	Provides support for upsert operations.			*/
/************************************************************************/



struct
    {
    pQueryDriver	ThisDriver;
    }
    MQUSINF;

#define MQUS_MAX_CRITERIA	32

/*** Data used by this driver ***/
typedef struct _MQUS
    {
    pExpression	AllCriteria;
    struct _Criteria
	{
	pExpression	Compare;
	pExpression	Exp;
	pExpression	Value;
	pQueryStructure	QS;
	}
	Criteria[MQUS_MAX_CRITERIA];
    int		nCriteria;
    char	InsertPath[OBJSYS_MAX_PATH+1];
    pObject	InsertPathObj;
    XArray	ToBeUpdated; /* of pParamObjects */
    }
    MqusData, *pMqusData;

/*** --ALGORITHM--
    A.	For each ON DUPLICATE expression,
	1.  Compile the expression into an expression tree.
	2.  Use the expression tree to create a {expression} = {value} tree
	3.  Save a reference to the expression tree and to the {value} node
    B.	Iterate over the Select Statement, retrieving matching objects.
	1.  Check Select statement HAVING clause against matching object.
	2.  For each ON DUPLICATE expression,
	    a.	Set the {value} node equal to the attribute from the SELECT.
	3.  Run a query on the insert path searching for objects that match
	    the list of criteria created in step (2).
	4.  If a matching object is not obtained:
	    a.	Insert the row.
	5.  For each matching object that IS obtained:
	    a.	Save the matching object and the select object for step (C).
    C.	For each object to be updated (from B(5)(a)),
	1.  For each item in the UPDATE SET expression list:
	    a.  Set the attribute in the matching row equal to the
		expression given.
***/


/*** mqusAnalyze - take a given query syntax structure, locate the ON
 *** DUPLICATE clause, and if there is one, we're in business.
 ***/
int
mqusAnalyze(pQueryStatement stmt)
    {
    pQueryStructure insert_qs, select_qs, ondup_qs;
    pQueryStructure item;
    pQueryElement qe;
    int i;
    pMqusData pdata;
    pExpression exp;

    	/** Search for an Upsert-type statement... **/
	ondup_qs = mq_internal_FindItem(stmt->QTree, MQ_T_ONDUPCLAUSE, NULL);
	select_qs = mq_internal_FindItem(stmt->QTree, MQ_T_SELECTCLAUSE, NULL);
	insert_qs = mq_internal_FindItem(stmt->QTree, MQ_T_INSERTCLAUSE, NULL);

	/** We get to sit on the bench this time? **/
	if (!select_qs || !insert_qs || !ondup_qs)
	    return 0;

	/** Bad news if other drivers haven't found a SELECT tree **/
	if (!stmt->Tree)
	    return -1;

	/** Alloc a queryexec node **/
	qe = mq_internal_AllocQE();
	qe->Driver = MQUSINF.ThisDriver;

	/** link in with on dup clause **/
	ondup_qs->QELinkage = qe;
	qe->QSLinkage = ondup_qs;

	/** Link with select items **/
	for(i=0;i<stmt->Tree->AttrNames.nItems;i++)
	    {
	    xaAddItem(&qe->AttrNames, stmt->Tree->AttrNames.Items[i]);
	    xaAddItem(&qe->AttrExprPtr, stmt->Tree->AttrExprPtr.Items[i]);
	    xaAddItem(&qe->AttrCompiledExpr, stmt->Tree->AttrCompiledExpr.Items[i]);
	    }

	/** Private data **/
	pdata = (pMqusData)nmMalloc(sizeof(MqusData));
	pdata->nCriteria = 0;
	qe->PrivateData = pdata;

	/** Handle the ON DUPLICATE keying items **/
	pdata->AllCriteria = NULL;
	for(i=0;i<ondup_qs->Children.nItems;i++)
	    {
	    item = (pQueryStructure)(ondup_qs->Children.Items[i]);
	    if (item->NodeType == MQ_T_ONDUPITEM)
		{
		if (pdata->nCriteria >= MQUS_MAX_CRITERIA)
		    {
		    mssError(1,"MQUS","Too many ON DUPLICATE criteria");
		    return -1;
		    }

		/** Compare node **/
		exp = expAllocExpression();
		exp->NodeType = EXPR_N_COMPARE;
		exp->CompareType = MLX_CMP_EQUALS;
		expAddNode(exp, item->Expr);
		pdata->Criteria[pdata->nCriteria].Compare = exp;
		pdata->Criteria[pdata->nCriteria].Exp = item->Expr;

		/** Main criteria tree **/
		if (pdata->AllCriteria)
		    {
		    exp = expAllocExpression();
		    exp->NodeType = EXPR_N_AND;
		    expAddNode(exp, pdata->AllCriteria);
		    expAddNode(exp, pdata->Criteria[pdata->nCriteria].Compare);
		    }
		pdata->AllCriteria = exp;

		/** Value node **/
		exp = expAllocExpression();
		expAddNode(pdata->Criteria[pdata->nCriteria].Compare, exp);
		pdata->Criteria[pdata->nCriteria].Value = exp;

		pdata->nCriteria++;
		}
	    }

	/** What path are we inserting into (and thus potentially updating)? **/
	strtcpy(pdata->InsertPath, insert_qs->Source, sizeof(pdata->InsertPath));

	/** Link the qe into the multiquery **/
	xaAddItem(&stmt->Trees, qe);
	xaAddItem(&qe->Children, stmt->Tree);
	stmt->Tree->Parent = qe;
	stmt->Tree = qe;

    return 0;
    }


/*** mqus_internal_CheckConstraint - validate the constraint, if any, on the
 *** current qe and mq.  Return -1 on error, 0 if fail, 1 if pass.
 ***/
int
mqus_internal_CheckConstraint(pQueryElement qe, pQueryStatement stmt)
    {

	/** Validate the constraint expression, otherwise succeed by default **/
        if (qe->Constraint)
            {
            expEvalTree(qe->Constraint, stmt->Query->ObjList);
	    if (qe->Constraint->DataType != DATA_T_INTEGER)
	        {
	        mssError(1,"MQUS","WHERE clause item must have a boolean/integer value.");
	        return -1;
	        }
	    if (!(qe->Constraint->Flags & EXPR_F_NULL) && qe->Constraint->Integer != 0) return 1;
	    }
        else
	    {
            return 1;
	    }

    return 0;
    }


/*** mqusStart - for an update query, much as for a insert query, we will
 *** run the entire operation within mqusStart() and return no rows for
 *** mqusNextItem.
 ***/
int
mqusStart(pQueryElement qe, pQueryStatement stmt, pExpression additional_expr)
    {
    pMqusData pdata;
    pQueryElement cld;
    int rval = -1;
    int is_started = 0;

	pdata = (pMqusData)qe->PrivateData;
	xaInit(&pdata->ToBeUpdated, 16);

	/** Now, 'trickle down' the Start operation to the SELECT clause **/
	cld = (pQueryElement)(qe->Children.Items[0]);
	if (cld->Driver->Start(cld, stmt, NULL) < 0) 
	    {
	    mssError(0,"MQUS","Failed to start child SELECT operation");
	    goto error;
	    }
	is_started = 1;

	/** Set iteration cnt to 0 **/
	qe->IterCnt = 0;

	/** Open the insert path object (used for dup checking) **/
	pdata->InsertPathObj = objOpen(stmt->Query->SessionID, pdata->InsertPath, O_RDWR, 0600, "system/directory");
	if (!pdata->InsertPathObj)
	    goto error;

	rval = 0;

    error:
	/** Close the SELECT **/
	if (is_started)
	    if (cld->Driver->Finish(cld, stmt) < 0)
		return -1;

    return rval;
    }


/*** mqusNextItem - filter out the items to be updated rather than inserted.
 *** Returns 1 if valid row obtained, 0 if no more rows are available, and
 *** -1 on error.
 ***/
int
mqusNextItem(pQueryElement qe, pQueryStatement stmt)
    {
    int cld_rval;
    pObjQuery find_dups_qy;
    pObject one_dup;
    int i;
    pMqusData pdata;
    pQueryElement cld;
    int rval, dup_cnt;
    pParamObjects objlist;
    pPseudoObject p;

	cld = (pQueryElement)(qe->Children.Items[0]);
	pdata = (pMqusData)qe->PrivateData;

	/** Find an insertable result **/
	while(1)
	    {
	    /** Find one SELECT result **/
	    cld_rval = cld->Driver->NextItem(cld, stmt);

	    /** An error occurred? **/
	    if (cld_rval < 0)
		goto error;

	    /** No more rows? **/
	    if (cld_rval == 0)
		break;

	    /** check HAVING clause **/
	    rval = mq_internal_EvalHavingClause(stmt, NULL);
	    if (rval < 0)
		goto error;
	    else if (rval == 0)
		continue;

	    /** Build the search criteria **/
	    for(i=0;i<pdata->nCriteria;i++)
		{
		/** Evaluate the expression on the SELECTed row **/
		p = mq_internal_CreatePseudoObject(stmt->Query, NULL);
		expModifyParam(stmt->OneObjList, "this", (void*)p);
		rval = expEvalTree(pdata->Criteria[i].Exp, stmt->OneObjList);
		mq_internal_FreePseudoObject(p);
		if (rval < 0)
		    goto error;

		/** Copy the value **/
		expCopyValue(pdata->Criteria[i].Exp, pdata->Criteria[i].Value, 1);
		if (pdata->Criteria[i].Exp->Flags & EXPR_F_NULL || pdata->Criteria[i].Exp->DataType == 0)
		    pdata->Criteria[i].Value->NodeType = EXPR_N_INTEGER;
		else
		    pdata->Criteria[i].Value->NodeType = expDataTypeToNodeType(pdata->Criteria[i].Exp->DataType);
		}

	    /** Search for existing duplicate rows in the insert source **/
	    find_dups_qy = objOpenQuery(pdata->InsertPathObj, NULL, NULL, pdata->AllCriteria, NULL);
	    if (!find_dups_qy)
		goto error;

	    /** Retrieve matching duplicate rows **/
	    dup_cnt = 0;
	    while ((one_dup = objQueryFetch(find_dups_qy, O_RDWR)) != NULL)
		{
		/** Save it for later **/
		objlist = expCreateParamList();
		if (!objlist)
		    {
		    objQueryClose(find_dups_qy);
		    goto error;
		    }
		expCopyList(stmt->Query->ObjList, objlist, -1);
		expLinkParams(objlist, stmt->Query->nProvidedObjects, -1);
		expAddParamToList(objlist, "this", one_dup, EXPR_O_CURRENT);
		xaAddItem(&pdata->ToBeUpdated, (void*)objlist);
		dup_cnt++;
		}
	    objQueryClose(find_dups_qy);

	    /** Allow the insert to happen if no dups found.  By returning 1, we
	     ** indicate to the higher-level QE modules (insertselect in this
	     ** case) that we've found a valid row.
	     **/
	    if (dup_cnt == 0)
		{
		qe->IterCnt++;
		return 1;
		}
	    }

	return 0;

    error:
	for(i=0;i<pdata->ToBeUpdated.nItems;i++)
	    {
	    objlist = (pParamObjects)xaGetItem(&pdata->ToBeUpdated, i);
	    if (objlist)
		{
		expUnlinkParams(objlist, stmt->Query->nProvidedObjects, -1);
		expFreeParamList(objlist);
		}
	    }
	xaClear(&pdata->ToBeUpdated);
	return -1;
    }


/*** mqusFinish - clean up.  It is here that we actually do the update
 *** operations that we queued up in the NextItem loop.
 ***/
int
mqusFinish(pQueryElement qe, pQueryStatement stmt)
    {
    pMqusData pdata;
    pQueryElement cld;
    int i,j;
    pParamObjects objlist;
    pQueryStructure update_qs;
    pExpression exp, assign_exp;
    int t;
    int rval = -1;

	cld = (pQueryElement)(qe->Children.Items[0]);
	pdata = (pMqusData)qe->PrivateData;

	/** Update any ON DUP rows **/
	expUnlinkParams(stmt->Query->ObjList, stmt->Query->nProvidedObjects, -1);
	for(i=0;i<pdata->ToBeUpdated.nItems;i++)
	    {
	    objlist = (pParamObjects)xaGetItem(&pdata->ToBeUpdated, i);
	    if (objlist)
		{
		expCopyList(objlist, stmt->Query->ObjList, -1);

		/** Apply all of the update criteria **/
		for(j=0;j<((pQueryStructure)qe->QSLinkage)->Children.nItems;j++)
		    {
		    update_qs = ((pQueryStructure)qe->QSLinkage)->Children.Items[j];
		    if (update_qs->NodeType != MQ_T_ONDUPUPDATEITEM)
			continue;
		    exp = update_qs->Expr;
		    assign_exp = update_qs->AssignExpr;

		    /** Get the value to be assigned **/
		    if (expEvalTree(exp, stmt->Query->ObjList) < 0) 
			{
			mssError(0,"MQUS","Could not evaluate UPDATE SET expression's value");
			goto error;
			}
		    t = exp->DataType;
		    if (t <= 0)
			{
			mssError(1,"MQUS","Could not evaluate UPDATE SET expression's value");
			goto error;
			}

		    /** Set the attribute on the object **/
		    if (expCopyValue(exp, assign_exp, 1) < 0)
			{
			mssError(1,"MQUS","Could not handle UPDATE SET expression's value");
			goto error;
			}
		    if (expReverseEvalTree(assign_exp, stmt->Query->ObjList) < 0)
			{
			mssError(0,"MQUS","Could not update the object");
			goto error;
			}
		    }

		/** Release the object list **/
		expUnlinkParams(objlist, stmt->Query->nProvidedObjects, -1);
		expFreeParamList(objlist);
		}
	    }

	for(i=stmt->Query->nProvidedObjects;i<stmt->Query->ObjList->nObjects;i++)
	    expModifyParamByID(stmt->Query->ObjList, i, NULL);

	rval = 0;

    error:
	xaDeInit(&pdata->ToBeUpdated);

	if (cld->Driver->Finish(cld, stmt) < 0)
	    return -1;

	objClose(pdata->InsertPathObj);

    return rval;
    }


/*** mqusRelease - does nothing for an update statement.
 ***/
int
mqusRelease(pQueryElement qe, pQueryStatement stmt)
    {
    return 0;
    }


/*** mqusInitialize - initialize this module and register with the multi-
 *** query system.
 ***/
int
mqusInitialize()
    {
    pQueryDriver drv;

    	/** Allocate the driver descriptor structure **/
	drv = (pQueryDriver)nmMalloc(sizeof(QueryDriver));
	if (!drv) return -1;
	memset(drv,0,sizeof(QueryDriver));

	/** Fill in the structure elements **/
	strcpy(drv->Name, "MQUS - MultiQuery ON DUPLICATE...UPDATE SET Statement Module");
	drv->Precedence = 4800;
	drv->Flags = 0;
	drv->Analyze = mqusAnalyze;
	drv->Start = mqusStart;
	drv->NextItem = mqusNextItem;
	drv->Finish = mqusFinish;
	drv->Release = mqusRelease;

	/** Register with the multiquery system. **/
	if (mqRegisterQueryDriver(drv) < 0) return -1;
	MQUSINF.ThisDriver = drv;

    return 0;
    }