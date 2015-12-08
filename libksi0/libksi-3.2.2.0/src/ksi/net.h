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

#ifndef KSI_NET_H_
#define KSI_NET_H_

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

	/**
	 * \addtogroup network Network Interface
	 * This module contains two networking concepts used in this API:
	 * - Network provider (#KSI_NetworkClient), this object takes care of network
	 * transport.
	 * - Network handle (#KSI_RequestHandle), this object contains a single request and
	 * is used to access the response.
	 * @{
	 */

	/**
	 * Free network handle object.
	 * \param[in]		handle			Network handle.
	 */
	void KSI_RequestHandle_free(KSI_RequestHandle *handle);

	/**
	 * Free network provider object.
	 * \param[in]		provider		Network provider.
	 */
	void KSI_NetworkClient_free(KSI_NetworkClient *provider);

	/**
	 * Sends a non-blocking signing request or initialize the handle.
	 * \param[in]		provider		Network provider.
	 * \param[in]		request			Request object.
	 * \param[out]		handle			Network handle.
	 *
	 * \return status code (#KSI_OK, when operation succeeded, otherwise an
	 * error code).
	 */
	int KSI_NetworkClient_sendSignRequest(KSI_NetworkClient *provider, KSI_AggregationReq *request, KSI_RequestHandle **handle);

	/**
	 * Sends a non-blocking extending request or initialize the handle.
	 * \param[in]		provider		Network provider.
	 * \param[in]		request			Extend request.
	 * \param[in]		handle			Network handle.
	 *
	 * \return status code (#KSI_OK, when operation succeeded, otherwise an
	 * error code).
	 */
	int KSI_NetworkClient_sendExtendRequest(KSI_NetworkClient *provider, KSI_ExtendReq *request, KSI_RequestHandle **handle);

	/**
	 * Sends a non-blocking publicationsfile request or initialize the handle.
	 * \param[in]		provider		Network provider.
	 * \param[in]		handle			Network handle.
	 *
	 * \return status code (#KSI_OK, when operation succeeded, otherwise an
	 * error code).
	 */
	int KSI_NetworkClient_sendPublicationsFileRequest(KSI_NetworkClient *provider, KSI_RequestHandle **handle);

	/**
	 * Setter for network request implementation context.
	 * \param[in]		handle			Network handle.
	 * \param[in]		netCtx			Network implementation context.
	 * \param[in]		netCtx_free		Cleanup method for the network context.
	 *
	 * \return status code (#KSI_OK, when operation succeeded, otherwise an
	 * error code).
	 */
	int KSI_RequestHandle_setImplContext(KSI_RequestHandle *handle, void *netCtx, void (*netCtx_free)(void *));

	/**
	 * Getter method for the network request implementation context.
	 * \param[in]		handle			Network handle.
	 * \param[out]		c				Pointer to the receiving pointer.
	 *
	 * \return status code (#KSI_OK, when operation succeeded, otherwise an
	 * error code).
	 * \note The caller may not free the output object.
	 */
	int KSI_RequestHandle_getNetContext(KSI_RequestHandle *handle, void **c);

	/**
	 * Getter for the request. The request can be set only while creating the network handle object
	 * (\see #KSI_RequestHandle_new).
	 *
	 * \param[in]		handle			Network handle.
	 * \param[out]		request			Pointer to the receiving pointer.
	 * \param[out]		request_len		Pointer to the receiving length value.
	 *
	 * \return status code (#KSI_OK, when operation succeeded, otherwise an error code).
	 * \note The output memory may not be freed by the caller.
	 */
	int KSI_RequestHandle_getRequest(KSI_RequestHandle *handle, const unsigned char **request, unsigned *request_len);

	/**
	 * Response value setter. Should be called only by the actual network provider implementation.
	 * \param[in]		handle			Network handle.
	 * \param[in]		response		Pointer to the response.
	 * \param[in]		response_len	Response length.
	 *
	 * \return status code (#KSI_OK, when operation succeeded, otherwise an
	 * error code).
	 * \note The \c response memory may be freed after a successful call to this method, as the
	 * contents is copied internally.
	 */
	int KSI_RequestHandle_setResponse(KSI_RequestHandle *handle, const unsigned char *response, unsigned response_len);

	/**
	 * A blocking function to read the response to the request. The function is blocking only
	 * for the first call on one handle. If the first call succeeds following calls output the
	 * same value.
	 * \param[in]		handle			Network handle.
	 * \param[out]		response		Pointer to the receiving response pointer.
	 * \param[out]		response_len	Pointer to the receiving response length value.
	 *
	 * \return status code (#KSI_OK, when operation succeeded, otherwise an
	 * error code).
	 */
	int KSI_RequestHandle_getResponse(KSI_RequestHandle *handle, const unsigned char **response, unsigned *response_len);

	/**
	 * TODO!
	 */
	int KSI_RequestHandle_getExtendResponse(KSI_RequestHandle *handle, KSI_ExtendResp **resp);

	/**
	 * TODO!
	 */
	int KSI_RequestHandle_getAggregationResponse(KSI_RequestHandle *handle, KSI_AggregationResp **resp);

	/**
	 * TODO!
	 */
	KSI_CTX *KSI_RequestHandle_getCtx (const KSI_RequestHandle *handle);
	/**
	 * Constructor for network handle object.
	 * \param[in]		ctx				KSI context.
	 * \param[in]		request			Pointer to request.
	 * \param[in]		request_length	Length of the request.
	 * \param[out]		handle			Pointer to the receiving network handle pointer.
	 *
	 * \return status code (#KSI_OK, when operation succeeded, otherwise an
	 * error code).
	 * \note The \c request value may be freed after a successful call to this function as
	 * its contents is copied internally.
	 */
	int KSI_RequestHandle_new(KSI_CTX *ctx, const unsigned char *request, unsigned request_length, KSI_RequestHandle **handle);

	/**
	 * As network handles may be created by using several KSI contexts with different network providers and/or
	 * the network provider of a KSI context may be changed during runtime, it is necessary to state the function
	 * to be called to receive the response.
	 * \param[in]		handle			Network handle.
	 * \param[in]		fn				Pointer to response reader function.
	 *
	 * \return status code (#KSI_OK, when operation succeeded, otherwise an
	 * error code).
	 */
	int KSI_RequestHandle_setReadResponseFn(KSI_RequestHandle *handle, int (*fn)(KSI_RequestHandle *));

	/**
	 * Initialized for an existing abstract network provider.
	 * \param[in]		ctx				KSI context.
	 * \param[out]		client			Abstract network client.
	 *
	 * \return status code (#KSI_OK, when operation succeeded, otherwise an error code).
	 *
	 * \note Do not use, unless implementing a new network client.
	 */
	int KSI_NetworkClient_init(KSI_CTX *ctx, KSI_NetworkClient *client);

	/**
	 * Setter for the implementation specific networking context.
	 * \param[in]		client			Network client.
	 * \param[in]		netCtx			Implementation specific context.
	 * \param[in]		netCtx_free		Pointer to the implementation specific network context cleanup method.
	 *
	 * \return status code (#KSI_OK, when operation succeeded, otherwise an
	 * error code).
	 */
	int KSI_NetworkClient_setNetCtx(KSI_NetworkClient *client, void *netCtx, void (*netCtx_free)(void *));

	/**
	 * Setter for sign request function.
	 * \param[in]		client			Network client.
	 * \param[in]		fn				Pointer to sign request function.
	 *
	 * \return status code (#KSI_OK, when operation succeeded, otherwise an
	 * error code).
	 */
	int KSI_NetworkClient_setSendSignRequestFn(KSI_NetworkClient *client, int (*fn)(KSI_NetworkClient *, KSI_AggregationReq *, KSI_RequestHandle **));

	/**
	 * Setter for sign request function.
	 * \param[in]		client			Network client.
	 * \param[in]		fn				Pointer to extend request function.
	 *
	 * \return status code (#KSI_OK, when operation succeeded, otherwise an
	 * error code).
	 */
	int KSI_NetworkClient_setSendExtendRequestFn(KSI_NetworkClient *client, int (*fn)(KSI_NetworkClient *, KSI_ExtendReq *, KSI_RequestHandle **));

	/**
	 * Setter for sign request function.
	 * \param[in]		client			Network client.
	 * \param[in]		fn				Pointer to publicationsfile request function.
	 *
	 * \return status code (#KSI_OK, when operation succeeded, otherwise an error code).
	 */
	int KSI_NetworkClient_setSendPublicationRequestFn(KSI_NetworkClient *client, int (*fn)(KSI_NetworkClient *, KSI_RequestHandle **));

	int KSI_NetworkClient_setExtenderUser(KSI_NetworkClient *net, const char *val);
	int KSI_NetworkClient_setExtenderPass(KSI_NetworkClient *net, const char *val);
	int KSI_NetworkClient_setAggregatorUser(KSI_NetworkClient *net, const char *val);
	int KSI_NetworkClient_setAggregatorPass(KSI_NetworkClient *net, const char *val);

	int KSI_NetworkClient_getExtenderUser(const KSI_NetworkClient *net, const char **val);
	int KSI_NetworkClient_getExtenderPass(const KSI_NetworkClient *net, const char **val);
	int KSI_NetworkClient_getAggregatorUser(const KSI_NetworkClient *net, const char **val);
	int KSI_NetworkClient_getAggregatorPass(const KSI_NetworkClient *net, const char **val);

	/**
	 * This function converts the aggregator response status code into a KSI status code.
	 * \see #KSI_StatusCode
	 */
	int KSI_convertAggregatorStatusCode(KSI_Integer *statusCode);

	/**
	 * This function converts the extender response status code into a KSI status code.
	 * \see #KSI_StatusCode
	 */
	int KSI_convertExtenderStatusCode(KSI_Integer *statusCode);

	/**
	 * Function to split the given uri into three parts: schema, host and port. If the
	 * part is missing from the uri, the output parameter will receive \c NULL or 0 for \c port
	 * as the value. If the output pointers are set to \c NULL, the value is not returned.
	 * \param[in]	uri			Pointer to the URI.
	 * \param[out]	scheme		Pointer to the receiving pointer of the scheme (may be \c NULL).
	 * \param[out]	host		Pointer to the receiving pointer of the host (may be \c NULL).
	 * \param[out]	port		Pointer to the receiving variable (may be \c NULL).
	 * \param[out]	path		Pointer to the receiving pointer of the path (may be \c NULL).
	 *
	 * \return status code (#KSI_OK, when operation succeeded, otherwise an error code).
	 * \note All aquired pointers have to be freed by the caller using #KSI_free.
	 */
	int KSI_UriSplitBasic(const char *uri, char **scheme, char **host, unsigned *port, char **path);
	/**
	 * @}
	 */
#ifdef __cplusplus
}
#endif

#endif /* KSI_NET_H_ */
