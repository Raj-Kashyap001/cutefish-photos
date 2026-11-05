// SPDX-License-Identifier: BSD-3-Clause
#include "photoswindow.h"

#include "detail.h"
#include "glyphs.h"
#include "grid.h"
#include "infopanel.h"
#include "sidebar.h"

#include "base/i18n.h"
#include "base/paths.h"
#include "base/strings.h"
#include "base/url.h"
#include "base/process.h"
#include "base/event_loop.h"
#include "ui/application.h"
#include "ui/about_dialog.h"
#include "ui/button.h"
#include "ui/check_box.h"
#include "ui/combo_box.h"
#include "ui/dialog_window.h"
#include "ui/file_chooser.h"
#include "ui/header_button.h"
#include "ui/icon.h"
#include "ui/label.h"
#include "ui/menu.h"
#include "ui/painter.h"
#include "ui/scroll_view.h"
#include "ui/search_field.h"
#include "ui/text_field.h"
#include "ui/theme.h"
#include "ui/toast.h"
#include "ui/color_provider.h"
#include "ui/window_header.h"
#include "ui/keys.h"
#include "ui/drag.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <system_error>

using namespace cutefish::ui;
using cutefish::i18n::tr;
namespace strings = cutefish::base::strings;

namespace photos {

// The first line of magick's error output, elided, for the toast.
static std::string magickError(const std::string& text)
{
    const size_t newline = text.find('\n');
    std::string line = text.substr(0, newline == std::string::npos ? text.size() : newline);
    while (!line.empty() && std::isspace(static_cast<unsigned char>(line.back())))
        line.pop_back();
    constexpr size_t limit = 80;
    if (line.size() > limit)
        line = line.substr(0, limit - 1) + "…";
    return line;
}

static constexpr double kHeaderHeight = 40;
static constexpr double kInfoWidth = InfoPanel::kWidth;

const std::vector<double>& PhotosWindow::gridSizes()
{
    static const std::vector<double> sizes = {96, 148, 208};
    return sizes;
}

// A toolbar button with one of the application's glyphs, on a hover plate,
// as the macOS toolbar has them.
class ToolButton : public View {
public:
    ToolButton(const std::string& glyph, double size = 30)
        : m_size(size)
    {
        setName("toolButton");
        setCursorShape(CursorShape::Pointer);
        m_icon = addChild<Icon>(glyph, 16);
        m_icon->setColor(Theme::current().text);
    }

    void setGlyph(const std::string& glyph)
    {
        takeChild(m_icon);
        m_icon = addChild<Icon>(glyph, 16);
        m_icon->setColor(iconColor());
        m_glyph = glyph;
        requestLayout();
    }

    void setChecked(bool checked)
    {
        if (m_checked == checked)
            return;
        m_checked = checked;
        m_icon->setColor(iconColor());
        requestPaint();
    }

    bool isChecked() const { return m_checked; }

    void setEnabled(bool enabled)
    {
        View::setEnabled(enabled);
        m_icon->setOpacity(enabled ? 1.0 : 0.35);
        requestPaint();
    }

    SizeF sizeHint() const override { return {m_size, m_size}; }
    bool acceptsPointer() const override { return true; }

    std::function<void()> onClicked;

protected:
    void layout() override
    {
        const double size = m_size;
        m_icon->setGeometry({(size - 16) / 2, (size - 16) / 2, 16, 16});
    }

    void paint(Painter& painter) override
    {
        const Theme& theme = Theme::current();
        Color fill {0, 0, 0, 0};
        if (m_pressed)
            fill = theme.text.withAlpha(0.18);
        else if (m_checked)
            fill = theme.text.withAlpha(0.12);
        else if (m_hovered)
            fill = theme.text.withAlpha(0.08);
        if (fill.a == 0)
            return;
        painter.fillRoundedRect(localRect(), m_size / 2, fill);
    }

    void pointerEnter() override { m_hovered = true; requestPaint(); }
    void pointerLeave() override { m_hovered = false; m_pressed = false; requestPaint(); }
    void pointerPress(const PointerEvent& event) override
    {
        if (event.button != MouseButton::Left)
            return;
        m_pressed = true;
        requestPaint();
    }
    void pointerRelease(const PointerEvent& event) override
    {
        if (event.button != MouseButton::Left)
            return;
        const bool wasPressed = m_pressed;
        m_pressed = false;
        requestPaint();
        if (wasPressed && localRect().contains(event.position) && onClicked)
            onClicked();
    }

private:
    Color iconColor() const
    {
        return m_checked ? Theme::current().highlight : Theme::current().text;
    }

    Icon* m_icon;
    double m_size;
    bool m_hovered = false;
    bool m_pressed = false;
    bool m_checked = false;
    std::string m_glyph;
};

// The empty library state, centred where the grid would be.
class EmptyState : public View {
public:
    EmptyState()
    {
        setName("emptyState");
        m_icon = addChild<ThemeIcon>("viewimage", 64);
        m_icon->setColor(Theme::current().disabledText);
        m_title = addChild<Label>(tr("No Photos"));
        m_title->setFontSize(20);
        m_title->setWeight(600);
        m_title->setTextAlignment(Label::TextAlignment::Center);
        m_hint = addChild<Label>(tr("Drop a folder here to browse it,\nor press Ctrl+O to choose one"));
        m_hint->setFontSize(13);
        m_hint->setColor(Theme::current().disabledText);
        m_hint->setTextAlignment(Label::TextAlignment::Center);
        m_hint->setWrap(true);
    }

protected:
    void layout() override
    {
        const double width = geometry().width;
        const double height = geometry().height;
        const double centre = height * 0.42;
        m_icon->setGeometry({(width - 64) / 2, centre - 96, 64, 64});
        m_title->setGeometry({0, centre - 24, width, 30});
        m_hint->setGeometry({width * 0.1, centre + 12, width * 0.8, 40});
    }

private:
    ThemeIcon* m_icon;
    Label* m_title;
    Label* m_hint;
};

// The toolbar over the content area, under the window's traffic lights.
class HeaderToolbar : public View {
public:
    explicit HeaderToolbar(double sidebarWidth)
        : m_sidebarWidth(sidebarWidth)
    {
        setName("toolbar");
        back = addChild<HeaderButton>(std::string(), 30);
        back->setGlyph("chevron-left", 18);
        back->setToolTip(tr("Back"));
        title = addChild<Label>(tr("Photos"));
        title->setFontSize(15);
        title->setWeight(700);
        title->setVerticallyCentered(true);

        saveRotation = addChild<HeaderButton>(tr("Save"), 28);
        saveRotation->setToolTip(tr("Save rotation to file"));
        saveRotation->setVisible(false);

        rotateLeft = addChild<ToolButton>("photo-rotate-left");
        rotateLeft->setToolTip(tr("Rotate left (Ctrl+[)"));
        rotateLeft->setVisible(false);

        rotateRight = addChild<ToolButton>("photo-rotate-right");
        rotateRight->setToolTip(tr("Rotate right (Ctrl+])"));
        rotateRight->setVisible(false);

        zoomOut = addChild<ToolButton>("photo-zoom-out");
        zoomOut->setToolTip(tr("Zoom out (Ctrl+-)"));
        zoomIn = addChild<ToolButton>("photo-zoom-in");
        zoomIn->setToolTip(tr("Zoom in (Ctrl++)"));

        fullscreen = addChild<ToolButton>("photo-fullscreen");
        fullscreen->setToolTip(tr("Full screen (F11)"));
        fullscreen->setVisible(false);

        info = addChild<ToolButton>("info");
        info->setToolTip(tr("Info (Ctrl+I)"));
        search = addChild<SearchField>(tr("Search"));
        search->setName("search");
    }

    void setSidebarWidth(double width)
    {
        if (m_sidebarWidth == width)
            return;
        m_sidebarWidth = width;
        requestLayout();
    }

    HeaderButton* back;
    Label* title;
    HeaderButton* saveRotation;
    ToolButton* rotateLeft;
    ToolButton* rotateRight;
    ToolButton* zoomOut;
    ToolButton* zoomIn;
    ToolButton* fullscreen;
    ToolButton* info;
    SearchField* search;
    bool detailMode = false;

protected:
    void layout() override
    {
        const double height = geometry().height;
        const double width = geometry().width;
        const double y = (height - 30) / 2;

        const double leftMargin = m_sidebarWidth > 0 ? m_sidebarWidth + 10 : 80;
        back->setGeometry({leftMargin, y, 30, 30});
        const double titleX = back->isVisible() ? leftMargin + 38 : (m_sidebarWidth > 0 ? m_sidebarWidth + 12 : 80);

        double rightX = width - 12;

        rightX -= 30;
        info->setGeometry({rightX, y, 30, 30});

        if (detailMode) {
            search->setVisible(false);

            rightX -= 34;
            fullscreen->setVisible(true);
            fullscreen->setGeometry({rightX, y, 30, 30});

            rightX -= 34;
            zoomIn->setGeometry({rightX, y, 30, 30});
            rightX -= 32;
            zoomOut->setGeometry({rightX, y, 30, 30});

            rightX -= 34;
            rotateRight->setVisible(true);
            rotateRight->setGeometry({rightX, y, 30, 30});

            rightX -= 32;
            rotateLeft->setVisible(true);
            rotateLeft->setGeometry({rightX, y, 30, 30});

            if (saveRotation->isVisible()) {
                const SizeF saveSz = saveRotation->sizeHint();
                rightX -= (saveSz.width + 6);
                saveRotation->setGeometry({rightX, y, saveSz.width, 30});
            }
        } else {
            fullscreen->setVisible(false);
            rotateLeft->setVisible(false);
            rotateRight->setVisible(false);
            saveRotation->setVisible(false);

            search->setVisible(true);
            const double searchWidth = std::clamp(width * 0.22, 140.0, 216.0);
            rightX -= (searchWidth + 4);
            search->setGeometry({rightX, y, searchWidth, 30});

            rightX -= 34;
            zoomIn->setGeometry({rightX, y, 30, 30});
            rightX -= 32;
            zoomOut->setGeometry({rightX, y, 30, 30});
        }

        const double maxTitleW = std::max(0.0, rightX - titleX - 12);
        title->setGeometry({titleX, 0, maxTitleW, height});
    }

private:
    double m_sidebarWidth;
};

// The whole window's layout.
class PhotosWindow::Root : public View {
public:
    explicit Root(PhotosWindow* owner)
        : m_owner(owner)
    {
        sidebar = addChild<PhotosSidebar>();
        sidebar->setName("sidebar");
        gridScroll = addChild<ScrollView>();
        gridScroll->setName("gridScroll");
        gridScroll->setScrollDirections(false, true);
        grid = gridScroll->setContent<GridContent>();
        detail = addChild<DetailView>();
        empty = addChild<EmptyState>();
        info = addChild<InfoPanel>();
        header = addChild<WindowHeader>(kHeaderHeight);
        header->setButtonMargin(4);
        header->setButtonsCentered(true);
        header->setContentUnderButtons(true);
        toolbar = header->setContent<HeaderToolbar>(sidebar->kWidth);
        info->setVisible(false);
        detail->setVisible(false);
        empty->setVisible(false);
    }

