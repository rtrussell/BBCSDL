// Adapted by Richard Russell, 18-Jun-2016 from QuickFill.cpp
//
// Author : John R. Shaw (shawj2@earthlink.net)
// Date   : Jan. 26 2004
//
// Copyright (C) 2004 John R. Shaw
// All rights reserved.
//
// This code may be used in compiled form in any way you desire. This
// file may be redistributed unmodified by any means PROVIDING it is 
// not sold for profit without the authors written consent, and 
// providing that this notice and the authors name is included. If 
// the source code in this file is used in any commercial application 
// then a simple email would be nice.
//
// Warranties and Disclaimers:
// THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND
// INCLUDING, BUT NOT LIMITED TO, WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
// IN NO EVENT WILL JOHN R. SHAW BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES,
// INCLUDING DAMAGES FOR LOSS OF PROFITS, LOSS OR INACCURACY OF DATA,
// INCURRED BY ANY PERSON FROM SUCH PERSON'S USAGE OF THIS SOFTWARE
// EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
//
// Please email bug reports, bug fixes, enhancements, requests and
// comments to: shawj2@earthlink.net
//
// Feb.  6, 2004 : Added left optimization, eliminates some revisits.
// Feb.  8, 2004 : Added reverse clip optimization, eliminates some revisits.
// Feb. 15, 2004 : Found PushOpposite() special case and corrected it.
// Feb. 19, 2004 : Changed internal scan, search and line drawing routines
//                 to use pixel color values, in order to increase overall
//                 speed while working with palettized bitmaps.
// Mar.  5, 2004 : 1) Moved PushVisitedLine() from QuickFill() to
//                 PushOpposite(), this increases the number of revisits and
//                 reduces the size of the visit-list (8:1).
//                 2) Changed visit-list to use HLINE_NODE, since block
//                 checking is no longer required and the number of
//                 allocations are reduce because the free-list can now be
//                 used by all (of course HLINE_NODE is larger than we need,
//                 since it is not a visit-list specific node type)
//----------------------------------------------------------------------------
//

#include <stdlib.h> 
#include "SDL2_gfxPrimitives.h"

// Doubly-linked-list node:

typedef struct hlineNode
{
	int x1, x2, y, dy ;
	struct hlineNode *pNext, *pPrev ;
} HLINE_NODE ;

// Global variables:

static HLINE_NODE*	pVisitList ;
static HLINE_NODE*	pLineList ;
static HLINE_NODE*	pFreeList ;
static int		LastY ;
static int		bXSortOn ;

//----------------------------------------------------------------------------
// Private methods
//----------------------------------------------------------------------------

/* Frees the list of free nodes */
static void FreeList(void)
{
	HLINE_NODE *pNext ;
	while (pFreeList) 
	{
		pNext = pFreeList->pNext ;
	  	free(pFreeList) ;
	  	pFreeList = pNext ;
	}

	while (pLineList)
	{
		pNext = pLineList->pNext ;
	  	free(pLineList) ;
	  	pLineList = pNext ;
	}
	
	while (pVisitList)
	{
		pNext = pVisitList->pNext ;
	  	free(pVisitList) ;
	  	pVisitList = pNext ;
	}
}

