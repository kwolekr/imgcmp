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

/* 
 * bptree.c - 
 *    High performance memory or file-backed B+ Tree, with support for range queries
 *    Also contains drawing routines via libgd for tree debugging and visualization
 */

#include "main.h"
#include "img.h"
#include "mmfile.h"
#include "bptree.h"


inline void _BptInitNewDB(void *baseaddr);
inline void _BptShiftLeafLeft(LPBTLEAF leaf);
inline void _BptShiftLeafRight(LPBTLEAF leaf);
inline void _BptShiftNodeLeft(LPBTNODE node);
inline void _BptShiftNodeRight(LPBTNODE node);
inline unsigned int _BptMakeSpaceLeaf(LPBTLEAF leaf, KEYTYPE key);
inline void _BptMakeSpaceNode(LPBTNODE node, int index);

inline int _BptBinarySearch(KEYTYPE *list, int len, KEYTYPE key);

unsigned int _BptAllocateSpace(LPBPTREE bpt, unsigned int size);
inline unsigned int _BptCreateNode(LPBPTREE bpt);
inline unsigned int _BptCreateLeaf(LPBPTREE bpt);

int _BptInsertBin(LPBPTREE bpt, LPBTLEAF leaf, unsigned int index, VALTYPE value);

unsigned int _BptSplitNode(LPBPTREE bpt, LPBTNODE node);
unsigned int _BptSplitLeaf(LPBPTREE bpt, LPBTLEAF leaf);
int _BptRedistributeNodeLeft(LPBPTREE bpt, LPBTNODE parent, int chindex);
int _BptRedistributeNodeRight(LPBPTREE bpt, LPBTNODE parent, int chindex);
int _BptRedistributeLeafLeft(LPBPTREE bpt, LPBTNODE parent, int chindex);
int _BptRedistributeLeafRight(LPBPTREE bpt, LPBTNODE parent, int chindex);

int _BptInsertWorker(LPBPTREE bpt, LPBTNODE btree, KEYTYPE key, VALTYPE value);
inline LPBTLEAF _BptGetContainingLeaf(LPBPTREE bpt, KEYTYPE key);

int _BptRepair(LPBPTREE bpt);


///////////////////////////////////////////////////////////////////////////////


inline void _BptInitNewDB(void *baseaddr) {
	LPBTHEADER header;
	LPBTLEAF rootleaf;
	
	header = baseaddr;
	header->signature = 'BTDB';
	header->bfactor   = BT_NBRANCHES;
#ifdef BT_KVP_ATTRIBS
	header->itemattrib = 1;
#else
	header->itemattrib = 0;
#endif
	header->depth     = 0;
	header->dirty     = 0;
	header->nnodes    = 0;
	header->nleaves   = 1;
	header->usedsize  = sizeof(BTHEADER) + sizeof(BTLEAF);
	header->rootoff   = sizeof(BTHEADER);

	rootleaf = (LPBTLEAF)((char *)baseaddr + sizeof(BTHEADER));
	rootleaf->attribs = BT_LEAF;
	rootleaf->nextoff = 0;
	rootleaf->prevoff = 0;
}


LPBPTREE BptOpen(const char *btfile) {
	LPBPTREE bpt;
	int status;

	bpt = malloc(sizeof(BPTREE));

	status = MMFileOpen(btfile, BT_FILE_INITIAL_SIZE, &bpt->fmi);
	if (!status) {
		fprintf(stderr, "ERROR: BptOpen: failed to open db\n");
		goto fail_malloc;
	} else if (status == -1) {
		_BptInitNewDB(bpt->fmi.addr);
	}

	//Also sets bpt->header, since it's in a union.
	//This might be undefined behavior, but ought to be okay!
	bpt->baseaddr = bpt->fmi.addr; 

	if (bpt->header->signature != 'BTDB') {
		fprintf(stderr, "ERROR: BptOpen: signature does not match\n");
		goto fail;
	}
	if (bpt->header->bfactor != BT_NBRANCHES) {
		fprintf(stderr, "ERROR: BptOpen: mismatched branching factor\n");
		goto fail;
	}
#ifdef BT_KVP_ATTRIBS
	if (!bpt->header->itemattrib) {
		fprintf(stderr, "ERROR: BptOpen: database items missing attributes\n");
		goto fail;
	}
#else
	if (bpt->header->itemattrib) {
		fprintf(stderr, "ERROR: BptOpen: database items have attributes\n");
		goto fail;
	}
#endif

	bpt->filesize = bpt->header->usedsize;
	bpt->root     = (LPBTNODE)(bpt->baseaddr + bpt->header->rootoff);

	if (bpt->header->dirty == 1) {
		if (!_BptRepair(bpt))
			goto fail;
	}

	return bpt;
fail:
	MMFileClose(&bpt->fmi);
fail_malloc:
	free(bpt);
	return NULL;
}


