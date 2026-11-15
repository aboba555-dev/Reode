// Copyright 2026 The Reode Authors

#include "game/cocos.h"

#include <windows.h>

#include <cstdint>

#include "core/logger.h"

namespace reode::cocos {
namespace {

constexpr int kMaxVTableSlots = 256;

constexpr char kNodeVTable[] = "??_7CCNode@cocos2d@@6B@";

template <typename Fn>
bool ResolveExport(HMODULE module, const char* name, Fn* out) {
  *out = reinterpret_cast<Fn>(GetProcAddress(module, name));
  if (*out == nullptr) {
    LogError("libcocos2d.dll is missing export {}", name);
    return false;
  }
  return true;
}

// Finds the vtable slot that holds the exported implementation `method`.
bool ResolveSlot(HMODULE module, void* const* vtable, const char* method,
                 int* slot) {
  void* address = reinterpret_cast<void*>(GetProcAddress(module, method));
  if (address == nullptr) {
    LogError("libcocos2d.dll is missing export {}", method);
    return false;
  }
  for (int i = 0; i < kMaxVTableSlots; ++i) {
    if (vtable[i] == address) {
      *slot = i;
      return true;
    }
  }
  LogError("{} is not in its class vtable", method);
  return false;
}

constexpr char kFileUtilsVTable[] = "??_7CCFileUtils@cocos2d@@6B@";

}  // namespace

struct Cocos::Exports {
  void* (*shared_director)() = nullptr;
  Size* (*get_win_size)(void* director, Size* out) = nullptr;
  Node* (*sprite_with_frame_name)(const char*) = nullptr;
  Node* (*label_bmfont_create)(const char*, const char*) = nullptr;
  Node* (*layer_color_create)(const Color4B*, float, float) = nullptr;
  Node* (*menu_create)() = nullptr;
  Node* (*menu_item_sprite_create)(Node*, Node*, Object*,
                                   MenuHandler) = nullptr;
  Node* (*menu_item_create)() = nullptr;
  // CCLayer::setTouchPriority. CCMenu does not override it.
  void (*layer_set_touch_priority)(Node*, int) = nullptr;
  void* (*shared_file_utils)() = nullptr;
  void* (*shared_sprite_frame_cache)() = nullptr;
  void (*add_sprite_frames)(void* cache, const char* plist,
                            const char* texture) = nullptr;
  void* (*shared_texture_cache)() = nullptr;
  void* (*add_image)(void* cache, const char* path, bool) = nullptr;
  void* (*sprite_frame_by_name)(void* cache, const char* name) = nullptr;
};

const Cocos* Cocos::Get() {
  static const Cocos* const kCocos = [] {
    auto* cocos = new Cocos();
    if (!cocos->Resolve()) {
      delete cocos;
      return static_cast<Cocos*>(nullptr);
    }
    return cocos;
  }();
  return kCocos;
}

bool Cocos::Resolve() {
  HMODULE module = GetModuleHandleW(L"libcocos2d.dll");
  if (module == nullptr) {
    LogError("libcocos2d.dll is not loaded");
    return false;
  }
  auto* exports = new Exports();
  bool ok = true;
  ok &= ResolveExport(module, "?sharedDirector@CCDirector@cocos2d@@SAPEAV12@XZ",
                      &exports->shared_director);
  ok &= ResolveExport(module,
                      "?getWinSize@CCDirector@cocos2d@@QEAA?AVCCSize@2@XZ",
                      &exports->get_win_size);
  ok &= ResolveExport(
      module, "?createWithSpriteFrameName@CCSprite@cocos2d@@SAPEAV12@PEBD@Z",
      &exports->sprite_with_frame_name);
  ok &= ResolveExport(module, "?create@CCLabelBMFont@cocos2d@@SAPEAV12@PEBD0@Z",
                      &exports->label_bmfont_create);
  ok &= ResolveExport(
      module, "?create@CCLayerColor@cocos2d@@SAPEAV12@AEBU_ccColor4B@2@MM@Z",
      &exports->layer_color_create);
  ok &= ResolveExport(module, "?create@CCMenu@cocos2d@@SAPEAV12@XZ",
                      &exports->menu_create);
  ok &= ResolveExport(module,
                      "?create@CCMenuItemSprite@cocos2d@@SAPEAV12@PEAVCCNode@"
                      "2@0PEAVCCObject@2@P842@EAAX1@Z@Z",
                      &exports->menu_item_sprite_create);
  ok &= ResolveExport(module, "?create@CCMenuItem@cocos2d@@SAPEAV12@XZ",
                      &exports->menu_item_create);
  ok &= ResolveExport(module, "?setTouchPriority@CCLayer@cocos2d@@UEAAXH@Z",
                      &exports->layer_set_touch_priority);
  ok &=
      ResolveExport(module, "?sharedFileUtils@CCFileUtils@cocos2d@@SAPEAV12@XZ",
                    &exports->shared_file_utils);
  ok &= ResolveExport(
      module, "?sharedSpriteFrameCache@CCSpriteFrameCache@cocos2d@@SAPEAV12@XZ",
      &exports->shared_sprite_frame_cache);
  ok &= ResolveExport(
      module,
      "?addSpriteFramesWithFile@CCSpriteFrameCache@cocos2d@@QEAAXPEBD0@Z",
      &exports->add_sprite_frames);
  ok &= ResolveExport(module,
                      "?sharedTextureCache@CCTextureCache@cocos2d@@SAPEAV12@XZ",
                      &exports->shared_texture_cache);
  ok &= ResolveExport(
      module,
      "?addImage@CCTextureCache@cocos2d@@QEAAPEAVCCTexture2D@2@PEBD_N@Z",
      &exports->add_image);
  ok &= ResolveExport(module,
                      "?spriteFrameByName@CCSpriteFrameCache@cocos2d@@"
                      "QEAAPEAVCCSpriteFrame@2@PEBD@Z",
                      &exports->sprite_frame_by_name);
  // addSearchPath is virtual and overridden by CCFileUtilsWin32.
  if (auto* const* file_utils_vtable = reinterpret_cast<void* const*>(
          GetProcAddress(module, kFileUtilsVTable));
      file_utils_vtable != nullptr) {
    ok &= ResolveSlot(module, file_utils_vtable,
                      "?addSearchPath@CCFileUtils@cocos2d@@UEAAXPEBD@Z",
                      &slots_.add_search_path);
  } else {
    LogError("libcocos2d.dll is missing export {}", kFileUtilsVTable);
    ok = false;
  }

  auto* const* vtable =
      reinterpret_cast<void* const*>(GetProcAddress(module, kNodeVTable));
  if (vtable == nullptr) {
    LogError("libcocos2d.dll is missing export {}", kNodeVTable);
    ok = false;
  } else {
    ok &=
        ResolveSlot(module, vtable, "?addChild@CCNode@cocos2d@@UEAAXPEAV12@H@Z",
                    &slots_.add_child);
    ok &= ResolveSlot(module, vtable,
                      "?setPosition@CCNode@cocos2d@@UEAAXAEBVCCPoint@2@@Z",
                      &slots_.set_position);
    ok &= ResolveSlot(module, vtable,
                      "?setAnchorPoint@CCNode@cocos2d@@UEAAXAEBVCCPoint@2@@Z",
                      &slots_.set_anchor_point);
    ok &= ResolveSlot(module, vtable, "?setScale@CCNode@cocos2d@@UEAAXM@Z",
                      &slots_.set_scale);
    ok &= ResolveSlot(module, vtable, "?setVisible@CCNode@cocos2d@@UEAAX_N@Z",
                      &slots_.set_visible);
    ok &= ResolveSlot(module, vtable, "?isVisible@CCNode@cocos2d@@UEAA_NXZ",
                      &slots_.is_visible);
    ok &= ResolveSlot(module, vtable,
                      "?getContentSize@CCNode@cocos2d@@UEBAAEBVCCSize@2@XZ",
                      &slots_.get_content_size);
    ok &= ResolveSlot(module, vtable,
                      "?setContentSize@CCNode@cocos2d@@UEAAXAEBVCCSize@2@@Z",
                      &slots_.set_content_size);
  }
  exports_ = exports;
  if (ok) {
    LogDebug(
        "cocos2d bindings resolved (addChild slot {}, setPosition slot {})",
        slots_.add_child, slots_.set_position);
  }
  return ok;
}

void Cocos::AddSearchPath(const char* path) const {
  void* file_utils = exports_->shared_file_utils();
  auto** vtable = *reinterpret_cast<void***>(file_utils);
  reinterpret_cast<void (*)(void*, const char*)>(
      vtable[slots_.add_search_path])(file_utils, path);
}

void Cocos::AddSpriteFrames(const char* plist, const char* texture) const {
  exports_->add_sprite_frames(exports_->shared_sprite_frame_cache(), plist,
                              texture);
}

bool Cocos::LoadTexture(const char* path) const {
  return exports_->add_image(exports_->shared_texture_cache(), path, false) !=
         nullptr;
}

bool Cocos::HasSpriteFrame(const char* name) const {
  return exports_->sprite_frame_by_name(exports_->shared_sprite_frame_cache(),
                                        name) != nullptr;
}

Node* Cocos::CreateSpriteWithFrameName(const char* frame_name) const {
  return exports_->sprite_with_frame_name(frame_name);
}

Node* Cocos::CreateLabel(const char* text, const char* font_file) const {
  return exports_->label_bmfont_create(text, font_file);
}

Node* Cocos::CreateLayerColor(Color4B color, float width, float height) const {
  return exports_->layer_color_create(&color, width, height);
}

Node* Cocos::CreateMenu() const { return exports_->menu_create(); }

Node* Cocos::CreateMenuItem() const { return exports_->menu_item_create(); }

void Cocos::SetTouchPriority(Node* layer, int priority) const {
  exports_->layer_set_touch_priority(layer, priority);
}

void Cocos::SetContentSize(Node* node, Size size) const {
  Virtual<void (*)(Node*, const Size*)>(node, slots_.set_content_size)(node,
                                                                       &size);
}

Node* Cocos::CreateMenuItemSprite(Node* normal, Node* selected, Object* target,
                                  MenuHandler handler) const {
  return exports_->menu_item_sprite_create(normal, selected, target, handler);
}

Size Cocos::WinSize() const {
  Size size;
  exports_->get_win_size(exports_->shared_director(), &size);
  return size;
}

void Cocos::AddChild(Node* parent, Node* child, int z_order) const {
  Virtual<void (*)(Node*, Node*, int)>(parent, slots_.add_child)(parent, child,
                                                                 z_order);
}

void Cocos::SetPosition(Node* node, Point position) const {
  Virtual<void (*)(Node*, const Point*)>(node, slots_.set_position)(node,
                                                                    &position);
}

void Cocos::SetAnchorPoint(Node* node, Point anchor) const {
  Virtual<void (*)(Node*, const Point*)>(node, slots_.set_anchor_point)(
      node, &anchor);
}

void Cocos::SetScale(Node* node, float scale) const {
  Virtual<void (*)(Node*, float)>(node, slots_.set_scale)(node, scale);
}

void Cocos::SetVisible(Node* node, bool visible) const {
  Virtual<void (*)(Node*, bool)>(node, slots_.set_visible)(node, visible);
}

bool Cocos::IsVisible(Node* node) const {
  return Virtual<bool (*)(Node*)>(node, slots_.is_visible)(node);
}

Size Cocos::ContentSize(Node* node) const {
  return *Virtual<const Size* (*)(Node*)>(node, slots_.get_content_size)(node);
}

}  // namespace reode::cocos
