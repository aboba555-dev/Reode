// Copyright 2026 The Reode Authors
//
// geode::AxisLayout, RowLayout, ColumnLayout and AxisLayoutOptions.
//
// Children are placed one after another along the main axis (x for rows,
// y for columns, bottom to top unless reversed), scaled down when they do
// not fit and, with growCrossAxis, wrapped into several lines.

#include <algorithm>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <vector>

#include "Geode/cocos/include/cocos2d.h"
#include "Geode/ui/Layout.hpp"
#include "Geode/utils/cocos.hpp"

namespace geode {

// --- AxisLayoutOptions ------------------------------------------------------

class AxisLayoutOptions::Impl {
 public:
  std::optional<bool> auto_scale;
  std::optional<float> min_scale;
  std::optional<float> max_scale;
  float relative_scale = 1.0f;
  std::optional<float> length;
  std::optional<float> prev_gap;
  std::optional<float> next_gap;
  bool break_line = false;
  bool same_line = false;
  int scale_priority = AXISLAYOUT_DEFAULT_PRIORITY;
  std::optional<AxisAlignment> cross_axis_alignment;
};

AxisLayoutOptions::AxisLayoutOptions() : m_impl(std::make_unique<Impl>()) {}
AxisLayoutOptions::~AxisLayoutOptions() = default;

AxisLayoutOptions* AxisLayoutOptions::create() {
  auto* options = new (std::nothrow) AxisLayoutOptions();
  if (options != nullptr) options->autorelease();
  return options;
}

std::optional<bool> AxisLayoutOptions::getAutoScale() const {
  return m_impl->auto_scale;
}
float AxisLayoutOptions::getMaxScale() const {
  return m_impl->max_scale.value_or(1.0f);
}
float AxisLayoutOptions::getMinScale() const {
  return m_impl->min_scale.value_or(AXISLAYOUT_DEFAULT_MIN_SCALE);
}
bool AxisLayoutOptions::hasExplicitMaxScale() const {
  return m_impl->max_scale.has_value();
}
bool AxisLayoutOptions::hasExplicitMinScale() const {
  return m_impl->min_scale.has_value();
}
float AxisLayoutOptions::getRelativeScale() const {
  return m_impl->relative_scale;
}
std::optional<float> AxisLayoutOptions::getLength() const {
  return m_impl->length;
}
std::optional<float> AxisLayoutOptions::getPrevGap() const {
  return m_impl->prev_gap;
}
std::optional<float> AxisLayoutOptions::getNextGap() const {
  return m_impl->next_gap;
}
bool AxisLayoutOptions::getBreakLine() const { return m_impl->break_line; }
bool AxisLayoutOptions::getSameLine() const { return m_impl->same_line; }
int AxisLayoutOptions::getScalePriority() const {
  return m_impl->scale_priority;
}
std::optional<AxisAlignment> AxisLayoutOptions::getCrossAxisAlignment() const {
  return m_impl->cross_axis_alignment;
}

AxisLayoutOptions* AxisLayoutOptions::setScaleLimits(std::optional<float> min,
                                                     std::optional<float> max) {
  m_impl->min_scale = min;
  m_impl->max_scale = max;
  return this;
}
AxisLayoutOptions* AxisLayoutOptions::setRelativeScale(float scale) {
  m_impl->relative_scale = scale;
  return this;
}
AxisLayoutOptions* AxisLayoutOptions::setAutoScale(
    std::optional<bool> enabled) {
  m_impl->auto_scale = enabled;
  return this;
}
AxisLayoutOptions* AxisLayoutOptions::setLength(std::optional<float> length) {
  m_impl->length = length;
  return this;
}
AxisLayoutOptions* AxisLayoutOptions::setPrevGap(std::optional<float> gap) {
  m_impl->prev_gap = gap;
  return this;
}
AxisLayoutOptions* AxisLayoutOptions::setNextGap(std::optional<float> gap) {
  m_impl->next_gap = gap;
  return this;
}
AxisLayoutOptions* AxisLayoutOptions::setBreakLine(bool enable) {
  m_impl->break_line = enable;
  return this;
}
AxisLayoutOptions* AxisLayoutOptions::setSameLine(bool enable) {
  m_impl->same_line = enable;
  return this;
}
AxisLayoutOptions* AxisLayoutOptions::setScalePriority(int priority) {
  m_impl->scale_priority = priority;
  return this;
}
AxisLayoutOptions* AxisLayoutOptions::setCrossAxisAlignment(
    std::optional<AxisAlignment> alignment) {
  m_impl->cross_axis_alignment = alignment;
  return this;
}

// --- AxisLayout -------------------------------------------------------------

namespace {

// One child being laid out.
struct Item {
  cocos2d::CCNode* node = nullptr;
  AxisLayoutOptions* options = nullptr;
  float main = 0;   // Unscaled length along the main axis.
  float cross = 0;  // Unscaled length along the cross axis.
  float scale = 1;
  float min_scale = 0;
  float prev_gap = 0;
  float next_gap = 0;
};

}  // namespace

class AxisLayout::Impl {
 public:
  Axis axis = Axis::Row;
  AxisAlignment axis_alignment = AxisAlignment::Center;
  AxisAlignment cross_alignment = AxisAlignment::Center;
  AxisAlignment cross_line_alignment = AxisAlignment::Center;
  float gap = 5.0f;
  bool axis_reverse = false;
  bool cross_reverse = false;
  bool auto_scale = true;
  bool grow_cross_axis = false;
  bool cross_axis_overflow = true;
  std::optional<float> auto_grow_axis;
  float default_min_scale = AXISLAYOUT_DEFAULT_MIN_SCALE;
  float default_max_scale = 1.0f;
  bool ignore_invisible = true;
  Padding padding;

