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

#include <string.h>

#include "internal.h"

#include "hashchain.h"
#include "tlv.h"
#include "tlv_template.h"

/* For optimization reasons, we need need access to KSI_DataHasher->closeExisting() function. */
#include "hash_impl.h"

KSI_IMPORT_TLV_TEMPLATE(KSI_HashChainLink)

struct KSI_HashChainLink_st {
	KSI_CTX *ctx;
	int isLeft;
	KSI_Integer *levelCorrection;
	KSI_DataHash *metaHash;
	KSI_MetaData *metaData;
	KSI_DataHash *imprint;
};

struct KSI_CalendarHashChain_st {
	KSI_CTX *ctx;
	KSI_Integer *publicationTime;
	KSI_Integer *aggregationTime;
	KSI_DataHash *inputHash;
	KSI_LIST(KSI_HashChainLink) *hashChain;
};

KSI_IMPLEMENT_LIST(KSI_HashChainLink, KSI_HashChainLink_free);
KSI_IMPLEMENT_LIST(KSI_CalendarHashChainLink, KSI_HashChainLink_free);
KSI_IMPLEMENT_LIST(KSI_CalendarHashChain, KSI_CalendarHashChain_free);

static long long int highBit(long long int n) {
    n |= (n >>  1);
    n |= (n >>  2);
    n |= (n >>  4);
    n |= (n >>  8);
    n |= (n >> 16);
    n |= (n >> 32);
    return n - (n >> 1);
}


static int addNvlImprint(const KSI_DataHash *first, const KSI_DataHash *second, KSI_DataHasher *hsr) {
	int res = KSI_UNKNOWN_ERROR;
	const KSI_DataHash *hsh = first;
	const unsigned char *imprint = NULL;
	unsigned int imprint_len;

	if (hsh == NULL) {
		if (second == NULL) {
			res = KSI_INVALID_ARGUMENT;
			goto cleanup;
		}
		hsh = second;
	}

	res = KSI_DataHash_getImprint(hsh, &imprint, &imprint_len);
	if (res != KSI_OK) goto cleanup;

	res = KSI_DataHasher_add(hsr, imprint, imprint_len);
	if (res != KSI_OK) goto cleanup;

	res = KSI_OK;

cleanup:

	KSI_nofree(imprint);

	return res;
}

static int addChainImprint(KSI_CTX *ctx, KSI_DataHasher *hsr, KSI_HashChainLink *link) {
	int res = KSI_UNKNOWN_ERROR;
	int mode = 0;
	const unsigned char *imprint = NULL;
	unsigned int imprint_len;
	KSI_MetaData *metaData = NULL;
	KSI_DataHash *metaHash = NULL;
	KSI_DataHash *hash = NULL;
	KSI_OctetString *tmpOctStr = NULL;

	KSI_ERR_clearErrors(ctx);
	if (ctx == NULL || hsr == NULL || link == NULL) {
		KSI_pushError(ctx, res = KSI_INVALID_ARGUMENT, NULL);
		goto cleanup;
	}

	res = KSI_HashChainLink_getImprint(link, &hash);
	if (res != KSI_OK) {
		KSI_pushError(ctx, res, NULL);
		goto cleanup;
	}

	res = KSI_HashChainLink_getMetaData(link, &metaData);
	if (res != KSI_OK) {
		KSI_pushError(ctx, res, NULL);
		goto cleanup;
	}

	res = KSI_HashChainLink_getMetaHash(link, &metaHash);
	if (res != KSI_OK) {
		KSI_pushError(ctx, res, NULL);
		goto cleanup;
	}

	if (hash != NULL) mode |= 0x01;
	if (metaHash != NULL) mode |= 0x02;
	if (metaData != NULL) mode |= 0x04;

	switch (mode) {
		case 0x01:
			res = KSI_DataHash_getImprint(hash, &imprint, &imprint_len);
			if (res != KSI_OK) {
				KSI_pushError(ctx, res, NULL);
				goto cleanup;
			}
			break;
		case 0x02:
			res = KSI_DataHash_getImprint(metaHash, &imprint, &imprint_len);
			if (res != KSI_OK) {
				KSI_pushError(ctx, res, NULL);
				goto cleanup;
			}
			break;
		case 0x04:
			res = KSI_MetaData_getRaw(metaData, &tmpOctStr);
			if (res != KSI_OK) {
				KSI_pushError(ctx, res, NULL);
				goto cleanup;
			}

			res = KSI_OctetString_extract(tmpOctStr, &imprint, &imprint_len);
			if (res != KSI_OK) {
				KSI_pushError(ctx, res, NULL);
				goto cleanup;
			}
			break;
		default:
			KSI_pushError(ctx, res = KSI_INVALID_FORMAT, NULL);
			goto cleanup;
	}

	res = KSI_DataHasher_add(hsr, imprint, imprint_len);
	if (res != KSI_OK) {
		KSI_pushError(ctx, res, NULL);
		goto cleanup;
	}

	res = KSI_OK;

cleanup:

	KSI_nofree(hash);
	KSI_nofree(metaHash);
	KSI_nofree(metaData);
	KSI_nofree(imprint);
	KSI_nofree(tmpOctStr);

	return res;
}