int _BptRepair(LPBPTREE bpt) {
	fprintf(stderr, "WARNING: database is dirty\n");
	//TODO: actually scan for errors and fix them (if possible)

	return 0;
}


void BptClose(LPBPTREE bpt) {
	if (!bpt)
		return;

	MMFileClose(&bpt->fmi);
	free(bpt);
}


inline void _BptShiftLeafLeft(LPBTLEAF leaf) {
	unsigned int i;

	for (i = 0; i != BTNITEMS(leaf); i++)
		leaf->items[i] = leaf->items[i + 1];
}


inline void _BptShiftLeafRight(LPBTLEAF leaf) {
	unsigned int i;

	for (i = BTNITEMS(leaf); i; i--)
		leaf->items[i] = leaf->items[i - 1];
}


inline void _BptShiftNodeLeft(LPBTNODE node) {
	unsigned int i;

	for (i = 0; i != node->nitems; i++) {
		node->keys[i]   = node->keys[i + 1];
		node->choffs[i] = node->choffs[i + 1];
	}
	node->choffs[i] = node->choffs[i + 1];
}


inline void _BptShiftNodeRight(LPBTNODE node) {
	unsigned int i;

	for (i = node->nitems; i; i--) {
		node->keys[i]       = node->keys[i - 1];
		node->choffs[i + 1] = node->choffs[i];
	}
	node->choffs[1] = node->choffs[0];
}


inline unsigned int _BptMakeSpaceLeaf(LPBTLEAF leaf, KEYTYPE key) {
	unsigned int i;
	//TODO: make this move based on index to avoid comparisons!!!!!!!!!!!!
	for (i = BTNITEMS(leaf); i && (key < leaf->items[i - 1].key); i--)
		leaf->items[i] = leaf->items[i - 1];
	return i;
}


inline void _BptMakeSpaceNode(LPBTNODE node, int index) {
	unsigned int i;

	for (i = node->nitems; i > (unsigned int)index; i--) {
		node->keys[i]       = node->keys[i - 1];
		node->choffs[i + 1] = node->choffs[i];
	}

	//when inserting the new key, it's going to be the same index as the child offset.
	//and then the new node gets attached to i+1'th child offset
	//therefore it's not necessary to take care of the one-off case here
}


unsigned int _BptAllocateSpace(LPBPTREE bpt, unsigned int size) {
	unsigned int offset;

	offset = bpt->filesize;

	if (offset + size > bpt->fmi.maplen) {
#ifdef DEBUG
		printf("Resizing db to %d bytes\n", bpt->fmi.maplen);
#endif
		if (!MMFileResize(&bpt->fmi, bpt->fmi.maplen << 1)) {
			fprintf(stderr, "ERROR: _BptAllocateSpace: failed to resize db\n");
			return 0;
		}
	}

	bpt->baseaddr = bpt->fmi.addr;

	bpt->filesize += size;
	bpt->header->usedsize = bpt->filesize;

	return offset;
}


inline unsigned int _BptCreateNode(LPBPTREE bpt) {
	unsigned int offset = _BptAllocateSpace(bpt, sizeof(BTNODE));
	if (offset)
		bpt->header->nnodes++;
	return offset;
}


inline unsigned int _BptCreateLeaf(LPBPTREE bpt) {
	unsigned int offset = _BptAllocateSpace(bpt, sizeof(BTLEAF));
	if (offset)
		bpt->header->nleaves++;
	return offset;
}


//not using this... only comes out slightly ahead of a linear search when len > 24,
//and loses its advantage when optimized
inline int _BptBinarySearch(KEYTYPE *list, int len, KEYTYPE key) {
	int i, d;

	i = d = len >> 1;
	while (i && i != len && (key < list[i - 1] || key > list[i])) {
		d = (d + 1) >> 1;
		if (list[i] > key)
			i -= d;
		else
			i += d;
	}
	return i;
}


