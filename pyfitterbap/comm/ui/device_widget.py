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
from .expanding_widget import ExpandingWidget
from pyfitterbap.comm.comm import Port0Events
import logging
import weakref

PORTS_COUNT = 32  # todo
DTYPES = ['str', 'json', 'bin',
          'f32', 'f64',
          'bool',
          'u8', 'u16', 'u32', 'u64',
          'i8', 'i16', 'i32', 'i64']
FLAGA = ['ro', 'hide', 'dev']
log = logging.getLogger(__name__)
SPACING = 5
MARGINS = (15, 0, 5, 5)

V_MIN = {
    'u8': 0,
    'u16': 0,
    'u32': 0,
    'u64': 0,
    'i8': -2**7,
    'i16': -2**15,
    'i32': -2**31,
    'i64': -2**64,
}


V_MAX = {
    'u8': 2**8 - 1,
    'u16': 2**16 - 1,
    'u32': 2**32 - 1,
    'u64': 2**64 - 1,
    'i8': 2**7 - 1,
    'i16': 2**15 - 1,
    'i32': 2**31 - 1,
    'i64': 2**64 - 1,
}


class DeviceWidget(QtWidgets.QWidget):

    def __init__(self, parent, device):
        super(DeviceWidget, self).__init__(parent)
        self._device = device
        self._layout = QtWidgets.QVBoxLayout(self)
        self._layout.setSpacing(SPACING)
        self._layout.setContentsMargins(5, 0, 5, 5)
        self._layout.setObjectName('device_layout')

        self._port_expander = ExpandingWidget(self, 'Ports')
        self._port_widget = PortWidget(self._port_expander)
        self._port_expander.setWidget(self._port_widget)
        self._layout.addWidget(self._port_expander)
        device.subscribe('h/c/port', self._port_widget.on_port_meta)

        self._status_expander = ExpandingWidget(self, 'Status')
        self._status_widget = StatusWidget(self._status_expander)
        self._status_expander.setWidget(self._status_widget)
        self._layout.addWidget(self._status_expander)

        # self._echo_widget = EchoWidget(self, device)
        # self._layout.addWidget(self._echo_widget)

        self._pubsub_expander = ExpandingWidget(self, 'PubSub')
        self._pubsub_widget = PubSubWidget(self._pubsub_expander, device)
        self._pubsub_expander.setWidget(self._pubsub_widget)
        self._layout.addWidget(self._pubsub_expander)

        self.setSizePolicy(QtWidgets.QSizePolicy.Preferred,
                           QtWidgets.QSizePolicy.Preferred)
        # print(f'device sizeHint={self.sizeHint()}, minimumSize={self.minimumSize()}')

    def minimumSize(self):
        return self.sizeHint()

    def status_update(self, status):
        self._status_widget.update(status)


class PortWidget(QtWidgets.QWidget):

    def __init__(self, parent=None):
        self._outstanding = 8
        self._tx_port_id = 0
        self._rx_port_id = 0
        self._send_fn = None
        super(PortWidget, self).__init__(parent)
        self.setObjectName('port_widget')
        self.setGeometry(QtCore.QRect(0, 0, 294, 401))
        self.setSizePolicy(QtWidgets.QSizePolicy.MinimumExpanding, QtWidgets.QSizePolicy.MinimumExpanding)

        self._layout = QtWidgets.QGridLayout(self)
        self._layout.setSpacing(SPACING)
        self._layout.setContentsMargins(*MARGINS)
        self._layout.setObjectName('port_widget_layout')

        port_id_hdr = QtWidgets.QLabel('Port ID', self)
        type_hdr = QtWidgets.QLabel('Type', self)
        name_hdr = QtWidgets.QLabel('Name', self)
        self._hdr = self._add_row(0, port_id_hdr, type_hdr, name_hdr)

        self._items = []
        for idx in range(PORTS_COUNT):
            port_id_label = QtWidgets.QLabel(self)
            port_id_label.setObjectName(f'port_id_label_{idx}')
            port_id_label.setText(f'{idx}')
            type_label = QtWidgets.QLabel(self)
            type_label.setObjectName(f'port_type_label_{idx}')
            type_label.setText(' ')
            name_label = QtWidgets.QLabel(self)
            name_label.setObjectName(f'port_name_label_{idx}')
            name_label.setText(' ')
            self._items.append(self._add_row(idx + 1, port_id_label, type_label, name_label))

    def _add_row(self, row, port_id_label, type_label, name_label):
        labels = port_id_label, type_label, name_label
        for idx, label in enumerate(labels):
            label.setSizePolicy(QtWidgets.QSizePolicy.Minimum, QtWidgets.QSizePolicy.Minimum)
            self._layout.addWidget(label, row, idx, 1, 1)
        return labels

    def on_port_meta(self, topic, value, **kwargs):
        if not topic.endswith('/meta'):
            return
        topic_parts = topic.split('/')
        port_id = int(topic_parts[-2])
        type_label, name_label = self._items[port_id][1:]
        if value is None:
            txt = '-'
            name = ''
        else:
            txt = value.get('type', 'unknown')
            name = value.get('name', '')
        log.info("%d, %s : %s => %s", port_id, topic, value, txt)
        type_label.setText(txt)
        name_label.setText(name)

    def eventFilter(self, obj, event):
        if obj == self:
            print(event)


