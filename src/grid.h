// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include "photo.h"

#include "ui/view.h"
#include "ui/image_view.h"

#include <functional>
#include <vector>

namespace photos {

// One photo cell: a square thumbnail with the rounded corners Apple Photos
// uses, a light fill on hover and a double ring on selection, drawn above
// the picture.
class ThumbnailView : public cutefish::ui::View {
public:
    explicit ThumbnailView(const PhotoItem& item);

    bool acceptsPointer() const override { return true; }
    const PhotoItem& item() const { return m_item; }
    void setSelected(bool selected);
    bool isSelected() const { return m_selected; }

    std::function<void(int clickCount, uint32_t modifiers)> onClicked;
    std::function<void(const cutefish::ui::PointF& windowPos)> onContextMenu;

protected:
    void layout() override;
    void pointerEnter() override;
    void pointerLeave() override;
    void pointerPress(const cutefish::ui::PointerEvent& event) override;
    void pointerRelease(const cutefish::ui::PointerEvent& event) override;
    bool scroll(const cutefish::ui::ScrollEvent& event) override;
    bool gesture(const cutefish::ui::GestureEvent& event) override;

private:
    // Paints the hover fill or the selection ring, above the picture.
    class RingView : public cutefish::ui::View {
    public:
        explicit RingView(ThumbnailView* owner)
            : m_owner(owner)
        {
            setName("ring");
        }
        bool acceptsPointer() const override { return false; }

    protected:
        void paint(cutefish::ui::Painter& painter) override;

    private:
        ThumbnailView* m_owner;
    };

    PhotoItem m_item;
    cutefish::ui::ImageView* m_image;
    RingView* m_ring;
    bool m_selected = false;
    bool m_hovered = false;
};

// The contents of the grid's scroll view: uniform square cells, left packed,
// reflowing on resize.
class GridContent : public cutefish::ui::View {
public:
    GridContent();

    void setItems(std::vector<PhotoItem> items);
    void setCellSize(double edge);
    void setSelectedIndex(int index);
    void selectAll();
    void clearSelection();
    std::vector<int> selectedIndices() const { return m_selectedIndices; }
    int selectedIndex() const { return m_selected; }
    size_t count() const { return m_items.size(); }
    const std::vector<PhotoItem>& items() const { return m_items; }

    double heightForWidth(double width) const override;
    cutefish::ui::SizeF sizeHint() const override;
    bool acceptsPointer() const override { return true; }
    bool gesture(const cutefish::ui::GestureEvent& event) override;
    bool scroll(const cutefish::ui::ScrollEvent& event) override;

    std::function<void(int index, int clickCount)> onActivated;
    std::function<void(int index, const cutefish::ui::PointF& windowPos)> onContextMenu;
    std::function<void(const cutefish::ui::PointF& windowPos)> onEmptyContextMenu;
    std::function<void()> onZoomIn;
    std::function<void()> onZoomOut;

protected:
    void layout() override;
    void pointerRelease(const cutefish::ui::PointerEvent& event) override;

private:
    static constexpr double kSpacing = 6;

    void rebuild();
    void applySelection();
    int columnsForWidth(double width) const;

    std::vector<PhotoItem> m_items;
    std::vector<ThumbnailView*> m_cells;
    double m_cell = 148;
    int m_selected = -1;
    std::vector<int> m_selectedIndices;
    int m_anchor = -1;
    double m_height = 0;
    double m_pinchCell = 148;   // the cell size the pinch began with
};

} // namespace photos