/*
Split(C):
          +-----+-+-+-----+                    +-----+-+-+-+-----+
        A | ... |u|y| ... |                  A | ... |u|w|y| ... |
          +-----+-+-+-----+                    +-----+-+-+-+-----+
                | | |                                | | | |
                | | |                                | | | |
                B | D                                B | | D
                  |                                   /   \
                  |           ======>                /     \ 
          +----+-+-+-+----+                 +-----+-+       +-+-----+
        C | .. |v|w|x| .. |               C | ... |v|       |x| ... | C'
          +----+-+-+-+----+                 +-----+-+       +-+-----+
                 | |                                |       |
                 | |                                |       |
                 E F                                E       F
*/

unsigned int _BptSplitNode(LPBPTREE bpt, LPBTNODE node) {
	LPBTNODE newnode;
	unsigned int i, offset;

	offset  = _BptCreateNode(bpt);
	if (!offset)
		return 0;

	newnode = (LPBTNODE)(bpt->baseaddr + offset);
	
	for (i = 0; i != BT_NBRANCHES / 2; i++) { //splits like [012] 3 [4567]
		newnode->keys[i]   = node->keys[i + BT_NBRANCHES / 2];
		newnode->choffs[i] = node->choffs[i + BT_NBRANCHES / 2];
	}
	newnode->choffs[BT_NBRANCHES / 2] = node->choffs[BT_NBRANCHES];

	newnode->nitems = BT_NBRANCHES / 2;
	node->nitems    = BT_NBRANCHES / 2 - 1;

#	ifdef DEBUG
		printf("DEBUG [%d]:  _BptSplitNode()\n", _nitems);
#	endif
	return offset;
}


unsigned int _BptSplitLeaf(LPBPTREE bpt, LPBTLEAF leaf) {
	LPBTLEAF newleaf;
	unsigned int i, offset;

	offset  = _BptCreateLeaf(bpt);
	newleaf = (LPBTLEAF)(bpt->baseaddr + offset);

	for (i = 0; i != BT_NBRANCHES / 2 + 1; i++) //splits like [01234] [5678]
		newleaf->items[i] = leaf->items[i + BT_NBRANCHES / 2];
	//newleaf->items[BT_NBRANCHES / 2] = leaf->items[BT_NBRANCHES];
	
	newleaf->attribs = (BT_NBRANCHES / 2 + 1) | BT_LEAF;
	leaf->attribs    = (BT_NBRANCHES / 2)     | BT_LEAF;

	//insert into linked list
	newleaf->prevoff = ((char *)leaf - bpt->baseaddr);
	newleaf->nextoff = leaf->nextoff;
	leaf->nextoff    = offset;

#	ifdef DEBUG
		printf("DEBUG [%d]:  _BptSplitLeaf()\n", _nitems);
#	endif
	return offset;
}



int _BptRedistributeNodeLeft(LPBPTREE bpt, LPBTNODE parent, int chindex) {
	LPBTNODE child, lchild;
	unsigned int childoffset;

	child = (LPBTNODE)(bpt->baseaddr + parent->choffs[chindex]);
	if (chindex > 0) {
		lchild = (LPBTNODE)(bpt->baseaddr + parent->choffs[chindex - 1]);
		if (lchild->nitems < BT_NBRANCHES - 2) { //TODO: recheck this
			childoffset = child->choffs[0];

			lchild->keys[lchild->nitems]       = parent->keys[chindex - 1];
			lchild->choffs[lchild->nitems + 1] = childoffset;
			parent->keys[chindex]              = child->keys[0];
			_BptShiftNodeLeft(child);
			
			lchild->nitems++;
			child->nitems--;

#			ifdef DEBUG
				printf("DEBUG [%d]:  _BptRedistributeNodeLeft()\n", _nitems);
#			endif
			return 1;
		}
	}

	return 0;
}


int _BptRedistributeNodeRight(LPBPTREE bpt, LPBTNODE parent, int chindex) {
	LPBTNODE child, rchild;
	unsigned int childoffset;

	child = (LPBTNODE)(bpt->baseaddr + parent->choffs[chindex]);
	if ((unsigned int)chindex < parent->nitems) {
		rchild = (LPBTNODE)(bpt->baseaddr + parent->choffs[chindex + 1]);
		if (rchild->nitems < BT_NBRANCHES - 1) { //was originally BT_NBRANCHES - 2
			childoffset = child->choffs[BT_NBRANCHES];

			_BptShiftNodeRight(rchild);
			rchild->keys[0]   = parent->keys[chindex];
			rchild->choffs[0] = childoffset;
			parent->keys[chindex] = child->keys[BT_NBRANCHES - 1];
			
			rchild->nitems++;
			child->nitems--;
#			ifdef DEBUG
				printf("DEBUG [%d]:  _BptRedistributeNodeRight()\n", _nitems);
#			endif
			return 1;
		}
	}

	return 0;
}


