/***************************************************************************
* Name  : cpu.cpp
* Brief : �T���̏����S�ʂ��s��
* Date  : 2016/02/01
****************************************************************************/
#include "stdafx.h"
#include "bit64.h"
#include "board.h"
#include "move.h"
#include "rev.h"
#include "cpu.h"
#include "endgame.h"
#include "hash.h"
#include "eval.h"
#include "ordering.h"
#include "count_last_flip_carry_64.h"
#include "mt.h"


/***************************************************************************
*
* Global
*
****************************************************************************/
// CPU�ݒ�i�[�p
BOOL g_mpcFlag;
BOOL g_tableFlag;
INT32 g_solveMethod;
INT32 g_empty;
INT32 g_limitDepth;
UINT64 g_casheSize;
BOOL g_refresh_hash_flag = TRUE;
INT32 g_key;

// CPU AI���
BOOL g_AbortFlag;
UINT64 g_countNode;
INT32 g_move;
INT32 g_infscore;

HashTable *g_hash = NULL;

MPCINFO mpcInfo[22];
MPCINFO mpcInfo_end[26];
double MPC_END_CUT_VAL;
INT32 g_mpc_level;

// endgame mpc info
const double cutval_table[8] =
{
	0.25, 0.50, 0.75, 1.00, 1.50, 2.00, 2.40, 3.00
};

const int cutval_table_percent[8 + 1] =
{
	20, 38, 54, 68, 86, 95, 98, 99, 100
};
const INT32 g_max_cut_table_size = 8;

char g_cordinates_table[64][4];
char g_AiMsg[128];
char g_PVLineMsg[256];
SetMessageToGUI g_set_message_funcptr[3];

INT32 g_pvline[60];
INT32 g_pvline_len;
UINT64 g_pvline_board[2][60];

/***************************************************************************
*
* ProtoType(private)
*
****************************************************************************/
INT32 SearchMiddle(UINT64 bk, UINT64 wh, UINT32 emptyNum, UINT32 color);
INT32 SearchWinLoss(UINT64 bk, UINT64 wh, UINT32 emptyNum, UINT32 color);
INT32 SearchExact(UINT64 bk, UINT64 wh, UINT32 emptyNum, UINT32 color);

INT32 PVS_SearchDeep(UINT64 bk, UINT64 wh, INT32 depth, INT32 empty, UINT32 color,
	HashTable *hash, INT32 alpha, INT32 beta, UINT32 passed, INT32 *selectivity, PVLINE *pline);

/***************************************************************************
* Name  : CreateCpuMessage
* Brief : CPU�̒�������t�h�ɑ��M����
* Args  : msg    : ���b�Z�[�W�i�[��
*         wh     : ���b�Z�[�W�i�[��̑傫��
*         eval   : �]���l
*         move   : ����ԍ�
*         cnt    : �[��
*         flag   : middle or winloss or exact
* Return: ����\�ʒu�̃r�b�g��
****************************************************************************/
void CreateCpuMessage(char *msg, int msglen, int eval, int move, int cnt, int flag)
{
	if (flag == ON_MIDDLE){
		if (eval > 1280000)
		{
			sprintf_s(msg, msglen, "%s[WIN:%+d](depth = %d)", g_cordinates_table[move],
				eval - 1280000, cnt);
		}
		else if (eval < -1280000)
		{
			sprintf_s(msg, msglen, "%s[LOSS:%d](depth = %d)", g_cordinates_table[move],
				eval + 1280000, cnt);
		}
		else if (eval == 1280000)
		{
			sprintf_s(msg, msglen, "%s[DRAW](depth = %d)", g_cordinates_table[move], cnt);
		}
		else if (eval >= 0)
		{
			sprintf_s(msg, msglen, "%s[%.3f](depth = %d)", g_cordinates_table[move],
				eval / (double)EVAL_ONE_STONE, cnt);
		}
		else
		{
			sprintf_s(msg, msglen, "%s[%.3f](depth = %d)", g_cordinates_table[move],
				eval / (double)EVAL_ONE_STONE, cnt);
		}
	}
	else if (flag == ON_WLD)
	{
		if (cnt == -1)
		{
			if (eval == WIN)
			{
				sprintf_s(msg, msglen, "guess... move %s [ WIN? ]", g_cordinates_table[move]);
			}
			else if (eval == LOSS)
			{
				sprintf_s(msg, msglen, "guess... move %s [ LOSS? ]", g_cordinates_table[move]);
			}
			else
			{
				sprintf_s(msg, msglen, "guess... move %s [ DRAW? ]", g_cordinates_table[move]);
			}
		}
		else
		{
			if (eval == WIN)
			{
				sprintf_s(msg, msglen, "%s [ WIN ]", g_cordinates_table[move]);
			}
			else if (eval == LOSS)
			{
				sprintf_s(msg, msglen, "%s [ LOSS ]", g_cordinates_table[move]);
			}
			else
			{
				sprintf_s(msg, msglen, "%s [ DRAW ]", g_cordinates_table[move]);
			}
		}

	}
	else
	{
		if (cnt == -2)
		{
			sprintf_s(msg, msglen, "guess... move %s [ %+d�`%+d ]", g_cordinates_table[move], eval - 1, eval + 1);
		}
		else if (cnt == -1)
		{
			if (eval > 0)
			{
				sprintf_s(msg, msglen, "guess... move %s [ WIN:%+d? ]", g_cordinates_table[move], eval);
			}
			else if (eval < 0)
			{
				sprintf_s(msg, msglen, "guess... move %s [ LOSS:%d? ]", g_cordinates_table[move], eval);
			}
			else
			{
				sprintf_s(msg, msglen, "guess... move %s [ DRAW:+0? ]", g_cordinates_table[move]);
			}
		}
		else
		{
			if (eval > 0)
			{
				sprintf_s(msg, msglen, "%s [ WIN:%+d ]", g_cordinates_table[move], eval);
			}
			else if (eval < 0)
			{
				sprintf_s(msg, msglen, "%s [ LOSS:%d ]", g_cordinates_table[move], eval);
			}
			else
			{
				sprintf_s(msg, msglen, "%s [ DRAW:+0 ]", g_cordinates_table[move]);
			}
		}
	}
}



