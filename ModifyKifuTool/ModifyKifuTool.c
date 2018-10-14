// ModifyKifuTool.cpp : このファイルには 'main' 関数が含まれています。プログラム実行の開始と終了がそこで行われます。
//
#include "stdafx.h"

#include "bit64.h"
#include "board.h"
#include "rev.h"
#include "hash.h"
#include "eval.h"
#include "move.h"
#include "cpu.h"
#include "mt.h"

#define EXACT_START_EMPTY  22

#define FIRST_BK 34628173824
#define FIRST_WH 68853694464

extern HashTable *g_hash;
extern UINT64     g_casheSize;


void strchg(char *buf, size_t bufSize, const char *str1, const char *str2)
{
	char tmp[256 + 1];
	char *p;

	while ((p = strstr(buf, str1)) != NULL)
	{
        *p = '\0';
        p += strlen(str1);
		strcpy_s(tmp, sizeof(tmp), p);
		strcat_s(buf, bufSize, str2);
		strcat_s(buf, bufSize, tmp);
	}
}


UINT32 InitializeAI()
{
	AlocMobilityFunc();

	BOOL result = LoadData();

	if (result == FALSE)
	{
		printf("ERROR:評価テーブルの読み込みに失敗しました\r\n");
		return 1;
	}

	/* pos番号-->指し手文字列変換テーブル */
	char cordinate[6];
	/* 指し手の座標表記変換用 */
	for (int i = 0; i < 64; i++)
	{
		sprintf_s(cordinate, 4, "%c%d", (i / 8) + 'A', (i % 8) + 1);
		strcpy_s(g_cordinates_table[i], sizeof(g_cordinates_table[i]), cordinate);
	}

	edge_stability_init();

	init_genrand((unsigned long)time(NULL));

	g_hash = HashNew(4194304);
	g_casheSize = 4194304;

	g_mpcFlag = 1;
	g_tableFlag = 1;
	g_infscore = INF_SCORE;

	return 0;
}

static char *ReadKifuLine(FILE *rfp, char *kifuDataLine, size_t kifuDataLineSize)
{
	char *ret;

	ret = fgets(kifuDataLine, (int)kifuDataLineSize, rfp);
	if (ret == NULL) return ret;

	strchg(kifuDataLine, kifuDataLineSize, " ", "");
	kifuDataLine[(60 - EXACT_START_EMPTY) * 2] = '\0';

	return kifuDataLine;
}



UINT32 MoveToSpecifiedEmpty(UINT64 *p_bk, UINT64 *p_wh, char *kifuDataLine, INT32 empty, UINT32 *color)
{
	UINT32 cnt;
	UINT64 bk, wh;
	UINT64 rev;
	UINT32 move;
	INT32  turn;
	INT32  l_empty;
	UINT8  l_color;
	UINT8  badFlag;

	badFlag = 0;
	bk = *p_bk;
	wh = *p_wh;
	l_color = BLACK;
	cnt = 0;
	turn = 0;

	while (1)
	{
		while (kifuDataLine[cnt] == ' ') {
			cnt++;
		}

		/* 英字を読み込む */
		/* オシマイ */
		if (!isalpha(kifuDataLine[cnt]))
		{
			break;
		}
		/* 数字を読み込む */
		/* なぜかおかしな棋譜データが結構ある */
		if (!isdigit(kifuDataLine[cnt + 1]))
		{
			badFlag = 1;
			break;
		}

		move = ((kifuDataLine[cnt] - 'A') * 8) + (kifuDataLine[cnt + 1] - '1');
		/* a9 とか明らかに間違った手を含んでいる棋譜がある */
		if (move < 0 || move >= 64)
		{
			badFlag = 1;
			break;
		}
		/* なぜかすでに置いたマスに置いている棋譜がある */
		if (bk & (1ULL << move) || wh & (1ULL << move))
		{
			badFlag = 1;
			break;
		}

		if (l_color == BLACK)
		{
			rev = GetRev[move](bk, wh);
			/* 黒パス？ */
			if (rev == 0)
			{
				rev = GetRev[move](wh, bk);
				/* 一応合法手になっているかのチェック */
				if (rev == 0)
				{
					badFlag = 1;
					break;
				}
				bk ^= rev;
				wh ^= ((1ULL << move) | rev);
			}
			else
			{
				bk ^= ((1ULL << move) | rev);
				wh ^= rev;
				l_color ^= 1;
			}
		}
		else
		{
			rev = GetRev[move](wh, bk);
			/* 白パス？ */
			if (rev == 0)
			{
				rev = GetRev[move](bk, wh);
				/* 一応合法手になっているかのチェック */
				if (rev == 0)
				{
					badFlag = 1;
					break;
				}
				bk ^= ((1ULL << move) | rev);
				wh ^= rev;
			}
			else
			{
				bk ^= rev;
				wh ^= ((1ULL << move) | rev);
				l_color ^= 1;
			}
		}

		cnt += 2;
		turn++;

		if (turn == 60 - empty)
		{
			break;
		}
	}

	l_empty = CountBit(~(bk | wh));
	if (l_empty != empty || badFlag == 1) return 1;

	*color = l_color;
	*p_bk = bk;
	*p_wh = wh;

	return 0;
}