int _BptRedistributeLeafLeft(LPBPTREE bpt, LPBTNODE parent, int chindex) {
	LPBTLEAF child, lchild;

	child = (LPBTLEAF)(bpt->baseaddr + parent->choffs[chindex]);
	if (chindex > 0) {
		lchild = (LPBTLEAF)(bpt->baseaddr + parent->choffs[chindex - 1]);
		if (BTNITEMS(lchild) < BT_NBRANCHES) {
			lchild->items[BTNITEMS(lchild)] = child->items[0];

			lchild->attribs++;
			child->attribs--;

			_BptShiftLeafLeft(child);
#			ifdef DEBUG
				printf("DEBUG [%d]:  _BptRedistributeLeafLeft()\n", _nitems);
#			endif
			return 1;
		}
	}

	return 0;
}


int _BptRedistributeLeafRight(LPBPTREE bpt, LPBTNODE parent, int chindex) {
	LPBTLEAF child, rchild;

	child = (LPBTLEAF)(bpt->baseaddr + parent->choffs[chindex]);
	if ((unsigned int)chindex < parent->nitems) {
		rchild = (LPBTLEAF)(bpt->baseaddr + parent->choffs[chindex + 1]);
		if (BTNITEMS(rchild) < BT_NBRANCHES) {
			rchild->attribs++;
			child->attribs--;

			_BptShiftLeafRight(rchild);

			rchild->items[0] = child->items[BTNITEMS(child)];

#			ifdef DEBUG
				printf("DEBUG [%d]:  _BptRedistributeLeafRight()\n", _nitems);
#			endif
			return 1;
		}
	}

	return 0;
}


#ifdef BT_USE_BINS

int _BptInsertBin(LPBPTREE bpt, LPBTLEAF leaf, unsigned int index, VALTYPE value) {
	unsigned int binoff, nitems, mitembits, maxitems;
	LPBTBIN bin, newbin;

	if (leaf->items[index].attribs & BT_ITEM_VALISBIN) {
		bin = (LPBTBIN)(bpt->baseaddr + leaf->items[index].binoff);
		while (bin->nextbinoff)
			bin = (LPBTBIN)(bpt->baseaddr + bin->nextbinoff);

		nitems    = BTBIN_NITEMS(bin);
		mitembits = BTBIN_GETMAXITEMBITS(bin);
		maxitems  = 1 << mitembits;

		if (nitems == maxitems) {
			mitembits++;
			maxitems <<= 1;

			binoff = _BptAllocateSpace(bpt, sizeof(BTBIN) + sizeof(VALTYPE) * maxitems);
			if (!binoff)
				return 0;

			newbin = (LPBTBIN)(bpt->baseaddr + binoff);
			newbin->attribs    = BT_BIN | nitems;
			newbin->nextbinoff = 0;
			BTBIN_SETMAXITEMBITS(newbin, mitembits);

			bin->nextbinoff = binoff;
			bin    = newbin;
			nitems = 0;
		}

		bin->vals[nitems] = value;
		bin->attribs++;
	} else {
		binoff = _BptAllocateSpace(bpt, sizeof(BTBIN) + sizeof(VALTYPE) * BT_DEFAULT_BIN_SIZE);
		if (!binoff)
			return 0;

		bin = (LPBTBIN)(bpt->baseaddr + binoff);

		bin->vals[0] = leaf->items[index].val;
		bin->vals[1] = value;
		bin->attribs = BT_BIN | 2;
		BTBIN_SETMAXITEMBITS(bin, BT_DEFAULT_BIN_SIZE_EXP);

		leaf->items[index].binoff  = binoff;
		leaf->items[index].attribs |= BT_ITEM_VALISBIN;
	}

	return 1;
}

#endif