void CreatePVLineStr(PVLINE *pline, INT32 empty, INT32 score)
{
	char *strptr = g_PVLineMsg;
	int bufsize = sizeof(g_PVLineMsg);
	int i;

	// PV shallower line
	for (i = 0; i <= g_empty - empty; i++)
	{
		sprintf_s(strptr, bufsize, "%s", g_cordinates_table[g_pvline[i]]);
		strptr += 2; // 2���������炷
		bufsize -= 2;
	}

	// PV deeper line
	int count = pline->cmove;
	for (; i < count; i++)
	{
		sprintf_s(strptr, bufsize, "%s", g_cordinates_table[pline->argmove[i]]);
		strptr += 2; // 2���������炷
		bufsize -= 2;
	}
}



void CreatePVLineStrAscii(INT32 *pline, INT32 empty, INT32 score)
{
	char *strptr = g_PVLineMsg;
	int bufsize = sizeof(g_PVLineMsg);

	// header
	int base;

	if (g_solveMethod == SOLVE_WLD)
	{
		char *wldstr[] = { "LOSS", "DRAW", "WIN" };
		base = sprintf_s(strptr, bufsize, "depth %d/%d@%s PV-Line : ", empty, g_empty, wldstr[score + 1]);
	}
	else if (g_solveMethod == SOLVE_EXACT)
	{
		base = sprintf_s(strptr, bufsize, "depth %d/%d@%+d PV-Line : ", empty, g_empty, score);
	}
	else
	{
		base = sprintf_s(strptr, bufsize, "depth %d/%d@%+.3f PV-Line : ",
			g_limitDepth - (g_empty - empty), g_limitDepth, score / (double)EVAL_ONE_STONE);
	}

	strptr += base;
	bufsize -= base;

	int i;
	// PV line from global
	for (i = 0; i < empty - 1; i++)
	{
		sprintf_s(strptr, bufsize, "%s-", g_cordinates_table[pline[i]]);
		strptr += 3; // 3���������炷
		bufsize -= 3;
	}

	sprintf_s(strptr, bufsize, "%s", g_cordinates_table[pline[i]]);
}


INT32 GetMoveFromHash(UINT64 bk, UINT64 wh, INT32 key)
{
	INT32 move;
	HashInfo *hashInfo = HashGet(g_hash, key, bk, wh);

	if (hashInfo != NULL && hashInfo->bestmove != -1) move = hashInfo->bestmove;
	else move = g_move;

	return move;
}


void StorePVLineToBoard(UINT64 bk, UINT64 wh, INT32 color, PVLINE *pline)
{
	INT32 error = 0;
	BOOL ret;
	g_pvline_len = pline->cmove;

	UINT64 bk_l = bk, wh_l = wh;

	for (int i = 0; i < g_pvline_len; i++)
	{
		if (pline->argmove[i] > 63) break;
		g_pvline[i] = pline->argmove[i];
		if (color == BLACK)
		{
			g_pvline_board[BLACK][i] = bk_l;
			g_pvline_board[WHITE][i] = wh_l;
			ret = boardMoves(&bk_l, &wh_l, 1ULL << g_pvline[i], g_pvline[i]);
			color ^= 1;
			if (ret == TRUE) error = 0;
		}
		else
		{
			g_pvline_board[BLACK][i] = wh_l;
			g_pvline_board[WHITE][i] = bk_l;
			ret = boardMoves(&wh_l, &bk_l, 1ULL << g_pvline[i], g_pvline[i]);
			color ^= 1;
			if (ret == TRUE) error = 0;
		}

		if (ret == FALSE)
		{
			i--; // ��蒼��
			error++;
			if (error == 2) break; // FFO#48 is entered this block...may be bug...?
		}
	}
}

/**
* @brief Stability Cutoff (SC).
*
* @param search Current position.
* @param alpha Alpha bound.
* @param beta Beta bound, to adjust if necessary.
* @param score Score to return in case of a cutoff is found.
* @return 'true' if a cutoff is found, false otherwise.
*/
BOOL search_SC_PVS(UINT64 bk, UINT64 wh, INT32 empty,
	volatile INT32 *alpha, volatile INT32 *beta, INT32 *score)
{
	if (*beta >= PVS_STABILITY_THRESHOLD[empty]) {
		*score = (64 - 2 * get_stability(wh, bk)) * EVAL_ONE_STONE;
		if (*score <= *alpha) {
			return TRUE;
		}
		else if (*score < *beta) *beta = *score;
	}
	return FALSE;
}


/***************************************************************************
* Name  : SetAbortFlag
* Brief : CPU�̏����𒆒f����
****************************************************************************/
void SetAbortFlag(){
	g_AbortFlag = TRUE;
}

