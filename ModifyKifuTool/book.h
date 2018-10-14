/***************************************************************************
* Name  : book.h
* Brief : ��Ί֘A�̏������s��
* Date  : 2016/02/01
****************************************************************************/

#include "stdafx.h"

#pragma once

#define NOT_CHANGE    0
#define CHANGE_LITTLE 1
#define CHANGE_MIDDLE 2
#define CHANGE_ROUGH  3
#define CHANGE_RANDOM 4

#define BOOK_MAX 13000000

typedef struct node
{
	struct node *child;
	struct node *next;
	UINT64 bk;
	UINT64 wh;
	int eval;
	char move;
	char depth;
}BooksNode;

extern BOOL g_book_done;
extern BooksNode *g_bookTreeRoot;

/***************************************************************************
* Name  : GetMoveFromBooks
* Brief : ��΂₩��CPU�̒�������肷��
* Return: ����\�ʒu�̃r�b�g��
****************************************************************************/
UINT64 GetMoveFromBooks(UINT64 bk, UINT64 wh, UINT32 color, UINT32 change, INT32 turn);
BOOL OpenBook(char *filename);
void BookFree(BooksNode *head);