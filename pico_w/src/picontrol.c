#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <pico/cyw43_arch.h>
#include <uni.h>

#include "hardware/gpio.h"
#include "pico/stdlib.h"

#include "sdkconfig.h"

// Sanity check
#ifndef CONFIG_BLUEPAD32_PLATFORM_CUSTOM
#error "Pico W must use BLUEPAD32_PLATFORM_CUSTOM"
#endif

#define UP_BTN 0
#define DOWN_BTN 1
#define LEFT_BTN 2
#define RIGHT_BTN 3
#define FIRE_BTN 4

// Declarations
static void trigger_event_on_gamepad(uni_hid_device_t *d);

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
    stdio_init_all();
    gpio_init(UP_BTN);
    gpio_init(DOWN_BTN);
    gpio_init(LEFT_BTN);
    gpio_init(RIGHT_BTN);
    gpio_init(FIRE_BTN);

    gpio_set_dir(UP_BTN, GPIO_OUT);
    gpio_set_dir(DOWN_BTN, GPIO_OUT);
    gpio_set_dir(LEFT_BTN, GPIO_OUT);
    gpio_set_dir(RIGHT_BTN, GPIO_OUT);
    gpio_set_dir(FIRE_BTN, GPIO_OUT);

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
    static uint8_t leds = 0;
    static uint8_t enabled = true;
    static uni_controller_t prev = {0};
    uni_gamepad_t *gp;

    if (memcmp(&prev, ctl, sizeof(*ctl)) == 0)
    {
        return;
    }
    prev = *ctl;
    // PRINT FULL DEBUG LOG
    // logi("(%p) id=%d ", d, uni_hid_device_get_idx_for_instance(d));
    // uni_controller_dump(ctl);

    switch (ctl->klass)
    {
    case UNI_CONTROLLER_CLASS_GAMEPAD:
        gp = &ctl->gamepad;

        if (gp->dpad == 0)
        {
            gpio_put(UP_BTN, true);
            gpio_put(DOWN_BTN, true);
            gpio_put(LEFT_BTN, true);
            gpio_put(RIGHT_BTN, true);
        }

        int x = gp->axis_x;
        int y = gp->axis_y;

        // DEBUG: Print Left stick Axis
        //logi("Axis z: %d, Axis y: %d\n", x, y);

        if (gp->dpad != 0 || x > 50 || y > 50 || x < -50 || y < -50)
        {
            if (gp->dpad == 5 || (x > 250 && y < -250))
            {
                gpio_put(RIGHT_BTN, false);
                gpio_put(UP_BTN, false);
                gpio_put(DOWN_BTN, true);
                gpio_put(LEFT_BTN, true);

                logi("Diagonal URX\n");
            }

            else if (gp->dpad == 9 || (x < -250 && y < -250))
            {
                gpio_put(RIGHT_BTN, true);
                gpio_put(UP_BTN, false);
                gpio_put(DOWN_BTN, true);
                gpio_put(LEFT_BTN, false);

                logi("Diagonal ULX\n");
            }
            else if (gp->dpad == 6 || (x > 250 && y > 250))
            {
                gpio_put(RIGHT_BTN, false);
                gpio_put(UP_BTN, true);
                gpio_put(DOWN_BTN, false);
                gpio_put(LEFT_BTN, true);

                logi("Diagonal DRX\n");
            }

            if (gp->dpad == 10 || (x < -250 && y > 250))
            {
                gpio_put(RIGHT_BTN, true);
                gpio_put(UP_BTN, true);
                gpio_put(DOWN_BTN, false);
                gpio_put(LEFT_BTN, false);

                logi("Diagonal DLX\n");
            }
            if (gp->dpad == 1 || y < -480)
            {
                gpio_put(UP_BTN, false);
                gpio_put(DOWN_BTN, true);
                gpio_put(LEFT_BTN, true);
                gpio_put(RIGHT_BTN, true);

                logi("UP\n");
            }
            if (gp->dpad == 2 || y > 480)
            {
                gpio_put(DOWN_BTN, false);
                gpio_put(UP_BTN, true);
                gpio_put(LEFT_BTN, true);
                gpio_put(RIGHT_BTN, true);

                logi("DOWN\n");
            }
            if (gp->dpad == 8 || x < (-480))
            {
                gpio_put(LEFT_BTN, false);
                gpio_put(UP_BTN, true);
                gpio_put(DOWN_BTN, true);
                gpio_put(RIGHT_BTN, true);

                logi("LEFT\n");
            }
            if (gp->dpad == 4 || x > 480)
            {
                gpio_put(RIGHT_BTN, false);
                gpio_put(UP_BTN, true);
                gpio_put(DOWN_BTN, true);
                gpio_put(LEFT_BTN, true);

                logi("RIGHT\n");
            }
        }
        // IGNORE ALL BUTTONS EXCEPT RTrigger and LCRS <- (Lower Cross - X PS, B NIN, A XBX)
        if (gp->buttons != 1 && (gp->buttons != 128))
        {
            gpio_put(FIRE_BTN, true);
        }

        if ((gp->buttons == 128 && gp->throttle > 100) || gp->buttons == 1)
        {
            gpio_put(FIRE_BTN, false);
            logi("FIRE\n");
        }

        // Debugging
        // Axis ry: control rumble
        /* if ((gp->buttons & BUTTON_A) && d->report_parser.set_rumble != NULL) {
            d->report_parser.set_rumble(d, 128, 128);
        }
        // Buttons: Control LEDs On/Off
        if ((gp->buttons & BUTTON_B) && d->report_parser.set_player_leds != NULL) {
            d->report_parser.set_player_leds(d, leds++ & 0x0f);
        }
        // Axis: control RGB color
        if ((gp->buttons & BUTTON_X) && d->report_parser.set_lightbar_color != NULL) {
            uint8_t r = (gp->axis_x * 256) / 512;
            uint8_t g = (gp->axis_y * 256) / 512;
            uint8_t b = (gp->axis_rx * 256) / 512;
            d->report_parser.set_lightbar_color(d, r, g, b);
        } */

        // Toggle Bluetooth connections
        /* if ((gp->buttons & BUTTON_SHOULDER_L) && enabled) {
            logi("*** Disabling Bluetooth connections\n");
            uni_bt_enable_new_connections_safe(false);
            enabled = false;
        }
        if ((gp->buttons & BUTTON_SHOULDER_R) && !enabled) {
            logi("*** Enabling Bluetooth connections\n");
            uni_bt_enable_new_connections_safe(true);
            enabled = true;
        } */
        break;
    default:
        loge("Unsupported controller class: %d\n", ctl->klass);
        break;
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
        // Optional: do something when "system" button gets pressed.
        trigger_event_on_gamepad((uni_hid_device_t *)data);
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
static void trigger_event_on_gamepad(uni_hid_device_t *d)
{
    // if (d->report_parser.set_rumble != NULL) {
    // d->report_parser.set_rumble(d, 0x80 /* value */, 15 /* duration */);
    //}

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