/***************************************************************************
* Name  : GetMoveFromAI
* Brief : CPU�̒����T���ɂ���Č��肷��
* Args  : bk        : ���̃r�b�g��
*         wh        : ���̃r�b�g��
*         empty     : �󔒃}�X�̐�
*         cpuConfig : CPU�̐ݒ�
* Return: ����\�ʒu�̃r�b�g��
****************************************************************************/
UINT64 GetMoveFromAI(UINT64 bk, UINT64 wh, UINT32 emptyNum, CPUCONFIG *cpuConfig)
{
	UINT64 move;

	if (cpuConfig->color != BLACK && cpuConfig->color != WHITE)
	{
		// �ォ��n���ꂽ�p�����[�^���s��
		return ILLIGAL_ARGUMENT;
	}

	// �L���b�V����������΁A�L���b�V�����������m��(1MB�����͖�������)
	if (cpuConfig->tableFlag == TRUE && cpuConfig->casheSize >= 1024)
	{
		if (g_hash == NULL)
		{
			g_hash = HashNew(cpuConfig->casheSize);
			g_casheSize = cpuConfig->casheSize;
		}
		else if (g_casheSize != cpuConfig->casheSize){
			HashDelete(g_hash);
			g_hash = HashNew(cpuConfig->casheSize);
			g_casheSize = cpuConfig->casheSize;
		}
	}

	UINT32 temp;
	// CPU�̓p�X
	if (CreateMoves(bk, wh, &temp) == 0){
		return MOVE_PASS;
	}

	g_mpcFlag = cpuConfig->mpcFlag;
	g_tableFlag = cpuConfig->tableFlag;
	g_empty = emptyNum;

	// ���̋ǖʂ̒u���\�����������Ă���
	int key = KEY_HASH_MACRO(bk, wh, cpuConfig->color);

	// ���Ղ��ǂ������`�F�b�N
	if (emptyNum <= cpuConfig->exactDepth)
	{
		g_limitDepth = emptyNum;
		g_infscore = INF_SCORE;
		g_evaluation = SearchExact(bk, wh, emptyNum, cpuConfig->color);
	}
	else if (emptyNum <= cpuConfig->winLossDepth)
	{
		g_limitDepth = emptyNum;
		g_infscore = WIN + 1;
		g_evaluation = SearchWinLoss(bk, wh, emptyNum, cpuConfig->color);
	}
	else
	{
		g_limitDepth = cpuConfig->searchDepth;
		g_evaluation = SearchMiddle(bk, wh, emptyNum, cpuConfig->color) + cpuConfig->color * 17500;
	}

	g_AbortFlag = FALSE;
	// �u���\���璅����擾
	if (g_tableFlag)
	{
		move = 1ULL << GetMoveFromHash(bk, wh, key);
	}
	else
	{
		move = 1ULL << g_move;
	}

	return move;
}

/***************************************************************************
* Name  : SearchMiddle
* Brief : ���Ձ`���Ղ�CPU�̒����T���ɂ���Č��肷��
* Args  : bk        : ���̃r�b�g��
*         wh        : ���̃r�b�g��
*         empty     : �󔒃}�X�̐�
*         cpuConfig : CPU�̐ݒ�
* Return: ����]���l
****************************************************************************/
INT32 SearchMiddle(UINT64 bk, UINT64 wh, UINT32 emptyNum, UINT32 color)
{
	INT32 alpha = NEGAMIN;
	INT32 beta = NEGAMAX;
	INT32 eval = 0;
	INT32 eval_b = 0;
	INT32 limit = g_limitDepth;
	INT32 selectivity;
	UINT32 key = KEY_HASH_MACRO(bk, wh, color);
	INT32 move;
	PVLINE line;

	/* ���OAI�ݒ�g���p(���͉����Ȃ�) */
	if (g_limitDepth > (INT32)emptyNum)
	{
		g_limitDepth = emptyNum;
	}

	g_empty = emptyNum;
	g_solveMethod = SOLVE_MIDDLE;

	if (g_tableFlag)
	{
		//HashClear(g_hash);
		g_hash->attribute = HASH_ATTR_MIDDLE;
	}
#if 0
	else
	{
		FixTableToMiddle(g_hash);
		// ����p�f�[�^�㏑���h�~�̂��ߎ��O�o�^
		g_hash->entry[key].deepest.bk = bk;
		g_hash->entry[key].deepest.wh = wh;
	}
#endif
	// ����p�f�[�^�㏑���h�~�̂��ߎ��O�o�^
	g_key = key;

	// �����[���[���D��T��
	for (int count = 2; count <= limit; count += 2)
	{
		// PV ��������
		memset(g_pvline, -1, sizeof(g_pvline));
		g_pvline_len = 0;
		selectivity = g_max_cut_table_size; // init max threshold
		g_mpc_level = g_max_cut_table_size;

		eval_b = eval;
		g_limitDepth = count;
		eval = PVS_SearchDeep(bk, wh, count, emptyNum, color, g_hash, alpha, beta, NO_PASS, &selectivity, &line);

		if (eval == ABORT)
		{
			break;
		}

		// �ݒ肵�������]���l���Ⴂ���H
		if (eval <= alpha)
		{
			// PV ��������
			memset(g_pvline, -1, sizeof(g_pvline));
			g_pvline_len = 0;
			selectivity = g_max_cut_table_size;
			// �Ⴂ�Ȃ烿�������ɍĐݒ肵�Č���
			eval = PVS_SearchDeep(bk, wh, count, emptyNum, color, g_hash, NEGAMIN, NEGAMAX, NO_PASS, &selectivity, &line);
		}
		// �ݒ肵�������]���l���������H
		else if (eval >= beta)
		{
			// PV ��������
			memset(g_pvline, -1, sizeof(g_pvline));
			g_pvline_len = 0;
			selectivity = g_max_cut_table_size;
			// �����Ȃ��������ɍĐݒ肵�Č���
			eval = PVS_SearchDeep(bk, wh, count, emptyNum, color, g_hash, NEGAMIN, NEGAMAX, NO_PASS, &selectivity, &line);
		}

		// ���̕����}8�ɂ��Č��� (��,��) ---> (eval - 8, eval + 8)
		alpha = eval - (8 * EVAL_ONE_STONE);
		beta = eval + (8 * EVAL_ONE_STONE);

		// UI�Ƀ��b�Z�[�W�𑗐M
		move = GetMoveFromHash(bk, wh, key);
		CreateCpuMessage(g_AiMsg, sizeof(g_AiMsg), eval + (color * 17500), move, count, ON_MIDDLE);
		g_set_message_funcptr[0](g_AiMsg);
	}

	// ���f���ꂽ�̂Œ��߂̊m��]���l��ԋp
	if (eval == ABORT){
		return eval_b;
	}

	return eval;
}

