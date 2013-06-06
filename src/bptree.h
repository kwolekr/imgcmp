/*-
 * Copyright (c) 2012 Ryan Kwolek <kwolekr2@cs.scranton.edu>. 
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright notice, this list of
 *     conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice, this list
 *     of conditions and the following disclaimer in the documentation and/or other materials
 *     provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef BPTREE_HEADER
#define BPTREE_HEADER

///////////////////////// Compile-time configuration //////////////////////////
#define BT_MEMORY      //Work with heap memory, do not use mmap().  NOT YET IMPLEMENTED

//#define BT_USE_BINS    //Duplicate entries will be placed in an array.
                       // Useful when expecting lots of duplicates.

#define BT_MPSAFE //Adds locks using SysV semaphores for multithread/process safety

#define BT_DEFAULT_BIN_SIZE 4     //Default number of elements in a bin before resizing.
							      // This value must be a power of 2.
#define BT_DEFAULT_BIN_SIZE_EXP 2 //Base-2 logarithm of BT_DEFAULT_BIN_SIZE.

//#define BT_NO_DUPS     //BptInsert() will fail when inserting a duplicate key.
                       // Can't be defined along with BT_USE_BINS.

#define BT_NBRANCHES 4 //Branching factor of the B-tree, 8 is good usually.

typedef float KEYTYPE;		  //Datatype of the key to compare entries
typedef unsigned int VALTYPE; //Datatype of the value that is associated with a key
///////////////////////////////////////////////////////////////////////////////

#include "mmfile.h"

#define BT_OVERFLOW (-1)
#define BT_ERROR    (-1)
#define BT_NOTFOUND 0

#define BT_LEAF    0x80000000
#define BT_BIN     0x40000000
#define BT_DELETED 0x20000000
#define BT_FLAGS   (BT_LEAF | BT_BIN | BT_DELETED)

#ifdef BT_USE_BINS
	#define BT_KVP_ATTRIBS
	#define BTNITEMS(x)               ((x)->attribs & 0x0000001F)
	//#define BTISBIN(x, i)             ((x)->attribs & (1 << ((i) + 5)))
	//#define BTSETBIN(x, i)             (x)->attribs |= 1 << ((i) + 5)
	#define BTBIN_NITEMS(x)           ((x)->attribs & 0x00FFFFFF)
	#define BTBIN_GETMAXITEMBITS(x)  (((x)->attribs >> 24) & 0x1F)
	#define BTBIN_SETMAXITEMBITS(x, i) (x)->attribs = ((x)->attribs & ~0x1F000000) | ((i) << 24)
	#define BTBIN_GETFROMOFFSET(x, o) ((LPBTBIN)((x)->baseaddr + o))

	#define BT_ITEM_KEYISBIN    0x01
	#define BT_ITEM_KEYINDIRECT 0x02
	#define BT_ITEM_GETKEYTYPE(x) (((x)->attribs >> 2) & 0x0F)
	//#define BT_ITEM_SETKEYTYPE ((x)->attribs = (x)->attribs & )
	#define BT_ITEM_GETKEYLEN
	#define BT_ITEM_SETKEYLEN

	#define BT_ITEM_VALISBIN    0x10000
	#define BT_ITEM_VALINDIRECT 0x20000
#else
	#define BTNITEMS(x) ((x)->attribs & ~BT_FLAGS)
#endif

/*
 *	BPT File format:
 *
 *	[UINT32] 'BTDB'
 *  [UINT 15] branching factor
 *  [UINT 1]  is there an attribute field in KVPAIR?
 *  [UINT8] depth of tree
 *  [UINT8] attributes (dirty?)
 *	[UINT32] number of nodes
 *	[UINT32] number of leaves
 *  [UINT32] number of items
 *	[UINT32] amount of currently used filespace
 *	[UINT32] offset of root node
 *	[void] branches, leaves, and duplicate data
 */

//#pragma pack(push, 1)

//
typedef struct _bptheader {
	uint32_t signature;
	unsigned int bfactor    : 15;
	unsigned int itemattrib : 1;
	uint8_t depth;
	uint8_t dirty;
	uint32_t nnodes;
	uint32_t nleaves;
	uint32_t nitems;
	uint32_t usedsize;
	uint32_t rootoff;
} BTHEADER, *LPBTHEADER;

