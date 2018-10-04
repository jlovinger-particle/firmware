/*
 * Copyright (c) 2018 Particle Industries, Inc.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "usart_hal.h"

#include "stream.h"

namespace particle {

class SerialStream: public Stream {
public:
    SerialStream(HAL_USART_Serial serial, uint32_t baudrate, uint32_t config);
    ~SerialStream();

    int read(char* data, size_t size) override;
    int peek(char* data, size_t size) override;
    int skip(size_t size) override;
    int write(const char* data, size_t size) override;
    int flush() override;
    int waitEvent(unsigned flags, unsigned timeout) override;

private:
    Ring_Buffer rxBuffer_;
    Ring_Buffer txBuffer_;
    HAL_USART_Serial serial_;
};

} // particle
