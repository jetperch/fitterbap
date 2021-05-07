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


from pyfitterbap.comm.comm import Comm
from .resync import Resync
from .open import OpenDialog
from .device_widget import DeviceWidget
from .waveform import WaveformWidget
from .vertical_scroll_area import VerticalScrollArea
from .expanding_widget import ExpandingWidget
from PySide2 import QtCore, QtGui, QtWidgets
from pyfitterbap import __version__, __url__
from pyfitterbap.pubsub import PubSub
import ctypes
import logging
import sys


log = logging.getLogger(__name__)
STATUS_BAR_TIMEOUT_DEFAULT = 2500
PORT_COUNT = 32


ABOUT = """\
<html>
<head>
</head>
<body>
FBP Comm UI<br/> 
Version {version}<br/>
<a href="{url}">{url}</a>

<pre>
Copyright 2018-2021 Jetperch LLC

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
</pre>
</body>
</html>
"""


def menu_setup(d, parent=None):
    k = {}
    for name, value in d.items():
        name_safe = name.replace('&', '')
        if isinstance(value, dict):
            wroot = QtWidgets.QMenu(parent)
            wroot.setTitle(name)
            parent.addAction(wroot.menuAction())
            w = menu_setup(value, wroot)
            w['__root__'] = wroot
        else:
            w = QtWidgets.QAction(parent)
            w.setText(name)
            if callable(value):
                w.triggered.connect(value)
            parent.addAction(w)
        k[name_safe] = w
    return k


class Device(QtCore.QObject):

    def __init__(self, parent, pubsub, dev, baudrate=None):
        super(Device, self).__init__(parent)
        self._parent = parent
        self._dev = dev
        self._topic_prefix = dev + '/'
        self._pubsub = pubsub
        self._resync = Resync(self)
        self._on_device_publish = self._resync.wrap(self._subscribe_device)
        self.comm = None
        self.widget = None
        self._device_widget = None
        self.baudrate = baudrate

        if pubsub is not None:
            pubsub.subscribe(dev, self._subscribe_parent, forward=True)

    def __str__(self):
        return f'Device({self._dev})'

    @property
    def topic_prefix(self):
        return self._topic_prefix

    def _subscribe_parent(self, topic: str, value, retain=None, src_cbk=None):
        # forward from application pubsub to device's C pubsub
        log.info('ui → device: %s = %s', topic, value)
        if topic.startswith(self._topic_prefix):
            topic = topic[len(self._topic_prefix):]
            if self.comm is not None:
                self.comm.publish(topic, value, retain=retain, src_cbk=self._on_device_publish)

    def _subscribe_device(self, topic: str, value, retain=None, src_cbk=None):
        # forward from device's C pubsub to application pubsub
        topic = self._topic_prefix + topic
        if not topic.endswith('/din'):
            log.info("device → ui: %s = %s", topic, value)
        self._pubsub.publish(topic, value, retain=retain, src_cbk=self._subscribe_parent)

    def publish(self, topic: str, value, retain=None, src_cbk=None):
        topic = self._topic_prefix + topic
        self._pubsub.publish(topic, value, retain=retain, src_cbk=src_cbk)

    def get(self, topic: str):
        topic = self._topic_prefix + topic
        return self._pubsub.get(topic)

    def subscribe(self, topic, cbk, skip_retained=None, forward=None):
        def src_cbk_fn(topic: str, value, retain=None, src_cbk=None):
            if topic.startswith(self._topic_prefix):
                topic = topic[len(self._topic_prefix):]
                cbk(topic, value, retain=retain)
        self._pubsub.subscribe(self._topic_prefix + topic, src_cbk_fn, skip_retained=skip_retained, forward=forward)
        return src_cbk_fn

    def unsubscribe(self, topic, cbk):
        self._pubsub.unsubscribe(self._topic_prefix + topic, cbk)

    def open(self):
        self.close()
        try:
            self.comm = Comm(self._dev, self._on_device_publish, baudrate=self.baudrate)
            self.widget = ExpandingWidget(self._parent, 'Device')
            self._device_widget = DeviceWidget(self.widget, self)
            self.widget.setWidget(self._device_widget)
        except Exception:
            log.exception('Could not open device')

    def close(self):
        if self.comm is not None:
            self.comm.close()
            self.comm = None
        if self._device_widget is not None:
            self._device_widget.close()
            self._device_widget = None
        self.widget = None

    def status_refresh(self):
        if self.comm is not None and self._device_widget is not None:
            status = self.comm.status()
            self._device_widget.status_update(status)


class DevicesWidget(QtWidgets.QWidget):

    def __init__(self, parent=None):
        super(DevicesWidget, self).__init__(parent)
        self.setSizePolicy(QtWidgets.QSizePolicy.Preferred,
                           QtWidgets.QSizePolicy.Expanding)
        self.setObjectName('devices_widget')
        self._index = 0
        self._layout = QtWidgets.QVBoxLayout(self)
        self._layout.setContentsMargins(5, 5, 5, 5)
        self._layout.setObjectName('devices_layout')

        self._spacer = QtWidgets.QSpacerItem(0, 0,
                                             QtWidgets.QSizePolicy.Minimum,
                                             QtWidgets.QSizePolicy.Expanding)
        self._layout.addItem(self._spacer)

    def add_device(self, device):
        self._layout.insertWidget(self._index, device.widget)
        self._index += 1
        self._layout.update()

    def remove_device(self, device):
        self._layout.removeWidget(device.widget)
        self._index -= 1


