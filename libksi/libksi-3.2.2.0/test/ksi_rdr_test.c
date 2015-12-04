/*
 * Copyright 2013-2015 Guardtime, Inc.
 *
 * This file is part of the Guardtime client SDK.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES, CONDITIONS, OR OTHER LICENSES OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 * "Guardtime" and "KSI" are trademarks or registered trademarks of
 * Guardtime, Inc., and no license to trademarks is granted; Guardtime
 * reserves and retains all trademark rights.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cutest/CuTest.h"
#include "all_tests.h"
#include  <ksi/tlv.h>
#include  <ksi/io.h>

extern KSI_CTX *ctx;

static const char TMP_FILE[] = "tmpfile.tmp";

struct KSI_RDR_st {
	/* Context for the reader. */
	KSI_CTX *ctx;

	/* Type of the reader (see #KSI_IO_Type) */
	int ioType;

	/* Union of inputs. */
	union {
		/* KSI_IO_FILE type input. */
		FILE *file;

		/* KSI_IO_MEM type input */
		struct {
			char *buffer;
			size_t buffer_length;

			/* Does the memory belong to this reader? */
			int ownCopy;
		} mem;
	} data;

	/* Offset of stream. */
	size_t offset;

	/* Indicates end of stream.
	 * \note This will be set after reading the stream. */
	int eof;
};


static void TestRdrFileBadStream(CuTest* tc) {
	int res;
	KSI_RDR *rdr = NULL;

	/* Init reader from non existing file name */
	res = KSI_RDR_fromStream(ctx, NULL, &rdr);

	/* Assert failure to initialize */
	CuAssert(tc, "Reader initzialisation did not fail from bad input stream.", res != KSI_OK);

	/* Assert there is no reader object. */
	CuAssert(tc, "There should be no reader object after failure.", rdr == NULL);

	/* Closing a NULL reader should not fail. */
	KSI_RDR_close(NULL);
}

static void TestRdrFileFileReading(CuTest* tc) {
	int res;
	unsigned char tmpBuf[0xffff];
	size_t readCount;

	static char testStr[] = "Randomness is too important to be left to chance";

	FILE *f = NULL;

	KSI_RDR *rdr = NULL;

	/* Write some data to file */
	f = fopen(getFullResourcePath(TMP_FILE) , "w");
	CuAssert(tc, "Unable to create temporary file", f != NULL);
	CuAssert(tc, "Unable to write temporary file", fprintf(f, "%s", testStr) > 0);
	CuAssert(tc, "Unable to close temporary file", !fclose(f));

	f = fopen(getFullResourcePath(TMP_FILE), "rb");
	/* Try reading it back. */
	res = KSI_RDR_fromStream(ctx, f, &rdr);
	CuAssert(tc, "Error creating reader from file.", res == KSI_OK);
	CuAssert(tc, "Creating reader from file did not fail, but object is still NULL", rdr != NULL);
	/* Read as a single block. */
	KSI_RDR_read_ex(rdr, tmpBuf, sizeof(tmpBuf), &readCount);

	CuAssert(tc, "Wrong length read", readCount == strlen(testStr));

	CuAssert(tc, "Reader is not at EOF", rdr->eof);
	fclose(f);
	KSI_RDR_close(rdr);

	/* Remove temporary file */
	CuAssert(tc, "Unable to remove temporary file", remove(getFullResourcePath(TMP_FILE)) == 0);
}


static void TestRdrFileReadingChuncks(CuTest* tc) {
	int res;
	unsigned char tmpBuf[0xffff];
	size_t readCount;
	size_t size = 0;

	static char testStr[] = "Randomness is too important to be left to chance";

	FILE *f = NULL;

	KSI_RDR *rdr = NULL;

	/* Write some data to file */
	f = fopen(getFullResourcePath(TMP_FILE), "w");
	CuAssert(tc, "Unable to create tempprary file", f != NULL);
	CuAssert(tc, "Unable to write temporary file", fprintf(f, "%s", testStr) > 0);
	CuAssert(tc, "Unable to close temporary file", !fclose(f));

	/* Try reading it back. */
	f = fopen(getFullResourcePath(TMP_FILE), "r");
	res = KSI_RDR_fromStream(ctx, f, &rdr);
	CuAssert(tc, "Error creating reader from file.", res == KSI_OK);
	CuAssert(tc, "Creating reader from file did not fail, but object is still NULL", rdr != NULL);
	/* Read blocks of size 10. */
	while (!KSI_RDR_isEOF(rdr)) {
		KSI_RDR_read_ex(rdr, tmpBuf + size, 10, &readCount);
		size += readCount;
	}

	CuAssert(tc, "Wrong length read", size == strlen(testStr));

	CuAssert(tc, "Reader is not at EOF", rdr->eof);
	fclose(f);
	KSI_RDR_close(rdr);

	/* Remove temporary file */
	CuAssert(tc, "Unable to remove temporary file", remove(getFullResourcePath(TMP_FILE)) == 0);
}

static void TestRdrMemInitExtStorage(CuTest* tc) {
	int res;
	size_t readCount;

	KSI_RDR *rdr = NULL;
	static char testData[] = "Random binary data.";
	unsigned char tmpBuf[0xffff];

	/* Init reader. */
	res = KSI_RDR_fromSharedMem(ctx, (unsigned char *)testData, sizeof(testData), &rdr);
	CuAssert(tc, "Failed initializing context from shared memory.", res == KSI_OK);
	CuAssert(tc, "Init did not fail, but object not created.", rdr != NULL);

	res = KSI_RDR_read_ex(rdr, tmpBuf, sizeof(tmpBuf), &readCount);
	CuAssert(tc, "Incorrect read count.", res == KSI_OK && readCount == sizeof(testData));

	CuAssert(tc, "Data missmatch", !memcmp(tmpBuf, testData, sizeof(testData)));

	KSI_RDR_close(rdr);
}

CuSuite* KSITest_RDR_getSuite(void)
{
	CuSuite* suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, TestRdrFileBadStream);
	SUITE_ADD_TEST(suite, TestRdrFileFileReading);
	SUITE_ADD_TEST(suite, TestRdrFileReadingChuncks);

	SUITE_ADD_TEST(suite, TestRdrMemInitExtStorage);

	return suite;
}