/***************************************************************************
* Name  : SearchExact
* Brief : CPU�̒�������s�T���ɂ���Č��肷��
* Args  : bk        : ���̃r�b�g��
*         wh        : ���̃r�b�g��
*         empty     : �󔒃}�X�̐�
*         cpuConfig : CPU�̐ݒ�
* Return: ����]���l
****************************************************************************/
INT32 SearchExact(UINT64 bk, UINT64 wh, UINT32 emptyNum, UINT32 color)
{
	INT32 alpha = -g_infscore;
	INT32 beta = g_infscore;
	INT32 eval = 0;
	INT32 eval_b = 0;
	UINT32 key = KEY_HASH_MACRO(bk, wh, color);
	INT32 aVal;
	INT32 bVal;
	INT32 move = NOMOVE, move_b;


#if 0
	// ���肪PV�ʂ�̎�𒅎肵���ꍇ��PVLINE�������Q��
	INT32 pv_depth = g_pvline_len - emptyNum;
	if (pv_depth > 0 &&
		g_pvline_board[BLACK][pv_depth] == bk &&
		g_pvline_board[WHITE][pv_depth] == wh)
	{

		if (g_hash->entry[key].deepest.bk == bk && g_hash->entry[key].deepest.wh == wh)
			g_hash->entry[key].deepest.bestmove = g_pvline[pv_depth];
		else g_hash->entry[key].newest.bestmove = g_pvline[pv_depth];

		// UI��PV���C������ʒm
		CreatePVLineStrAscii(&g_pvline[pv_depth], emptyNum, g_evaluation);
		g_set_message_funcptr[1](g_PVLineMsg);

		return g_evaluation;
	}
#endif

#if 0
	g_limitDepth = (INT32)(emptyNum / 1.5);

	if (g_limitDepth % 2) g_limitDepth--;
	if (g_limitDepth > 24)
	{
		g_limitDepth = 24;
	}

	if (g_limitDepth >= 6)
	{
		// ���O���s�T��
		eval = SearchMiddle(bk, wh, emptyNum, color);

		eval /= EVAL_ONE_STONE;

		if (eval % 2)
		{
			if (eval > 0) eval++;
			else eval--;
		}

		CreateCpuMessage(g_AiMsg, sizeof(g_AiMsg), eval, g_hash->entry[key].deepest.bestmove, -2, ON_EXACT);
		g_set_message_funcptr[0](g_AiMsg);

		// �u���\��΍��T���p�ɏ�����
		FixTableToExact(g_hash);
		//HashClear(g_hash);
	}
	else
	{
		HashClear(g_hash);
		aVal = -INF_SCORE;
		bVal = INF_SCORE;
	}

#else
	g_hash->attribute = HASH_ATTR_EXACT;
	aVal = -g_infscore;
	bVal = g_infscore;
#endif

	// PV ��������
	memset(g_pvline, -1, sizeof(g_pvline));
	g_pvline_len = 0;

	// ����p�f�[�^�㏑���h�~�̂��ߎ��O�o�^
	g_key = key;

	PVLINE line;
	INT32 selectivity;

	// �O���[�o���ϐ�������
	g_solveMethod = SOLVE_EXACT;
	g_limitDepth = emptyNum;
	g_empty = emptyNum;
	move_b = NOMOVE;
#if 1

	if (g_empty >= 12)
	{
		// �I��MPC�T��(0.25�Ё`3.0��)
		for (INT32 lv = 0; lv < g_max_cut_table_size + 1; lv++)
		{
			lv = g_max_cut_table_size;
			g_mpc_level = lv;
			// Abort���̂��߂ɒ��O�̕]�����ʂ�ۑ�
			eval_b = eval;
			move_b = move;

			MPC_END_CUT_VAL = cutval_table[g_mpc_level];

			if (lv == g_max_cut_table_size)
			{
				// PVS�΍��T��
				selectivity = lv;
				eval = PVS_SearchDeepExact(bk, wh, emptyNum, color, g_hash, aVal, bVal, NO_PASS, &selectivity, &line);

				// ���f���ꂽ�̂Œ��߂̊m��]���l��ԋp
				if (g_AbortFlag == TRUE)
				{
					g_hash->entry[key].deepest.bestmove = move_b;
					return eval_b;
				}

				if (eval <= aVal)
				{
					eval = PVS_SearchDeepExact(bk, wh, emptyNum, color, g_hash, -g_infscore, eval + 1, NO_PASS, &selectivity, &line);
				}
				else if (eval >= bVal)
				{
					eval = PVS_SearchDeepExact(bk, wh, emptyNum, color, g_hash, eval - 1, g_infscore, NO_PASS, &selectivity, &line);
				}

				// �u���\����őP����擾
				move = GetMoveFromHash(bk, wh, key);
				//printf("[%s] : %+d @ %d%%", g_cordinates_table[move], eval, cutval_table_percent[lv]);
			}
			else
			{
				selectivity = lv;
				eval = PVS_SearchDeepExact(bk, wh, emptyNum, color, g_hash, aVal, bVal, NO_PASS, &selectivity, &line);

				// ���f���ꂽ�̂Œ��߂̊m��]���l��ԋp
				if (g_AbortFlag == TRUE)
				{
					g_hash->entry[key].deepest.bestmove = move_b;
					return eval_b;
				}

				if (eval <= aVal)
				{
					eval--;
				}
				else if (eval >= bVal)
				{
					eval++;
				}

				if (eval % 2)
				{
					if (eval > 0) eval++;
					else eval--;
				}

				aVal = eval - 1;
				bVal = eval + 1;
			}
			
			// �u���\����őP����擾
			move = GetMoveFromHash(bk, wh, key);
			//printf("[%s] : %+d @ %d%%\n", g_cordinates_table[move], eval, cutval_table_percent[lv]);
		}
	}
	else
	{
		// init max threshold
		selectivity = g_max_cut_table_size;
		// PVS�΍��T��
		eval = PVS_SearchDeepExact(bk, wh, emptyNum, color, g_hash, -g_infscore, g_infscore, NO_PASS, &selectivity, &line);
		move = GetMoveFromHash(bk, wh, key);
	}
#else
	selectivity = g_max_cut_table_size;
	g_mpc_level = g_max_cut_table_size;
	eval = PVS_SearchDeepExact(bk, wh, emptyNum, color, g_hash, aVal, bVal, NO_PASS, &selectivity, &line);
	// ���f���ꂽ�̂Œ��߂̊m��]���l��ԋp
	if (eval == ABORT)
	{
		return eval_b;
	}
#endif

	// ���f���ꂽ�̂Œ��߂̊m��]���l��ԋp
	if (g_AbortFlag == TRUE)
	{
		g_hash->entry[key].deepest.bestmove = move_b;
		return eval_b;
	}
	else
	{
		// PVLINE�ǖʂ�ۑ�
		StorePVLineToBoard(bk, wh, color, &line);
		CreatePVLineStr(&line, emptyNum, eval);
		g_move = move;
	}

	return eval;
}