int _BptInsertWorker(LPBPTREE bpt, LPBTNODE btree, KEYTYPE key, VALTYPE value) {
	LPBTLEAF leaf, child, newchild, rchild;
	LPBTNODE nchild, newnchild;
	unsigned int i, newchoff;
	KEYTYPE newkey;
	int result;

	if (btree->nitems & BT_LEAF) {
		leaf = (LPBTLEAF)btree;

#if defined(BT_NO_DUPS) || defined(BT_USE_BINS)
		for (i = 0; i != BTNITEMS(leaf) && key != leaf->items[i].key; i++);
		if (i != BTNITEMS(leaf)) {
#	ifdef BT_NO_DUPS
			return 0;
#	else
			return _BptInsertBin(bpt, leaf, i, value);
#	endif
		}
#endif

		i = _BptMakeSpaceLeaf(leaf, key);
		leaf->items[i].key = key;
		leaf->items[i].val = value;
#ifdef BT_KVP_ATTRIBS
		leaf->items[i].attribs = 0;
#endif
		leaf->attribs++;

		bpt->header->nitems++;

		if (BTNITEMS(leaf) == BT_NBRANCHES + 1)
			return BT_OVERFLOW;
	} else {
#if BT_NBRANCHES < 32
		for (i = 0; i != btree->nitems && (btree->keys[i] <= key); i++);
#else
		i = _BptBinarySearch(btree->keys, btree->nitems, key);
#endif

		child  = (LPBTLEAF)(bpt->baseaddr + btree->choffs[i]);
		result = _BptInsertWorker(bpt, (LPBTNODE)child, key, value);
		if (!result)
			return 0;
		
		if (result == BT_OVERFLOW) {
			if (child->attribs & BT_LEAF) {
				if (_BptRedistributeLeafLeft(bpt, btree, i)) {
					btree->keys[i - 1] = child->items[0].key;
				} else if (_BptRedistributeLeafRight(bpt, btree, i)) {
					rchild = (LPBTLEAF)(bpt->baseaddr + btree->choffs[i + 1]);
					btree->keys[i] = rchild->items[0].key; //////???? might need to be i+1?
				} else {
					newchoff = _BptSplitLeaf(bpt, child);
					newchild = (LPBTLEAF)(bpt->baseaddr + newchoff);
					newkey   = newchild->items[0].key;

					_BptMakeSpaceNode(btree, i);
					btree->keys[i]       = newkey;
					btree->choffs[i + 1] = newchoff;
					
					btree->nitems++;
				} 
			} else {
				nchild = (LPBTNODE)child;
				if (_BptRedistributeNodeLeft(bpt, btree, i)) {
					//btree->keys[i] = nchild->keys[0];
				} else if (_BptRedistributeNodeRight(bpt, btree, i)) {
					//can't really do anything here
				} else {
					newchoff  = _BptSplitNode(bpt, nchild);
					newnchild = (LPBTNODE)(bpt->baseaddr + newchoff);
					newkey    = nchild->keys[BT_NBRANCHES / 2 - 1];
					_BptMakeSpaceNode(btree, i);
					
					btree->choffs[i + 1] = newchoff;
					btree->keys[i]       = newkey;

					btree->nitems++;
				}
			}
			if (btree->nitems == BT_NBRANCHES)
				return BT_OVERFLOW;
		}
	}
	return 1;
}


int BptInsert(LPBPTREE bpt, KEYTYPE key, VALTYPE value) {
	LPBTNODE newroot, newnode;
	LPBTLEAF newleaf;
	unsigned int newrootoff, newchildoff;
	int result;

	if (!bpt)
		return 0;

	//lock here
	bpt->header->dirty = 1;

	result = _BptInsertWorker(bpt, bpt->root, key, value);
	if (!result)
		return 0;
	
	if (result == BT_OVERFLOW) {
		newrootoff = _BptCreateNode(bpt);
		newroot    = (LPBTNODE)(bpt->baseaddr + newrootoff);
		newroot->nitems = 1;

		if (bpt->root->nitems & BT_LEAF) {
			newchildoff = _BptSplitLeaf(bpt, (LPBTLEAF)bpt->root);
			newleaf     = (LPBTLEAF)(bpt->baseaddr + newchildoff);

			newroot->keys[0] = newleaf->items[0].key;
		} else {
			newchildoff = _BptSplitNode(bpt, bpt->root);
			newnode     = (LPBTNODE)(bpt->baseaddr + newchildoff);

			newroot->keys[0] = bpt->root->keys[BT_NBRANCHES / 2 - 1];
		}

		newroot->choffs[0] = (char *)bpt->root - bpt->baseaddr;
		newroot->choffs[1] = newchildoff;
		bpt->root = newroot;
		bpt->header->rootoff = newrootoff;
		bpt->header->depth++;
	}

	bpt->header->dirty = 0;
	//unlock here

	return 1;
}


