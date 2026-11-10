#pragma once

#include "fried/editor/editor.h"

#include <QImage>
#include <QWidget>

// Shows the player's frame with the overlay on it, and hands the mouse to the editor in the frame's own pixels.
class ViewportWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ViewportWidget(fried::editor::Editor &editor, QWidget *parent = nullptr);

    // The editor's frame or overlay changed.
    void refresh();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    QPointF toFrame(const QPointF &position) const;
    double scale() const;
    QPointF origin() const;

    fried::editor::Editor &m_editor;
    QImage m_image;
};
