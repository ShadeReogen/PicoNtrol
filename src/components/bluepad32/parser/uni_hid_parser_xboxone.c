// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

// More info about Xbox One gamepad:
// https://support.xbox.com/en-US/xbox-one/accessories/xbox-one-wireless-controller

// Technical info taken from:
// https://github.com/atar-axis/xpadneo/blob/master/hid-xpadneo/src/hid-xpadneo.c

#include "parser/uni_hid_parser_xboxone.h"

#include "controller/uni_controller.h"
#include "hid_usage.h"
#include "uni_hid_device.h"
#include "uni_log.h"

// Xbox doesn't report trigger buttons. Instead it reports throttle/brake.
// This threshold represents the minimum value of throttle/brake to "report"
// the trigger buttons.
#define TRIGGER_BUTTON_THRESHOLD 32

static const uint16_t XBOX_WIRELESS_VID = 0x045e;  // Microsoft
static const uint16_t XBOX_WIRELESS_PID = 0x02e0;  // Xbox One (Bluetooth)

static const uint8_t xbox_hid_descriptor_4_8_fw[] = {
    0x05, 0x01, 0x09, 0x05, 0xa1, 0x01, 0x85, 0x01, 0x09, 0x01, 0xa1, 0x00, 0x09, 0x30, 0x09, 0x31, 0x15, 0x00, 0x27,
    0xff, 0xff, 0x00, 0x00, 0x95, 0x02, 0x75, 0x10, 0x81, 0x02, 0xc0, 0x09, 0x01, 0xa1, 0x00, 0x09, 0x32, 0x09, 0x35,
    0x15, 0x00, 0x27, 0xff, 0xff, 0x00, 0x00, 0x95, 0x02, 0x75, 0x10, 0x81, 0x02, 0xc0, 0x05, 0x02, 0x09, 0xc5, 0x15,
    0x00, 0x26, 0xff, 0x03, 0x95, 0x01, 0x75, 0x0a, 0x81, 0x02, 0x15, 0x00, 0x25, 0x00, 0x75, 0x06, 0x95, 0x01, 0x81,
    0x03, 0x05, 0x02, 0x09, 0xc4, 0x15, 0x00, 0x26, 0xff, 0x03, 0x95, 0x01, 0x75, 0x0a, 0x81, 0x02, 0x15, 0x00, 0x25,
    0x00, 0x75, 0x06, 0x95, 0x01, 0x81, 0x03, 0x05, 0x01, 0x09, 0x39, 0x15, 0x01, 0x25, 0x08, 0x35, 0x00, 0x46, 0x3b,
    0x01, 0x66, 0x14, 0x00, 0x75, 0x04, 0x95, 0x01, 0x81, 0x42, 0x75, 0x04, 0x95, 0x01, 0x15, 0x00, 0x25, 0x00, 0x35,
    0x00, 0x45, 0x00, 0x65, 0x00, 0x81, 0x03, 0x05, 0x09, 0x19, 0x01, 0x29, 0x0f, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01,
    0x95, 0x0f, 0x81, 0x02, 0x15, 0x00, 0x25, 0x00, 0x75, 0x01, 0x95, 0x01, 0x81, 0x03, 0x05, 0x0c, 0x0a, 0x24, 0x02,
    0x15, 0x00, 0x25, 0x01, 0x95, 0x01, 0x75, 0x01, 0x81, 0x02, 0x15, 0x00, 0x25, 0x00, 0x75, 0x07, 0x95, 0x01, 0x81,
    0x03, 0x05, 0x0c, 0x09, 0x01, 0x85, 0x02, 0xa1, 0x01, 0x05, 0x0c, 0x0a, 0x23, 0x02, 0x15, 0x00, 0x25, 0x01, 0x95,
    0x01, 0x75, 0x01, 0x81, 0x02, 0x15, 0x00, 0x25, 0x00, 0x75, 0x07, 0x95, 0x01, 0x81, 0x03, 0xc0, 0x05, 0x0f, 0x09,
    0x21, 0x85, 0x03, 0xa1, 0x02, 0x09, 0x97, 0x15, 0x00, 0x25, 0x01, 0x75, 0x04, 0x95, 0x01, 0x91, 0x02, 0x15, 0x00,
    0x25, 0x00, 0x75, 0x04, 0x95, 0x01, 0x91, 0x03, 0x09, 0x70, 0x15, 0x00, 0x25, 0x64, 0x75, 0x08, 0x95, 0x04, 0x91,
    0x02, 0x09, 0x50, 0x66, 0x01, 0x10, 0x55, 0x0e, 0x15, 0x00, 0x26, 0xff, 0x00, 0x75, 0x08, 0x95, 0x01, 0x91, 0x02,
    0x09, 0xa7, 0x15, 0x00, 0x26, 0xff, 0x00, 0x75, 0x08, 0x95, 0x01, 0x91, 0x02, 0x65, 0x00, 0x55, 0x00, 0x09, 0x7c,
    0x15, 0x00, 0x26, 0xff, 0x00, 0x75, 0x08, 0x95, 0x01, 0x91, 0x02, 0xc0, 0x05, 0x06, 0x09, 0x20, 0x85, 0x04, 0x15,
    0x00, 0x26, 0xff, 0x00, 0x75, 0x08, 0x95, 0x01, 0x81, 0x02, 0xc0,
};

