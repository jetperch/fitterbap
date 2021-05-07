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

import weakref
from collections.abc import Mapping


def _as_bool(x):
    if isinstance(x, str):
        x = x.lower()
    if x in [0, 'no', 'off', 'disable', 'disabled', 'false', 'inactive']:
        return False
    if x in [1, 'yes', 'on', 'enable', 'enabled', 'true', 'active']:
        return True
    raise ValueError(f'Invalid boolean value: {x}')


class _Topic:

    def __init__(self, parent, topic, value=None):
        """Hold a single Topic entry for :class:`PubSub`.

        :param parent: The parent :class:`Topic` instance.
        :param topic: The topic string.
        :param value: The optional initial value for the topic.
        """
        self._topic = topic
        self._value = value
        self.meta = None
        if parent is not None:
            parent = weakref.ref(parent)
        self.parent = parent
        self.children: Mapping[str, _Topic] = {}
        self._subscribers = []  # list of tuples of callback, forward

    def __str__(self):
        return f'Topic({self._topic}, value={self._value})'

    @property
    def name(self):
        return self._topic

    @property
    def value(self):
        return self._value

    @value.setter
    def value(self, x):
        self.publish(x)

    def publish(self, x, retain=None, src_cbk=None):
        if bool(retain):
            if self._value == x:  # deduplicate
                return
            self._value = x
        else:
            self._value = None
        topic = self
        while topic is not None:
            for subscriber, forward in topic._subscribers:
                if subscriber == src_cbk:
                    continue
                subscriber(self._topic, x, retain)
            topic = topic.parent
            if topic is not None:
                topic = topic()  # weakref to parent

    def forward(self, topic, value, retain=None, src_cbk=None, traverse_parent=False):
        for subscriber, forward in self._subscribers:
            if subscriber == src_cbk or not forward:
                continue
            subscriber(topic, value, retain)
        if traverse_parent and self.parent is not None:
            parent = self.parent()
            if parent is not None:
                parent.forward(topic, value, retain, src_cbk, traverse_parent)

    def _publish_new_subscriber(self, cbk):
        for child in self.children.values():
            child._publish_new_subscriber(cbk)
        if self._value is not None:
            cbk(self._topic, self._value)

    def subscribe(self, cbk, skip_retained=None, forward=None):
        if not callable(cbk):
            raise ValueError('subscribers must be callable')
        if cbk not in self._subscribers:
            self._subscribers.append((cbk, forward))
        if not bool(skip_retained):
            self._publish_new_subscriber(cbk)

    def unsubscribe(self, cbk):
        unsub = []
        for idx, (cbk_fn, _) in enumerate(self._subscribers):
            if cbk == cbk_fn:
                unsub.append(idx)
        for idx in reversed(unsub):
            self._subscribers.pop(idx)


