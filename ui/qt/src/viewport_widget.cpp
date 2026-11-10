#include "viewport_widget.h"

#include "asset_panel.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

using fried::editor::DrawCommand;
using fried::editor::Editor;

ViewportWidget::ViewportWidget(Editor &editor, QWidget *parent)
    : QWidget(parent)
    , m_editor(editor)
{
    setMouseTracking(true);
    setMinimumSize(320, 180);
    setAcceptDrops(true);
}

void ViewportWidget::refresh()
{
    const fried::editor::Frame &frame = m_editor.frame();
    if (frame.m_width > 0 && frame.m_rgba.size() == (size_t)frame.m_width * frame.m_height * 4)
    {
        // The image shares the frame's bytes, which stay alive in the editor until the next frame replaces them, and is copied here.
        m_image = QImage(frame.m_rgba.data(), frame.m_width, frame.m_height, frame.m_width * 4, QImage::Format_RGBA8888).copy();
    }
    update();
}

double ViewportWidget::scale() const
{
    int frameWidth = m_image.isNull() ? m_editor.viewport().frameWidth() : m_image.width();
    int frameHeight = m_image.isNull() ? m_editor.viewport().frameHeight() : m_image.height();
    return std::min((double)width() / frameWidth, (double)height() / frameHeight);
}

QPointF ViewportWidget::origin() const
{
    int frameWidth = m_image.isNull() ? m_editor.viewport().frameWidth() : m_image.width();
    int frameHeight = m_image.isNull() ? m_editor.viewport().frameHeight() : m_image.height();
    return QPointF((width() - frameWidth * scale()) / 2, (height() - frameHeight * scale()) / 2);
}

QPointF ViewportWidget::toFrame(const QPointF &position) const
{
    return (position - origin()) / scale();
}

void ViewportWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(24, 24, 28));
    if (m_image.isNull())
    {
        painter.setPen(QColor(150, 150, 160));
        painter.drawText(rect(), Qt::AlignCenter, QString::fromStdString(m_editor.message()));
        return;
    }

    painter.translate(origin());
    painter.scale(scale(), scale());
    painter.drawImage(0, 0, m_image);

    painter.setClipRect(QRectF(0, 0, m_image.width(), m_image.height()));
    for (const DrawCommand &command : m_editor.overlay())
    {
        QColor color((command.m_rgba >> 16) & 0xff, (command.m_rgba >> 8) & 0xff, command.m_rgba & 0xff, (command.m_rgba >> 24) & 0xff);
        QPen pen(color, 1.5 / scale());
        painter.setPen(pen);
        switch (command.m_kind)
        {
        case DrawCommand::Kind::Line:
            painter.drawLine(QPointF(command.m_x, command.m_y), QPointF(command.m_x + command.m_width, command.m_y + command.m_height));
            break;
        case DrawCommand::Kind::FilledRect:
            painter.fillRect(QRectF(command.m_x, command.m_y, command.m_width, command.m_height), color);
            break;
        default:
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(QRectF(command.m_x, command.m_y, command.m_width, command.m_height));
            break;
        }
    }
}

void ViewportWidget::mousePressEvent(QMouseEvent *event)
{
    setFocus();
    QPointF frame = toFrame(event->position());
    bool pans = event->button() == Qt::MiddleButton || event->button() == Qt::RightButton;
    if (pans || event->button() == Qt::LeftButton)
    {
        m_editor.pointerDown(frame.x(), frame.y(), pans ? Editor::Pointer::Pan : Editor::Pointer::Primary);
    }
}

void ViewportWidget::mouseMoveEvent(QMouseEvent *event)
{
    QPointF frame = toFrame(event->position());
    m_editor.pointerMove(frame.x(), frame.y());
}

void ViewportWidget::mouseReleaseEvent(QMouseEvent *)
{
    m_editor.pointerUp();
}

void ViewportWidget::wheelEvent(QWheelEvent *event)
{
    QPointF frame = toFrame(event->position());
    m_editor.wheel(frame.x(), frame.y(), event->angleDelta().y() / 120.0);
}

void ViewportWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat(assetMimeType()))
    {
        event->acceptProposedAction();
    }
}

void ViewportWidget::dragMoveEvent(QDragMoveEvent *event)
{
    event->acceptProposedAction();
}

void ViewportWidget::dropEvent(QDropEvent *event)
{
    std::string root, path;
    if (fried::editor::parseAssetReference(QString::fromUtf8(event->mimeData()->data(assetMimeType())).toStdString(), &root, &path))
    {
        QPointF frame = toFrame(event->position());
        m_editor.dropAsset(root, path, frame.x(), frame.y());
        event->acceptProposedAction();
    }
}