// Supported Xbox One firmware revisions.
// Probably there are more revisions, but I only found two in the "wild".
enum xboxone_firmware {
    XBOXONE_FIRMWARE_V3_1,  // The one that came pre-installed, or close to it.
    XBOXONE_FIRMWARE_V4_8,  // The one released in 2019-10
    XBOXONE_FIRMWARE_V5,    // BLE version
};

// xboxone_instance_t represents data used by the Wii driver instance.
typedef struct wii_instance_s {
    enum xboxone_firmware version;
} xboxone_instance_t;
_Static_assert(sizeof(xboxone_instance_t) < HID_DEVICE_MAX_PARSER_DATA, "Xbox one intance too big");

static xboxone_instance_t* get_xboxone_instance(uni_hid_device_t* d);
static void parse_usage_firmware_v3_1(uni_hid_device_t* d,
                                      hid_globals_t* globals,
                                      uint16_t usage_page,
                                      uint16_t usage,
                                      int32_t value);
static void parse_usage_firmware_v4_v5(uni_hid_device_t* d,
                                       hid_globals_t* globals,
                                       uint16_t usage_page,
                                       uint16_t usage,
                                       int32_t value);

// Needed for the GameSir T3s controller when put in iOS mode, which is basically impersonates a
// Xbox Wireless controller with FW 4.8.
bool uni_hid_parser_xboxone_does_name_match(struct uni_hid_device_s* d, const char* name) {
    if (!name)
        return false;

    if (strncmp("Xbox Wireless Controller", name, 25) != 0)
        return false;

    // Fake PID/VID
    uni_hid_device_set_vendor_id(d, XBOX_WIRELESS_VID);
    uni_hid_device_set_product_id(d, XBOX_WIRELESS_PID);
    // Fake HID
    uni_hid_device_set_hid_descriptor(d, xbox_hid_descriptor_4_8_fw, sizeof(xbox_hid_descriptor_4_8_fw));
    return true;
}

void uni_hid_parser_xboxone_setup(uni_hid_device_t* d) {
    xboxone_instance_t* ins = get_xboxone_instance(d);
    // FIXME: Parse HID descriptor and see if it supports 0xf buttons. Checking
    // for the len is a horrible hack.
    if (d->hid_descriptor_len > 330) {
        logi("Xbox: Assuming it is firmware v4.8 or v5.x\n");
        ins->version = XBOXONE_FIRMWARE_V4_8;
    } else {
        // If it is really firmware 4.8, it will be set later.
        logi("Xbox: Assuming it is firmware v3.1\n");
        ins->version = XBOXONE_FIRMWARE_V3_1;
    }

    uni_hid_device_set_ready_complete(d);
}

void uni_hid_parser_xboxone_init_report(uni_hid_device_t* d) {
    // Reset old state. Each report contains a full-state.
    uni_controller_t* ctl = &d->controller;
    memset(ctl, 0, sizeof(*ctl));

    ctl->klass = UNI_CONTROLLER_CLASS_GAMEPAD;
}