/***************************************************************************
* Name  : SearchWinLoss
* Brief : CPU�̒�������s�T���ɂ���Č��肷��
* Args  : bk        : ���̃r�b�g��
*         wh        : ���̃r�b�g��
*         empty     : �󔒃}�X�̐�
*         cpuConfig : CPU�̐ݒ�
* Return: ����]���l
****************************************************************************/
INT32 SearchWinLoss(UINT64 bk, UINT64 wh, UINT32 emptyNum, UINT32 color)
{
	INT32 eval = 0;
	INT32 eval_b = 0;
	UINT32 key = KEY_HASH_MACRO(bk, wh, color);
	INT32 move = NOMOVE, move_b;

#if 1
	g_limitDepth = emptyNum - 2;

	if (g_limitDepth % 2) g_limitDepth--;
	if (g_limitDepth > 24)
	{
		g_limitDepth = 24;
	}

	if (g_limitDepth >= 12)
	{
		// ���O���s�T��
		eval = SearchMiddle(bk, wh, emptyNum, color);

		eval /= EVAL_ONE_STONE;
		eval -= eval % 2;

		CreateCpuMessage(g_AiMsg, sizeof(g_AiMsg), eval, g_hash->entry[key].deepest.bestmove, -2, ON_WLD);
		g_set_message_funcptr[0](g_AiMsg);

		// �u���\��΍��T���p�ɏ�����
		if (g_tableFlag) FixTableToWinLoss(g_hash);
	}
	else
	{
		if (g_hash->attribute != HASH_ATTR_WLD)
		{
			HashClear(g_hash);
			g_hash->attribute = HASH_ATTR_WLD;
		}
		// ����p�f�[�^�㏑���h�~�̂��ߎ��O�o�^
		g_key = key;
	}

#else
	HashClear(g_hash);
#endif

	PVLINE line;
	INT32 selectivity;
	char *wld_str[] = {"LOSS", "DRAW", "WIN"};

	// �O���[�o���ϐ�������
	g_solveMethod = SOLVE_WLD;
	g_limitDepth = emptyNum;
	g_empty = emptyNum;

	// PV���C���\������
	g_set_message_funcptr[1]("");

	// �I��MPC�T��(0.25�Ё`3.0��)
	for (INT32 lv = 0; lv < g_max_cut_table_size + 1; lv++)
	{
		selectivity = lv;
		g_mpc_level = lv;
		// Abort���̂��߂ɒ��O�̕]�����ʂ�ۑ�
		eval_b = eval;
		move_b = move;

		MPC_END_CUT_VAL = cutval_table[g_mpc_level];

		// PVS�΍��T��
		eval = PVS_SearchDeepWinLoss(bk, wh, emptyNum, color, g_hash, LOSS, WIN, NO_PASS, &selectivity, &line);

		// ���f���ꂽ�̂Œ��߂̊m��]���l��ԋp
		if (g_AbortFlag == TRUE)
		{
			sprintf_s(g_AiMsg, sizeof(g_AiMsg), "Aborted Solver.");
			g_set_message_funcptr[0](g_AiMsg);
			g_hash->entry[key].deepest.bestmove = move_b;
			return eval_b;
		}

		// �u���\����őP����擾
		move = GetMoveFromHash(bk, wh, key);

		sprintf_s(g_AiMsg, sizeof(g_AiMsg), "[%s] : %s @ %d%%", g_cordinates_table[move], wld_str[eval + 1], cutval_table_percent[lv]);
		g_set_message_funcptr[2](g_AiMsg);

		CreateCpuMessage(g_AiMsg, sizeof(g_AiMsg), eval, move, -1, ON_WLD);
		g_set_message_funcptr[0](g_AiMsg);
	}

	// ���f���ꂽ�̂Œ��߂̊m��]���l��ԋp
	if (g_AbortFlag == TRUE)
	{
		sprintf_s(g_AiMsg, sizeof(g_AiMsg), "Aborted Solver.");
		g_set_message_funcptr[0](g_AiMsg);
		return eval_b;
	}
	else
	{
		sprintf_s(g_AiMsg, sizeof(g_AiMsg), "Solved.");
		g_set_message_funcptr[0](g_AiMsg);
	}

	return eval;
}



