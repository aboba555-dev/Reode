// Copyright 2026 The Reode Authors
//
// geode::WeakRefPool / WeakRefController.
//
// The pool keeps one strong reference to every managed object. When that
// is the only reference left, the object counts as dead: the pool lets it
// go and its controller starts returning nullptr.

#include <memory>

#include "Geode/cocos/include/cocos2d.h"
#include "Geode/utils/cocos.hpp"

namespace geode {

bool WeakRefController::isManaged() { return m_obj != nullptr; }

cocos2d::CCObject* WeakRefController::get() const { return m_obj; }

void WeakRefController::swap(cocos2d::CCObject* other) {
  WeakRefPool* pool = WeakRefPool::get();
  std::shared_ptr<WeakRefController> self;
  if (m_obj != nullptr) {
    auto it = pool->m_pool.find(m_obj);
    if (it != pool->m_pool.end()) {
      self = it->second;
      pool->m_pool.erase(it);
    }
    m_obj->release();
  }
  m_obj = other;
  if (other != nullptr) {
    other->retain();
    if (self != nullptr) pool->m_pool[other] = self;
  }
}

WeakRefPool* WeakRefPool::get() {
  static WeakRefPool* const kPool = new WeakRefPool();
  return kPool;
}

std::shared_ptr<WeakRefController> WeakRefPool::manage(cocos2d::CCObject* obj) {
  if (obj == nullptr) return std::make_shared<WeakRefController>();
  if (auto it = m_pool.find(obj); it != m_pool.end()) return it->second;
  auto controller = std::make_shared<WeakRefController>();
  controller->m_obj = obj;
  obj->retain();
  m_pool.emplace(obj, controller);
  return controller;
}

void WeakRefPool::check(cocos2d::CCObject* obj) {
  if (obj == nullptr) return;
  auto it = m_pool.find(obj);
  if (it != m_pool.end() && obj->retainCount() <= 1) forget(obj);
}

void WeakRefPool::forget(cocos2d::CCObject* obj) {
  auto it = m_pool.find(obj);
  if (it == m_pool.end()) return;
  it->second->m_obj = nullptr;
  m_pool.erase(it);
  obj->release();
}

}  // namespace geode
