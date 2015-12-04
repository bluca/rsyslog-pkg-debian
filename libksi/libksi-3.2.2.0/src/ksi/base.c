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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"
#include "net_http.h"
#include "net_uri.h"
#include "ctx_impl.h"
#include "pkitruststore.h"

KSI_IMPLEMENT_LIST(GlobalCleanupFn, NULL);

#ifdef COMMIT_ID
#  define KSI_VERSION_STRING "libksi " VERSION "-" COMMIT_ID
#else
#  define KSI_VERSION_STRING "libksi " VERSION
#endif

const char *KSI_getVersion(void) {
	static const char versionString[] = KSI_VERSION_STRING;
	return versionString;
}

const char *KSI_getErrorString(int statusCode) {
	switch (statusCode) {
		case KSI_OK:
			return "No errors.";
		case KSI_INVALID_ARGUMENT:
			return "Invalid argument.";
		case KSI_INVALID_FORMAT:
			return "Invalid format.";
		case KSI_UNTRUSTED_HASH_ALGORITHM:
			return "The hash algorithm is not trusted.";
		case KSI_UNAVAILABLE_HASH_ALGORITHM:
			return "The hash algorithm is not implemented or unavailable.";
		case KSI_BUFFER_OVERFLOW:
			return "Buffer overflow.";
		case KSI_TLV_PAYLOAD_TYPE_MISMATCH:
			return "TLV payload type mismatch.";
		case KSI_ASYNC_NOT_FINISHED:
			return "Asynchronous call not yet finished.";
		case KSI_INVALID_SIGNATURE:
			return "Invalid KSI signature.";
		case KSI_INVALID_PKI_SIGNATURE:
			return "Invalid PKI signature.";
		case KSI_PKI_CERTIFICATE_NOT_TRUSTED:
			return "The PKI certificate is not trusted.";
		case KSI_OUT_OF_MEMORY:
			return "Out of memory.";
		case KSI_IO_ERROR:
			return "I/O error.";
		case KSI_NETWORK_ERROR:
			return "Network error.";
		case KSI_NETWORK_CONNECTION_TIMEOUT:
			return "Network connection timeout.";
		case KSI_NETWORK_SEND_TIMEOUT:
			return "Network send timeout.";
		case KSI_NETWORK_RECIEVE_TIMEOUT:
			return "Network receive timeout.";
		case KSI_HTTP_ERROR:
			return "HTTP error.";
		case KSI_SERVICE_UNKNOWN_ERROR:
			return "Unknown service error.";
		case KSI_EXTEND_WRONG_CAL_CHAIN:
			return "The given calendar chain is not a continuation of the signature calendar chain.";
		case KSI_EXTEND_NO_SUITABLE_PUBLICATION:
			return "There is no suitable publication yet.";
		case KSI_VERIFICATION_FAILURE:
			return "Verification failed.";
		case KSI_INVALID_PUBLICATION:
			return "Invalid publication.";
		case KSI_PUBLICATIONS_FILE_NOT_SIGNED_WITH_PKI:
			return "The publications file is not signed.";
		case KSI_CRYPTO_FAILURE:
			return "Cryptographic failure.";
		case KSI_HMAC_MISMATCH:
			return "HMAC mismatch.";
		case KSI_SERVICE_INVALID_REQUEST:
			return "The request had invalid format.";
		case KSI_SERVICE_AUTHENTICATION_FAILURE:
			return "The request could not be authenticated.";
		case KSI_SERVICE_INVALID_PAYLOAD:
			return "The request contained invalid payload.";
		case KSI_SERVICE_INTERNAL_ERROR:
			return "The server encountered an unspecified internal error.";
		case KSI_SERVICE_UPSTREAM_ERROR:
			return "The server encountered unspecified critical errors connecting to upstream servers.";
		case KSI_SERVICE_UPSTREAM_TIMEOUT:
			return "No response from upstream servers";
		case KSI_SERVICE_AGGR_REQUEST_TOO_LARGE:
			return "The request indicated client-side aggregation tree larger than allowed for the client.";
		case KSI_SERVICE_AGGR_REQUEST_OVER_QUOTA:
			return "The request combined with other requests from the same client in the same round would create an aggregation sub-tree larger than allowed for the client.";
		case KSI_SERVICE_EXTENDER_INVALID_TIME_RANGE:
			return "The request asked for a hash chain going backwards in time.";
		case KSI_SERVICE_EXTENDER_DATABASE_MISSING:
			return "The server misses the internal database needed to service the request.";
		case KSI_SERVICE_EXTENDER_DATABASE_CORRUPT:
			return "The server's internal database is in an inconsistent state.";
		case KSI_SERVICE_EXTENDER_REQUEST_TIME_TOO_OLD:
			return "The request asked for hash values older than the oldest round in the server's database.";
		case KSI_SERVICE_EXTENDER_REQUEST_TIME_TOO_NEW:
			return "The request asked for hash values newer than the newest round in the server's database.";
		case KSI_SERVICE_EXTENDER_REQUEST_TIME_IN_FUTURE:
			return "The request asked for hash values newer than the current real time";
		case KSI_UNKNOWN_ERROR:
			return "Unknown internal error.";
		default:
			return "Unknown status code.";
	}
}

