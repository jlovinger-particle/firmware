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

#include "ifapi.h"
#include "ot_api.h"
#include "openthread/lwip_openthreadif.h"
#include "wiznet/wiznetif.h"
#include "nat64.h"
#include <mutex>
#include <memory>
#include <nrf52840.h>
#include "random.h"
#include "check.h"
#include "border_router_manager.h"
#include <malloc.h>
#include "esp32_ncp_client.h"
#include "wifi_manager.h"
#include "ncp.h"
#include "debug.h"

using namespace particle;
using namespace particle::net;
using namespace particle::net::nat;

namespace {

/* th2 - OpenThread */
BaseNetif* th2 = nullptr;
/* en3 - Ethernet FeatherWing */
BaseNetif* en3 = nullptr;

} /* anonymous */

namespace particle {
/*
class WifiManagerInitializer {
public:
    WifiManagerInitializer() {
        const int ret = init();
        SPARK_ASSERT(ret == 0);
    }

    WifiManager* instance() const {
        return mgr_.get();
    }

private:
    std::unique_ptr<SerialStream> strm_;
    std::unique_ptr<services::at::ArgonNcpAtClient> atClient_;
    std::unique_ptr<WifiNcpClient> ncpClient_;
    std::unique_ptr<WifiManager> mgr_;

    int init() {
        // Serial stream
        std::unique_ptr<SerialStream> strm(new(std::nothrow) SerialStream(HAL_USART_SERIAL2, 921600,
                SERIAL_8N1 | SERIAL_FLOW_CONTROL_RTS_CTS));
        CHECK_TRUE(strm, SYSTEM_ERROR_NO_MEMORY);
        // AT client
        std::unique_ptr<services::at::ArgonNcpAtClient> atClient(
                new(std::nothrow) services::at::ArgonNcpAtClient(strm.get()));
        CHECK_TRUE(atClient, SYSTEM_ERROR_NO_MEMORY);
        // NCP client
        std::unique_ptr<WifiNcpClient> ncpClient(new(std::nothrow) Esp32NcpClient(atClient.get()));
        CHECK_TRUE(ncpClient, SYSTEM_ERROR_NO_MEMORY);
        // WiFi manager
        mgr_.reset(new(std::nothrow) WifiManager(ncpClient.get()));
        CHECK_TRUE(mgr_, SYSTEM_ERROR_NO_MEMORY);
        strm_ = std::move(strm);
        atClient_ = std::move(atClient);
        ncpClient_ = std::move(ncpClient);
        return 0;
    }
};
*/
WifiManager* wifiManager() {
    return nullptr;
/*
    static WifiManagerInitializer mgr;
    return mgr.instance();
*/
}

} // particle

int if_init_platform(void*) {
    /* lo1 (created by LwIP) */

    /* th2 - OpenThread */
    th2 = new OpenThreadNetif(ot_get_instance());

    /* en3 - Ethernet FeatherWing (optional) */
    uint8_t mac[6] = {};
    {
        const uint32_t lsb = __builtin_bswap32(NRF_FICR->DEVICEADDR[0]);
        const uint32_t msb = NRF_FICR->DEVICEADDR[1] & 0xffff;
        memcpy(mac + 2, &lsb, sizeof(lsb));
        mac[0] = msb >> 8;
        mac[1] = msb;
        /* Drop 'multicast' bit */
        mac[0] &= 0b11111110;
        /* Set 'locally administered' bit */
        mac[0] |= 0b10;
    }
    en3 = new WizNetif(HAL_SPI_INTERFACE1, D5, D3, D4, mac);
    uint8_t dummy;
    if (if_get_index(en3->interface(), &dummy)) {
        /* No en3 present */
        delete en3;
        en3 = nullptr;
    } else {
        /* Enable border router by default */
        BorderRouterManager::instance()->start();
    }

    auto m = mallinfo();
    const size_t total = m.uordblks + m.fordblks;
    LOG(TRACE, "Heap: %lu/%lu Kbytes used", m.uordblks / 1000, total / 1000);

    return 0;
}

extern "C" {

struct netif* lwip_hook_ip4_route_src(const ip4_addr_t* src, const ip4_addr_t* dst) {
    if (en3) {
        return en3->interface();
    }

    return nullptr;
}

int lwip_hook_ip6_forward_pre_routing(struct pbuf* p, struct ip6_hdr* ip6hdr, struct netif* inp, u32_t* flags) {
    auto nat64 = BorderRouterManager::instance()->getNat64();
    if (nat64) {
        return nat64->ip6Input(p, ip6hdr, inp);
    }

    /* Do not forward */
    return 1;
}

int lwip_hook_ip4_input_pre_upper_layers(struct pbuf* p, const struct ip_hdr* iphdr, struct netif* inp) {
    auto nat64 = BorderRouterManager::instance()->getNat64();
    if (nat64) {
        int r = nat64->ip4Input(p, (ip_hdr*)iphdr, inp);
        if (r) {
            /* Ip4 hooks do not free the packet if it has been handled by the hook */
            pbuf_free(p);
        }

        return r;
    }

    /* Try to handle locally if not consumed by NAT64 */
    return 0;
}

}