static int aggregateChain(KSI_CTX *ctx, KSI_LIST(KSI_HashChainLink) *chain, const KSI_DataHash *inputHash, int startLevel, int hash_id, int isCalendar, int *endLevel, KSI_DataHash **outputHash) {
	int res = KSI_UNKNOWN_ERROR;
	int level = startLevel;
	KSI_DataHasher *hsr = NULL;
	KSI_DataHash *hsh = NULL;
	KSI_HashChainLink *link = NULL;
	int algo_id = hash_id;
	char chr_level;
	char logMsg[0xff];
	size_t i;

	KSI_ERR_clearErrors(ctx);
	if (ctx == NULL || chain == NULL || inputHash == NULL || outputHash == NULL) {
		KSI_pushError(ctx, res = KSI_INVALID_ARGUMENT, NULL);
		goto cleanup;
	}

	/* If we are calculating the calendar chain, initialize the hash algorithm id using
	 * the input hash. */
	if (isCalendar) {
		res = KSI_DataHash_extract(inputHash, &algo_id, NULL, NULL);
		if (res != KSI_OK) {
			KSI_pushError(ctx, res, NULL);
			goto cleanup;
		}
	}

	sprintf(logMsg, "Starting %s hash chain aggregation with input hash.", isCalendar ? "calendar": "aggregation");
	KSI_LOG_logDataHash(ctx, KSI_LOG_DEBUG, logMsg, inputHash);

	/* Loop over all the links in the chain. */
	for (i = 0; i < KSI_HashChainLinkList_length(chain); i++) {
		res = KSI_HashChainLinkList_elementAt(chain, i, &link);
		if (res != KSI_OK) {
			KSI_pushError(ctx, res, NULL);
			goto cleanup;
		}

		if (!isCalendar) {
			KSI_uint64_t levelCorrection = KSI_Integer_getUInt64(link->levelCorrection);
			if (levelCorrection > 0xff || level + levelCorrection + 1 > 0xff)
				KSI_pushError(ctx, res = KSI_INVALID_ARGUMENT, "Aggregation chain level out of range.");
			level += (int)levelCorrection + 1;
		} else {
			/* Update the hash algo id when we encounter a left link. */
			if (link->isLeft) {
				res = KSI_DataHash_extract(link->imprint, &algo_id, NULL, NULL);
				if (res != KSI_OK) {
					KSI_pushError(ctx, res, NULL);
					goto cleanup;
				}
			}
		}

		/* Create or reset the hasher. */
		if (hsr == NULL) {
			res = KSI_DataHasher_open(ctx, algo_id, &hsr);
		} else {
			res = KSI_DataHasher_reset(hsr);
		}
		if (res != KSI_OK) {
			KSI_pushError(ctx, res, NULL);
			goto cleanup;
		}

		if (link->isLeft) {
			res = addNvlImprint(hsh, inputHash, hsr);
			if (res != KSI_OK) {
				KSI_pushError(ctx, res, NULL);
				goto cleanup;
			}

			res = addChainImprint(ctx, hsr, link);
			if (res != KSI_OK) {
				KSI_pushError(ctx, res, NULL);
				goto cleanup;
			}
		} else {
			res = addChainImprint(ctx, hsr, link);
			if (res != KSI_OK) {
				KSI_pushError(ctx, res, NULL);
				goto cleanup;
			}

			res = addNvlImprint(hsh, inputHash, hsr);
			if (res != KSI_OK) {
				KSI_pushError(ctx, res, NULL);
				goto cleanup;
			}
		}


		if (level > 0xff) {
			KSI_pushError(ctx, res = KSI_INVALID_FORMAT, "Aggregation chain length exceeds 0xff.");
			goto cleanup;
		}

		chr_level = (char) level;
		KSI_DataHasher_add(hsr, &chr_level, 1);

		if (hsh != NULL) {
			res = hsr->closeExisting(hsr, hsh);
		} else {
			res = KSI_DataHasher_close(hsr, &hsh);
		}
		if (res != KSI_OK) {
			KSI_pushError(ctx, res, NULL);
			goto cleanup;
		}
	}


	if (endLevel != NULL) *endLevel = level;
	*outputHash = hsh;
	hsh = NULL;

	sprintf(logMsg, "Finished %s hash chain aggregation with output hash.", isCalendar ? "calendar": "aggregation");
	KSI_LOG_logDataHash(ctx, KSI_LOG_DEBUG, logMsg, *outputHash);

	res = KSI_OK;

cleanup:

	KSI_DataHasher_free(hsr);
	KSI_DataHash_free(hsh);

	return res;
}

