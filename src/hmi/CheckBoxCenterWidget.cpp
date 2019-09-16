#include "CheckBoxCenterWidget.h"
#include <QCheckBox>
#include <QHBoxLayout>
#include <QSpacerItem>
CheckBoxCenterWidget::CheckBoxCenterWidget(QWidget *parent, bool isChecked) :
    QWidget (parent),
    _checkbox(new QCheckBox(this))
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Fixed));
    layout->addWidget(_checkbox);
    layout->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Fixed));
    layout->setContentsMargins(0, 0, 0, 0);

    _checkbox->setChecked(isChecked);
}

bool CheckBoxCenterWidget::isChecked() const { return _checkbox->isChecked(); }
void CheckBoxCenterWidget::setChecked(bool checked) { _checkbox->setChecked(checked); }
