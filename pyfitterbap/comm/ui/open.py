# Copyright 2020-2021 Jetperch LLC
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

import serial.tools.list_ports
from PySide6 import QtCore, QtGui, QtWidgets


BAUDRATES = [9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600, 1000000, 2000000, 3000000, 5000000]
BAUDRATE_DEFAULT_IDX = 10


class OpenDialog(QtWidgets.QDialog):

    def __init__(self, parent=None):
        super(OpenDialog, self).__init__(parent)
        self.setObjectName('OpenDialog')
        self.setWindowTitle('Open')
        self.resize(259, 140)
        self._main_vertical_layout = QtWidgets.QVBoxLayout(self)
        self._main_vertical_layout.setObjectName('open_main_layout')

        self._layout = QtWidgets.QGridLayout()
        self._layout.setObjectName('open_inner_layout')

        self._device_label = QtWidgets.QLabel(self)
        self._device_label.setObjectName('open_device_label')
        self._device_label.setText("Device")
        self._layout.addWidget(self._device_label, 0, 0, 1, 1)

        self._device_combo_box = QtWidgets.QComboBox(self)
        self._device_combo_box.setObjectName('open_device_combo_box')
        for device in self._devices():
            self._device_combo_box.addItem(device)
        self._layout.addWidget(self._device_combo_box, 0, 1, 1, 1)

        self._baud_label = QtWidgets.QLabel(self)
        self._baud_label.setObjectName('open_baud_label')
        self._baud_label.setText('Baud')
        self._layout.addWidget(self._baud_label, 1, 0, 1, 1)

        self._baud_combo_box = QtWidgets.QComboBox(self)
        self._baud_combo_box.setObjectName("open_device_combo_box")
        for baud in BAUDRATES:
            self._baud_combo_box.addItem(str(baud))
        self._baud_combo_box.setEditable(True)
        self._baud_combo_box.setCurrentIndex(BAUDRATE_DEFAULT_IDX)
        self._baud_validator = QtGui.QIntValidator(0, 10000000, self)
        self._baud_combo_box.setValidator(self._baud_validator)
        self._layout.addWidget(self._baud_combo_box, 1, 1, 1, 1)

        self._main_vertical_layout.addLayout(self._layout)

        self.buttonBox = QtWidgets.QDialogButtonBox(self)
        self.buttonBox.setOrientation(QtCore.Qt.Horizontal)
        self.buttonBox.setStandardButtons(QtWidgets.QDialogButtonBox.Cancel | QtWidgets.QDialogButtonBox.Ok)
        self.buttonBox.setObjectName("buttonBox")
        self._main_vertical_layout.addWidget(self.buttonBox)

        QtCore.QObject.connect(self.buttonBox, QtCore.SIGNAL("accepted()"), self.accept)
        QtCore.QObject.connect(self.buttonBox, QtCore.SIGNAL("rejected()"), self.reject)

    def _devices(self):
        comports = serial.tools.list_ports.comports()
        devices = [c.device for c in comports]
        if len(devices) and devices[0].startswith('COM'):
            ports = sorted([int(d[3:]) for d in devices])
            devices = [f'COM{port}' for port in ports]
        else:
            devices = sorted(devices)
        return devices

    def exec_(self):
        if QtWidgets.QDialog.exec_(self) == 1:
            return {
                'device': self._device_combo_box.currentText(),
                'baud': int(self._baud_combo_box.currentText()),
            }
        else:
            return None