//
typedef struct _kvpair {
#ifdef BT_KVP_ATTRIBS
	unsigned int attribs;
#endif
	KEYTYPE key;
#ifdef BT_USE_BINS
	union {
		VALTYPE val;
		unsigned int binoff;
	};
#else
	VALTYPE val;
#endif
} KVPAIR, *LPKVPAIR;

//
typedef struct _bptleafattribs {
	int nitems      : 5;
	int binbitfield : 24;
	int isdeleted   : 1;
	int isbin       : 1;
	int isleaf      : 1;
} BTLEAFATTRIBS, *LPBTLEAFATTRIBS;

//
typedef struct _bptbinattribs {
	int nitems    : 24;
	int maxitems  : 5;
	int isdeleted : 1;
	int isbin     : 1;
	int isleaf    : 1;
} BTBINATTRIBS, *LPBTBINATTRIBS;

//
typedef struct _bptkvpairattribs {
	int keyisbin    : 1;  //for completeness, not necessarily useful
	int keyindirect : 1;
	int keytype     : 4;  //16 different datatypes? hmm, good enough
	int keylen      : 10; //max size is 1023, 0 length means variable
	int valisbin    : 1;
	int valindirect : 1;
	int valtype     : 4;
	int vallen      : 10;
} KVPATTRIBS, *LPVKPATTRIBS;

//
typedef struct _bptnode {
	uint32_t nitems;
	KEYTYPE keys[BT_NBRANCHES];
	uint32_t choffs[BT_NBRANCHES + 1];
} BTNODE, *LPBTNODE;

//
typedef struct _bptleaf {
	uint32_t attribs;
	KVPAIR items[BT_NBRANCHES + 1];
	uint32_t prevoff;
	uint32_t nextoff;
} BTLEAF, *LPBTLEAF;

//
typedef struct _bptbin {
	uint32_t attribs;
	uint32_t nextbinoff;
	VALTYPE vals[0];
} BTBIN, *LPBTBIN;

//#pragma pack(pop)

//
typedef struct _bptree {
	LPBTNODE root;
	union {
		char *baseaddr;
		LPBTHEADER header;
	};
	unsigned int filesize;
	FMAPINFO fmi;
} BPTREE, *LPBPTREE;


#define BT_FILE_INITIAL_SIZE (sizeof(BTHEADER) + sizeof(BTNODE) + 2 * sizeof(BTLEAF))


LPBPTREE BptOpen(const char *btfile);
/*
 * Routine Description:
 *    This routine loads a B+ tree from the specified file. If specified file
 *    does not exist, it is created and initialized via BptNewDBInit.
 *
 * Arguments:
 *    btfile	filename of B+ tree DB to load. If NULL, the memory mapping
 *              is not file-backed.
 *
 * Return Value:
 *    A pointer to a BPTREE structure associated with the opened B+ tree passed to all
 *    subsequent B+ tree operations (success), or returns NULL (failure).
 *    For extended error information, read errno.
 *
 */

void BptClose(LPBPTREE bpt);
/*
 * Routine Description:
 *    This routine closes a B+ tree.  After this operation, the value of the pointer
 *    passed to BptClose is no longer valid.
 *
 * Arguments:
 *    bpt		pointer to B+ tree structure to be closed
 *
 * Return Value:
 *    (none)
 *
 */

int BptInsert(LPBPTREE bpt, KEYTYPE key, VALTYPE value);
/*
 * Routine Description:
 *    This routine inserts value identified by key into a B+ tree.
 *    If BT_NO_DUPS is defined and there is a collision between keys, this
 *    routine will fail. If BT_USE_BINS is defined and there is a collision,
 *    a 'bin' data structure will be created in the tree.
 *
 * Arguments:
 *    bpt		pointer to B+ tree structure the item is being inserted into
 *    key		key identifying the item being inserted
 *    value		value being associated with key
 *
 * Return Value:
 *     1 (success) or 0 (failure).  For extended error information, read errno.
 */

