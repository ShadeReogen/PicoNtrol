#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <pico/cyw43_arch.h>
#include <uni.h>
#include "math.h"

#include "hardware/gpio.h"
#include "pico/stdlib.h"

#include "sdkconfig.h"

// Sanity check
#ifndef CONFIG_BLUEPAD32_PLATFORM_CUSTOM
#error "Pico W must use BLUEPAD32_PLATFORM_CUSTOM"
#endif

// Joystick Analog dead zone
#define DEAD_ZONE 150

#define DATA_PIN 0
#define LATCH_PIN 1
#define PULSE_PIN 2

// Declarations
static void update_gamepad(uni_hid_device_t *d);
bool buttonValues[8];

//
// Platform Overrides
//
static void picontrol_init(int argc, const char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    logi("picontrol: init()\n");
}

static void picontrol_on_init_complete(void)
{
    logi("picontrol: on_init_complete()\n");

    // Safe to call "unsafe" functions since they are called from BT thread

    // Start scanning
    uni_bt_enable_new_connections_unsafe(true);

    // Based on runtime condition, you can delete or list the stored BT keys.
    /* if (0)
        uni_bt_del_keys_unsafe();
    else */
    uni_bt_list_keys_unsafe();

    // Turn off LED once init is done.

    // INIT NES PINS TO GPIO OUT
    gpio_init(DATA_PIN);
    gpio_set_dir(DATA_PIN, GPIO_OUT);

    gpio_init(LATCH_PIN);
    gpio_set_dir(LATCH_PIN, GPIO_IN);

    gpio_init(PULSE_PIN);
    gpio_set_dir(PULSE_PIN, GPIO_IN);

    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
}

static void picontrol_on_device_connected(uni_hid_device_t *d)
{
    logi("PicoNtrol: device connected: %p\n", d);
    if (d->report_parser.set_rumble != NULL)
    {
        d->report_parser.set_rumble(d, 0x80, 15);
    }
}

static void picontrol_on_device_disconnected(uni_hid_device_t *d)
{
    logi("PicoNtrol: device disconnected: %p\n", d);
}

static uni_error_t picontrol_on_device_ready(uni_hid_device_t *d)
{
    logi("picontrol: device ready: %p\n", d);

    // You can reject the connection by returning an error.
    return UNI_ERROR_SUCCESS;
}

static void picontrol_on_controller_data(uni_hid_device_t *d, uni_controller_t *ctl)
{

    static uni_controller_t prev = {0};
    uni_gamepad_t *gp;

    if (memcmp(&prev, ctl, sizeof(*ctl)) == 0)
    {
        return;
    }
    prev = *ctl;
    // PRINT FULL DEBUG LOG
    /* logi("(%p) id=%d ", d, uni_hid_device_get_idx_for_instance(d));
    uni_controller_dump(ctl); */

    switch (ctl->klass)
    {
    case UNI_CONTROLLER_CLASS_GAMEPAD:
    {
        gp = &ctl->gamepad;

        buttonValues[0] = (gp->buttons & (1 << 1)) == 0;
        buttonValues[1] = (gp->buttons & (1 << 2)) == 0;
        buttonValues[2] = (gp->misc_buttons & (1 << 1)) == 0;
        buttonValues[3] = (gp->misc_buttons & (1 << 3)) == 0;
        buttonValues[4] = (gp->dpad & (1 << 0)) == 0;
        buttonValues[5] = (gp->dpad & (1 << 1)) == 0;
        buttonValues[6] = (gp->dpad & (1 << 2)) == 0;
        buttonValues[7] = (gp->dpad & (1 << 3)) == 0;

        /* if (!gpio_get(LATCH_PIN))
        {
            gpio_put(DATA_PIN, a);
            sleep_us(6);
            gpio_put(DATA_PIN, b);
            sleep_us(6);
            gpio_put(DATA_PIN, sel);
            sleep_us(6);
            gpio_put(DATA_PIN, start);
            sleep_us(6);
            gpio_put(DATA_PIN, up);
            sleep_us(6);
            gpio_put(DATA_PIN, down);
            sleep_us(6);
            gpio_put(DATA_PIN, left);
            sleep_us(6);
            gpio_put(DATA_PIN, right);
            sleep_us(6);
        } */
    }
    break;
    default:
        loge("Unsupported controller class: %d\n", ctl->klass);
        break;
    }
}

static void on_nes_poll()
{

    for (int i = 0; i < 8; i++)
    {
        gpio_put(DATA_PIN, buttonValues[i]);
        sleep_us(6);
    }
}
static const uni_property_t *picontrol_get_property(uni_property_idx_t idx)
{
    // Deprecated
    ARG_UNUSED(idx);
    return NULL;
}

static void picontrol_on_oob_event(uni_platform_oob_event_t event, void *data)
{
    switch (event)
    {
    case UNI_PLATFORM_OOB_GAMEPAD_SYSTEM_BUTTON:
        // CHANGE CONSOLE ON SYSTEM BUTTON PRESS
        update_gamepad((uni_hid_device_t *)data);
        break;

    case UNI_PLATFORM_OOB_BLUETOOTH_ENABLED:
        // When the "bt scanning" is on / off. Could by triggered by different events
        // Useful to notify the user
        logi("picontrol_on_oob_event: Bluetooth enabled: %d\n", (bool)(data));
        break;

    default:
        logi("picontrol_on_oob_event: unsupported event: 0x%04x\n", event);
    }
}

//
// Helpers
//
static void update_gamepad(uni_hid_device_t *d)
{
    if (d->report_parser.set_rumble != NULL)
    {
        d->report_parser.set_rumble(d, 0xF0 /* value */, 15 /* duration */);
    }

    if (d->report_parser.set_player_leds != NULL)
    {
        static uint8_t led = 0;
        led += 1;
        led &= 0xf;
        d->report_parser.set_player_leds(d, led);
    }

    if (d->report_parser.set_lightbar_color != NULL)
    {
        static uint8_t red = 0x10;
        static uint8_t green = 0x20;
        static uint8_t blue = 0x40;

        red += 0x10;
        green -= 0x20;
        blue += 0x40;
        d->report_parser.set_lightbar_color(d, red, green, blue);
    }
}

//
// Entry Point
//
struct uni_platform *get_picontrol(void)
{
    static struct uni_platform plat = {
        .name = "PicoNtrol",
        .init = picontrol_init,
        .on_init_complete = picontrol_on_init_complete,
        .on_device_connected = picontrol_on_device_connected,
        .on_device_disconnected = picontrol_on_device_disconnected,
        .on_device_ready = picontrol_on_device_ready,
        .on_oob_event = picontrol_on_oob_event,
        .on_controller_data = picontrol_on_controller_data,
        .get_property = picontrol_get_property,
    };

    return &plat;
}
