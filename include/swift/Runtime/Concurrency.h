//===--- Concurrency.h - Runtime interface for concurrency ------*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2020 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// The runtime interface for concurrency.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_RUNTIME_CONCURRENCY_H
#define SWIFT_RUNTIME_CONCURRENCY_H

#include "swift/ABI/TaskGroup.h"
#include "swift/ABI/TaskStatus.h"

namespace swift {
class DefaultActor;

struct SwiftError;

struct AsyncTaskAndContext {
  AsyncTask *Task;
  AsyncContext *InitialContext;
};

/// Create a task object with no future which will run the given
/// function.
///
/// The task is not yet scheduled.
///
/// If a parent task is provided, flags.task_hasChildFragment() must
/// be true, and this must be called synchronously with the parent.
/// The parent is responsible for creating a ChildTaskStatusRecord.
/// TODO: should we have a single runtime function for creating a task
///       and doing this child task status record management?
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
AsyncTaskAndContext swift_task_create(JobFlags flags,
                                      AsyncTask *parent,
                const ThinNullaryAsyncSignature::FunctionPointer *function);

/// Create a task object with no future which will run the given
/// function.
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
AsyncTaskAndContext swift_task_create_f(JobFlags flags,
                                        AsyncTask *parent,
                             ThinNullaryAsyncSignature::FunctionType *function,
                                        size_t initialContextSize);

/// Caution: not all future-initializing functions actually throw, so
/// this signature may be incorrect.
using FutureAsyncSignature =
  AsyncSignature<void(void*), /*throws*/ true>;

/// Create a task object with a future which will run the given
/// function.
///
/// The task is not yet scheduled.
///
/// If a parent task is provided, flags.task_hasChildFragment() must
/// be true, and this must be called synchronously with the parent.
/// The parent is responsible for creating a ChildTaskStatusRecord.
/// TODO: should we have a single runtime function for creating a task
///       and doing this child task status record management?
///
/// flags.task_isFuture must be set. \c futureResultType is the type
///
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
AsyncTaskAndContext swift_task_create_future(
    JobFlags flags, AsyncTask *parent, const Metadata *futureResultType,
    const FutureAsyncSignature::FunctionPointer *function);

/// Create a task object with a future which will run the given
/// function.
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
AsyncTaskAndContext swift_task_create_future_f(
    JobFlags flags, AsyncTask *parent, const Metadata *futureResultType,
    FutureAsyncSignature::FunctionType *function,
    size_t initialContextSize);

/// Create a task object with a future which will run the given
/// function, and offer its result to the task group
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
AsyncTaskAndContext swift_task_create_group_future_f(
    JobFlags flags,
    AsyncTask *parent, TaskGroup *group,
    const Metadata *futureResultType,
    FutureAsyncSignature::FunctionType *function,
    size_t initialContextSize);

/// Allocate memory in a task.
///
/// This must be called synchronously with the task.
///
/// All allocations will be rounded to a multiple of MAX_ALIGNMENT.
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
void *swift_task_alloc(AsyncTask *task, size_t size);

/// Deallocate memory in a task.
///
/// The pointer provided must be the last pointer allocated on
/// this task that has not yet been deallocated; that is, memory
/// must be allocated and deallocated in a strict stack discipline.
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
void swift_task_dealloc(AsyncTask *task, void *ptr);

/// Cancel a task and all of its child tasks.
///
/// This can be called from any thread.
///
/// This has no effect if the task is already cancelled.
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
void swift_task_cancel(AsyncTask *task);

/// Cancel all child tasks of `parent` that belong to the `group`.
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
void swift_task_cancel_group_child_tasks(AsyncTask *task, TaskGroup *group);

/// Get 'active' AsyncTask, depending on platform this may use thread local storage.
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
AsyncTask* swift_task_get_active();

/// Escalate the priority of a task and all of its child tasks.
///
/// This can be called from any thread.
///
/// This has no effect if the task already has at least the given priority.
/// Returns the priority of the task.
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
JobPriority
swift_task_escalate(AsyncTask *task, JobPriority newPriority);

using TaskFutureWaitSignature =
  AsyncSignature<void(AsyncTask *, OpaqueValue *), /*throws*/ false>;

/// Wait for a non-throwing future task to complete.
///
/// This can be called from any thread. Its Swift signature is
///
/// \code
/// func swift_task_future_wait(on task: _owned Builtin.NativeObject) async
///     -> Success
/// \endcode
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swiftasync)
TaskFutureWaitSignature::FunctionType
swift_task_future_wait;

