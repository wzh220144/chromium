/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef IDBTransaction_h
#define IDBTransaction_h

#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/DOMStringList.h"
#include "core/events/EventListener.h"
#include "modules/EventModules.h"
#include "modules/EventTargetModules.h"
#include "modules/ModulesExport.h"
#include "modules/indexeddb/IDBMetadata.h"
#include "modules/indexeddb/IndexedDB.h"
#include "platform/bindings/ActiveScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/Vector.h"
#include "public/platform/modules/indexeddb/WebIDBDatabase.h"
#include "public/platform/modules/indexeddb/WebIDBTypes.h"

namespace blink {

class DOMException;
class ExecutionContext;
class ExceptionState;
class IDBDatabase;
class IDBIndex;
class IDBObjectStore;
class IDBOpenDBRequest;
class IDBRequest;
class ScriptState;

class MODULES_EXPORT IDBTransaction final
    : public EventTargetWithInlineData,
      public ActiveScriptWrappable<IDBTransaction>,
      public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(IDBTransaction);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static IDBTransaction* CreateObserver(ExecutionContext*,
                                        int64_t,
                                        const HashSet<String>& scope,
                                        IDBDatabase*);

  static IDBTransaction* CreateNonVersionChange(ScriptState*,
                                                int64_t,
                                                const HashSet<String>& scope,
                                                WebIDBTransactionMode,
                                                IDBDatabase*);
  static IDBTransaction* CreateVersionChange(
      ExecutionContext*,
      int64_t,
      IDBDatabase*,
      IDBOpenDBRequest*,
      const IDBDatabaseMetadata& old_metadata);
  ~IDBTransaction() override;
  DECLARE_VIRTUAL_TRACE();

  static WebIDBTransactionMode StringToMode(const String&);

  // When the connection is closed backend will be 0.
  WebIDBDatabase* BackendDB() const;

  int64_t Id() const { return id_; }
  bool IsActive() const { return state_ == kActive; }
  bool IsFinished() const { return state_ == kFinished; }
  bool IsFinishing() const { return state_ == kFinishing; }
  bool IsReadOnly() const { return mode_ == kWebIDBTransactionModeReadOnly; }
  bool IsVersionChange() const {
    return mode_ == kWebIDBTransactionModeVersionChange;
  }

  // Implement the IDBTransaction IDL
  const String& mode() const;
  DOMStringList* objectStoreNames() const;
  IDBDatabase* db() const { return database_.Get(); }
  DOMException* error() const { return error_; }
  IDBObjectStore* objectStore(const String& name, ExceptionState&);
  void abort(ExceptionState&);

  void RegisterRequest(IDBRequest*);
  void UnregisterRequest(IDBRequest*);

  // The methods below are called right before the changes are applied to the
  // database's metadata. We use this unusual sequencing because some of the
  // methods below need to access the metadata values before the change, and
  // following the same lifecycle for all methods makes the code easier to
  // reason about.
  void ObjectStoreCreated(const String& name, IDBObjectStore*);
  void ObjectStoreDeleted(const int64_t object_store_id, const String& name);
  void ObjectStoreRenamed(const String& old_name, const String& new_name);
  // Called when deleting an index whose IDBIndex had been created.
  void IndexDeleted(IDBIndex*);

  void SetActive(bool);
  void SetError(DOMException*);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(abort);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(complete);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(error);

  void OnAbort(DOMException*);
  void OnComplete();

  // EventTarget
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // ScriptWrappable
  bool HasPendingActivity() const final;

  // For use in IDBObjectStore::IsNewlyCreated(). The rest of the code should
  // use IDBObjectStore::IsNewlyCreated() instead of calling this method
  // directly.
  int64_t OldMaxObjectStoreId() const {
    DCHECK(IsVersionChange());
    return old_database_metadata_.max_object_store_id;
  }

  // Returns a detailed message to use when throwing TransactionInactiveError,
  // depending on whether the transaction is just inactive or has finished.
  const char* InactiveErrorMessage() const;