INT32 CheckPVLine(UINT64 bk, UINT64 wh, INT32 empty, UINT32 color, INT32 *pv, INT32 *pvSize, INT32 *score)
{
	UINT32 temp;
	INT32  eval;
	UINT32 cnt;
	UINT64 rev;
	INT32  turn;
	INT32  l_eval;
	UINT8  badFlag;

	badFlag = 0;
	turn = 0;
	cnt = 0;
	eval = g_infscore;
	l_eval = g_infscore;

	HashClear(g_hash);
	while(turn < EXACT_START_EMPTY)
	{
		CreateMoves(bk, wh, &temp);
		if (temp == 0)
		{
			CreateMoves(wh, bk, &temp);
			if (temp == 0) break; // 空きマスがあるが互いに打てない
			else
			{
				color ^= 1;
				Swap(&bk, &wh);
			}
		}

		eval = SearchExact(bk, wh, empty--, color);
		if (l_eval == g_infscore)
		{
			if(color == BLACK) l_eval = eval;
			else l_eval = -eval;
		}
		if (eval == ABORT)
		{
			printf("Aborted.\n");
			badFlag = 1;
			break;
		}

		if (bk & (1ULL << g_move) || wh & (1ULL << g_move))
		{
			printf("Illigal move..\n");
			badFlag = 1;
			break;
		}
		pv[turn] = g_move;
		printf("%s", g_cordinates_table[g_move]);

		rev = GetRev[g_move](bk, wh);
		bk ^= ((1ULL << g_move) | rev);
		wh ^= rev;
		color ^= 1;
		cnt += 2;
		turn++;

		Swap(&bk, &wh);
	}

	if (badFlag == 1) return 1;

	*score = l_eval;
	printf(" : %+3d\n", *score);
	*pvSize = turn;

	return 0;
}


INT32 GeneratePVLine(UINT64 bk, UINT64 wh, INT32 empty, UINT32 color, INT32 *pv, INT32 *pvSize, INT32 *score)
{
	UINT32 ret;

	if (color == WHITE) Swap(&bk, &wh);

	ret = CheckPVLine(bk, wh, empty, color, pv, pvSize, score);

	return ret;
}