class StatusWidget(QtWidgets.QWidget):

    def __init__(self, parent):
        super(StatusWidget, self).__init__(parent)
        self.setObjectName('status')
        self._layout = QtWidgets.QGridLayout(self)
        self._layout.setSpacing(SPACING)
        self._layout.setContentsMargins(*MARGINS)
        self._layout.setObjectName('status_layout')
        self._prev = None
        self._items = {}

    def clear(self):
        while not self._layout.isEmpty():
            item = self._layout.takeAt(0)
            widget = item.widget()
            if widget is not None:
                widget.setParent(None)
        self._items.clear()
        self._prev = None

    def _statistic_get(self, name):
        try:
            return self._items[name]
        except KeyError:
            row = self._layout.rowCount()
            n = QtWidgets.QLabel(self)
            n.setText(name)
            v = QtWidgets.QLabel(self)
            self._layout.addWidget(n, row, 0)
            self._layout.addWidget(v, row, 1)
            value = (n, v)
            self._items[name] = value
            return value

    def _statistic_update(self, name, value):
        _, v = self._statistic_get(name)
        v.setText(str(value))

    def update(self, status):
        for top_key, top_obj in status.items():
            if isinstance(top_obj, dict):
                for key, obj in top_obj.items():
                    self._statistic_update(f'{top_key}.{key}', obj)
            else:
                self._statistic_update(top_key, top_obj)
        if self._prev is not None:
            rx_bytes = status['rx']['msg_bytes'] - self._prev['rx']['msg_bytes']
            tx_bytes = status['tx']['msg_bytes'] - self._prev['tx']['msg_bytes']
            self._statistic_update('Δrx.msg_bytes', rx_bytes)
            self._statistic_update('Δtx.msg_bytes', tx_bytes)
        self._prev = status


class EchoWidget(QtWidgets.QWidget):

    def __init__(self, parent, device):
        self._device = weakref.ref(device)
        super(EchoWidget, self).__init__(parent)

        self.setObjectName('echo')
        self.setGeometry(QtCore.QRect(0, 0, 294, 401))

        self._layout = QtWidgets.QGridLayout(self)
        self._layout.setSpacing(SPACING)
        self._layout.setContentsMargins(*MARGINS)
        self._layout.setObjectName('echo_layout')

        self._outstanding_label = QtWidgets.QLabel(self)
        self._outstanding_label.setObjectName('echo_outstanding_label')
        self._outstanding_label.setText('Outstanding frames')
        self._layout.addWidget(self._outstanding_label, 0, 0, 1, 1)

        self._outstanding_combo_box = QtWidgets.QComboBox(self)
        self._outstanding_combo_box.setObjectName('echo_outstanding_combobox')
        for frame in [1, 2, 4, 8, 16, 32]:
            self._outstanding_combo_box.addItem(str(frame))
        self._outstanding_combo_box.setEditable(True)
        self._outstanding_combo_box.setCurrentIndex(3)
        self._outstanding_validator = QtGui.QIntValidator(0, 256, self)
        self._outstanding_combo_box.setValidator(self._outstanding_validator)
        self._layout.addWidget(self._outstanding_combo_box, 0, 1, 1, 1)

        self._button = QtWidgets.QPushButton(self)
        self._button.setCheckable(True)
        self._button.setText('Press to start')
        self._button.toggled.connect(self._on_button_toggled)
        self._layout.addWidget(self._button, 1, 0, 2, 0)

    def _on_echo_enabled(self, topic, value):
        self._button.setChecked(value)

    def _on_echo_outstanding(self, topic, value):
        self._outstanding_combo_box.setCurrentText(str(value))

    def _on_button_toggled(self, checked):
        log.info('echo button  %s', checked)
        txt = 'Press to stop' if checked else 'Press to start'
        self._button.setText(txt)
        device = self._device()
        if device is not None:
            value = 1 if checked else 0
            device.publish('h/c/port/0/echo/enable', value, retain=True, src_cbk=self._on_echo_enabled)


