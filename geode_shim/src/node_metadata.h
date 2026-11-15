// Copyright 2026 The Reode Authors

#ifndef REODE_GEODE_SHIM_SRC_NODE_METADATA_H_
#define REODE_GEODE_SHIM_SRC_NODE_METADATA_H_

#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace cocos2d {
class CCNode;
class CCObject;
}  // namespace cocos2d

namespace geode {
class Layout;
class LayoutOptions;
namespace comm {
class ListenerHandle;
}  // namespace comm
namespace modifier {
class FieldContainer;
}  // namespace modifier
}  // namespace geode

namespace reode::geode_shim {

// Everything Geode attaches to a cocos2d node. cocos2d::CCNode's layout is
// fixed by libcocos2d.dll, so this lives in a side table and is destroyed
// together with the node (see NodeMetadataStore).
struct NodeMetadata {
  NodeMetadata();
  ~NodeMetadata();
  NodeMetadata(const NodeMetadata&) = delete;
  NodeMetadata& operator=(const NodeMetadata&) = delete;

  std::unique_ptr<geode::modifier::FieldContainer> fields;
  std::string id;
  // Retained objects.
  std::unordered_map<std::string, cocos2d::CCObject*> user_objects;
  std::vector<cocos2d::CCObject*> tethers;
  geode::Layout* layout = nullptr;
  geode::LayoutOptions* layout_options = nullptr;
  std::unordered_set<std::string> user_flags;
  // Scale the node had before SimpleAxisLayout first scaled it.
  std::optional<float> layout_base_scale;
  // Event listeners owned by the node, by id ("" = anonymous).
  std::vector<
      std::pair<std::string, std::unique_ptr<geode::comm::ListenerHandle>>>
      listeners;
};

class NodeMetadataStore {
 public:
  static NodeMetadataStore& Instance();

  NodeMetadataStore(const NodeMetadataStore&) = delete;
  NodeMetadataStore& operator=(const NodeMetadataStore&) = delete;

  // Returns the metadata of `node`, creating it if needed.
  NodeMetadata& Get(cocos2d::CCNode* node);
  // Returns the metadata of `node` or nullptr, without creating it.
  NodeMetadata* Find(cocos2d::CCNode* node);

  // Detaches the metadata of a node being destroyed. The caller destroys it
  // outside the lock, because field and listener destructors may create or
  // destroy other nodes.
  std::unique_ptr<NodeMetadata> Release(cocos2d::CCNode* node);

  // Global index for a $modify class name (Field.hpp).
  std::size_t FieldIndexFor(const char* class_name);

 private:
  NodeMetadataStore() = default;
  ~NodeMetadataStore() = default;

  void EnsureDestructorHook();

  std::once_flag hook_once_;
  std::recursive_mutex mutex_;
  std::unordered_map<cocos2d::CCNode*, std::unique_ptr<NodeMetadata>> nodes_;
  std::unordered_map<std::string, std::size_t> field_indices_;
};

}  // namespace reode::geode_shim

#endif  // REODE_GEODE_SHIM_SRC_NODE_METADATA_H_
