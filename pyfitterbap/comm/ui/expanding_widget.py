# Copyright 2021 Jetperch LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


from PySide6 import QtCore, QtGui, QtWidgets


class ExpandingWidget(QtWidgets.QWidget):

    def __init__(self, parent=None, title=None, animation_duration_ms=None):
        super(ExpandingWidget, self).__init__(parent=parent)
        self._widget: QtWidgets.QWidget = None
        self._animation_duration_ms = 250 if animation_duration_ms is None else int(animation_duration_ms)
        self._animation = None
        self._main_layout = QtWidgets.QVBoxLayout(self)
        self._main_layout.setContentsMargins(0, 0, 0, 0)

        self._toggle_button = QtWidgets.QToolButton()
        self._toggle_button.setStyleSheet("QToolButton { border: none; }")
        self._toggle_button.setToolButtonStyle(QtCore.Qt.ToolButtonTextBesideIcon)
        self._toggle_button.setArrowType(QtCore.Qt.RightArrow)
        self._toggle_button.setCheckable(True)
        self._toggle_button.setChecked(False)
        self.title = title
        self._main_layout.addWidget(self._toggle_button)

        self._toggle_button.clicked.connect(self._start_animation)

    @property
    def title(self):
        return self._toggle_button.text()

    @title.setter
    def title(self, title):
        title = '' if title is None else str(title)
        self._toggle_button.setText(title)

    def _start_animation(self, checked):
        if self._widget is None:
            return
        if checked:
            self._widget.show()
        else:
            self._widget.hide()
        arrow_type = QtCore.Qt.DownArrow if checked else QtCore.Qt.RightArrow
        self._toggle_button.setArrowType(arrow_type)

    def _show(self):
        if self._widget is None:
            return
        # See https://www.qtcentre.org/threads/60494-Animate-an-hidden-widget
        if self._widget.isHidden():
            self._widget.show()   # required to get size
            pos = self._widget.pos()
            size = self._widget.size()
            fo = QtCore.QRect(pos.x(), pos.y() + size.height(), size.width(), size.height())
            fi = QtCore.QRect(pos, size)
        else:
            fo = self._widget.geometry()
            fi = QtCore.QRect(fo.x(), fo.y() - fo.height(), fo.width(), fo.height())

        animation = QtCore.QPropertyAnimation(self._widget, b'geometry')
        animation.setDuration(self._animation_duration_ms)
        animation.setEasingCurve(QtCore.QEasingCurve.Linear)
        animation.setStartValue(fo)
        animation.setEndValue(fi)
        animation.start()
        self._animation = animation

    def setWidget(self, widget):
        self._widget = widget
        widget.setParent(self)
        self._main_layout.addWidget(self._widget)
        self._widget.setHidden(not self._toggle_button.isChecked())


class VerticalScrollArea(QtWidgets.QScrollArea):

    def __init__(self, parent=None):
        super(VerticalScrollArea, self).__init__(parent)
        self.setWidgetResizable(True)
        self.setHorizontalScrollBarPolicy(QtGui.Qt.ScrollBarAlwaysOff)
        self.setVerticalScrollBarPolicy(QtGui.Qt.ScrollBarAsNeeded)
        self.setSizePolicy(QtWidgets.QSizePolicy.Preferred,
                           QtWidgets.QSizePolicy.Preferred)

    def setWidget(self, widget):
        widget.installEventFilter(self)
        return super(VerticalScrollArea, self).setWidget(widget)

    def eventFilter(self, obj, event):
        widget = self.widget()
        if obj == widget and event.type() == QtCore.QEvent.Resize:
            width = widget.minimumSizeHint().width() + self.verticalScrollBar().width()
            self.setMinimumWidth(width)
        return False


class _MainWindow(QtWidgets.QMainWindow):

    def __init__(self):
        super(_MainWindow, self).__init__()
        self.setWindowTitle('ExpandingWidget Demo')
        self._scroll_widget = VerticalScrollArea(self)
        self._scroll_widget.setObjectName('central_scroll')
        self.setCentralWidget(self._scroll_widget)

        self._widget = QtWidgets.QWidget(self._scroll_widget)
        self._widget.setObjectName('central_widget')
        self._scroll_widget.setWidget(self._widget)

        self._layout = QtWidgets.QVBoxLayout(self._widget)
        self._layout.setSpacing(6)
        self._layout.setContentsMargins(11, 11, 11, 11)
        self._layout.setObjectName('central_layout')

        self._widgets = []
        for widget_id in range(5):
            widget = ExpandingWidget(self._widget, title=f'Widget {widget_id}')
            inner_widget = QtWidgets.QWidget(widget)
            layout = QtWidgets.QVBoxLayout(inner_widget)
            labels = []
            for label_id in range(10):
                label = QtWidgets.QLabel(f'Widget {widget_id}, Label {label_id}', inner_widget)
                layout.addWidget(label)
                labels.append(label)
            widget.setWidget(inner_widget)
            self._widgets.append([widget, inner_widget, layout, labels])
            self._layout.addWidget(widget)

        self._spacer = QtWidgets.QSpacerItem(0, 0,
                                             QtWidgets.QSizePolicy.Minimum,
                                             QtWidgets.QSizePolicy.Expanding)
        self._layout.addItem(self._spacer)
        self.show()


def _run():
    import sys
    import ctypes
    # http://doc.qt.io/qt-5/highdpi.html
    # https://vicrucann.github.io/tutorials/osg-qt-high-dpi/
    if sys.platform.startswith('win'):
        ctypes.windll.user32.SetProcessDPIAware()
    QtWidgets.QApplication.setAttribute(QtCore.Qt.AA_EnableHighDpiScaling)
    app = QtWidgets.QApplication(sys.argv)
    ui = _MainWindow()
    rc = app.exec_()
    del ui
    del app
    return rc


if __name__ == '__main__':
    _run()