    Root(const Root&) = delete;
    Root& operator=(const Root&) = delete;

    PhotosSidebar* sidebar;
    ScrollView* gridScroll;
    GridContent* grid;
    DetailView* detail;
    EmptyState* empty;
    InfoPanel* info;
    WindowHeader* header;
    HeaderToolbar* toolbar;

protected:
    void layout() override
    {
        const double width = geometry().width;
        const double height = geometry().height;
        const bool showSidebar = m_owner->m_sidebarVisible && !m_owner->m_detail;
        const double sidebarWidth = !showSidebar ? 0
                                     : width < 760 ? 200
                                                   : PhotosSidebar::kWidth;
        sidebar->setVisible(showSidebar);
        sidebar->setGeometry({0, 0, sidebarWidth, height});
        toolbar->detailMode = m_owner->m_detail;
        toolbar->setSidebarWidth(sidebarWidth);
        header->setGeometry({0, 0, width, kHeaderHeight});

        const double infoWidth = info->isVisible() ? kInfoWidth : 0;
        const double contentX = sidebarWidth;
        const double contentWidth = std::max(0.0, width - contentX - infoWidth);
        const double y = kHeaderHeight;
        const double contentHeight = std::max(0.0, height - y);
        gridScroll->setGeometry({contentX, y, contentWidth, contentHeight});
        detail->setGeometry({contentX, y, contentWidth, contentHeight});
        empty->setGeometry({contentX, y, contentWidth, contentHeight});
        info->setGeometry({contentX + contentWidth, y, infoWidth, contentHeight});
        m_owner->m_window.setBackgroundBlur({0, 0, width, height}, Theme::current().windowRadius);
    }

    void paint(Painter& painter) override
    {
        const Theme& theme = Theme::current();
        const double width = geometry().width;
        const double height = geometry().height;
        const bool showSidebar = m_owner->m_sidebarVisible && !m_owner->m_detail;
        const double sidebarWidth = !showSidebar ? 0
                                     : width < 760 ? 200
                                                   : PhotosSidebar::kWidth;
        const Color separator = color(ColorId::HeaderSeparator);
        if (sidebarWidth > 0)
            painter.drawVerticalLine(sidebarWidth, 0, height, separator);
        if (info->isVisible()) {
            const double infoX = width - kInfoWidth;
            painter.drawVerticalLine(infoX, kHeaderHeight, height - kHeaderHeight, separator);
        }
    }

    bool canDrop(const DropOffer& offer) const override
    {
        return offer.hasMimeType("text/uri-list");
    }

    uint32_t drop(const DragEvent& event) override
    {
        DropOffer* offer = event.offer;
        if (!offer)
            return NoAction;
        PhotosWindow* owner = m_owner;
        offer->read("text/uri-list", [owner](std::string data) {
            if (!owner)
                return;
            for (const std::string& url : cutefish::base::url::parseUriList(data)) {
                const auto path = cutefish::base::url::toLocalFile(url);
                if (!path)
                    continue;
                std::error_code error;
                const std::filesystem::path candidate = *path;
                if (std::filesystem::is_directory(candidate, error)) {
                    owner->setFolder(*path);
                    return;
                }
                if (isImageFile(candidate.filename().string())) {
                    owner->setFolder(candidate.parent_path().string());
                    owner->openPath(*path);
                    return;
                }
            }
        });
        return CopyAction;
    }

private:
    PhotosWindow* m_owner;
};

#ifndef PHOTOS_VERSION
#define PHOTOS_VERSION "1.0"
#endif

// A clickable text link that highlights and opens with xdg-open on click.
class LinkLabel : public Label {
public:
    explicit LinkLabel(std::string url)
        : Label(url), m_url(std::move(url))
    {
        setColor(Theme::current().highlight);
        setCursorShape(CursorShape::Pointer);
        setTextAlignment(TextAlignment::Center);
    }

    bool acceptsPointer() const override { return true; }

    void pointerEnter() override
    {
        m_hovered = true;
        requestPaint();
    }

    void pointerLeave() override
    {
        m_hovered = false;
        requestPaint();
    }

    void pointerRelease(const PointerEvent& event) override
    {
        if (event.button == MouseButton::Left && !m_url.empty())
            cutefish::base::Process::startDetached("xdg-open", {m_url});
    }

    void paint(Painter& painter) override
    {
        Label::paint(painter);
        if (m_hovered) {
            const RectF r = localRect();
            const double textWidth = tightBoundingRect().width;
            const double x = std::max(0.0, (r.width - textWidth) / 2);
            painter.fillRect({x, r.height - 2, textWidth, 1}, Theme::current().highlight);
        }
    }

private:
    std::string m_url;
    bool m_hovered = false;
};

// The custom About dialog content: app icon, title, version, description and link.
class AboutContent : public View {
public:
    explicit AboutContent(DialogWindow* dialog = nullptr)
        : m_dialog(dialog)
    {
        setName("aboutContent");

        m_icon = addChild<ThemeIcon>("cutefish-photos", 64);

        m_title = addChild<Label>(tr("Photos"));
        m_title->setFontSize(20);
        m_title->setWeight(700);
        m_title->setTextAlignment(Label::TextAlignment::Center);

        m_version = addChild<Label>(std::string(tr("Version ")) + PHOTOS_VERSION);
        m_version->setFontSize(12);
        m_version->setColor(Theme::current().disabledText);
        m_version->setTextAlignment(Label::TextAlignment::Center);

        m_desc = addChild<Label>(tr("Browse, rotate, resize and convert your photos.\nFast, private, and runs entirely on your computer."));
        m_desc->setFontSize(13);
        m_desc->setWrap(true);
        m_desc->setColor(Theme::current().text);
        m_desc->setTextAlignment(Label::TextAlignment::Center);

        m_link = addChild<LinkLabel>("https://cutefish.org");
        m_link->setFontSize(12);

        m_closeButton = addChild<DialogButton>(tr("OK"), true);
        m_closeButton->onClicked = [this] {
            if (m_dialog)
                m_dialog->close();
        };
    }

    void setDialog(DialogWindow* dialog) { m_dialog = dialog; }

    SizeF sizeHint() const override { return {380, 270}; }
    double heightForWidth(double) const override { return 270; }

protected:
    void layout() override
    {
        const double w = geometry().width;
        double y = 16;
        m_icon->setGeometry({(w - 64) / 2, y, 64, 64});
        y += 64 + 10;
        m_title->setGeometry({16, y, w - 32, 24});
        y += 24 + 4;
        m_version->setGeometry({16, y, w - 32, 16});
        y += 16 + 10;
        m_desc->setGeometry({20, y, w - 40, 36});
        y += 36 + 10;
        m_link->setGeometry({20, y, w - 40, 18});
        y += 18 + 16;
        const SizeF btnSize = m_closeButton->sizeHint();
        m_closeButton->setGeometry({(w - btnSize.width) / 2, y, btnSize.width, btnSize.height});
    }

private:
    DialogWindow* m_dialog = nullptr;
    ThemeIcon* m_icon = nullptr;
    Label* m_title = nullptr;
    Label* m_version = nullptr;
    Label* m_desc = nullptr;
    LinkLabel* m_link = nullptr;
    DialogButton* m_closeButton = nullptr;
};

// Keyboard shortcuts sheet dialog.
class ShortcutsContent : public View {
public:
    explicit ShortcutsContent(DialogWindow* dialog = nullptr)
        : m_dialog(dialog)
    {
        setName("shortcutsContent");

        auto addSection = [this](const std::string& title) {
            auto* lbl = addChild<Label>(title);
            lbl->setFontSize(13);
            lbl->setWeight(700);
            lbl->setColor(Theme::current().highlight);
            m_sections.push_back(lbl);
        };

        auto addRow = [this](const std::string& shortcut, const std::string& desc) {
            auto* sc = addChild<Label>(shortcut);
            sc->setFontSize(12);
            sc->setWeight(600);
            sc->setColor(Theme::current().text);

            auto* d = addChild<Label>(desc);
            d->setFontSize(12);
            d->setColor(Theme::current().disabledText);

            m_rows.push_back({sc, d});
        };

        addSection(tr("Navigation"));
        addRow("Left / Right", tr("Previous / Next photo"));
        addRow("Return / Double click", tr("Open photo"));
        addRow("Esc / Backspace", tr("Back to library"));

        addSection(tr("Viewing & Zoom"));
        addRow("Ctrl + / Ctrl -", tr("Zoom in / out"));
        addRow("Ctrl 0 / Ctrl 1", tr("Fit to window / Actual size"));
        addRow("F11", tr("Toggle full screen"));
        addRow("Ctrl \\", tr("Toggle sidebar"));
        addRow("Ctrl I", tr("Toggle info panel"));

        addSection(tr("Image Actions"));
        addRow("Ctrl [ / Ctrl ]", tr("Rotate preview left / right"));
        addRow("Ctrl S", tr("Save preview rotation to file"));
        addRow("Ctrl B", tr("Set as desktop background"));
        addRow("F2", tr("Rename photo"));
        addRow("Delete", tr("Move to trash"));

        m_closeButton = addChild<DialogButton>(tr("Close"), true);
        m_closeButton->onClicked = [this] {
            if (m_dialog)
                m_dialog->close();
        };
    }

    void setDialog(DialogWindow* dialog) { m_dialog = dialog; }