inline LPBTLEAF _BptGetContainingLeaf(LPBPTREE bpt, KEYTYPE key) {
	LPBTNODE node;
	int i;

	node = bpt->root;
	while (!(node->nitems & BT_LEAF)) {
#		if BT_NBRANCHES < 32
			for (i = 0; i != node->nitems && (node->keys[i] <= key); i++);
#		else
			i = _BptBinarySearch(node->keys, node->nitems, key);
#		endif

		node = (LPBTNODE)(bpt->baseaddr + node->choffs[i]);
	}
	return (LPBTLEAF)node;
}


int BptSearch(LPBPTREE bpt, KEYTYPE key, VALTYPE *val) {
	LPBTLEAF leaf;
	int i;

	if (!bpt || !val)
		return BT_ERROR;

	leaf = _BptGetContainingLeaf(bpt, key);
	
	for (i = 0; i != BTNITEMS(leaf) && key != leaf->items[i].key; i++);
	if (i == BTNITEMS(leaf))
		return BT_NOTFOUND;

	*val = leaf->items[i].val;

	return 1;
}

#if 0
int BptSearchRange(LPBPTREE bpt, KEYTYPE key, KEYTYPE delta, KVPAIR **matches_out) {
	KVPAIR *results;
	LPBTLEAF leaf, fleaf, bleaf;
	int i, nitems, fleafpos, bleafpos, curindex;

	if (!bpt || !bpt->baseaddr || !bpt->root || !matches_out)
		return 0;
	
	leaf = _BptGetContainingLeaf(bpt, key);

	nitems = 0;

	//forward boundary search
	fleaf = leaf;
	while (fleaf->nextoff) {
		if (BTNITEMS(fleaf) && fleaf->items[BTNITEMS(fleaf) - 1].key > key + delta)
			break;
		nitems += BTNITEMS(fleaf);
		fleaf = (LPBTLEAF)(bpt->baseaddr + fleaf->nextoff);
	}

	for (i = 0; i != BTNITEMS(fleaf); i++) {
		if (fleaf->items[i].key > key + delta)
			break;
		nitems++;
	}
	fleafpos = i;

	//backward boundary search
	bleaf = leaf;
	while (bleaf->prevoff) {
		if (BTNITEMS(bleaf) && bleaf->items[0].key < key - delta)
			break;
		nitems += BTNITEMS(bleaf);
		bleaf = (LPBTLEAF)(bpt->baseaddr + bleaf->prevoff);
	}
	
	for (i = BTNITEMS(bleaf) - 1; i >= 0; i--) {
		if (bleaf->items[i].key < key - delta)
			break;
		nitems++;
	}
	bleafpos = i + 1;

	if (!nitems)
		return BT_NOTFOUND;

	//correction since leaf is iterated over twice - it's messy if we don't.
	nitems -= BTNITEMS(leaf); 

	//now put the mathching key/value pairs in the result array
	results = malloc(nitems * sizeof(KVPAIR));
	curindex = 0;

	for (i = bleafpos; i != BTNITEMS(bleaf); i++) {
		results[curindex].key = bleaf->items[i].key;
		results[curindex].val = bleaf->items[i].val;
		curindex++;
	}

	leaf = (LPBTLEAF)(bpt->baseaddr + bleaf->nextoff);
	while (leaf != fleaf) {
		for (i = 0; i != BTNITEMS(leaf); i++) {
			results[curindex].key = leaf->items[i].key;
			results[curindex].val = leaf->items[i].val;
			curindex++;
		}
		leaf = (LPBTLEAF)(bpt->baseaddr + leaf->nextoff);
	}

	for (i = 0; i != fleafpos; i++) {
		results[curindex].key = fleaf->items[i].key;
		results[curindex].val = fleaf->items[i].val;
		curindex++;
	}

	*matches_out = results;

	return nitems;
}
#endif


