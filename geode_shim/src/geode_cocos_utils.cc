// Copyright 2026 The Reode Authors
//
// geode::cocos helpers.

#include <windows.h>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <typeinfo>

#include "Geode/cocos/include/cocos2d.h"
#include "Geode/utils/cocos.hpp"
#include "fmt/format.h"

namespace geode::cocos {
namespace {

using cocos2d::CCArray;
using cocos2d::CCNode;
using cocos2d::CCPoint;
using cocos2d::CCRect;
using cocos2d::CCSize;
using cocos2d::CCSprite;

// CCEGLView::getMousePosition is virtual in the game's cocos2d; the SDK
// header declares an inline version that reads fields directly, so the
// exported one is called instead.
CCPoint RawMousePosition() {
  using Fn = CCPoint* (*)(const cocos2d::CCEGLView*, CCPoint*);
  static Fn const kFn = reinterpret_cast<Fn>(GetProcAddress(
      GetModuleHandleW(L"libcocos2d.dll"),
      "?getMousePosition@CCEGLView@cocos2d@@UEBA?AVCCPoint@2@XZ"));
  CCPoint point;
  if (kFn != nullptr) kFn(cocos2d::CCEGLView::sharedOpenGLView(), &point);
  return point;
}

Result<std::uint8_t> HexByte(std::string_view text) {
  std::uint8_t value = 0;
  auto [ptr, error] =
      std::from_chars(text.data(), text.data() + text.size(), value, 16);
  if (error != std::errc() || ptr != text.data() + text.size()) {
    return Err(fmt::format("\"{}\" is not hexadecimal", text));
  }
  return Ok(value);
}

// Expands "#rgb" / "#rgba" shorthand and strips the '#'.
Result<std::string> NormalizeHex(std::string_view text, bool permissive) {
  if (text.starts_with('#')) text.remove_prefix(1);
  std::string hex(text);
  if (permissive && (hex.size() == 3 || hex.size() == 4)) {
    std::string expanded;
    for (char c : hex) expanded += {c, c};
    hex = expanded;
  }
  if (hex.size() != 6 && hex.size() != 8) {
    return Err(fmt::format("\"{}\" is not a hex color", text));
  }
  return Ok(hex);
}

bool SpriteFrameMatches(CCNode* node, cocos2d::CCSpriteFrame* frame) {
  auto* sprite = cast::typeinfo_cast<CCSprite*>(node);
  if (sprite == nullptr || frame == nullptr) return false;
  return sprite->getTexture() == frame->getTexture() &&
         sprite->getTextureRect().equals(frame->getRect());
}

}  // namespace

CCPoint getMousePos() {
  auto* view = cocos2d::CCEGLView::sharedOpenGLView();
  CCSize win = cocos2d::CCDirector::sharedDirector()->getWinSize();
  CCSize frame = view->getFrameSize();
  CCPoint mouse = RawMousePosition();
  if (frame.width <= 0 || frame.height <= 0) return mouse;
  return CCPoint(mouse.x / frame.width * win.width,
                 win.height - mouse.y / frame.height * win.height);
}

bool nodeIsVisible(CCNode* node) {
  for (; node != nullptr; node = node->getParent()) {
    if (!node->isVisible()) return false;
  }
  return true;
}

std::string_view getObjectName(cocos2d::CCObject const* obj) {
  if (obj == nullptr) return "nullptr";
  std::string_view name = typeid(*obj).name();
  for (std::string_view prefix : {"class ", "struct "}) {
    if (name.starts_with(prefix)) name.remove_prefix(prefix.size());
  }
  return name;
}

void limitNodeSize(CCNode* node, CCSize const& size, float def, float min) {
  CCSize content = node->getContentSize();
  float scale = def;
  if (content.width > 0) scale = std::min(scale, size.width / content.width);
  if (content.height > 0) {
    scale = std::min(scale, size.height / content.height);
  }
  node->setScale(std::max(scale, min));
}

void limitNodeWidth(CCNode* node, float width, float def, float min) {
  float content = node->getContentSize().width;
  float scale = content > 0 ? std::min(def, width / content) : def;
  node->setScale(std::max(scale, min));
}

void limitNodeHeight(CCNode* node, float height, float def, float min) {
  float content = node->getContentSize().height;
  float scale = content > 0 ? std::min(def, height / content) : def;
  node->setScale(std::max(scale, min));
}

CCNode* getChildByTagRecursive(CCNode* node, int tag) {
  if (node == nullptr) return nullptr;
  if (CCNode* child = node->getChildByTag(tag)) return child;
  CCArray* children = node->getChildren();
  if (children == nullptr) return nullptr;
  for (unsigned int i = 0; i < children->count(); ++i) {
    if (CCNode* found = getChildByTagRecursive(
            static_cast<CCNode*>(children->objectAtIndex(i)), tag)) {
      return found;
    }
  }
  return nullptr;
}

bool isSpriteFrameName(CCNode* node, const char* name) {
  return SpriteFrameMatches(
      node,
      cocos2d::CCSpriteFrameCache::sharedSpriteFrameCache()->spriteFrameByName(
          name));
}

CCNode* getChildBySpriteFrameName(CCNode* parent, const char* name) {
  if (parent == nullptr) return nullptr;
  auto* frame =
      cocos2d::CCSpriteFrameCache::sharedSpriteFrameCache()->spriteFrameByName(
          name);
  if (frame == nullptr) return nullptr;
  CCArray* children = parent->getChildren();
  if (children == nullptr) return nullptr;
  for (unsigned int i = 0; i < children->count(); ++i) {
    auto* child = static_cast<CCNode*>(children->objectAtIndex(i));
    if (SpriteFrameMatches(child, frame)) return child;
  }
  return nullptr;
}

bool isSpriteName(CCNode* node, const char* name) {
  auto* sprite = cast::typeinfo_cast<CCSprite*>(node);
  if (sprite == nullptr) return false;
  auto* texture = cocos2d::CCTextureCache::sharedTextureCache()->textureForKey(
      cocos2d::CCFileUtils::sharedFileUtils()
          ->fullPathForFilename(name, false)
          .c_str());
  return texture != nullptr && sprite->getTexture() == texture;
}

CCNode* getChildBySpriteName(CCNode* parent, const char* name) {
  if (parent == nullptr) return nullptr;
  CCArray* children = parent->getChildren();
  if (children == nullptr) return nullptr;
  for (unsigned int i = 0; i < children->count(); ++i) {
    auto* child = static_cast<CCNode*>(children->objectAtIndex(i));
    if (isSpriteName(child, name)) return child;
  }
  return nullptr;
}

bool fileExistsInSearchPaths(char const* filename) {
  auto* utils = cocos2d::CCFileUtils::sharedFileUtils();
  return utils->isFileExist(utils->fullPathForFilename(filename, false));
}

CCRect calculateNodeCoverage(CCArray* nodes) {
  CCRect coverage;
  bool first = true;
  for (unsigned int i = 0; nodes != nullptr && i < nodes->count(); ++i) {
    auto* node = static_cast<CCNode*>(nodes->objectAtIndex(i));
    CCRect box = node->boundingBox();
    if (first) {
      coverage = box;
      first = false;
      continue;
    }
    float min_x = std::min(coverage.getMinX(), box.getMinX());
    float min_y = std::min(coverage.getMinY(), box.getMinY());
    float max_x = std::max(coverage.getMaxX(), box.getMaxX());
    float max_y = std::max(coverage.getMaxY(), box.getMaxY());
    coverage = CCRect(min_x, min_y, max_x - min_x, max_y - min_y);
  }
  return coverage;
}

CCRect calculateChildCoverage(CCNode* parent) {
  return calculateNodeCoverage(parent->getChildren());
}

cocos2d::CCScene* switchToScene(cocos2d::CCScene* scene) {
  cocos2d::CCDirector::sharedDirector()->replaceScene(
      cocos2d::CCTransitionFade::create(0.5f, scene));
  return scene;
}

cocos2d::CCScene* switchToScene(cocos2d::CCLayer* layer) {
  auto* scene = cocos2d::CCScene::create();
  scene->addChild(layer);
  return switchToScene(scene);
}

Result<cocos2d::ccColor3B> cc3bFromHexString(std::string_view hexValue,
                                             bool permissive) {
  auto hex = NormalizeHex(hexValue, permissive);
  if (!hex) return Err(hex.unwrapErr());
  std::string_view text = hex.unwrap();
  if (text.size() != 6) return Err("expected 6 hex digits");
  auto r = HexByte(text.substr(0, 2));
  auto g = HexByte(text.substr(2, 2));
  auto b = HexByte(text.substr(4, 2));
  if (!r || !g || !b) return Err("invalid hex color");
  return Ok(cocos2d::ccc3(r.unwrap(), g.unwrap(), b.unwrap()));
}

Result<cocos2d::ccColor4B> cc4bFromHexString(std::string_view hexValue,
                                             bool requireAlpha,
                                             bool permissive) {
  auto hex = NormalizeHex(hexValue, permissive);
  if (!hex) return Err(hex.unwrapErr());
  std::string text = hex.unwrap();
  if (text.size() == 6) {
    if (requireAlpha) return Err("expected 8 hex digits");
    text += "ff";
  }
  auto r = HexByte(std::string_view(text).substr(0, 2));
  auto g = HexByte(std::string_view(text).substr(2, 2));
  auto b = HexByte(std::string_view(text).substr(4, 2));
  auto a = HexByte(std::string_view(text).substr(6, 2));
  if (!r || !g || !b || !a) return Err("invalid hex color");
  return Ok(cocos2d::ccc4(r.unwrap(), g.unwrap(), b.unwrap(), a.unwrap()));
}

std::string cc3bToHexString(cocos2d::ccColor3B const& color) {
  return fmt::format("{:02x}{:02x}{:02x}", color.r, color.g, color.b);
}

std::string cc4bToHexString(cocos2d::ccColor4B const& color) {
  return fmt::format("{:02x}{:02x}{:02x}{:02x}", color.r, color.g, color.b,
                     color.a);
}

// Gives every touch-handling descendant of `node` a higher priority than
// the node itself, so nested menus receive touches before their parents.
void handleTouchPriority(CCNode* node, bool force) {
  auto* layer = cast::typeinfo_cast<cocos2d::CCLayer*>(node);
  int base = layer != nullptr ? layer->getTouchPriority()
                              : cocos2d::kCCMenuHandlerPriority;
  CCArray* children = node->getChildren();
  if (children == nullptr) return;
  for (unsigned int i = 0; i < children->count(); ++i) {
    auto* child = static_cast<CCNode*>(children->objectAtIndex(i));
    if (auto* child_layer = cast::typeinfo_cast<cocos2d::CCLayer*>(child)) {
      if (force || child_layer->getTouchPriority() >= base) {
        child_layer->setTouchPriority(base - 1);
      }
    }
    handleTouchPriority(child, force);
  }
}

}  // namespace geode::cocos

