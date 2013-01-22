/**
 * sort.c
 *
 * Copyright (c) 2012
 *      libchewing Core Team. See ChangeLog for details.
 *
 * See the file "COPYING" for information on usage and redistribution
 * of this file.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chewing-private.h"
#include "chewing-utf8-util.h"
#include "global-private.h"
#include "key2pho-private.h"
#include "zuin-private.h"

#define CHARDEF_BEGIN		"%chardef  begin"
#define CHARDEF_END		"%chardef  end"
#define MAX_LINE_LEN		(1024)
#define MAX_WORD_DATA		(60000)
#define MAX_PHONE		(12000)
#define MAX_PHRASE_BUF_LEN	(149)
#define MAX_FILE_NAME		(256)
#define MAX_PHRASE_DATA		(420000)
#define PHONEID_FILE		"phoneid.dic"

const char USAGE[] =
	"usage: %s <phone.cin> <tsi.src>\n"
	"This program creates the following new files:\n"
#ifdef USE_BINARY_DATA
	"* " CHAR_INDEX_PHONE_FILE "\n\tindex of word file (phone -> index)\n"
	"* " CHAR_INDEX_BEGIN_FILE "\n\tindex of word file (index -> offset)\n"
#else
	"* " CHAR_INDEX_FILE "\n\tindex of word file\n"
#endif
	"* " CHAR_FILE "\n\tmain word file\n"
	"* " PH_INDEX_FILE "\n\tindex of phrase file\n"
	"* " DICT_FILE "\n\tmain phrase file\n"
	"* " PHONEID_FILE "\n\tintermediate file for make_tree\n"
;

struct WordData {
	uint16_t phone;
	char word[MAX_UTF8_SIZE];
};

struct PhraseData {
	char phrase[MAX_PHRASE_BUF_LEN];
	int freq;
	uint16_t phone[MAX_PHRASE_LEN + 1];
};

struct WordData word_data[MAX_WORD_DATA];
int num_word_data = 0;

struct PhraseData phrase_data[MAX_PHRASE_DATA];
int num_phrase_data = 0;

const struct PhraseData EXCEPTION_PHRASE[] = {
	{ "\xE5\xA5\xBD\xE8\x90\x8A\xE5\xA1\xA2" /* 好萊塢 */ , 0, { 5691, 4138, 256 } /* ㄏㄠˇ ㄌㄞˊ ㄨ */ },
	{ "\xE6\x88\x90\xE6\x97\xA5\xE5\xAE\xB6" /* 成日家 */ , 0, { 8290, 9220, 6281 } /* ㄔㄥˊ ㄖˋ ㄐㄧㄚ˙ */ },
	{ "\xE7\xB5\x90\xE5\xB7\xB4" /* 結巴 */ , 0, { 6304, 521 } /*  ㄐㄧㄝ ㄅㄚ˙ */ },
};

int store_word(const char *line)
{
	char phone_buf[MAX_UTF8_SIZE * ZUIN_SIZE + 1];
	char key_buf[ZUIN_SIZE + 1];

	if (num_word_data >= MAX_WORD_DATA) {
		fprintf(stderr, "Need to increase MAX_WORD_DATA to process\n");
		exit(-1);
	}

	/* FIXME: Hope the buffers are sufficient. */
	sscanf(line, "%s %s", key_buf, word_data[num_word_data].word);

	if (strlen(key_buf) > ZUIN_SIZE)
		return -1;
	PhoneFromKey(phone_buf, key_buf, KB_DEFAULT, 1);
	word_data[num_word_data].phone = UintFromPhone(phone_buf);
	++num_word_data;

	return 0;
}

int compare_word_by_phone(const void *x, const void *y)
{
	const struct WordData *a = (struct WordData *)x;
	const struct WordData *b = (struct WordData *)y;

	if (a->phone != b->phone)
		return a->phone - b->phone;

	/* Compare address for stable sort */
	if (a < b)
		return -1;
	if (a > b)
		return 1;
	return 0;
}

