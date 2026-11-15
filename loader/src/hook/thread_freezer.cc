// Copyright 2026 The Reode Authors

#include "hook/thread_freezer.h"

#include <windows.h>
// tlhelp32.h must come after windows.h.
#include <tlhelp32.h>

#include <cstdint>
#include <functional>

namespace reode::hook {

ThreadFreezer::ThreadFreezer() {
  const DWORD process_id = GetCurrentProcessId();
  const DWORD self_id = GetCurrentThreadId();
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (snapshot == INVALID_HANDLE_VALUE) return;

  THREADENTRY32 entry{};
  entry.dwSize = sizeof(entry);
  for (BOOL ok = Thread32First(snapshot, &entry); ok;
       ok = Thread32Next(snapshot, &entry)) {
    if (entry.th32OwnerProcessID != process_id ||
        entry.th32ThreadID == self_id) {
      continue;
    }
    HANDLE thread = OpenThread(
        THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE,
        entry.th32ThreadID);
    if (thread == nullptr) continue;
    if (SuspendThread(thread) == static_cast<DWORD>(-1)) {
      CloseHandle(thread);
      continue;
    }
    threads_.push_back(thread);
  }
  CloseHandle(snapshot);
}

ThreadFreezer::~ThreadFreezer() {
  for (HANDLE thread : threads_) {
    ResumeThread(thread);
    CloseHandle(thread);
  }
}

void ThreadFreezer::RelocateInstructionPointers(
    const std::function<std::uintptr_t(std::uintptr_t)>& relocate) {
  for (HANDLE thread : threads_) {
    CONTEXT context{};
    context.ContextFlags = CONTEXT_CONTROL;
    if (!GetThreadContext(thread, &context)) continue;
    std::uintptr_t new_rip = relocate(context.Rip);
    if (new_rip != context.Rip) {
      context.Rip = new_rip;
      SetThreadContext(thread, &context);
    }
  }
}

}  // namespace reode::hook
