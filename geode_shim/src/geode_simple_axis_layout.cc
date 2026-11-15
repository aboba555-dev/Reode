// Copyright 2026 The Reode Authors
//
// geode::SimpleAxisLayout, SimpleRowLayout, SimpleColumnLayout,
// SimpleAxisLayoutOptions and AxisGap.
//
// A single line of children. Each axis has a scaling mode that decides
// whether children shrink, stretch, or the layout itself grows or fits.
// AxisGap children replace the default gap between their neighbours.

#include <algorithm>
#include <memory>
#include <new>
#include <optional>
#include <vector>

#include "Geode/cocos/include/cocos2d.h"
#include "Geode/ui/SimpleAxisLayout.hpp"
#include "Geode/utils/cocos.hpp"
#include "node_metadata.h"

namespace geode {

// --- SimpleAxisLayoutOptions ------------------------------------------------

class SimpleAxisLayoutOptions::Impl {
 public:
  std::optional<float> min_relative_scale;
  std::optional<float> max_relative_scale;
  ScalingPriority priority = ScalingPriority::Normal;
};

SimpleAxisLayoutOptions::SimpleAxisLayoutOptions()
    : m_impl(std::make_unique<Impl>()) {}
SimpleAxisLayoutOptions::~SimpleAxisLayoutOptions() = default;

SimpleAxisLayoutOptions* SimpleAxisLayoutOptions::create() {
  auto* options = new (std::nothrow) SimpleAxisLayoutOptions();
  if (options != nullptr) options->autorelease();
  return options;
}

SimpleAxisLayoutOptions* SimpleAxisLayoutOptions::setMinRelativeScale(
    std::optional<float> scale) {
  m_impl->min_relative_scale = scale;
  return this;
}
SimpleAxisLayoutOptions* SimpleAxisLayoutOptions::setMaxRelativeScale(
    std::optional<float> scale) {
  m_impl->max_relative_scale = scale;
  return this;
}
SimpleAxisLayoutOptions* SimpleAxisLayoutOptions::setScalingPriority(
    ScalingPriority priority) {
  m_impl->priority = priority;
  return this;
}
std::optional<float> SimpleAxisLayoutOptions::getMinRelativeScale() const {
  return m_impl->min_relative_scale;
}
std::optional<float> SimpleAxisLayoutOptions::getMaxRelativeScale() const {
  return m_impl->max_relative_scale;
}
ScalingPriority SimpleAxisLayoutOptions::getScalingPriority() const {
  return m_impl->priority;
}

// --- AxisGap ----------------------------------------------------------------

class AxisGap::Impl {
 public:
  float gap = 0;
};

AxisGap::AxisGap(float gap) : m_impl(std::make_unique<Impl>()) {
  m_impl->gap = gap;
}
AxisGap::~AxisGap() = default;

AxisGap* AxisGap::create(float gap) {
  auto* node = new (std::nothrow) AxisGap(gap);
  if (node == nullptr) return nullptr;
  if (!node->init()) {
    delete node;
    return nullptr;
  }
  node->autorelease();
  return node;
}

AxisGap* AxisGap::setGap(float gap) {
  m_impl->gap = gap;
  return this;
}
float AxisGap::getGap() const { return m_impl->gap; }

// --- SimpleAxisLayout -------------------------------------------------------

class SimpleAxisLayout::Impl {
 public:
  Axis axis = Axis::Row;
  AxisScaling main_scaling = AxisScaling::ScaleDownGaps;
  AxisScaling cross_scaling = AxisScaling::None;
  MainAxisAlignment main_alignment = MainAxisAlignment::Start;
  CrossAxisAlignment cross_alignment = CrossAxisAlignment::Center;
  AxisDirection main_direction = AxisDirection::LeftToRight;
  AxisDirection cross_direction = AxisDirection::TopToBottom;
  float gap = 0.0f;
  std::optional<float> min_relative_scale = 0.5f;
  std::optional<float> max_relative_scale = 2.0f;
  bool ignore_invisible = true;
  Padding padding;
};

SimpleAxisLayout::SimpleAxisLayout(Axis axis)
    : m_impl(std::make_unique<Impl>()) {
  m_impl->axis = axis;
  if (axis == Axis::Column) {
    m_impl->main_direction = AxisDirection::TopToBottom;
    m_impl->cross_direction = AxisDirection::LeftToRight;
  }
}
SimpleAxisLayout::~SimpleAxisLayout() = default;

SimpleAxisLayout* SimpleAxisLayout::create(Axis axis) {
  auto* layout = new (std::nothrow) SimpleAxisLayout(axis);
  if (layout != nullptr) layout->autorelease();
  return layout;
}

namespace {

struct Entry {
  cocos2d::CCNode* node = nullptr;
  float base_scale = 1;
  float main = 0;   // At base scale.
  float cross = 0;  // At base scale.
  float min_scale = 0;
  float max_scale = 0;
  bool fixed = false;  // ScalingPriority::Never
  float gap_before = 0;
};

float Clamp(float value, float low, float high) {
  return std::max(low, std::min(value, high));
}

}  // namespace

void SimpleAxisLayout::apply(cocos2d::CCNode* on) {
  const Impl& impl = *m_impl;
  const bool row = impl.axis == Axis::Row;
  auto main_of = [&](cocos2d::CCSize size) {
    return row ? size.width : size.height;
  };
  auto cross_of = [&](cocos2d::CCSize size) {
    return row ? size.height : size.width;
  };

  // Collect items; AxisGap nodes only set the gap before the next item.
  std::vector<Entry> entries;
  std::optional<float> pending_gap;
  cocos2d::CCArray* children = on->getChildren();
  for (unsigned int i = 0; children != nullptr && i < children->count(); ++i) {
    auto* node = static_cast<cocos2d::CCNode*>(children->objectAtIndex(i));
    if (auto* gap = cast::typeinfo_cast<AxisGap*>(node)) {
      pending_gap = pending_gap.value_or(0) + gap->getGap();
      continue;
    }
    if (impl.ignore_invisible && !node->isVisible()) continue;
    auto& metadata = reode::geode_shim::NodeMetadataStore::Instance().Get(node);
    if (!metadata.layout_base_scale) {
      metadata.layout_base_scale = node->getScale();
    }
    Entry entry;
    entry.node = node;
    entry.base_scale = *metadata.layout_base_scale;
    entry.main = main_of(node->getContentSize()) * entry.base_scale;
    entry.cross = cross_of(node->getContentSize()) * entry.base_scale;
    entry.min_scale = impl.min_relative_scale.value_or(0.0f);
    entry.max_scale = impl.max_relative_scale.value_or(1e9f);
    if (auto* options = cast::typeinfo_cast<SimpleAxisLayoutOptions*>(
            node->getLayoutOptions())) {
      if (auto value = options->getMinRelativeScale()) entry.min_scale = *value;
      if (auto value = options->getMaxRelativeScale()) entry.max_scale = *value;
      entry.fixed = options->getScalingPriority() == ScalingPriority::Never;
    }
    entry.gap_before = entries.empty() ? 0 : pending_gap.value_or(impl.gap);
    pending_gap.reset();
    entries.push_back(entry);
  }
  if (entries.empty()) return;

  const Padding& pad = impl.padding;
  const float pad_main = row ? pad.left + pad.right : pad.top + pad.bottom;
  const float pad_cross = row ? pad.top + pad.bottom : pad.left + pad.right;
  cocos2d::CCSize size = on->getContentSize();

  float items_main = 0;
  float gaps = 0;
  float max_cross = 0;
  for (const Entry& entry : entries) {
    items_main += entry.main;
    gaps += entry.gap_before;
    max_cross = std::max(max_cross, entry.cross);
  }

  // Main axis.
  float available = main_of(size) - pad_main;
  float gap_factor = 1.0f;
  float scale = 1.0f;
  switch (impl.main_scaling) {
    case AxisScaling::None:
      break;
    case AxisScaling::ScaleDownGaps:
      if (items_main + gaps > available) {
        const float gap_room = std::max(0.0f, available - items_main);
        gap_factor = gaps > 0 ? gap_room / gaps : 1.0f;
        if (items_main > available && items_main > 0) {
          scale = available / items_main;
        }
      }
      break;
    case AxisScaling::ScaleDown:
      if (items_main + gaps > available && items_main > 0) {
        scale = std::max(0.0f, available - gaps) / items_main;
      }
      break;
    case AxisScaling::Scale:
      if (items_main > 0) scale = std::max(0.0f, available - gaps) / items_main;
      break;
    case AxisScaling::Grow:
      if (items_main + gaps > available) available = items_main + gaps;
      break;
    case AxisScaling::Fit:
      available = items_main + gaps;
      break;
  }

  // Cross axis.
  float cross_available = cross_of(size) - pad_cross;
  if (impl.cross_scaling == AxisScaling::Grow) {
    cross_available = std::max(cross_available, max_cross);
  } else if (impl.cross_scaling == AxisScaling::Fit) {
    cross_available = max_cross;
  }

  // Apply the final scale of every item.
  float used_main = 0;
  for (Entry& entry : entries) {
    float item_scale = entry.fixed ? 1.0f : scale;
    if (!entry.fixed && entry.cross > 0) {
      if (impl.cross_scaling == AxisScaling::ScaleDown &&
          entry.cross * item_scale > cross_available) {
        item_scale = cross_available / entry.cross;
      } else if (impl.cross_scaling == AxisScaling::Scale ||
                 impl.cross_scaling == AxisScaling::ScaleDownGaps) {
        item_scale = std::min(item_scale, cross_available / entry.cross);
      }
    }
    item_scale = Clamp(item_scale, entry.min_scale, entry.max_scale);
    entry.node->setScale(entry.base_scale * item_scale);
    entry.main *= item_scale;
    entry.cross *= item_scale;
    entry.gap_before *= gap_factor;
    used_main += entry.main + entry.gap_before;
  }

  // Resize the layout node where the scaling modes ask for it.
  float new_main = main_of(size);
  float new_cross = cross_of(size);
  if (impl.main_scaling == AxisScaling::Grow ||
      impl.main_scaling == AxisScaling::Fit) {
    new_main = available + pad_main;
  }
  if (impl.cross_scaling == AxisScaling::Grow ||
      impl.cross_scaling == AxisScaling::Fit) {
    new_cross = cross_available + pad_cross;
  }
  if (new_main != main_of(size) || new_cross != cross_of(size)) {
    size = row ? cocos2d::CCSize(new_main, new_cross)
               : cocos2d::CCSize(new_cross, new_main);
    on->setContentSize(size);
  }

  // Positions. Distances are measured from the start of the main axis in
  // its direction, then mapped to node coordinates.
  const float free = std::max(0.0f, available - used_main);
  const std::size_t count = entries.size();
  float cursor = 0;
  float extra = 0;
  switch (impl.main_alignment) {
    case MainAxisAlignment::Start:
      break;
    case MainAxisAlignment::Center:
      cursor = free / 2;
      break;
    case MainAxisAlignment::End:
      cursor = free;
      break;
    case MainAxisAlignment::Even:
      extra = free / (count + 1);
      cursor = extra;
      break;
    case MainAxisAlignment::Between:
      extra = count > 1 ? free / (count - 1) : 0;
      if (count == 1) cursor = free / 2;
      break;
    case MainAxisAlignment::Around:
      extra = free / count;
      cursor = extra / 2;
      break;
  }

  // Row: LeftToRight grows x. Column: TopToBottom shrinks y.
  const bool main_reversed =
      row ? impl.main_direction == AxisDirection::RightToLeft
          : impl.main_direction == AxisDirection::TopToBottom;
  const bool cross_reversed =
      row ? impl.cross_direction == AxisDirection::TopToBottom
          : impl.cross_direction == AxisDirection::RightToLeft;
  const float main_origin = row ? pad.left : pad.bottom;
  const float cross_origin = row ? pad.bottom : pad.left;

  for (std::size_t i = 0; i < count; ++i) {
    Entry& entry = entries[i];
    if (i != 0) cursor += entry.gap_before + extra;
    float cross_offset = 0;
    switch (impl.cross_alignment) {
      case CrossAxisAlignment::Start:
        cross_offset = 0;
        break;
      case CrossAxisAlignment::Center:
        cross_offset = (cross_available - entry.cross) / 2;
        break;
      case CrossAxisAlignment::End:
        cross_offset = cross_available - entry.cross;
        break;
    }
    // Lower edge of the item on each axis, in node coordinates.
    float main_low = main_reversed ? available - cursor - entry.main : cursor;
    float cross_low = cross_reversed
                          ? cross_available - cross_offset - entry.cross
                          : cross_offset;
    cocos2d::CCPoint anchor = entry.node->isIgnoreAnchorPointForPosition()
                                  ? cocos2d::CCPointZero
                                  : entry.node->getAnchorPoint();
    float main_pos =
        main_origin + main_low + entry.main * (row ? anchor.x : anchor.y);
    float cross_pos =
        cross_origin + cross_low + entry.cross * (row ? anchor.y : anchor.x);
    entry.node->setPosition(row ? cocos2d::CCPoint(main_pos, cross_pos)
                                : cocos2d::CCPoint(cross_pos, main_pos));
    cursor += entry.main;
  }
}

cocos2d::CCSize SimpleAxisLayout::getSizeHint(cocos2d::CCNode* on) const {
  const bool row = m_impl->axis == Axis::Row;
  float main = 0;
  float cross = 0;
  bool first = true;
  cocos2d::CCArray* children = on->getChildren();
  for (unsigned int i = 0; children != nullptr && i < children->count(); ++i) {
    auto* node = static_cast<cocos2d::CCNode*>(children->objectAtIndex(i));
    if (cast::typeinfo_cast<AxisGap*>(node) != nullptr) continue;
    if (m_impl->ignore_invisible && !node->isVisible()) continue;
    cocos2d::CCSize size = node->getScaledContentSize();
    main += (row ? size.width : size.height) + (first ? 0 : m_impl->gap);
    cross = std::max(cross, row ? size.height : size.width);
    first = false;
  }
  return row ? cocos2d::CCSize(main, cross) : cocos2d::CCSize(cross, main);
}

SimpleAxisLayout* SimpleAxisLayout::setAxis(Axis axis) {
  m_impl->axis = axis;
  return this;
}
SimpleAxisLayout* SimpleAxisLayout::setMainAxisScaling(AxisScaling scaling) {
  m_impl->main_scaling = scaling;
  return this;
}
SimpleAxisLayout* SimpleAxisLayout::setCrossAxisScaling(AxisScaling scaling) {
  m_impl->cross_scaling = scaling;
  return this;
}
SimpleAxisLayout* SimpleAxisLayout::setMainAxisAlignment(
    MainAxisAlignment alignment) {
  m_impl->main_alignment = alignment;
  return this;
}
SimpleAxisLayout* SimpleAxisLayout::setCrossAxisAlignment(
    CrossAxisAlignment alignment) {
  m_impl->cross_alignment = alignment;
  return this;
}
SimpleAxisLayout* SimpleAxisLayout::setMainAxisDirection(
    AxisDirection direction) {
  m_impl->main_direction = direction;
  return this;
}
SimpleAxisLayout* SimpleAxisLayout::setCrossAxisDirection(
    AxisDirection direction) {
  m_impl->cross_direction = direction;
  return this;
}
SimpleAxisLayout* SimpleAxisLayout::setGap(float gap) {
  m_impl->gap = gap;
  return this;
}
SimpleAxisLayout* SimpleAxisLayout::setMinRelativeScale(
    std::optional<float> scale) {
  m_impl->min_relative_scale = scale;
  return this;
}
SimpleAxisLayout* SimpleAxisLayout::setMaxRelativeScale(
    std::optional<float> scale) {
  m_impl->max_relative_scale = scale;
  return this;
}
SimpleAxisLayout* SimpleAxisLayout::ignoreInvisibleChildren(bool ignore) {
  m_impl->ignore_invisible = ignore;
  return this;
}
bool SimpleAxisLayout::isIgnoreInvisibleChildren() const {
  return m_impl->ignore_invisible;
}
SimpleAxisLayout* SimpleAxisLayout::setPadding(Padding const& padding) {
  m_impl->padding = padding;
  return this;
}
Axis SimpleAxisLayout::getAxis() const { return m_impl->axis; }
AxisScaling SimpleAxisLayout::getMainAxisScaling() const {
  return m_impl->main_scaling;
}
AxisScaling SimpleAxisLayout::getCrossAxisScaling() const {
  return m_impl->cross_scaling;
}
MainAxisAlignment SimpleAxisLayout::getMainAxisAlignment() const {
  return m_impl->main_alignment;
}
CrossAxisAlignment SimpleAxisLayout::getCrossAxisAlignment() const {
  return m_impl->cross_alignment;
}
AxisDirection SimpleAxisLayout::getMainAxisDirection() const {
  return m_impl->main_direction;
}
AxisDirection SimpleAxisLayout::getCrossAxisDirection() const {
  return m_impl->cross_direction;
}
float SimpleAxisLayout::getGap() const { return m_impl->gap; }
std::optional<float> SimpleAxisLayout::getMinRelativeScale() const {
  return m_impl->min_relative_scale;
}
std::optional<float> SimpleAxisLayout::getMaxRelativeScale() const {
  return m_impl->max_relative_scale;
}
Padding SimpleAxisLayout::getPadding() const { return m_impl->padding; }

// --- SimpleRowLayout / SimpleColumnLayout -----------------------------------

SimpleRowLayout::SimpleRowLayout() : SimpleAxisLayout(Axis::Row) {}
SimpleRowLayout::~SimpleRowLayout() = default;

SimpleRowLayout* SimpleRowLayout::create() {
  auto* layout = new (std::nothrow) SimpleRowLayout();
  if (layout != nullptr) layout->autorelease();
  return layout;
}

SimpleColumnLayout::SimpleColumnLayout() : SimpleAxisLayout(Axis::Column) {}
SimpleColumnLayout::~SimpleColumnLayout() = default;

SimpleColumnLayout* SimpleColumnLayout::create() {
  auto* layout = new (std::nothrow) SimpleColumnLayout();
  if (layout != nullptr) layout->autorelease();
  return layout;
}

}  // namespace geode