int KSI_CTX_new(KSI_CTX **context) {
	int res = KSI_UNKNOWN_ERROR;

	KSI_CTX *ctx = NULL;
	KSI_UriClient *client = NULL;
	KSI_PKITruststore *pkiTruststore = NULL;

	ctx = KSI_new(KSI_CTX);
	if (ctx == NULL) {
		res = KSI_OUT_OF_MEMORY;
		goto cleanup;
	}
	/* Init error stack */
	ctx->errors_size = KSI_ERR_STACK_LEN;
	ctx->errors = KSI_malloc(sizeof(KSI_ERR) * ctx->errors_size);
	if (ctx->errors == NULL) {
		res = KSI_OUT_OF_MEMORY;
		goto cleanup;
	}
	ctx->errors_count = 0;
	ctx->publicationsFile = NULL;
	ctx->pkiTruststore = NULL;
	ctx->netProvider = NULL;
	ctx->publicationCertEmail = NULL;
	ctx->loggerCB = NULL;
	ctx->requestHeaderCB = NULL;
	ctx->loggerCtx = NULL;
	ctx->requestCounter = 0;
	KSI_ERR_clearErrors(ctx);

	/* Create global cleanup list as the first thing. */
	res = KSI_List_new(NULL, &ctx->cleanupFnList);
	if (res != KSI_OK) goto cleanup;

	/* Create and set the logger. */
	res = KSI_CTX_setLoggerCallback(ctx, KSI_LOG_StreamLogger, stdout);
	if (res != KSI_OK) goto cleanup;

	res = KSI_CTX_setLogLevel(ctx, KSI_LOG_NONE);
	if (res != KSI_OK) goto cleanup;

	res = KSI_UriClient_new(ctx, &client);
	if (res != KSI_OK) goto cleanup;

	res = KSI_CTX_setNetworkProvider(ctx, (KSI_NetworkClient *)client);
	if (res != KSI_OK) goto cleanup;
	ctx->isCustomNetProvider = 0;
	client = NULL;

	/* Create and set the PKI truststore */
	res = KSI_PKITruststore_new(ctx, 1, &pkiTruststore);
	if (res != KSI_OK) goto cleanup;
	res = KSI_CTX_setPKITruststore(ctx, pkiTruststore);
	if (res != KSI_OK) goto cleanup;
	pkiTruststore = NULL;

	res = KSI_CTX_setPublicationCertEmail(ctx, "publications@guardtime.com");
	if (res != KSI_OK) goto cleanup;

	/* Return the context. */
	*context = ctx;
	ctx = NULL;

	res = KSI_OK;

cleanup:

	KSI_UriClient_free(client);
	KSI_PKITruststore_free(pkiTruststore);

	KSI_CTX_free(ctx);

	return res;
}