/***************************************************************************
* Name  : PVS_SearchDeep
* Brief : PV Search ���s���A�]���l����ɍőP����擾
* Args  : bk        : ���̃r�b�g��
*         wh        : ���̃r�b�g��
*         depth     : �ǂސ[��
*         empty     : �󂫃}�X��
*         alpha     : ���̃m�[�h�ɂ����鉺���l
*         beta      : ���̃m�[�h�ɂ��������l
*         color     : CPU�̐F
*         hash      : �u���\�̐擪�|�C���^
*         pass_cnt  : ���܂ł̃p�X�̐�(�Q�J�E���g�ŏI���Ƃ݂Ȃ�)
* Return: ����\�ʒu�̃r�b�g��
****************************************************************************/
INT32 PVS_SearchDeep(UINT64 bk, UINT64 wh, INT32 depth, INT32 empty, UINT32 color,
	HashTable *hash, INT32 alpha, INT32 beta, UINT32 passed, INT32 *p_selectivity, PVLINE *pline)
{

	/* �A�{�[�g���� */
	if (g_AbortFlag == TRUE)
	{
		return ABORT;
	}

	if (depth == 0)
	{
		pline->cmove = 0;
		/* �t�m�[�h(�ǂ݂̌��E�l�̃m�[�h)�̏ꍇ�͕]���l���Z�o */
		InitIndexBoard(bk, wh);
		return Evaluation(g_board, bk, wh, color, 59 - empty);
	}

	if (g_limitDepth >= DEPTH_DEEP_TO_SHALLOW_SEARCH && depth < DEPTH_DEEP_TO_SHALLOW_SEARCH)
	{
		return AB_Search(bk, wh, depth, empty, color, alpha, beta, passed, pline);
	}

	g_countNode++;

	INT32 score, bestscore, lower, upper, bestmove;
	UINT32 key;
	MoveList movelist[36], *iter;
	Move *move;
	HashInfo *hashInfo;

	bestmove = NOMOVE;
	lower = alpha;
	upper = beta;

	// stability cutoff
	if (depth != g_limitDepth && search_SC_PVS(bk, wh, empty, &alpha, &beta, &score)) return score;

	/************************************************************
	*
	* �u���\�J�b�g�I�t�t�F�[�Y
	*
	*************************************************************/

	/* transposition cutoff ? */
	key = KEY_HASH_MACRO(bk, wh, color);
	if (g_tableFlag){
		/* �L�[�𐶐� */
		hashInfo = HashGet(hash, key, bk, wh);
		if (hashInfo != NULL)
		{
			if (hashInfo->depth >= depth &&
				hashInfo->selectivity >= *p_selectivity)
			{
				int hash_upper;
				hash_upper = hashInfo->upper;
				if (hash_upper <= lower)
				{
					// transposition table cutoff
					*p_selectivity = hashInfo->selectivity;
					pline->cmove = 0;
					return hash_upper;
				}
				int hash_lower;
				hash_lower = hashInfo->lower;
				if (hash_lower >= upper)
				{
					// transposition table cutoff
					*p_selectivity = hashInfo->selectivity;
					pline->cmove = 0;
					return hash_lower;
				}
				if (hash_lower == hash_upper)
				{
					// transposition table cutoff
					*p_selectivity = hashInfo->selectivity;
					pline->cmove = 0;
					return hash_lower;
				}

				// change window width
				lower = max(lower, hash_lower);
				upper = min(upper, hash_upper);
			}
			bestmove = hashInfo->bestmove;
		}
	}

	/************************************************************
	*
	* Multi-Prob-Cut(MPC) �t�F�[�Y
	*
	*************************************************************/
#if 1
	if (g_mpcFlag && depth >= MPC_MIN_DEPTH && depth <= 24)
	{
		INT32 mpc_level;
		double MPC_CUT_VAL;
		if (empty <= 24)
		{
			MPC_CUT_VAL = cutval_table[5];
			mpc_level = 5;
		}
		else if (empty <= 36)
		{
			MPC_CUT_VAL = cutval_table[4];
			mpc_level = 4;
		}
		else
		{
			MPC_CUT_VAL = cutval_table[3];
			mpc_level = 3;
		}

		MPCINFO *mpcInfo_p = &mpcInfo[depth - MPC_MIN_DEPTH];
		INT32 value = (INT32)(alpha - (mpcInfo_p->deviation * MPC_CUT_VAL) - mpcInfo_p->offset);
		if (value < NEGAMIN + 1) value = NEGAMIN + 1;
		score = AB_Search(bk, wh, mpcInfo_p->depth, empty, color, value - 1, value, passed,  pline);
		if (score < value)
		{
			//HashUpdate(hash, key, bk, wh, alpha, beta, alpha, mpcInfo_p->depth, NOMOVE, mpc_level, NEGAMAX);
			*p_selectivity = g_max_cut_table_size; // store selectivity
			return alpha;
		}

		value = (INT32)(beta + (mpcInfo_p->deviation * MPC_CUT_VAL) - mpcInfo_p->offset);
		if (value > NEGAMAX - 1) value = NEGAMAX - 1;
		score = AB_Search(bk, wh, mpcInfo_p->depth, empty, color, value, value + 1, passed, pline);
		if (score > value)
		{
			//HashUpdate(hash, key, bk, wh, alpha, beta, beta, mpcInfo_p->depth, NOMOVE, mpc_level, NEGAMAX);
			*p_selectivity = g_max_cut_table_size; // store selectivity
			return beta;
		}
	}
#endif

	UINT32 moveCount;
	UINT64 moves = CreateMoves(bk, wh, &moveCount);
	BOOL pv_flag = TRUE;
	INT32 selectivity;
	INT32 max_selectivity = 0;

	// �����flip-bit�����߂�move�\���̂ɕۑ�
	StoreMovelist(movelist, bk, wh, moves);

	if (movelist->next == NULL) {
		if (passed) {
			bestscore = CountBit(bk) - CountBit(wh);

			if (bestscore > 0)
			{
				bestscore += 1280000;
			}
			else if (bestscore < 0)
			{
				bestscore -= 1280000;
			}

			bestmove = NOMOVE;
			pline->cmove = 0;
		}
		else {
			bestscore = -PVS_SearchDeep(wh, bk, depth, empty, color ^ 1, hash, -upper, -lower, 1, p_selectivity, pline);
			max_selectivity = *p_selectivity;
			bestmove = PASS;
		}

		return bestscore;
	}
	else
	{
		PVLINE line;
		if (moveCount > 1)
		{
			if (empty > 15)
			{
				SortMoveListMiddle(movelist, bk, wh, hash, empty, alpha, beta, color);
			}
			else
			{
				// ��̕��בւ�
				SortMoveListEnd(movelist, bk, wh, hash, empty, alpha, beta, color);
			}
		}

		/* �u���\�ŎQ�Əo�����肩���ɒ��肷�邽�߂Ƀ\�[�g */
		if (bestmove != NOMOVE && bestmove != movelist->next->move.pos)
		{
			SortMoveListTableMoveFirst(movelist, bestmove);
		}

		bestscore = NEGAMIN;
		/* other moves : try to refute the first/best one */
		for (iter = movelist->next; lower < upper && iter != NULL; iter = iter->next)
		{
			selectivity = g_max_cut_table_size; // init max thresould
			move = &(iter->move);
			// PV�\���L���p
			g_pvline[g_empty - empty] = move->pos;

			if (pv_flag)
			{
				score = -PVS_SearchDeep(wh ^ move->rev, bk ^ ((1ULL << move->pos) | move->rev),
					depth - 1, empty - 1, color ^ 1, hash, -upper, -lower, 0, &selectivity, &line);
				pv_flag = FALSE;
			}
			else
			{
				score = -PVS_SearchDeep(wh^move->rev, bk ^ ((1ULL << move->pos) | move->rev),
					depth - 1, empty - 1, color ^ 1, hash, -lower - 1, -lower, 0, &selectivity, &line);
				if (score > lower && score < upper)
				{
					selectivity = g_max_cut_table_size; // init max thresould
					lower = score;
					score = -PVS_SearchDeep(wh ^ move->rev, bk ^ ((1ULL << move->pos) | move->rev),
						depth - 1, empty - 1, color ^ 1, hash, -upper, -lower, 0, &selectivity, &line);
				}
			}

			if (score >= upper)
			{
				bestscore = score;
				break;
			}

			if (score > bestscore) {
				bestscore = score;
				bestmove = move->pos;
				if (bestscore > lower)
				{
					lower = bestscore;
					if (line.cmove < 0 || line.cmove > 59)
					{
						line.cmove = 0;
					}
					pline->argmove[0] = bestmove;
					memcpy(pline->argmove + 1, line.argmove, line.cmove);
					pline->cmove = line.cmove + 1;
#if 0
					if (g_empty - empty == 0)
					{
						CreatePVLineStr(pline, empty, (bestscore + color * 17500) * (1 - (2 * ((g_empty - empty) % 2))));
						g_set_message_funcptr[1](g_PVLineMsg);
					}
#endif
				}
			}
		}
	}

	/* �u���\�ɓo�^ */
	if (g_empty == empty)
	{
		g_move = bestmove;
	}

	HashUpdate(hash, key, bk, wh, alpha, beta, bestscore, depth, bestmove, g_max_cut_table_size, NEGAMAX);
	*p_selectivity = g_max_cut_table_size; // store selectivity

	return bestscore;

}

