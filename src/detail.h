// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include "photo.h"

#include "ui/view.h"
#include "ui/image_view.h"
#include "ui/image.h"

#include <functional>

namespace photos {

// The single photo page: the picture fitted to the view, zoomable with the
// toolbar buttons, Ctrl and the wheel, or Ctrl+0 (fit) and Ctrl+1 (actual
// size). When it is larger than the area it can be dragged with the pointer,
// and pans with the wheel.
class DetailView : public cutefish::ui::View {
public:
    DetailView();

    void setPhoto(const PhotoItem& item);
    void clearPhoto();
    bool hasPhoto() const { return !m_item.path.empty(); }
    const PhotoItem& item() const { return m_item; }
    cutefish::ui::SizeF naturalSize() const { return m_natural; }

    // 0 means fit; otherwise logical pixels per image pixel.
    double zoom() const { return m_zoom; }
    bool isFit() const { return m_zoom == 0; }
    void zoomIn();
    void zoomOut();
    void zoomFit();
    void zoomActualSize();

    // Preview rotation in degrees (0, 90, 180, 270)
    int previewRotation() const { return m_previewRotation; }
    void rotatePreview(int deltaDegrees);
    void resetPreviewRotation();

    std::function<void(bool ready)> onStatusChanged;
    std::function<void()> onZoomChanged;
    std::function<void(int rotation)> onRotationChanged;
    std::function<void(const cutefish::ui::PointF& windowPos)> onContextMenu;

protected:
    void layout() override;
    bool acceptsPointer() const override { return true; }
    void pointerPress(const cutefish::ui::PointerEvent& event) override;
    void pointerMove(const cutefish::ui::PointerEvent& event) override;
    void pointerRelease(const cutefish::ui::PointerEvent& event) override;
    bool scroll(const cutefish::ui::ScrollEvent& event) override;
    bool gesture(const cutefish::ui::GestureEvent& event) override;
    void hoverMove(const cutefish::ui::PointerEvent& event) override;

private:
    void applyImageView();
    void hidePicture();
    void setZoom(double zoom, cutefish::ui::PointF around);

    PhotoItem m_item;
    class RotatedImageView;
    RotatedImageView* m_image;
    int m_previewRotation = 0;
    double m_zoom = 0;          // 0 = fit the view
    double m_fitScale = 1;      // scale that fits, once the size is known
    cutefish::ui::SizeF m_natural;
    cutefish::ui::PointF m_offset;   // image's origin in view coordinates
    cutefish::ui::PointF m_pointer;  // where the pointer is, for pinch zoom
    bool m_panning = false;
    cutefish::ui::PointF m_panOrigin;
    cutefish::ui::PointF m_offsetOrigin;
    double m_pinchScale = 1;    // the scale of the pinch the update belongs to
};

} // namespace photos