using TaskFutureWaitThrowingSignature =
  AsyncSignature<void(AsyncTask *, OpaqueValue *), /*throws*/ true>;

/// Wait for a potentially-throwing future task to complete.
///
/// This can be called from any thread. Its Swift signature is
///
/// \code
/// func swift_task_future_wait_throwing(on task: _owned Builtin.NativeObject)
///    async throws -> Success
/// \endcode
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swiftasync)
TaskFutureWaitThrowingSignature::FunctionType
swift_task_future_wait_throwing;

using TaskGroupFutureWaitThrowingSignature =
  AsyncSignature<void(AsyncTask *, TaskGroup *, Metadata *), /*throws*/ true>;

/// Wait for a readyQueue of a Channel to become non empty.
///
/// This can be called from any thread. Its Swift signature is
///
/// \code
/// func swift_task_group_wait_next_throwing(
///     waitingTask: Builtin.NativeObject, // current task
///     group: UnsafeRawPointer,
/// ) async -> T
/// \endcode
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swiftasync)
TaskGroupFutureWaitThrowingSignature::FunctionType
swift_task_group_wait_next_throwing;

/// Create a new `TaskGroup` using the task's allocator.
/// The caller is responsible for retaining and managing the group's lifecycle.
///
/// Its Swift signature is
///
/// \code
/// func swift_task_group_create(
///     _ task: Builtin.NativeObject
/// ) -> Builtin.NativeObject
/// \endcode
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
swift::TaskGroup* swift_task_group_create(AsyncTask *task);

/// Attach a child task to the parent task's task group record.
///
/// Its Swift signature is
///
/// \code
/// func swift_task_group_attachChild(
///     group: UnsafeRawPointer,
///     parent: Builtin.NativeObject,
///     child: Builtin.NativeObject
/// )
/// \endcode
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
void swift_task_group_attachChild(TaskGroup *group,
                                  AsyncTask *parent, AsyncTask *child);

/// Its Swift signature is
///
/// \code
/// func swift_task_group_destroy(
///     _ task: Builtin.NativeObject,
///     _ group: UnsafeRawPointer
/// )
/// \endcode
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
void swift_task_group_destroy(AsyncTask *task, TaskGroup *group);

/// Before starting a task group child task, inform the group that there is one
/// more 'pending' child to account for.
///
/// This can be called from any thread. Its Swift signature is
///
/// \code
/// func swift_task_group_add_pending(
///     group: UnsafeRawPointer
/// ) -> Bool
/// \endcode
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
bool swift_task_group_add_pending(TaskGroup *group);

/// Cancel all tasks in the group.
/// This also prevents new tasks from being added.
///
/// This can be called from any thread. Its Swift signature is
///
/// \code
/// func swift_task_group_cancel_all(
///     task: Builtin.NativeObject,
///     group: UnsafeRawPointer
/// )
/// \endcode
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
void swift_task_group_cancel_all(AsyncTask *task, TaskGroup *group);

/// Check ONLY if the group was explicitly cancelled, e.g. by `cancelAll`.
///
/// This check DOES NOT take into account the task in which the group is running
/// being cancelled or not.
///
/// This can be called from any thread. Its Swift signature is
///
/// \code
/// func swift_task_group_is_cancelled(
///     task: Builtin.NativeObject,
///     group: UnsafeRawPointer
/// )
/// \endcode
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
bool swift_task_group_is_cancelled(AsyncTask *task, TaskGroup *group);

/// Check the readyQueue of a task group, return true if it has no pending tasks.
///
/// This can be called from any thread. Its Swift signature is
///
/// \code
/// func swift_task_group_is_empty(
///     _ group: UnsafeRawPointer
/// ) -> Bool
/// \endcode
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
bool swift_task_group_is_empty(TaskGroup *group);