class PubSub:
    """A trivial, local publish/subscribe implementation.

    :param topic_prefix: The topic prefix string owned by this
        implementation, which is used for distributed pubsub.
        Provide '' or None if you are using a single instance.

    This PubSub implementation features:
    * retained values
    * de-duplication for retained values
    * Support for cascaded distributed instances
    * Omit publisher on publish

    Topic names are hierarchical using '/' separators.
    Request topic metadata using '$' or 'my/path/$'.
    Metadata is returned as 'my/path/var$'
    Query the current value using '?' or 'my/path/?'.
    The query values are returned as 'my/path/var?'
    """
    def __init__(self, topic_prefix: str = None):
        self._topic_prefix = '' if topic_prefix is None else str(topic_prefix)
        self._root = _Topic(None, '')

    def _topic_find(self, topic, create=False):
        t = self._root
        while len(topic) and topic[-1] in '/$?':
            topic = topic[:-1]
        if len(topic):
            parts_so_far = []
            parts = topic.split('/')
            for part in parts:
                parts_so_far.append(part)
                child = t.children.get(part)
                if child is None:
                    if create:
                        subtopic = '/'.join(parts_so_far)
                        child = _Topic(t, subtopic)
                        t.children[part] = child
                    else:
                        return None
                t = child
        return t

    def _topic_find_existing_base(self, topic):
        t = self._root
        while len(topic) and topic[-1] in '/$?':
            topic = topic[:-1]
        if len(topic):
            parts_so_far = []
            parts = topic.split('/')
            for part in parts:
                parts_so_far.append(part)
                child = t.children.get(part)
                if child is None:
                    return t
                t = child
        return t

    def _meta_send_all(self, topic: _Topic, src_cbk=None):
        if topic is None:
            return
        if topic.meta is not None:
            topic.forward(topic.name + '$', topic.meta, retain=False, src_cbk=src_cbk, traverse_parent=True)
        for child in topic.children.values():
            self._meta_send_all(child, src_cbk)

    def publish(self, topic, value, retain=None, src_cbk=None):
        """Publish to a topic.

        :param topic: The topic name.
        :param value: The value for the topic.
        :param retain: True to retain (hold on to) value, False publish & discard.
            None (default) is False.
        :param src_cbk: The subscriber that will not be updated.
            None (default) updates all applicable subscribers.
        """
        if not len(topic):
            raise ValueError('Empty topic not allowed')
        if topic == '$':
            self._meta_send_all(self._topic_find(self._topic_prefix, create=True), src_cbk)
            self._root.forward(topic, None, retain=retain, src_cbk=src_cbk)
        elif topic.endswith('/$'):
            if topic.startswith(self._topic_prefix):  # for us
                self._meta_send_all(self._topic_find(topic, create=True), src_cbk)
            else:
                t = self._topic_find_existing_base(topic)
                t.forward(topic, None, retain=retain, src_cbk=src_cbk, traverse_parent=True)
        elif topic[-1] == '$':  # endswith
            t = self._topic_find(topic[:-1], create=True)
            if topic.startswith(self._topic_prefix):  # for use
                if value is None:  # request for us
                    if t.value is not None:
                        t.forward(topic, t.value, retain=True, src_cbk=src_cbk, traverse_parent=True)
                else:  # set for us
                    t.meta = value
                    t.forward(topic, value, retain=True, src_cbk=src_cbk, traverse_parent=True)
            else:  # get or set for another pubsub instance relay
                t.forward(topic, value, retain=True, src_cbk=src_cbk, traverse_parent=True)
        elif topic[-1] == '?':
            pass  # todo, topic query
        else:
            t = self._topic_find(topic, create=True)
            t.publish(value, retain, src_cbk)

    def meta(self, topic, meta):
        if topic[-1] != '$':
            topic = topic + '$'
        self.publish(topic, meta)

    def get(self, topic):
        """Get the retained value for the topic.

        :param topic: The topic name.
        :return: The topic retained value.
        :raise KeyError: If topic does not exist.
        """
        t = self._topic_find(topic, create=False)
        if t is None:
            raise KeyError(f'topic {topic} does not exist')
        return t.value

    def subscribe(self, topic, cbk, skip_retained=None, forward=None):
        """Subscribe to a topic and its children.

        :param topic: The topic name.
        :param cbk: The callable(topic, value, retain) called on value changes.
        :param skip_retained: Skip the update of all retained values
            that normally occurs when subscribing.
        :param forward: When true, forward $ and ? requests to this subscriber.
            Use True for distributed PubSub instances and user interfaces.
        """
        t = self._topic_find(topic, create=True)
        t.subscribe(cbk, skip_retained=skip_retained, forward=forward)

    def unsubscribe(self, topic, cbk):
        """Unsubscribe from a topic.

        :param topic: The topic name.
        :param cbk: The callable provided to subscribe.
        """
        t = self._topic_find(topic, create=True)
        t.unsubscribe(cbk)

    def create(self, topic, meta=None, subscriber_cbk=None, skip_retained=None, forward=None):
        t = self._topic_find(topic, create=False)
        if t is not None:
            raise ValueError(f'Topic {topic} already exists')
        t = self._topic_find(topic, create=True)
        t.meta = meta
        if meta is not None and 'default' in meta:
            retain = _as_bool(meta.get('retain', 0))
            x = meta['default']
            t.publish(x, retain, subscriber_cbk)
        if subscriber_cbk is not None:
            t.subscribe(subscriber_cbk, skip_retained=skip_retained, forward=forward)
