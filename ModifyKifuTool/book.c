/***************************************************************************
* Name  : book.cpp
* Brief : �ǖʂƒ�΃f�[�^���Ƃ炵���킹�Ē���
* Date  : 2016/02/01
****************************************************************************/
#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "bit64.h"
#include "board.h"
#include "rev.h"
#include "move.h"
#include "eval.h"
#include "board.h"
#include "book.h"
#include "fio.h"
#include "mt.h"

#define N_BOOK_MAX 1000000


typedef struct
{
	INT32 eval;
	BooksNode *node;
}BookData;

/***************************************************************************
*
* Global
*
****************************************************************************/
/* �w����̉�]�E�Ώ̕ϊ��t���O */
int TRANCE_MOVE;


BooksNode *g_hashBookTable[N_BOOK_MAX];
BooksNode *g_bookTreeRoot;
BooksNode *g_bestNode;
BookData g_bookList[36];
INT32 g_booklist_cnt;

double g_err_rate;
INT32 max_change_num[2];
BOOL g_book_done;


/***************************************************************************
*
* ProtoType(private)
*
****************************************************************************/
INT32 SearchBooks(BooksNode *book_root, UINT64 bk, UINT64 wh, UINT32 color, UINT32 change, INT32 turn);
BooksNode *SearchBookInfo(BooksNode *book_header, BooksNode *before_book_header, UINT64 bk, UINT64 wh, INT32 turn);
INT32 book_alphabeta(BooksNode *header, UINT32 depth, INT32 alpha, INT32 beta, UINT32 color);
void sort_book_node(BookData data_list[], int cnt);
BookData *select_node(UINT32 change);



/***************************************************************************
* Name  : GetMoveFromBooks
* Brief : ��΂���CPU�̒�������肷��
* Return: ����\�ʒu�̃r�b�g��
****************************************************************************/
UINT64 GetMoveFromBooks(UINT64 bk, UINT64 wh, UINT32 color, UINT32 change, INT32 turn)
{
	INT64 move;
	if (turn == 0 && bk == BK_FIRST && wh == WH_FIRST)
	{
		UINT64 first_move_list[] = { c4, d3, e6, f5 };
		// ���ڂ̒���͂ǂ��ɒ��肵�Ă������Ȃ̂Ń����_���Ƃ���
		int rand = genrand_int32() % 4;
		return first_move_list[rand];

	}
	else if (g_bookTreeRoot != NULL)
	{
		move = SearchBooks(g_bookTreeRoot, bk, wh, color, change, turn);
	}
	else
	{
		move = MOVE_NONE;
	}

	if (move == MOVE_NONE)
	{
		return move;
	}

	return 1ULL << move;

}


void set_book_error(int change)
{

	switch (change)
	{
	case 0: // no random
		g_err_rate = 0.0;
		break;
	case 1: // err 5%
		g_err_rate = 0.05;
		break;
	case 2: // err 10%
		g_err_rate = 0.1;
		break;
	case 3: // err 20%
		g_err_rate = 0.2;
		break;
	case 4: // full random
		g_err_rate = 100.0;
		break;
	}

}


