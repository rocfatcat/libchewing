/**
 * char.c
 *
 * Copyright (c) 1999, 2000, 2001
 *	Lu-chuan Kung and Kang-pen Chen.
 *	All rights reserved.
 *
 * Copyright (c) 2004, 2005, 2006, 2008
 *	libchewing Core Team. See ChangeLog for details.
 *
 * See the file "COPYING" for information on usage and redistribution
 * of this file.
 */

/** 
 * @file char.c
 * @brief word data file
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "global-private.h"
#include "chewing-definition.h"
#include "char-private.h"
#include "dict-private.h"
#include "tree-private.h"
#include "private.h"
#include "plat_mmap.h"

int GetCharFirst( ChewingData *pgdata, Word *wrd_ptr, uint16_t phoneid )
{
	int phone_phr_id;
	Phrase phrase = { 0 };
	int ret;

	phone_phr_id = TreeFindPhrase( pgdata, 0, 0, &phoneid );
	if ( phone_phr_id < 0 ) {
		return 0;
	}

	ret = GetPhraseFirst( pgdata, &phrase, phone_phr_id );
	strncpy( wrd_ptr->word, phrase.phrase, sizeof( wrd_ptr->word ) );
	return ret;
}

int GetCharNext( ChewingData *pgdata, Word *wrd_ptr )
{
	Phrase phrase = { 0 };
	int ret;

	ret = GetPhraseNext( pgdata, &phrase );
	strncpy( wrd_ptr->word, phrase.phrase, sizeof( wrd_ptr->word ) );
	return ret;
}