/**
 *
 */
static int calculateCalendarAggregationTime(KSI_LIST(KSI_HashChainLink) *chain, const KSI_Integer *pub_time, time_t *utc_time) {
	int res = KSI_UNKNOWN_ERROR;
	long long int r;
	long long int t = 0;
	KSI_HashChainLink *hn = NULL;
	size_t i;

	if (chain == NULL || pub_time == NULL) {
		res = KSI_INVALID_ARGUMENT;
		goto cleanup;
	}

	if (KSI_HashChainLinkList_length(chain) == 0) {
		res = KSI_INVALID_FORMAT;
		goto cleanup;
	}

	r = (time_t) KSI_Integer_getUInt64(pub_time);

	/* Traverse the list from the end to the beginning. */
	for (i = 0; i < KSI_HashChainLinkList_length(chain); i++) {
		int isLeft = 0;
		if (r <= 0) {
			res = KSI_INVALID_FORMAT;
			goto cleanup;
		}
		res = KSI_HashChainLinkList_elementAt(chain, KSI_HashChainLinkList_length(chain) - i - 1, &hn);
		if (res != KSI_OK) goto cleanup;

		res = KSI_HashChainLink_getIsLeft(hn, &isLeft);
		if (res != KSI_OK) goto cleanup;

		if (isLeft) {
			r = highBit(r) - 1;
		} else {
			t += highBit(r);
			r -= highBit(r);
		}
	}

	if (r != 0) {
		res = KSI_INVALID_FORMAT;
		goto cleanup;
	}

	*utc_time = (time_t) t;

	res = KSI_OK;

cleanup:

	KSI_nofree(hn);

	return res;
}

/**
 *
 */
int KSI_HashChain_aggregate(KSI_CTX *ctx, KSI_LIST(KSI_HashChainLink) *chain, const KSI_DataHash *inputHash, int startLevel, int hash_id, int *endLevel, KSI_DataHash **outputHash) {
	return aggregateChain(ctx, chain, inputHash, startLevel, hash_id, 0, endLevel, outputHash);
}

/**
 *
 */
int KSI_HashChain_aggregateCalendar(KSI_CTX *ctx, KSI_LIST(KSI_HashChainLink) *chain, const KSI_DataHash *inputHash, KSI_DataHash **outputHash) {
	return aggregateChain(ctx, chain, inputHash, 0xff, -1, 1, NULL, outputHash);
}

