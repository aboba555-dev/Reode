// Copyright 2026 The Reode Authors
//
// Geode's event centers. The event templates in the SDK headers live inside
// each mod; a center only maps an event filter to the port that stores the
// listeners for it. Ports and filter clones are created by the filter's own
// virtual functions, so they always match the code that uses them.

#include <cstddef>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "Geode/loader/Event.hpp"

namespace geode::comm {
namespace {

using PortMap = std::unordered_map<std::shared_ptr<BaseFilter>,
                                   std::shared_ptr<OpaquePortBase>,
                                   BaseFilterHash, BaseFilterEqual>;

// Looks up the port of `filter`, upgrading it through `migrate` when it was
// created by an older SDK. Returns nullptr when no port exists.
template <typename Migrate>
PortMap::iterator Find(PortMap& ports, BaseFilter const* filter,
                       Migrate& migrate) {
  auto it = ports.find(filter);
  if (it == ports.end()) return it;
  if (OpaquePortBase* migrated = migrate(it->second.get())) {
    it->second.reset(migrated);
  }
  return it;
}

template <typename Migrate>
PortMap::iterator FindOrCreate(PortMap& ports, BaseFilter const* filter,
                               Migrate& migrate) {
  auto it = Find(ports, filter, migrate);
  if (it != ports.end()) return it;
  std::shared_ptr<BaseFilter> key(filter->clone());
  std::shared_ptr<OpaquePortBase> port(filter->getPort());
  return ports.emplace(std::move(key), std::move(port)).first;
}

}  // namespace

// --- Thread-local center ----------------------------------------------------

class EventCenterThreadLocal::Impl {
 public:
  PortMap ports;
};

EventCenterThreadLocal::EventCenterThreadLocal()
    : m_impl(std::make_unique<Impl>()) {}

EventCenterThreadLocal::~EventCenterThreadLocal() = default;

EventCenterThreadLocal* EventCenterThreadLocal::get() {
  // Leaked on purpose: listeners may outlive thread-exit destructors.
  thread_local EventCenterThreadLocal* const center =
      new EventCenterThreadLocal();
  return center;
}

bool EventCenterThreadLocal::send(BaseFilter const* filter, SendFuncType func,
                                  MigrateFuncType migratePort) noexcept {
  auto it = Find(m_impl->ports, filter, migratePort);
  if (it == m_impl->ports.end()) return false;
  // A listener may remove the last receiver, which erases the entry.
  std::shared_ptr<OpaquePortBase> port = it->second;
  return func(port.get());
}

ListenerHandle EventCenterThreadLocal::addReceiver(
    BaseFilter const* filter, AddFuncType func,
    MigrateFuncType migratePort) noexcept {
  auto it = FindOrCreate(m_impl->ports, filter, migratePort);
  ReceiverHandle handle = func(it->second.get());
  return ListenerHandle(it->first, handle, nullptr);
}

size_t EventCenterThreadLocal::getReceiverCount(
    BaseFilter const* filter, SizeFuncType func,
    MigrateFuncType migratePort) noexcept {
  auto it = Find(m_impl->ports, filter, migratePort);
  if (it == m_impl->ports.end()) return 0;
  return func(it->second.get());
}

size_t EventCenterThreadLocal::removeReceiver(
    BaseFilter const* filter, RemoveFuncType func,
    MigrateFuncType migratePort) noexcept {
  auto it = Find(m_impl->ports, filter, migratePort);
  if (it == m_impl->ports.end()) return static_cast<size_t>(-1);
  size_t remaining = func(it->second.get());
  if (remaining == 0) m_impl->ports.erase(it);
  return remaining;
}

// --- Global (thread-safe) center --------------------------------------------

class EventCenterGlobal::Impl {
 public:
  std::recursive_mutex mutex;
  PortMap ports;
};

EventCenterGlobal::EventCenterGlobal() : m_impl(std::make_unique<Impl>()) {}

EventCenterGlobal::~EventCenterGlobal() = default;

EventCenterGlobal* EventCenterGlobal::get() {
  static EventCenterGlobal* const kCenter = new EventCenterGlobal();
  return kCenter;
}

// Listeners may add or remove listeners while being called, so the mutex is
// recursive.
bool EventCenterGlobal::send(BaseFilter const* filter, SendFuncType func,
                             MigrateFuncType migratePort) noexcept {
  std::lock_guard lock(m_impl->mutex);
  auto it = Find(m_impl->ports, filter, migratePort);
  if (it == m_impl->ports.end()) return false;
  std::shared_ptr<OpaquePortBase> port = it->second;
  return func(port.get());
}

ListenerHandle EventCenterGlobal::addReceiver(
    BaseFilter const* filter, AddFuncType func,
    MigrateFuncType migratePort) noexcept {
  std::lock_guard lock(m_impl->mutex);
  auto it = FindOrCreate(m_impl->ports, filter, migratePort);
  ReceiverHandle handle = func(it->second.get());
  return ListenerHandle(it->first, handle, nullptr);
}

size_t EventCenterGlobal::getReceiverCount(
    BaseFilter const* filter, SizeFuncType func,
    MigrateFuncType migratePort) noexcept {
  std::lock_guard lock(m_impl->mutex);
  auto it = Find(m_impl->ports, filter, migratePort);
  if (it == m_impl->ports.end()) return 0;
  return func(it->second.get());
}

size_t EventCenterGlobal::removeReceiver(BaseFilter const* filter,
                                         RemoveFuncType func,
                                         MigrateFuncType migratePort) noexcept {
  std::lock_guard lock(m_impl->mutex);
  auto it = Find(m_impl->ports, filter, migratePort);
  if (it == m_impl->ports.end()) return static_cast<size_t>(-1);
  size_t remaining = func(it->second.get());
  if (remaining == 0) m_impl->ports.erase(it);
  return remaining;
}

// --- Legacy center ----------------------------------------------------------

EventCenter* EventCenter::get() {
  static EventCenter* const kCenter = new EventCenter();
  return kCenter;
}

}  // namespace geode::comm
