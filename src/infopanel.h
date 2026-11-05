// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include "photo.h"
#include "ui/view.h"
#include "ui/image_view.h"

namespace cutefish::ui {
class ImageView;
class Label;
class Button;
}

#include <functional>
#include <string>

namespace photos {

// The panel on the right, as Photos has it: the picture, its name and its
// metadata. In the grid, with nothing selected, it summarises the library.
class InfoPanel : public cutefish::ui::View {
public:
    InfoPanel();

    static constexpr double kWidth = 264;

    void showSummary(int count, uint64_t totalSize, const std::string& root);
    void showPhoto(const PhotoItem& item, bool ready, const cutefish::ui::SizeF& natural, bool isFavorite);
    void setFavorite(bool isFavorite);

    std::function<void()> onToggleFavorite;

protected:
    void layout() override;

private:
    void setMode(bool photo);

    cutefish::ui::ImageView* m_preview = nullptr;
    cutefish::ui::Label* m_title = nullptr;
    cutefish::ui::Label* m_subtitle = nullptr;
    cutefish::ui::Label* m_dimensionsCaption = nullptr;
    cutefish::ui::Label* m_dimensionsValue = nullptr;
    cutefish::ui::Label* m_sizeCaption = nullptr;
    cutefish::ui::Label* m_sizeValue = nullptr;
    cutefish::ui::Label* m_dateCaption = nullptr;
    cutefish::ui::Label* m_dateValue = nullptr;
    cutefish::ui::Button* m_favoriteButton = nullptr;
    cutefish::ui::Label* m_summaryTitle = nullptr;
    cutefish::ui::Label* m_summaryCount = nullptr;
    cutefish::ui::Label* m_summarySize = nullptr;
    cutefish::ui::Label* m_summaryFolder = nullptr;

    std::string m_mode = "summary";
    bool m_favorite = false;
};

} // namespace photos
