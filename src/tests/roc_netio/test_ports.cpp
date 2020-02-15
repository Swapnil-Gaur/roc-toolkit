/*
 * Copyright (c) 2015 Roc authors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <CppUTest/TestHarness.h>

#include "roc_address/socket_addr.h"
#include "roc_core/buffer_pool.h"
#include "roc_core/heap_allocator.h"
#include "roc_netio/network_loop.h"
#include "roc_packet/concurrent_queue.h"
#include "roc_packet/packet_pool.h"

namespace roc {
namespace netio {

namespace {

enum { MaxBufSize = 500 };

core::HeapAllocator allocator;
core::BufferPool<uint8_t> buffer_pool(allocator, MaxBufSize, true);
packet::PacketPool packet_pool(allocator, true);

UdpSenderConfig make_sender_config(const char* ip, int port) {
    UdpSenderConfig config;
    CHECK(config.bind_address.set_host_port(address::Family_IPv4, ip, port));
    return config;
}

UdpReceiverConfig make_receiver_config(const char* ip, int port) {
    UdpReceiverConfig config;
    CHECK(config.bind_address.set_host_port(address::Family_IPv4, ip, port));
    return config;
}

NetworkLoop::PortHandle add_udp_receiver(NetworkLoop& net_loop,
                                         UdpReceiverConfig& config,
                                         packet::IWriter& writer) {
    NetworkLoop::Tasks::AddUdpReceiverPort task(config, writer);
    CHECK(!task.success());
    if (!net_loop.schedule_and_wait(task)) {
        CHECK(!task.success());
        return NULL;
    }
    CHECK(task.success());
    return task.get_handle();
}

NetworkLoop::PortHandle add_udp_sender(NetworkLoop& net_loop, UdpSenderConfig& config) {
    NetworkLoop::Tasks::AddUdpSenderPort task(config);
    CHECK(!task.success());
    if (!net_loop.schedule_and_wait(task)) {
        CHECK(!task.success());
        return NULL;
    }
    CHECK(task.success());
    return task.get_handle();
}

void remove_port(NetworkLoop& net_loop, NetworkLoop::PortHandle handle) {
    NetworkLoop::Tasks::RemovePort task(handle);
    CHECK(!task.success());
    CHECK(net_loop.schedule_and_wait(task));
    CHECK(task.success());
}

} // namespace

TEST_GROUP(ports) {};

TEST(ports, init) {
    NetworkLoop net_loop(packet_pool, buffer_pool, allocator);
    CHECK(net_loop.valid());

    UNSIGNED_LONGS_EQUAL(0, net_loop.num_ports());
}

TEST(ports, add) {
    packet::ConcurrentQueue queue;

    NetworkLoop net_loop(packet_pool, buffer_pool, allocator);
    CHECK(net_loop.valid());

    UdpSenderConfig tx_config = make_sender_config("0.0.0.0", 0);
    UdpReceiverConfig rx_config = make_receiver_config("0.0.0.0", 0);

    UNSIGNED_LONGS_EQUAL(0, net_loop.num_ports());

    NetworkLoop::PortHandle tx_handle = add_udp_sender(net_loop, tx_config);
    CHECK(tx_handle);
    UNSIGNED_LONGS_EQUAL(1, net_loop.num_ports());

    NetworkLoop::PortHandle rx_handle = add_udp_receiver(net_loop, rx_config, queue);
    CHECK(rx_handle);
    UNSIGNED_LONGS_EQUAL(2, net_loop.num_ports());
}

TEST(ports, add_remove) {
    packet::ConcurrentQueue queue;

    NetworkLoop net_loop(packet_pool, buffer_pool, allocator);
    CHECK(net_loop.valid());

    UdpSenderConfig tx_config = make_sender_config("0.0.0.0", 0);
    UdpReceiverConfig rx_config = make_receiver_config("0.0.0.0", 0);

    UNSIGNED_LONGS_EQUAL(0, net_loop.num_ports());

    NetworkLoop::PortHandle tx_handle = add_udp_sender(net_loop, tx_config);
    CHECK(tx_handle);
    UNSIGNED_LONGS_EQUAL(1, net_loop.num_ports());

    NetworkLoop::PortHandle rx_handle = add_udp_receiver(net_loop, rx_config, queue);
    CHECK(rx_handle);
    UNSIGNED_LONGS_EQUAL(2, net_loop.num_ports());

    remove_port(net_loop, tx_handle);
    UNSIGNED_LONGS_EQUAL(1, net_loop.num_ports());

    remove_port(net_loop, rx_handle);
    UNSIGNED_LONGS_EQUAL(0, net_loop.num_ports());
}

TEST(ports, add_remove_add) {
    NetworkLoop net_loop(packet_pool, buffer_pool, allocator);
    CHECK(net_loop.valid());

    UdpSenderConfig tx_config = make_sender_config("0.0.0.0", 0);

    NetworkLoop::PortHandle tx_handle = add_udp_sender(net_loop, tx_config);
    CHECK(tx_handle);
    UNSIGNED_LONGS_EQUAL(1, net_loop.num_ports());

    remove_port(net_loop, tx_handle);
    UNSIGNED_LONGS_EQUAL(0, net_loop.num_ports());

    tx_handle = add_udp_sender(net_loop, tx_config);
    CHECK(tx_handle);
    UNSIGNED_LONGS_EQUAL(1, net_loop.num_ports());
}

TEST(ports, add_duplicate) {
    packet::ConcurrentQueue queue;

    NetworkLoop net_loop(packet_pool, buffer_pool, allocator);
    CHECK(net_loop.valid());

    UdpSenderConfig port1_tx = make_sender_config("0.0.0.0", 0);

    NetworkLoop::PortHandle tx_handle = add_udp_sender(net_loop, port1_tx);
    CHECK(tx_handle);
    UNSIGNED_LONGS_EQUAL(1, net_loop.num_ports());

    UdpReceiverConfig port1_rx =
        make_receiver_config("0.0.0.0", port1_tx.bind_address.port());

    CHECK(!add_udp_sender(net_loop, port1_tx));
    UNSIGNED_LONGS_EQUAL(1, net_loop.num_ports());

    CHECK(!add_udp_receiver(net_loop, port1_rx, queue));
    UNSIGNED_LONGS_EQUAL(1, net_loop.num_ports());

    UdpReceiverConfig port2_rx = make_receiver_config("0.0.0.0", 0);

    NetworkLoop::PortHandle rx_handle = add_udp_receiver(net_loop, port2_rx, queue);
    CHECK(rx_handle);
    UNSIGNED_LONGS_EQUAL(2, net_loop.num_ports());

    UdpSenderConfig port2_tx =
        make_sender_config("0.0.0.0", port2_rx.bind_address.port());

    CHECK(!add_udp_sender(net_loop, port2_tx));
    UNSIGNED_LONGS_EQUAL(2, net_loop.num_ports());

    CHECK(!add_udp_receiver(net_loop, port2_rx, queue));
    UNSIGNED_LONGS_EQUAL(2, net_loop.num_ports());

    remove_port(net_loop, tx_handle);
    UNSIGNED_LONGS_EQUAL(1, net_loop.num_ports());

    remove_port(net_loop, rx_handle);
    UNSIGNED_LONGS_EQUAL(0, net_loop.num_ports());
}

} // namespace netio
} // namespace roc