int compare_word(const void *x, const void *y)
{
	const struct WordData *a = (const struct WordData *)x;
	const struct WordData *b = (const struct WordData *)y;
	int ret;

	ret = strcmp(a->word, b->word);
	if (ret != 0)
		return ret;

	if (a->phone != b->phone)
		return a->phone - b->phone;

	return 0;
}

int compare_word_no_duplicated(const void *x, const void *y)
{
	int ret;

	ret = compare_word(x, y);
	if (!ret) {
		const struct WordData *a = (const struct WordData *)x;
		fprintf(stderr, "Duplicated word found (`%s', %d).\n", a->word, a->phone);
		exit(-1);
	}

	return ret;
}

void read_phone_cin(const char *filename)
{
	FILE *phone_cin;
	char buf[MAX_LINE_LEN];
	char *ret;

	phone_cin = fopen(filename, "r");
	if (!phone_cin) {
		fprintf(stderr, "Error opening the file %s\n", filename);
		exit(-1);
	}

	/* Find `%chardef  begin' */
	for (;;) {
		ret = fgets(buf, sizeof(buf), phone_cin);
		if (!ret) {
			fprintf(stderr, "Cannot find %s\n", CHARDEF_BEGIN);
			exit(-1);
		}

		if (strncmp(buf, CHARDEF_BEGIN, strlen(CHARDEF_BEGIN)) == 0) {
			break;
		}
	}

	/* read all words into word_data. */
	for (;;) {
		ret = fgets(buf, sizeof(buf), phone_cin);
		if (!ret || buf[0] == '%')
			break;

		if (store_word(buf) != 0) {
			fprintf(stderr,"The line `%s' in `%s' is corrupted!\n", buf, filename);
			exit(-1);
		}
	}
	fclose(phone_cin);

	return;
}

void sort_word_for_dictionary()
{
	qsort(word_data, num_word_data, sizeof(word_data[0]), compare_word_no_duplicated);
}

int is_exception_phrase(struct PhraseData *phrase) {
	int i;

	for (i = 0; i < sizeof(EXCEPTION_PHRASE) / sizeof(EXCEPTION_PHRASE[0]); ++i) {
		if (strcmp(phrase->phrase, EXCEPTION_PHRASE[i].phrase) == 0 &&
			memcmp(phrase->phone, EXCEPTION_PHRASE[i].phone, sizeof(phrase->phone)) == 0) {
			return 1;
		}
	}

	return 0;
}