/***************************************************************************
* Name  : SearchBooks
* Brief : ��΂₩��CPU�̒�������肷��
* Return: ����\�ʒu�̃r�b�g��
****************************************************************************/
INT32 SearchBooks(BooksNode *book_root, UINT64 bk, UINT64 wh,
	UINT32 color, UINT32 change, INT32 turn)
{
	INT32 move = MOVE_NONE;
	ULONG eval = 0;
	BookData *book_data;
	BooksNode *book_header;

	g_booklist_cnt = 0;
	// �ω��x�ݒ�
	set_book_error(change);

	/* �ǖʂ���Y���̒�΂�T�� */
	book_header = SearchBookInfo(book_root, book_root, bk, wh, turn);

	if (book_header != NULL)
	{
		/* �]���l�ɂ�莟�̎���΂���I�� */
		eval = book_alphabeta(book_header, 0, NEGAMIN, NEGAMAX, color);
		book_data = select_node(change);
		g_evaluation = book_data->eval;

		/* �w����̑Ώ̉�]�ϊ��̏ꍇ���� */
		switch (TRANCE_MOVE)
		{
		case 0:
			move = book_data->node->move;
			break;
		case 1:
		{
			UINT64 t_move = rotate_90(1ULL << book_data->node->move);
			move = CountBit((t_move & (-(INT64)t_move)) - 1);
		}
		break;
		case 2:
		{
			UINT64 t_move = rotate_180(1ULL << book_data->node->move);
			move = CountBit((t_move & (-(INT64)t_move)) - 1);
		}
		break;
		case 3:
		{
			UINT64 t_move = rotate_270(1ULL << book_data->node->move);
			move = CountBit((t_move & (-(INT64)t_move)) - 1);
		}
		break;
		case 4:
		{
			UINT64 t_move = symmetry_x(1ULL << book_data->node->move);
			move = CountBit((t_move & (-(INT64)t_move)) - 1);
		}
		break;
		case 5:
		{
			UINT64 t_move = symmetry_y(1ULL << book_data->node->move);
			move = CountBit((t_move & (-(INT64)t_move)) - 1);
		}
		break;
		case 6:
		{
			UINT64 t_move = symmetry_b(1ULL << book_data->node->move);
			move = CountBit((t_move & (-(INT64)t_move)) - 1);
		}
		break;
		case 7:
		{
			UINT64 t_move = symmetry_w(1ULL << book_data->node->move);
			move = CountBit((t_move & (-(INT64)t_move)) - 1);
		}
		break;
		default:
			move = book_data->node->move;
			break;
		}
	}

	return move;
}



/***************************************************************************
* Name  : SearchBookInfo
* Brief : ��΂���CPU�̒�������肷��
* Return: ����\�ʒu�̃r�b�g��
****************************************************************************/
BooksNode *SearchBookInfo(BooksNode *book_header, BooksNode *before_book_header,
	UINT64 bk, UINT64 wh, INT32 turn)
{
	/* �t�m�[�h�܂Ō������Č�����Ȃ��ꍇ */
	if (book_header == NULL)
	{
		return NULL;
	}
	if (book_header->depth > turn)
	{
		return NULL;
	}
	if (book_header->depth == turn)
	{
		do
		{
			/* �Y���̒�΂𔭌�(��]�E�Ώ̂��l����) */
			if (book_header->bk == bk && book_header->wh == wh)
			{
				/* �w����̉�]�E�Ώ̕ϊ��Ȃ� */
				TRANCE_MOVE = 0;
				return before_book_header;
			}
			/* 90�x�̉�]�` */
			if (book_header->bk == rotate_90(bk) && book_header->wh == rotate_90(wh))
			{
				TRANCE_MOVE = 1;
				return before_book_header;
			}
			/* 180�x�̉�]�` */
			if (book_header->bk == rotate_180(bk) && book_header->wh == rotate_180(wh))
			{
				TRANCE_MOVE = 2;
				return before_book_header;
			}
			/* 270�x�̉�]�` */
			if (book_header->bk == rotate_270(bk) && book_header->wh == rotate_270(wh))
			{
				TRANCE_MOVE = 3;
				return before_book_header;
			}
			/* X���̑Ώ̌` */
			if (book_header->bk == symmetry_x(bk) && book_header->wh == symmetry_x(wh))
			{
				TRANCE_MOVE = 4;
				return before_book_header;
			}
			/* Y���̑Ώ̌` */
			if (book_header->bk == symmetry_y(bk) && book_header->wh == symmetry_y(wh))
			{
				TRANCE_MOVE = 5;
				return before_book_header;
			}
			/* �u���b�N���C���̑Ώ̌` */
			if (book_header->bk == symmetry_b(bk) && book_header->wh == symmetry_b(wh))
			{
				TRANCE_MOVE = 6;
				return before_book_header;
			}
			/* �z���C�g���C���̑Ώ̌` */
			if (book_header->bk == symmetry_w(bk) && book_header->wh == symmetry_w(wh))
			{
				TRANCE_MOVE = 7;
				return before_book_header;
			}
			book_header = book_header->next;
		} while (book_header != NULL);

		return NULL;
	}

	BooksNode *ret;
	if ((ret = SearchBookInfo(book_header->child, book_header, bk, wh, turn)) != NULL)
	{
		return ret;
	}

	book_header = book_header->next;
	while (book_header != NULL)
	{
		/* �Y���ǖʂ��擾 */
		if ((ret = SearchBookInfo(book_header->child, book_header, bk, wh, turn)) != NULL)
		{
			return ret;
		}
		book_header = book_header->next;
	}
	return NULL;
}



