// SPDX-License-Identifier: BSD-3-Clause
#include "detail.h"

#include "ui/color_provider.h"
#include "ui/event.h"
#include "ui/image_view.h"
#include "ui/painter.h"
#include "ui/theme.h"

#include <algorithm>
#include <cmath>

using namespace cutefish::ui;

namespace photos {

static constexpr double kMinimumZoom = 0.05;
static constexpr double kMaximumZoom = 16.0;

class DetailView::RotatedImageView : public ImageView {
public:
    RotatedImageView()
    {
        setName("picture");
        setFillMode(ImageView::FillMode::Stretch);
        setAutoTransform(true);
        setSmooth(true);
    }

    void setRotationAngle(int degrees)
    {
        int norm = degrees % 360;
        if (norm < 0)
            norm += 360;
        if (m_rotation == norm)
            return;
        m_rotation = norm;
        requestPaint();
    }

    int rotationAngle() const { return m_rotation; }

protected:
    void paint(Painter& painter) override
    {
        if (m_rotation == 0) {
            ImageView::paint(painter);
            return;
        }
        painter.save();
        const RectF r = localRect();
        painter.rotate(double(m_rotation), {r.width / 2.0, r.height / 2.0});
        ImageView::paint(painter);
        painter.restore();
    }

private:
    int m_rotation = 0;
};

DetailView::DetailView()
{
    setName("detail");
    setClipsChildren(true);
    setTracksHover(true);
    m_image = addChild<RotatedImageView>();
    m_image->onStatusChanged = [this](ImageView::Status status) {
        if (status == ImageView::Status::Ready) {
            m_natural = m_image->sizeHint();
            applyImageView();
            if (onStatusChanged)
                onStatusChanged(true);
        } else if (status == ImageView::Status::Error) {
            if (onStatusChanged)
                onStatusChanged(false);
        }
    };
    hidePicture();
}

// Kept as a helper so the source stays easy to read: the picture draws only
// while it has one.
void DetailView::hidePicture()
{
    m_image->setVisible(false);
}

void DetailView::setPhoto(const PhotoItem& item)
{
    m_item = item;
    m_natural = {};
    m_zoom = 0;
    m_offset = {};
    m_previewRotation = 0;
    m_image->setRotationAngle(0);
    m_image->setVisible(true);
    m_image->setSource(item.path);
    if (onZoomChanged)
        onZoomChanged();
    if (onRotationChanged)
        onRotationChanged(0);
}

void DetailView::clearPhoto()
{
    m_item = {};
    m_natural = {};
    m_zoom = 0;
    m_previewRotation = 0;
    m_image->setRotationAngle(0);
    m_image->setVisible(false);
    m_image->setSource({});
    if (onRotationChanged)
        onRotationChanged(0);
}

void DetailView::rotatePreview(int deltaDegrees)
{
    m_previewRotation = (m_previewRotation + deltaDegrees) % 360;
    if (m_previewRotation < 0)
        m_previewRotation += 360;
    if (m_image)
        m_image->setRotationAngle(m_previewRotation);
    applyImageView();
    if (onRotationChanged)
        onRotationChanged(m_previewRotation);
}

void DetailView::resetPreviewRotation()
{
    if (m_previewRotation == 0)
        return;
    m_previewRotation = 0;
    if (m_image)
        m_image->setRotationAngle(0);
    applyImageView();
    if (onRotationChanged)
        onRotationChanged(0);
}

void DetailView::applyImageView()
{
    const RectF viewport = localRect();
    const double rawWidth = m_natural.width > 0 ? m_natural.width : 0;
    const double rawHeight = m_natural.height > 0 ? m_natural.height : 0;
    if (viewport.width <= 0 || viewport.height <= 0 || rawWidth <= 0 || rawHeight <= 0) {
        m_image->setGeometry(viewport);
        return;
    }

    const bool rotated90 = (m_previewRotation == 90 || m_previewRotation == 270);
    const double naturalWidth = rotated90 ? rawHeight : rawWidth;
    const double naturalHeight = rotated90 ? rawWidth : rawHeight;

    m_fitScale = std::min(viewport.width / naturalWidth, viewport.height / naturalHeight);
    const double scale = m_zoom == 0 ? m_fitScale : std::clamp(m_zoom, kMinimumZoom, kMaximumZoom);
    const double visW = naturalWidth * scale;
    const double visH = naturalHeight * scale;
    const double unrotW = rawWidth * scale;
    const double unrotH = rawHeight * scale;

    if (visW <= viewport.width)
        m_offset.x = (viewport.width - visW) / 2;
    else
        m_offset.x = std::clamp(m_offset.x, viewport.width - visW, 0.0);

    if (visH <= viewport.height)
        m_offset.y = (viewport.height - visH) / 2;
    else
        m_offset.y = std::clamp(m_offset.y, viewport.height - visH, 0.0);

    const double cx = m_offset.x + visW / 2.0;
    const double cy = m_offset.y + visH / 2.0;

    m_image->setGeometry({cx - unrotW / 2.0, cy - unrotH / 2.0, unrotW, unrotH});
    setCursorShape(visW > viewport.width || visH > viewport.height ? CursorShape::Grab : CursorShape::Default);
}

void DetailView::layout()
{
    applyImageView();
}

void DetailView::setZoom(double zoom, PointF around)
{
    const RectF viewport = localRect();
    const double oldScale = m_zoom == 0 ? m_fitScale : m_zoom;
    const double newScale = std::clamp(zoom, kMinimumZoom, kMaximumZoom);
    if (newScale == oldScale)
        return;
    // The point of the picture under the pointer stays where it is.
    const PointF viewPoint {around.x, around.y};
    m_offset.x = viewPoint.x - (viewPoint.x - m_offset.x) * (newScale / oldScale);
    m_offset.y = viewPoint.y - (viewPoint.y - m_offset.y) * (newScale / oldScale);
    m_zoom = newScale;
    // Close enough to the fit: keep the picture centred in fit mode.
    if (m_zoom <= m_fitScale * 1.02)
        m_zoom = 0;
    applyImageView();
    if (onZoomChanged)
        onZoomChanged();
}

void DetailView::zoomIn()
{
    const RectF viewport = localRect();
    const PointF centre {viewport.width / 2, viewport.height / 2};
    const double scale = m_zoom == 0 ? m_fitScale : m_zoom;
    setZoom(scale * 1.25, centre);
}

void DetailView::zoomOut()
{
    const RectF viewport = localRect();
    const PointF centre {viewport.width / 2, viewport.height / 2};
    const double scale = m_zoom == 0 ? m_fitScale : m_zoom;
    setZoom(scale / 1.25, centre);
}

void DetailView::zoomFit()
{
    if (m_zoom != 0) {
        m_zoom = 0;
        applyImageView();
        if (onZoomChanged)
            onZoomChanged();
    }
}

void DetailView::zoomActualSize()
{
    const RectF viewport = localRect();
    setZoom(1.0, {viewport.width / 2, viewport.height / 2});
}

void DetailView::pointerPress(const PointerEvent& event)
{
    if (event.button != MouseButton::Left)
        return;
    m_panning = true;
    m_panOrigin = event.position;
    m_offsetOrigin = m_offset;
    setCursorShape(CursorShape::Grabbing);
}

void DetailView::pointerMove(const PointerEvent& event)
{
    if (!m_panning)
        return;
    m_offset = m_offsetOrigin + (event.position - m_panOrigin);
    applyImageView();
    if (m_panning)
        setCursorShape(CursorShape::Grabbing);
}

void DetailView::pointerRelease(const PointerEvent& event)
{
    if (event.button == MouseButton::Right) {
        if (onContextMenu)
            onContextMenu(mapToWindow(event.position));
        return;
    }
    if (event.button != MouseButton::Left)
        return;
    m_panning = false;
    const RectF viewport = localRect();
    const double scale = m_zoom == 0 ? m_fitScale : m_zoom;
    const double width = m_natural.width * scale;
    const double height = m_natural.height * scale;
    setCursorShape(width > viewport.width || height > viewport.height ? CursorShape::Grab : CursorShape::Default);
}

void DetailView::hoverMove(const PointerEvent& event)
{
    m_pointer = event.position;
}

bool DetailView::gesture(const GestureEvent& event)
{
    if (event.type == GestureEvent::Type::PinchBegin) {
        m_pinchScale = 1;
        return true;
    }
    if (event.type == GestureEvent::Type::PinchUpdate) {
        const double scale = std::max(0.01, m_pinchScale);
        const double delta = event.scale / scale;
        m_pinchScale = event.scale;
        const RectF viewport = localRect();
        // The point under the fingers (the last hover position) stays put.
        const PointF around {m_pointer.x != 0 || m_pointer.y != 0 ? m_pointer : PointF{viewport.width / 2, viewport.height / 2}};
        const double current = m_zoom == 0 ? m_fitScale : m_zoom;
        setZoom(current * delta, around);
        return true;
    }
    if (event.type == GestureEvent::Type::PinchEnd) {
        m_pinchScale = 1;
        return true;
    }
    return false;
}

bool DetailView::scroll(const ScrollEvent& event)
{
    const RectF viewport = localRect();
    const PointF around = (m_pointer.x > 0 && m_pointer.y > 0)
        ? m_pointer
        : PointF{viewport.width / 2.0, viewport.height / 2.0};

    if ((event.modifiers & ControlModifier) || !event.precise) {
        const double delta = (event.value120Y != 0) ? -double(event.value120Y) : event.deltaY;
        if (delta != 0) {
            const double scale = m_zoom == 0 ? m_fitScale : m_zoom;
            const double factor = delta < 0 ? 1.15 : (1.0 / 1.15);
            setZoom(scale * factor, around);
            return true;
        }
    }

    if (m_zoom == 0)
        return false;

    m_offset.x -= event.deltaX;
    m_offset.y -= event.deltaY;
    applyImageView();
    return true;
}

} // namespace photos