class MainWindow(QtWidgets.QMainWindow):

    def __init__(self, app):
        self._devices = {}
        self._pubsub = PubSub('ui')
        super(MainWindow, self).__init__()
        self.setObjectName('MainWindow')
        self.setWindowTitle('FBP Comm')
        self.resize(640, 480)

        self._central_widget = QtWidgets.QWidget(self)
        self._central_widget.setObjectName('central')
        self._central_widget.setSizePolicy(QtWidgets.QSizePolicy.Expanding,
                                           QtWidgets.QSizePolicy.Expanding)
        self.setCentralWidget(self._central_widget)

        self._central_layout = QtWidgets.QHBoxLayout(self._central_widget)
        self._central_layout.setSpacing(6)
        self._central_layout.setContentsMargins(11, 11, 11, 11)
        self._central_layout.setObjectName('central_layout')

        self._devices_scroll = VerticalScrollArea(self._central_widget)
        self._central_layout.addWidget(self._devices_scroll)

        self._devices_widget = DevicesWidget(self._devices_scroll)
        self._devices_scroll.setWidget(self._devices_widget)

        #self._horizontalSpacer = QtWidgets.QSpacerItem(40, 20,
        #                                               QtWidgets.QSizePolicy.Expanding,
        #                                               QtWidgets.QSizePolicy.Minimum)
        #self._central_layout.addItem(self._horizontalSpacer)

        self._waveform = WaveformWidget(self._central_widget, self._pubsub)
        size_policy = QtWidgets.QSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Expanding)
        size_policy.setHorizontalStretch(1)
        self._waveform.setSizePolicy(size_policy)
        self._central_layout.addWidget(self._waveform)

        # Status bar
        self._status_bar = QtWidgets.QStatusBar(self)
        self.setStatusBar(self._status_bar)
        self._status_indicator = QtWidgets.QLabel(self._status_bar)
        self._status_indicator.setObjectName('status_indicator')
        self._status_bar.addPermanentWidget(self._status_indicator)

        # status update timer
        self._status_update_timer = QtCore.QTimer(self)
        self._status_update_timer.setInterval(1000)  # milliseconds
        self._status_update_timer.timeout.connect(self._on_status_update_timer)
        self._status_update_timer.start()

        self._menu_bar = QtWidgets.QMenuBar(self)
        self._menu_items = menu_setup(
            {
                '&File': {
                    '&Open': self._on_file_open,
                    '&Close': self._on_file_close,
                    'E&xit': self._on_file_exit,
                },
                '&Help': {
                    '&Credits': self._on_help_credits,
                    '&About': self._on_help_about,
                }
            },
            self._menu_bar)
        self.setMenuBar(self._menu_bar)
        self.show()
        self._on_file_open()

    def _on_publish(self, topic, value, retain=None, src_cbk=None):
        log.info(f'publish {topic} => {value}')
        self._pubsub.publish(topic, value, retain=retain, src_cbk=src_cbk)

    def _device_open(self, dev, baudrate):
        self._device_close(dev)
        log.info('_device_open')
        try:
            device = Device(self, self._pubsub, dev, baudrate)
            device.open()
            self._devices_widget.add_device(device)
            self._devices[dev] = device
        except Exception:
            log.exception('Could not open device')

    def _device_close(self, dev):
        log.info('_device_close')
        if dev in self._devices:
            device = self._devices.pop(dev)
            self._devices_widget.remove_device(device)
            try:
                device.close()
            except Exception:
                log.exception('Could not close device')

    def _device_close_all(self):
        devs = list(self._devices.keys())
        for dev in devs:
            self._device_close(dev)

    def _on_file_open(self):
        log.info('_on_file_open')
        params = OpenDialog(self).exec_()
        if params is not None:
            log.info(f'open {params}')
            self._device_open(params['device'], params['baud'])

    def _on_status_update_timer(self):
        for device in self._devices.values():
            device.status_refresh()

    def closeEvent(self, event):
        log.info('closeEvent()')
        self._device_close_all()
        return super(MainWindow, self).closeEvent(event)

    def _on_file_close(self):
        log.info('_on_file_close')
        self._device_close_all()

    def _on_file_exit(self):
        log.info('_on_file_exit')
        self._device_close_all()
        self.close()

    def _on_help_credits(self):
        log.info('_on_help_credits')

    def _on_help_about(self):
        log.info('_on_help_about')
        txt = ABOUT.format(version=__version__,
                           url=__url__)
        QtWidgets.QMessageBox.about(self, 'Delta Link UI', txt)

    @QtCore.Slot(str)
    def status_msg(self, msg, timeout=None, level=None):
        """Display a status message.

        :param msg: The message to display.
        :param timeout: The optional timeout in milliseconds.  0
            does not time out.
        :param level: The logging level for the message.  None (default)
            is equivalent to log.INFO.
        """
        level = logging.INFO if level is None else level
        log.log(level, msg)
        timeout = STATUS_BAR_TIMEOUT_DEFAULT if timeout is None else int(timeout)
        self._status_bar.showMessage(msg, timeout)

    @QtCore.Slot(str)
    def error_msg(self, msg, timeout=None):
        self.status_msg(msg, timeout, level=log.ERROR)


def _high_dpi_enable():
    # http://doc.qt.io/qt-5/highdpi.html
    # https://vicrucann.github.io/tutorials/osg-qt-high-dpi/
    if sys.platform.startswith('win'):
        ctypes.windll.user32.SetProcessDPIAware()
    QtWidgets.QApplication.setAttribute(QtCore.Qt.AA_EnableHighDpiScaling)


def run():
    _high_dpi_enable()
    app = QtWidgets.QApplication(sys.argv)
    ui = MainWindow(app)
    rc = app.exec_()
    del ui
    del app
    return rc