int KSI_CTX_registerGlobals(KSI_CTX *ctx, int (*initFn)(void), void (*cleanupFn)(void)) {
	int res = KSI_UNKNOWN_ERROR;
	size_t *pos = NULL;

	if (ctx == NULL || initFn == NULL || cleanupFn == NULL) {
		res = KSI_INVALID_ARGUMENT;
		goto cleanup;
	}

	res = KSI_List_indexOf(ctx->cleanupFnList, (void *)cleanupFn, &pos);
	if (res != KSI_OK) goto cleanup;

	/* Only run the init function if the cleanup function is not found. */
	if (pos == NULL) {
		res = initFn();
		if (res != KSI_OK) goto cleanup;

		res = KSI_List_append(ctx->cleanupFnList, (void *)cleanupFn);
		if (res != KSI_OK) goto cleanup;
	}

	res = KSI_OK;

cleanup:

	KSI_free(pos);

	return res;
}

static void globalCleanup(KSI_CTX *ctx) {
	int res;
	size_t pos;
	void (*fn)(void);

	for (pos = 0; pos < KSI_List_length(ctx->cleanupFnList); pos++) {
		res = KSI_List_elementAt(ctx->cleanupFnList, pos, (void **)&fn);
		if (res != KSI_OK) {
			KSI_LOG_error(ctx, "Unable to retrieve cleanup function.");
			break;
		}

		if (fn == NULL) {
			KSI_LOG_error(ctx, "Got NULL as global cleanup method.");
			break;
		}

		fn();
	}
}

/**
 *
 */
void KSI_CTX_free(KSI_CTX *ctx) {
	if (ctx != NULL) {
		/* Call cleanup methods. */
		globalCleanup(ctx);

		KSI_List_free(ctx->cleanupFnList);

		KSI_free(ctx->errors);

		KSI_NetworkClient_free(ctx->netProvider);
		KSI_PKITruststore_free(ctx->pkiTruststore);

		KSI_PublicationsFile_free(ctx->publicationsFile);
		KSI_free(ctx->publicationCertEmail);

		KSI_free(ctx);
	}
}

int KSI_sendSignRequest(KSI_CTX *ctx, KSI_AggregationReq *request, KSI_RequestHandle **handle) {
	int res = KSI_UNKNOWN_ERROR;
	KSI_RequestHandle *tmp = NULL;
	KSI_NetworkClient *netProvider = NULL;

	KSI_ERR_clearErrors(ctx);
	if (ctx == NULL || request == NULL || handle == NULL) {
		KSI_pushError(ctx, res = KSI_INVALID_ARGUMENT, NULL);
		goto cleanup;
	}

	netProvider = ctx->netProvider;

	res = KSI_NetworkClient_sendSignRequest(netProvider, request, &tmp);
	if (res != KSI_OK) {
		KSI_pushError(ctx, res, NULL);
		goto cleanup;
	}

	*handle = tmp;
	tmp = NULL;

	res = KSI_OK;

cleanup:

	KSI_RequestHandle_free(tmp);

	return res;
}

int KSI_sendExtendRequest(KSI_CTX *ctx, KSI_ExtendReq *request, KSI_RequestHandle **handle) {
	int res = KSI_UNKNOWN_ERROR;
	KSI_RequestHandle *tmp = NULL;
	KSI_NetworkClient *netProvider = NULL;

	KSI_ERR_clearErrors(ctx);
	if (ctx == NULL || request == NULL || handle == NULL) {
		KSI_pushError(ctx, res = KSI_INVALID_ARGUMENT, NULL);
		goto cleanup;
	}

	netProvider = ctx->netProvider;

	res = KSI_NetworkClient_sendExtendRequest(netProvider, request, &tmp);
	if (res != KSI_OK) {
		KSI_pushError(ctx,res, NULL);
		goto cleanup;
	}

	*handle = tmp;
	tmp = NULL;

	res = KSI_OK;

cleanup:

	KSI_RequestHandle_free(tmp);

	return res;
}

int KSI_sendPublicationRequest(KSI_CTX *ctx, const unsigned char *request, unsigned request_length, KSI_RequestHandle **handle) {
	int res = KSI_UNKNOWN_ERROR;
	KSI_NetworkClient *netProvider = NULL;

	KSI_ERR_clearErrors(ctx);
	if (ctx == NULL || handle == NULL) {
		KSI_pushError(ctx, res = KSI_INVALID_ARGUMENT, NULL);
		goto cleanup;
	}

	netProvider = ctx->netProvider;

	res = KSI_NetworkClient_sendPublicationsFileRequest(netProvider, handle);
	if (res != KSI_OK) {
		KSI_pushError(ctx,res, NULL);
		goto cleanup;
	}

	res = KSI_OK;

cleanup:

	return res;

}

