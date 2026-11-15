// Copyright 2026 The Reode Authors

#ifndef REODE_LOADER_SRC_GAME_COCOS_H_
#define REODE_LOADER_SRC_GAME_COCOS_H_

#include <cstdint>

namespace reode::cocos {

// Opaque cocos2d-x objects. Reode never depends on their memory layout; it
// only calls functions exported by libcocos2d.dll.
class Node;
class Object;

struct Point {
  float x = 0;
  float y = 0;
};

struct Size {
  float width = 0;
  float height = 0;
};

struct Color4B {
  std::uint8_t r = 0;
  std::uint8_t g = 0;
  std::uint8_t b = 0;
  std::uint8_t a = 0;
};

// Signature of a CCMenuItem callback. A cocos2d SEL_MenuHandler is a
// pointer-to-member of a single-inheritance class, which on x64 MSVC is a
// plain code pointer called with `this` (the item's target) in rcx.
using MenuHandler = void (*)(Object* target, Object* sender);

// Functions resolved at runtime from libcocos2d.dll by their decorated
// names. Virtual methods are called through the object's vtable, using slot
// indices discovered from the exported CCNode vtable, so overrides in
// subclasses (e.g. CCSprite::setPosition) are respected.
class Cocos {
 public:
  // Resolves everything on first use. Returns nullptr (and logs the missing
  // symbol) if the running libcocos2d.dll is not compatible.
  static const Cocos* Get();

  // Static factories.
  Node* CreateSpriteWithFrameName(const char* frame_name) const;
  Node* CreateLabel(const char* text, const char* font_file) const;
  Node* CreateLayerColor(Color4B color, float width, float height) const;
  Node* CreateMenu() const;
  Node* CreateMenuItemSprite(Node* normal, Node* selected, Object* target,
                             MenuHandler handler) const;
  // A CCMenuItem with no image and no callback.
  Node* CreateMenuItem() const;

  // CCLayer::setTouchPriority; lower values receive touches first.
  void SetTouchPriority(Node* layer, int priority) const;

  Size WinSize() const;

  // CCFileUtils::sharedFileUtils()->addSearchPath(path).
  void AddSearchPath(const char* path) const;
  // CCSpriteFrameCache::sharedSpriteFrameCache()->addSpriteFramesWithFile.
  // `texture` is resolved through the search paths, with the -hd / -uhd
  // variant picked for the current texture quality.
  void AddSpriteFrames(const char* plist, const char* texture) const;
  // CCTextureCache::addImage; false if the image cannot be found or read.
  bool LoadTexture(const char* path) const;
  bool HasSpriteFrame(const char* name) const;

  // Virtual CCNode methods.
  void AddChild(Node* parent, Node* child, int z_order) const;
  void SetPosition(Node* node, Point position) const;
  void SetAnchorPoint(Node* node, Point anchor) const;
  void SetScale(Node* node, float scale) const;
  void SetVisible(Node* node, bool visible) const;
  bool IsVisible(Node* node) const;
  Size ContentSize(Node* node) const;
  void SetContentSize(Node* node, Size size) const;

 private:
  struct Exports;
  struct Slots {
    int add_child = -1;
    int set_position = -1;
    int set_anchor_point = -1;
    int set_scale = -1;
    int set_visible = -1;
    int is_visible = -1;
    int get_content_size = -1;
    int set_content_size = -1;
    int add_search_path = -1;  // In the CCFileUtils vtable.
  };

  Cocos() = default;
  bool Resolve();

  template <typename Fn>
  Fn Virtual(Node* node, int slot) const {
    auto** vtable = *reinterpret_cast<void***>(node);
    return reinterpret_cast<Fn>(vtable[slot]);
  }

  const Exports* exports_ = nullptr;
  Slots slots_;
};

}  // namespace reode::cocos

#endif  // REODE_LOADER_SRC_GAME_COCOS_H_
