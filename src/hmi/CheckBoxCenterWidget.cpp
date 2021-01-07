//========================================================================
//
// Copyright (C) 2020 Matthieu Bruel <Matthieu.Bruel@gmail.com>
// This file is a part of ngPost : https://github.com/mbruel/ngPost
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 3..
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>
//
//========================================================================

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