/**
 * KSI_CalendarHashChain
 */
void KSI_CalendarHashChain_free(KSI_CalendarHashChain *t) {
	if (t != NULL) {
		KSI_Integer_free(t->publicationTime);
		KSI_Integer_free(t->aggregationTime);
		KSI_DataHash_free(t->inputHash);
		KSI_HashChainLinkList_free(t->hashChain);
		KSI_free(t);
	}
}

int KSI_CalendarHashChain_new(KSI_CTX *ctx, KSI_CalendarHashChain **t) {
	int res = KSI_UNKNOWN_ERROR;
	KSI_CalendarHashChain *tmp = NULL;
	tmp = KSI_new(KSI_CalendarHashChain);
	if (tmp == NULL) {
		res = KSI_OUT_OF_MEMORY;
		goto cleanup;
	}

	tmp->ctx = ctx;
	tmp->publicationTime = NULL;
	tmp->aggregationTime = NULL;
	tmp->inputHash = NULL;
	tmp->hashChain = NULL;
	*t = tmp;
	tmp = NULL;
	res = KSI_OK;
cleanup:
	KSI_CalendarHashChain_free(tmp);
	return res;
}

int KSI_CalendarHashChain_aggregate(KSI_CalendarHashChain *chain, KSI_DataHash **hsh) {
	int res = KSI_UNKNOWN_ERROR;

	if (chain == NULL || hsh == NULL) {
		res = KSI_INVALID_ARGUMENT;
		goto cleanup;
	}
	KSI_ERR_clearErrors(chain->ctx);

	res = KSI_HashChain_aggregateCalendar(chain->ctx, chain->hashChain, chain->inputHash, hsh);
	if (res != KSI_OK) {
		KSI_pushError(chain->ctx, res, NULL);
		goto cleanup;
	}

	res = KSI_OK;

cleanup:

	return res;
}

int KSI_CalendarHashChain_calculateAggregationTime(KSI_CalendarHashChain *chain, time_t *aggrTime) {
	int res = KSI_UNKNOWN_ERROR;

	if (chain == NULL || aggrTime == NULL) {
		res = KSI_INVALID_ARGUMENT;
		goto cleanup;
	}
	KSI_ERR_clearErrors(chain->ctx);

	res = calculateCalendarAggregationTime(chain->hashChain, chain->publicationTime, aggrTime);
	if (res != KSI_OK) {
		KSI_pushError(chain->ctx, res, NULL);
		goto cleanup;
	}

	res = KSI_OK;

cleanup:

	return res;

}

KSI_IMPLEMENT_GETTER(KSI_CalendarHashChain, KSI_Integer*, publicationTime, PublicationTime);
KSI_IMPLEMENT_GETTER(KSI_CalendarHashChain, KSI_Integer*, aggregationTime, AggregationTime);
KSI_IMPLEMENT_GETTER(KSI_CalendarHashChain, KSI_DataHash*, inputHash, InputHash);
KSI_IMPLEMENT_GETTER(KSI_CalendarHashChain, KSI_LIST(KSI_HashChainLink)*, hashChain, HashChain);

KSI_IMPLEMENT_SETTER(KSI_CalendarHashChain, KSI_Integer*, publicationTime, PublicationTime);
KSI_IMPLEMENT_SETTER(KSI_CalendarHashChain, KSI_Integer*, aggregationTime, AggregationTime);
KSI_IMPLEMENT_SETTER(KSI_CalendarHashChain, KSI_DataHash*, inputHash, InputHash);
KSI_IMPLEMENT_SETTER(KSI_CalendarHashChain, KSI_LIST(KSI_HashChainLink)*, hashChain, HashChain);

/**
 * KSI_HashChainLink
 */
void KSI_HashChainLink_free(KSI_HashChainLink *t) {
	if (t != NULL) {
		KSI_DataHash_free(t->metaHash);
		KSI_MetaData_free(t->metaData);
		KSI_DataHash_free(t->imprint);
		KSI_Integer_free(t->levelCorrection);
		KSI_free(t);
	}
}

