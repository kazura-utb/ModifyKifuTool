/***************************************************************************
* Name  : search.h
* Brief : �T���̏����S�ʂ��s��
* Date  : 2016/02/01
****************************************************************************/

#include "stdafx.h"

#pragma once

#include "hash.h"

//#define LOSSGAME

#define KEY_HASH_MACRO(b, w, c) (UINT32)((b ^ ((w) >> 1ULL)) % (g_casheSize - 1))
//#define KEY_HASH_MACRO(b, w, c) GenerateHashValue(b, w, c);

#define ILLIGAL_ARGUMENT 0x80000001
#define MOVE_PASS 0x0

#define ON_MIDDLE 0
#define ON_WLD 1
#define ON_EXACT 2

#define SOLVE_WLD   0
#define SOLVE_EXACT 1
#define SOLVE_MIDDLE 2

#define EMPTIES_MID_ORDER_TO_END_ORDER 12
#define EMPTIES_DEEP_TO_SHALLOW_SEARCH 10
#define END_DEPTH_DEEP_TO_SHALLOW_SEARCH 19
#define DEPTH_DEEP_TO_SHALLOW_SEARCH 5

#define NO_PASS 0
#define ABORT 0x80000000

#define MPC_MIN_DEPTH 3
#define MPC_END_MIN_DEPTH 6
#define MPC_END_MAX_DEPTH 30

typedef struct
{
	UINT32 color;				// CPU�̐F
	UINT32 casheSize;			// �u���\�̃T�C�Y
	UINT32 searchDepth;			// ���Փǂ݂̐[��
	UINT32 winLossDepth;		// ���s�T�����J�n����[��
	UINT32 exactDepth;			// �΍��T�����J�n����[��
	UINT32 bookVariability;	    // ��΂̕ω��x
	UINT8  bookFlag;			// ��΂��g�p���邩�ǂ���
	UINT8  mpcFlag;				// MPC���g�p���邩�ǂ���
	UINT8  tableFlag;			// �u���\���g�p���邩�ǂ���

}CPUCONFIG;


typedef void(__stdcall *SetMessageToGUI)(char *);

/* MPC */
typedef struct
{
	int depth;
	int offset;
	int deviation;
}MPCINFO;


typedef struct PVLINE {
	int   cmove;          // Number of moves in the line.
	char argmove[60];  // The line.
} PVLINE;

extern INT32 g_pvline[60];
extern INT32 g_pvline_len;

extern int g_solveMethod;
extern BOOL g_tableFlag;
extern char g_cordinates_table[64][4];
extern INT32 g_limitDepth;
extern INT32 g_empty;
extern INT32 g_move;
extern UINT64 g_casheSize;
extern INT32 g_infscore;
extern MPCINFO mpcInfo[22];
extern MPCINFO mpcInfo_end[26];
extern double MPC_END_CUT_VAL;
extern INT32 g_mpc_level;
const extern double cutval_table[8];
extern const INT32 g_max_cut_table_size;
extern UINT64 g_countNode;
extern HashTable *g_hash;
extern INT32 g_key;

extern BOOL g_mpcFlag;
extern BOOL g_tableFlag;
// CPU AI���
extern BOOL g_AbortFlag;
extern UINT64 g_countNode;
extern char g_AiMsg[128];
extern char g_PVLineMsg[256];
extern SetMessageToGUI g_set_message_funcptr[3];

/***************************************************************************
* Name  : GetMoveFromAI
* Brief : CPU�̒����T���ɂ���Č��肷��
* Args  : bk        : ���̃r�b�g��
*         wh        : ���̃r�b�g��
*         empty     : �󔒃}�X�̐�
*         cpuConfig : CPU�̐ݒ�
* Return: ����\�ʒu�̃r�b�g��
****************************************************************************/
UINT64 GetMoveFromAI(UINT64 bk, UINT64 wh, UINT32 emptyNum, CPUCONFIG *cpuConfig);

/***************************************************************************
* Name  : AB_SearchNoPV
* Brief : �P���ȃ����T���ɂ���Ď�����肷��
* Args  : bk        : ���̃r�b�g��
*         wh        : ���̃r�b�g��
*         depth     : �T�����Ă���m�[�h�̐[��
*         empty     : �󔒃}�X�̐�
*         color     : �T�����Ă���m�[�h�̐F
*         alpha     : 
*         beta      :
*         passed    : �p�X�������̃t���O
* Return: �]���l
****************************************************************************/
INT32 AB_SearchNoPV(UINT64 bk, UINT64 wh, INT32 depth, INT32 empty, UINT32 color,
	INT32 alpha, INT32 beta, UINT32 passed);

/***************************************************************************
* Name  : AlphaBetaSearch
* Brief : MPC����̒P���ȃ����T���ɂ���Ď�����肷��
* Args  : bk        : ���̃r�b�g��
*         wh        : ���̃r�b�g��
*         depth     : �T�����Ă���m�[�h�̐[��
*         empty     : �󔒃}�X�̐�
*         color     : �T�����Ă���m�[�h�̐F
*         alpha     :
*         beta      :
*         passed    : �p�X�������̃t���O
* Return: �]���l
****************************************************************************/
INT32 AB_Search(UINT64 bk, UINT64 wh, INT32 depth, INT32 empty, UINT32 color,
	INT32 alpha, INT32 beta, UINT32 passed, PVLINE *pline);

/***************************************************************************
* Name  : SetAbortFlag
* Brief : CPU�̏����𒆒f����
****************************************************************************/
void SetAbortFlag();


void CreatePVLineStr(PVLINE *pline, INT32 empty, INT32 score);
INT32 SearchExact(UINT64 bk, UINT64 wh, UINT32 emptyNum, UINT32 color);