    SizeF sizeHint() const override { return {440, 480}; }
    double heightForWidth(double) const override { return 480; }

protected:
    void layout() override
    {
        const double w = geometry().width;
        double y = 14;

        size_t rowIdx = 0;
        size_t secIdx = 0;

        auto layoutSection = [&]() {
            if (secIdx < m_sections.size()) {
                m_sections[secIdx]->setGeometry({20, y, w - 40, 20});
                y += 24;
                secIdx++;
            }
        };

        auto layoutRows = [&](size_t count) {
            for (size_t i = 0; i < count && rowIdx < m_rows.size(); ++i, ++rowIdx) {
                m_rows[rowIdx].shortcut->setGeometry({28, y, 160, 18});
                m_rows[rowIdx].desc->setGeometry({190, y, w - 210, 18});
                y += 20;
            }
            y += 8;
        };

        layoutSection(); // Navigation
        layoutRows(3);

        layoutSection(); // Viewing & Zoom
        layoutRows(5);

        layoutSection(); // Image Actions
        layoutRows(5);

        y += 6;
        const SizeF btnSize = m_closeButton->sizeHint();
        m_closeButton->setGeometry({(w - btnSize.width) / 2, y, btnSize.width, btnSize.height});
    }

private:
    struct RowItem { Label* shortcut; Label* desc; };

    DialogWindow* m_dialog = nullptr;
    std::vector<Label*> m_sections;
    std::vector<RowItem> m_rows;
    DialogButton* m_closeButton = nullptr;
};

// The rename dialog's content: caption, text field and Cancel / Rename buttons.
class RenameContent : public View {
public:
    RenameContent(const std::string& oldName, std::function<bool(const std::string& newName)> apply)
        : m_apply(std::move(apply))
    {
        setName("renameContent");
        m_caption = addChild<Label>(tr("Enter a new name for this photo:"));
        m_caption->setFontSize(12);
        m_caption->setColor(Theme::current().disabledText);

        m_field = addChild<TextField>();
        m_field->setText(oldName);
        m_field->setFontSize(13);

        m_cancel = addChild<DialogButton>(tr("Cancel"));
        m_rename = addChild<DialogButton>(tr("Rename"), true);

        m_cancel->onClicked = [this] { if (m_dialog) m_dialog->close(); };
        m_rename->onClicked = [this] { accept(); };
        m_field->onAccepted = [this] { accept(); };
    }

    void setDialog(DialogWindow* dialog) { m_dialog = dialog; }

protected:
    void layout() override
    {
        const double width = std::max(0.0, geometry().width);
        double y = kPadding;
        m_caption->setGeometry({kPadding, y, width - 2 * kPadding, 18});
        y += 18 + 12;
        m_field->setGeometry({kPadding, y, width - 2 * kPadding, 30});
        y += 30 + 16;
        const SizeF cancelSize = m_cancel->sizeHint();
        const SizeF renameSize = m_rename->sizeHint();
        m_rename->setGeometry({width - kPadding - renameSize.width, y, renameSize.width, renameSize.height});
        m_cancel->setGeometry({m_rename->geometry().x - 8 - cancelSize.width, y, cancelSize.width, cancelSize.height});
    }

    SizeF sizeHint() const override { return {360, 136}; }

private:
    static constexpr double kPadding = 18;

    void accept()
    {
        const std::string text = m_field->text();
        if (m_apply(text) && m_dialog)
            m_dialog->close();
    }

    Label* m_caption;
    TextField* m_field;
    DialogButton* m_cancel;
    DialogButton* m_rename;
    DialogWindow* m_dialog = nullptr;
    std::function<bool(const std::string& newName)> m_apply;
};

// The resize dialog's content: original size, width and height fields, an
// aspect-ratio lock and the two actions.
class ResizeContent : public View {
public:
    ResizeContent(const SizeF& natural, std::function<bool(int width, int height)> apply)
        : m_ratio(natural.width > 0 && natural.height > 0 && natural.height > 0 ? natural.width / natural.height : 0)
        , m_apply(std::move(apply))
    {
        setName("resizeContent");
        char buffer[128];
        std::snprintf(buffer, sizeof buffer, tr("Original: %.0f × %.0f"), natural.width, natural.height);
        m_caption = addChild<Label>(buffer);
        m_caption->setFontSize(12);
        m_caption->setColor(Theme::current().disabledText);

        m_widthCaption = addChild<Label>(tr("Width"));
        m_widthField = addChild<TextField>();
        m_widthField->setText(std::to_string(int(std::round(natural.width))));
        m_heightCaption = addChild<Label>(tr("Height"));
        m_heightField = addChild<TextField>();
        m_heightField->setText(std::to_string(int(std::round(natural.height))));
        for (TextField* field : {m_widthField, m_heightField}) {
            field->setFontSize(13);
            // A small spinner behaviour would be nice; keyboard arrows work through the field.
        }

        m_keep = addChild<CheckBox>(tr("Keep aspect ratio"));
        m_keep->setChecked(true);

        m_cancel = addChild<DialogButton>(tr("Cancel"));
        m_resize = addChild<DialogButton>(tr("Resize"), true);

        m_widthField->onTextChanged = [this] { widthEdited(); };
        m_heightField->onTextChanged = [this] { heightEdited(); };
        m_cancel->onClicked = [this] { m_dialog->close(); };
        m_resize->onClicked = [this] { accept(); };
    }

    void setDialog(DialogWindow* dialog) { m_dialog = dialog; }

protected:
    void layout() override
    {
        const double width = std::max(0.0, geometry().width);
        const double labelWidth = 64;
        const double fieldWidth = std::max(0.0, width - 2 * kPadding - labelWidth - 40);
        double y = kPadding;
        m_caption->setGeometry({kPadding, y, width - 2 * kPadding, 18});
        y += 18 + 14;
        m_widthCaption->setGeometry({kPadding, y, labelWidth, 30});
        m_widthField->setGeometry({kPadding + labelWidth, y, fieldWidth, 30});
        m_heightCaption->setGeometry({kPadding, y + 38, labelWidth, 30});
        m_heightField->setGeometry({kPadding + labelWidth, y + 38, fieldWidth, 30});
        y += 38 + 38 + 10;
        m_keep->setGeometry({kPadding + labelWidth, y, width - kPadding - labelWidth, 26});
        y += 26 + 14;
        const SizeF cancelSize = m_cancel->sizeHint();
        const SizeF resizeSize = m_resize->sizeHint();
        m_resize->setGeometry({width - kPadding - resizeSize.width, y, resizeSize.width, resizeSize.height});
        m_cancel->setGeometry({m_resize->geometry().x - 8 - cancelSize.width, y, cancelSize.width, cancelSize.height});
    }

    SizeF sizeHint() const override { return {360, 208}; }

private:
    static constexpr double kPadding = 18;

    void widthEdited()
    {
        if (m_updating || !m_keep->isChecked() || m_ratio <= 0)
            return;
        const int width = parseInt(m_widthField->text());
        if (width <= 0)
            return;
        m_updating = true;
        m_heightField->setText(std::to_string(int(std::lround(width / m_ratio))));
        m_updating = false;
    }

    void heightEdited()
    {
        if (m_updating || !m_keep->isChecked() || m_ratio <= 0)
            return;
        const int height = parseInt(m_heightField->text());
        if (height <= 0)
            return;
        m_updating = true;
        m_widthField->setText(std::to_string(int(std::lround(height * m_ratio))));
        m_updating = false;
    }

    static int parseInt(const std::string& text)
    {
        try {
            return std::stoi(text);
        } catch (...) {
            return -1;
        }
    }

    void accept()
    {
        const int width = parseInt(m_widthField->text());
        const int height = parseInt(m_heightField->text());
        if (m_apply(width, height) && m_dialog)
            m_dialog->close();
    }

    Label* m_caption;
    Label* m_widthCaption;
    Label* m_heightCaption;
    TextField* m_widthField;
    TextField* m_heightField;
    CheckBox* m_keep;
    DialogButton* m_cancel;
    DialogButton* m_resize;
    DialogWindow* m_dialog = nullptr;
    double m_ratio;
    bool m_updating = false;
    std::function<bool(int width, int height)> m_apply;
};

// The convert dialog's content: a format picker; Convert asks where to save.
class ConvertContent : public View {
public:
    explicit ConvertContent(std::function<void(const std::string& extension)> save)
        : m_save(std::move(save))
    {
        setName("convertContent");
        m_caption = addChild<Label>(tr("Save a converted copy of this photo. The original is left unchanged."));
        m_caption->setFontSize(12);
        m_caption->setColor(Theme::current().disabledText);
        m_caption->setWrap(true);

        m_formatCaption = addChild<Label>(tr("Format"));
        m_format = addChild<ComboBox>(std::vector<std::string>{"PNG", "JPEG", "WebP", "TIFF", "BMP", "GIF", "AVIF"});
        m_format->setCurrentIndex(0);

        m_cancel = addChild<DialogButton>(tr("Cancel"));
        m_convert = addChild<DialogButton>(tr("Convert…"), true);
        m_cancel->onClicked = [this] { m_dialog->close(); };
        m_convert->onClicked = [this] {
            static const std::vector<std::string> extensions = {"png", "jpg", "webp", "tiff", "bmp", "gif", "avif"};
            const int index = std::clamp(m_format->currentIndex(), 0, int(extensions.size()) - 1);
            if (m_dialog)
                m_dialog->close();
            m_save(extensions[size_t(index)]);
        };
    }

    void setDialog(DialogWindow* dialog) { m_dialog = dialog; }

protected:
    void layout() override
    {
        const double width = std::max(0.0, geometry().width);
        double y = kPadding;
        m_caption->setGeometry({kPadding, y, width - 2 * kPadding, 34});
        y += 34 + 14;
        m_formatCaption->setGeometry({kPadding, y, 64, 30});
        m_format->setGeometry({kPadding + 64, y, width - 2 * kPadding - 64, 30});
        y += 30 + 20;
        const SizeF cancelSize = m_cancel->sizeHint();
        const SizeF convertSize = m_convert->sizeHint();
        m_convert->setGeometry({width - kPadding - convertSize.width, y, convertSize.width, convertSize.height});
        m_cancel->setGeometry({m_convert->geometry().x - 8 - cancelSize.width, y, cancelSize.width, cancelSize.height});
    }

    SizeF sizeHint() const override { return {360, 176}; }

private:
    static constexpr double kPadding = 18;