/// Add a status record to a task.  The record should not be
/// modified while it is registered with a task.
///
/// This must be called synchronously with the task.
///
/// If the task is already cancelled, returns `false` but still adds
/// the status record.
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
bool swift_task_addStatusRecord(AsyncTask *task,
                                TaskStatusRecord *record);

/// Add a status record to a task if the task has not already
/// been cancelled.   The record should not be modified while it is
/// registered with a task.
///
/// This must be called synchronously with the task.
///
/// If the task is already cancelled, returns `false` and does not
/// add the status record.
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
bool swift_task_tryAddStatusRecord(AsyncTask *task,
                                   TaskStatusRecord *record);

/// Remove a status record from a task.  After this call returns,
/// the record's memory can be freely modified or deallocated.
///
/// This must be called synchronously with the task.  The record must
/// be registered with the task or else this may crash.
///
/// The given record need not be the last record added to
/// the task, but the operation may be less efficient if not.
///s
/// Returns false if the task has been cancelled.
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
bool swift_task_removeStatusRecord(AsyncTask *task, TaskStatusRecord *record);

/// Attach a child task to its parent task and return the newly created
/// `ChildTaskStatusRecord`.
///
/// The record must be removed with by the parent invoking
/// `swift_task_detachChild` when the child has completed.
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
ChildTaskStatusRecord*
swift_task_attachChild(AsyncTask *parent, AsyncTask *child);

/// Remove a child task from the parent tracking it.
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
void swift_task_detachChild(AsyncTask *parent, ChildTaskStatusRecord *record);

SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
size_t swift_task_getJobFlags(AsyncTask* task);

SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
bool swift_task_isCancelled(AsyncTask* task);

/// Create and add an cancellation record to the task.
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
CancellationNotificationStatusRecord*
swift_task_addCancellationHandler(
    AsyncTask *task, CancellationNotificationStatusRecord::FunctionType handler);

/// Remove the passed cancellation record from the task.
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
void swift_task_removeCancellationHandler(
    AsyncTask *task, CancellationNotificationStatusRecord *record);

using TaskLocalValuesFragment = AsyncTask::TaskLocalValuesFragment;

/// Get a task local value from the passed in task. Its Swift signature is
///
/// \code
/// func _taskLocalValueGet<Key>(
///   _ task: Builtin.NativeObject,
///   keyType: Any.Type /*Key.Type*/,
///   inheritance: UInt8/*TaskLocalInheritance*/
/// ) -> UnsafeMutableRawPointer? where Key: TaskLocalKey
/// \endcode
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
OpaqueValue*
swift_task_localValueGet(AsyncTask* task,
                         const Metadata *keyType,
                         TaskLocalValuesFragment::TaskLocalInheritance inheritance);

/// Add a task local value to the passed in task.
///
/// This must be only invoked by the task itself to avoid concurrent writes.
///
/// Its Swift signature is
///
/// \code
///  public func _taskLocalValuePush<Value>(
///    _ task: Builtin.NativeObject,
///    keyType: Any.Type/*Key.Type*/,
///    value: __owned Value
///  )
/// \endcode
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
void swift_task_localValuePush(AsyncTask* task,
                         const Metadata *keyType,
                         /* +1 */ OpaqueValue *value,
                         const Metadata *valueType);

/// Remove task a local binding from the task local values stack.
///
/// This must be only invoked by the task itself to avoid concurrent writes.
///
/// Its Swift signature is
///
/// \code
///  public func _taskLocalValuePop(
///    _ task: Builtin.NativeObject
///  )
/// \endcode
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
void swift_task_localValuePop(AsyncTask* task);

/// This should have the same representation as an enum like this:
///    enum NearestTaskDeadline {
///      case none
///      case alreadyCancelled
///      case active(TaskDeadline)
///    }
/// TODO: decide what this interface should really be.
struct NearestTaskDeadline {
  enum Kind : uint8_t {
    None,
    AlreadyCancelled,
    Active
  };

  TaskDeadline Value;
  Kind ValueKind;
};

