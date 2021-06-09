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


import pyqtgraph as pg
from PySide6 import QtCore, QtGui, QtWidgets
import logging
import numpy as np


class WaveformWidget(QtWidgets.QWidget):

    def __init__(self, parent=None, pubsub=None):
        super(WaveformWidget, self).__init__(parent)
        self._topics = {}
        self._log = logging.getLogger(__name__)

        self._layout = QtWidgets.QVBoxLayout(self)
        self._layout.setSpacing(0)
        self._layout.setContentsMargins(0, 0, 0, 0)

        self._source_widget = QtWidgets.QWidget(self)
        self._source_layout = QtWidgets.QHBoxLayout(self._source_widget)
        self._source_label = QtWidgets.QLabel('Source: ', parent=self._source_widget)
        self._source_layout.addWidget(self._source_label)
        self._source_combobox = QtWidgets.QComboBox(self._source_widget)
        self._source_layout.addWidget(self._source_combobox)
        self._layout.addWidget(self._source_widget)

        self.win = pg.GraphicsLayoutWidget(parent=self, show=True, title="Waveform")
        self.win.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Expanding)
        # self.win.sceneObj.sigMouseClicked.connect(self._on_mouse_clicked_event)
        # self.win.sceneObj.sigMouseMoved.connect(self._on_mouse_moved_event)
        self._layout.addWidget(self.win)

        self._plot: pg.PlotItem = self.win.addPlot(row=0, col=0)
        self._plot.showGrid(True, True, 128)
        self._curve = self._plot.plot(pen='y')

        self._data = np.empty(32000, dtype=np.float32)
        self._data[:] = np.nan
        self._buffer = np.empty(32000, dtype=np.float32)
        self._index = 0
        self._data_pending = False

        if pubsub is not None:
            pubsub.subscribe('', self._on_update, skip_retained=True)

        self._timer = QtCore.QTimer()
        self._timer.timeout.connect(self._redraw)
        self._timer.start(50)

    def _redraw(self):
        if self._data_pending:
            self._data_pending = False
            self._curve.setData(self._data)

    def _on_update(self, topic, value, retain):
        if topic.endswith('/din'):
            v = self._topics.get(topic, 0)
            if v == 0:
                self._log.info('Added waveform source: %s', topic)
                self._source_combobox.addItem(topic)
            self._topics[topic] = v + 1
            if topic == self._source_combobox.currentText():
                sample_id_bytes = value[:4]
                d = self._buffer
                d_next = np.frombuffer(value[4:], dtype=np.float32)
                d_len = len(d_next)
                d_end = self._index + d_len
                if d_end > len(d):
                    d_len = len(d) - self._index
                    d[self._index:] = d_next[:d_len]
                    self._data, self._buffer = self._buffer, self._data
                    self._data_pending = True
                    self._index = 0
                else:
                    d[self._index:(self._index + d_len)] = d_next
                    self._index += d_len
