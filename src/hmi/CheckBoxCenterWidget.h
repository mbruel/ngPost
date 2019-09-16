#ifndef CHECKBOXCENTERWIDGET_H
#define CHECKBOXCENTERWIDGET_H

#include <QWidget>
class QCheckBox;

class CheckBoxCenterWidget : public QWidget
{
public:
    CheckBoxCenterWidget(QWidget *parent = nullptr, bool isChecked = false);
    ~CheckBoxCenterWidget() = default;

    bool isChecked() const;
    void setChecked(bool checked);

private:
    QCheckBox *_checkbox;
};

#endif // CHECKBOXCENTERWIDGET_H
