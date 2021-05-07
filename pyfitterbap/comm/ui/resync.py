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

import logging
from PySide2 import QtCore, QtGui, QtWidgets
from queue import Queue, Empty


log = logging.getLogger(__name__)


class _ResyncEvent(QtCore.QEvent):
    """An event containing a request for a resync function call."""
    EVENT_TYPE = QtCore.QEvent.Type(QtCore.QEvent.registerEventType())

    def __init__(self):
        QtCore.QEvent.__init__(self, self.EVENT_TYPE)

    def __str__(self):
        return '_ResyncEvent()'

    def __len__(self):
        return 0


class Resync(QtCore.QObject):
    """Resynchronize to the Qt event thread.

    :param parent: The parent QObject.

    Qt does not easily support resynchronization from non-Qt threads.  See
    this [bug report](https://github.com/jetperch/pyjoulescope_ui/issues/69).
    This class implements a resynchronization service that allows a non-Qt
    thread to invoke a method on the main Qt event thread.

    See [this page](https://doc.qt.io/qt-5/threads-qobject.html) for a
    Qt threading overview.
    """
    def __init__(self, parent=None):
        super(Resync, self).__init__(parent)
        self._resync_handlers = {}
        self._resync_queue = Queue()

    def event(self, event: QtCore.QEvent):
        if event.type() == _ResyncEvent.EVENT_TYPE:
            # process our resync calls.
            event.accept()
            try:
                fn, args, kwargs, ev = self._resync_queue.get(timeout=0.0)
                if id(event) != id(ev):
                    log.warning('event mismatch')
                fn(*args, **kwargs)
            except Empty:
                log.warning('event signaled but not available')
            except Exception:
                log.exception('resync queue failed')
            return True
        else:
            return super(Resync, self).event(event)

    def _resync_handle(self, name, args, kwargs):
        # safely resynchronize to the main Qt event thread
        event = _ResyncEvent()
        self._resync_queue.put((name, args, kwargs, event))
        QtCore.QCoreApplication.postEvent(self, event)

    def clear(self, target=None):
        """Clear handlers.

        :param target: The target provided to :meth:`factory`.  If None,
            then clear all targets.
        """
        if target is not None:
            key = id(target)
            self._resync_handlers.pop(key)
        else:
            self._resync_handlers.clear()

    def wrap(self, target):
        """Get a function that will call target from the main Qt event loop.

        :param target: The target callable(args, kwargs).  This instance will
            hold a reference to target.
        :return: The resynchronization function that invokes target from the
            main thread's Qt event loop.  Calls to this function
            complete immediately, but the actual processing is deferred.
        """
        def fn(*args, **kwargs):
            return self._resync_handle(target, args, kwargs)
        key = id(target)
        if key not in self._resync_handlers:
            self._resync_handlers[key] = fn
        return self._resync_handlers[key]
