/*
 * Copyright 2021 Jetperch LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "fitterbap/comm/port.h"

int32_t fbp_port_register(struct fbp_port_api_s * self, const struct fbp_port_config_s * config) {
    int32_t rc = self->initialize(self, config);
    if (rc) {
        return rc;
    }
    return fbp_transport_port_register(config->transport, config->port_id, self->meta,
                                       self->on_event, self->on_recv, self);
}