int KSI_receivePublicationsFile(KSI_CTX *ctx, KSI_PublicationsFile **pubFile) {
	int res = KSI_UNKNOWN_ERROR;
	KSI_RequestHandle *handle = NULL;
	const unsigned char *raw = NULL;
	unsigned raw_len = 0;
	KSI_PublicationsFile *tmp = NULL;

	KSI_ERR_clearErrors(ctx);
	if (ctx == NULL || pubFile == NULL) {
		KSI_pushError(ctx, res = KSI_INVALID_ARGUMENT, NULL);
		goto cleanup;
	}

	/* TODO! Implement mechanism for reloading (e.g cache timeout) */
	if (ctx->publicationsFile == NULL) {
		KSI_LOG_debug(ctx, "Receiving publications file.");

		res = KSI_sendPublicationRequest(ctx, NULL, 0, &handle);
		if (res != KSI_OK) {
			KSI_pushError(ctx,res, NULL);
			goto cleanup;
		}

		res = KSI_RequestHandle_getResponse(handle, &raw, &raw_len);
		if (res != KSI_OK) {
			KSI_pushError(ctx,res, NULL);
			goto cleanup;
		}

		res = KSI_PublicationsFile_parse(ctx, raw, raw_len, &tmp);
		if (res != KSI_OK) {
			KSI_pushError(ctx,res, NULL);
			goto cleanup;
		}

		ctx->publicationsFile = tmp;
		tmp = NULL;

		KSI_LOG_debug(ctx, "Publications file received.");
	}

	*pubFile = ctx->publicationsFile;

	res = KSI_OK;

cleanup:

	KSI_RequestHandle_free(handle);
	KSI_PublicationsFile_free(tmp);

	return res;

}

int KSI_verifyPublicationsFile(KSI_CTX *ctx, KSI_PublicationsFile *pubFile) {
	int res = KSI_UNKNOWN_ERROR;

	KSI_ERR_clearErrors(ctx);
	if (ctx == NULL || pubFile == NULL) {
		KSI_pushError(ctx, res = KSI_INVALID_ARGUMENT, NULL);
		goto cleanup;
	}

	res = KSI_PublicationsFile_verify(pubFile, ctx);
	if (res != KSI_OK) {
		KSI_pushError(ctx,res, NULL);
		goto cleanup;
	}

	res = KSI_OK;

cleanup:

	return res;
}

int KSI_verifySignature(KSI_CTX *ctx, KSI_Signature *sig) {
	int res = KSI_UNKNOWN_ERROR;

	KSI_ERR_clearErrors(ctx);
	if (ctx == NULL || sig == NULL) {
		KSI_pushError(ctx, res = KSI_INVALID_ARGUMENT, NULL);
		goto cleanup;
	}

	res = KSI_Signature_verify(sig, ctx);
	if (res != KSI_OK) {
		KSI_pushError(ctx,res, NULL);
		goto cleanup;
	}

	res = KSI_OK;

cleanup:

	return res;
}

int KSI_createSignature(KSI_CTX *ctx, KSI_DataHash *dataHash, KSI_Signature **sig) {
	int res = KSI_UNKNOWN_ERROR;
	KSI_Signature *tmp = NULL;

	KSI_ERR_clearErrors(ctx);
	if (ctx == NULL || dataHash == NULL || sig == NULL) {
		KSI_pushError(ctx, res = KSI_INVALID_ARGUMENT, NULL);
		goto cleanup;
	}

	res = KSI_Signature_create(ctx, dataHash, &tmp);
	if (res != KSI_OK) {
		KSI_pushError(ctx,res, NULL);
		goto cleanup;
	}

	*sig = tmp;
	tmp = NULL;

	res = KSI_OK;

cleanup:

	KSI_Signature_free(tmp);

	return res;
}