    Label* m_caption;
    Label* m_formatCaption;
    ComboBox* m_format;
    DialogButton* m_cancel;
    DialogButton* m_convert;
    DialogWindow* m_dialog = nullptr;
    std::function<void(const std::string& extension)> m_save;
};

PhotosWindow::PhotosWindow()
    : m_settings(cutefish::base::paths::cutefishConfig("photos"))
{
    m_window.setTitle(tr("Photos"));
    m_window.resize({1024, 680});
    m_window.setMinimumSize({680, 440});
    m_window.setRestoresSize("Main");
    m_window.setBackgroundColor(ColorId::TranslucentWindowBackground);
    m_window.setBackgroundBlur({0, 0, 1024, 680}, Theme::current().windowRadius);

    auto root = std::make_unique<Root>(this);
    m_root = root.get();
    m_window.setContentView(std::move(root));

    Root* rootView = m_root;
    rootView->sidebar->onSelectionChanged = [this](const std::string& selection) { setSelection(selection); };
    rootView->grid->onActivated = [this](int index, int clickCount) {
        if (clickCount >= 2)
            showDetail(index);
    };
    rootView->grid->onContextMenu = [this](int index, const PointF& pos) {
        showGridContextMenu(index, pos);
    };
    rootView->grid->onEmptyContextMenu = [this](const PointF& pos) {
        showGridEmptyContextMenu(pos);
    };
    rootView->detail->onContextMenu = [this](const PointF& pos) {
        showDetailContextMenu(pos);
    };
    rootView->sidebar->onContextMenu = [this](int rowIndex, const PointF& pos) {
        showSidebarContextMenu(rowIndex, pos);
    };
    rootView->toolbar->back->onClicked = [this] { backToGrid(); };
    rootView->toolbar->zoomOut->onClicked = [this] { zoomOut(); };
    rootView->toolbar->zoomIn->onClicked = [this] { zoomIn(); };
    rootView->toolbar->rotateLeft->onClicked = [this] { rotatePreview(-90); };
    rootView->toolbar->rotateRight->onClicked = [this] { rotatePreview(90); };
    rootView->toolbar->saveRotation->onClicked = [this] { savePreviewRotation(); };
    rootView->toolbar->fullscreen->onClicked = [this] { toggleFullScreen(); };
    rootView->toolbar->info->onClicked = [this] { toggleInfo(); };
    rootView->toolbar->search->onTextChanged = [this] { applySearch(); };
    rootView->grid->onZoomIn = [this] { zoomIn(); };
    rootView->grid->onZoomOut = [this] { zoomOut(); };
    rootView->sidebar->onAddFolder = [this] { chooseAlbumFolder(); };
    rootView->detail->onStatusChanged = [this](bool ready) {
        if (m_detailIndex < 0 || m_detailIndex >= int(m_items.size()))
            return;
        m_root->info->showPhoto(m_items[size_t(m_detailIndex)], ready, m_root->detail->naturalSize(),
                                m_favorites.contains(m_items[size_t(m_detailIndex)].path));
    };
    rootView->detail->onZoomChanged = [this] { updateHeader(); };
    rootView->detail->onRotationChanged = [this](int) { updateHeader(); };
    rootView->info->onToggleFavorite = [this] { toggleFavorite(); };

    // Shortcuts.
    m_window.addShortcut(ControlModifier, key::letter('o'), [this] { chooseFolder(); });
    m_window.addShortcut(ControlModifier, '=', [this] { zoomIn(); });
    m_window.addShortcut(ControlModifier | ShiftModifier, '=', [this] { zoomIn(); });
    m_window.addShortcut(ControlModifier, '+', [this] { zoomIn(); });
    m_window.addShortcut(ControlModifier | ShiftModifier, '+', [this] { zoomIn(); });
    m_window.addShortcut(ControlModifier, 0xffab /* KP_Add */, [this] { zoomIn(); });
    m_window.addShortcut(ControlModifier, '-', [this] { zoomOut(); });
    m_window.addShortcut(ControlModifier | ShiftModifier, '-', [this] { zoomOut(); });
    m_window.addShortcut(ControlModifier, 0xffad /* KP_Subtract */, [this] { zoomOut(); });
    m_window.addShortcut(ControlModifier, key::letter('b'), [this] {
        if (m_detail && m_detailIndex >= 0 && m_detailIndex < int(m_items.size())) {
            setAsWallpaper(m_items[size_t(m_detailIndex)].path);
        } else {
            const int sel = m_root->grid->selectedIndex();
            if (sel >= 0 && sel < int(m_items.size()))
                setAsWallpaper(m_items[size_t(sel)].path);
        }
    });
    m_window.addShortcut(ControlModifier, key::letter('f'), [this] { toggleFavorite(); });
    m_window.addShortcut(NoModifier, 0xffc8 /* F11 */, [this] {
        m_window.setFullScreen(!m_window.states().fullScreen);
    });
    m_window.addShortcut(NoModifier, 0xffbf /* F2 */, [this] {
        if (m_detail && m_detailIndex >= 0)
            showRenameDialog(m_detailIndex);
        else {
            const int sel = m_root->grid->selectedIndex();
            if (sel >= 0)
                showRenameDialog(sel);
        }
    });
    m_window.addShortcut(NoModifier, key::Delete, [this] {
        if (m_detail && m_detailIndex >= 0)
            moveToTrash(m_detailIndex);
        else {
            const int sel = m_root->grid->selectedIndex();
            if (sel >= 0)
                moveToTrash(sel);
        }
    });
    m_window.addShortcut(NoModifier, key::Escape, [this] {
        if (m_window.states().fullScreen)
            m_window.setFullScreen(false);
        else
            backToGrid();
    });
    m_window.addShortcut(NoModifier, key::Left, [this] { navigate(-1); });
    m_window.addShortcut(NoModifier, key::Right, [this] { navigate(1); });
    m_window.addShortcut(ControlModifier, key::letter('i'), [this] { toggleInfo(); });
    m_window.addShortcut(ControlModifier, '\\', [this] { toggleSidebar(); });
    m_window.addShortcut(ControlModifier, key::letter('q'), [] { Application::instance()->quit(0); });
    m_window.addShortcut(ControlModifier | ShiftModifier, key::letter('f'), [this] { showInFileManager(); });
    m_window.addShortcut(ControlModifier, '[', [this] {
        if (m_detail)
            rotatePreview(-90);
        else
            rotateImage(-90);
    });
    m_window.addShortcut(ControlModifier, ']', [this] {
        if (m_detail)
            rotatePreview(90);
        else
            rotateImage(90);
    });
    m_window.addShortcut(ControlModifier | ShiftModifier, '[', [this] {
        if (m_detail)
            rotatePreview(180);
        else
            rotateImage(180);
    });
    m_window.addShortcut(ControlModifier | ShiftModifier, key::letter('h'), [this] { flipImage(true); });
    m_window.addShortcut(ControlModifier | ShiftModifier, key::letter('v'), [this] { flipImage(false); });
    m_window.addShortcut(ControlModifier | ShiftModifier, key::letter('r'), [this] { showResizeDialog(); });
    m_window.addShortcut(ControlModifier | ShiftModifier, key::letter('e'), [this] { showConvertDialog(); });
    buildMainMenu();
    m_window.addShortcut(ControlModifier, '0', [this] {
        if (m_detail)
            m_root->detail->zoomFit();
    });
    m_window.addShortcut(ControlModifier, '1', [this] {
        if (m_detail)
            m_root->detail->zoomActualSize();
    });

    // Repaint the translucent background on resize: handled in Root::layout.

    m_customAlbums = m_settings.stringListValue("CustomAlbums");
    const std::string pictures = cutefish::base::paths::userDirectory(cutefish::base::paths::UserDirectory::Pictures);
    const std::string lastFolder = m_settings.stringValue("LibraryFolder");
    m_folder = !lastFolder.empty() ? lastFolder : pictures;
    setFolder(m_folder);
}

PhotosWindow::~PhotosWindow()
{
    // The window must not outlive the menu the status bar shows.
    m_window.setMainMenu(nullptr);
}

void PhotosWindow::openPath(const std::string& path)
{
    for (size_t i = 0; i < m_items.size(); ++i)
        if (m_items[i].path == path) {
            showDetail(int(i));
            return;
        }
}

void PhotosWindow::openArgument(const std::string& argument)
{
    if (argument.empty())
        return;
    std::error_code error;
    const std::filesystem::path path = argument;
    if (std::filesystem::is_directory(path, error)) {
        setFolder(path.string());
        return;
    }
    if (std::filesystem::is_regular_file(path, error) && isImageFile(path.filename().string())) {
        setFolder(path.parent_path().string());
        openPath(path.string());
    }
}

void PhotosWindow::setFolder(const std::string& folder)
{
    std::error_code error;
    if (!std::filesystem::is_directory(folder, error)) {
        const std::string pictures = cutefish::base::paths::userDirectory(cutefish::base::paths::UserDirectory::Pictures);
        if (std::filesystem::is_directory(pictures, error))
            m_folder = pictures;
        else
            return;
    } else {
        m_folder = folder;
    }

    m_library = scanLibrary(m_folder, m_customAlbums);
    m_root->sidebar->setLibrary(m_library);
    if (m_selection.starts_with("album:")) {
        const std::string albumPath = m_selection.substr(6);
        bool found = false;
        for (const auto& album : m_library.albums)
            if (album.path == albumPath)
                found = true;
        if (!found)
            m_selection = "all";
    }
    m_settings.setValue("LibraryFolder", m_folder);
    m_detail = false;
    m_detailIndex = -1;
    m_search.clear();
    m_root->toolbar->search->setText({});
    refreshItems();
}

void PhotosWindow::setSelection(const std::string& selection)
{
    if (m_selection == selection)
        return;
    m_selection = selection;
    m_root->sidebar->setSelection(selection);
    m_detail = false;
    m_detailIndex = -1;
    refreshItems();
}

void PhotosWindow::refreshItems()
{
    std::vector<PhotoItem> items;
    if (m_selection == "all") {
        items = m_library.photos;
    } else if (m_selection == "favorites") {
        for (const PhotoItem& item : m_library.photos)
            if (m_favorites.contains(item.path))
                items.push_back(item);
    } else if (m_selection.starts_with("album:")) {
        const std::string albumPath = m_selection.substr(6);
        for (const auto& album : m_library.albums)
            if (album.path == albumPath)
                items = album.photos;
    }

    // Search narrows to the name.
    if (!m_search.empty()) {
        std::vector<PhotoItem> filtered;
        for (const PhotoItem& item : items) {
            std::string name = item.name;
            std::transform(name.begin(), name.end(), name.begin(),
                           [](unsigned char c) { return char(std::tolower(c)); });
            if (name.find(m_search) != std::string::npos)
                filtered.push_back(item);
        }
        items = std::move(filtered);
    }

    m_items = std::move(items);
    m_detailIndex = m_detailIndex >= 0 && m_detailIndex < int(m_items.size()) ? m_detailIndex : -1;

    m_root->grid->setItems(m_items);
    m_root->grid->setCellSize(gridSizes()[size_t(m_zoomLevel)]);
    m_root->gridScroll->setVisible(!m_detail);
    m_root->detail->setVisible(m_detail);
    m_root->empty->setVisible(!m_detail && m_items.empty());
    m_root->gridScroll->requestLayout();
    m_root->grid->requestLayout();

    if (m_infoVisible) {
        if (m_detail && m_detailIndex >= 0) {
            const PhotoItem& item = m_items[size_t(m_detailIndex)];
            m_root->info->showPhoto(item, m_root->detail->hasPhoto(), m_root->detail->naturalSize(),
                                    m_favorites.contains(item.path));
        } else {
            m_root->info->showSummary(int(m_items.size()), m_library.totalSize, m_folder);
        }
    }

    updateHeader();
}

void PhotosWindow::showDetail(int index)
{
    if (index < 0 || index >= int(m_items.size()))
        return;
    m_detail = true;
    m_detailIndex = index;
    m_root->grid->setSelectedIndex(-1);
    m_root->detail->setPhoto(m_items[size_t(index)]);
    m_root->gridScroll->setVisible(false);
    m_root->detail->setVisible(true);
    m_root->empty->setVisible(false);
    if (m_infoVisible)
        m_root->info->showPhoto(m_items[size_t(index)], false, {}, m_favorites.contains(m_items[size_t(index)].path));
    updateHeader();
    m_window.requestLayout();
}

void PhotosWindow::backToGrid()
{
    if (!m_detail)
        return;
    m_detail = false;
    m_detailIndex = -1;
    m_root->detail->clearPhoto();
    m_root->gridScroll->setVisible(true);
    m_root->detail->setVisible(false);
    m_root->empty->setVisible(m_items.empty());
    if (m_infoVisible)
        m_root->info->showSummary(int(m_items.size()), m_library.totalSize, m_folder);
    updateHeader();
    m_window.requestLayout();
}

void PhotosWindow::navigate(int step)
{
    if (!m_detail)
        return;
    const int count = int(m_items.size());
    if (count == 0)
        return;
    int index = m_detailIndex + step;
    index = ((index % count) + count) % count;
    m_detailIndex = index;
    const PhotoItem& item = m_items[size_t(index)];
    m_root->detail->setPhoto(item);
    if (m_infoVisible)
        m_root->info->showPhoto(item, false, {}, m_favorites.contains(item.path));
    updateHeader();
}

void PhotosWindow::toggleInfo()
{
    m_infoVisible = !m_infoVisible;
    m_root->info->setVisible(m_infoVisible);
    m_root->toolbar->info->setChecked(m_infoVisible);
    if (m_infoVisible) {
        if (m_detail && m_detailIndex >= 0) {
            const PhotoItem& item = m_items[size_t(m_detailIndex)];
            m_root->info->showPhoto(item, m_root->detail->hasPhoto(), m_root->detail->naturalSize(),
                                    m_favorites.contains(item.path));
        } else {
            m_root->info->showSummary(int(m_items.size()), m_library.totalSize, m_folder);
        }
    }
    m_window.requestLayout();
}

void PhotosWindow::toggleFavorite()
{
    if (m_detail && m_detailIndex >= 0) {
        const PhotoItem& item = m_items[size_t(m_detailIndex)];
        const bool added = m_favorites.toggle(item.path);
        m_root->info->setFavorite(added);
        if (m_selection == "favorites" && !added)
            refreshItems();   // the item leaves the grid
        updateHeader();
        return;
    }
    const int selected = m_root->grid->selectedIndex();
    if (selected >= 0 && selected < int(m_items.size())) {
        const bool added = m_favorites.toggle(m_items[size_t(selected)].path);
        if (m_selection == "favorites" && !added)
            refreshItems();
    }
}

void PhotosWindow::zoomIn()
{
    if (m_detail) {
        m_root->detail->zoomIn();
        return;
    }
    if (m_zoomLevel < int(gridSizes().size()) - 1) {
        ++m_zoomLevel;
        m_root->grid->setCellSize(gridSizes()[size_t(m_zoomLevel)]);
    }
}

void PhotosWindow::zoomOut()
{
    if (m_detail) {
        m_root->detail->zoomOut();
        return;
    }
    if (m_zoomLevel > 0) {
        --m_zoomLevel;
        m_root->grid->setCellSize(gridSizes()[size_t(m_zoomLevel)]);
    }
}

void PhotosWindow::applySearch()
{
    m_search = m_root->toolbar->search->text();
    std::transform(m_search.begin(), m_search.end(), m_search.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    if (m_detail) {
        // Searching takes you back to the grid, like Photos.
        m_detail = false;
        m_detailIndex = -1;
        m_root->detail->clearPhoto();
    }
    refreshItems();
}

void PhotosWindow::updateHeader()
{
    HeaderToolbar* toolbar = m_root->toolbar;
    toolbar->detailMode = m_detail;
    toolbar->back->setVisible(m_detail);
    m_window.setTitle(m_detail && m_detailIndex >= 0 ? m_items[size_t(m_detailIndex)].name : tr("Photos"));
    if (m_detail && m_detailIndex >= 0) {
        toolbar->title->setText(m_items[size_t(m_detailIndex)].name);
        const int rot = m_root->detail->previewRotation();
        toolbar->saveRotation->setVisible(rot != 0);
    } else if (m_selection == "favorites") {
        toolbar->title->setText(tr("Favorites"));
        toolbar->saveRotation->setVisible(false);
    } else if (m_selection.starts_with("album:")) {
        const std::string albumPath = m_selection.substr(6);
        std::string name = albumPath;
        const size_t slash = albumPath.find_last_of('/');
        if (slash != std::string::npos)
            name = albumPath.substr(slash + 1);
        toolbar->title->setText(name);
        toolbar->saveRotation->setVisible(false);
    } else {
        toolbar->title->setText(tr("All Photos"));
        toolbar->saveRotation->setVisible(false);
    }
    toolbar->requestLayout();
    m_window.requestLayout();
}

void PhotosWindow::chooseFolder()
{
    auto* chooser = new FileChooser(&m_window);
    chooser->setMode(FileChooser::Mode::OpenFolder);
    chooser->setTitle(tr("Open Folder"));
    chooser->setCurrentFolder(m_folder);
    chooser->onAccepted = [this, chooser](const std::vector<std::string>& paths, const std::string&) {
        if (!paths.empty())
            setFolder(paths.front());
        delete chooser;
    };
    chooser->onRejected = [chooser] { delete chooser; };
    chooser->open();
}

void PhotosWindow::showNotification(const std::string& text, int milliseconds)
{
    if (!m_toast)
        m_toast = std::make_unique<Toast>(&m_window);
    m_toast->show(text, std::chrono::milliseconds(milliseconds));
}

void PhotosWindow::runMagick(const std::vector<std::string>& arguments,
                             std::function<void(int exitCode, const std::string& errorOutput)> done)
{
    if (m_process && m_process->isRunning()) {
        showNotification(tr("Please wait for the current edit to finish"), 2000);
        return;
    }
    m_magickError.clear();
    auto process = std::make_unique<cutefish::base::Process>(cutefish::base::EventLoop::current());
    cutefish::base::Process* raw = process.get();
    raw->onStandardError = [this](std::string_view chunk) { m_magickError.append(chunk); };
    m_process = std::move(process);
    raw->onFinished = [this, callback = std::move(done)](int exitCode, bool crashed) {
        const std::string error = std::move(m_magickError);
        m_magickError.clear();
        if (callback)
            callback(crashed ? -1 : exitCode, error);
    };
    if (!raw->start("magick", arguments)) {
        m_process.reset();
        showNotification(tr("Could not run ImageMagick"), 3000);
    }
}

void PhotosWindow::rotateImage(double degrees)
{
    if (!m_detail || m_detailIndex < 0 || m_detailIndex >= int(m_items.size()))
        return;
    const std::string path = m_items[size_t(m_detailIndex)].path;
    const char* rotation = degrees < 0 ? "-90" : (degrees >= 180 ? "180" : "90");
    runMagick({path, "-auto-orient", "-rotate", rotation, path},
              [this](int code, const std::string& errorOutput) {
                  if (code != 0) {
                      const std::string reason = magickError(errorOutput);
                      showNotification(reason.empty() ? tr("Could not rotate the photo")
                                                      : tr("Could not rotate the photo: ") + reason,
                                       5000);
                      return;
                  }
                  showNotification(tr("Rotated"), 1500);
                  reloadAfterEdit();
              });
}

void PhotosWindow::flipImage(bool horizontal)
{
    if (!m_detail || m_detailIndex < 0 || m_detailIndex >= int(m_items.size()))
        return;
    const std::string path = m_items[size_t(m_detailIndex)].path;
    runMagick({path, horizontal ? "-flop" : "-flip", path},
              [this](int code, const std::string& errorOutput) {
                  if (code != 0) {
                      const std::string reason = magickError(errorOutput);
                      showNotification(reason.empty() ? tr("Could not flip the photo")
                                                      : tr("Could not flip the photo: ") + reason,
                                       5000);
                      return;
                  }
                  showNotification(tr("Flipped"), 1500);
                  reloadAfterEdit();
              });
}

int PhotosWindow::indexOfPath(const std::string& path) const
{
    for (size_t i = 0; i < m_items.size(); ++i)
        if (m_items[i].path == path)
            return int(i);
    return -1;
}

void PhotosWindow::reloadAfterEdit()
{
    const std::string current = (m_detail && m_detailIndex >= 0 && m_detailIndex < int(m_items.size()))
            ? m_items[size_t(m_detailIndex)].path
            : std::string();
    const int gridSelection = m_root->grid->selectedIndex();
    const std::string gridPath = (gridSelection >= 0 && gridSelection < int(m_items.size()))
            ? m_items[size_t(gridSelection)].path
            : std::string();

    m_library = scanLibrary(m_folder, m_customAlbums);
    m_root->sidebar->setLibrary(m_library);
    refreshItems();

    if (!current.empty()) {
        if (const int index = indexOfPath(current); index >= 0) {
            m_detailIndex = index;
            const PhotoItem& item = m_items[size_t(index)];
            // Reload the picture from disk; the zoom resets to fit.
            m_root->detail->setPhoto(item);
            if (m_infoVisible)
                m_root->info->showPhoto(item, false, {}, m_favorites.contains(current));
        }
    } else if (!gridPath.empty()) {
        if (const int index = indexOfPath(gridPath); index >= 0)
            m_root->grid->setSelectedIndex(index);
    }
    updateHeader();
}

void PhotosWindow::toggleSidebar()
{
    m_sidebarVisible = !m_sidebarVisible;
    m_window.requestLayout();
}

void PhotosWindow::showAbout()
{
    if (m_aboutDialog && m_aboutDialog->window()) {
        m_aboutDialog->window()->activate();
        return;
    }
    m_aboutDialog = std::make_unique<DialogWindow>(&m_window, tr("About Photos"), 380);
    auto* content = m_aboutDialog->setContent<AboutContent>(m_aboutDialog.get());
    m_aboutDialog->onClosed = [this] {
        m_aboutDialog.reset();
    };
    m_aboutDialog->show();
}

void PhotosWindow::showShortcutsDialog()
{
    if (m_shortcutsDialog && m_shortcutsDialog->window()) {
        m_shortcutsDialog->window()->activate();
        return;
    }
    m_shortcutsDialog = std::make_unique<DialogWindow>(&m_window, tr("Keyboard Shortcuts"), 440);
    auto* content = m_shortcutsDialog->setContent<ShortcutsContent>(m_shortcutsDialog.get());
    m_shortcutsDialog->onClosed = [this] {
        m_shortcutsDialog.reset();
    };
    m_shortcutsDialog->show();
}

int PhotosWindow::currentPhotoIndex() const
{
    if (m_detail)
        return m_detailIndex;
    if (m_root && m_root->grid)
        return m_root->grid->selectedIndex();
    return -1;
}

std::string PhotosWindow::currentPhotoPath() const
{
    const int idx = currentPhotoIndex();
    if (idx >= 0 && idx < int(m_items.size()))
        return m_items[size_t(idx)].path;
    return {};
}

void PhotosWindow::resetPreviewRotation()
{
    if (m_detail && m_root && m_root->detail) {
        m_root->detail->resetPreviewRotation();
        m_root->toolbar->saveRotation->setVisible(false);
        m_root->toolbar->requestLayout();
        showNotification(tr("Rotation reset"), 1500);
    }
}

void PhotosWindow::showInFileManager()
{
    cutefish::base::Process::startDetached("cutefish-filemanager", {m_folder});
}

void PhotosWindow::setAsWallpaper(const std::string& path)
{
    if (path.empty())
        return;
    m_appearance.setWallpaper(path);
    showNotification(tr("Desktop background set"), 2000);
}

void PhotosWindow::showItemInFileManager(const std::string& path)
{
    if (path.empty()) {
        showInFileManager();
        return;
    }
    cutefish::base::Process::startDetached("cutefish-filemanager", {path});
}

void PhotosWindow::moveToTrash(int index)
{
    if (index < 0 || index >= int(m_items.size()))
        return;
    const std::string path = m_items[size_t(index)].path;
    const std::string name = m_items[size_t(index)].name;
    cutefish::base::Process::startDetached("gio", {"trash", path});
    showNotification(tr("Moved to trash: ") + name, 2500);

    cutefish::base::EventLoop::current()->post([this] {
        reloadAfterEdit();
    });
}

void PhotosWindow::showRenameDialog(int index)
{
    if (index < 0 || index >= int(m_items.size()))
        return;
    const PhotoItem& item = m_items[size_t(index)];
    const std::string oldPath = item.path;
    const std::string oldName = item.name;

    auto* dialog = new DialogWindow(&m_window, tr("Rename Photo"), 380);
    auto* content = dialog->setContent<RenameContent>(oldName, [this, oldPath, oldName](const std::string& newName) -> bool {
        if (newName.empty() || newName == oldName)
            return true;
        std::filesystem::path p(oldPath);
        std::filesystem::path newP = p.parent_path() / newName;
        if (!newP.has_extension() && p.has_extension())
            newP.replace_extension(p.extension());

        std::error_code ec;
        if (std::filesystem::exists(newP, ec)) {
            showNotification(tr("A file with that name already exists"), 3000);
            return false;
        }
        std::filesystem::rename(oldPath, newP, ec);
        if (ec) {
            showNotification(tr("Could not rename photo: ") + ec.message(), 4000);
            return false;
        }
        showNotification(tr("Renamed to ") + newP.filename().string(), 2500);
        reloadAfterEdit();
        return true;
    });
    content->setDialog(dialog);
    dialog->onClosed = [dialog] { delete dialog; };
    dialog->show();
}

void PhotosWindow::showResizeDialog(int index)
{
    if (index < 0)
        index = m_detail ? m_detailIndex : m_root->grid->selectedIndex();
    if (index < 0 || index >= int(m_items.size()))
        return;
    const PhotoItem& item = m_items[size_t(index)];
    SizeF natural = (m_detail && index == m_detailIndex) ? m_root->detail->naturalSize() : SizeF{};
    const std::string path = item.path;

    if (natural.width <= 0 || natural.height <= 0) {
        FILE* fp = popen(("identify -format \"%w %h\" \"" + path + "\" 2>/dev/null").c_str(), "r");
        if (fp) {
            int w = 0, h = 0;
            if (fscanf(fp, "%d %d", &w, &h) == 2 && w > 0 && h > 0)
                natural = {double(w), double(h)};
            pclose(fp);
        }
    }
    if (natural.width <= 0 || natural.height <= 0)
        natural = {800, 600};

    auto* dialog = new DialogWindow(&m_window, tr("Resize"), 396);
    auto* content = dialog->setContent<ResizeContent>(natural, [this, path](int width, int height) -> bool {
        if (width < 1 || height < 1 || width > 100000 || height > 100000) {
            showNotification(tr("Enter a width and height between 1 and 100000"), 3000);
            return false;
        }
        runMagick({path, "-auto-orient", "-resize", std::to_string(width) + "x" + std::to_string(height) + "!", path},
                  [this, width, height](int code, const std::string& errorOutput) {
                      if (code != 0) {
                          const std::string reason = magickError(errorOutput);
                          showNotification(reason.empty() ? tr("Could not resize the photo")
                                                          : tr("Could not resize the photo: ") + reason,
                                           5000);
                          return;
                      }
                      char buffer[64];
                      std::snprintf(buffer, sizeof buffer, tr("Resized to %d × %d"), width, height);
                      showNotification(buffer, 3000);
                      reloadAfterEdit();
                  });
        return true;
    });
    content->setDialog(dialog);
    dialog->onClosed = [dialog] { delete dialog; };
    dialog->show();
}

void PhotosWindow::showConvertDialog(int index)
{
    if (index < 0)
        index = m_detail ? m_detailIndex : m_root->grid->selectedIndex();
    if (index < 0 || index >= int(m_items.size()))
        return;
    const std::string path = m_items[size_t(index)].path;
    auto* dialog = new DialogWindow(&m_window, tr("Convert to Format"), 396);
    auto* content = dialog->setContent<ConvertContent>([this, path](const std::string& extension) {
        std::filesystem::path stem = std::filesystem::path(path).replace_extension("");
        auto* chooser = new FileChooser(&m_window);
        chooser->setMode(FileChooser::Mode::SaveFile);
        chooser->setTitle(tr("Save Converted Photo"));
        chooser->setCurrentFile(stem.replace_extension("." + extension).string());
        chooser->onAccepted = [this, path, chooser](const std::vector<std::string>& paths, const std::string&) {
            if (!paths.empty()) {
                const std::string destination = paths.front();
                const std::string lower = destination.substr(destination.size() > 4 ? destination.size() - 4 : 0);
                const bool lossy = lower == ".jpg" || lower == ".jpeg" || lower == ".webp" || lower == ".avif";
                runMagick({path, "-auto-orient", "-quality", lossy ? "92" : "95", destination},
                          [this, destination](int code, const std::string&) {
                              if (code == 0)
                                  showNotification(std::string(tr("Saved ")) + std::filesystem::path(destination).filename().string(),
                                                   3000);
                              else
                                  showNotification(tr("Could not convert the photo"), 4000);
                          });
            }
            delete chooser;
        };
        chooser->onRejected = [chooser] { delete chooser; };
        chooser->open();
    });
    content->setDialog(dialog);
    dialog->onClosed = [dialog] { delete dialog; };
    dialog->show();
}

void PhotosWindow::showGridContextMenu(int index, const PointF& windowPos)
{
    if (index < 0 || index >= int(m_items.size()))
        return;
    m_root->grid->setSelectedIndex(index);
    const PhotoItem& item = m_items[size_t(index)];
    const std::string path = item.path;
    const bool isFav = m_favorites.contains(path);

    m_contextMenu = std::make_unique<Menu>();
    m_contextMenu->addItem(tr("Open"), [this, index] {
        showDetail(index);
    });
    m_contextMenu->addSeparator();

    const size_t bg = m_contextMenu->addItem(tr("Set as Desktop Background"), [this, path] {
        setAsWallpaper(path);
    });
    m_contextMenu->setItemKeyEquivalent(bg, "b", ControlModifier);

    const size_t fav = m_contextMenu->addItem(isFav ? tr("Remove from Favorites") : tr("Favorite"), [this, path] {
        m_favorites.toggle(path);
        if (m_selection == "favorites")
            refreshItems();
    });
    m_contextMenu->setItemKeyEquivalent(fav, "f", ControlModifier);

    m_contextMenu->addSeparator();
    const size_t ren = m_contextMenu->addItem(tr("Rename…"), [this, index] {
        showRenameDialog(index);
    });
    m_contextMenu->setItemKeyEquivalent(ren, "F2", 0);

    const size_t res = m_contextMenu->addItem(tr("Resize…"), [this, index] {
        showResizeDialog(index);
    });
    m_contextMenu->setItemKeyEquivalent(res, "r", ControlModifier | ShiftModifier);

    const size_t conv = m_contextMenu->addItem(tr("Convert To…"), [this, index] {
        showConvertDialog(index);
    });
    m_contextMenu->setItemKeyEquivalent(conv, "e", ControlModifier | ShiftModifier);

    m_contextMenu->addSeparator();
    const size_t reveal = m_contextMenu->addItem(tr("Show in File Manager"), [this, path] {
        showItemInFileManager(path);
    });
    m_contextMenu->setItemKeyEquivalent(reveal, "f", ControlModifier | ShiftModifier);

    const size_t del = m_contextMenu->addItem(tr("Move to Trash"), [this, index] {
        moveToTrash(index);
    });
    m_contextMenu->setItemKeyEquivalent(del, "Delete", 0);

    m_contextMenu->popup(&m_window, windowPos);
}

void PhotosWindow::rotatePreview(int deltaDegrees)
{
    if (!m_detail)
        return;
    m_root->detail->rotatePreview(deltaDegrees);
    updateHeader();
}

void PhotosWindow::savePreviewRotation()
{
    if (!m_detail || m_detailIndex < 0 || m_detailIndex >= int(m_items.size()))
        return;
    const int rot = m_root->detail->previewRotation();
    if (rot == 0)
        return;
    const std::string path = m_items[size_t(m_detailIndex)].path;
    const std::string rotStr = std::to_string(rot);
    runMagick({path, "-auto-orient", "-rotate", rotStr, path},
              [this](int code, const std::string& errorOutput) {
                  if (code != 0) {
                      const std::string reason = magickError(errorOutput);
                      showNotification(reason.empty() ? tr("Could not save rotated photo")
                                                      : tr("Could not save rotated photo: ") + reason,
                                       5000);
                      return;
                  }
                  showNotification(tr("Rotation saved to file"), 2000);
                  m_root->detail->resetPreviewRotation();
                  reloadAfterEdit();
              });
}

void PhotosWindow::toggleFullScreen()
{
    m_window.setFullScreen(!m_window.states().fullScreen);
}

void PhotosWindow::chooseAlbumFolder()
{
    auto* chooser = new FileChooser(&m_window);
    chooser->setMode(FileChooser::Mode::OpenFolder);
    chooser->setTitle(tr("Add Folder to Albums"));
    chooser->setCurrentFolder(m_folder);
    chooser->onAccepted = [this, chooser](const std::vector<std::string>& paths, const std::string&) {
        if (!paths.empty())
            addAlbumFolder(paths.front());
        delete chooser;
    };
    chooser->onRejected = [chooser] { delete chooser; };
    chooser->open();
}

void PhotosWindow::addAlbumFolder(const std::string& folder)
{
    std::error_code ec;
    const std::filesystem::path p = std::filesystem::canonical(folder, ec);
    if (ec || !std::filesystem::is_directory(p))
        return;
    const std::string pathStr = p.string();
    for (const auto& existing : m_customAlbums) {
        if (existing == pathStr) {
            showNotification(tr("Folder is already in Albums"), 2500);
            return;
        }
    }
    m_customAlbums.push_back(pathStr);
    m_settings.setValue("CustomAlbums", m_customAlbums);
    m_settings.sync();
    m_library = scanLibrary(m_folder, m_customAlbums);
    m_root->sidebar->setLibrary(m_library);
    refreshItems();
    showNotification(tr("Added album: ") + p.filename().string(), 2500);
}

void PhotosWindow::removeAlbumFolder(const std::string& folder)
{
    auto it = std::remove(m_customAlbums.begin(), m_customAlbums.end(), folder);
    if (it != m_customAlbums.end()) {
        m_customAlbums.erase(it, m_customAlbums.end());
        m_settings.setValue("CustomAlbums", m_customAlbums);
        m_settings.sync();
        if (m_selection == "album:" + folder)
            m_selection = "all";
        m_library = scanLibrary(m_folder, m_customAlbums);
        m_root->sidebar->setLibrary(m_library);
        refreshItems();
        showNotification(tr("Album removed"), 2000);
    }
}

void PhotosWindow::showGridEmptyContextMenu(const PointF& windowPos)
{
    m_contextMenu = std::make_unique<Menu>();
    const size_t open = m_contextMenu->addItem(tr("Open Library Folder…"), [this] {
        chooseFolder();
    });
    m_contextMenu->setItemKeyEquivalent(open, "o", ControlModifier);

    m_contextMenu->addItem(tr("Add Folder to Albums…"), [this] {
        chooseAlbumFolder();
    });

    m_contextMenu->addSeparator();
    const size_t sb = m_contextMenu->addItem(m_sidebarVisible ? tr("Hide Sidebar") : tr("Show Sidebar"), [this] {
        toggleSidebar();
    });
    m_contextMenu->setItemKeyEquivalent(sb, "\\", ControlModifier);

    const size_t inf = m_contextMenu->addItem(m_infoVisible ? tr("Hide Info Panel") : tr("Show Info Panel"), [this] {
        toggleInfo();
    });
    m_contextMenu->setItemKeyEquivalent(inf, "i", ControlModifier);

    m_contextMenu->popup(&m_window, windowPos);
}

void PhotosWindow::showDetailContextMenu(const PointF& windowPos)
{
    if (!m_detail || m_detailIndex < 0 || m_detailIndex >= int(m_items.size()))
        return;
    const PhotoItem& item = m_items[size_t(m_detailIndex)];
    const std::string path = item.path;
    const bool isFav = m_favorites.contains(path);

    m_contextMenu = std::make_unique<Menu>();
    const size_t back = m_contextMenu->addItem(tr("Back to Photos"), [this] {
        backToGrid();
    });
    m_contextMenu->setItemKeyEquivalent(back, "Escape", 0);

    m_contextMenu->addSeparator();
    const size_t bg = m_contextMenu->addItem(tr("Set as Desktop Background"), [this, path] {
        setAsWallpaper(path);
    });
    m_contextMenu->setItemKeyEquivalent(bg, "b", ControlModifier);

    const size_t fav = m_contextMenu->addItem(isFav ? tr("Remove from Favorites") : tr("Favorite"), [this] {
        toggleFavorite();
    });
    m_contextMenu->setItemKeyEquivalent(fav, "f", ControlModifier);

    m_contextMenu->addSeparator();
    const size_t rotL = m_contextMenu->addItem(tr("Rotate Left 90°"), [this] {
        rotatePreview(-90);
    });
    m_contextMenu->setItemKeyEquivalent(rotL, "[", ControlModifier);

    const size_t rotR = m_contextMenu->addItem(tr("Rotate Right 90°"), [this] {
        rotatePreview(90);
    });
    m_contextMenu->setItemKeyEquivalent(rotR, "]", ControlModifier);

    if (m_root->detail->previewRotation() != 0) {
        m_contextMenu->addItem(tr("Save Rotation to File"), [this] {
            savePreviewRotation();
        });
        m_contextMenu->addItem(tr("Reset Preview"), [this] {
            m_root->detail->resetPreviewRotation();
            updateHeader();
        });
    }

    const size_t fit = m_contextMenu->addItem(tr("Fit to Window"), [this] {
        m_root->detail->zoomFit();
    });
    m_contextMenu->setItemKeyEquivalent(fit, "0", ControlModifier);

    const size_t act = m_contextMenu->addItem(tr("Actual Size"), [this] {
        m_root->detail->zoomActualSize();
    });
    m_contextMenu->setItemKeyEquivalent(act, "1", ControlModifier);

    const size_t fs = m_contextMenu->addItem(m_window.states().fullScreen ? tr("Exit Full Screen") : tr("Enter Full Screen"), [this] {
        toggleFullScreen();
    });
    m_contextMenu->setItemKeyEquivalent(fs, "F11", 0);

    m_contextMenu->addSeparator();
    const size_t ren = m_contextMenu->addItem(tr("Rename…"), [this] {
        showRenameDialog(m_detailIndex);
    });
    m_contextMenu->setItemKeyEquivalent(ren, "F2", 0);

    const size_t res = m_contextMenu->addItem(tr("Resize…"), [this] {
        showResizeDialog(m_detailIndex);
    });
    m_contextMenu->setItemKeyEquivalent(res, "r", ControlModifier | ShiftModifier);

    const size_t conv = m_contextMenu->addItem(tr("Convert To…"), [this] {
        showConvertDialog(m_detailIndex);
    });
    m_contextMenu->setItemKeyEquivalent(conv, "e", ControlModifier | ShiftModifier);

    m_contextMenu->addSeparator();
    const size_t reveal = m_contextMenu->addItem(tr("Show in File Manager"), [this, path] {
        showItemInFileManager(path);
    });
    m_contextMenu->setItemKeyEquivalent(reveal, "f", ControlModifier | ShiftModifier);

    const size_t del = m_contextMenu->addItem(tr("Move to Trash"), [this] {
        moveToTrash(m_detailIndex);
    });
    m_contextMenu->setItemKeyEquivalent(del, "Delete", 0);

    m_contextMenu->popup(&m_window, windowPos);
}

void PhotosWindow::showSidebarContextMenu(int rowIndex, const PointF& windowPos)
{
    m_contextMenu = std::make_unique<Menu>();
    const auto* r = m_root->sidebar->row(rowIndex);
    if (r && r->id.starts_with("album:")) {
        const std::string albumPath = r->path;
        const bool isCustom = r->custom;
        m_contextMenu->addItem(tr("Open Album"), [this, r] {
            setSelection(r->id);
        });
        m_contextMenu->addItem(tr("Show in File Manager"), [this, albumPath] {
            showItemInFileManager(albumPath);
        });
        if (isCustom) {
            m_contextMenu->addSeparator();
            m_contextMenu->addItem(tr("Remove from Albums"), [this, albumPath] {
                removeAlbumFolder(albumPath);
            });
        }
        m_contextMenu->addSeparator();
    }
    m_contextMenu->addItem(tr("Add Folder to Albums…"), [this] {
        chooseAlbumFolder();
    });
    const size_t add = m_contextMenu->addItem(tr("Open Library Folder…"), [this] {
        chooseFolder();
    });
    m_contextMenu->setItemKeyEquivalent(add, "o", ControlModifier);
    m_contextMenu->popup(&m_window, windowPos);
}

void PhotosWindow::buildMainMenu()
{
    m_mainMenu = std::make_unique<Menu>();
    const uint32_t ctrl = ControlModifier;
    const uint32_t shift = ShiftModifier;

    // 1. Photos (App Menu)
    Menu* app = m_mainMenu->addSubmenu(tr("Photos"));
    app->setRole(MenuRole::Application);
    app->addItem(tr("About Photos"), [this] { showAbout(); });
    app->addSeparator();
    const size_t hideItem = app->addItem(tr("Hide Photos"), [this] { m_window.hideApplication(); });
    app->setItemKeyEquivalent(hideItem, "h", ctrl);
    app->addSeparator();
    const size_t quitItem = app->addItem(tr("Quit Photos"), [] { Application::instance()->quit(0); });
    app->setItemKeyEquivalent(quitItem, "q", ctrl);

    // 2. File
    Menu* file = m_mainMenu->addSubmenu(tr("File"));
    const size_t open = file->addItem(tr("Open Library Folder…"), [this] { chooseFolder(); });
    file->setItemKeyEquivalent(open, "o", ctrl);
    file->addItem(tr("Add Folder to Albums…"), [this] { chooseAlbumFolder(); });
    file->addSeparator();

    const size_t backItem = file->addItem(tr("Back to Library"), [this] { backToGrid(); });
    file->setItemKeyEquivalent(backItem, "w", ctrl);

    const size_t reveal = file->addItem(tr("Show in File Manager"), [this] {
        showItemInFileManager(currentPhotoPath());
    });
    file->setItemKeyEquivalent(reveal, "f", ctrl | shift);

    const size_t setBgFile = file->addItem(tr("Set as Desktop Background"), [this] {
        const std::string path = currentPhotoPath();
        if (!path.empty())
            setAsWallpaper(path);
    });
    file->setItemKeyEquivalent(setBgFile, "b", ctrl);
    file->addSeparator();

    const size_t trashItem = file->addItem(tr("Move to Trash"), [this] {
        const int idx = currentPhotoIndex();
        if (idx >= 0)
            moveToTrash(idx);
    });
    file->setItemKeyEquivalent(trashItem, "Delete", 0);

    file->onAboutToShow = [file, backItem, reveal, setBgFile, trashItem, this] {
        const bool hasActive = (currentPhotoIndex() >= 0);
        file->setItemEnabled(backItem, m_detail);
        file->setItemEnabled(reveal, hasActive || !m_folder.empty());
        file->setItemEnabled(setBgFile, hasActive);
        file->setItemEnabled(trashItem, hasActive);
    };

    // 3. Edit
    Menu* edit = m_mainMenu->addSubmenu(tr("Edit"));
    const size_t selectAll = edit->addItem(tr("Select All"), [this] {
        if (!m_detail && !m_items.empty())
            m_root->grid->setSelectedIndex(0);
    });
    edit->setItemKeyEquivalent(selectAll, "a", ctrl);

    const size_t deselectAll = edit->addItem(tr("Deselect All"), [this] {
        if (!m_detail)
            m_root->grid->setSelectedIndex(-1);
    });
    edit->setItemKeyEquivalent(deselectAll, "a", ctrl | shift);
    edit->addSeparator();

    const size_t renameItem = edit->addItem(tr("Rename…"), [this] {
        const int idx = currentPhotoIndex();
        if (idx >= 0)
            showRenameDialog(idx);
    });
    edit->setItemKeyEquivalent(renameItem, "F2", 0);

    const size_t resize = edit->addItem(tr("Resize…"), [this] {
        showResizeDialog(currentPhotoIndex());
    });
    edit->setItemKeyEquivalent(resize, "r", ctrl | shift);

    const size_t convert = edit->addItem(tr("Convert To…"), [this] {
        showConvertDialog(currentPhotoIndex());
    });
    edit->setItemKeyEquivalent(convert, "e", ctrl | shift);

    edit->onAboutToShow = [edit, selectAll, deselectAll, renameItem, resize, convert, this] {
        const bool hasActive = (currentPhotoIndex() >= 0);
        edit->setItemEnabled(selectAll, !m_detail && !m_items.empty());
        edit->setItemEnabled(deselectAll, !m_detail && (m_root->grid->selectedIndex() >= 0));
        edit->setItemEnabled(renameItem, hasActive);
        edit->setItemEnabled(resize, hasActive);
        edit->setItemEnabled(convert, hasActive);
    };

    // 4. Image
    Menu* image = m_mainMenu->addSubmenu(tr("Image"));
    const size_t rotateLeft = image->addItem(tr("Rotate Preview Left 90°"), [this] {
        if (m_detail)
            rotatePreview(-90);
        else if (currentPhotoIndex() >= 0)
            rotateImage(-90);
    });
    image->setItemKeyEquivalent(rotateLeft, "[", ctrl);

    const size_t rotateRight = image->addItem(tr("Rotate Preview Right 90°"), [this] {
        if (m_detail)
            rotatePreview(90);
        else if (currentPhotoIndex() >= 0)
            rotateImage(90);
    });
    image->setItemKeyEquivalent(rotateRight, "]", ctrl);

    const size_t rotate180 = image->addItem(tr("Rotate 180°"), [this] {
        if (m_detail)
            rotatePreview(180);
        else if (currentPhotoIndex() >= 0)
            rotateImage(180);
    });
    image->setItemKeyEquivalent(rotate180, "[", ctrl | shift);

    const size_t flipHorizontal = image->addItem(tr("Flip Horizontal"), [this] { flipImage(true); });
    image->setItemKeyEquivalent(flipHorizontal, "h", ctrl | shift);
    const size_t flipVertical = image->addItem(tr("Flip Vertical"), [this] { flipImage(false); });
    image->setItemKeyEquivalent(flipVertical, "v", ctrl | shift);
    image->addSeparator();

    const size_t saveRotItem = image->addItem(tr("Save Rotation to File"), [this] {
        savePreviewRotation();
    });
    image->setItemKeyEquivalent(saveRotItem, "s", ctrl);

    const size_t resetRotItem = image->addItem(tr("Reset Preview Rotation"), [this] {
        resetPreviewRotation();
    });
    image->setItemKeyEquivalent(resetRotItem, "r", ctrl);
    image->addSeparator();

    const size_t setBg = image->addItem(tr("Set as Desktop Background"), [this] {
        const std::string path = currentPhotoPath();
        if (!path.empty())
            setAsWallpaper(path);
    });
    image->setItemKeyEquivalent(setBg, "b", ctrl);

    image->onAboutToShow = [image, rotateLeft, rotateRight, rotate180, flipHorizontal, flipVertical, saveRotItem, resetRotItem, setBg, this] {
        const bool hasActive = (currentPhotoIndex() >= 0);
        const bool hasRot = (m_detail && m_root->detail && m_root->detail->previewRotation() != 0);
        image->setItemEnabled(rotateLeft, hasActive);
        image->setItemEnabled(rotateRight, hasActive);
        image->setItemEnabled(rotate180, hasActive);
        image->setItemEnabled(flipHorizontal, hasActive);
        image->setItemEnabled(flipVertical, hasActive);
        image->setItemEnabled(saveRotItem, hasRot);
        image->setItemEnabled(resetRotItem, hasRot);
        image->setItemEnabled(setBg, hasActive);
    };

    // 5. View
    Menu* view = m_mainMenu->addSubmenu(tr("View"));
    const size_t backView = view->addItem(tr("Back to Library"), [this] { backToGrid(); });
    view->setItemKeyEquivalent(backView, "Escape", 0);
    view->addSeparator();

    const size_t zoomInItem = view->addItem(tr("Zoom In"), [this] { zoomIn(); });
    view->setItemKeyEquivalent(zoomInItem, "=", ctrl);
    const size_t zoomOutItem = view->addItem(tr("Zoom Out"), [this] { zoomOut(); });
    view->setItemKeyEquivalent(zoomOutItem, "-", ctrl);
    view->addSeparator();

    const size_t fit = view->addItem(tr("Fit to Window"), [this] { if (m_detail) m_root->detail->zoomFit(); });
    view->setItemKeyEquivalent(fit, "0", ctrl);
    const size_t actual = view->addItem(tr("Actual Size"), [this] { if (m_detail) m_root->detail->zoomActualSize(); });
    view->setItemKeyEquivalent(actual, "1", ctrl);
    view->addSeparator();

    Menu* thumbSizeMenu = view->addSubmenu(tr("Thumbnail Size"));
    const size_t szSmall = thumbSizeMenu->addItem(tr("Small (96px)"), [this] {
        m_zoomLevel = 0;
        m_root->grid->setCellSize(gridSizes()[0]);
    });
    const size_t szMed = thumbSizeMenu->addItem(tr("Medium (148px)"), [this] {
        m_zoomLevel = 1;
        m_root->grid->setCellSize(gridSizes()[1]);
    });
    const size_t szLarge = thumbSizeMenu->addItem(tr("Large (208px)"), [this] {
        m_zoomLevel = 2;
        m_root->grid->setCellSize(gridSizes()[2]);
    });
    thumbSizeMenu->onAboutToShow = [thumbSizeMenu, szSmall, szMed, szLarge, this] {
        thumbSizeMenu->setItemChecked(szSmall, m_zoomLevel == 0);
        thumbSizeMenu->setItemChecked(szMed, m_zoomLevel == 1);
        thumbSizeMenu->setItemChecked(szLarge, m_zoomLevel == 2);
    };

    view->addSeparator();
    const size_t fs = view->addItem(tr("Toggle Full Screen"), [this] {
        toggleFullScreen();
    });
    view->setItemKeyEquivalent(fs, "F11", 0);

    const size_t sidebarItem = view->addItem(tr("Toggle Sidebar"), [this] { toggleSidebar(); });
    view->setItemKeyEquivalent(sidebarItem, "\\", ctrl);
    const size_t infoItem = view->addItem(tr("Toggle Info Panel"), [this] { toggleInfo(); });
    view->setItemKeyEquivalent(infoItem, "i", ctrl);

    view->onAboutToShow = [view, backView, fit, actual, sidebarItem, infoItem, this] {
        view->setItemEnabled(backView, m_detail);
        view->setItemEnabled(fit, m_detail);
        view->setItemEnabled(actual, m_detail);
        view->setItemChecked(sidebarItem, m_sidebarVisible);
        view->setItemChecked(infoItem, m_infoVisible);
    };

    // 6. Window
    Menu* win = m_mainMenu->addSubmenu(tr("Window"));
    win->setRole(MenuRole::Windows);
    const size_t minItem = win->addItem(tr("Minimize"), [this] { m_window.minimize(); });
    win->setItemKeyEquivalent(minItem, "m", ctrl);
    win->addItem(tr("Zoom / Maximize"), [this] {
        m_window.setMaximized(!m_window.states().maximized);
    });
    win->addSeparator();
    win->addItem(tr("Bring All to Front"), [this] { m_window.activate(); });

    // 7. Help
    Menu* help = m_mainMenu->addSubmenu(tr("Help"));
    help->setRole(MenuRole::Help);
    help->addItem(tr("Keyboard Shortcuts"), [this] { showShortcutsDialog(); });
    help->addItem(tr("Cutefish Website"), [] {
        cutefish::base::Process::startDetached("xdg-open", {"https://cutefish.org"});
    });
    help->addSeparator();
    help->addItem(tr("About Photos"), [this] { showAbout(); });

    m_window.setMainMenu(m_mainMenu.get());
}

} // namespace photos
