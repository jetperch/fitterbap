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


from PySide2 import QtCore, QtGui, QtWidgets
import logging


class VerticalScrollArea(QtWidgets.QScrollArea):

    def __init__(self, parent=None):
        super(VerticalScrollArea, self).__init__(parent)
        self.setWidgetResizable(True)
        self.setHorizontalScrollBarPolicy(QtGui.Qt.ScrollBarAlwaysOff)
        self.setVerticalScrollBarPolicy(QtGui.Qt.ScrollBarAsNeeded)
        self.setSizePolicy(QtWidgets.QSizePolicy.Preferred,
                           QtWidgets.QSizePolicy.Preferred)
        self._log = logging.getLogger(__name__)

    def setWidget(self, widget):
        widget.installEventFilter(self)
        return super(VerticalScrollArea, self).setWidget(widget)

    def eventFilter(self, obj, event):
        widget = self.widget()
        if obj == widget and event.type() == QtCore.QEvent.Resize:
            width = widget.minimumSizeHint().width() + self.verticalScrollBar().width()
            self.setMinimumWidth(width)
            self._log.debug('scroll: sizeHint=%s, minimumSizeHint=%s, minimumSize=%s',
                            self.sizeHint(), self.minimumSizeHint(), self.minimumSize())
        return False
