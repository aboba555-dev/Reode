// Copyright 2026 The Reode Authors
//
// Geode's additions to cocos2d: node IDs, user objects and flags, layouts,
// anchored children, node-owned event listeners, $modify fields, plus the
// short get() accessors of cocos2d singletons.

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Geode/cocos/base_nodes/CCNode.h"
#include "Geode/cocos/cocoa/CCArray.h"
#include "Geode/cocos/include/cocos2d.h"
#include "Geode/loader/Event.hpp"
#include "Geode/modify/Field.hpp"
#include "Geode/ui/Layout.hpp"
#include "Geode/utils/cocos.hpp"
#include "node_metadata.h"

namespace reode::geode_shim {
namespace {

using cocos2d::CCArray;
using cocos2d::CCNode;
using cocos2d::CCObject;

NodeMetadataStore& Store() { return NodeMetadataStore::Instance(); }

std::vector<CCNode*> ChildrenOf(CCNode* node) {
  std::vector<CCNode*> children;
  CCArray* array = node->getChildren();
  if (array == nullptr) return children;
  for (unsigned int i = 0; i < array->count(); ++i) {
    children.push_back(static_cast<CCNode*>(array->objectAtIndex(i)));
  }
  return children;
}

CCNode* FindChildRecursive(CCNode* node, std::string_view id) {
  std::vector<CCNode*> children = ChildrenOf(node);
  for (CCNode* child : children) {
    if (child->getID() == id) return child;
  }
  for (CCNode* child : children) {
    if (CCNode* found = FindChildRecursive(child, id)) return found;
  }
  return nullptr;
}

// Rewrites the children array of `parent` into `order` and gives the
// children increasing arrival orders, so that cocos2d's stable z-order sort
// keeps that order among equal z values.
void ApplyChildOrder(CCNode* parent, const std::vector<CCNode*>& order) {
  CCArray* array = parent->getChildren();
  if (array == nullptr || order.empty()) return;
  unsigned int arrival = order.front()->getOrderOfArrival();
  for (CCNode* child : order)
    arrival = std::min(arrival, child->getOrderOfArrival());
  for (CCNode* child : order) {
    child->retain();
    array->removeObject(child, false);
  }
  for (CCNode* child : order) {
    array->addObject(child);
    child->release();
    child->setOrderOfArrival(arrival++);
  }
  parent->sortAllChildren();
}

void Insert(CCNode* parent, CCNode* child, CCNode* reference, bool after) {
  std::vector<CCNode*> order = ChildrenOf(parent);
  const bool has_reference =
      reference != nullptr &&
      std::find(order.begin(), order.end(), reference) != order.end();
  parent->addChild(child, has_reference ? reference->getZOrder() : 0);
  if (!has_reference) {
    if (after) return;  // addChild already appends.
    order.insert(order.begin(), child);
  } else {
    auto it = std::find(order.begin(), order.end(), reference);
    order.insert(after ? it + 1 : it, child);
  }
  ApplyChildOrder(parent, order);
}

}  // namespace
}  // namespace reode::geode_shim

