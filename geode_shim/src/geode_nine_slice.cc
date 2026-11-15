// Copyright 2026 The Reode Authors
//
// geode::NineSlice: a sprite cut into a 3x3 grid whose corners keep their
// size while the edges and the center stretch to the node's content size.
//
// The nine parts are sprites in one batch node. The batch node is a real
// child, so cocos2d draws it, but getChildren() and getChildrenCount() hide
// it, so that callers only see the children they added themselves.

#include <algorithm>
#include <array>
#include <climits>
#include <cstddef>
#include <memory>

#include "Geode/cocos/include/cocos2d.h"
#include "Geode/ui/NineSlice.hpp"

using cocos2d::CCArray;
using cocos2d::CCNode;
using cocos2d::CCNodeRGBA;
using cocos2d::CCObject;
using cocos2d::CCPoint;
using cocos2d::CCRect;
using cocos2d::CCSize;
using cocos2d::CCSprite;
using cocos2d::CCSpriteBatchNode;
using cocos2d::CCSpriteFrame;
using cocos2d::CCTexture2D;

namespace geode {
namespace {

// Grid parts, row by row from the top left.
enum Part : std::size_t {
  kTopLeft,
  kTop,
  kTopRight,
  kLeft,
  kCenter,
  kRight,
  kBottomLeft,
  kBottom,
  kBottomRight,
  kPartCount,
};

constexpr int kBatchZOrder = INT_MIN;

// Splits `length` into start, middle and end segments for insets `start` and
// `end`. When the insets do not fit, they shrink proportionally.
std::array<float, 3> Segments(float length, float start, float end) {
  float insets = start + end;
  if (insets > length && insets > 0) {
    float factor = length / insets;
    start *= factor;
    end *= factor;
  }
  return {start, std::max(0.0f, length - start - end), end};
}

}  // namespace

class NineSlice::Impl {
 public:
  ~Impl() {
    if (visible_children != nullptr) visible_children->release();
  }