int BptSearchRange(LPBPTREE bpt, KEYTYPE min, KEYTYPE max, KVPAIR **matches_out) {
	KVPAIR *results;
	LPBTLEAF leaf, fleaf, bleaf;
	int i, nitems, fleafpos, bleafpos, curindex, leafic;

	if (!bpt || !matches_out || max < min)
		return BT_ERROR;
	
	leaf  = _BptGetContainingLeaf(bpt, min);

	nitems = 0;

	//scan for the beginning
	for (i = 0; i != BTNITEMS(leaf) && (leaf->items[i].key < min); i++);
	if (i == BTNITEMS(leaf)) {
		i = 0;
		leaf = (LPBTLEAF)(bpt->baseaddr + leaf->nextoff);
	}
		//return BT_NOTFOUND; //nothing was >= min
	bleaf = leaf;
	bleafpos = i;

	//scan through the links until the end of the range
	while (1) {
		leafic = BTNITEMS(leaf);
		if (leafic && leaf->items[leafic - 1].key > max) {
			for (i = 0; i != leafic && (leaf->items[i].key <= max); i++);
			fleafpos = i;
			break;
		}
		if (!leaf->nextoff) {
			fleafpos = leafic;
			break;
		}
		nitems += leafic;
		leaf = (LPBTLEAF)(bpt->baseaddr + leaf->nextoff);
	}
	fleaf = leaf;
	nitems += fleafpos;
	nitems -= bleafpos;

	if (!nitems)
		return BT_NOTFOUND;
	if (nitems < 0)
		return BT_ERROR;

	//now put the mathching key/value pairs in the result array
	results = malloc(nitems * sizeof(KVPAIR));

	curindex = 0;
	i = bleafpos;
	leaf = bleaf;
	while (i != fleafpos || leaf != fleaf) {
		results[curindex] = leaf->items[i];
		curindex++;

		i++;
		if (i == BTNITEMS(leaf)) {
			if (!leaf->nextoff)
				break;
			leaf = (LPBTLEAF)(bpt->baseaddr + leaf->nextoff);
			//if (leaf == fleaf && i == fleafpos)
			//	break;
			i = 0;
		}
	}

	*matches_out = results;

	return nitems;
}


int BptGetMin(LPBPTREE bpt, KVPAIR *min) {
	LPBTNODE node;
	LPBTLEAF leaf;

	if (!bpt || !min)
		return BT_ERROR;

	if (!bpt->header->nitems)
		return BT_NOTFOUND;

	node = bpt->root;
	while (!(node->nitems & BT_LEAF))
		node = (LPBTNODE)(bpt->baseaddr + node->choffs[0]);

	leaf = (LPBTLEAF)node;
	while (BTNITEMS(leaf) == 0)
		leaf = (LPBTLEAF)(bpt->baseaddr + leaf->nextoff);

	memcpy(min, &leaf->items[0], sizeof(KVPAIR));

	return 1;
}


int BptGetMax(LPBPTREE bpt, KVPAIR *max) {
	LPBTNODE node;
	LPBTLEAF leaf;

	if (!bpt || !max)
		return BT_ERROR;

	if (!bpt->header->nitems)
		return BT_NOTFOUND;

	node = bpt->root;
	while (!(node->nitems & BT_LEAF))
		node = (LPBTNODE)(bpt->baseaddr + node->choffs[node->nitems]);

	leaf = (LPBTLEAF)node;
	while (BTNITEMS(leaf) == 0)
		leaf = (LPBTLEAF)(bpt->baseaddr + leaf->prevoff);

	memcpy(max, &leaf->items[BTNITEMS(leaf) - 1], sizeof(KVPAIR));

	return 1;
}


int BptEnumerate(LPBPTREE bpt, KVPAIR **results_out) {
	LPBTNODE node;
	LPBTLEAF leaf;
	KVPAIR *items;
	int nitems, curitem, i;

	if (!bpt || !results_out)
		return BT_ERROR;

	nitems = bpt->header->nitems;
	if (!nitems)
		return BT_NOTFOUND;

	node = bpt->root;
	while (!(node->nitems & BT_LEAF))
		node = (LPBTNODE)(bpt->baseaddr + node->choffs[0]);
	leaf = (LPBTLEAF)node;

	items = malloc(nitems * sizeof(KVPAIR));

	curitem = 0;
	while (leaf) {
		for (i = 0; i != BTNITEMS(leaf); i++) {
			items[curitem] = leaf->items[i];
			curitem++;
		}
		leaf = (LPBTLEAF)(bpt->baseaddr + leaf->nextoff);
	}

	if (curitem != nitems) { //should never happen!
		fprintf(stderr, "ERROR: BptEnumerate: item count inconsistency, "
			"curitem == %d, nitems == %d\n", curitem, nitems);
		free(items);
		return BT_ERROR;
	}

	*results_out = items;

	return nitems;
}