namespace cocos2d {

using reode::geode_shim::NodeMetadata;
using reode::geode_shim::NodeMetadataStore;

// --- $modify fields ---------------------------------------------------------

geode::modifier::FieldContainer* CCNode::getFieldContainer(
    char const* /*forClass*/) {
  NodeMetadata& metadata = NodeMetadataStore::Instance().Get(this);
  if (metadata.fields == nullptr) {
    metadata.fields = std::make_unique<geode::modifier::FieldContainer>();
  }
  return metadata.fields.get();
}

// --- IDs --------------------------------------------------------------------

geode::ZStringView CCNode::getID() {
  static const std::string kEmpty;
  NodeMetadata* metadata = NodeMetadataStore::Instance().Find(this);
  return metadata != nullptr ? metadata->id : kEmpty;
}

void CCNode::setID(std::string id) {
  NodeMetadataStore::Instance().Get(this).id = std::move(id);
}

CCNode* CCNode::getChildByID(std::string_view id) {
  for (CCNode* child : reode::geode_shim::ChildrenOf(this)) {
    if (child->getID() == id) return child;
  }
  return nullptr;
}

CCNode* CCNode::getChildByIDRecursive(std::string_view id) {
  return reode::geode_shim::FindChildRecursive(this, id);
}

CCNode* CCNode::querySelector(std::string_view query) {
  CCNode* current = this;
  bool immediate = false;
  std::size_t pos = 0;
  while (pos < query.size() && current != nullptr) {
    if (query[pos] == ' ') {
      ++pos;
      continue;
    }
    if (query[pos] == '>') {
      immediate = true;
      ++pos;
      continue;
    }
    std::size_t end = query.find_first_of(" >", pos);
    if (end == std::string_view::npos) end = query.size();
    std::string_view id = query.substr(pos, end - pos);
    current = immediate ? current->getChildByID(id)
                        : current->getChildByIDRecursive(id);
    immediate = false;
    pos = end;
  }
  return current == this ? nullptr : current;
}

void CCNode::removeChildByID(std::string_view id) {
  if (CCNode* child = getChildByID(id)) removeChild(child);
}

// --- Child order ------------------------------------------------------------

void CCNode::insertBefore(CCNode* child, CCNode* before) {
  reode::geode_shim::Insert(this, child, before, /*after=*/false);
}

void CCNode::insertAfter(CCNode* child, CCNode* after) {
  reode::geode_shim::Insert(this, child, after, /*after=*/true);
}

void CCNode::swapChildIndices(CCNode* first, CCNode* second) {
  std::vector<CCNode*> order = reode::geode_shim::ChildrenOf(this);
  auto a = std::find(order.begin(), order.end(), first);
  auto b = std::find(order.begin(), order.end(), second);
  if (a == order.end() || b == order.end()) return;
  std::iter_swap(a, b);
  reode::geode_shim::ApplyChildOrder(this, order);
}

bool CCNode::hasAncestor(CCNode* ancestor) {
  if (ancestor == nullptr)
    ancestor = CCDirector::sharedDirector()->getRunningScene();
  for (CCNode* node = getParent(); node != nullptr; node = node->getParent()) {
    if (node == ancestor) return true;
  }
  return false;
}

// --- User objects and flags -------------------------------------------------

void CCNode::setUserObject(std::string id, CCObject* object) {
  NodeMetadata& metadata = NodeMetadataStore::Instance().Get(this);
  auto it = metadata.user_objects.find(id);
  if (it != metadata.user_objects.end()) {
    if (it->second != nullptr) it->second->release();
    metadata.user_objects.erase(it);
  }
  if (object != nullptr) {
    object->retain();
    metadata.user_objects.emplace(std::move(id), object);
  }
}

CCObject* CCNode::getUserObject(std::string_view id) {
  NodeMetadata* metadata = NodeMetadataStore::Instance().Find(this);
  if (metadata == nullptr) return nullptr;
  auto it = metadata->user_objects.find(std::string(id));
  return it == metadata->user_objects.end() ? nullptr : it->second;
}

void CCNode::addTether(CCObject* object) {
  if (object == nullptr) return;
  object->retain();
  NodeMetadataStore::Instance().Get(this).tethers.push_back(object);
}

void CCNode::removeTether(CCObject* object) {
  NodeMetadata* metadata = NodeMetadataStore::Instance().Find(this);
  if (metadata == nullptr) return;
  auto it =
      std::find(metadata->tethers.begin(), metadata->tethers.end(), object);
  if (it == metadata->tethers.end()) return;
  metadata->tethers.erase(it);
  object->release();
}

void CCNode::setUserFlag(std::string id, bool state) {
  NodeMetadata& metadata = NodeMetadataStore::Instance().Get(this);
  if (state) {
    metadata.user_flags.insert(std::move(id));
  } else {
    metadata.user_flags.erase(id);
  }
}

bool CCNode::getUserFlag(std::string_view id) {
  NodeMetadata* metadata = NodeMetadataStore::Instance().Find(this);
  return metadata != nullptr && metadata->user_flags.contains(std::string(id));
}

// --- Layouts ----------------------------------------------------------------

void CCNode::setLayout(geode::Layout* layout, bool apply, bool respectAnchor) {
  if (respectAnchor && isIgnoreAnchorPointForPosition()) {
    // Keep the node visually in place while switching to anchor-based
    // positioning: its position now refers to the anchor point.
    CCPoint anchor = getAnchorPointInPoints();
    ignoreAnchorPointForPosition(false);
    setPosition(getPosition() + anchor);
  }
  NodeMetadata& metadata = NodeMetadataStore::Instance().Get(this);
  if (layout != nullptr) layout->retain();
  if (metadata.layout != nullptr) metadata.layout->release();
  metadata.layout = layout;
  if (apply) updateLayout();
}

geode::Layout* CCNode::getLayout() {
  NodeMetadata* metadata = NodeMetadataStore::Instance().Find(this);
  return metadata != nullptr ? metadata->layout : nullptr;
}

void CCNode::updateLayout(bool updateChildOrder) {
  if (updateChildOrder) sortAllChildren();
  if (geode::Layout* layout = getLayout()) layout->apply(this);
}

void CCNode::setLayoutOptions(geode::LayoutOptions* options, bool apply) {
  NodeMetadata& metadata = NodeMetadataStore::Instance().Get(this);
  if (options != nullptr) options->retain();
  if (metadata.layout_options != nullptr) metadata.layout_options->release();
  metadata.layout_options = options;
  if (apply && getParent() != nullptr) getParent()->updateLayout();
}

geode::LayoutOptions* CCNode::getLayoutOptions() {
  NodeMetadata* metadata = NodeMetadataStore::Instance().Find(this);
  return metadata != nullptr ? metadata->layout_options : nullptr;
}

void CCNode::addChildAtPosition(CCNode* child, geode::Anchor anchor,
                                CCPoint const& offset, bool useAnchorLayout) {
  addChildAtPosition(child, anchor, offset, child->getAnchorPoint(),
                     useAnchorLayout);
}

void CCNode::addChildAtPosition(CCNode* child, geode::Anchor anchor,
                                CCPoint const& offset,
                                CCPoint const& nodeAnchor,
                                bool useAnchorLayout) {
  child->setAnchorPoint(nodeAnchor);
  if (useAnchorLayout) {
    auto* options = geode::AnchorLayoutOptions::create();
    options->setAnchor(anchor);
    options->setOffset(offset);
    child->setLayoutOptions(options, false);
  }
  addChild(child);
  child->setPosition(
      geode::AnchorLayout::getAnchoredPosition(this, anchor, offset));
}

void CCNode::updateAnchoredPosition(geode::Anchor anchor,
                                    CCPoint const& offset) {
  updateAnchoredPosition(anchor, offset, getAnchorPoint());
}

void CCNode::updateAnchoredPosition(geode::Anchor anchor, CCPoint const& offset,
                                    CCPoint const& nodeAnchor) {
  setAnchorPoint(nodeAnchor);
  if (auto* options = geode::cast::typeinfo_cast<geode::AnchorLayoutOptions*>(
          getLayoutOptions())) {
    options->setAnchor(anchor);
    options->setOffset(offset);
  }
  if (getParent() != nullptr) {
    setPosition(
        geode::AnchorLayout::getAnchoredPosition(getParent(), anchor, offset));
  }
}

// --- Content size helpers ---------------------------------------------------

void CCNode::setScaledContentSize(CCSize const& size) {
  setContentSize(CCSize(size.width / getScaleX(), size.height / getScaleY()));
}

void CCNode::setContentWidth(float width) {
  setContentSize(CCSize(width, getContentSize().height));
}

void CCNode::setContentHeight(float height) {
  setContentSize(CCSize(getContentSize().width, height));
}

float CCNode::getContentWidth() const { return getContentSize().width; }

float CCNode::getContentHeight() const { return getContentSize().height; }

float CCNode::getScaledContentWidth() const {
  return getContentSize().width * const_cast<CCNode*>(this)->getScaleX();
}

float CCNode::getScaledContentHeight() const {
  return getContentSize().height * const_cast<CCNode*>(this)->getScaleY();
}

// --- Node-owned event listeners ---------------------------------------------

geode::comm::ListenerHandle* CCNode::addEventListenerInternal(
    std::string id, geode::comm::ListenerHandle handle) {
  NodeMetadata& metadata = NodeMetadataStore::Instance().Get(this);
  if (!id.empty()) removeEventListener(id);
  auto owned = std::make_unique<geode::comm::ListenerHandle>(std::move(handle));
  geode::comm::ListenerHandle* result = owned.get();
  metadata.listeners.emplace_back(std::move(id), std::move(owned));
  return result;
}

void CCNode::removeEventListener(geode::comm::ListenerHandle* handle) {
  NodeMetadata* metadata = NodeMetadataStore::Instance().Find(this);
  if (metadata == nullptr) return;
  std::erase_if(metadata->listeners, [&](const auto& entry) {
    return entry.second.get() == handle;
  });
}

void CCNode::removeEventListener(std::string_view id) {
  NodeMetadata* metadata = NodeMetadataStore::Instance().Find(this);
  if (metadata == nullptr) return;
  std::erase_if(metadata->listeners,
                [&](const auto& entry) { return entry.first == id; });
}

geode::comm::ListenerHandle* CCNode::getEventListener(std::string_view id) {
  NodeMetadata* metadata = NodeMetadataStore::Instance().Find(this);
  if (metadata == nullptr) return nullptr;
  for (const auto& [key, handle] : metadata->listeners) {
    if (key == id) return handle.get();
  }
  return nullptr;
}

size_t CCNode::getEventListenerCount() {
  NodeMetadata* metadata = NodeMetadataStore::Instance().Find(this);
  return metadata != nullptr ? metadata->listeners.size() : 0;
}

// --- CCArray additions and singleton accessors ------------------------------

CCObject* CCArray::firstObject() {
  return count() > 0 ? objectAtIndex(0) : nullptr;
}

CCArray* CCArray::shallowCopy() { return CCArray::createWithArray(this); }

CCDirector* CCDirector::get() { return sharedDirector(); }

CCScene* CCScene::get() {
  return CCDirector::sharedDirector()->getRunningScene();
}

CCSpriteFrameCache* CCSpriteFrameCache::get() {
  return sharedSpriteFrameCache();
}

CCTextureCache* CCTextureCache::get() { return sharedTextureCache(); }

CCTouchDispatcher* CCTouchDispatcher::get() {
  return CCDirector::sharedDirector()->getTouchDispatcher();
}

CCKeyboardDispatcher* CCKeyboardDispatcher::get() {
  return CCDirector::sharedDirector()->getKeyboardDispatcher();
}

CCScheduler* CCScheduler::get() {
  return CCDirector::sharedDirector()->getScheduler();
}

CCEGLView* CCEGLView::get() {
  return CCDirector::sharedDirector()->getOpenGLView();
}

CCFileUtils* CCFileUtils::get() { return sharedFileUtils(); }

CCApplication* CCApplication::get() { return sharedApplication(); }

}  // namespace cocos2d

namespace geode {

size_t modifier::getFieldIndexForClass(char const* name) {
  return reode::geode_shim::NodeMetadataStore::Instance().FieldIndexFor(name);
}

// --- DestructorLock ---------------------------------------------------------

namespace {

std::unordered_set<void*>& Locked() {
  static auto* const kLocked = new std::unordered_set<void*>();
  return *kLocked;
}

}  // namespace

void DestructorLock::addLock(void* self) { Locked().insert(self); }
void DestructorLock::removeLock(void* self) { Locked().erase(self); }
bool DestructorLock::isLocked(void* self) { return Locked().contains(self); }
void DestructorLock::addLock(cocos2d::CCNode* self) {
  addLock(static_cast<void*>(self));
}
void DestructorLock::removeLock(cocos2d::CCNode* self) {
  removeLock(static_cast<void*>(self));
}
bool DestructorLock::isLocked(cocos2d::CCNode* self) {
  return isLocked(static_cast<void*>(self));
}

}  // namespace geode
