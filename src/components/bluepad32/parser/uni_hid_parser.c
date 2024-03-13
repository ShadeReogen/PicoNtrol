// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "parser/uni_hid_parser.h"

#include "hid_usage.h"
#include "uni_hid_device.h"
#include "uni_log.h"

// HID Usage Tables:
// https://www.usb.org/sites/default/files/documents/hut1_12v2.pdf

void uni_hid_parse_input_report(struct uni_hid_device_s* d, const uint8_t* report, uint16_t report_len) {
    btstack_hid_parser_t parser;

    uni_report_parser_t* rp = &d->report_parser;

    // Certain devices like iCade might not set "init_report".
    if (rp->init_report)
        rp->init_report(d);

    // Certain devices like Nintendo Wii U Pro doesn't support HID descriptor.
    // For those kind of devices, just send the raw report.
    if (rp->parse_input_report) {
        rp->parse_input_report(d, report, report_len);
    }

    // Devices that suport regular HID reports.
    if (rp->parse_usage) {
        btstack_hid_parser_init(&parser, d->hid_descriptor, d->hid_descriptor_len, HID_REPORT_TYPE_INPUT, report,
                                report_len);
        while (btstack_hid_parser_has_more(&parser)) {
            uint16_t usage_page;
            uint16_t usage;
            int32_t value;
            hid_globals_t globals;

            // Save globals, since they are destroyed by btstack_hid_parser_get_field()
            // see: https://github.com/bluekitchen/btstack/issues/187
            globals.logical_minimum = parser.global_logical_minimum;
            globals.logical_maximum = parser.global_logical_maximum;
            globals.report_count = parser.global_report_count;
            globals.report_id = parser.global_report_id;
            globals.report_size = parser.global_report_size;
            globals.usage_page = parser.global_usage_page;

            btstack_hid_parser_get_field(&parser, &usage_page, &usage, &value);

            logd("usage_page = 0x%04x, usage = 0x%04x, value = 0x%x\n", usage_page, usage, value);
            rp->parse_usage(d, &globals, usage_page, usage, value);
        }
    }
}

// Converts a possible value between (0, x) to (-x/2, x/2), and normalizes it
// between -512 and 511.
int32_t uni_hid_parser_process_axis(hid_globals_t* globals, uint32_t value) {
    int32_t max = globals->logical_maximum;
    int32_t min = globals->logical_minimum;

    // Amazon Fire 1st Gen reports max value as unsigned (0xff == 255) but the
    // spec says they are signed. So the parser correctly treats it as -1 (0xff).
    if (max == -1) {
        max = (1 << globals->report_size) - 1;
    }

    // Get the range: how big can be the number
    int32_t range = (max - min) + 1;

    // First we "center" the value, meaning that 0 is when the axis is not used.
    int32_t centered = value - range / 2 - min;

    // Then we normalize between -512 and 511
    int32_t normalized = centered * AXIS_NORMALIZE_RANGE / range;
    logd("original = %d, centered = %d, normalized = %d (range = %d, min=%d, max=%d)\n", value, centered, normalized,
         range, min, max);

    return normalized;
}

// Converts a possible value between (0, x) to (0, 1023)
int32_t uni_hid_parser_process_pedal(hid_globals_t* globals, uint32_t value) {
    int32_t max = globals->logical_maximum;
    int32_t min = globals->logical_minimum;

    // Amazon Fire 1st Gen reports max value as unsigned (0xff == 255) but the
    // spec says they are signed. So the parser correctly treats it as -1 (0xff).
    if (max == -1) {
        max = (1 << globals->report_size) - 1;
    }

    // Get the range: how big can be the number
    int32_t range = (max - min) + 1;
    int32_t normalized = value * AXIS_NORMALIZE_RANGE / range;
    logd("original = %d, normalized = %d (range = %d, min=%d, max=%d)\n", value, normalized, range, min, max);

    return normalized;
}

uint8_t uni_hid_parser_process_hat(hid_globals_t* globals, uint32_t value) {
    int32_t v = (int32_t)value;
    // Assumes if value is outside valid range, then it is a "null value"
    if (v < globals->logical_minimum || v > globals->logical_maximum)
        return 0xff;
    // 0 should be the first value for hat, meaning that 0 is the "up" position.
    return v - globals->logical_minimum;
}

void uni_hid_parser_process_dpad(uint16_t usage, uint32_t value, uint8_t* dpad) {
    switch (usage) {
        case HID_USAGE_DPAD_UP:
            if (value)
                *dpad |= DPAD_UP;
            else
                *dpad &= ~DPAD_UP;
            break;
        case HID_USAGE_DPAD_DOWN:
            if (value)
                *dpad |= DPAD_DOWN;
            else
                *dpad &= ~DPAD_DOWN;
            break;
        case HID_USAGE_DPAD_RIGHT:
            if (value)
                *dpad |= DPAD_RIGHT;
            else
                *dpad &= ~DPAD_RIGHT;
            break;
        case HID_USAGE_DPAD_LEFT:
            if (value)
                *dpad |= DPAD_LEFT;
            else
                *dpad &= ~DPAD_LEFT;
            break;
        default:
            logi("Unsupported DPAD usage: 0x%02x", usage);
            break;
    }
}

uint8_t uni_hid_parser_hat_to_dpad(uint8_t hat) {
    uint8_t dpad = 0;
    switch (hat) {
        case 0xff:
        case 0x08:
            // joy.up = joy.down = joy.left = joy.right = 0;
            break;
        case 0:
            dpad = DPAD_UP;
            break;
        case 1:
            dpad = DPAD_UP | DPAD_RIGHT;
            break;
        case 2:
            dpad = DPAD_RIGHT;
            break;
        case 3:
            dpad = DPAD_RIGHT | DPAD_DOWN;
            break;
        case 4:
            dpad = DPAD_DOWN;
            break;
        case 5:
            dpad = DPAD_DOWN | DPAD_LEFT;
            break;
        case 6:
            dpad = DPAD_LEFT;
            break;
        case 7:
            dpad = DPAD_LEFT | DPAD_UP;
            break;
        default:
            loge("Error parsing hat value: 0x%02x\n", hat);
            break;
    }
    return dpad;
}