/***************************************************************************
* Name  : book_alphabeta
* Brief : ��΂̌���̂����A��X�ɕ]���l���ł������Ȃ���̂��Z�o����
* Return: ��΂̕]���l
****************************************************************************/
INT32 book_alphabeta(BooksNode *header, UINT32 depth, INT32 alpha, INT32 beta, UINT32 color)
{

	// �t�m�[�h�H
	if (header->next == NULL && header->child == NULL)
	{
		if (abs(header->eval) == NEGAMAX)
		{
			header->eval = NEGAMAX;
		}
		/* �t�m�[�h(�ǂ݂̌��E�l�̃m�[�h)�̏ꍇ�͕]���l���Z�o */
		if (color == WHITE) return -header->eval;
		return header->eval;
	}

	int eval;
	int max;
	max = NEGAMIN;
	if (header->child)
	{
		if (depth != 0)
		{
			eval = -book_alphabeta(header->child, depth + 1, -beta, -alpha, color ^ 1);
		}
		else
		{
			// Prevent cutting(alpha) node for get correct evaluation
			eval = -book_alphabeta(header->child, depth + 1, NEGAMIN, NEGAMAX, color ^ 1);
		}
		// ���J�b�g
		if (depth != 0 && beta <= eval)
		{
			return eval;
		}
		/* ���܂ł��ǂ��ǖʂ�������΍őP��̍X�V */
		if (eval > max)
		{
			max = eval;
			if (alpha < max) alpha = max;
		}

		if (depth == 0)
		{
			g_bookList[g_booklist_cnt].eval = eval;
			g_bookList[g_booklist_cnt++].node = header->child;
		}
	}

	BooksNode *next_node = header->child;
	if (next_node != NULL)
	{
		while (next_node->next)
		{
			if (depth != 0)
			{
				eval = -book_alphabeta(next_node->next, depth + 1, -beta, -alpha, color ^ 1);
			}
			else
			{
				// Prevent cutting(alpha) node for get correct evaluation
				eval = -book_alphabeta(next_node->next, depth + 1, NEGAMIN, NEGAMAX, color ^ 1);
			}

			// ���J�b�g
			if (depth != 0 && beta <= eval)
			{
				return eval;
			}

			/* ���܂ł��ǂ��ǖʂ�������΍őP��̍X�V */
			if (eval > max)
			{
				max = eval;
				if (alpha < max) alpha = max;
			}

			if (depth == 0)
			{
				g_bookList[g_booklist_cnt].eval = eval;
				g_bookList[g_booklist_cnt++].node = next_node->next;
			}

			next_node = next_node->next;
		}
	}
	else
	{
		// NEXT�m�[�h�����Ȃ�����(���m�[�h�ɕ]���l������̂ŕԂ�)
		if (color == WHITE) max = -header->eval;
		else max = header->eval;
	}

	return max;
}



/***************************************************************************
* Name  : sort_book_node
* Brief : ��΂̌���̂����A�]���l�̍������Ƀ\�[�g
****************************************************************************/
void sort_book_node(BookData data_list[], int cnt)
{
	BookData temp;

	for (int i = 0; i < cnt - 1; i++)
	{
		for (int j = cnt - 1; j > i; j--)
		{
			if (data_list[j - 1].eval < data_list[j].eval)
			{
				temp = data_list[j - 1];
				data_list[j - 1] = data_list[j];
				data_list[j] = temp;
			}
		}
	}

}

/***************************************************************************
* Name  : extract_booklist
* Brief : �덷�ɂ��m�[�h�I��
****************************************************************************/
BookData *extract_booklist(BookData *node_list, double rate, int cnt)
{
	int i;
	int entry_count = 1;
	BookData *extracted_list[36];

	extracted_list[0] = &node_list[0]; // best node

	double abs_threshold = 160000 * rate;

	for (i = 1; i < cnt; i++)
	{
		if (node_list[0].eval - node_list[i].eval <= abs_threshold)
		{
			extracted_list[i] = &node_list[i];
			entry_count++;
		}
	}

	return extracted_list[genrand_int32() % entry_count];
}