void uni_hid_parser_xboxone_parse_usage(uni_hid_device_t* d,
                                        hid_globals_t* globals,
                                        uint16_t usage_page,
                                        uint16_t usage,
                                        int32_t value) {
    xboxone_instance_t* ins = get_xboxone_instance(d);
    if (ins->version == XBOXONE_FIRMWARE_V3_1) {
        parse_usage_firmware_v3_1(d, globals, usage_page, usage, value);
    } else {
        // Valid for v4 and v5
        parse_usage_firmware_v4_v5(d, globals, usage_page, usage, value);
    }
}

static void parse_usage_firmware_v3_1(uni_hid_device_t* d,
                                      hid_globals_t* globals,
                                      uint16_t usage_page,
                                      uint16_t usage,
                                      int32_t value) {
    uint8_t hat;
    uni_controller_t* ctl = &d->controller;

    switch (usage_page) {
        case HID_USAGE_PAGE_GENERIC_DESKTOP:
            switch (usage) {
                case HID_USAGE_AXIS_X:
                    ctl->gamepad.axis_x = uni_hid_parser_process_axis(globals, value);
                    break;
                case HID_USAGE_AXIS_Y:
                    ctl->gamepad.axis_y = uni_hid_parser_process_axis(globals, value);
                    break;
                case HID_USAGE_AXIS_Z:
                    ctl->gamepad.brake = uni_hid_parser_process_pedal(globals, value);
                    break;
                case HID_USAGE_AXIS_RX:
                    ctl->gamepad.axis_rx = uni_hid_parser_process_axis(globals, value);
                    break;
                case HID_USAGE_AXIS_RY:
                    ctl->gamepad.axis_ry = uni_hid_parser_process_axis(globals, value);
                    break;
                case HID_USAGE_AXIS_RZ:
                    ctl->gamepad.throttle = uni_hid_parser_process_pedal(globals, value);
                    break;
                case HID_USAGE_HAT:
                    hat = uni_hid_parser_process_hat(globals, value);
                    ctl->gamepad.dpad = uni_hid_parser_hat_to_dpad(hat);
                    break;
                case HID_USAGE_SYSTEM_MAIN_MENU:
                    if (value)
                        ctl->gamepad.misc_buttons |= MISC_BUTTON_SYSTEM;
                    break;
                case HID_USAGE_DPAD_UP:
                case HID_USAGE_DPAD_DOWN:
                case HID_USAGE_DPAD_RIGHT:
                case HID_USAGE_DPAD_LEFT:
                    uni_hid_parser_process_dpad(usage, value, &ctl->gamepad.dpad);
                    break;
                default:
                    logi("Xbox: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
                    break;
            }
            break;

        case HID_USAGE_PAGE_GENERIC_DEVICE_CONTROLS:
            switch (usage) {
                case HID_USAGE_BATTERY_STRENGTH:
                    ctl->battery = value;
                    break;
                default:
                    logi("Xbox: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
                    break;
            }
            break;

        case HID_USAGE_PAGE_BUTTON: {
            switch (usage) {
                case 0x01:  // Button A
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_A;
                    break;
                case 0x02:  // Button B
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_B;
                    break;
                case 0x03:  // Button X
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_X;
                    break;
                case 0x04:  // Button Y
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_Y;
                    break;
                case 0x05:  // Button Left
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_SHOULDER_L;
                    break;
                case 0x06:  // Button Right
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_SHOULDER_R;
                    break;
                case 0x07:  // View button
                    if (value)
                        ctl->gamepad.misc_buttons |= MISC_BUTTON_SELECT;
                    break;
                case 0x08:  // Menu button
                    if (value)
                        ctl->gamepad.misc_buttons |= MISC_BUTTON_START;
                    break;
                case 0x09:  // Thumb left
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_THUMB_L;
                    break;
                case 0x0a:  // Thumb right
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_THUMB_R;
                    break;
                case 0x0f: {
                    // Only available in firmware v4.8 / 5.x
                    xboxone_instance_t* ins = get_xboxone_instance(d);
                    ins->version = XBOXONE_FIRMWARE_V4_8;
                    logi("Xbox: Firmware 4.8 / 5.x detected\n");
                    break;
                }
                default:
                    logi("Xbox: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
                    break;
            }
            break;
        }

        case HID_USAGE_PAGE_CONSUMER:
            // New in Xbox One firmware v4.8
            switch (usage) {
                case HID_USAGE_RECORD:  // FW 5.15.5
                    break;
                case HID_USAGE_AC_BACK:
                    break;
                default:
                    logi("Xbox: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
                    break;
            }
            break;

        // unknown usage page
        default:
            logi("Xbox: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
            break;
    }
}

// v4.8 / 5.x are almost identical to the Android mappings.
static void parse_usage_firmware_v4_v5(uni_hid_device_t* d,
                                       hid_globals_t* globals,
                                       uint16_t usage_page,
                                       uint16_t usage,
                                       int32_t value) {
    uint8_t hat;
    uni_controller_t* ctl = &d->controller;

    xboxone_instance_t* ins = get_xboxone_instance(d);

    switch (usage_page) {
        case HID_USAGE_PAGE_GENERIC_DESKTOP:
            switch (usage) {
                case HID_USAGE_AXIS_X:
                    ctl->gamepad.axis_x = uni_hid_parser_process_axis(globals, value);
                    break;
                case HID_USAGE_AXIS_Y:
                    ctl->gamepad.axis_y = uni_hid_parser_process_axis(globals, value);
                    break;
                case HID_USAGE_AXIS_Z:
                    ctl->gamepad.axis_rx = uni_hid_parser_process_axis(globals, value);
                    break;
                case HID_USAGE_AXIS_RZ:
                    ctl->gamepad.axis_ry = uni_hid_parser_process_axis(globals, value);
                    break;
                case HID_USAGE_HAT:
                    hat = uni_hid_parser_process_hat(globals, value);
                    ctl->gamepad.dpad = uni_hid_parser_hat_to_dpad(hat);
                    break;
                case HID_USAGE_SYSTEM_MAIN_MENU:
                    if (value)
                        ctl->gamepad.misc_buttons |= MISC_BUTTON_SYSTEM;
                    break;
                case HID_USAGE_DPAD_UP:
                case HID_USAGE_DPAD_DOWN:
                case HID_USAGE_DPAD_RIGHT:
                case HID_USAGE_DPAD_LEFT:
                    uni_hid_parser_process_dpad(usage, value, &ctl->gamepad.dpad);
                    break;
                default:
                    logi("Xbox: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
                    break;
            }
            break;

        case HID_USAGE_PAGE_SIMULATION_CONTROLS:
            switch (usage) {
                case 0xc4:  // Accelerator
                    ctl->gamepad.throttle = uni_hid_parser_process_pedal(globals, value);
                    if (ctl->gamepad.throttle >= TRIGGER_BUTTON_THRESHOLD)
                        ctl->gamepad.buttons |= BUTTON_TRIGGER_R;
                    break;
                case 0xc5:  // Brake
                    ctl->gamepad.brake = uni_hid_parser_process_pedal(globals, value);
                    if (ctl->gamepad.brake >= TRIGGER_BUTTON_THRESHOLD)
                        ctl->gamepad.buttons |= BUTTON_TRIGGER_L;
                    break;
                default:
                    logi("Xbox: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
                    break;
            }
            break;

        case HID_USAGE_PAGE_GENERIC_DEVICE_CONTROLS:
            switch (usage) {
                case HID_USAGE_BATTERY_STRENGTH:
                    ctl->battery = value;
                    break;
                default:
                    logi("Xbox: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
                    break;
            }
            break;

        case HID_USAGE_PAGE_BUTTON: {
            switch (usage) {
                case 0x01:  // Button A
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_A;
                    break;
                case 0x02:  // Button B
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_B;
                    break;
                case 0x03:  // Unused
                    break;
                case 0x04:  // Button X
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_X;
                    break;
                case 0x05:  // Button Y
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_Y;
                    break;
                case 0x06:  // Unused
                    break;
                case 0x07:  // Shoulder Left
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_SHOULDER_L;
                    break;
                case 0x08:  // Shoulder Right
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_SHOULDER_R;
                    break;
                case 0x09:  // Unused
                case 0x0a:  // Unused
                    break;
                case 0x0b:  // Unused in v4.8, used in v5.x
                    if (value)
                        ctl->gamepad.misc_buttons |= MISC_BUTTON_SELECT;
                    break;
                case 0x0c:  // Burger button
                    if (value)
                        ctl->gamepad.misc_buttons |= MISC_BUTTON_START;
                    break;
                case 0x0d:  // Xbox button
                    if (value)
                        ctl->gamepad.misc_buttons |= MISC_BUTTON_SYSTEM;
                    break;
                case 0x0e:  // Thumb Left
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_THUMB_L;
                    break;
                case 0x0f:  // Thumb Right
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_THUMB_R;
                    break;
                default:
                    logi("Xbox: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
                    break;
            }
            break;
        }

        case HID_USAGE_PAGE_CONSUMER:
            switch (usage) {
                case HID_USAGE_RECORD:
                    // Model 1914: Share button
                    // Model 1708: reports it but always 0
                    // FW 5.x
                    if (ins->version != XBOXONE_FIRMWARE_V5) {
                        ins->version = XBOXONE_FIRMWARE_V5;
                        logi("Xbox: Assuming it is firmware v5.x\n");
                    }
                    if (value)
                        ctl->gamepad.misc_buttons |= MISC_BUTTON_CAPTURE;
                    break;
                case HID_USAGE_AC_BACK:  // Back in v4.8 (not v5.x)
                    if (value)
                        ctl->gamepad.misc_buttons |= MISC_BUTTON_SELECT;
                    break;
                case HID_USAGE_ASSIGN_SELECTION:
                case HID_USAGE_ORDER_MOVIE:
                case HID_USAGE_MEDIA_SELECT_SECURITY:
                    // Xbox Adaptive Controller
                    // Don't know the purpose of these "usages".
                    break;
                default:
                    logi("Xbox: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
                    break;
            }
            break;

        // unknown usage page
        default:
            logi("Xbox: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
            break;
    }
}

void uni_hid_parser_xboxone_set_rumble(uni_hid_device_t* d, uint8_t value, uint8_t duration) {
    if (d == NULL) {
        loge("Xbox: Invalid device\n");
        return;
    }

    // Actuators for the force feedback (FF).
    enum {
        FF_RIGHT = 1 << 0,
        FF_LEFT = 1 << 1,
        FF_TRIGGER_RIGHT = 1 << 2,
        FF_TRIGGER_LEFT = 1 << 3,
    };

    struct ff_report {
        // Report related
        uint8_t transaction_type;  // type of transaction
        uint8_t report_id;         // report id
        // Force-feedback related
        uint8_t enable_actuators;    // LSB 0-3 for each actuator
        uint8_t force_left_trigger;  // HID descriptor says that it goes from 0-100
        uint8_t force_right_trigger;
        uint8_t force_left;
        uint8_t force_right;
        uint8_t duration;  // unknown unit, 255 is ~second
        uint8_t start_delay;
        uint8_t loop_count;  // how many times "duration" is repeated
    } __attribute__((packed));

    // TODO: It seems that the max value is 127. Double check
    value /= 2;

    struct ff_report ff = {
        .transaction_type = (HID_MESSAGE_TYPE_DATA << 4) | HID_REPORT_TYPE_OUTPUT,
        .report_id = 0x03,  // taken from HID descriptor
        .enable_actuators = FF_RIGHT | FF_LEFT | FF_TRIGGER_LEFT | FF_TRIGGER_RIGHT,
        .force_left_trigger = value,
        .force_right_trigger = value,
        // Don't enable force_left/force_right actuators.
        // They keep vibrating forever on some 8BitDo controllers
        // https://gitlab.com/ricardoquesada/unijoysticle2/-/issues/10
        .force_left = 0,
        .force_right = 0,
        .duration = duration,
        .start_delay = 0,
        .loop_count = 0,
    };

    uni_hid_device_send_intr_report(d, (uint8_t*)&ff, sizeof(ff));
}

void uni_hid_parser_xboxone_device_dump(uni_hid_device_t* d) {
    static const char* versions[] = {
        "v3.1",
        "v4.8",
        "v5.x",
    };
    xboxone_instance_t* ins = get_xboxone_instance(d);
    if (ins->version >= 0 && ins->version < ARRAY_SIZE(versions))
        logi("\tXbox: FW version %s\n", versions[ins->version]);
}

//
// Helpers
//
xboxone_instance_t* get_xboxone_instance(uni_hid_device_t* d) {
    return (xboxone_instance_t*)&d->parser_data[0];
}
