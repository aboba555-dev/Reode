// Copyright 2026 The Reode Authors

#include "node_metadata.h"

#include <windows.h>

#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include "Geode/cocos/base_nodes/CCNode.h"
#include "Geode/loader/Event.hpp"
#include "Geode/modify/Field.hpp"
#include "Geode/ui/Layout.hpp"
#include "host.h"

namespace reode::geode_shim {
namespace {

using NodeDestructor = void (*)(cocos2d::CCNode* self);

constexpr char kNodeDestructor[] = "??1CCNode@cocos2d@@UEAA@XZ";

constinit NodeDestructor g_original_destructor = nullptr;

void DestructorDetour(cocos2d::CCNode* self) {
  // Metadata goes first: field destructors may still use the node.
  NodeMetadataStore::Instance().Release(self).reset();
  g_original_destructor(self);
}

}  // namespace

NodeMetadata::NodeMetadata() = default;

NodeMetadata::~NodeMetadata() {
  listeners.clear();
  fields.reset();
  for (auto& [key, object] : user_objects) {
    if (object != nullptr) object->release();
  }
  for (cocos2d::CCObject* object : tethers) object->release();
  if (layout != nullptr) layout->release();
  if (layout_options != nullptr) layout_options->release();
}

NodeMetadataStore& NodeMetadataStore::Instance() {
  static NodeMetadataStore* const kInstance = new NodeMetadataStore();
  return *kInstance;
}

void NodeMetadataStore::EnsureDestructorHook() {
  std::call_once(hook_once_, [] {
    HMODULE cocos = GetModuleHandleW(L"libcocos2d.dll");
    void* target =
        cocos != nullptr
            ? reinterpret_cast<void*>(GetProcAddress(cocos, kNodeDestructor))
            : nullptr;
    ReodeStatus status =
        target == nullptr
            ? kReodeNotFound
            : Api().create_hook(
                  nullptr, target, reinterpret_cast<void*>(&DestructorDetour),
                  0, reinterpret_cast<void**>(&g_original_destructor), nullptr);
    if (status != kReodeOk) {
      Log(nullptr, kReodeLogError,
          "could not hook CCNode::~CCNode; node metadata will leak");
    }
  });
}

NodeMetadata& NodeMetadataStore::Get(cocos2d::CCNode* node) {
  EnsureDestructorHook();
  std::lock_guard lock(mutex_);
  auto& metadata = nodes_[node];
  if (metadata == nullptr) metadata = std::make_unique<NodeMetadata>();
  return *metadata;
}

NodeMetadata* NodeMetadataStore::Find(cocos2d::CCNode* node) {
  std::lock_guard lock(mutex_);
  auto it = nodes_.find(node);
  return it == nodes_.end() ? nullptr : it->second.get();
}

std::unique_ptr<NodeMetadata> NodeMetadataStore::Release(
    cocos2d::CCNode* node) {
  std::lock_guard lock(mutex_);
  auto it = nodes_.find(node);
  if (it == nodes_.end()) return nullptr;
  auto metadata = std::move(it->second);
  nodes_.erase(it);
  return metadata;
}

std::size_t NodeMetadataStore::FieldIndexFor(const char* class_name) {
  std::lock_guard lock(mutex_);
  auto [it, inserted] =
      field_indices_.try_emplace(class_name, field_indices_.size());
  return it->second;
}

}  // namespace reode::geode_shim
