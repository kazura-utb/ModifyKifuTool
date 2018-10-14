/***************************************************************************
* Name  : bit64.h
* Brief : �r�b�g���Z�֘A�̏������s��
* Date  : 2016/02/01
****************************************************************************/

#include "stdafx.h"

#pragma once

extern const unsigned long long X_TO_BIT[];

#define x_to_bit(x) X_TO_BIT[x]

/* ����\���v�Z�p */
typedef struct {
	unsigned long high;
	unsigned long low;
} st_bit;

typedef int(*BIT_MOB)(st_bit, st_bit, UINT64 *);

BOOL AlocMobilityFunc(void);
/***************************************************************************
* Name  : CountBit
* Brief : �r�b�g�񂩂�P�������Ă���r�b�g�̐��𐔂���
* Return: �P�������Ă���r�b�g��
****************************************************************************/
INT32 CountBit(UINT64 bit);