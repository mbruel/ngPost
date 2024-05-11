#ifndef CIRCULARIMAGEBUTTON_H
#define CIRCULARIMAGEBUTTON_H
// From J.Hilk : https://forum.qt.io/topic/111821/creating-circular-qpushbutton/9
#include <QPushButton>

class CircularImageButton : public QPushButton
{
    Q_OBJECT
public:
    explicit CircularImageButton(QWidget *parent = nullptr);

    void setImage(const QString &imgPath);

protected:
    virtual void paintEvent(QPaintEvent *) override;
    virtual void resizeEvent(QResizeEvent *)override;

private:
    QImage _image;
};

#endif // CIRCULARIMAGEBUTTON_H