int BptRemove(LPBPTREE bpt, KEYTYPE key) {
	LPBTLEAF leaf;
	int i;

	if (!bpt)
		return 0;

	//lock here
	bpt->header->dirty = 1;

	leaf = _BptGetContainingLeaf(bpt, key);

	for (i = 0; i != BTNITEMS(leaf) && key != leaf->items[i].key; i++);
	if (i == BTNITEMS(leaf))
		return BT_NOTFOUND;

#ifdef BT_USE_BINS
	if (leaf->items[i].attribs & BT_ITEM_VALISBIN) {
		unsigned int binoff;
		LPBTBIN bin;

		binoff = leaf->items[i].binoff;
		while (binoff) {
			bin = (LPBTBIN)(bpt->baseaddr + binoff);
			bin->attribs |= BT_DELETED;
			binoff = bin->nextbinoff;
		}
	}
#endif

	for (; i != BTNITEMS(leaf) - 1; i++)
		leaf->items[i] = leaf->items[i + 1];

	bpt->header->nitems--;

	bpt->header->dirty = 0;
	//unlock here

	return 1;
}


///////////////////////////////////////////////////////////////////////////////
int bgcolor, fgcolor;

#define IMG_CX 1800
#define IMG_CY 270

#define LEAF_CX (30)
#define LEAF_CY (BT_NBRANCHES * 12 + 3)

#define NODE_CX (BT_NBRANCHES * 8)
#define NODE_CY (14)


void _BptDrawWorker(gdImagePtr im, LPBPTREE bpt, LPBTNODE node, int level, int index, int xpos) {
	unsigned int i;
	LPBTLEAF leaf;
	LPBTNODE child;
	int x1, x2, y1, y2, newxpos;
	char buf[32];

	if (node->nitems & BT_LEAF) {
		leaf = (LPBTLEAF)node;

		x1 = xpos - LEAF_CX / 2;
		x2 = xpos + LEAF_CX / 2;

		y1 = level * 45 + 15;
		y2 = y1 + LEAF_CY;

		gdImageFilledRectangle(im, x1, y1, x2, y2, bgcolor);
		for (i = 0; i != BTNITEMS(leaf); i++) {
			sprintf(buf, "%f, %d", leaf->items[i].key, leaf->items[i].val);
			gdImageString(im, gdFontGetTiny(), x1 + 2, y1 + i * 12, (unsigned char *)buf, fgcolor);
		}
	} else {
		x1 = xpos - (node->nitems * 16) / 2;
		x2 = xpos + (node->nitems * 16) / 2;

		y1 = level * 45 + 15;
		y2 = y1 + NODE_CY / 2;
		y1 -= NODE_CY / 2;

		gdImageFilledRectangle(im, x1, y1, x2, y2, bgcolor);
		for (i = 0; i != node->nitems; i++) {
			sprintf(buf, "%f|", node->keys[i]);
			gdImageString(im, gdFontGetTiny(), x1 + 16 * i + 1, y1 + 2, (unsigned char *)buf, fgcolor);
		}

		level++;
		for (i = 0; i != node->nitems + 1; i++) {
			child = (LPBTNODE)(bpt->baseaddr + node->choffs[i]);
			if (child->nitems & BT_LEAF)
				newxpos = xpos + (int)(((float)i - (float)node->nitems / 2.f) * NODE_CX);
			else
				newxpos = xpos + (int)(((float)i - (float)node->nitems / 2.f) * NODE_CX * ((float)35 / (float)(level * 2))); 
			gdImageLine(im, x1 + i * 16, y2, newxpos, level * 45 + 15, fgcolor);
			_BptDrawWorker(im, bpt, child, level, i, newxpos);
		}
	}
}


int BptDraw(LPBPTREE bpt, const char *img_filename) {
	gdImagePtr im;

	im = gdImageCreateTrueColor(IMG_CX, IMG_CY);

	bgcolor = gdImageColorAllocate(im, 0xFF, 0x00, 0x00);
	fgcolor = gdImageColorAllocate(im, 0xFF, 0xFF, 0xFF);

	_BptDrawWorker(im, bpt, bpt->root, 0, 0, IMG_CX / 2);

	ImgSavePng(img_filename, im);

	gdImageDestroy(im);
	return 1;
}