class Value(QtCore.QObject):

    def __init__(self, parent, topic, meta):
        QtCore.QObject.__init__(self, parent)
        self._parent = parent
        self._topic = topic
        self._meta = meta
        self._value = None
        self._options = None
        self.label = None
        self.editor = None
        # See include/fitterbap/pubsub.md for the latest format information
        dtype = meta.get('dtype')
        if dtype not in DTYPES:
            log.warning('topic %s: unsupported dtype %s', topic, dtype)
            return
        name = topic
        brief = meta.get('brief')
        detail = meta.get('detail')
        default = meta.get('default')
        options = meta.get('options')
        flags = meta.get('flags')
        flags = [] if flags is None else flags
        # print(f'META: topic={topic}, dtype={dtype}, brief={brief}, detail={detail}, default={default}, options={options}, flags={flags}')

        self.label = QtWidgets.QLabel(name, parent)
        tooltip = f'<html><body><p>{brief}</p>'
        if detail is not None:
            tooltip += f'<p>{detail}</p>'
        tooltip += f'</body></html>'
        self.label.setToolTip(tooltip)

        if 'ro' in flags:
            self.editor = QtWidgets.QLabel(parent)
        elif options is not None and len(options):
            self._options = []
            self.editor = QtWidgets.QComboBox(parent)
            for idx, opts in enumerate(options):
                v = opts[0]
                if len(opts) > 1:
                    e = str(opts[1])
                else:
                    e = str(v)
                self._options.append((v, e))
                self.editor.addItem(e)
            self.editor.currentIndexChanged.connect(self._on_combobox)
        elif dtype == 'str':
            self.editor = QtWidgets.QTextEdit(parent)
        elif dtype == 'json':
            self.editor = QtWidgets.QLabel("json: unsupported", parent)
        elif dtype == 'bin':
            self.editor = QtWidgets.QLabel("bin: unsupported", parent)
        #elif dtype in ['f32', 'f64']:
        #    pass
        elif dtype in ['u8', 'u16', 'u32', 'u64', 'i8', 'i16', 'i32', 'i64']:
            v_min, v_max, v_step = V_MIN[dtype], V_MAX[dtype], 1
            drange = self._meta.get('range')
            if drange is None:
                pass
            elif len(drange) == 2:
                v_min, v_max = drange
            elif len(drange) == 3:
                v_min, v_max, v_step = drange
            else:
                raise RuntimeError('topic %s: invalid range %s', self._topic, drange)
            self.editor = QtWidgets.QSpinBox(parent)
            if 'ro' not in flags:
                self.editor.setRange(v_min, v_max)
                self.editor.setSingleStep(v_step)
            self.editor.valueChanged.connect(self._on_spinbox)
        elif dtype == 'bool':
            self.editor = QtWidgets.QCheckBox(parent)
            self.editor.clicked.connect(self._on_clicked)
        else:
            self.editor = QtWidgets.QLabel("value", parent)
        self.editor.setToolTip(tooltip)
        if 'hide' in flags:
            self.setVisible(False)
        # if 'dev' in flags:
        #     self.setVisible(False)
        if default is not None:
            self.value = default

    def setVisible(self, visible):
        if self.label is not None:
            self.label.setEnabled(visible)
            self.label.setVisible(visible)
        if self.editor is not None:
            self.editor.setEnabled(visible)
            self.editor.setVisible(visible)

    @property
    def value(self):
        return self._value

    def _find_option_idx(self, x):
        for idx, k in enumerate(self._options):
            if x in k:
                return idx
        raise RuntimeError(f'topic {self._topic}: option {x} not found')

    @value.setter
    def value(self, x):
        dtype = self._meta.get('dtype')
        b = self.editor.blockSignals(True)
        try:
            if 'ro' in self._meta.get('flags', []):
                if dtype == 'u32' and self._meta.get('format') == 'version':
                    x = int(x)
                    major = (x >> 24) & 0xff
                    minor = (x >> 16) & 0xff
                    patch = x & 0xffff
                    x = f'{major}.{minor}.{patch}'
                self.editor.setText(str(x))
            elif self._options is not None:
                idx = self._find_option_idx(x)
                self.editor.setCurrentIndex(idx)
            elif dtype == 'str':
                self.editor.setText(str(x))
            elif dtype in ['json', 'bin']:
                pass
            elif dtype in ['f32', 'f64']:
                pass
            elif dtype in ['u8', 'u16', 'u32', 'u64', 'i8', 'i16', 'i32', 'i64']:
                self.editor.setValue(int(x))
            elif dtype == 'bool':
                self.editor.setChecked(bool(x))
            else:
                pass
        finally:
            self._value = x
            self.editor.blockSignals(b)

    def _publish(self, value):
        self._parent.publish(self._topic, value)

    def _on_combobox(self, idx):
        opts = self._meta.get('options')[idx]
        self._publish(opts[0])

    def _on_clicked(self, value):
        self._publish(bool(value))

    def _on_spinbox(self, value):
        self._publish(value)