// --- JSON serialization of colors -------------------------------------------

namespace matjson {
namespace {

template <typename Color>
geode::Result<Color> ColorFromJson(Value const& value, bool alpha) {
  if (value.isString()) {
    std::string text = value.asString().unwrap();
    if constexpr (requires { Color{}.a; }) {
      return geode::cocos::cc4bFromHexString(text, false, true);
    } else {
      return geode::cocos::cc3bFromHexString(text, true);
    }
  }
  std::uint8_t channels[4] = {0, 0, 0, 255};
  const char* names[4] = {"r", "g", "b", "a"};
  const int count = alpha ? 4 : 3;
  for (int i = 0; i < count; ++i) {
    const Value* channel = nullptr;
    if (value.isArray() && value.size() > static_cast<std::size_t>(i)) {
      channel = &value[static_cast<std::size_t>(i)];
    } else if (value.isObject() && value.contains(names[i])) {
      channel = &value[names[i]];
    }
    if (channel == nullptr) {
      if (i == 3) break;  // Alpha is optional.
      return geode::Err("expected a color");
    }
    auto number = channel->asInt();
    if (!number) return geode::Err("color channels must be numbers");
    channels[i] = static_cast<std::uint8_t>(
        std::clamp<std::int64_t>(number.unwrap(), 0, 255));
  }
  if constexpr (requires { Color{}.a; }) {
    return geode::Ok(
        cocos2d::ccc4(channels[0], channels[1], channels[2], channels[3]));
  } else {
    return geode::Ok(cocos2d::ccc3(channels[0], channels[1], channels[2]));
  }
}

}  // namespace

geode::Result<cocos2d::ccColor3B> Serialize<cocos2d::ccColor3B>::fromJson(
    Value const& value) {
  return ColorFromJson<cocos2d::ccColor3B>(value, false);
}

Value Serialize<cocos2d::ccColor3B>::toJson(cocos2d::ccColor3B const& value) {
  return Value("#" + geode::cocos::cc3bToHexString(value));
}

geode::Result<cocos2d::ccColor4B> Serialize<cocos2d::ccColor4B>::fromJson(
    Value const& value) {
  return ColorFromJson<cocos2d::ccColor4B>(value, true);
}

Value Serialize<cocos2d::ccColor4B>::toJson(cocos2d::ccColor4B const& value) {
  return Value("#" + geode::cocos::cc4bToHexString(value));
}

}  // namespace matjson