INT32 AB_Search(UINT64 bk, UINT64 wh, INT32 depth, INT32 empty, UINT32 color,
	INT32 alpha, INT32 beta, UINT32 passed, PVLINE *pline)
{

	/* �A�{�[�g���� */
	if (g_AbortFlag)
	{
		return ABORT;
	}

	g_countNode++;

	if (depth == 0)
	{
		pline->cmove = 0;
		/* �t�m�[�h(�ǂ݂̌��E�l�̃m�[�h)�̏ꍇ�͕]���l���Z�o */
		InitIndexBoard(bk, wh);
		return Evaluation(g_board, bk, wh, color, 59 - empty);
	}

	int eval;
	// stability cutoff
	if (search_SC_PVS(bk, wh, empty, &alpha, &beta, &eval)) return eval;

	/************************************************************
	*
	* Multi-Prob-Cut(MPC) �t�F�[�Y
	*
	*************************************************************/
#if 1
	if (g_mpcFlag && depth >= MPC_MIN_DEPTH && depth <= 24)
	{
		double MPC_CUT_VAL;
		if (empty <= 24)
		{
			MPC_CUT_VAL = cutval_table[5];
		}
		else if (empty <= 36)
		{
			MPC_CUT_VAL = cutval_table[4];
		}
		else
		{
			MPC_CUT_VAL = cutval_table[3];
		}


		MPCINFO *mpcInfo_p = &mpcInfo[depth - MPC_MIN_DEPTH];
		INT32 value = (INT32)(alpha - (mpcInfo_p->deviation * MPC_CUT_VAL) - mpcInfo_p->offset);
		if (value < NEGAMIN + 1) value = NEGAMIN + 1;
		eval = AB_Search(bk, wh, mpcInfo_p->depth, empty, color, value - 1, value, passed, pline);
		if (eval < value)
		{
			return alpha;
		}

		value = (INT32)(beta + (mpcInfo_p->deviation * MPC_CUT_VAL) - mpcInfo_p->offset);
		if (value > NEGAMAX - 1) value = NEGAMAX - 1;
		eval = AB_Search(bk, wh, mpcInfo_p->depth, empty, color, value, value + 1, passed, pline);
		if (eval > value)
		{
			return beta;
		}
	}
#endif

	int move_cnt;
	int max;                    //���݂̍ō��]���l
	UINT64 rev;
	UINT64 moves;             //���@��̃��X�g�A�b�v

	/* ���@�萶���ƃp�X�̏��� */
	if ((moves = CreateMoves(bk, wh, (UINT32 *)(&move_cnt))) == 0)
	{
		if (passed)
		{
			max = CountBit(bk) - CountBit(wh);
			if (max > 0)
			{
				max += 1280000;
			}
			else if (max < 0)
			{
				max -= 1280000;
			}

			pline->cmove = 0;
			return max;
		}
		max = -AB_Search(wh, bk, depth, empty, color ^ 1, -beta, -alpha, 1, pline);
	}
	else
	{
		int pos;
		PVLINE line;

		max = NEGAMIN;
		do
		{
			/* �ÓI�����Â��i���Ȃ��R�X�g�ő啝�ɍ���������݂����j */
			pos = GetOrderPosition(moves);
			rev = GetRev[pos](bk, wh);
			// PV�\���L���p
			g_pvline[g_empty - empty] = pos;

			/* �^�[����i�߂čċA������ */
			eval = -AB_Search(wh ^ rev, bk ^ ((1ULL << pos) | rev),
				depth - 1, empty - 1, color ^ 1, -beta, -alpha, 0, &line);

			if (beta <= eval)
			{
				return eval;
			}

			/* ���܂ł��ǂ��ǖʂ�������΍őP��̍X�V */
			if (eval > max)
			{
				max = eval;
				if (max > alpha)
				{
					alpha = max;
					if (line.cmove < 0 || line.cmove > 59)
					{
						line.cmove = 0;
					}
					pline->argmove[0] = pos;
					memcpy(pline->argmove + 1, line.argmove, line.cmove);
					pline->cmove = line.cmove + 1;
				}
			}

			moves ^= 1ULL << pos;

		} while (moves);
	}

	return max;

}

