// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/renderer/credential_manager_client.h"

#include <stdint.h>

#include <memory>
#include <tuple>
#include <utility>

#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "content/public/test/render_view_test.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebCredential.h"
#include "third_party/WebKit/public/platform/WebCredentialManagerClient.h"
#include "third_party/WebKit/public/platform/WebCredentialManagerError.h"
#include "third_party/WebKit/public/platform/WebPasswordCredential.h"

namespace password_manager {

namespace {

const char kTestCredentialPassword[] = "https://password.com/";
const char kTestCredentialEmpty[] = "https://empty.com/";
const char kTestCredentialReject[] = "https://reject.com/";

class FakeCredentialManager : public mojom::CredentialManager {
 public:
  FakeCredentialManager() {}
  ~FakeCredentialManager() override {}

  void BindRequest(mojom::CredentialManagerRequest request) {
    bindings_.AddBinding(this, std::move(request));
  }

 private:
  // mojom::CredentialManager methods:
  void Store(const CredentialInfo& credential,
             StoreCallback callback) override {
    std::move(callback).Run();
  }

  void RequireUserMediation(RequireUserMediationCallback callback) override {
    std::move(callback).Run();
  }

  void Get(bool zero_click_only,
           bool include_passwords,
           const std::vector<GURL>& federations,
           GetCallback callback) override {
    const std::string& url = federations[0].spec();

    if (url == kTestCredentialPassword) {
      CredentialInfo info;
      info.type = CredentialType::CREDENTIAL_TYPE_PASSWORD;
      std::move(callback).Run(mojom::CredentialManagerError::SUCCESS, info);
    } else if (url == kTestCredentialEmpty) {
      std::move(callback).Run(mojom::CredentialManagerError::SUCCESS,
                              CredentialInfo());
    } else if (url == kTestCredentialReject) {
      std::move(callback).Run(
          mojom::CredentialManagerError::PASSWORDSTOREUNAVAILABLE,
          base::nullopt);
    }
  }

  mojo::BindingSet<mojom::CredentialManager> bindings_;
};

class CredentialManagerClientTest : public content::RenderViewTest {
 public:
  CredentialManagerClientTest()
      : callback_errored_(false), callback_succeeded_(false) {}
  ~CredentialManagerClientTest() override {}

  void SetUp() override {
    content::RenderViewTest::SetUp();
    client_.reset(new CredentialManagerClient(view_));

    service_manager::InterfaceProvider* remote_interfaces =
        view_->GetMainRenderFrame()->GetRemoteInterfaces();
    service_manager::InterfaceProvider::TestApi test_api(remote_interfaces);
    test_api.SetBinderForName(
        mojom::CredentialManager::Name_,
        base::Bind(&CredentialManagerClientTest::BindCredentialManager,
                   base::Unretained(this)));
  }

  void TearDown() override {
    credential_.reset();
    client_.reset();
    content::RenderViewTest::TearDown();
  }

  bool callback_errored() const { return callback_errored_; }
  void set_callback_errored(bool state) { callback_errored_ = state; }
  bool callback_succeeded() const { return callback_succeeded_; }
  void set_callback_succeeded(bool state) { callback_succeeded_ = state; }

  void BindCredentialManager(mojo::ScopedMessagePipeHandle handle) {
    fake_cm_.BindRequest(mojom::CredentialManagerRequest(std::move(handle)));
  }

  std::unique_ptr<blink::WebPasswordCredential> credential_;
  blink::WebCredentialManagerError error_;

 protected:
  std::unique_ptr<CredentialManagerClient> client_;

  FakeCredentialManager fake_cm_;