  CCTexture2D* texture = nullptr;
  // The source image within `texture`, in points, in its upright size.
  CCRect rect;
  bool rotated = false;
  Insets insets;
  float scale_multiplier = 1.0f;
  bool repeat_center = false;
  CCSpriteBatchNode* batch = nullptr;
  std::array<CCSprite*, kPartCount> parts = {};
  // Size of each part in the source image.
  std::array<CCSize, kPartCount> source_sizes = {};
  CCArray* visible_children = nullptr;
};

NineSlice::NineSlice() : m_impl(std::make_unique<Impl>()) {}

NineSlice::~NineSlice() = default;

NineSlice* NineSlice::create(ZStringView file, CCRect const& rect,
                             Insets const& insets) {
  auto* node = new NineSlice();
  if (node->initWithFile(file, rect, insets)) {
    node->autorelease();
    return node;
  }
  delete node;
  return nullptr;
}

NineSlice* NineSlice::createWithSpriteFrameName(ZStringView spriteFrameName,
                                                Insets const& insets) {
  auto* node = new NineSlice();
  if (node->initWithSpriteFrameName(spriteFrameName, insets)) {
    node->autorelease();
    return node;
  }
  delete node;
  return nullptr;
}

NineSlice* NineSlice::createWithSpriteFrame(CCSpriteFrame* spriteFrame,
                                            Insets const& insets) {
  auto* node = new NineSlice();
  if (node->initWithSpriteFrame(spriteFrame, insets)) {
    node->autorelease();
    return node;
  }
  delete node;
  return nullptr;
}

bool NineSlice::initWithFile(ZStringView file, CCRect const& rect,
                             Insets const& insets) {
  CCTexture2D* texture =
      cocos2d::CCTextureCache::sharedTextureCache()->addImage(file.c_str(),
                                                              false);
  if (texture == nullptr) return false;
  m_impl->texture = texture;
  m_impl->rotated = false;
  m_impl->rect = rect.size.width > 0 && rect.size.height > 0
                     ? rect
                     : CCRect(CCPoint(0, 0), texture->getContentSize());
  setup(insets, m_impl->rect);
  return true;
}

bool NineSlice::initWithSpriteFrameName(ZStringView spriteFrameName,
                                        Insets const& insets) {
  return initWithSpriteFrame(
      cocos2d::CCSpriteFrameCache::sharedSpriteFrameCache()->spriteFrameByName(
          spriteFrameName.c_str()),
      insets);
}

bool NineSlice::initWithSpriteFrame(CCSpriteFrame* spriteFrame,
                                    Insets const& insets) {
  if (spriteFrame == nullptr || spriteFrame->getTexture() == nullptr) {
    return false;
  }
  m_impl->texture = spriteFrame->getTexture();
  m_impl->rect = spriteFrame->getRect();
  m_impl->rotated = spriteFrame->isRotated();
  setup(insets, m_impl->rect);
  return true;
}

void NineSlice::setup(Insets const& insets, CCRect const& rect) {
  CCNodeRGBA::init();
  setAnchorPoint(CCPoint(0.5f, 0.5f));
  m_impl->rect = rect;
  m_impl->visible_children = CCArray::create();
  m_impl->visible_children->retain();
  m_impl->batch = CCSpriteBatchNode::createWithTexture(m_impl->texture);
  CCNodeRGBA::addChild(m_impl->batch, kBatchZOrder, 0);
  m_impl->insets = insets;
  createSprites();
  setContentSize(rect.size);
}

void NineSlice::createSprites() {
  Impl& impl = *m_impl;
  for (CCSprite*& part : impl.parts) {
    if (part != nullptr) part->removeFromParent();
    part = nullptr;
  }
  const CCSize size = impl.rect.size;
  Insets insets = impl.insets;
  // Without insets, the image is split into equal thirds.
  if (insets == Insets{}) {
    insets = {size.height / 3, size.width / 3, size.height / 3,
              size.width / 3};
  }
  std::array<float, 3> columns = Segments(size.width, insets.left,
                                          insets.right);
  std::array<float, 3> rows = Segments(size.height, insets.top, insets.bottom);
  float v = 0;  // Distance from the top of the image.
  for (std::size_t row = 0; row < 3; ++row) {
    float u = 0;  // Distance from the left of the image.
    for (std::size_t column = 0; column < 3; ++column) {
      std::size_t index = row * 3 + column;
      CCSize part_size(columns[column], rows[row]);
      impl.source_sizes[index] = part_size;
      // A rotated frame is stored turned by 90 degrees, so a region at (u, v)
      // of the upright image starts at (height - v - h, u) in the texture.
      CCRect region =
          impl.rotated
              ? CCRect(impl.rect.origin.x + size.height - v - part_size.height,
                       impl.rect.origin.y + u, part_size.width,
                       part_size.height)
              : CCRect(impl.rect.origin.x + u, impl.rect.origin.y + v,
                       part_size.width, part_size.height);
      CCSpriteFrame* frame = CCSpriteFrame::createWithTexture(
          impl.texture, region, impl.rotated, CCPoint(0, 0), part_size);
      CCSprite* sprite = CCSprite::createWithSpriteFrame(frame);
      if (sprite != nullptr) {
        sprite->setAnchorPoint(CCPoint(0, 0));
        impl.batch->addChild(sprite);
      }
      impl.parts[index] = sprite;
      u += columns[column];
    }
    v += rows[row];
  }
  updateSprites();
}

void NineSlice::updateSprites() {
  Impl& impl = *m_impl;
  if (impl.batch == nullptr) return;
  const CCSize size = getContentSize();
  // Insets as drawn: the source size of the corner parts, scaled.
  float scale = impl.scale_multiplier;
  std::array<float, 3> columns =
      Segments(size.width, impl.source_sizes[kTopLeft].width * scale,
               impl.source_sizes[kTopRight].width * scale);
  std::array<float, 3> rows =
      Segments(size.height, impl.source_sizes[kTopLeft].height * scale,
               impl.source_sizes[kBottomLeft].height * scale);
  float top = size.height;
  for (std::size_t row = 0; row < 3; ++row) {
    float x = 0;
    top -= rows[row];
    for (std::size_t column = 0; column < 3; ++column) {
      std::size_t index = row * 3 + column;
      CCSprite* sprite = impl.parts[index];
      const CCSize source = impl.source_sizes[index];
      if (sprite != nullptr) {
        bool shown = source.width > 0 && source.height > 0 &&
                     columns[column] > 0 && rows[row] > 0;
        sprite->setVisible(shown);
        if (shown) {
          sprite->setPosition(CCPoint(x, top));
          sprite->setScaleX(columns[column] / source.width);
          sprite->setScaleY(rows[row] / source.height);
        }
      }
      x += columns[column];
    }
  }
  impl.batch->setContentSize(size);
}

void NineSlice::setScaleMultiplier(float scaleMultiplier) {
  m_impl->scale_multiplier = scaleMultiplier;
  updateSprites();
}

float NineSlice::getScaleMultiplier() { return m_impl->scale_multiplier; }

// TODO(reode): draw a repeated center; it is always stretched for now.
void NineSlice::setRepeatCenter(bool repeat) {
  m_impl->repeat_center = repeat;
}

bool NineSlice::getRepeatCenter() const { return m_impl->repeat_center; }

void NineSlice::setInsets(Insets const& insets) {
  m_impl->insets = insets;
  if (m_impl->batch != nullptr) createSprites();
}

NineSlice::Insets NineSlice::getInsets() const { return m_impl->insets; }

void NineSlice::setInsetTop(float top) {
  Insets insets = m_impl->insets;
  insets.top = top;
  setInsets(insets);
}

void NineSlice::setInsetRight(float right) {
  Insets insets = m_impl->insets;
  insets.right = right;
  setInsets(insets);
}

void NineSlice::setInsetBottom(float bottom) {
  Insets insets = m_impl->insets;
  insets.bottom = bottom;
  setInsets(insets);
}

void NineSlice::setInsetLeft(float left) {
  Insets insets = m_impl->insets;
  insets.left = left;
  setInsets(insets);
}

float NineSlice::getInsetTop() const { return m_impl->insets.top; }
float NineSlice::getInsetRight() const { return m_impl->insets.right; }
float NineSlice::getInsetBottom() const { return m_impl->insets.bottom; }
float NineSlice::getInsetLeft() const { return m_impl->insets.left; }

CCSprite* NineSlice::getTopLeft() { return m_impl->parts[kTopLeft]; }
CCSprite* NineSlice::getTopRight() { return m_impl->parts[kTopRight]; }
CCSprite* NineSlice::getBottomLeft() { return m_impl->parts[kBottomLeft]; }
CCSprite* NineSlice::getBottomRight() { return m_impl->parts[kBottomRight]; }
CCSprite* NineSlice::getTop() { return m_impl->parts[kTop]; }
CCSprite* NineSlice::getBottom() { return m_impl->parts[kBottom]; }
CCSprite* NineSlice::getLeft() { return m_impl->parts[kLeft]; }
CCSprite* NineSlice::getRight() { return m_impl->parts[kRight]; }
CCSprite* NineSlice::getCenter() { return m_impl->parts[kCenter]; }

CCSpriteBatchNode* NineSlice::getBatchNode() { return m_impl->batch; }

void NineSlice::addChild(CCNode* child, int zOrder, int tag) {
  CCNodeRGBA::addChild(child, zOrder, tag);
}

void NineSlice::removeChild(CCNode* child, bool cleanup) {
  CCNodeRGBA::removeChild(child, cleanup);
}

CCArray* NineSlice::getChildren() {
  CCArray* visible = m_impl->visible_children;
  if (visible == nullptr) return CCNodeRGBA::getChildren();
  visible->removeAllObjects();
  if (CCArray* all = CCNodeRGBA::getChildren()) {
    for (unsigned int i = 0; i < all->count(); ++i) {
      CCObject* child = all->objectAtIndex(i);
      if (child != m_impl->batch) visible->addObject(child);
    }
  }
  return visible;
}

unsigned int NineSlice::getChildrenCount() const {
  unsigned int count = CCNodeRGBA::getChildrenCount();
  return m_impl->batch != nullptr && count > 0 ? count - 1 : count;
}

void NineSlice::setContentSize(CCSize const& size) {
  CCNodeRGBA::setContentSize(size);
  updateSprites();
}

void NineSlice::visit() { CCNodeRGBA::visit(); }

// The parts are children of the batch node, which does not pass colors on,
// so colors and opacity are applied to them directly.
void NineSlice::setColor(cocos2d::ccColor3B const& color) {
  CCNodeRGBA::setColor(color);
  for (CCSprite* part : m_impl->parts) {
    if (part != nullptr) part->setColor(getDisplayedColor());
  }
}

void NineSlice::setOpacity(GLubyte opacity) {
  CCNodeRGBA::setOpacity(opacity);
  for (CCSprite* part : m_impl->parts) {
    if (part != nullptr) part->setOpacity(getDisplayedOpacity());
  }
}

void NineSlice::setOpacityModifyRGB(bool var) {
  for (CCSprite* part : m_impl->parts) {
    if (part != nullptr) part->setOpacityModifyRGB(var);
  }
}

void NineSlice::updateDisplayedOpacity(GLubyte parentOpacity) {
  CCNodeRGBA::updateDisplayedOpacity(parentOpacity);
  for (CCSprite* part : m_impl->parts) {
    if (part != nullptr) part->setOpacity(getDisplayedOpacity());
  }
}

void NineSlice::updateDisplayedColor(cocos2d::ccColor3B const& parentColor) {
  CCNodeRGBA::updateDisplayedColor(parentColor);
  for (CCSprite* part : m_impl->parts) {
    if (part != nullptr) part->setColor(getDisplayedColor());
  }
}

}  // namespace geode
