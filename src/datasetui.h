// Copyright (C) 2012, Chris J. Foster and the other authors and contributors
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name of the software's owners nor the names of its
//   contributors may be used to endorse or promote products derived from this
//   software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// (This is the BSD 3-clause license)

#include <QListView>
#include <QKeyEvent>
#include <QWheelEvent>

#include <iostream>

/// User interface for overview of loaded data sets
///
/// The interface contains mechanisms for showing which data sets are currently
/// loaded, for manipulating the current selection, etc.
class DataSetUI : public QWidget
{
    Q_OBJECT
    public:
        DataSetUI(QWidget* parent = 0);

        /// Return underlying view which displays loaded data set list
        QAbstractItemView* view();

    private slots:
        // Slots to manipulate model selection
        void selectAll();
        void selectNone();
        void selectionInvert();

    private:
        QListView* m_listView;
};


//------------------------------------------------------------------------------
/// List view for data sets with additional mouse and keyboard controls:
///
/// * Pressing delete removes the currently selected elements
/// * Mouse wheel scrolls the current selection rather than the scroll area
class DataSetListView : public QListView
{
    Q_OBJECT
    public:
        DataSetListView(QWidget* parent = 0)
            : QListView(parent)
        { }

        virtual void keyPressEvent(QKeyEvent* event);
        virtual void wheelEvent(QWheelEvent* event);
};