  // True if a message's callback's 'onSuccess'/'onError' methods were called,
  // false otherwise. We put these on the test object rather than on the
  // Test*Callbacks objects because ownership of those objects passes into the
  // client, which destroys the callbacks after calling them to resolve the
  // pending Blink-side Promise.
  bool callback_errored_;
  bool callback_succeeded_;
};

class TestNotificationCallbacks
    : public blink::WebCredentialManagerClient::NotificationCallbacks {
 public:
  explicit TestNotificationCallbacks(CredentialManagerClientTest* test)
      : test_(test) {}

  ~TestNotificationCallbacks() override {}

  void OnSuccess() override { test_->set_callback_succeeded(true); }

  void OnError(blink::WebCredentialManagerError reason) override {
    test_->set_callback_errored(true);
  }

 private:
  CredentialManagerClientTest* test_;
};

class TestRequestCallbacks
    : public blink::WebCredentialManagerClient::RequestCallbacks {
 public:
  explicit TestRequestCallbacks(CredentialManagerClientTest* test)
      : test_(test) {}

  ~TestRequestCallbacks() override {}

  void OnSuccess(std::unique_ptr<blink::WebCredential> credential) override {
    test_->set_callback_succeeded(true);

    blink::WebCredential* ptr = credential.release();
    test_->credential_.reset(static_cast<blink::WebPasswordCredential*>(ptr));
  }

  void OnError(blink::WebCredentialManagerError reason) override {
    test_->set_callback_errored(true);
    test_->credential_.reset();
    test_->error_ = reason;
  }

 private:
  CredentialManagerClientTest* test_;
};

void RunAllPendingTasks() {
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
  run_loop.Run();
}

}  // namespace

TEST_F(CredentialManagerClientTest, SendStore) {
  credential_.reset(new blink::WebPasswordCredential("", "", "", GURL()));
  std::unique_ptr<TestNotificationCallbacks> callbacks(
      new TestNotificationCallbacks(this));
  client_->DispatchStore(*credential_, callbacks.release());

  RunAllPendingTasks();

  EXPECT_TRUE(callback_succeeded());
  EXPECT_FALSE(callback_errored());
}

TEST_F(CredentialManagerClientTest, SendRequestUserMediation) {
  std::unique_ptr<TestNotificationCallbacks> callbacks(
      new TestNotificationCallbacks(this));
  client_->DispatchRequireUserMediation(callbacks.release());

  RunAllPendingTasks();

  EXPECT_TRUE(callback_succeeded());
  EXPECT_FALSE(callback_errored());
}

TEST_F(CredentialManagerClientTest, SendRequestCredential) {
  std::unique_ptr<TestRequestCallbacks> callbacks(
      new TestRequestCallbacks(this));
  std::vector<GURL> federations;
  federations.push_back(GURL(kTestCredentialPassword));
  client_->DispatchGet(false, true, federations, callbacks.release());

  RunAllPendingTasks();

  EXPECT_TRUE(callback_succeeded());
  EXPECT_FALSE(callback_errored());
  EXPECT_TRUE(credential_);
  EXPECT_EQ("password", credential_->GetType());
}

TEST_F(CredentialManagerClientTest, SendRequestCredentialEmpty) {
  std::unique_ptr<TestRequestCallbacks> callbacks(
      new TestRequestCallbacks(this));
  std::vector<GURL> federations;
  federations.push_back(GURL(kTestCredentialEmpty));
  client_->DispatchGet(false, true, federations, callbacks.release());

  RunAllPendingTasks();

  EXPECT_TRUE(callback_succeeded());
  EXPECT_FALSE(callback_errored());
  EXPECT_FALSE(credential_);
}

TEST_F(CredentialManagerClientTest, SendRequestCredentialReject) {
  std::unique_ptr<TestRequestCallbacks> callbacks(
      new TestRequestCallbacks(this));
  std::vector<GURL> federations;
  federations.push_back(GURL(kTestCredentialReject));
  client_->DispatchGet(false, true, federations, callbacks.release());

  RunAllPendingTasks();

  EXPECT_FALSE(callback_succeeded());
  EXPECT_TRUE(callback_errored());
  EXPECT_FALSE(credential_);
  EXPECT_EQ(blink::WebCredentialManagerError::
                kWebCredentialManagerPasswordStoreUnavailableError,
            error_);
}

}  // namespace password_manager
