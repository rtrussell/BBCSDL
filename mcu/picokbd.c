/* picokbd.c - Local USB keyboard handling */

#include "pico.h"
#include "pico/stdlib.h"
#include "bsp/board.h"
#include "tusb.h"
#include "class/hid/hid.h"
#include <stdio.h>

// #define DEBUG

// Defined in bbccon.c
extern int putkey (char key);
// Defined in picovdu.c
extern void video_periodic (void);

static bool bRepeat;
static uint8_t led_flags = 0;

void set_leds (uint8_t leds)
    {
    uint8_t const addr = 1;
    led_flags = leds;

    tusb_control_request_t ledreq = {
        .bmRequestType_bit.recipient = TUSB_REQ_RCPT_INTERFACE,
        .bmRequestType_bit.type = TUSB_REQ_TYPE_CLASS,
        .bmRequestType_bit.direction = TUSB_DIR_OUT,
        .bRequest = HID_REQ_CONTROL_SET_REPORT,
        .wValue = HID_REPORT_TYPE_OUTPUT << 8,
        .wIndex = 0,    // Interface number
        .wLength = sizeof (led_flags)
        };
    
    tuh_control_xfer (addr, &ledreq, &led_flags, NULL);
    }

typedef struct
    {
    uint8_t unsh;
    uint8_t shft;
    } KEYMAP;

static KEYMAP keymap[] =
    {
    {'1', '!'},     // 0x1e
    {'2', '"'},     // 0x1f
    {'3', 0x9C},    // 0x20
    {'4', '$'},     // 0x21
    {'5', '%'},     // 0x22
    {'6', '^'},     // 0x23
    {'7', '&'},     // 0x24
    {'8', '*'},     // 0x25
    {'9', '('},     // 0x26
    {'0', ')'},     // 0x27
    {0x0D, 0x0D},   // 0x28
    {0x1B, 0x1B},   // 0x29
    {0x08, 0x08},   // 0x2a
    {0x09, 0x09},   // 0x2b
    {' ', ' '},     // 0x2c
    {'-', '_'},     // 0x2d
    {'=', '+'},     // 0x2e
    {'[', '{'},     // 0x2f
    {']', '}'},     // 0x30
    {'\\', '|'},    // 0x31
    {'#', '~'},     // 0x32
    {';', ':'},     // 0x33
    {'\'', '@'},    // 0x34
    {'`', '~'},     // 0x35
    {',', '<'},     // 0x36
    {'.', '>'},     // 0x37
    {'/', '?'},     // 0x38
    };

static KEYMAP padmap[] =
    {
    {0x00, '/'},   // 0x54 HID_KEY_KEYPAD_DIVIDE
    {0x00, '*'},   // 0x55 HID_KEY_KEYPAD_MULTIPLY
    {0x00, '-'},   // 0x56 HID_KEY_KEYPAD_SUBTRACT
    {0x00, '+'},   // 0x57 HID_KEY_KEYPAD_ADD
    {0x0D, 0x0D},  // 0x58 HID_KEY_KEYPAD_ENTER
    {0x00, '1'},   // 0x59 HID_KEY_KEYPAD_1
    {0x0A, '2'},   // 0x5A HID_KEY_KEYPAD_2
    {0x00, '3'},   // 0x5B HID_KEY_KEYPAD_3
    {0x08, '4'},   // 0x5C HID_KEY_KEYPAD_4
    {0x00, '5'},   // 0x5D HID_KEY_KEYPAD_5
    {0x09, '6'},   // 0x5E HID_KEY_KEYPAD_6
    {0x00, '7'},   // 0x5F HID_KEY_KEYPAD_7
    {0x0B, '8'},   // 0x60 HID_KEY_KEYPAD_8
    {0x00, '9'},   // 0x61 HID_KEY_KEYPAD_9
    {0x00, '0'},   // 0x62 HID_KEY_KEYPAD_0
    {0x00, '.'}    // 0x63 HID_KEY_KEYPAD_DECIMAL
    };

static struct
    {
    uint8_t hid;
    uint8_t asc;
    } otherkey[] =
    {
    {HID_KEY_DELETE, 0x7F},
    {HID_KEY_ARROW_RIGHT, 0x09},
    {HID_KEY_ARROW_LEFT, 0x08},
    {HID_KEY_ARROW_DOWN, 0x0A},
    {HID_KEY_ARROW_UP, 0x0B}
    };

