// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include "photo.h"

#include "base/signal.h"
#include "ui/view.h"
#include "ui/icon.h"

#include <functional>
#include <string>
#include <vector>

namespace cutefish::ui {
class Label;
}

namespace photos {

// The Photos sidebar: "Library" (All Photos, Favorites) and "Albums" (one per
// subfolder with images). Rows are accent filled while selected, like the macOS
// Photos sidebar.
class PhotosSidebar : public cutefish::ui::View {
public:
    PhotosSidebar();

    static constexpr double kWidth = 232;

    // Rebuilds the album rows from the library; keeps the selection if it
    // still exists.
    void setLibrary(const LibraryData& data);
    void setSelection(const std::string& selection);
    const std::string& selection() const { return m_selected; }

    cutefish::ui::SizeF sizeHint() const override { return {kWidth, 0}; }

    std::function<void(const std::string& selection)> onSelectionChanged;
    std::function<void(int rowIndex, const cutefish::ui::PointF& windowPos)> onContextMenu;
    std::function<void()> onAddFolder;

    struct Row {
        cutefish::ui::Icon* icon = nullptr;
        cutefish::ui::Label* label = nullptr;
        std::string id;
        std::string path;
        bool custom = false;
        cutefish::ui::RectF rect;
    };
    const Row* row(int index) const {
        return (index >= 0 && index < int(m_rows.size())) ? &m_rows[size_t(index)] : nullptr;
    }

protected:
    void layout() override;
    void paint(cutefish::ui::Painter& painter) override;
    bool acceptsPointer() const override { return true; }
    void pointerPress(const cutefish::ui::PointerEvent& event) override;
    void pointerRelease(const cutefish::ui::PointerEvent& event) override;
    void hoverChanged() override;
    void hoverMove(const cutefish::ui::PointerEvent& event) override;

private:
    void rebuild();
    void place(double width);
    int rowAt(cutefish::ui::PointF point) const;
    static std::string selectionForAlbum(const std::string& path);

    const LibraryData* m_library = nullptr;
    cutefish::ui::Label* m_libraryHeader = nullptr;
    cutefish::ui::Label* m_albumsHeader = nullptr;
    std::vector<Row> m_rows;
    std::string m_selected = "all";
    int m_hovered = -1;
    int m_pressed = -1;
};

} // namespace photos