INT32 AB_SearchNoPV(UINT64 bk, UINT64 wh, INT32 depth, INT32 empty, UINT32 color,
	INT32 alpha, INT32 beta, UINT32 passed)
{

	/* �A�{�[�g���� */
	if (g_AbortFlag)
	{
		return ABORT;
	}

	g_countNode++;

	if (depth == 0)
	{
		/* �t�m�[�h(�ǂ݂̌��E�l�̃m�[�h)�̏ꍇ�͕]���l���Z�o */
		InitIndexBoard(bk, wh);
		return Evaluation(g_board, bk, wh, color, 59 - empty);
	}

	int eval;
	// stability cutoff
	if (search_SC_PVS(bk, wh, empty, &alpha, &beta, &eval)) return eval;

	/************************************************************
	*
	* Multi-Prob-Cut(MPC) �t�F�[�Y
	*
	*************************************************************/
#if 1
	if (g_mpcFlag && depth >= MPC_MIN_DEPTH && depth <= 24)
	{
		double MPC_CUT_VAL;
		if (empty <= 24)
		{
			MPC_CUT_VAL = cutval_table[5];
		}
		else if (empty <= 36)
		{
			MPC_CUT_VAL = cutval_table[4];
		}
		else
		{
			MPC_CUT_VAL = cutval_table[3];
		}

		MPCINFO *mpcInfo_p = &mpcInfo[depth - MPC_MIN_DEPTH];
		INT32 value = (INT32)(alpha - (mpcInfo_p->deviation * MPC_CUT_VAL) - mpcInfo_p->offset);
		if (value < NEGAMIN + 1) value = NEGAMIN + 1;
		eval = AB_SearchNoPV(bk, wh, mpcInfo_p->depth, empty, color, value - 1, value, passed);
		if (eval < value)
		{
			return alpha;
		}

		value = (INT32)(beta + (mpcInfo_p->deviation * MPC_CUT_VAL) - mpcInfo_p->offset);
		if (value > NEGAMAX - 1) value = NEGAMAX - 1;
		eval = AB_SearchNoPV(bk, wh, mpcInfo_p->depth, empty, color, value, value + 1, passed);
		if (eval > value)
		{
			return beta;
		}
	}
#endif


	int move_cnt;
	int max;                    //���݂̍ō��]���l
	UINT64 rev;
	UINT64 moves;             //���@��̃��X�g�A�b�v

	/* ���@�萶���ƃp�X�̏��� */
	if ((moves = CreateMoves(bk, wh, (UINT32 *)(&move_cnt))) == 0)
	{
		if (passed)
		{
			max = CountBit(bk) - CountBit(wh);
			if (max > 0)
			{
				max += 1280000;
			}
			else if (max < 0)
			{
				max -= 1280000;
			}
			else
			{
				max = 1280000;
			}

			return max;
		}
		max = -AB_SearchNoPV(wh, bk, depth, empty, color ^ 1, -beta, -alpha, 1);
	}
	else
	{
		int pos;
		max = NEGAMIN;
		do
		{
			/* �ÓI�����Â��i���Ȃ��R�X�g�ő啝�ɍ���������݂����j */
			pos = GetOrderPosition(moves);
			rev = GetRev[pos](bk, wh);
			/* �^�[����i�߂čċA������ */
			eval = -AB_SearchNoPV(wh ^ rev, bk ^ ((1ULL << pos) | rev),
				depth - 1, empty - 1, color ^ 1, -beta, -alpha, 0);

			if (beta <= eval)
			{
				return eval;
			}

			/* ���܂ł��ǂ��ǖʂ�������΍őP��̍X�V */
			if (eval > max)
			{
				max = eval;
				alpha = max(alpha, eval);
			}

			moves ^= 1ULL << pos;

		} while (moves);
	}

	return max;

}