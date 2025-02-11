// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_WORKER_FETCH_CONTEXT_IMPL_H_
#define CONTENT_RENDERER_SERVICE_WORKER_WORKER_FETCH_CONTEXT_IMPL_H_

#include "content/common/service_worker/service_worker_types.h"
#include "content/common/worker_url_loader_factory_provider.mojom.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "third_party/WebKit/public/platform/WebWorkerFetchContext.h"
#include "url/gurl.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace content {

class ResourceDispatcher;

class WorkerFetchContextImpl : public blink::WebWorkerFetchContext,
                               public mojom::ServiceWorkerWorkerClient {
 public:
  explicit WorkerFetchContextImpl(
      mojom::WorkerURLLoaderFactoryProviderPtrInfo provider_info);
  ~WorkerFetchContextImpl() override;

  // blink::WebWorkerFetchContext implementation:
  void InitializeOnWorkerThread(base::SingleThreadTaskRunner*) override;
  std::unique_ptr<blink::WebURLLoader> CreateURLLoader() override;
  void WillSendRequest(blink::WebURLRequest&) override;
  bool IsControlledByServiceWorker() const override;
  void SetDataSaverEnabled(bool) override;
  bool IsDataSaverEnabled() const override;
  blink::WebURL FirstPartyForCookies() const override;

  // mojom::ServiceWorkerWorkerClient implementation:
  void SetControllerServiceWorker(int64_t controller_version_id) override;

  // Sets the fetch context status of the parent frame.
  void set_service_worker_provider_id(int id);
  void set_is_controlled_by_service_worker(bool flag);
  void set_parent_frame_id(int id);
  void set_first_party_for_cookies(
      const blink::WebURL& first_party_for_cookies);

 private:
  mojom::WorkerURLLoaderFactoryProviderPtrInfo provider_info_;
  int service_worker_provider_id_ = kInvalidServiceWorkerProviderId;
  bool is_controlled_by_service_worker_ = false;

  // Initialized on the worker thread when InitializeOnWorkerThread() is called.
  std::unique_ptr<ResourceDispatcher> resource_dispatcher_;
  std::unique_ptr<mojo::AssociatedBinding<mojom::ServiceWorkerWorkerClient>>
      binding_;
  mojom::WorkerURLLoaderFactoryProviderPtr provider_;
  mojom::URLLoaderFactoryAssociatedPtr url_loader_factory_;

  // Updated when mojom::ServiceWorkerWorkerClient::SetControllerServiceWorker()
  // is called from the browser process via mojo IPC.
  int controller_version_id_ = kInvalidServiceWorkerVersionId;

  bool is_data_saver_enabled_ = false;
  int parent_frame_id_ = MSG_ROUTING_NONE;
  GURL first_party_for_cookies_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_WORKER_FETCH_CONTEXT_IMPL_H_
