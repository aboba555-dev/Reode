// Copyright 2026 The Reode Authors
//
// Geode layouts: anchor-based positioning.
//
// TODO(reode): AxisLayout, RowLayout, ColumnLayout and SimpleAxisLayout.

#include <new>

#include "Geode/cocos/include/cocos2d.h"
#include "Geode/ui/Layout.hpp"
#include "Geode/utils/cocos.hpp"

namespace geode {
namespace {

template <typename T>
T* CreateAutoreleased() {
  auto* object = new (std::nothrow) T();
  if (object != nullptr) object->autorelease();
  return object;
}

}  // namespace

// --- AnchorLayoutOptions ----------------------------------------------------

AnchorLayoutOptions* AnchorLayoutOptions::create() {
  return CreateAutoreleased<AnchorLayoutOptions>();
}

Anchor AnchorLayoutOptions::getAnchor() const { return m_anchor; }

cocos2d::CCPoint AnchorLayoutOptions::getOffset() const { return m_offset; }

AnchorLayoutOptions* AnchorLayoutOptions::setAnchor(Anchor anchor) {
  m_anchor = anchor;
  return this;
}

AnchorLayoutOptions* AnchorLayoutOptions::setOffset(
    cocos2d::CCPoint const& offset) {
  m_offset = offset;
  return this;
}

// --- AnchorLayout -----------------------------------------------------------

AnchorLayout* AnchorLayout::create() {
  return CreateAutoreleased<AnchorLayout>();
}

cocos2d::CCPoint AnchorLayout::getAnchoredPosition(
    cocos2d::CCNode* in, Anchor anchor, cocos2d::CCPoint const& offset) {
  const cocos2d::CCSize size = in->getContentSize();
  cocos2d::CCPoint point;
  switch (anchor) {
    case Anchor::Center:
      point = cocos2d::CCPoint(size.width / 2, size.height / 2);
      break;
    case Anchor::TopLeft:
      point = cocos2d::CCPoint(0, size.height);
      break;
    case Anchor::Top:
      point = cocos2d::CCPoint(size.width / 2, size.height);
      break;
    case Anchor::TopRight:
      point = cocos2d::CCPoint(size.width, size.height);
      break;
    case Anchor::Right:
      point = cocos2d::CCPoint(size.width, size.height / 2);
      break;
    case Anchor::BottomRight:
      point = cocos2d::CCPoint(size.width, 0);
      break;
    case Anchor::Bottom:
      point = cocos2d::CCPoint(size.width / 2, 0);
      break;
    case Anchor::BottomLeft:
      point = cocos2d::CCPoint(0, 0);
      break;
    case Anchor::Left:
      point = cocos2d::CCPoint(0, size.height / 2);
      break;
  }
  return point + offset;
}

void AnchorLayout::apply(cocos2d::CCNode* on) {
  cocos2d::CCArray* children = on->getChildren();
  if (children == nullptr) return;
  for (unsigned int i = 0; i < children->count(); ++i) {
    auto* child = static_cast<cocos2d::CCNode*>(children->objectAtIndex(i));
    if (auto* options = cast::typeinfo_cast<AnchorLayoutOptions*>(
            child->getLayoutOptions())) {
      child->setPosition(
          getAnchoredPosition(on, options->getAnchor(), options->getOffset()));
    }
  }
}

cocos2d::CCSize AnchorLayout::getSizeHint(cocos2d::CCNode* on) const {
  return on->getContentSize();
}

// --- CopySizeLayout ---------------------------------------------------------

CopySizeLayout* CopySizeLayout::create() {
  auto* layout = CreateAutoreleased<CopySizeLayout>();
  if (layout != nullptr) {
    layout->m_targets = cocos2d::CCArray::create();
    layout->m_targets->retain();
  }
  return layout;
}

CopySizeLayout::~CopySizeLayout() {
  if (m_targets != nullptr) m_targets->release();
}

CopySizeLayout* CopySizeLayout::add(cocos2d::CCNode* target) {
  m_targets->addObject(target);
  return this;
}

CopySizeLayout* CopySizeLayout::remove(cocos2d::CCNode* target) {
  m_targets->removeObject(target);
  return this;
}

void CopySizeLayout::apply(cocos2d::CCNode* in) {
  AnchorLayout::apply(in);
  for (unsigned int i = 0; i < m_targets->count(); ++i) {
    auto* target = static_cast<cocos2d::CCNode*>(m_targets->objectAtIndex(i));
    // Ignore targets that were removed from the node.
    if (target->getParent() != in) continue;
    target->setContentSize(in->getContentSize());
    target->updateLayout();
  }
}

cocos2d::CCSize CopySizeLayout::getSizeHint(cocos2d::CCNode* in) const {
  return in->getContentSize();
}

}  // namespace geode
