// SPDX-License-Identifier: BSD-3-Clause
#include "sidebar.h"

#include "base/i18n.h"
#include "ui/color_provider.h"
#include "ui/event.h"
#include "ui/label.h"
#include "ui/painter.h"
#include "ui/theme.h"

using namespace cutefish::ui;
using cutefish::i18n::tr;

namespace photos {

static constexpr double kRowHeight = 34;
static constexpr double kRowSpacing = 2;
static constexpr double kSectionHeight = 30;
static constexpr double kHeaderInset = 44;   // below the window header
static constexpr double kInset = 10;

static std::string albumSelection(const std::string& path)
{
    return "album:" + path;
}

PhotosSidebar::PhotosSidebar()
{
    setName("sidebar");
    setTracksHover(true);
    m_libraryHeader = addChild<Label>("Library");
    m_libraryHeader->setName("libraryHeader");
    m_albumsHeader = addChild<Label>("Albums");
    m_albumsHeader->setName("albumsHeader");
    rebuild();
}

void PhotosSidebar::setLibrary(const LibraryData& data)
{
    m_library = &data;
    rebuild();
}

void PhotosSidebar::rebuild()
{
    while (!m_rows.empty()) {
        Row& row = m_rows.back();
        if (row.icon)
            takeChild(row.icon);
        if (row.label)
            takeChild(row.label);
        m_rows.pop_back();
    }

    const auto addRow = [&](const std::string& id, const std::string& icon, const std::string& text,
                            const std::string& path = {}, bool custom = false) {
        Row row;
        row.icon = addChild<ThemeIcon>(icon, 18);
        row.label = addChild<Label>(text);
        row.label->setElide(Label::Elide::Right);
        row.label->setVerticallyCentered(true);
        row.id = id;
        row.path = path;
        row.custom = custom;
        m_rows.push_back(row);
    };

    addRow("all", "viewimage", tr("All Photos"));
    addRow("favorites", "heart-filled-symbolic", tr("Favorites"));

    bool albums = false;
    if (m_library) {
        for (const auto& album : m_library->albums) {
            addRow(albumSelection(album.path), "folder-pictures", album.name, album.path, album.custom);
            albums = true;
        }
    }
    m_albumsHeader->setVisible(albums);
    m_hovered = -1;
    m_pressed = -1;
    // Keep the selection if it still exists, else fall back to All Photos.
    bool exists = false;
    for (const Row& row : m_rows)
        if (row.id == m_selected)
            exists = true;
    if (!exists)
        m_selected = "all";
    requestLayout();
    requestPaint();
}

void PhotosSidebar::setSelection(const std::string& selection)
{
    if (m_selected == selection)
        return;
    m_selected = selection;
    requestLayout();
    requestPaint();
}

void PhotosSidebar::place(double width)
{
    const Theme& theme = Theme::current();
    const double sectionInset = kInset + 4;

    m_libraryHeader->setText(tr("LIBRARY"));
    m_libraryHeader->setFontSize(12);
    m_libraryHeader->setWeight(700);
    m_libraryHeader->setColor(theme.disabledText);
    m_libraryHeader->setVerticallyCentered(true);
    m_libraryHeader->setGeometry({sectionInset, kHeaderInset, std::max(0.0, width - 2 * sectionInset), kSectionHeight});

    const double albumsVisible = m_albumsHeader->isVisible();
    m_albumsHeader->setText(tr("ALBUMS"));
    m_albumsHeader->setFontSize(12);
    m_albumsHeader->setWeight(700);
    m_albumsHeader->setColor(theme.disabledText);
    m_albumsHeader->setVerticallyCentered(true);

    double y = kHeaderInset + kSectionHeight;
    const double rowInset = kInset;
    const double rowWidth = std::max(0.0, width - 2 * rowInset);
    bool placedAlbumsHeader = false;
    for (Row& row : m_rows) {
        if (!placedAlbumsHeader && row.id.starts_with("album:")) {
            if (albumsVisible) {
                m_albumsHeader->setGeometry({sectionInset, y, std::max(0.0, width - 2 * sectionInset), kSectionHeight});
                y += kSectionHeight;
            }
            placedAlbumsHeader = true;
        }
        row.rect = {rowInset, y, rowWidth, kRowHeight};
        const bool selected = row.id == m_selected;
        const Color tint = selected ? theme.highlightedText : theme.highlight;
        row.icon->setColor(tint);
        row.icon->setGeometry({row.rect.x + 10, y + (kRowHeight - 18) / 2, 18, 18});
        row.label->setColor(selected ? theme.highlightedText : theme.text);
        row.label->setWeight(selected ? 600 : 400);
        row.label->setGeometry({row.rect.x + 36, y, std::max(0.0, row.rect.width - 46), kRowHeight});
        y += kRowHeight + kRowSpacing;
    }
}

void PhotosSidebar::layout()
{
    place(geometry().width);
}

void PhotosSidebar::paint(Painter& painter)
{
    const Theme& theme = Theme::current();
    const double inset = kInset;
    for (int i = 0; i < int(m_rows.size()); ++i) {
        const Row& row = m_rows[size_t(i)];
        const bool selected = row.id == m_selected;
        Color fill {0, 0, 0, 0};
        if (selected)
            fill = theme.highlight;
        else if (i == m_pressed)
            fill = theme.text.withAlpha(0.16);
        else if (i == m_hovered)
            fill = theme.text.withAlpha(0.08);
        if (fill.a == 0)
            continue;
        painter.fillRoundedRect({inset, row.rect.y, row.rect.width, row.rect.height}, theme.mediumRadius, fill);
    }
}

int PhotosSidebar::rowAt(PointF point) const
{
    for (size_t i = 0; i < m_rows.size(); ++i) {
        const RectF& rect = m_rows[i].rect;
        if (point.x >= kInset && point.x < kInset + rect.width && point.y >= rect.y && point.y < rect.y + rect.height)
            return int(i);
    }
    return -1;
}

void PhotosSidebar::pointerPress(const PointerEvent& event)
{
    m_pressed = rowAt(event.position);
    requestPaint();
}

void PhotosSidebar::hoverMove(const PointerEvent& event)
{
    const int hovered = rowAt(event.position);
    if (hovered != m_hovered) {
        m_hovered = hovered;
        requestPaint();
    }
}

void PhotosSidebar::hoverChanged()
{
    if (!isHovered() && m_hovered >= 0) {
        m_hovered = -1;
        requestPaint();
    }
}

void PhotosSidebar::pointerRelease(const PointerEvent& event)
{
    if (event.button == MouseButton::Right) {
        const int row = rowAt(event.position);
        if (onContextMenu)
            onContextMenu(row, mapToWindow(event.position));
        return;
    }
    const int released = rowAt(event.position);
    const int pressed = m_pressed;
    m_pressed = -1;
    requestPaint();
    if (released < 0 || released != pressed)
        return;
    const std::string id = m_rows[size_t(released)].id;
    if (id == m_selected)
        return;
    m_selected = id;
    requestLayout();
    requestPaint();
    if (onSelectionChanged)
        onSelectionChanged(id);
}

} // namespace photos
