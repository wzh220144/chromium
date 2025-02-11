// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BytesConsumerTestUtil_h
#define BytesConsumerTestUtil_h

#include "modules/fetch/BytesConsumer.h"
#include "modules/fetch/FetchDataLoader.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Deque.h"
#include "platform/wtf/Vector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class ExecutionContext;

class BytesConsumerTestUtil {
  STATIC_ONLY(BytesConsumerTestUtil);

 public:
  class MockBytesConsumer : public BytesConsumer {
   public:
    static MockBytesConsumer* Create() {
      return new ::testing::StrictMock<MockBytesConsumer>();
    }

    MOCK_METHOD2(BeginRead, Result(const char**, size_t*));
    MOCK_METHOD1(EndRead, Result(size_t));
    MOCK_METHOD1(DrainAsBlobDataHandle,
                 PassRefPtr<BlobDataHandle>(BlobSizePolicy));
    MOCK_METHOD0(DrainAsFormData, PassRefPtr<EncodedFormData>());
    MOCK_METHOD1(SetClient, void(Client*));
    MOCK_METHOD0(ClearClient, void());
    MOCK_METHOD0(Cancel, void());
    MOCK_CONST_METHOD0(GetPublicState, PublicState());
    MOCK_CONST_METHOD0(GetError, Error());

    String DebugName() const override { return "MockBytesConsumer"; }

   protected:
    MockBytesConsumer();
  };

  class MockFetchDataLoaderClient
      : public GarbageCollectedFinalized<MockFetchDataLoaderClient>,
        public FetchDataLoader::Client {
    USING_GARBAGE_COLLECTED_MIXIN(MockFetchDataLoaderClient);

   public:
    static ::testing::StrictMock<MockFetchDataLoaderClient>* Create() {
      return new ::testing::StrictMock<MockFetchDataLoaderClient>;
    }

    DEFINE_INLINE_VIRTUAL_TRACE() { FetchDataLoader::Client::Trace(visitor); }

    MOCK_METHOD1(DidFetchDataLoadedBlobHandleMock,
                 void(RefPtr<BlobDataHandle>));
    MOCK_METHOD1(DidFetchDataLoadedArrayBufferMock, void(DOMArrayBuffer*));
    MOCK_METHOD1(DidFetchDataLoadedString, void(const String&));
    MOCK_METHOD0(DidFetchDataLoadStream, void());
    MOCK_METHOD0(DidFetchDataLoadFailed, void());

    void DidFetchDataLoadedArrayBuffer(DOMArrayBuffer* array_buffer) override {
      DidFetchDataLoadedArrayBufferMock(array_buffer);
    }
    // In mock methods we use RefPtr<> rather than PassRefPtr<>.
    void DidFetchDataLoadedBlobHandle(
        PassRefPtr<BlobDataHandle> blob_data_handle) override {
      DidFetchDataLoadedBlobHandleMock(std::move(blob_data_handle));
    }
  };

  class Command final {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

   public:
    enum Name {
      kData,
      kDone,
      kError,
      kWait,
    };

    explicit Command(Name name) : name_(name) {}
    Command(Name name, const Vector<char>& body) : name_(name), body_(body) {}
    Command(Name name, const char* body, size_t size) : name_(name) {
      body_.Append(body, size);
    }
    Command(Name name, const char* body) : Command(name, body, strlen(body)) {}
    Name GetName() const { return name_; }
    const Vector<char>& Body() const { return body_; }

   private:
    const Name name_;
    Vector<char> body_;
  };

  // ReplayingBytesConsumer stores commands via |add| and replays the stored
  // commends when read.
  class ReplayingBytesConsumer final : public BytesConsumer {
   public:
    // The ExecutionContext is needed to get a WebTaskRunner.
    explicit ReplayingBytesConsumer(ExecutionContext*);
    ~ReplayingBytesConsumer();

    // Add a command to this handle. This function must be called BEFORE
    // any BytesConsumer methods are called.
    void Add(const Command& command) { commands_.push_back(command); }

    Result BeginRead(const char** buffer, size_t* available) override;
    Result EndRead(size_t read_size) override;

    void SetClient(Client*) override;
    void ClearClient() override;
    void Cancel() override;
    PublicState GetPublicState() const override;
    Error GetError() const override;
    String DebugName() const override { return "ReplayingBytesConsumer"; }

    bool IsCancelled() const { return is_cancelled_; }

    DECLARE_TRACE();

   private:
    void NotifyAsReadable(int notification_token);
    void Close();
    void GetError(const Error&);

    Member<ExecutionContext> execution_context_;
    Member<BytesConsumer::Client> client_;
    InternalState state_ = InternalState::kWaiting;
    Deque<Command> commands_;
    size_t offset_ = 0;
    BytesConsumer::Error error_;
    int notification_token_ = 0;
    bool is_cancelled_ = false;
  };

  class TwoPhaseReader final : public GarbageCollectedFinalized<TwoPhaseReader>,
                               public BytesConsumer::Client {
    USING_GARBAGE_COLLECTED_MIXIN(TwoPhaseReader);

   public:
    // |consumer| must not have a client when called.
    explicit TwoPhaseReader(BytesConsumer* /* consumer */);

    void OnStateChange() override;
    std::pair<BytesConsumer::Result, Vector<char>> Run();

    DEFINE_INLINE_TRACE() {
      visitor->Trace(consumer_);
      BytesConsumer::Client::Trace(visitor);
    }

   private:
    Member<BytesConsumer> consumer_;
    BytesConsumer::Result result_ = BytesConsumer::Result::kShouldWait;
    Vector<char> data_;
  };
};

}  // namespace blink

#endif  // BytesConsumerTestUtil_h
