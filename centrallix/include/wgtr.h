#ifndef _WIDGET_TREE_H
#define _WIDGET_TREE_H

/************************************************************************/
/* Centrallix Application Server System 				*/
/* Centrallix Core       						*/
/* 									*/
/* Copyright (C) 1999-2006 LightSys Technology Services, Inc.		*/
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
/* Module: 	widget tree     					*/
/* Author:	Matt McGill (MJM)					*/
/* Creation:	July 15, 2004   					*/
/* Description:	Provides an interface for creating and manipulating  	*/
/*		widget trees, primarily used in the process of rendering*/
/*		a DHTML page from an application.                      	*/
/************************************************************************/

/**CVSDATA***************************************************************

 **END-CVSDATA***********************************************************/

#include "obj.h"
#include "expression.h"
#include "cxlib/xarray.h"
#include "iface.h"

#define WGTR_MAX_TEMPLATE	8	/** Maximum templates concurrently applying to page **/

#define WGTR_F_NONVISUAL    1		/** a widget is visual by default, non-visual if this is set **/
#define WGTR_F_CONTAINER    2		/** set for container widgets **/
#define WGTR_F_FLOATING	    4		/** widgets that 'float' - childwindows and popup menus **/


typedef struct
    {
    int		MinWidth;
    int		MinHeight;
    int		MaxWidth;
    int		MaxHeight;
    int		CharWidth;
    int		CharHeight;
    int		ParagraphHeight;	/* total height of one line of text */
    char	AKey[64];
    }
    WgtrClientInfo, *pWgtrClientInfo;


typedef struct
    {
    int				Type;		/** data type DATA_T_xxx **/
    char			Name[32];
    pObjPresentationHints	Hints;		/** controls to apply to the param, incl defaults **/
    pTObjData			Value;		/** Value set by client **/
    }
    WgtrAppParam, *pWgtrAppParam;


typedef struct _WN
    {
    int		Magic;
    void*	reserved;
    int		Flags;
    int		RenderFlags;			/** Flags reserved for the rendering process **/
    char	Type[64];			/** widget type - editbox, etc. **/
    char	Name[64];			/** widget name **/
    char	DName[64];			/** autogenerated - deployment name **/
    int		r_x, r_y, r_width, r_height;	/** Requested geometry **/
    int		pre_x, pre_y, pre_width, pre_height;  /** pre-layout geom. **/
    int		fl_x, fl_y, fl_width, fl_height;/** Flexibility **/
    double  	fx, fy, fw, fh;			/** internal flexibility calculations **/
    int		min_width, min_height;		/** absolute minimums **/
    int		x, y, width, height;		/** actual geometry **/
    int		top, bottom, left, right;	/** container offsets **/
    XArray	Properties;			/** Array of widget properties **/
    XArray	Children;			/** Array of child widgets **/
    struct _WN* Parent;
    struct _WN*	Root;
    int		CurrProperty;			/** Property to return on next call to wgtNextProperty **/
    int		CurrChild;			/** Child to return on next call to wgtrNextChild **/
    int		Verified;			/** Was the node verified? **/
    XArray	Interfaces;			/** Array of supported interface handles **/
    pObjSession ObjSession;			/** Object session this node's related to **/
    void*	LayoutGrid;			/** Grid for this container **/
    void*	StartHLine;
    void*	EndHLine;
    void*	StartVLine;
    void*	EndVLine;
    void*	DMPrivate;			/** private data for use by deployment method **/
    char*	ThisTemplatePath;
    char*	TemplatePaths[WGTR_MAX_TEMPLATE];
    }
    WgtrNode, *pWgtrNode;

typedef struct
    {
    int			Magic;		    /** Magic number **/
    pWgtrNode		Tree;		    /** Widget tree for this iterator **/
    int			TraversMethod;	    /** WGTR_TM_XXX **/
    pWgtrNode		CurrNode;	    /** Next node to be returned **/
    XArray		BacktrackStack;	    /** Keep track of parents for backtracking **/
    XArray		VistQueue;	    /** For keeping track of nodes to visit **/
    } WgtrIterator, *pWgtrIterator;

typedef struct
    {
    pWgtrNode		Tree;		    /** The tree being verified **/
    XArray		VerifyQueue;	    /** Queue of nodes to be verified **/
    int			NumWidgets;	    /** Number of widgets currently in the queue **/
    int			CurrWidgetIndex;    /** Index of widget being verified right now **/
    pWgtrNode		CurrWidget;	    /** Pointer to widget being verified right now **/
    pWgtrClientInfo	ClientInfo;
    } WgtrVerifySession, *pWgtrVerifySession;


/** Widget Driver information structure **/
typedef struct
    {
    char	    Name[64];		    /* Driver name */
    int		    (*Verify)();	    /* Function for verifying the widget */
    int		    (*New)();		    /* Function for initializing a widget */
    XArray	    Types;		    /* Pseudo-types this widget driver will handle */
    }
    WgtrDriver, *pWgtrDriver;