void key_press (uint8_t modifier, uint8_t key)
    {
#ifdef DEBUG
    printf ("key_press (%d)\n", key);
#endif
    uint8_t leds = led_flags;
    if ( key < HID_KEY_A )
        {
        return;
        }
    else if ( key <= HID_KEY_Z )
        {
        if (( modifier & ( KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT ))
            || ( led_flags & KEYBOARD_LED_CAPSLOCK ))
            {
            key += 'A' - HID_KEY_A;
            }
        else
            {
            key += 'a' - HID_KEY_A;
            }
        if ( modifier & ( KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTCTRL ))
            key &= 0x1F;
        putkey (key);
        }
    else if ( key <= HID_KEY_SLASH )
        {
        key -= HID_KEY_1;
        if ( modifier & ( KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT ))
            {
            key = keymap[key].shft;
            }
        else
            {
            key = keymap[key].unsh;
            }
        if ( modifier & ( KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTCTRL ))
            key &= 0x1F;
        putkey (key);
        }
    else if (( key >= HID_KEY_KEYPAD_DIVIDE ) && ( key <= HID_KEY_KEYPAD_DECIMAL ))
        {
        key -= HID_KEY_KEYPAD_DIVIDE;
        if (( modifier & ( KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT ))
            || ( led_flags & KEYBOARD_LED_NUMLOCK ))
            {
            key = padmap[key].shft;
            }
        else
            {
            key = padmap[key].unsh;
            }
        if ( key )
            {
            if ( modifier & ( KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTCTRL ))
                key &= 0x1F;
            putkey (key);
            }
        }
    else if ( key == HID_KEY_NUM_LOCK )
        {
        leds ^= KEYBOARD_LED_NUMLOCK;
        set_leds (leds);
        }
    else if ( key == HID_KEY_CAPS_LOCK )
        {
        leds ^= KEYBOARD_LED_CAPSLOCK;
        set_leds (leds);
        }
    else if ( key ==  HID_KEY_SCROLL_LOCK )
        {
        leds ^= KEYBOARD_LED_SCROLLLOCK;
        set_leds (leds);
        }
    else
        {
        for (int i = 0; i < sizeof (otherkey) / sizeof (otherkey[0]); ++i)
            {
            if ( key == otherkey[i].hid )
                {
                putkey (otherkey[i].asc);
                break;
                }
            }
        }
    }

// look up new key in previous keys
static inline int find_key_in_report(hid_keyboard_report_t const *p_report, uint8_t keycode)
    {
    for (int i = 0; i < 6; i++)
        {
        if (p_report->keycode[i] == keycode) return i;
        }
    return -1;
    }

static inline void process_kbd_report(hid_keyboard_report_t const *p_new_report)
    {
    static hid_keyboard_report_t prev_report = {0, 0, {0}}; // previous report to check key released
    for (int i = 0; i < 6; ++i)
        {
        uint8_t key = p_new_report->keycode[i];
        if ( key )
            {
#ifdef DEBUG
            printf ("Key %d reported.\n", key);
#endif
            int kp = find_key_in_report(&prev_report, key);
            if ( kp < 0 )
                {
                key_press (p_new_report->modifier, key);
                bRepeat = true;
                }
            else
                {
                prev_report.keycode[kp] = 0;
                }
            }
        }
    for (int i = 0; i < 6; ++i)
        {
        uint8_t key = prev_report.keycode[i];
        if ( key )
            {
            // Key release
            bRepeat = true;
            }
        }
    prev_report = *p_new_report;
    }

#if PICO_SDK_VERSION_MAJOR == 1
#if PICO_SDK_VERSION_MINOR < 2
CFG_TUSB_MEM_SECTION static hid_keyboard_report_t usb_keyboard_report;

void hid_task(void)
	{
    uint8_t const addr = 1;

    if (tuh_hid_keyboard_is_mounted(addr))
		{
        if (!tuh_hid_keyboard_is_busy(addr))
			{
            process_kbd_report(&usb_keyboard_report);
            tuh_hid_keyboard_get_report(addr, &usb_keyboard_report);
			}
		}
	}

