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

#ifndef ABOUTNGPOST_H
#define ABOUTNGPOST_H

#include <QDialog>
class NgPost;

namespace Ui {
class AboutNgPost;
}

class AboutNgPost : public QDialog
{
    Q_OBJECT
private:
    static constexpr const char *sTextColor = "#25aae1"; //!< same blue that the logo

public:
    explicit AboutNgPost(NgPost *ngPost, QWidget *parent = nullptr);
    ~AboutNgPost();

protected:
    void keyPressEvent(QKeyEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;

private:
    Ui::AboutNgPost *ui;
};

#endif // ABOUTNGPOST_H