/* Push a node onto the line list */
static void PushLine(int x1, int x2, int y, int dy)
{
	HLINE_NODE *pNew = pFreeList ;
	if (pNew)
		pFreeList = pFreeList->pNext ;
	else
		pNew = (HLINE_NODE*) malloc (sizeof(HLINE_NODE)) ;

	/* Add to start of list */
	pNew->x1 = x1 ;
	pNew->x2 = x2 ;
	pNew->y  = y ;
	pNew->dy = dy ;

	if (bXSortOn)
	{
		/* This is for the single line visiting code.
		 * The code runs about 18% slower but you can
		 * use fill patterns with it.
		 * Since the basic algorithym scans lines from
		 * left to right it is required that the list
		 * be sorted from left to right.
		 */
		HLINE_NODE *pThis,*pPrev=(HLINE_NODE*)0 ;
		for (pThis=pLineList ;pThis ;pThis=pThis->pNext) 
		{
			if (x1 <= pThis->x1) 
				break ;
			pPrev = pThis ;
		}
		if (pPrev)
		{
			pNew->pNext = pPrev->pNext ;
			pNew->pPrev = pPrev ;
			pPrev->pNext = pNew ;
			if (pNew->pNext) 
				pNew->pNext->pPrev = pNew ;
		}
		else
		{
			pNew->pNext = pLineList ;
			pNew->pPrev = (HLINE_NODE*)0 ;
			if (pNew->pNext) 
				pNew->pNext->pPrev = pNew ;
			pLineList = pNew ;
		}
	}
	else
	{
		pNew->pNext = pLineList ;
		pLineList = pNew ;
	}
}

/* Pop a node off the line list */
static void PopLine(int *x1, int *x2, int *y, int *dy)
{
	if (pLineList)
	{
		HLINE_NODE *pThis,*pPrev ;
		/* Search lines on stack for same line as last line.
		 * This smooths out the flooding of the graphics object
		 * and reduces the size of the stack.
		 */
		pPrev = pLineList ;
		for (pThis=pLineList->pNext ; pThis ; pThis=pThis->pNext)
		{
			if (pThis->y == LastY)
				break ;
			pPrev = pThis ;
		}
		/* If pThis found - remove it from list */
		if (pThis)
		{
			pPrev->pNext = pThis->pNext ;
			if (pPrev->pNext) 
				pPrev->pNext->pPrev = pPrev ;
			*x1 = pThis->x1 ;
			*x2 = pThis->x2 ;
			*y  = pThis->y ;
			*dy = pThis->dy ;
		}
		/* Remove from start of list */
		else
		{
			*x1 = pLineList->x1 ;
			*x2 = pLineList->x2 ;
			*y  = pLineList->y ;
			*dy = pLineList->dy ;
			pThis = pLineList ;
			pLineList = pLineList->pNext ;
			if (pLineList) 
				pLineList->pPrev = (HLINE_NODE*)0 ;
		}

		pThis->pNext = pFreeList ;
		pFreeList = pThis ;
		LastY = *y ;
	}
}

/***** Single Visit Code *******************************************/

/* Jan 24, 2004
 * Testing showed a gaping hole in the push opposite code, that
 * would cause QuickFill to get stuck. The cause of this was the
 * fact that push opposite just reduced the number of revisits but
 * did not stop them.
 */

/* Adds line to visited block list */
static void PushVisitedLine(int x1, int x2, int y)
{
	HLINE_NODE *pNew = pFreeList ;
	if (pNew) 
		pFreeList = pFreeList->pNext ;
	else
		pNew = (HLINE_NODE*) malloc (sizeof(HLINE_NODE)) ;
	/* Add to start of list */
	pNew->x1 = x1 ;
	pNew->x2 = x2 ;
	pNew->y  = y ;
	pNew->pNext = pVisitList ;
	pVisitList = pNew ;
}

/* Checks if line has already been visited */
static int IsRevisit(int x1,int x2,int y)
{
	HLINE_NODE* pNext = pVisitList ;
	while (pNext)
	{
		if (pNext->y == y && pNext->x1 <= x1 && x2 <= pNext->x2)
			break ;
		pNext = pNext->pNext ;
	}
	return (pNext != NULL) ;
}

/* Find next line segment on parent line.
 * Note: This function is designed to be
 *		called until NULL is returned.
 */