/** traversal methods for iterators **/
#define WGTR_TM_LEVELORDER	1
#define	WGTR_TM_PREORDER	2
#define WGTR_TM_POSTORDER	3

/** wgtr creation and destruction **/
pWgtrNode wgtrParseObject(pObjSession s, char* path, int mode, int permission_mask, char* type, pStruct app_params, char* templates[]);  /** parse osml object **/
pWgtrNode wgtrParseOpenObject(pObject obj, pStruct app_params, char* templates[]);	/** parses an open OSML object into a widget tree **/
void wgtrFree(pWgtrNode tree);	/** frees memory associated with a widget tree **/
pWgtrNode wgtrNewNode(	char* name, char* type, pObjSession s,
			int rx, int ry, int rwidth, int rheight,
			int flx, int fly, int flwidth, int flheight);   /** create a new widget node **/
int wgtrSetupNode(pWgtrNode node);

/** wgtr iterator functions **/
pWgtrIterator wgtrGetIterator(pWgtrNode tree, int traversal_type);	/** returns an iterator for the tree **/
pWgtrNode wgtrNext(pWgtrIterator itr);	/** return next node in tree **/
void wgtrFreeIterator(pWgtrIterator itr);	/** frees memory associated with a widget tree **/

/** accessors **/
int wgtrGetPropertyType(pWgtrNode widget, char* name);	/** get the type of the given property **/
int wgtrGetPropertyValue(pWgtrNode widget, char* name, int datatype, pObjData val); /** get property value **/
char* wgtrFirstPropertyName(pWgtrNode widget);	/** returns name of first property in property array **/
char* wgtrNextPropertyName(pWgtrNode widget);	/** returns next name in property array **/
char* wgtrGetRootDName(pWgtrNode widget);	/** returns the deployment name of the tree root **/
char* wgtrGetDName(pWgtrNode widget);	/** returns the deployment name of the tree node **/
char* wgtrGetTemplatePath(pWgtrNode widget, int n);	/** returns the path to the template used **/

/** modifiers **/
int wgtrAddProperty(pWgtrNode widget, char* name, int datatype, pObjData val, int isnull); /** add a property to the widget **/
int wgtrDeleteProperty(pWgtrNode widget, char* name);	/** deletes the property from the widget **/
int wgtrSetProperty(pWgtrNode widget, char* name, int datatype, pObjData val);	/** sets the widget property val **/
int wgtrRenameProperty(pWgtrNode widget, char* oldname, char* newname);	/* used to handle deprecated property names */
int wgtrDeleteChild(pWgtrNode widget, char* child_name);    /** deletes child widget from tree, along w/ sub-widgets **/
int wgtrAddChild(pWgtrNode widget, pWgtrNode child);	    /** graft one tree onto another **/
pWgtrNode wgtrNextChild(pWgtrNode tree);	/** return the next child **/
pWgtrNode wgtrFirstChild(pWgtrNode tree);	/** return the first child **/
int wgtrImplementsInterface(pWgtrNode this, char* iface_ref);	    /** state that a wgt implements an interface **/
int wgtrSetDMPrivateData(pWgtrNode tree, void* data);
void* wgtrGetDMPrivateData(pWgtrNode tree);
int wgtrGetContainerHeight(pWgtrNode tree);
int wgtrGetContainerWidth(pWgtrNode tree);
int wgtrMoveChildren(pWgtrNode tree, int x_offset, int y_offset);

/** misc. functions **/
pObjPresentationHints wgtrWgtToHints(pWgtrNode widget);	/** mimick objObjToHints **/
pExpression wgtrGetExpr(pWgtrNode widget, char* attrname);	/** Get an expression from a widget node **/

/** verification functions **/
int wgtrVerify(pWgtrNode tree, pWgtrClientInfo client_info);	/** Verify a widget-tree. s must be pHtSession **/
int wgtrScheduleVerify(pWgtrVerifySession vs, pWgtrNode widget); /** add a widget to the Verify Queue **/
int wgtrCancelVerify(pWgtrVerifySession cs, pWgtrNode widget);	/** remove a widget from the Verify Queue **/
int wgtrReverify(pWgtrVerifySession vs, pWgtrNode widget);	/** add to queue only if not already waiting **/

/** wgtr driver-related functions **/
int wgtrRegisterDriver(char* name, int (*Verify)(), int (*New)());	/** registers a widget driver **/
int wgtrAddType(char* name, char* type_name);	    /** associate a type with a wgtr driver **/
int wgtrAddDeploymentMethod(char* method, int (*Render)());	/** add a deployment method to a driver **/
int wgtrRender(pFile output, pObjSession obj_s, pWgtrNode tree, pStruct params, pWgtrClientInfo c_info, char* method);
int wgtrRenderObject(pFile output, pObjSession s, pObject obj, pStruct app_params, pWgtrClientInfo client_info, char* templates[], char* method);

/** for debugging **/
void wgtrPrint(pWgtrNode tree, int indent);	/** for debug purposes **/

#endif