/// Returns the nearest deadline that's been registered with this task.
///
/// This must be called synchronously with the task.
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
NearestTaskDeadline
swift_task_getNearestDeadline(AsyncTask *task);

/// Run the given async function and block the current thread until
/// it returns.  This is a hack added for testing purposes; eventually
/// top-level code will be an async context, and the need for this in
/// tests should go away.  We *definitely* do not want this to be part
/// of the standard feature set.
///
/// The argument is a `() async -> ()` function, whose ABI is currently
/// quite complex.  Eventually this should use a different convention;
/// that's rdar://72105841.
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
void swift_task_runAndBlockThread(const void *function,
                                  HeapObject *functionContext);

/// Switch the current task to a new executor if we aren't already
/// running on a compatible executor.
///
/// The resumption function pointer and continuation should be set
/// appropriately in the task.
///
/// Generally the compiler should inline a fast-path compatible-executor
/// check to avoid doing the suspension work.  This function should
/// generally be tail-called, as it may continue executing the task
/// synchronously if possible.
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swiftasync)
void swift_task_switch(AsyncTask *task,
                       ExecutorRef currentExecutor,
                       ExecutorRef newExecutor);

/// Enqueue the given job to run asynchronously on the given executor.
///
/// The resumption function pointer and continuation should be set
/// appropriately in the task.
///
/// Generally you should call swift_task_switch to switch execution
/// synchronously when possible.
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
void swift_task_enqueue(Job *job, ExecutorRef executor);

/// Enqueue the given job to run asynchronously on the global
/// execution pool.
///
/// The resumption function pointer and continuation should be set
/// appropriately in the task.
///
/// Generally you should call swift_task_switch to switch execution
/// synchronously when possible.
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
void swift_task_enqueueGlobal(Job *job);

/// FIXME: only exists for the quick-and-dirty MainActor implementation.
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
void swift_task_enqueueMainExecutor(Job *job);

/// FIXME: only exists for the quick-and-dirty MainActor implementation.
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
void swift_MainActor_register(HeapObject *actor);

/// A hook to take over global enqueuing.
/// TODO: figure out a better abstraction plan than this.
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
void (*swift_task_enqueueGlobal_hook)(Job *job);

/// Initialize the runtime storage for a default actor.
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
void swift_defaultActor_initialize(DefaultActor *actor);

/// Destroy the runtime storage for a default actor.
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
void swift_defaultActor_destroy(DefaultActor *actor);

/// Enqueue a job on the default actor implementation.
///
/// The job must be ready to run.  Notably, if it's a task, that
/// means that the resumption function and context should have been
/// set appropriately.
///
/// Jobs are assumed to be "self-consuming": once it starts running,
/// the job memory is invalidated and the executor should not access it
/// again.
///
/// Jobs are generally expected to keep the actor alive during their
/// execution.
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
void swift_defaultActor_enqueue(Job *job, DefaultActor *actor);

/// Resume a task from its continuation, given a normal result value.
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
void swift_continuation_resume(/* +1 */ OpaqueValue *result,
                               void *continuation,
                               const Metadata *resumeType);

/// Resume a task from its throwing continuation, given a normal result value.
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
void swift_continuation_throwingResume(/* +1 */ OpaqueValue *result,
                                       void *continuation,
                                       const Metadata *resumeType);

/// Resume a task from its throwing continuation by throwing an error.
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
void swift_continuation_throwingResumeWithError(/* +1 */ SwiftError *error,
                                                void *continuation,
                                                const Metadata *resumeType);

/// SPI helper to log a misuse of a `CheckedContinuation` to the appropriate places in the OS.
extern "C" SWIFT_CC(swift)
void swift_continuation_logFailedCheck(const char *message);

/// Drain the queue
/// If the binary links CoreFoundation, uses CFRunLoopRun
/// Otherwise it uses dispatchMain.
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
void swift_task_asyncMainDrainQueue();

/// Establish that the current thread is running as the given
/// executor, then run a job.
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
void swift_job_run(Job *job, ExecutorRef executor);

/// Return the current thread's active task reference.
SWIFT_EXPORT_FROM(swift_Concurrency) SWIFT_CC(swift)
AsyncTask *swift_task_getCurrent(void);

}

#endif