static HLINE_NODE* FindNextLine(int x1,int x2,int y)
{
	static HLINE_NODE *pFindNext ;
	HLINE_NODE *pThis ;
	if (!pFindNext) 
		pFindNext = pLineList ;
	for (pThis=pFindNext ;pThis ;pThis=pThis->pNext)
	{
		if ((pThis->y+pThis->dy) == y)
		{
			if (x1 < pThis->x1 && pThis->x1 <= x2) 
			{
				pFindNext = pThis->pNext ;
				return pThis ;
			}
		}
	}
	pFindNext = NULL ;
	return NULL ;
}

/* Removes pThis from line list */
static void PopThis(HLINE_NODE *pThis)
{
	if (pLineList)
	{
		/* If pThis is not start of list */
		if (pThis->pPrev)
		{
			HLINE_NODE *pPrev = pThis->pPrev ;
			pPrev->pNext = pThis->pNext ;
			if (pPrev->pNext) 
				pPrev->pNext->pPrev = pPrev ;
		}
		/* Remove pThis from start of list */
		else
		{
			pLineList = pLineList->pNext ;
			if (pLineList) 
				pLineList->pPrev = (HLINE_NODE*)0 ;
		}
		pThis->pNext = pFreeList ;
		pFreeList = pThis ;
	}
}

/* Push unvisited lines onto the stack
 *
 * +-----------------------------------+
 * |                                   |
 * +---+   +---+   +---+   +---+   +---+
 * |   |   |   |   |   |   |   |   |   |
 * |   |(..+---+...+---+...+---+..)|   | <- Pushed Up
 * |   |xxxxxxxxxxxxxSxxxxxxxxxxxxx|   | <- Drawn
 * +---+(-------------------------)+---+ <- Pushed Down
 *
 * +-----------------------------------+
 * |                                   |
 * +---+   +---+   +---+   +---+   +---+
 * |   |(.)|   |(.)|   |(.)|   |(.)|   | <- Pushed Up
 * |   |xxx+...+xxx+...+xxx+...+xxx|   | <- Drawn
 * |   |xxxxxxxxxxxxxxxxxxxxxxxxxxx|   |
 * +---+---------------------------+---+
 *
 * +-----------------------------------+
 * |                                   |
 * +---+(.)+---+(.)+---+(.)+---+(.)+---+ <- Pushed Up
 * |   |xxx|   |xxx|   |xxx|   |xxx|   | <- Drawn
 * |   |xxx+---+xxx+---+xxx+---+xxx|   |
 * |   |xxxxxxxxxxxxxxxxxxxxxxxxxxx|   |
 * +---+---------------------------+---+
 *
 * +-----------------------------------+
 * |    (.)     (.)     (.)     (.)    | <- Pushed Up
 * +---+xxx+---+xxx+---+xxx+---+xxx+---+ <- Drawn
 * |   |xxx|   |xxx|   |xxx|   |xxx|   |
 * |   |xxx+---+xxx+---+xxx+---+xxx|   |
 * |   |xxxxxxxxxxxxxxxxxxxxxxxxxxx|   |
 * +---+---------------------------+---+
 *
 * +(---------------------------------)+ <- Pushed Up
 * |xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx| <- Drawn
 * +(-)+xxx+(-)+xxx+(-)+xxx+(-)+xxx+(-)+ <- Pushed Down (PushOpposite)
 * |   |xxx|   |xxx|   |xxx|   |xxx|   |
 * |   |xxx+---+xxx+---+xxx+---+xxx|   |
 * |   |xxxxxxxxxxxxxxxxxxxxxxxxxxx|   |
 * +---+---------------------------+---+
 */