void tuh_hid_keyboard_mounted_cb(uint8_t dev_addr)
    {
    // application set-up
#ifdef DEBUG
    printf("A Keyboard device (address %d) is mounted\r\n", dev_addr);
#endif
    tuh_hid_keyboard_get_report(dev_addr, &usb_keyboard_report);
    }

void tuh_hid_keyboard_unmounted_cb(uint8_t dev_addr)
    {
    // application tear-down
#ifdef DEBUG
    printf("A Keyboard device (address %d) is unmounted\r\n", dev_addr);
#endif
    }

// invoked ISR context
void tuh_hid_keyboard_isr(uint8_t dev_addr, xfer_result_t event)
    {
    (void) dev_addr;
    (void) event;
    }

#else   // PICO_SDK_VERSION_MINOR >= 2
void hid_task (void)
    {
    }

// Each HID instance can has multiple reports

#define MAX_REPORT  4
static uint8_t kbd_addr;
static uint8_t _report_count;
static tuh_hid_report_info_t _report_info_arr[MAX_REPORT];

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

// Invoked when device with hid interface is mounted
// Report descriptor is also available for use. tuh_hid_parse_report_descriptor()
// can be used to parse common/simple enough descriptor.
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
    {
    // Interface protocol
    uint8_t const interface_protocol = tuh_hid_interface_protocol(dev_addr, instance);
    if ( interface_protocol == HID_ITF_PROTOCOL_KEYBOARD )
        {
        kbd_addr = dev_addr;
#ifdef DEBUG
        printf ("Keyboard mounted: dev_addr = %d\n", dev_addr);
#endif
    
        _report_count = tuh_hid_parse_report_descriptor(_report_info_arr, MAX_REPORT,
            desc_report, desc_len);
#ifdef DEBUG
        printf ("%d reports defined\n", _report_count);
#endif
        }
    }

// Invoked when device with hid interface is un-mounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t __attribute__((unused)) instance)
    {
#ifdef DEBUG
    printf ("Device %d unmounted\n");
#endif
    if ( dev_addr == kbd_addr )
        {
        kbd_addr = 0;
        }
    }

// Invoked when received report from device via interrupt endpoint
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t __attribute__((unused)) instance,
    uint8_t const* report, uint16_t len)
    {
    if ( dev_addr != kbd_addr ) return;

    uint8_t const rpt_count = _report_count;
    tuh_hid_report_info_t* rpt_info_arr = _report_info_arr;
    tuh_hid_report_info_t* rpt_info = NULL;

    if ( rpt_count == 1 && rpt_info_arr[0].report_id == 0)
        {
        // Simple report without report ID as 1st byte
        rpt_info = &rpt_info_arr[0];
        }
    else
        {
        // Composite report, 1st byte is report ID, data starts from 2nd byte
        uint8_t const rpt_id = report[0];

        // Find report id in the arrray
        for(uint8_t i=0; i<rpt_count; i++)
            {
            if (rpt_id == rpt_info_arr[i].report_id )
                {
                rpt_info = &rpt_info_arr[i];
                break;
                }
            }

        report++;
        len--;
        }

    if (!rpt_info)
        {
#ifdef DEBUG
        printf("Couldn't find the report info for this report !\r\n");
#endif
        return;
        }

    if ( rpt_info->usage_page == HID_USAGE_PAGE_DESKTOP )
        {
        switch (rpt_info->usage)
            {
            case HID_USAGE_DESKTOP_KEYBOARD:
                // Assume keyboard follow boot report layout
                process_kbd_report( (hid_keyboard_report_t const*) report );
                break;

            default:
                break;
            }
        }
    }

#endif  // PICO_SDK_VERSION_MINOR
#endif  // PICO_SDK_VERSION_MAJOR

static struct repeating_timer s_kbd_timer;
static bool keyboard_periodic (struct repeating_timer *prt)
    {
#ifdef DEBUG
    // printf (".");
#endif
    bRepeat = true;
    while ( bRepeat )
        {
        bRepeat = false;
        tuh_task();
        hid_task();
        }
    video_periodic ();
    return true;
    }

void setup_keyboard (void)
    {
#ifdef DEBUG
    printf ("setup_keyboard " __DATE__ " " __TIME__ "\n");
#endif
    tusb_init();
    add_repeating_timer_ms (100, keyboard_periodic, NULL, &s_kbd_timer);
    }
