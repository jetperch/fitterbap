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
from datetime import datetime

_LOG_LEVEL_MAP = {
    -1: ('O', 'off'),
    0:  ('E', 'emergency'),
    1:  ('A', 'alert'),
    2:  ('C', 'critical'),
    3:  ('E', 'error'),
    4:  ('W', 'warning'),
    5:  ('N', 'notice'),
    6:  ('I', 'info'),
    7:  ('D1', 'debug1'),
    8:  ('D2', 'debug2'),
    9:  ('D3', 'debug3'),
    10: ('A', 'all'),
}

_COLUMNS = ['Timestamp', 'Level', 'Device', 'Prefix', 'Thread', 'Filename', 'Line', 'Message']


class LogWidget(QtWidgets.QWidget):

    def __init__(self, parent=None):
        """Display Fitterbap device log messages.

        :param parent: The parent widget.
        """
        QtWidgets.QWidget.__init__(self, parent)
        self._model = QtGui.QStandardItemModel(0, 8, self)
        self._model.setHorizontalHeaderLabels(_COLUMNS)
        self._messages = []
        self._layout = QtWidgets.QVBoxLayout(self)
        self._layout.setSpacing(5)
        self._layout.setContentsMargins(5, 5, 5, 5)
        self._layout.setObjectName('log_layout')

        self._ctrl_bar = QtWidgets.QWidget(self)
        self._ctrl_bar.setObjectName('log_ctrl_bar')
        self._layout.addWidget(self._ctrl_bar)
        self._ctrl_layout = QtWidgets.QHBoxLayout(self._ctrl_bar)

        self._table = QtWidgets.QTableView(self)
        self._table.setObjectName('log_table')
        self._table.setModel(self._model)
        self._layout.addWidget(self._table)

        self.setSizePolicy(QtWidgets.QSizePolicy.Preferred,
                           QtWidgets.QSizePolicy.Preferred)

    @QtCore.Slot(object)
    def message(self, msg):
        """Receive a new log message.

        :param msg: The log message dict which contains the fields:
            * timestamp: The float timestamp in UTC seconds from the POSIX epoch.
            * level: The log level integer.
            * device: The device string that generated this message.
            * origin_prefix: The prefix character for the subsystem that generated this message.
            * origin_thread: The thread integer for the subsystem that generated this message.
            * filename: The source code filename string.
            * line: The source code line number.
            * message: The emitted log message string.
        """
        utc = datetime.utcfromtimestamp(msg['timestamp'])
        utc_str = utc.isoformat()
        _, level_str = _LOG_LEVEL_MAP.get(msg['level'], _LOG_LEVEL_MAP[9])
        prefix = str(chr(msg['origin_prefix']))
        items = [
            QtGui.QStandardItem(utc_str),
            QtGui.QStandardItem(level_str),
            QtGui.QStandardItem(msg['device']),
            QtGui.QStandardItem(prefix),
            QtGui.QStandardItem(str(msg['origin_thread'])),
            QtGui.QStandardItem(msg['filename']),
            QtGui.QStandardItem(str(msg['line'])),
            QtGui.QStandardItem(msg['message']),
        ]
        msg['items'] = items
        self._messages.append(msg)
        self._model.appendRow(items)

    def on_publish(self, topic, value, retain):
        self.message(value)