int KSI_extendSignature(KSI_CTX *ctx, KSI_Signature *sig, KSI_Signature **extended) {
	int res = KSI_UNKNOWN_ERROR;
	KSI_PublicationsFile *pubFile = NULL;
	KSI_Integer *signingTime = NULL;
	KSI_PublicationRecord *pubRec = NULL;
	KSI_Signature *extSig = NULL;

	KSI_ERR_clearErrors(ctx);
	if (ctx == NULL || sig == NULL || extended == NULL) {
		KSI_pushError(ctx, res = KSI_INVALID_ARGUMENT, NULL);
		goto cleanup;
	}

	res = KSI_receivePublicationsFile(ctx, &pubFile);
	if (res != KSI_OK) {
		KSI_pushError(ctx,res, NULL);
		goto cleanup;
	}


	res = KSI_Signature_getSigningTime(sig, &signingTime);
	if (res != KSI_OK) {
		KSI_pushError(ctx,res, NULL);
		goto cleanup;
	}


	res = KSI_PublicationsFile_getNearestPublication(pubFile, signingTime, &pubRec);
	if (res != KSI_OK) {
		KSI_pushError(ctx,res, NULL);
		goto cleanup;
	}

	if (pubRec == NULL) {
		KSI_pushError(ctx, res = KSI_EXTEND_NO_SUITABLE_PUBLICATION, NULL);
		goto cleanup;
	}

	res = KSI_Signature_extend(sig, ctx, pubRec, &extSig);
	if (res != KSI_OK) {
		KSI_pushError(ctx,res, NULL);
		goto cleanup;
	}


	*extended = extSig;
	extSig = NULL;

	res = KSI_OK;

cleanup:

	KSI_Signature_free(extSig);
	return res;
}

int KSI_extendSignatureToPublication(KSI_CTX *ctx, KSI_Signature *sig, char *pubString, KSI_Signature **extended) {
	int res = KSI_UNKNOWN_ERROR;
	KSI_PublicationsFile *pubFile = NULL;
	KSI_Integer *signingTime = NULL;
	KSI_PublicationRecord *pubRec = NULL;
	KSI_Signature *extSig = NULL;

	KSI_ERR_clearErrors(ctx);
	if (ctx == NULL || sig == NULL || pubString == NULL || extended == NULL) {
		KSI_pushError(ctx, res = KSI_INVALID_ARGUMENT, NULL);
		goto cleanup;
	}

	res = KSI_receivePublicationsFile(ctx, &pubFile);
	if (res != KSI_OK) {
		KSI_pushError(ctx,res, NULL);
		goto cleanup;
	}


	res = KSI_Signature_getSigningTime(sig, &signingTime);
	if (res != KSI_OK) {
		KSI_pushError(ctx,res, NULL);
		goto cleanup;
	}


	res = KSI_PublicationsFile_getNearestPublication(pubFile, signingTime, &pubRec);
	if (res != KSI_OK) {
		KSI_pushError(ctx,res, NULL);
		goto cleanup;
	}


	if (pubRec == NULL) {
		KSI_pushError(ctx, res = KSI_EXTEND_NO_SUITABLE_PUBLICATION, NULL);
		goto cleanup;
	}

	res = KSI_Signature_extend(sig, ctx, pubRec, &extSig);
	if (res != KSI_OK) {
		KSI_pushError(ctx,res, NULL);
		goto cleanup;
	}


	*extended = extSig;
	extSig = NULL;

	res = KSI_OK;

cleanup:

	KSI_Signature_free(extSig);

	return res;
}

int KSI_CTX_setLogLevel(KSI_CTX *ctx, int level) {
	int res = KSI_UNKNOWN_ERROR;

	KSI_ERR_clearErrors(ctx);
	if (ctx == NULL) {
		KSI_pushError(ctx, res = KSI_INVALID_ARGUMENT, NULL);
		goto cleanup;
	}

	ctx->logLevel = level;

	res = KSI_OK;

cleanup:

	return res;
}