int KSI_HashChainLink_new(KSI_CTX *ctx, KSI_HashChainLink **t) {
	int res = KSI_UNKNOWN_ERROR;
	KSI_HashChainLink *tmp = NULL;

	if (ctx == NULL || t == NULL) {
		KSI_pushError(ctx, res = KSI_INVALID_ARGUMENT, NULL);
		goto cleanup;
	}

	tmp = KSI_new(KSI_HashChainLink);
	if (tmp == NULL) {
		res = KSI_OUT_OF_MEMORY;
		goto cleanup;
	}

	tmp->ctx = ctx;
	tmp->isLeft = 0;
	tmp->levelCorrection = NULL;
	tmp->metaHash = NULL;
	tmp->metaData = NULL;
	tmp->imprint = NULL;

	*t = tmp;
	tmp = NULL;

	res = KSI_OK;

cleanup:

	KSI_HashChainLink_free(tmp);
	return res;
}


int KSI_CalendarHashChainLink_fromTlv(KSI_TLV *tlv, KSI_CalendarHashChainLink **link) {
	int res = KSI_UNKNOWN_ERROR;
	KSI_HashChainLink *tmp = NULL;
	KSI_DataHash *hsh = NULL;
	int isLeft = 0;
	KSI_CTX *ctx = NULL;

	if (tlv == NULL || link == NULL) {
		res = KSI_UNKNOWN_ERROR;
		goto cleanup;
	}
	ctx = KSI_TLV_getCtx(tlv);
	KSI_ERR_clearErrors(ctx);

	/* Determine if it is a left or right link. */
	switch (KSI_TLV_getTag(tlv)) {
		case 0x07: isLeft = 1; break;
		case 0x08: isLeft = 0; break;
		default: {
			char errm[0xff];
			KSI_snprintf(errm, sizeof(errm), "Unknown tag for hash chain link: 0x%02x", KSI_TLV_getTag(tlv));
			KSI_pushError(ctx, res = KSI_INVALID_FORMAT, errm);
			goto cleanup;
		}
	}

	/* Create a new link. */
	res = KSI_HashChainLink_new(ctx, &tmp);
	if (res != KSI_OK) {
		KSI_pushError(ctx, res, NULL);
		goto cleanup;
	}

	tmp->isLeft = isLeft;

	/* Encode the input TLV as a data hash object. */
	res = KSI_DataHash_fromTlv(tlv, &hsh);
	if (res != KSI_OK) {
		KSI_pushError(ctx, res, NULL);
		goto cleanup;
	}

	tmp->imprint = hsh;
	hsh = NULL;

	*link = tmp;
	tmp = NULL;

	res = KSI_OK;

cleanup:

	KSI_HashChainLink_free(tmp);
	KSI_DataHash_free(hsh);

	return res;
}


int KSI_CalendarHashChainLink_toTlv(KSI_CTX *ctx, KSI_CalendarHashChainLink *link, unsigned tag, int isNonCritica, int isForward, KSI_TLV **tlv) {
	int res = KSI_UNKNOWN_ERROR;
	KSI_TLV *tmp = NULL;
	unsigned tagOverride = 0;

	KSI_ERR_clearErrors(ctx);
	if (ctx == NULL || link == NULL || tlv == NULL) {
		res = KSI_INVALID_ARGUMENT;
		goto cleanup;
	}

	if (link->isLeft) tagOverride = 0x07;
	else tagOverride = 0x08;

	res = KSI_DataHash_toTlv(ctx, link->imprint, tagOverride, isNonCritica, isForward, &tmp);
	if (res != KSI_OK) {
		KSI_pushError(ctx, res, NULL);
		goto cleanup;
	}

	*tlv = tmp;
	tmp = NULL;

	res = KSI_OK;

cleanup:

	KSI_TLV_free(tmp);

	return res;
}