class PubSubWidget(QtWidgets.QWidget):

    def __init__(self, parent, device):
        self._device = weakref.ref(device)
        super(PubSubWidget, self).__init__(parent)
        self.setObjectName('pubsub_widget')
        self.setGeometry(QtCore.QRect(0, 0, 294, 401))

        self._rows = 0
        self._layout = QtWidgets.QGridLayout(self)
        self._layout.setSpacing(SPACING)
        self._layout.setContentsMargins(*MARGINS)
        self._layout.setObjectName('pubsub_widget_layout')
        self._values: dict[str, Value] = {}
        device.subscribe('', self._on_update, forward=True)

    def clear(self):
        while not self._layout.isEmpty():
            item = self._layout.takeAt(0)
            widget = item.widget()
            if widget is not None:
                widget.setParent(None)
        self._values.clear()

    def publish(self, topic, value, retain=True):
        device = self._device()
        if device is None:
            return
        device.publish(topic, value, retain=retain, src_cbk=self._on_update)

    def get(self, topic):
        device = self._device()
        if device is None:
            return
        return device.get(topic)

    def _on_update(self, topic, value, retain=None):
        # print(f'{topic} : {value}')
        if topic == './conn/add':
            self._on_conn_add(value)
        elif topic == './conn/remove':
            self._on_conn_remove(value)
        elif topic.endswith('$') and topic != '$' and not topic.endswith('/$'):
            self._on_meta(topic[:-1], value)
        v = self._values.get(topic)
        if v is not None:
            v.value = value

    def _on_conn_add(self, topic_list):
        for topic in topic_list.split('\x1f'):
            if topic.endswith('/'):
                topic = topic[:-1]
            device = self._device()
            if device is not None:
                t = f'{topic}/$'
                log.info(f'request device metadata: {t}')
                device.publish(t, None)

    def _on_conn_remove(self, topic_list):
        for topic in topic_list.split('\x1f'):
            if topic.endswith('/'):
                topic = topic[:-1]
            topic = f'{topic}/'
            for value_str, value in self._values.items():
                if value_str.startswith(topic):
                    value.setVisible(False)

    def _on_meta(self, topic, meta):
        if meta is None:
            return
        if topic in self._values:
            self._values[topic].setVisible(True)
            return
        value = Value(self, topic, meta)
        if value.label is not None:
            self._values[topic] = value
            self._layout.addWidget(value.label, self._rows, 0)
            self._layout.addWidget(value.editor, self._rows, 1)
            self._rows += 1
            self._layout.update()
            try:
                value.value = self.get(topic)
            except Exception:
                pass  # use default value