/***************************************************************************
* Name  : SelectNode
* Brief : ��΂̕ω��x�ɂ���Č�������肷��
* Return: ��Δԍ�
****************************************************************************/
BookData *select_node(UINT32 change)
{
	BookData *ret = NULL;

	sort_book_node(g_bookList, g_booklist_cnt);

	if (change == NOT_CHANGE)
	{
		ret = &g_bookList[0];
	}
	else if (change != CHANGE_RANDOM) // err = 5%�`20%
	{
		ret = extract_booklist(g_bookList, g_err_rate, g_booklist_cnt);
	}
	else // full random
	{
		ret = &g_bookList[genrand_int32() % g_booklist_cnt];
	}

	return ret;
}



/* �m�[�h��ڑ� */
void AppendChild(BooksNode *parent, BooksNode *node)
{
	if (parent == NULL)
	{
		return;
	}
	if (parent->child == NULL)
	{
		parent->child = node;
	}
}



/* �m�[�h��ڑ� */
void AppendNext(BooksNode *parent, BooksNode *node)
{
	if (parent == NULL)
	{
		return;
	}

	while (parent->next != NULL)
	{
		parent = parent->next;
	}
	parent->next = node;
}



BooksNode *SearchChild(BooksNode *head, int move)
{
	if (head->child == NULL)
	{
		return NULL;
	}
	head = head->child;
	while (head != NULL)
	{
		if (head->move == move)
		{
			return head;
		}
		head = head->next;
	}

	return NULL;
}



void bookline_strtok(char *line_data, char* dest_data, INT32 *eval)
{
	int cnt;
	for (cnt = 0; line_data[cnt] != ';'; cnt++);

	memcpy_s(dest_data, 128, line_data, cnt);
	dest_data[cnt] = '\0';
	*eval = (INT32)(atof(&line_data[cnt + 1]) * EVAL_ONE_STONE);
}