int KSI_HashChainLink_fromTlv(KSI_TLV *tlv, KSI_HashChainLink **link) {
	int res = KSI_UNKNOWN_ERROR;
	KSI_HashChainLink *tmp = NULL;
	int isLeft;
	KSI_CTX *ctx = NULL;

	if (tlv == NULL || link == NULL) {
		res = KSI_INVALID_ARGUMENT;
		goto cleanup;
	}
	ctx = KSI_TLV_getCtx(tlv);
	KSI_ERR_clearErrors(ctx);

	switch (KSI_TLV_getTag(tlv)) {
		case 0x07: isLeft = 1; break;
		case 0x08: isLeft = 0; break;
		default: {
			char errm[0xff];
			KSI_snprintf(errm, sizeof(errm), "Unknown tag for hash chain link: 0x%02x", KSI_TLV_getTag(tlv));
			KSI_pushError(ctx, res = KSI_INVALID_FORMAT, errm);
			goto cleanup;
		}
	}

	res = KSI_HashChainLink_new(ctx, &tmp);
	if (res != KSI_OK) {
		KSI_pushError(ctx, res, NULL);
		goto cleanup;
	}

	res = KSI_TlvTemplate_extract(ctx, tmp, tlv, KSI_TLV_TEMPLATE(KSI_HashChainLink));
	if (res != KSI_OK) {
		KSI_pushError(ctx, res, NULL);
		goto cleanup;
	}

	tmp->isLeft = isLeft;

	*link = tmp;
	tmp = NULL;

	res = KSI_OK;

cleanup:

	KSI_HashChainLink_free(tmp);

	return res;
}


int KSI_HashChainLink_toTlv(KSI_CTX *ctx, KSI_HashChainLink *link, unsigned tag, int isNonCritica, int isForward, KSI_TLV **tlv) {
	int res = KSI_UNKNOWN_ERROR;
	KSI_TLV *tmp = NULL;
	unsigned tagOverride = 0;

	KSI_ERR_clearErrors(ctx);
	if (ctx == NULL || link == NULL || tlv == NULL) {
		KSI_pushError(ctx, res = KSI_INVALID_ARGUMENT, NULL);
		goto cleanup;
	}

	if (link->isLeft) {
		tagOverride = 0x07;
	} else {
		tagOverride = 0x08;
	}

	res = KSI_TLV_new(ctx, KSI_TLV_PAYLOAD_TLV, tagOverride, isNonCritica, isForward, &tmp);
	if (res != KSI_OK) {
		KSI_pushError(ctx, res, NULL);
		goto cleanup;
	}

	res = KSI_TlvTemplate_construct(ctx, tmp, link, KSI_TLV_TEMPLATE(KSI_HashChainLink));
	if (res != KSI_OK) {
		KSI_pushError(ctx, res, NULL);
		goto cleanup;
	}

	*tlv = tmp;
	tmp = NULL;

	res = KSI_OK;

cleanup:

	KSI_TLV_free(tmp);

	return res;
}


KSI_IMPLEMENT_GETTER(KSI_HashChainLink, int, isLeft, IsLeft);
KSI_IMPLEMENT_GETTER(KSI_HashChainLink, KSI_Integer*, levelCorrection, LevelCorrection);
KSI_IMPLEMENT_GETTER(KSI_HashChainLink, KSI_DataHash*, metaHash, MetaHash);
KSI_IMPLEMENT_GETTER(KSI_HashChainLink, KSI_MetaData*, metaData, MetaData);
KSI_IMPLEMENT_GETTER(KSI_HashChainLink, KSI_DataHash*, imprint, Imprint);

KSI_IMPLEMENT_SETTER(KSI_HashChainLink, int, isLeft, IsLeft);
KSI_IMPLEMENT_SETTER(KSI_HashChainLink, KSI_Integer*, levelCorrection, LevelCorrection);
KSI_IMPLEMENT_SETTER(KSI_HashChainLink, KSI_DataHash*, metaHash, MetaHash);
KSI_IMPLEMENT_SETTER(KSI_HashChainLink, KSI_MetaData*, metaData, MetaData);
KSI_IMPLEMENT_SETTER(KSI_HashChainLink, KSI_DataHash*, imprint, Imprint);

