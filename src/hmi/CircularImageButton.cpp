#include "CircularImageButton.h"

#include <QPainter>

CircularImageButton::CircularImageButton(QWidget *parent) : QPushButton(parent)
{}

void CircularImageButton::setImage(QString const& imgPath)
{
    _image = QImage(imgPath);
    repaint();
}

void CircularImageButton::paintEvent(QPaintEvent *)
{
    //Do not paint base implementation -> no styles are applied

    int diameter = qMin(height(), width());

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.translate(width() / 2, height() / 2);

    if (isDown())
    {
        painter.setPen(QPen(QColor("black"), 2));
        painter.setBrush(QBrush(Qt::lightGray));
        painter.drawEllipse(QRect(-diameter / 2, -diameter / 2, diameter, diameter));
    }
    painter.drawImage(QRect(-diameter / 2, -diameter / 2, diameter, diameter), _image);

}

void CircularImageButton::resizeEvent(QResizeEvent *e)
{
    QPushButton::resizeEvent(e);
    int diameter = qMin(height(), width())+4 ;
    int xOff =(width() - diameter ) / 2;
    int yOff =(height() - diameter) / 2;
    setMask(QRegion(xOff, yOff, diameter, diameter, QRegion::Ellipse));
}
