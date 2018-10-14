/***************************************************************************
* Name  : ordering.h
* Brief : ��̕��ёւ��֘A�̏������s��
* Date  : 2016/02/02
****************************************************************************/

#include "stdafx.h"

#pragma once

/* ���胊�X�g */

typedef struct {
	char move;
	UINT64 rev;
}MOVELIST;

UINT32 GetOrderPosition(UINT64 blank);
UINT32 MoveOrderingMiddle(INT8 *pos_list, UINT64 b_board, UINT64 w_board,
	HashTable *hash, UINT64 moves, UINT64 rev_list[], INT32 depth, UINT32 empty,
	INT32 alpha, INT32 beta, UINT32 color);

void SortMoveListTableMoveFirst(MoveList *movelist, int move);
void SortFastfirst(MoveList *movelist, UINT64 bk, UINT64 wh);
void SortPotentionalFastfirst(MoveList *movelist, UINT64 bk, UINT64 wh, UINT64 blank);
void SortMoveListMiddle(
	MoveList *movelist,
	UINT64 bk, UINT64 wh,
	HashTable *hash,
	UINT32 empty,
	INT32 alpha, INT32 beta,
	UINT32 color);
void SortMoveListEnd(
	MoveList *movelist,
	UINT64 bk, UINT64 wh,
	HashTable *hash,
	UINT32 empty,
	INT32 alpha, INT32 beta,
	UINT32 color);