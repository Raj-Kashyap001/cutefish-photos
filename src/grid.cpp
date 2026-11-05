// SPDX-License-Identifier: BSD-3-Clause
#include "grid.h"

#include "base/strings.h"
#include "ui/event.h"
#include "ui/image_view.h"
#include "ui/painter.h"
#include "ui/theme.h"
#include "ui/color_provider.h"

#include <algorithm>
#include <cmath>

using namespace cutefish::ui;

namespace photos {

static constexpr double kCornerRadius = 7;
// The size live pinch zooms the grid between.
static constexpr double kMinimumPinchCell = 72;
static constexpr double kMaximumPinchCell = 280;

ThumbnailView::ThumbnailView(const PhotoItem& item)
    : m_item(item)
{
    setName("thumbnail");
    setCursorShape(CursorShape::Pointer);
    m_image = addChild<ImageView>(item.path);
    m_image->setName("picture");
    m_image->setFillMode(ImageView::FillMode::PreserveAspectCrop);
    m_image->setCornerRadius(kCornerRadius, Corners::Circular);
    m_image->setThumbnail(true);
    m_image->setAutoTransform(true);
    m_image->setToolTip(item.name);
    m_ring = addChild<RingView>(this);
}

void ThumbnailView::setSelected(bool selected)
{
    if (m_selected == selected)
        return;
    m_selected = selected;
    m_ring->requestPaint();
}

void ThumbnailView::layout()
{
    const RectF rect = localRect();
    m_image->setGeometry(rect);
    m_ring->setGeometry(rect);
}

void ThumbnailView::RingView::paint(Painter& painter)
{
    const Theme& theme = Theme::current();
    const RectF rect = localRect();
    if (m_owner->m_selected) {
        // An accent ring with a hairline of white inside it, so the selection
        // reads on any picture.
        painter.drawSmoothRectangle(rect.adjusted(1, 1, -1, -1), kCornerRadius + 1, Color{255, 255, 255, 34},
                                    3, theme.highlight);
        painter.drawSmoothRectangle(rect.adjusted(5.5, 5.5, -5.5, -5.5), kCornerRadius - 3.5,
                                    Color{0, 0, 0, 0}, 1, Color::rgb(0xffffff).withAlpha(0.9));
    } else if (m_owner->m_hovered) {
        painter.fillRoundedRect(rect, kCornerRadius, Color::rgb(0xffffff).withAlpha(0.18));
    }
}

void ThumbnailView::pointerEnter()
{
    m_hovered = true;
    m_ring->requestPaint();
}

void ThumbnailView::pointerLeave()
{
    m_hovered = false;
    m_ring->requestPaint();
}

void ThumbnailView::pointerPress(const PointerEvent& event)
{
    if (event.button == MouseButton::Right) {
        setSelected(true);
    }
}

void ThumbnailView::pointerRelease(const PointerEvent& event)
{
    if (!localRect().contains(event.position))
        return;
    if (event.button == MouseButton::Right) {
        if (onContextMenu)
            onContextMenu(mapToWindow(event.position));
        return;
    }
    if (event.button == MouseButton::Left && onClicked)
        onClicked(event.clickCount);
}

bool ThumbnailView::scroll(const ScrollEvent& event)
{
    if (event.modifiers & ControlModifier) {
        if (auto* g = dynamic_cast<GridContent*>(parent()))
            return g->scroll(event);
    }
    return false;
}

bool ThumbnailView::gesture(const GestureEvent& event)
{
    if (auto* g = dynamic_cast<GridContent*>(parent()))
        return g->gesture(event);
    return false;
}

GridContent::GridContent()
{
    setName("grid");
    setClipsChildren(true);
}

void GridContent::setItems(std::vector<PhotoItem> items)
{
    m_items = std::move(items);
    m_selected = -1;
    rebuild();
}

void GridContent::setCellSize(double edge)
{
    if (m_cell == edge)
        return;
    m_cell = edge;
    requestLayout();
}

void GridContent::setSelectedIndex(int index)
{
    if (m_selected == index)
        return;
    if (m_selected >= 0 && m_selected < int(m_cells.size()))
        m_cells[size_t(m_selected)]->setSelected(false);
    m_selected = index;
    if (m_selected >= 0 && m_selected < int(m_cells.size()))
        m_cells[size_t(m_selected)]->setSelected(true);
}

void GridContent::rebuild()
{
    while (!m_cells.empty()) {
        std::unique_ptr<View> child = takeChild(m_cells.back());
        m_cells.pop_back();
    }
    m_cells.reserve(m_items.size());
    for (size_t i = 0; i < m_items.size(); ++i) {
        ThumbnailView* cell = addChild<ThumbnailView>(m_items[i]);
        const int index = int(i);
        cell->onClicked = [this, index](int clickCount) {
            setSelectedIndex(index);
            if (onActivated)
                onActivated(index, clickCount);
        };
        cell->onContextMenu = [this, index](const PointF& windowPos) {
            setSelectedIndex(index);
            if (onContextMenu)
                onContextMenu(index, windowPos);
        };
        m_cells.push_back(cell);
    }
    requestLayout();
}

void GridContent::pointerRelease(const PointerEvent& event)
{
    if (event.button == MouseButton::Right) {
        if (onEmptyContextMenu)
            onEmptyContextMenu(mapToWindow(event.position));
    }
}

int GridContent::columnsForWidth(double width) const
{
    return std::max(1, int(std::floor((width + kSpacing) / (m_cell + kSpacing))));
}

double GridContent::heightForWidth(double width) const
{
    if (m_items.empty())
        return kSpacing;
    const int columns = columnsForWidth(width);
    const int rows = int((m_items.size() + size_t(columns) - 1) / size_t(columns));
    return kSpacing + double(rows) * (m_cell + kSpacing);
}

SizeF GridContent::sizeHint() const
{
    return {0, m_height};
}

bool GridContent::gesture(const GestureEvent& event)
{
    if (event.type == GestureEvent::Type::PinchBegin) {
        m_pinchCell = m_cell;
        return true;
    }
    if (event.type == GestureEvent::Type::PinchUpdate) {
        m_cell = std::clamp(m_pinchCell * std::max(0.05, event.scale), kMinimumPinchCell, kMaximumPinchCell);
        requestLayout();
        return true;
    }
    if (event.type == GestureEvent::Type::PinchEnd)
        return true;
    return false;
}

bool GridContent::scroll(const ScrollEvent& event)
{
    if (event.modifiers & ControlModifier) {
        const double delta = (event.value120Y != 0) ? -double(event.value120Y) : event.deltaY;
        if (delta < 0 && onZoomIn) {
            onZoomIn();
            return true;
        } else if (delta > 0 && onZoomOut) {
            onZoomOut();
            return true;
        }
    }
    return false;
}

void GridContent::layout()
{
    const double width = std::max(0.0, geometry().width);
    const int columns = columnsForWidth(width);
    const double height = heightForWidth(width);
    m_height = height;
    for (size_t i = 0; i < m_cells.size(); ++i) {
        const int column = int(i % size_t(columns));
        const int row = int(i / size_t(columns));
        const double x = kSpacing + double(column) * (m_cell + kSpacing);
        const double y = kSpacing + double(row) * (m_cell + kSpacing);
        m_cells[i]->setGeometry({x, y, m_cell, m_cell});
    }
}

} // namespace photos