 protected:
  // EventTarget
  DispatchEventResult DispatchEventInternal(Event*) override;

 private:
  using IDBObjectStoreMap = HeapHashMap<String, Member<IDBObjectStore>>;

  // For observer transactions.
  IDBTransaction(ExecutionContext*,
                 int64_t,
                 const HashSet<String>& scope,
                 IDBDatabase*);

  // For non-upgrade transactions.
  IDBTransaction(ScriptState*,
                 int64_t,
                 const HashSet<String>& scope,
                 WebIDBTransactionMode,
                 IDBDatabase*);

  // For upgrade transactions.
  IDBTransaction(ExecutionContext*,
                 int64_t,
                 IDBDatabase*,
                 IDBOpenDBRequest*,
                 const IDBDatabaseMetadata&);

  void EnqueueEvent(Event*);

  // Called when a transaction is aborted.
  void AbortOutstandingRequests();
  void RevertDatabaseMetadata();

  // Called when a transaction is completed (committed or aborted).
  void Finished();

  enum State {
    kInactive,   // Created or started, but not in an event callback
    kActive,     // Created or started, in creation scope or an event callback
    kFinishing,  // In the process of aborting or completing.
    kFinished,   // No more events will fire and no new requests may be filed.
  };

  const int64_t id_;
  Member<IDBDatabase> database_;
  Member<IDBOpenDBRequest> open_db_request_;
  const WebIDBTransactionMode mode_;

  // The names of the object stores that make up this transaction's scope.
  //
  // Transactions may not access object stores outside their scope.
  //
  // The scope of versionchange transactions is the entire database. We
  // represent this case with an empty |scope_|, because copying all the store
  // names would waste both time and memory.
  //
  // Using object store names to represent a transaction's scope is safe
  // because object stores cannot be renamed in non-versionchange
  // transactions.
  const HashSet<String> scope_;

  State state_ = kActive;
  bool has_pending_activity_ = true;
  Member<DOMException> error_;

  HeapListHashSet<Member<IDBRequest>> request_list_;

#if DCHECK_IS_ON()
  bool finish_called_ = false;
#endif  // DCHECK_IS_ON()

  // Caches the IDBObjectStore instances returned by the objectStore() method.
  //
  // The spec requires that a transaction's objectStore() returns the same
  // IDBObjectStore instance for a specific store, so this cache is necessary
  // for correctness.
  //
  // objectStore() throws for completed/aborted transactions, so this is not
  // used after a transaction is finished, and can be cleared.
  IDBObjectStoreMap object_store_map_;

  // The metadata of object stores when they are opened by this transaction.
  //
  // Only valid for versionchange transactions.
  HeapHashMap<Member<IDBObjectStore>, RefPtr<IDBObjectStoreMetadata>>
      old_store_metadata_;

  // The metadata of deleted object stores without IDBObjectStore instances.
  //
  // Only valid for versionchange transactions.
  Vector<RefPtr<IDBObjectStoreMetadata>> deleted_object_stores_;

  // Tracks the indexes deleted by this transaction.
  //
  // This set only includes indexes that were created before this transaction,
  // and were deleted during this transaction. Once marked for deletion, these
  // indexes are removed from their object stores' index maps, so we need to
  // stash them somewhere else in case the transaction gets aborted.
  //
  // This set does not include indexes created and deleted during this
  // transaction, because we don't need to change their metadata when the
  // transaction aborts, as they will still be marked for deletion.
  //
  // Only valid for versionchange transactions.
  HeapVector<Member<IDBIndex>> deleted_indexes_;

  // Shallow snapshot of the database metadata when the transaction starts.
  //
  // This does not include a snapshot of the database's object store / index
  // metadata.
  //
  // Only valid for versionchange transactions.
  IDBDatabaseMetadata old_database_metadata_;
};

}  // namespace blink

#endif  // IDBTransaction_h
