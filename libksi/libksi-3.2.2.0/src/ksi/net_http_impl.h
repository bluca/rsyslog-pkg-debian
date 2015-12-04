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

#ifndef NET_HTTP_INTERNAL_H_
#define NET_HTTP_INTERNAL_H_

#include "internal.h"
#include "net_http.h"
#include "net_impl.h"

#ifdef __cplusplus
extern "C" {
#endif

	struct KSI_HttpClient_st {
		KSI_NetworkClient parent;

		int connectionTimeoutSeconds;
		int readTimeoutSeconds;
		int httpStatus;
		char *urlAggregator;
		char *urlExtender;
		char *urlPublication;
		char *agentName;
	
		int (*sendRequest)(KSI_NetworkClient *, KSI_RequestHandle *, char *);

		void *implCtx;
		void (*implCtx_free)(void *);
	};


#ifdef __cplusplus
}
#endif

#endif /* NET_HTTP_INTERNAL_H_ */