UINT32 g_malloc_count = 0;
UINT32 g_debug_count = 0;
BooksNode *StructionBookTree(BooksNode *head, INT32 *new_eval_p, char *line_data_p, char** next, INT32 depth, INT32 *return_depth)
{
	char *decode_sep;
	char move;
	INT32 current_eval;
	UINT64 rev;

	// �s�̏I���`�F�b�N
	if (line_data_p[depth] == '\0')
	{
		/* ���C�����I�������̂Ŏ����C���𕪗� */
		decode_sep = strtok_s(NULL, "\n", next);
		g_debug_count++;

		if (g_debug_count == 68022)
		{
			g_debug_count = 68022;
		}

		if (decode_sep == NULL)
		{
			// end...
			head->eval = *new_eval_p;
			return NULL;
		}

		int i;
		char old_line_data[256];
		strcpy_s(old_line_data, 256, line_data_p);
		// ���C���f�[�^�ƕ]���l�𕪗�
		bookline_strtok(decode_sep, line_data_p, &current_eval);
		head->eval = *new_eval_p;
		*new_eval_p = current_eval;

		// �߂�[��������
		int new_len = (int)strlen(line_data_p);
		int old_len = (int)strlen(old_line_data);
		if (old_len > new_len)
		{
			for (i = 0; i < new_len; i += 2)
			{
				if (old_line_data[i] != line_data_p[i] ||
					old_line_data[i + 1] != line_data_p[i + 1]) break;
			}
		}
		else
		{
			for (i = 0; i < old_len; i += 2)
			{
				if (old_line_data[i] != line_data_p[i] ||
					old_line_data[i + 1] != line_data_p[i + 1]) break;
			}
		}

		*return_depth = i;

		return head;
	}
	else
	{
		// ���C���������擾
		move = (line_data_p[depth] - 'a') * 8 + line_data_p[depth + 1] - '1';
		// �m�[�h���쐬���ēo�^
		BooksNode *child_node = (BooksNode *)malloc(sizeof(BooksNode));
		g_malloc_count++;
		child_node->child = NULL;
		child_node->next = NULL;
		child_node->depth = depth / 2;
		child_node->eval = NEGAMIN;
		child_node->move = move;
		if ((child_node->depth % 2) != 0)
		{
			// ����
			rev = GetRev[head->move](head->bk, head->wh);
			child_node->bk = head->bk ^ (rev | (1ULL << head->move));
			child_node->wh = head->wh ^ rev;
		}
		else
		{
			// ����
			rev = GetRev[head->move](head->wh, head->bk);
			child_node->wh = head->wh ^ (rev | (1ULL << head->move));
			child_node->bk = head->bk ^ rev;
		}

		/* first entry, start child node area */

		BooksNode *ret;
		ret = StructionBookTree(child_node, new_eval_p, line_data_p, next, depth + 2, return_depth);

		// file end or fault sequence
		if (ret == NULL) {
			free(child_node);
			return NULL;
		}

		// �m�[�h��o�^
		AppendChild(head, child_node);

		int count = 0;
		while (*return_depth == depth)
		{

			move = (line_data_p[depth] - 'a') * 8 + line_data_p[depth + 1] - '1';

			// �m�[�h���쐬���ēo�^
			BooksNode *next_node = (BooksNode *)malloc(sizeof(BooksNode));
			g_malloc_count++;
			next_node->child = NULL;
			next_node->next = NULL;
			next_node->depth = depth / 2;
			next_node->eval = NEGAMIN;
			next_node->move = move;
			if ((next_node->depth % 2) != 0)
			{
				// ����
				rev = GetRev[head->move](head->bk, head->wh);
				next_node->bk = head->bk ^ (rev | (1ULL << head->move));
				next_node->wh = head->wh ^ rev;
			}
			else
			{
				// ����
				rev = GetRev[head->move](head->wh, head->bk);
				next_node->wh = head->wh ^ (rev | (1ULL << head->move));
				next_node->bk = head->bk ^ rev;
			}

			ret = StructionBookTree(next_node, new_eval_p, line_data_p, next, depth + 2, return_depth);

			// file end sequence
			if (ret == NULL) {
				// �m�[�h��o�^
				AppendNext(head->child, next_node);
				return NULL;
			}

			// �m�[�h��o�^
			AppendNext(head->child, next_node);

		}

		return head;
	}

}

/***************************************************************************
* Name  : OpenBook
* Brief : ��΃f�[�^���J��
* Return: TRUE/FALSE
****************************************************************************/
BOOL OpenBook(char *filename)
{
	UCHAR* decodeData;
	char *decode_sep, *next_line;
	INT64 decodeDataLen;
	BooksNode *root;
	char line_data[128];
	INT32 eval;

	// �t�@�C������f�R�[�h
	decodeData = DecodeBookData(&decodeDataLen, filename);
	if (decodeDataLen == -1)
	{
		return FALSE;
	}

	// root�ݒ�
	root = (BooksNode *)malloc(sizeof(BooksNode));
	root->bk = BK_FIRST;
	root->wh = WH_FIRST;
	root->eval = 0;
	root->depth = 0;
	root->move = 19; /* C4 */
	root->child = NULL;
	root->next = NULL;

	decode_sep = strtok_s(decodeData, "\n", &next_line);
	if (decode_sep == NULL)
	{
		// end...
		return FALSE;
	}
	// ���C���f�[�^�ƕ]���l�𕪗�
	bookline_strtok(decode_sep, line_data, &eval);

	// �[���D��Ŗ؍\�����\�z
	INT32 return_depth = 0;
	StructionBookTree(root, &eval, line_data, &next_line, 2, &return_depth);

	g_bookTreeRoot = root;

	free(decodeData);
	if (g_bookTreeRoot->child == NULL)
	{
		// ���������Ȃ�
		return FALSE;
	}

	return TRUE;
}

/***************************************************************************
* Name  : BookFree
* Brief : ��΃f�[�^�̂��߂̃����������
****************************************************************************/
void BookFree(BooksNode *head)
{
	// �t�m�[�h�H
	if (head->next == NULL && head->child == NULL)
	{
		free(head);
		return;
	}

	if (head->child)
	{
		BookFree(head->child);
		head->child = NULL;
	}

	while (head->next)
	{
		BookFree(head->next);
		head->next = NULL;
	}

	free(head);

	return;
}