void store_phrase(const char *line)
{
	const char DELIM[] = " \t\n";
	char buf[MAX_LINE_LEN];
	char *phrase;
	char *freq;
	char *bopomofo;
	int phrase_len;
	int i;
	int j;
	struct WordData word;
	char bopomofo_buf[MAX_UTF8_SIZE * ZUIN_SIZE + 1];

	if (num_phrase_data >= MAX_PHRASE_DATA) {
		fprintf(stderr, "Need to increase MAX_PHRASE_DATA to process\n");
		exit(-1);
	}

	strncpy(buf, line, sizeof(buf));

	/* read phrase */
	phrase = strtok(buf, DELIM);
	if (!phrase) {
		fprintf(stderr, "Error reading line `%s'\n", line);
		exit(-1);
	}
	strncpy(phrase_data[num_phrase_data].phrase, phrase, sizeof(phrase_data[0].phrase));

	/* read frequency */
	freq = strtok(NULL, DELIM);
	if (!freq) {
		fprintf(stderr, "Error reading line `%s'\n", line);
		exit(-1);
	}

	errno = 0;
	phrase_data[num_phrase_data].freq = strtol(freq, 0, 0);
	if (errno) {
		fprintf(stderr, "Error reading frequency `%s' in `%s'\n", freq, line);
		exit(-1);
	}

	/* read bopomofo */
	for (bopomofo = strtok(NULL, DELIM), phrase_len = 0;
		bopomofo && phrase_len < MAX_PHRASE_LEN;
		bopomofo = strtok(NULL, DELIM), ++phrase_len) {

		phrase_data[num_phrase_data].phone[phrase_len] = UintFromPhone(bopomofo);
		if (phrase_data[num_phrase_data].phone[phrase_len] == 0) {
			fprintf(stderr, "Error reading bopomofo `%s' in `%s'\n", bopomofo, line);
			exit(-1);
		}
	}
	if (bopomofo) {
		fprintf(stderr, "Phrase `%s' too long\n", phrase);
	}

	/* check phrase length & bopomofo length */
	if (ueStrLen(phrase_data[num_phrase_data].phrase) != phrase_len) {
		fprintf(stderr, "Phrase length and bopomofo length mismatch in `%s'\n", line);
		exit(-1);
	}

	if (phrase_len == 1) {
		fprintf(stderr, "Word `%s' shall be in phone.cin, not tsi.src\n", phrase_data[num_phrase_data].phrase);
		exit(-1);
	}

	/* check each word in phrase */
	for (i = 0; i < phrase_len; ++i) {
		ueStrNCpy(word.word, ueStrSeek(phrase_data[num_phrase_data].phrase, i), 1, 1);
		word.phone = phrase_data[num_phrase_data].phone[i];

		if (bsearch(&word, word_data, num_word_data, sizeof(word), compare_word) == NULL &&
			!is_exception_phrase(&phrase_data[num_phrase_data])) {

			PhoneFromUint(bopomofo_buf, sizeof(bopomofo_buf), word.phone);

			fprintf(stderr, "Error in phrase `%s' ", phrase_data[num_phrase_data].phrase);
			fprintf(stderr, "{%d", phrase_data[num_phrase_data].phone[0]);
			for (j = 1; j < phrase_len; ++j) {
				fprintf(stderr, ", %d", phrase_data[num_phrase_data].phone[j]);
			}
			fprintf(stderr, "}. ");
			fprintf(stderr, "Word `%s' has no phone %d (%s).\n", word.word, word.phone, bopomofo_buf);
			/* FIXME: shall exit(-1) when tsi.src is fixed */
			//exit(-1);
		}
	}

	++num_phrase_data;
}

int compare_phrase(const void *x, const void *y)
{
	const struct PhraseData *a = (const struct PhraseData *) x;
	const struct PhraseData *b = (const struct PhraseData *) y;
	int cmp;
	int i;

	for (i = 0; i < sizeof(a->phone) / sizeof(a->phone[0]); ++i) {
		cmp = a->phone[i] - b->phone[i];
		if (cmp)
			return cmp;
	}

	if (!strcmp(a->phrase, b->phrase)) {
		fprintf(stderr, "Duplicated phrase `%s' found.\n", a->phrase);
		exit(-1);
	}

	if (a->freq == b->freq) {
		if (a->phone[1] == 0) {
			if (a < b)
				return -1;
			if (a > b)
				return 1;
		}
		fprintf(stderr, "Phrase `%s' and `%s' have the same phone and frequency (%d).\n", a->phrase, b->phrase, a->freq);
		/* FIXME: shall exit(-1) when tsi.src is fixed */
		//exit(-1);
	}

	return b->freq - a->freq;
}

void read_tsi_src(const char *filename)
{
	FILE *tsi_src;
	char buf[MAX_LINE_LEN];

	tsi_src = fopen(filename, "r");
	if (!tsi_src) {
		fprintf(stderr, "Error opening the file %s\n", filename);
		exit(-1);
	}

	while (fgets(buf, sizeof(buf), tsi_src)) {
		store_phrase(buf);
	}

}