int KSI_ERR_init(KSI_CTX *ctx, KSI_ERR *err) {
	err->ctx = ctx;

	KSI_ERR_fail(err, KSI_UNKNOWN_ERROR, 0, "null", 0, "Internal error: Probably a function returned without a distinctive success or error.");

	return KSI_OK;
}

int KSI_ERR_apply(KSI_ERR *err) {
	KSI_CTX *ctx = err->ctx;
	KSI_ERR *ctxErr = NULL;

	if (ctx != NULL) {
		if (err->statusCode != KSI_OK) {
			ctxErr = &ctx->errors[ctx->errors_count % ctx->errors_size];

			ctxErr->statusCode = err->statusCode;
			ctxErr->extErrorCode = err->extErrorCode;
			ctxErr->lineNr = err->lineNr;
			KSI_strncpy(ctxErr->fileName, KSI_strnvl(err->fileName), sizeof(err->fileName));
			KSI_strncpy(ctxErr->message, KSI_strnvl(err->message), sizeof(err->message));

			ctx->errors_count++;
		}
	}
	/* Return the result, which does not indicate the result of this method. */
	return err->statusCode;
}

void KSI_ERR_push(KSI_CTX *ctx, int statusCode, long extErrorCode, const char *fileName, unsigned int lineNr, const char *message) {
	KSI_ERR *ctxErr = NULL;
	const char *tmp = NULL;

	/* Do nothing if the context is missing. */
	if (ctx == NULL) return;

	/* Do nothing if there's no error. */
	if (statusCode == KSI_OK) return;

	/* Get the error container to use for storage. */
	ctxErr = &ctx->errors[ctx->errors_count % ctx->errors_size];

	ctxErr->statusCode = statusCode;
	ctxErr->extErrorCode = extErrorCode;
	ctxErr->lineNr = lineNr;
	tmp = KSI_strnvl(fileName);
	KSI_strncpy(ctxErr->fileName, tmp, sizeof(ctxErr->fileName));
	tmp = KSI_strnvl(message);
	KSI_strncpy(ctxErr->message, tmp, sizeof(ctxErr->message));

	ctx->errors_count++;
}

void KSI_ERR_success(KSI_ERR *err) {
	err->statusCode = KSI_OK;
	*err->message = '\0';
}

int KSI_ERR_pre(KSI_ERR *err, int cond, char *fileName, int lineNr) {
	if (!cond) {
		KSI_ERR_init(NULL, err);
		KSI_ERR_fail(err, KSI_INVALID_ARGUMENT, 0, fileName, lineNr, NULL);
	}

	return !cond;
}

int KSI_ERR_fail(KSI_ERR *err, int statusCode, long extErrorCode, char *fileName, unsigned int lineNr, const char *message) {
	if (statusCode != KSI_OK) {
		err->extErrorCode = extErrorCode;
		err->statusCode = statusCode;
		if (message == NULL) {
			KSI_strncpy(err->message, KSI_getErrorString(statusCode), sizeof(err->message));
		} else {
			KSI_strncpy(err->message, KSI_strnvl(message), sizeof(err->message));
		}
		KSI_strncpy(err->fileName, KSI_strnvl(fileName), sizeof(err->fileName));
		err->lineNr = lineNr;
	}
	return statusCode;
}

void KSI_ERR_clearErrors(KSI_CTX *ctx) {
	if (ctx != NULL) {
		ctx->errors_count = 0;
	}
}

int KSI_ERR_statusDump(KSI_CTX *ctx, FILE *f) {
	int res = KSI_UNKNOWN_ERROR;
	KSI_ERR *err = NULL;
	unsigned int i;

	fprintf(f, "KSI error trace:\n");
	if (ctx->errors_count == 0) {
		printf("  No errors.\n");
		goto cleanup;
	}

	/* List all errors, starting from the most general. */
	for (i = 0; i < ctx->errors_count && i < ctx->errors_size; i++) {
		err = ctx->errors + ((ctx->errors_count - i - 1) % ctx->errors_size);
		fprintf(f, "  %3lu) %s:%u - (%d/%ld) %s\n", ctx->errors_count - i, err->fileName, err->lineNr,err->statusCode, err->extErrorCode, err->message);
	}

	/* If there where more errors than buffers for the errors, indicate the fact */
	if (ctx->errors_count > ctx->errors_size) {
		fprintf(f, "  ... (more errors)\n");
	}

	res = KSI_OK;

cleanup:

	return res;
}

