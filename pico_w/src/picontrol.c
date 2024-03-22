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

#define UP_BTN 0
#define DOWN_BTN 1
#define LEFT_BTN 2
#define RIGHT_BTN 3
#define FIRE_BTN 4
#define INPUT_A 5 // Paddle A /Touch Tablet < ---   No idea how tf these work. Can't test them
#define INPUT_B 6 // Paddle B /Touch Tablet < ---   I just know that the extra inputs are mapped to this.

// Declarations
static void update_gamepad(uni_hid_device_t *d);

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

    /*
    TODO:
    Inactivce as I have no clue how to implement these.
    What do they do? --> Need Paddle /Tablet

    gpio_init(INPUT_A);
    gpio_init(INPUT_B);
    gpio_set_dir(INPUT_A, GPIO_OUT);
    gpio_set_dir(INPUT_B, GPIO_OUT);

    */

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
    /* logi("(%p) id=%d ", d, uni_hid_device_get_idx_for_instance(d));
    uni_controller_dump(ctl); */

    switch (ctl->klass)
    {
    case UNI_CONTROLLER_CLASS_GAMEPAD:
    {
        gp = &ctl->gamepad;
        bool up = true, down = true, left = true, right = true;

        if (gp->dpad == 0)
        {
            gpio_put(UP_BTN, up);
            gpio_put(DOWN_BTN, down);
            gpio_put(LEFT_BTN, left);
            gpio_put(RIGHT_BTN, right);
        }

        int x = gp->axis_x;
        int y = gp->axis_y;

        /*
         * I set the Dpad to have a higher priority than the analog stick
         *
         * Why?
         * It is improbable that a user wants to input direction with both at the same time, so
         * since accidental stick movement is more likely to happen compared to accidental dpad
         * presses, the dpad input is prioritized.
         * Feel free to change this and recompile or open an issue to discuss this.
         *
         * NdP: This if-else-hell approach is chosen mindfully, to reduce input lag.
         */
        if (gp->dpad != 0)
        {
            if (gp->dpad == 5)
            {
                right = false;
                up = false;
                logi("Diagonal URX\n");
            }
            else if (gp->dpad == 9)
            {
                left = false;
                up = false;
                logi("Diagonal ULX\n");
            }
            else if (gp->dpad == 6)
            {
                right = false;
                down = false;
                logi("Diagonal DRX\n");
            }
            else if (gp->dpad == 10)
            {
                left = false;
                down = false;
                logi("Diagonal DLX\n");
            }
            else if (gp->dpad == 1)
            {
                up = false;
                down = true;
                left = true;
                right = true;
                logi("UP\n");
            }
            else if (gp->dpad == 2)
            {
                down = false;
                up = true;
                left = true;
                right = true;
                logi("DOWN\n");
            }
            else if (gp->dpad == 8)
            {
                left = false;
                up = true;
                down = true;
                right = true;
                logi("LEFT\n");
            }
            else if (gp->dpad == 4)
            {
                right = false;
                up = true;
                down = true;
                left = true;
                logi("RIGHT\n");
            }
        }

        else if (!(fabs(x) < DEAD_ZONE && fabs(y) < DEAD_ZONE))
        {
            /*
             * Cool nerdy math function to nail every analog position correctly 😎
             */
            double angle = atan2(y, x) * (180.0 / M_PI);

            // Adjust angle to be positive
            if (angle < 0)
            {
                angle += 360.0;
            }

            if (angle >= 22.5 && angle < 67.5)
            {
                right = false;
                down = false;
                logi("Down-Right\n");
            }
            else if (angle >= 67.5 && angle < 112.5)
            {
                down = false;
                up = true;
                left = true;
                right = true;
                logi("Down\n");
            }
            else if (angle >= 112.5 && angle < 157.5)
            {
                left = false;
                down = false;
                logi("Down-Left\n");
            }
            else if (angle >= 157.5 && angle < 202.5)
            {
                left = false;
                up = true;
                down = true;
                right = true;
                logi("Left\n");
            }
            else if (angle >= 202.5 && angle < 247.5)
            {
                left = false;
                up = false;
                logi("Up-Left\n");
            }
            else if (angle >= 247.5 && angle < 292.5)
            {
                up = false;
                down = true;
                left = true;
                right = true;
                logi("Up\n");
            }
            else if (angle >= 292.5 && angle < 337.5)
            {
                right = false;
                up = false;
                logi("Up-Right\n");
            }
            else
            {
                right = false;
                up = true;
                down = true;
                left = true;
                logi("Right\n");
            }
        }

        // Handle button presses
        if (gp->buttons == 0)
        {
            gpio_put(FIRE_BTN, true);
        }

        if (gp->buttons == 2)
        {
            up = false;
            logi("UP\n");
        }

        if ((gp->buttons == 128 && gp->throttle > 100) || gp->buttons == 1)
        {
            gpio_put(FIRE_BTN, false);
            logi("FIRE\n");
        }

        // Set GPIOs based on direction
        gpio_put(UP_BTN, up);
        gpio_put(DOWN_BTN, down);
        gpio_put(LEFT_BTN, left);
        gpio_put(RIGHT_BTN, right);
    }
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
