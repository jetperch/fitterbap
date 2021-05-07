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

import unittest
from pyfitterbap.pubsub import PubSub


class PubSubTest(unittest.TestCase):

    def setUp(self):
        self.p = PubSub(topic_prefix='hello')
        self.sub1 = []
        self.sub2 = []

    def sub1_fn(self, topic, value, retain=None):
        self.sub1.append((topic, value))

    def sub2_fn(self, topic, value, retain=None):
        self.sub2.append((topic, value))

    def test_sub_pub(self):
        self.p.subscribe('hello/world', self.sub1_fn)
        self.p.publish('hello/world', 'there')
        self.assertEqual([('hello/world', 'there')], self.sub1)

    def test_pub_sub_not_retained(self):
        self.p.publish('hello/world', 'there')
        self.p.subscribe('hello/world', self.sub1_fn)
        self.assertEqual([], self.sub1)

    def test_pub_sub_retained(self):  # retained value
        self.p.publish('hello/world', 'there', retain=True)
        self.p.subscribe('hello/world', self.sub1_fn)
        self.assertEqual([('hello/world', 'there')], self.sub1)

    def test_retained_dedup(self):
        self.p.publish('hello/world', 'there', retain=True)
        self.p.subscribe('hello/world', self.sub1_fn)
        self.p.publish('hello/world', 'there', retain=True)
        self.assertEqual([('hello/world', 'there')], self.sub1)

    def test_pub_src(self):
        self.p.subscribe('hello/world', self.sub1_fn)
        self.p.publish('hello/world', 'there', src_cbk=self.sub1_fn)
        self.assertEqual([], self.sub1)

    def test_sub_parent(self):
        self.p.subscribe('', self.sub1_fn)
        self.p.subscribe('hello/there', self.sub2_fn)
        self.p.publish('hello/there/world', 'value')
        self.assertEqual([('hello/there/world', 'value')], self.sub1)
        self.assertEqual([('hello/there/world', 'value')], self.sub1)

    def test_unsub(self):
        self.p.subscribe('hello/world', self.sub1_fn)
        self.p.unsubscribe('hello/world', self.sub1_fn)
        self.p.publish('hello/world', 'there')
        self.assertEqual([], self.sub1)

    def test_get(self):
        self.p.publish('hello/world', 'there', retain=True)
        self.assertEqual('there', self.p.get('hello/world'))
        self.p.publish('hello/world', 'new', retain=True)
        self.assertEqual('new', self.p.get('hello/world'))
        self.p.publish('hello/world', 'newer')  # not retained!
        self.assertIsNone(self.p.get('hello/world'))

    def test_meta(self):
        meta1 = {'type': 'u32'}
        self.p.meta('hello/world', meta1)
        self.p.subscribe('', self.sub1_fn, forward=True)
        self.p.publish('$', None)
        self.assertEqual([('hello/world$', meta1), ('$', None)], self.sub1)

    def test_meta_other_subscribe_root(self):
        self.p.subscribe('', self.sub1_fn, forward=True)
        self.p.publish('other/$', None)
        self.assertEqual([('other/$', None)], self.sub1)

    def test_meta_other_subscribe_subtopic(self):
        self.p.subscribe('other', self.sub1_fn, forward=True)
        self.p.publish('other/$', None)
        self.assertEqual([('other/$', None)], self.sub1)

    # def test_query(self):  # todo