static void PushOpposite(int OldX1,int OldX2,int x1,int x2,int y,int dy)
{
	/* Find next line on parent line */
	HLINE_NODE *pFind = FindNextLine(x1,x2,y) ;
	if (!pFind)
	{
		/* push cliped left ends */
		if (x1 < --OldX1) 
			PushLine(x1,--OldX1,y,-dy) ;
		if (x2 > ++OldX2) 
			PushLine(++OldX2,x2,y,-dy) ;
	}
	else
	{
		/* push cliped left */
		if (x1 < --OldX1) 
			PushLine(x1,--OldX1,y,-dy) ;
		/* set test value for right cliping */
		OldX1 = x2+1 ;
		do
		{
			/* push valid line only */
			if (++OldX2 < pFind->x1-2) 
				PushLine(++OldX2,pFind->x1-2,y,-dy) ;
			OldX2 = pFind->x2 ;
			/* clip right end if needed */
			if (OldX2 > OldX1)
			{
				pFind->x1 = ++OldX1 ;
			}
			else /* pop previously visited line */
				PopThis(pFind) ;
			pFind = FindNextLine(x1,x2,y) ;
		} while (pFind) ;

		/* Special Cases
		 *
		 * S = Seed point
		 * x = Filled point
		 * o = Unfilled point
		 *
		 * CASE 1:                CASE 2: Indirectly solved by
		 *                                the solution to case 1.
		 * +-----------------+    +-----------------+
		 * |xxxxxxxxxxxxxxxxx|    |xxxxxxxxxxxxxxxxx|
		 * |x+-----+x+-----+x|    |x+-----+x+-----+x|
		 * |x|xxxxx|x|x|x|x|x|    |x|xxxxx|x|x|x|x|x|
		 * |x|x|x|x|x|x|x|x|x|    |x|x|x|x|x|x|x|x|x|
		 * |x|x|x|x|x|x|x|x|x|    |x|x|x|x|x|x|x|x|x|
		 * |x|x|x|x|x|x|x|x|x|    |x|x|x|x|x|x|x|x|x|
		 * |xxxxxxxxxxxxxxx|x|    |xxxxxxxSxxxxxxx|x|
		 * |x|x|x|x|x|o|o|o|x|    |x|x|x|o|x|x|x|x|x|
		 * |x|x|x|x|x|o|o|o|x|    |x|x|x|o|x|x|x|x|x|
		 * |x|x|x|x|x|o|o|o|x|    |x|x|x|o|x|x|x|x|x|
		 * |x|xxxxx|x|o|o|o|x|    |x|xxxxx|x|x|x|x|x|
		 * |x+-----+x+-----+x|    |x+-----+x+-----+x|
		 * |xxxxxxxxxxxxxxxxS|    |xxxxxxxxxxxxxxxxx|
		 * +-----------------+    +-----------------+
		 *
		 */
		if (++OldX2 < x2) 
			PushLine(OldX2,x2,y,-dy) ;
	}
	PushVisitedLine(x1,x2,y) ;
}

// Arguments: Coordinates of horizontal line and new color of line.
static void DrawHorizontalLine(unsigned int *pBitmap, int x1, int x2, int y, int w, unsigned int dwValue)
{
	unsigned int *p = &pBitmap[x1 + w*y] ;
	for ( ; x1 <= x2 ; ++x1) 
		*p++ = dwValue ;
}

//	xmin > result -> failure
static int ScanLeft(unsigned int *pBitmap, int x, int y, int w, int h, int xmin, unsigned int dwValue)
{
	unsigned int *p = &pBitmap[x + w*y] ;
	if ((x < 0) || (x >= w) || (y < 0) || (y >= h))
		return --xmin ;
	for ( ; x >= xmin ; --x)
		if (dwValue != *p--)
			break ;
	return x ;
}

//	xmin > result -> failure
static int SearchLeft(unsigned int *pBitmap, int x, int y, int w, int h, int xmin, unsigned int dwValue)
{
	unsigned int *p = &pBitmap[x + w*y] ;
	if ((x < 0) || (x >= w) || (y < 0) || (y >= h))
		return --xmin ;
	for ( ; x >= xmin ; --x)
		if (dwValue == *p--)
			break ;
	return x ;
}