void merge_word_to_phrase()
{
	int i;

	for (i = 0; i < num_word_data; ++i, ++num_phrase_data) {
		strncpy(phrase_data[num_phrase_data].phrase, word_data[i].word, sizeof(phrase_data[0].phrase));
		phrase_data[num_phrase_data].phone[0] = word_data[i].phone;
		phrase_data[num_phrase_data].phone[1] = 0;
		phrase_data[num_phrase_data].freq = 0;
	}
}

int compare_phone_in_phrase(int x, int y)
{
	return memcmp(phrase_data[x].phone, phrase_data[y].phone, sizeof(phrase_data[0].phone));
}

void write_phrase_data()
{
	FILE *dict_file;
	FILE *ph_index_file;
	FILE *phoneid_file;
	int i;
	int j;
	int pos;
#ifdef USE_BINARY_DATA
	unsigned char size;
#endif

#ifdef USE_BINARY_DATA
	dict_file = fopen(DICT_FILE, "wb");
	ph_index_file = fopen(PH_INDEX_FILE, "wb");
#else
	dict_file = fopen(DICT_FILE, "w");
	ph_index_file = fopen(PH_INDEX_FILE, "w");
#endif
	phoneid_file = fopen(PHONEID_FILE, "w");

	if (!(dict_file && ph_index_file && phoneid_file)) {
		fprintf(stderr, "Cannot open output file.\n");
		exit(-1);
	}

	for (i = 0; i < num_phrase_data - 1; ++i) {
		if (i == 0 || compare_phone_in_phrase(i - 1, i)) {
			pos = ftell(dict_file);
#ifdef USE_BINARY_DATA
			fwrite(&pos, sizeof(pos), 1, ph_index_file);
#else
			fprintf(ph_index_file, "%d\n", pos);
#endif
		}
#ifdef USE_BINARY_DATA
		size = strlen(phrase_data[i].phrase);
		fwrite(&size, sizeof(size), 1, dict_file);
		fwrite(phrase_data[i].phrase, size, 1, dict_file);
		fwrite(&phrase_data[i].freq, sizeof(phrase_data[0].freq), 1, dict_file);
#else
		fprintf(dict_file, "%s %d\t", phrase_data[i].phrase, phrase_data[i].freq);
#endif
	}

	pos = ftell(dict_file);
#ifdef USE_BINARY_DATA
	fwrite(&pos, sizeof(pos), 1, ph_index_file);
	size = strlen(phrase_data[i].phrase);
	fwrite(&size, sizeof(size), 1, dict_file);
	fwrite(phrase_data[i].phrase, size, 1, dict_file);
	fwrite(&phrase_data[i].freq, sizeof(phrase_data[0].freq), 1, dict_file);
	pos = ftell(dict_file);
	fwrite(&pos, sizeof(pos), 1, ph_index_file);
#else
	fprintf(ph_index_file, "%d\n", pos);
	fprintf(dict_file, "%s %d", phrase_data[i].phrase, phrase_data[i].freq);
	pos = ftell(dict_file);
	fprintf(ph_index_file, "%d\n", pos);
#endif

	for (i = 0; i < num_phrase_data; ++i) {
		if (i > 0 && !compare_phone_in_phrase(i - 1, i))
			continue;

		for (j = 0; phrase_data[i].phone[j]; ++j) {
			fprintf(phoneid_file, "%hu ", phrase_data[i].phone[j]);
		}
		fprintf(phoneid_file, "0\n");

	}

	fclose(phoneid_file);
	fclose(ph_index_file);
	fclose(dict_file);
}

int main(int argc, char *argv[])
{
	if (argc != 3) {
		printf(USAGE, argv[0]);
		return -1;
	}

	read_phone_cin(argv[1]);
	merge_word_to_phrase();

	sort_word_for_dictionary();
	read_tsi_src(argv[2]);

	qsort(phrase_data, num_phrase_data, sizeof(phrase_data[0]), compare_phrase);

	write_phrase_data();

	return 0;
}
