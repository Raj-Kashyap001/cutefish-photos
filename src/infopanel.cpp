// SPDX-License-Identifier: BSD-3-Clause
#include "infopanel.h"

#include "base/i18n.h"
#include "ui/button.h"
#include "ui/label.h"
#include "ui/theme.h"

#include <cstdio>

using namespace cutefish::ui;
using cutefish::i18n::tr;

namespace photos {

static constexpr double kPadding = 16;
static constexpr double kPreviewHeight = 180;
static constexpr double kCaptionSize = 11;
static constexpr double kValueSize = 13;

static Label* makeCaption(View* parent, const std::string& text)
{
    Label* label = parent->addChild<Label>(text);
    label->setFontSize(kCaptionSize);
    label->setWeight(600);
    label->setColor(Theme::current().disabledText);
    return label;
}

static Label* makeValue(View* parent)
{
    Label* label = parent->addChild<Label>(std::string());
    label->setFontSize(kValueSize);
    label->setElide(Label::Elide::Right);
    return label;
}

InfoPanel::InfoPanel()
{
    setName("infoPanel");

    m_preview = addChild<ImageView>();
    m_preview->setName("preview");
    m_preview->setFillMode(ImageView::FillMode::PreserveAspectFit);
    m_preview->setCornerRadius(10, Corners::Circular);
    m_preview->setThumbnail(true);
    m_preview->setAutoTransform(true);

    m_title = addChild<Label>();
    m_title->setWeight(700);
    m_title->setFontSize(14);
    m_title->setWrap(true);
    m_title->setMaximumLineCount(2);
    m_title->setElide(Label::Elide::Right);

    m_subtitle = addChild<Label>();
    m_subtitle->setFontSize(12);
    m_subtitle->setColor(Theme::current().disabledText);

    m_dimensionsCaption = makeCaption(this, tr("Dimensions"));
    m_dimensionsValue = makeValue(this);
    m_sizeCaption = makeCaption(this, tr("Size"));
    m_sizeValue = makeValue(this);
    m_dateCaption = makeCaption(this, tr("Added"));
    m_dateValue = makeValue(this);

    m_favoriteButton = addChild<Button>(tr("Add to Favorites"));
    m_favoriteButton->onClicked = [this] {
        if (onToggleFavorite)
            onToggleFavorite();
    };

    m_summaryTitle = addChild<Label>(tr("Photos"));
    m_summaryTitle->setWeight(700);
    m_summaryTitle->setFontSize(18);

    m_summaryCount = addChild<Label>();
    m_summaryCount->setFontSize(13);

    m_summarySize = addChild<Label>();
    m_summarySize->setFontSize(13);
    m_summarySize->setColor(Theme::current().disabledText);

    m_summaryFolder = addChild<Label>();
    m_summaryFolder->setFontSize(11);
    m_summaryFolder->setColor(Theme::current().disabledText);
    m_summaryFolder->setWrap(true);
    m_summaryFolder->setElide(Label::Elide::Middle);

    setMode(false);
}

void InfoPanel::setMode(bool photo)
{
    m_mode = photo ? "photo" : "summary";
    const bool show = photo;
    for (View* view : {static_cast<View*>(m_preview), static_cast<View*>(m_title),
                       static_cast<View*>(m_subtitle), static_cast<View*>(m_dimensionsCaption),
                       static_cast<View*>(m_dimensionsValue), static_cast<View*>(m_sizeCaption),
                       static_cast<View*>(m_sizeValue), static_cast<View*>(m_dateCaption),
                       static_cast<View*>(m_dateValue), static_cast<View*>(m_favoriteButton)})
        view->setVisible(show);
    for (View* view : {static_cast<View*>(m_summaryTitle), static_cast<View*>(m_summaryCount),
                       static_cast<View*>(m_summarySize), static_cast<View*>(m_summaryFolder)})
        view->setVisible(!show);
    requestLayout();
}

void InfoPanel::showSummary(int count, uint64_t totalSize, const std::string& root)
{
    setMode(false);
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, tr("%d Photos"), count);
    m_summaryCount->setText(buffer);
    m_summarySize->setText(humanSize(totalSize));
    m_summaryFolder->setText(root);
}

void InfoPanel::showPhoto(const PhotoItem& item, bool ready, const SizeF& natural, bool isFavorite)
{
    setMode(true);
    m_title->setText(item.name);
    m_subtitle->setText(typeDescription(item.path));
    if (ready && natural.width > 0) {
        char buffer[64];
        std::snprintf(buffer, sizeof buffer, "%.0f × %.0f", natural.width, natural.height);
        m_dimensionsValue->setText(buffer);
    } else {
        m_dimensionsValue->setText(tr("Loading…"));
    }
    m_sizeValue->setText(humanSize(item.size));
    m_dateValue->setText(formatDate(item.modified));
    m_preview->setSource(item.path);
    setFavorite(isFavorite);
}

void InfoPanel::setFavorite(bool isFavorite)
{
    m_favorite = isFavorite;
    m_favoriteButton->setText(isFavorite ? tr("Remove from Favorites") : tr("Add to Favorites"));
}

void InfoPanel::layout()
{
    const double width = std::max(0.0, geometry().width);
    const double contentWidth = std::max(0.0, width - 2 * kPadding);
    double y = kPadding;

    if (m_mode == "photo") {
        m_preview->setGeometry({kPadding, y, contentWidth, kPreviewHeight});
        y += kPreviewHeight + 14;

        const double titleHeight = 38;   // two lines of 14px text
        m_title->setGeometry({kPadding, y, contentWidth, titleHeight});
        y += titleHeight + 2;
        m_subtitle->setGeometry({kPadding, y, contentWidth, 18});
        y += 18 + 18;

        const auto placeRow = [&](Label* caption, Label* value) {
            caption->setGeometry({kPadding, y, contentWidth, 15});
            value->setGeometry({kPadding, y + 15, contentWidth, 20});
            y += 15 + 20 + 14;
        };
        placeRow(m_dimensionsCaption, m_dimensionsValue);
        placeRow(m_sizeCaption, m_sizeValue);
        placeRow(m_dateCaption, m_dateValue);

        const SizeF size = m_favoriteButton->sizeHint();
        m_favoriteButton->setGeometry({kPadding, y, std::min(contentWidth, size.width + 24), size.height});
        return;
    }

    m_summaryTitle->setGeometry({kPadding, y, contentWidth, 26});
    y += 26 + 4;
    m_summaryCount->setGeometry({kPadding, y, contentWidth, 20});
    y += 20 + 2;
    m_summarySize->setGeometry({kPadding, y, contentWidth, 18});
    y += 18 + 18;
    m_summaryFolder->setGeometry({kPadding, y, contentWidth, 60});
}

} // namespace photos
