// SPDX-License-Identifier: Apache-2.0
// Copyright 2021 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_BT_CONN_H
#define UNI_BT_CONN_H

#include <stdbool.h>
#include <stdint.h>

#include <btstack.h>

#define UNI_BT_CONN_HANDLE_INVALID 0xffff

typedef enum {
    UNI_BT_CONN_PROTOCOL_NONE,
    UNI_BT_CONN_PROTOCOL_BR_EDR,
    UNI_BT_CONN_PROTOCOL_BLE,
} uni_bt_conn_protocol_t;

typedef enum {
    UNI_BT_CONN_STATE_DEVICE_NONE,  // Must be the first state

    UNI_BT_CONN_STATE_DEVICE_DISCOVERED,

    UNI_BT_CONN_STATE_REMOTE_NAME_REQUEST,
    UNI_BT_CONN_STATE_REMOTE_NAME_INQUIRED,
    UNI_BT_CONN_STATE_REMOTE_NAME_FETCHED,

    UNI_BT_CONN_STATE_SDP_VENDOR_REQUESTED,
    UNI_BT_CONN_STATE_SDP_VENDOR_FETCHED,
    UNI_BT_CONN_STATE_SDP_HID_DESCRIPTOR_REQUESTED,
    UNI_BT_CONN_STATE_SDP_HID_DESCRIPTOR_FETCHED,

    UNI_BT_CONN_STATE_L2CAP_CONTROL_CONNECTION_REQUESTED,
    UNI_BT_CONN_STATE_L2CAP_CONTROL_CONNECTED,
    UNI_BT_CONN_STATE_L2CAP_INTERRUPT_CONNECTION_REQUESTED,
    UNI_BT_CONN_STATE_L2CAP_INTERRUPT_CONNECTED,

    UNI_BT_CONN_STATE_DEVICE_PENDING_READY,
    UNI_BT_CONN_STATE_DEVICE_READY,
} uni_bt_conn_state_t;

typedef struct {
    bd_addr_t btaddr;
    hci_con_handle_t handle;

    uint16_t control_cid;
    uint16_t interrupt_cid;

    // BR/EDR only
    uint8_t page_scan_repetition_mode;
    uint16_t clock_offset;

    // BLE & BR/EDR
    uint8_t rssi;

    bool incoming;
    bool connected;

    uni_bt_conn_state_t state;
    uni_bt_conn_protocol_t protocol;
} uni_bt_conn_t;

void uni_bt_conn_init(uni_bt_conn_t* conn);
void uni_bt_conn_set_state(uni_bt_conn_t* conn, uni_bt_conn_state_t state);
uni_bt_conn_state_t uni_bt_conn_get_state(uni_bt_conn_t* conn);
void uni_bt_conn_set_protocol(uni_bt_conn_t* conn, uni_bt_conn_protocol_t protocol);
void uni_bt_conn_get_address(uni_bt_conn_t* conn, bd_addr_t out_addr);
bool uni_bt_conn_is_incoming(uni_bt_conn_t* conn);
void uni_bt_conn_set_connected(uni_bt_conn_t* conn, bool connected);
bool uni_bt_conn_is_connected(uni_bt_conn_t* conn);
void uni_bt_conn_disconnect(uni_bt_conn_t* conn);

#endif  // UNI_BT_CONN_H