int KSI_ERR_getBaseErrorMessage(KSI_CTX *ctx, char *buf, unsigned len, int *error, int *ext){
	KSI_ERR *err = NULL;

	if (ctx == NULL || buf == NULL){
		return KSI_INVALID_ARGUMENT;
	}

	err = ctx->errors;

	if (error != NULL)	*error = err->statusCode;
	if (ext != NULL)	*ext = err->extErrorCode;

	if(ctx->errors_count)
		KSI_strncpy(buf, err->message, len);
	else
		KSI_strncpy(buf, "", len);

	return KSI_OK;
}

void *KSI_malloc(size_t size) {
	return malloc(size);
}

void *KSI_calloc(size_t num, size_t size) {
	return calloc(num, size);
}

void KSI_free(void *ptr) {
	free(ptr);
}

static int KSI_CTX_setUri(KSI_CTX *ctx,
		const char *uri, const char *loginId, const char *key,
		int (*setter)(KSI_UriClient*, const char*, const char *, const char *)){
	int res = KSI_UNKNOWN_ERROR;
	KSI_UriClient *client = NULL;

	KSI_ERR_clearErrors(ctx);
	if (ctx == NULL || uri == NULL) {
		KSI_pushError(ctx, res = KSI_INVALID_ARGUMENT, NULL);
		goto cleanup;
	}

	if (ctx->isCustomNetProvider){
		KSI_pushError(ctx, res = KSI_INVALID_ARGUMENT, "Unable to set url after initial network provider replacement.");
		goto cleanup;
	}

	client = (KSI_UriClient*)ctx->netProvider;

	res = setter(client, uri, loginId, key);
	if (res != KSI_OK) {
		KSI_pushError(ctx,res, NULL);
		goto cleanup;
	}

	res = KSI_OK;

cleanup:

	return res;
}

static int KSI_UriClient_setPublicationUrl_wrapper(KSI_UriClient *client, const char *uri, const char *not_used_1, const char *not_used_2){
	return KSI_UriClient_setPublicationUrl(client, uri);
}

int KSI_CTX_setAggregator(KSI_CTX *ctx, const char *uri, const char *loginId, const char *key){
	return KSI_CTX_setUri(ctx, uri, loginId, key, KSI_UriClient_setAggregator);
}

int KSI_CTX_setExtender(KSI_CTX *ctx, const char *uri, const char *loginId, const char *key){
	return KSI_CTX_setUri(ctx, uri, loginId, key, KSI_UriClient_setExtender);
}

int KSI_CTX_setPublicationUrl(KSI_CTX *ctx, const char *uri){
	return KSI_CTX_setUri(ctx, uri, uri, uri, KSI_UriClient_setPublicationUrl_wrapper);
}

static int KSI_CTX_setTimeoutSeconds(KSI_CTX *ctx, int timeout, int (*setter)(KSI_UriClient*, int)){
	int res = KSI_UNKNOWN_ERROR;
	KSI_UriClient *client = NULL;

	KSI_ERR_clearErrors(ctx);
	if (ctx == NULL || ctx->netProvider == NULL) {
		KSI_pushError(ctx, res = KSI_INVALID_ARGUMENT, NULL);
		goto cleanup;
	}

	if (ctx->isCustomNetProvider){
		KSI_pushError(ctx, res = KSI_INVALID_ARGUMENT, "Unable to set timeout after initial network provider replacement.");
		goto cleanup;
	}

	client = (KSI_UriClient*)ctx->netProvider;

	res = setter(client, timeout);
	if (res != KSI_OK) {
		KSI_pushError(ctx,res, NULL);
		goto cleanup;
	}

	res = KSI_OK;

cleanup:

	return res;
}

int KSI_CTX_setConnectionTimeoutSeconds(KSI_CTX *ctx, int timeout){
	return KSI_CTX_setTimeoutSeconds(ctx, timeout, KSI_UriClient_setConnectionTimeoutSeconds);
}