//	xmax < result -> failure
static int ScanRight(unsigned int *pBitmap, int x, int y, int w, int h, int xmax, unsigned int dwValue)
{
	unsigned int *p = &pBitmap[x + w*y] ;
	if ((x < 0) || (x >= w) || (y < 0) || (y >= h))
		return ++xmax ;
	for ( ; x <= xmax ; ++x)
		if (dwValue != *p++)
			break ;
	return x ;
}

//	xmax < result -> failure
static int SearchRight(unsigned int *pBitmap, int x, int y, int w, int h, int xmax, unsigned int dwValue)
{
	unsigned int *p = &pBitmap[x + w*y] ;
	if ((x < 0) || (x >= w) || (y < 0) || (y >= h))
		return ++xmax ;
	for ( ; x <= xmax ; ++x)
		if (dwValue == *p++)
			break ;
	return x ;
}

//----------------------------------------------------------------------------
// Public methods
//----------------------------------------------------------------------------

/* Arguments:
 *		Pointer to 32-bpp bitmap to fill
 *		coordinates of start point
 *		width and height of bitmap
 *		fill color 32-bits
 *		target color 32-bits
 *		type 0 = flood while target, 1 = flood until target
*/
void flood(unsigned int* pBitmap, int x, int y, int w, int h,
		unsigned int fill_color, unsigned int target_color, int type)
{
	int dy ;
	int ChildLeft, ChildRight ;
	int ParentLeft, ParentRight ;

	/* Initialize global variables */
	LastY = -1 ;
	pVisitList = NULL ;
	pLineList = NULL ;
	pFreeList = NULL ;

	/* Initialize internal info based on fill type */
	if (type)
		bXSortOn = 1 ;
	else
		bXSortOn = 0 ;

#define FindLeft(p,x,y,w,h,xmin,color) \
	(type ? SearchLeft(p,x,y,w,h,xmin,color) : ScanLeft(p,x,y,w,h,xmin,color))
#define FindRight(p,x,y,w,h,xmax,color) \
	(type ? SearchRight(p,x,y,w,h,xmax,color) : ScanRight(p,x,y,w,h,xmax,color))
#define SkipRight(p,x,y,w,h,xmax,color) \
	(type ? ScanRight(p,x,y,w,h,xmax,color) : SearchRight(p,x,y,w,h,xmax,color))

	// initialise line list
	pFreeList = (HLINE_NODE*) malloc (sizeof(HLINE_NODE)) ;
	pFreeList->pNext = NULL ;

	/* Push starting point on stack.
	 * During testing calling FindLeft() & FindRight() here reduced the number
	 * of revisits by 1 and the number of items on the visit list by 2.
	 */
	ChildLeft  = FindLeft(pBitmap,x,y,w,h,0,target_color)+1 ;
	ChildRight = FindRight(pBitmap,x,y,w,h,w - 1,target_color)-1 ;
	PushLine(ChildLeft,ChildRight,y,+1) ; /* Needed in one special case */
	PushLine(ChildLeft,ChildRight,++y,-1) ;

	/* Now start flooding */
	while (pLineList)
	{
		PopLine(&ParentLeft,&ParentRight,&y,&dy) ;
		y += dy ;
		if (y < 0 || h - 1 < y)
			continue ;

		if (bXSortOn && IsRevisit(ParentLeft,ParentRight,y))
			continue ;

		/* Find ChildLeft end  (ChildLeft>ParentLeft on failure)  */
		ChildLeft = FindLeft(pBitmap,ParentLeft,y,w,h,0,target_color)+1 ;
		if (ChildLeft<=ParentLeft)
		{
			/* Find ChildRight end  (this should not fail here)  */
			ChildRight = FindRight(pBitmap,ParentLeft+1,y,w,h,w - 1,target_color)-1 ;

			/* Fill line */
			if (ChildLeft == ChildRight) 
				pBitmap[ChildRight + y*w] = fill_color ;
			else
				DrawHorizontalLine(pBitmap,ChildLeft,ChildRight,y,w,fill_color) ;

			/* Push unvisited lines */
			if (ParentLeft-1<=ChildLeft && ChildRight<=ParentRight+1)
			{
				/* No reverse clipping required
				 *
				 * Example: Going down
				 * +-------------+
				 * |xxxxxxxxxxxxx|
				 * +---+xxxxx+---+
				 * |   |xxxxx|	 |   <- Parent Line (CASE 1)
				 * |   |xxxxx|	 |   <- Child Line
				 * |   |xxxxx|   |
				 * |  ++xxxxx++  |   <- Parent Line (CASE 2)
				 * |  |xxxxxxx|  |   <- Child Line
				 * +--+-------+--+
				 */
				PushLine(ChildLeft,ChildRight,y,dy) ;
			}
			else
			{
				if (bXSortOn)
				{
					PushOpposite(ParentLeft,ParentRight,ChildLeft,ChildRight,y,dy) ;
				}
				else if (ChildLeft == ParentLeft)
				{
					/* Reverse clip left
					 *
					 * Example: Going up
					 * +-----------------+
					 * |xxxxxxxxxxxxxxxxx|   <- Child Line
					 * |xxx+----+   +----+   <- Parent Line
					 * |xxx|    |   |    |
					 * +---+----+---+----+
					 */
					PushLine(ParentRight+2,ChildRight,y,-dy) ;
				}
				else if (ChildRight == ParentRight)
				{
					/* Reverse clip right
					 *
					 * Example: Going up
					 * +-----------------+
					 * |xxxxxxxxxxxxxxxxx|   <- Child Line
					 * +----+   +----+xxx|   <- Parent Line
					 * |    |   |    |xxx|
					 * +----+---+----+---+
					 */
					PushLine(ChildLeft,ParentLeft-2,y,-dy) ;
				}
				else
				{
					/* Normal reverse push
					 *
					 * Example: Going up
					 * +-------------------+
					 * |xxxxxxxxxxxxxxxxxxx| <- Child Line
					 * +   +---+xxx+---+   | <- Parent Line
					 * |   |   |xxx|   |   |
					 * +---+---+---+---+---+
					 */
					PushLine(ChildLeft,ChildRight,y,-dy) ;
				}
				PushLine(ChildLeft,ChildRight,y,dy) ;
			}
			/* Addvance ChildRight end on to border */
			++ChildRight ;
		}
		else ChildRight = ParentLeft ;

		/* Fill betweens */
		while (ChildRight < ParentRight)
		{
			/* Skip to new ChildLeft end  (ChildRight>ParentRight on failure)  */
			ChildRight = SkipRight(pBitmap,ChildRight+1,y,w,h,ParentRight,target_color) ;
			/* If new ChildLeft end found */
			if (ChildRight<=ParentRight)
			{
				ChildLeft = ChildRight ;

				/* Find ChildRight end  (this should not fail here)  */
				ChildRight = FindRight(pBitmap,ChildLeft+1,y,w,h,w - 1,target_color)-1 ;

				/* Fill line */
				if (ChildLeft == ChildRight) 
					pBitmap[ChildRight + y*w] = fill_color ;
				else
					DrawHorizontalLine(pBitmap,ChildLeft,ChildRight,y,w,fill_color) ;

				/* Push unvisited lines */
				if (ChildRight <= ParentRight+1)
				{
					PushLine(ChildLeft,ChildRight,y,dy) ;
				}
				else
				{
					if (bXSortOn)
					{
						PushOpposite(ParentLeft,ParentRight,ChildLeft,ChildRight,y,dy) ;
					}
					else
						PushLine(ChildLeft,ChildRight,y,-dy) ;
					PushLine(ChildLeft,ChildRight,y,dy) ;
				}
				/* Advance ChildRight end onto border */
				++ChildRight ;
			}
		}
	}

	FreeList() ;
}
