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
#include "verification_impl.h"

int KSI_VerificationResult_reset(KSI_VerificationResult *info) {
	int res = KSI_UNKNOWN_ERROR;

	if (info == NULL) {
		res = KSI_INVALID_ARGUMENT;
		goto cleanup;
	}

	info->stepsFailed = 0;
	info->stepsPerformed = 0;

	info->verifyDocumentHash = false;
	info->documentHash = NULL;
	info->docAggrLevel = 0;

	info->useUserPublication = false;
	info->userPublication = NULL;

	info->publicationsFile = NULL;

	info->steps_len = 0;

	KSI_DataHash_free(info->aggregationHash);
	info->aggregationHash = NULL;

	res = KSI_OK;

cleanup:

	return res;
}

int KSI_VerificationResult_init(KSI_VerificationResult *info, KSI_CTX *ctx) {
	int res = KSI_UNKNOWN_ERROR;
	if (info == NULL) {
		res = KSI_INVALID_ARGUMENT;
		goto cleanup;
	}

	info->ctx = ctx;
	info->aggregationHash = NULL;
	res = KSI_VerificationResult_reset(info);
	if (res != KSI_OK) goto cleanup;

	res = KSI_OK;

cleanup:

	return res;
}


static int addVerificationStepResult(KSI_VerificationResult *info, KSI_VerificationStep step, const char *desc, bool succeeded) {
	int res = KSI_UNKNOWN_ERROR;
	KSI_VerificationStepResult *result = NULL;
	if (info == NULL) {
		res = KSI_INVALID_ARGUMENT;
		goto cleanup;
	}

	if (!succeeded) {
		info->stepsFailed |= step;
	}
	info->stepsPerformed |= step;
	result = info->steps + info->steps_len++;
	result->step = step;
	result->succeeded = succeeded;
	result->description[0] = '\0';
	if (desc != NULL) {
		KSI_strncpy(result->description, desc, sizeof(result->description));
	}

	res = KSI_OK;

cleanup:

	return res;
}

int KSI_VerificationResult_addFailure(KSI_VerificationResult *info, KSI_VerificationStep step, const char *desc) {
	KSI_LOG_debug(info->ctx, "Verification step 0x%02x failed with message: %s", (int)step, desc);
	return addVerificationStepResult(info, step, desc, 0);
}

int KSI_VerificationResult_addSuccess(KSI_VerificationResult *info, KSI_VerificationStep step, const char *desc) {
	return addVerificationStepResult(info, step, desc, 1);
}

size_t KSI_VerificationResult_getStepResultCount(const KSI_VerificationResult *info) {
	return info == NULL ? 0 : info->steps_len;
}

int KSI_VerificationResult_getStepResult(const KSI_VerificationResult *info, size_t index, const KSI_VerificationStepResult **result) {
	int res = KSI_UNKNOWN_ERROR;
	if (info == NULL || index > info->steps_len || result == NULL) {
		res = KSI_INVALID_ARGUMENT;
		goto cleanup;
	}

	*result = info->steps + index;

	res = KSI_OK;

cleanup:

	return res;
}

int KSI_VerificationResult_isStepPerformed(const KSI_VerificationResult *info, enum KSI_VerificationStep_en step) {
	return info != NULL && (info->stepsPerformed & step);
}

int KSI_VerificationResult_isStepSuccess(const KSI_VerificationResult *info, enum KSI_VerificationStep_en step) {
	return KSI_VerificationResult_isStepPerformed(info, step) && (info->stepsFailed & step) == 0;
}

int KSI_VerificationStepResult_getStep(const KSI_VerificationStepResult *result) {
	return result == NULL ? 0 : result->step;
}

const char *KSI_VerificationStepResult_getDescription(const KSI_VerificationStepResult *result) {
	return result == NULL ? "null" : result->description;
}

int KSI_VerificationStepResult_isSuccess(const KSI_VerificationStepResult *result) {
	return result != NULL && result->succeeded;
}

const char *KSI_VerificationResult_lastFailureMessage(const KSI_VerificationResult *info) {
	const char *msg = NULL;
	size_t i;
	if (info == NULL) goto cleanup;
	if (info->steps_len == 0) goto cleanup;
	if (info->stepsFailed == 0) goto cleanup;
	for (i = info->steps_len; i > 0; i--) {
		if (!info->steps[i - 1].succeeded) {
			msg = info->steps[i - 1].description;
			goto cleanup;
		}
	}

cleanup:

	return msg;
}