int BptSearch(LPBPTREE bpt, KEYTYPE key, VALTYPE *val);
/*
 * Routine Description:
 *    This routine searches for value(s) associated to a specific key in a B+ tree.
 *
 * Arguments:
 *    bpt		pointer to B+ tree structure the item is being searched within
 *    key		key to search for
 *    val		(OUT) pointer to VALTYPE to receive the value associated
 *              with key.  On failure, *val is not modified.
 *
 * Return Value:
 *    -1 (failure), 0 (not found), 1 (success)
 */

int BptSearchRange(LPBPTREE bpt, KEYTYPE min, KEYTYPE max, KVPAIR **matches_out);
/*
 * Routine Description:
 *    This routine searches for items with keys in the range [min, max].  On success,
 *    a buffer is allocated with malloc() that is sizeof(KVPAIR) * number of items large.
 *    The buffer can be deallocated with free().
 *
 * Arguments:
 *    bpt			pointer to B+ tree structure the item ranged is being
 *                  searched within
 *    min			minimum valued key to search for
 *    max			maximum valued key to search for
 *    matches_out	(OUT) pointer to a pointer to an array of KVPAIR structures containing
 *                  the keys and values.  On failure, *matches_out is not modifed.
 *
 * Return Value:
 *    -1 (failure), or number of items found (success)
 */

int BptGetMin(LPBPTREE bpt, KVPAIR *min);
/*
 * Routine Description:
 *    This routine retrieves the item with the lowest key value in the tree.
 *
 * Arguments:
 *    bpt		pointer to B+ tree structure the minimal item is being retrieved from
 *    min		(OUT) pointer to a KVPAIR structure to write the key and value of the
 *              minimal item.  On failure or no items, min is not modified.
 *
 * Return Value:
 *    -1 (failure), 0 (no items), or 1 (success)
 */

int BptGetMax(LPBPTREE bpt, KVPAIR *max);
/*
 * Routine Description:
 *    This routine retrieves the item with the highest key value in the tree.
 *
 * Arguments:
 *    bpt		pointer to B+ tree structure the maximal item is being retrieved from
 *    max		(OUT) pointer to a KVPAIR structure to write the key and value of the
 *              maximal item.  On failure or no items, max is not modified.
 *
 * Return Value:
 *    -1 (failure), 0 (no items), or 1 (success)
 */

int BptEnumerate(LPBPTREE bpt, KVPAIR **results_out);
/*
 * Routine Description:
 *    This routine retrieves all key-value pairs in the tree.  On success, a buffer is
 *    allocated with malloc() that is sizeof(KVPAIR) * number of items large.  The buffer
 *    can be deallocated with free().  If BT_USE_BINS is defined and an item is a bin,
 *    the ISBIN flag is set for that item and binoff contains the offset to a BTBIN structure
 *    which can be retreived by calling BptGetBinFromOffset().
 *
 * Arguments:
 *    bpt			pointer to B+ tree structure the items are being retrieved from
 *    results_out	(OUT) pointer to an array of KVPAIR structures containing all items
 *                  in the tree.  On failure, *results_out is not modified.
 *
 * Return Value:
 *    -1 (failure), or number of items retrieved (success)
 */

int BptRemove(LPBPTREE bpt, KEYTYPE key);
/*
 * Routine Description:
 *    This routine removes the item identified by key from the specified B+ tree.
 *    If there is more than one value associated to a key and BT_USE_BINS is enabled,
 *    all values associated to the key will be removed.
 *
 * Arguments:
 *    bpt		pointer to B+ tree structure the item is being removed from
 *    key		key of the item to be removed
 *
 * Return Value:
 *    -1 (failure), 0 (not found), or 1 (success)
 */

int BptDraw(LPBPTREE bpt, const char *img_filename);
/*
 * Routine Description:
 *    This routine draws a tree to a PNG image file using libgd.
 *
 * Arguments:
 *    bpt			B+ tree to draw
 *    img_filename	filename to save the tree image as
 *
 * Return Value:
 *    1 (success) or 0 (failure)
 *
 */

#endif //BPTREE_HEADER