INT32 CheckLineData(char *kifuDataLine, INT32 score)
{
	INT32  cnt;
	UINT64 bk, wh;
	UINT64 rev;
	UINT32 color;
	UINT32 move;
	INT32  turn;
	UINT32 empty;
	UINT8  badFlag;
	size_t kifuDataLineSize;

	bk = FIRST_BK;
	wh = FIRST_WH;
	color = BLACK;
	badFlag = 0;
	turn = 0;
	cnt = 0;

	kifuDataLineSize = strlen(kifuDataLine);
	while (cnt < kifuDataLineSize)
	{
		move = ((kifuDataLine[cnt] - 'A') * 8) + (kifuDataLine[cnt + 1] - '1');

		if (bk & (1ULL << move) || wh & (1ULL << move))
		{
			badFlag = 1;
			break;
		}

		if (color == BLACK)
		{
			rev = GetRev[move](bk, wh);
			/* 黒パス？ */
			if (rev == 0)
			{
				rev = GetRev[move](wh, bk);
				/* 一応合法手になっているかのチェック */
				if (rev == 0)
				{
					badFlag = 1;
					break;
				}
				bk ^= rev;
				wh ^= ((1ULL << move) | rev);
			}
			else
			{
				bk ^= ((1ULL << move) | rev);
				wh ^= rev;
				color ^= 1;
			}
		}
		else
		{
			rev = GetRev[move](wh, bk);
			/* 白パス？ */
			if (rev == 0)
			{
				rev = GetRev[move](bk, wh);
				/* 一応合法手になっているかのチェック */
				if (rev == 0)
				{
					badFlag = 1;
					break;
				}
				bk ^= ((1ULL << move) | rev);
				wh ^= rev;
			}
			else
			{
				bk ^= rev;
				wh ^= ((1ULL << move) | rev);
				color ^= 1;
			}
		}

		cnt += 2;
		turn++;
	}

	if (badFlag == 1) return 1;

	empty = 60 - turn;

	turn = GetExactScore(bk, wh, empty);
	if (score != turn)
	{
		printf("Score mismatch..(score = %d, score2 = %d)\n", score, turn);
		return 1;
	}

	return 0;

}



INT32 WriteModifiedKifuData(FILE *wfp, char *kifuDataLine, INT32 *pvline, INT32 pvlineSize, INT32 score)
{
	INT32   ret;
	char    pvStr[256];
	size_t  size;
	UINT32  cnt;

	size = sizeof(pvStr);
	strcpy_s(pvStr, size, kifuDataLine);
	cnt = (UINT32 )strlen(pvStr);
	size -= cnt;

	for (int i = 0; i < pvlineSize; i++)
	{
		sprintf_s(&pvStr[cnt], size, "%s", g_cordinates_table[pvline[i]]);
		cnt += 2;
		size -= 2;
	}

	// 最終チェック
	ret = CheckLineData(pvStr, score);
	if (ret != 0) return ret;

	fprintf(wfp, "%s %d\n", pvStr, score);

	return ret;
}



int main()
{
	UINT32  ret;
	UINT32  count;
	char    rfilename[64];
	char    wfilename[64];
	UINT64  bk, wh;
	UINT32  color;
	char    kifuDataLine[256];
	INT32   pvline[EXACT_START_EMPTY];
	INT32   pvlineSize;
	INT32   score;
	FILE   *rfp;
	FILE   *wfp;

	ret = InitializeAI();
	if (ret != 0)
	{
		printf("Failed InitializeAI()\n");
		return 0;
	}

	for(int i = 0; i < 37; i++)
	{
		// 棋譜ファイル名生成
		sprintf_s(rfilename, sizeof(rfilename), "D:\\Repos\\learningTools\\learningTools\\kifu\\%02dE4.gam.%d.new", i / 2 + 1, (i % 2) + 1);
		// 訂正棋譜ファイル名生成
		sprintf_s(wfilename, sizeof(wfilename), "kifu\\%02dE4.gam.%d.kif", i / 2 + 1, (i % 2) + 1);

		if (fopen_s(&rfp, rfilename, "r") != 0)
		{
			break;
		}
		if (fopen_s(&wfp, wfilename, "w") != 0)
		{
			break;
		}

		count = 0;
		// 棋譜データの読み込み
		while(ReadKifuLine(rfp, kifuDataLine, sizeof(kifuDataLine)) != NULL)
		{
			bk = FIRST_BK;
			wh = FIRST_WH;

			printf("%5d : ", count++);

			// 空きマスが２２つになるまで手を進める
			// エラーが出たら無視して次のラインへ
			ret = MoveToSpecifiedEmpty(&bk, &wh, kifuDataLine, EXACT_START_EMPTY, &color);
			if (ret != 0)
			{
				printf("Failed MoveToSpecifiedEmpty()\n");
				continue;
			}

			ret = GeneratePVLine(bk, wh, EXACT_START_EMPTY, color, pvline, &pvlineSize, &score);
			if (ret != 0)
			{
				printf("Failed GeneratePVLine()\n");
				continue;
			}

			ret = WriteModifiedKifuData(wfp, kifuDataLine, pvline, pvlineSize, score);
			if (ret != 0)
			{
				printf("Failed WriteModifiedKifuData()\n");
				continue;
			}
		}
	}

	return 0;

}