int KSI_CTX_setTransferTimeoutSeconds(KSI_CTX *ctx, int timeout){
	return KSI_CTX_setTimeoutSeconds(ctx, timeout, KSI_UriClient_setTransferTimeoutSeconds);
}

#define CTX_VALUEP_SETTER(var, nam, typ, fre)												\
int KSI_CTX_set##nam(KSI_CTX *ctx, typ *var) { 												\
	int res = KSI_UNKNOWN_ERROR;															\
	if (ctx == NULL) {																		\
		res = KSI_INVALID_ARGUMENT;															\
		goto cleanup;																		\
	}																						\
	if (ctx->var != NULL) {																	\
		fre(ctx->var);																		\
	}																						\
	ctx->var = var;																			\
	res = KSI_OK;																			\
cleanup:																					\
	return res;																				\
} 																							\

#define CTX_VALUEP_GETTER(var, nam, typ) 													\
int KSI_CTX_get##nam(KSI_CTX *ctx, typ **var) { 											\
	int res = KSI_UNKNOWN_ERROR;															\
	if (ctx == NULL || var == NULL) {														\
		res = KSI_INVALID_ARGUMENT;															\
		goto cleanup;																		\
	}																						\
	*var = ctx->var;																		\
	res = KSI_OK;																			\
cleanup:																					\
	return res;																				\
} 																							\

#define CTX_GET_SET_VALUE(var, nam, typ, fre) 												\
	CTX_VALUEP_SETTER(var, nam, typ, fre)													\
	CTX_VALUEP_GETTER(var, nam, typ)														\

CTX_GET_SET_VALUE(pkiTruststore, PKITruststore, KSI_PKITruststore, KSI_PKITruststore_free)
CTX_GET_SET_VALUE(publicationsFile, PublicationsFile, KSI_PublicationsFile, KSI_PublicationsFile_free)

int KSI_CTX_setNetworkProvider(KSI_CTX *ctx, KSI_NetworkClient *netProvider){
    int res = KSI_UNKNOWN_ERROR;

	if (ctx == NULL){
        res = KSI_INVALID_ARGUMENT;
        goto cleanup;
    }

	if (ctx->netProvider != NULL) {
        KSI_NetworkClient_free (ctx->netProvider);
    }

	ctx->netProvider = netProvider;
    ctx->isCustomNetProvider = 1;
	res = KSI_OK;

cleanup:
	return res;
}

int KSI_CTX_setRequestHeaderCallback(KSI_CTX *ctx, KSI_RequestHeaderCallback cb) {
	int res = KSI_UNKNOWN_ERROR;

	if (ctx == NULL) {
		res = KSI_INVALID_ARGUMENT;
		goto cleanup;
	}

	ctx->requestHeaderCB = cb;

	res = KSI_OK;

cleanup:

	return res;
}

int KSI_CTX_setLoggerCallback(KSI_CTX *ctx, KSI_LoggerCallback cb, void *logCtx) {
	int res = KSI_UNKNOWN_ERROR;
	if (ctx == NULL) {
		res = KSI_INVALID_ARGUMENT;
		goto cleanup;
	}

	ctx->loggerCB = cb;
	ctx->loggerCtx = logCtx;

	res = KSI_OK;

cleanup:

	return res;
}


int KSI_CTX_setPublicationCertEmail(KSI_CTX *ctx, const char *email) {
	int res = KSI_UNKNOWN_ERROR;
	char *tmp = NULL;
	if (ctx == NULL) {
		res = KSI_INVALID_ARGUMENT;
		goto cleanup;
	}

	if (email != NULL && email[0] != '\0') {
		size_t len = strlen(email);
		tmp = KSI_calloc(len + 1, 1);
		if (tmp == NULL) {
			res = KSI_OUT_OF_MEMORY;
			goto cleanup;
		}

		memcpy(tmp, email, len + 1);
	}

	KSI_free(ctx->publicationCertEmail);
	ctx->publicationCertEmail = tmp;
	tmp = NULL;

	res = KSI_OK;
cleanup:
	KSI_free(tmp);
	return res;
}
CTX_VALUEP_GETTER(publicationCertEmail, PublicationCertEmail, const char)
