//========================================================================
//
// Copyright (C) 2020 Matthieu Bruel <Matthieu.Bruel@gmail.com>
//
// This file is a part of ngPost : https://github.com/mbruel/ngPost
//
// ngPost is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; version 3.0 of the License.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// You should have received a copy of the GNU Lesser General Public
// License along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301,
// USA.
//
//========================================================================

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