  std::vector<Item> Collect(cocos2d::CCNode* on) const;
  float MainOf(cocos2d::CCSize size) const {
    return axis == Axis::Row ? size.width : size.height;
  }
  float CrossOf(cocos2d::CCSize size) const {
    return axis == Axis::Row ? size.height : size.width;
  }
};

std::vector<Item> AxisLayout::Impl::Collect(cocos2d::CCNode* on) const {
  std::vector<Item> items;
  cocos2d::CCArray* children = on->getChildren();
  if (children == nullptr) return items;
  for (unsigned int i = 0; i < children->count(); ++i) {
    auto* node = static_cast<cocos2d::CCNode*>(children->objectAtIndex(i));
    if (ignore_invisible && !node->isVisible()) continue;
    Item item;
    item.node = node;
    item.options =
        cast::typeinfo_cast<AxisLayoutOptions*>(node->getLayoutOptions());
    const cocos2d::CCSize size = node->getContentSize();
    item.main = MainOf(size);
    item.cross = CrossOf(size);
    float max_scale = default_max_scale;
    item.min_scale = default_min_scale;
    item.prev_gap = gap;
    item.next_gap = gap;
    if (item.options != nullptr) {
      if (item.options->hasExplicitMaxScale()) {
        max_scale = item.options->getMaxScale();
      }
      if (item.options->hasExplicitMinScale()) {
        item.min_scale = item.options->getMinScale();
      }
      max_scale *= item.options->getRelativeScale();
      item.min_scale *= item.options->getRelativeScale();
      if (auto length = item.options->getLength()) item.main = *length;
      if (auto value = item.options->getPrevGap()) item.prev_gap = *value;
      if (auto value = item.options->getNextGap()) item.next_gap = *value;
    }
    const bool scales =
        item.options != nullptr && item.options->getAutoScale().has_value()
            ? *item.options->getAutoScale()
            : auto_scale;
    item.scale = scales ? max_scale : node->getScale();
    if (!scales) item.min_scale = item.scale;
    items.push_back(item);
  }
  return items;
}

AxisLayout::AxisLayout(Axis axis) : m_impl(std::make_unique<Impl>()) {
  m_impl->axis = axis;
}
AxisLayout::~AxisLayout() = default;

AxisLayout* AxisLayout::create(Axis axis) {
  auto* layout = new (std::nothrow) AxisLayout(axis);
  if (layout != nullptr) layout->autorelease();
  return layout;
}

namespace {

// Length of a run of items, including the gaps between them.
float RunLength(const std::vector<Item*>& items) {
  float length = 0;
  for (std::size_t i = 0; i < items.size(); ++i) {
    length += items[i]->main * items[i]->scale;
    if (i + 1 < items.size()) {
      length += std::max(items[i]->next_gap, items[i + 1]->prev_gap);
    }
  }
  return length;
}

// Scales items of a line down (never below their minimum) until it fits.
void FitLine(std::vector<Item*>& line, float available) {
  for (int pass = 0; pass < 8; ++pass) {
    float length = RunLength(line);
    if (length <= available) return;
    float scalable = 0;
    for (Item* item : line) {
      if (item->scale > item->min_scale) scalable += item->main * item->scale;
    }
    if (scalable <= 0) return;
    const float factor = std::max(0.0f, 1.0f - (length - available) / scalable);
    for (Item* item : line) {
      if (item->scale > item->min_scale) {
        item->scale = std::max(item->min_scale, item->scale * factor);
      }
    }
  }
}

float Align(AxisAlignment alignment, float free) {
  switch (alignment) {
    case AxisAlignment::Start:
    case AxisAlignment::Between:
      return 0;
    case AxisAlignment::End:
      return free;
    case AxisAlignment::Center:
    case AxisAlignment::Even:
      return free / 2;
  }
  return 0;
}

}  // namespace

void AxisLayout::apply(cocos2d::CCNode* on) {
  Impl& impl = *m_impl;
  std::vector<Item> items = impl.Collect(on);
  const bool row = impl.axis == Axis::Row;
  const Padding& pad = impl.padding;
  const float pad_main_start = row ? pad.left : pad.bottom;
  const float pad_main_end = row ? pad.right : pad.top;
  const float pad_cross_start = row ? pad.bottom : pad.left;
  const float pad_cross_end = row ? pad.top : pad.right;

  cocos2d::CCSize size = on->getContentSize();
  float available = impl.MainOf(size) - pad_main_start - pad_main_end;
  if (impl.auto_grow_axis) available = std::numeric_limits<float>::max();

  // Split into lines.
  std::vector<std::vector<Item*>> lines(1);
  for (Item& item : items) {
    std::vector<Item*>& line = lines.back();
    const bool force_same =
        item.options != nullptr && item.options->getSameLine();
    if (!line.empty() && impl.grow_cross_axis && !force_same) {
      std::vector<Item*> candidate = line;
      candidate.push_back(&item);
      std::vector<Item> backup;
      for (Item* entry : candidate) backup.push_back(*entry);
      FitLine(candidate, available);
      const bool fits = RunLength(candidate) <= available;
      for (std::size_t i = 0; i < candidate.size(); ++i)
        *candidate[i] = backup[i];
      if (!fits) lines.emplace_back();
    }
    lines.back().push_back(&item);
    if (item.options != nullptr && item.options->getBreakLine()) {
      lines.emplace_back();
    }
  }
  if (lines.back().empty()) lines.pop_back();
  for (auto& line : lines) FitLine(line, available);

  // Grow the node along the main axis if requested.
  if (impl.auto_grow_axis) {
    float longest = 0;
    for (auto& line : lines) longest = std::max(longest, RunLength(line));
    float length =
        std::max(*impl.auto_grow_axis, longest + pad_main_start + pad_main_end);
    size = row ? cocos2d::CCSize(length, size.height)
               : cocos2d::CCSize(size.width, length);
    on->setContentSize(size);
    available = impl.MainOf(size) - pad_main_start - pad_main_end;
  }

  // Cross-axis extent of each line and of all lines together.
  std::vector<float> line_cross;
  float total_cross = 0;
  for (auto& line : lines) {
    float cross = 0;
    for (Item* item : line) cross = std::max(cross, item->cross * item->scale);
    line_cross.push_back(cross);
    total_cross += cross;
  }
  if (lines.size() > 1) total_cross += impl.gap * (lines.size() - 1);
  float cross_available = impl.CrossOf(size) - pad_cross_start - pad_cross_end;
  if (impl.grow_cross_axis && !impl.cross_axis_overflow &&
      total_cross > cross_available) {
    cross_available = total_cross;
    float cross = total_cross + pad_cross_start + pad_cross_end;
    size = row ? cocos2d::CCSize(size.width, cross)
               : cocos2d::CCSize(cross, size.height);
    on->setContentSize(size);
  }

  // Lines go from the cross start (bottom / left) unless reversed; with
  // multiple rows the first row is on top, as text would be.
  float cross_cursor = pad_cross_start + Align(impl.cross_line_alignment,
                                               cross_available - total_cross);
  std::vector<std::size_t> line_order(lines.size());
  for (std::size_t i = 0; i < lines.size(); ++i) {
    line_order[i] = row != impl.cross_reverse ? lines.size() - 1 - i : i;
  }

  for (std::size_t index : line_order) {
    std::vector<Item*>& line = lines[index];
    const float cross_size = line_cross[index];
    std::vector<Item*> ordered = line;
    // Columns run bottom to top; reversing makes them top to bottom.
    if (impl.axis_reverse) std::ranges::reverse(ordered);

    const float length = RunLength(ordered);
    const float free = std::max(0.0f, available - length);
    float extra_gap = 0;
    float cursor = pad_main_start + Align(impl.axis_alignment, free);
    if (impl.axis_alignment == AxisAlignment::Between && ordered.size() > 1) {
      extra_gap = free / (ordered.size() - 1);
    } else if (impl.axis_alignment == AxisAlignment::Even) {
      extra_gap = free / (ordered.size() + 1);
      cursor = pad_main_start + extra_gap;
    }

    for (std::size_t i = 0; i < ordered.size(); ++i) {
      Item* item = ordered[i];
      item->node->setScale(item->scale);
      const float main = item->main * item->scale;
      const float cross = item->cross * item->scale;
      AxisAlignment alignment =
          item->options != nullptr && item->options->getCrossAxisAlignment()
              ? *item->options->getCrossAxisAlignment()
              : impl.cross_alignment;
      float cross_offset = Align(alignment, cross_size - cross);
      if (alignment == AxisAlignment::Between) cross_offset = 0;

      cocos2d::CCPoint anchor = item->node->isIgnoreAnchorPointForPosition()
                                    ? cocos2d::CCPointZero
                                    : item->node->getAnchorPoint();
      const float anchor_main = row ? anchor.x : anchor.y;
      const float anchor_cross = row ? anchor.y : anchor.x;
      const float main_pos = cursor + main * anchor_main;
      const float cross_pos =
          cross_cursor + cross_offset + cross * anchor_cross;
      item->node->setPosition(row ? cocos2d::CCPoint(main_pos, cross_pos)
                                  : cocos2d::CCPoint(cross_pos, main_pos));

      cursor += main;
      if (i + 1 < ordered.size()) {
        cursor +=
            std::max(item->next_gap, ordered[i + 1]->prev_gap) + extra_gap;
      }
    }
    cross_cursor += cross_size + impl.gap;
  }
}

cocos2d::CCSize AxisLayout::getSizeHint(cocos2d::CCNode* on) const {
  std::vector<Item> items = m_impl->Collect(on);
  std::vector<Item*> line;
  float cross = 0;
  for (Item& item : items) {
    line.push_back(&item);
    cross = std::max(cross, item.cross * item.scale);
  }
  const Padding& pad = m_impl->padding;
  const bool row = m_impl->axis == Axis::Row;
  float main = RunLength(line);
  main += row ? pad.left + pad.right : pad.top + pad.bottom;
  cross += row ? pad.top + pad.bottom : pad.left + pad.right;
  return row ? cocos2d::CCSize(main, cross) : cocos2d::CCSize(cross, main);
}

Axis AxisLayout::getAxis() const { return m_impl->axis; }
AxisAlignment AxisLayout::getAxisAlignment() const {
  return m_impl->axis_alignment;
}
AxisAlignment AxisLayout::getCrossAxisAlignment() const {
  return m_impl->cross_alignment;
}
AxisAlignment AxisLayout::getCrossAxisLineAlignment() const {
  return m_impl->cross_line_alignment;
}
float AxisLayout::getGap() const { return m_impl->gap; }
bool AxisLayout::getAxisReverse() const { return m_impl->axis_reverse; }
bool AxisLayout::getCrossAxisReverse() const { return m_impl->cross_reverse; }
bool AxisLayout::getAutoScale() const { return m_impl->auto_scale; }
bool AxisLayout::getGrowCrossAxis() const { return m_impl->grow_cross_axis; }
bool AxisLayout::getCrossAxisOverflow() const {
  return m_impl->cross_axis_overflow;
}
std::optional<float> AxisLayout::getAutoGrowAxis() const {
  return m_impl->auto_grow_axis;
}
float AxisLayout::getDefaultMinScale() const {
  return m_impl->default_min_scale;
}
float AxisLayout::getDefaultMaxScale() const {
  return m_impl->default_max_scale;
}
Padding AxisLayout::getPadding() const { return m_impl->padding; }
bool AxisLayout::isIgnoreInvisibleChildren() const {
  return m_impl->ignore_invisible;
}

AxisLayout* AxisLayout::setAxis(Axis axis) {
  m_impl->axis = axis;
  return this;
}
AxisLayout* AxisLayout::setAxisAlignment(AxisAlignment align) {
  m_impl->axis_alignment = align;
  return this;
}
AxisLayout* AxisLayout::setCrossAxisAlignment(AxisAlignment align) {
  m_impl->cross_alignment = align;
  return this;
}
AxisLayout* AxisLayout::setCrossAxisLineAlignment(AxisAlignment align) {
  m_impl->cross_line_alignment = align;
  return this;
}
AxisLayout* AxisLayout::setGap(float gap) {
  m_impl->gap = gap;
  return this;
}
AxisLayout* AxisLayout::setAxisReverse(bool reverse) {
  m_impl->axis_reverse = reverse;
  return this;
}
AxisLayout* AxisLayout::setCrossAxisReverse(bool reverse) {
  m_impl->cross_reverse = reverse;
  return this;
}
AxisLayout* AxisLayout::setAutoScale(bool enable) {
  m_impl->auto_scale = enable;
  return this;
}
AxisLayout* AxisLayout::setGrowCrossAxis(bool expand) {
  m_impl->grow_cross_axis = expand;
  return this;
}
AxisLayout* AxisLayout::setCrossAxisOverflow(bool allow) {
  m_impl->cross_axis_overflow = allow;
  return this;
}
AxisLayout* AxisLayout::setAutoGrowAxis(
    std::optional<float> allowAndMinLength) {
  m_impl->auto_grow_axis = allowAndMinLength;
  return this;
}
AxisLayout* AxisLayout::setDefaultScaleLimits(float min, float max) {
  m_impl->default_min_scale = min;
  m_impl->default_max_scale = max;
  return this;
}
AxisLayout* AxisLayout::ignoreInvisibleChildren(bool ignore) {
  m_impl->ignore_invisible = ignore;
  return this;
}
AxisLayout* AxisLayout::setPadding(Padding const& padding) {
  m_impl->padding = padding;
  return this;
}

// --- RowLayout / ColumnLayout -----------------------------------------------

RowLayout::RowLayout() : AxisLayout(Axis::Row) {}

RowLayout* RowLayout::create() {
  auto* layout = new (std::nothrow) RowLayout();
  if (layout != nullptr) layout->autorelease();
  return layout;
}

ColumnLayout::ColumnLayout() : AxisLayout(Axis::Column) {}

ColumnLayout* ColumnLayout::create() {
  auto* layout = new (std::nothrow) ColumnLayout();
  if (layout != nullptr) layout->autorelease();
  return layout;
}

}  // namespace geode
