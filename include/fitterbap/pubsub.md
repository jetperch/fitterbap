<!--
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
-->

# PubSub

This PubSub module provides a publish-subscribe implementation suitable
for small embedded microcontrollers.  Features include:
- Multiple data types including integers, floats, str, json, binary
- Constant pointers for more efficient memory usage
- Retained messages, but only for fixed-size types (≤8 bytes) and const pointers.
- Topic metadata for automatically populating user interfaces.
- Supports distributed instances in
  [polytree](https://en.wikipedia.org/wiki/Polytree) topology:
  - Concept of topic prefix ownership.
  - Query retained messages
  - Asynchronous error reporting
  - State recovery in the event that any pubsub instance resets.
  - Automatic topic routing using "fitterbap/comm/pubsub_port.h".
- Thread-safe, in-order operation.  All updates queued and processed from
  the pubsub context.  Subscribers can safely publish topic updates from
  the subscriber callback with recursion or stack problems.
- Reentrant on publish using mutex.
- Dynamic allocation but only free in fbp_pubsub_finalize(), which is
  not normally used in an embedded microcontroller instantiations.
- Guaranteed in-order topic traversal for retained messages based
  upon creation order.


## Topics

Topic names are any valid UTF-8.  However, we highly recommend restricting
topic names to ASCII standard letters and numbers 0-9, A-Z, a-z, ".", _ and - 
(ASCII codes 45, 46, 48-57, 65-90, 95, 97-122)
The following symbols are reserved:

    /?#$'"`&@%

Topics are hierarchical, and each level of the hierarchy is separated by
'/'.  The topic should **_not_** start with '/'.  We recommend using short
names or abbreviations, including single letters, to keep the topic
string short.  Topics should be 30 bytes or less.  Topic storage is
limited to 32 bytes by default which includes the operation suffix
character and the null termination.  This small topic size supports
memory-constrained devices.  Topics can have metadata to provide
user-meaningful names and descriptions.

Topics that end in '$' are the JSON metadata for the associated topic
without the $.  Most microcontrollers should use both CONST and RETAIN
flags for metadata.  The metadata format is JSON with the following
keys:
- dtype: one of [str, json, bin, f32, f64, u8, u16, u32, u64, i8, i16, i32, i64]
- brief: A brief string description (recommended).
- detail: A more detailed string description (optional).
- default: The recommended default value (optional).
- options: A list of options, which is each a list of:   
     [value, [alt1 [, ...]]]   
     The alternates must be given in order.  The first value
     must be the value as dtype.  The second value alt1
     (when provided) is used to automatically populate user
     interfaces, and it can be the same as value.  Additional
     values will be interpreted as equivalents.
- range: The list of [v_min, v_max] or [v_min, v_max, v_step].  Both
  v_min and v_max are *inclusive*.  v_step defaults to 1 if omitted.
- format: Formatting hints for the value:
  - version: The u32 dtype should be interpreted as major8.minor8.patch16.
- flags: A list of flags for this topic.  Options include:
  - ro: This topic cannot be updated.
  - hide: This topic should not appear in the user interface.
  - dev: Developer option that should not be used in production.

To re-enumerate all metadata, publish NULL to "$" or "topic/$".
This implementation recognizes this request, and will publish
all metadata instances to "topic/hierarchy/name$".  The response
is forwarded to all matching response subscribers.  If the metadata
request does not match this pubsub instance, it will be forwarded
to any matching subscribers with the request flag.
Each pubsub instance will respond to the matching topics it owns.

If publishing to a topic "t/h/n" owned by a pubsub instance fails, then that
instance will publish to "t/h/n#".  The error will be forward to any matching
response subscribers.  The value is a u32 containing the error code.


## Special Topics

In addition to topics ending in a special character, some topics
have special meaning.  Any topic that starts with "_/" is considered
local to a PubSub instance.  Links should not propagate those topics
to any other PubSub instance.

Topics that start with "./" indicate comm events.  

The following topics are reserved:

- **_/topic/prefix**: The retained topic prefix for this PubSub instance.
- **_/topic/add**: Add the topic prefix as a PubSub child.  Provided by 
  each child link upon connection.
- **_/topic/remove**: Remove the topic prefix as a PubSub child.  Provided by
  a child link upon disconnection.
- **_/topic/list**: The retained "unit separator" (0x1F) separated topic 
  prefix list for this PubSub instance and all children, as aggregated
  from **_/topic/add** and **_/topic/remove**.
  Note that this list updates before the connection fully establishes.
  See ./comm/add and ./comm/remove below.
- **./comm/add**: A connection was established, and the list of base topics 
  are now available.
- **./comm/remove**: A connection was lost, and the list of base topics
  are now available.


## Distributed State Recovery

This PubSub implementation supports a distributed architecture across
multiple microcontrollers.  In real-world systems, bad things happen.
A watchdog timer can reset a microcontroller.  A link can temporarily
fail.  The state must remain consistent under all these cases.

Let's take the following system:

```
       Client          Host   Client          Host           
      A      <-- AB -->      B      <-- BC -->      C
    PubSub      Link       PubSub      Link       PubSub
```

The goal is to ensure that all three PubSub instances (A, B, C)
maintain consistent state.  Consistent state must be established
at startup and whenever any instance resets.

The PubSub Links (AB, BC) share information between the PubSub instances.
The Links are designed with a Host and Client architecture to form a 
directed graph, specifically a PolyTree.

This example describes "fitterbap/comm/pubsub_port.h", which implements
a link as a Comm protocol Port.
Each Link Host and Client maintains conn_count, the number of 
times that it has established connection.  If 
client_conn_count > host_conn_count or both == 1, then the client
state propagates to the host.  If 
client_conn_count <= host_conn_count, the the host state
propagates to the client.


### Case 1: Connect AB only

1. Link AB connects, client_conn_count == 1 and host_conn_count == 1
2. Client AB advertises topic base "a"
3. Host AB subscribes to B topic "a" without RETAIN
4. Client AB subscribes to A topic "" with RETAIN
5. Client AB forwards PubSub A retained messages to PubSub B 


### Case 2: Connect BC after AB

After Case 1:
1. Link BC connects, client_conn_count == 1 and host_conn_count == 1
2. Client BC advertises topic base "b" with dependant "a"
3. Host BC subscribes to C topic "b" and "a" without RETAIN
4. Client BC subscribes to B topic "" with RETAIN
5. Client BC forwards PubSub B retained messages to PubSub C, including the
   retained messages from A.


### Case 3: Connect AB after BC

1. Link BC connects, client_conn_count == 1 and host_conn_count == 1
2. Client BC advertises topic base "b"
3. Host BC subscribes to B topic "" with RETAIN
4. Client BC subscribes to B topic "" with RETAIN
4. Client BC forwards PubSub B retained messages to PubSub C
5. Link AB connects, client_conn_count == 1 and host_conn_count == 1
6. Client AB advertises topic base "a"
7. Host AB subscribes to B topic "a" without RETAIN
8. Client AB subscribes to A topic "" with RETAIN
9. Client AB forwards PubSub A retained messages to PubSub B, which are
   also forwarded by Client BC to C.


### Case 4: A resets

1. Link AB disconnects
2. Link AB connects, client_conn_count == 1 and host_conn_count == 2
3. Client AB advertises topic base "a"
3. Host AB subscribes to B topic "a" with RETAIN
4. Client AB subscribes to B topic "" without RETAIN
5. Host AB forwards PubSub B retained messages to PubSub A.


### Case 5: B resets with AB reconnecting first

1. Link AB and BC disconnect
2. Link AB connects, client_conn_count == 2 and host_conn_count == 1
3. Client AB advertises topic base "a"
4. Host AB subscribes to B topic "a" without RETAIN
5. Client AB subscribes to B topic "" with RETAIN
6. Client AB forwards PubSub A retained messages to PubSub B.
7. Link BC connects, client_conn_count == 1 and host_conn_count == 2
8. Client BC advertises topic base "b" with additional "a"
9. Host BC subscribes to C topic "b" and "a" with RETAIN
10. Client BC subscribes to B topic "" without RETAIN
11. Host BC forwards PubSub C retained messages to PubSub B (& A)

Note: PubSub C on topic "a" will overwrite any changes to
PubSub A while PubSub B is disconnected.


### Case 6: B resets with BC first

1. Link AB and BC disconnect
2. Link BC connects, client_conn_count == 1 and host_conn_count == 2
3. Client BC advertises topic base "b" with additional "a"
4. Host BC subscribes to C topic "b" with RETAIN
5. Client BC subscribes to B topic "" without RETAIN
6. Host BC forwards PubSub C retained messages to PubSub B
7. Link AB connects, client_conn_count == 2 and host_conn_count == 1
8. Client AB advertises topic base "a"
9. Host AB subscribes to B topic "a" without RETAIN
10. Client BC forwards dependent topic "b"   
11. Client AB subscribes to B topic "" with RETAIN
12. Client AB forwards PubSub A retained messages to PubSub B (& C)

Note: PubSub A on topic "a" will overwrite any changes to
PubSub C while PubSub B is disconnected.


### Case 7: C resets

This is very similar to Case 2.  All PubSub C state is restored
from PubSub B, just like with first connection.



## Alternatives include:

- [pubsub-c](https://github.com/jaracil/pubsub-c) but uses dynamic memory.
- [ZCM](https://zerocm.github.io/zcm/)
  

## PubSub Servers & Brokers

- [MQTT](https://mqtt.org/): Can be used by microcontrollers, but usually
  hosted on a microprocessor.  See [mosquitto](https://mosquitto.org/).
- [PubNub](https://www.pubnub.com/)
