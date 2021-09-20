/* picokbd.c - Local USB keyboard handling */

#include "pico.h"
#include "pico/stdlib.h"
#include "bsp/board.h"
#include "tusb.h"
#include "class/hid/hid.h"
#include <stdio.h>

#define DEBUG   0

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

static const KEYMAP keymap[] =
    {
    {'1', '!'},     // 0x1E HID_KEY_1
    {'2', '"'},     // 0x1F HID_KEY_2
    {'3', 0x9C},    // 0x20 HID_KEY_3
    {'4', '$'},     // 0x21 HID_KEY_4
    {'5', '%'},     // 0x22 HID_KEY_5
    {'6', '^'},     // 0x23 HID_KEY_6
    {'7', '&'},     // 0x24 HID_KEY_7
    {'8', '*'},     // 0x25 HID_KEY_8
    {'9', '('},     // 0x26 HID_KEY_9
    {'0', ')'},     // 0x27 HID_KEY_0
    {0x0D, 0x0D},   // 0x28 HID_KEY_ENTER
    {0x1B, 0x1B},   // 0x29 HID_KEY_ESCAPE
    {0x08, 0x08},   // 0x2A HID_KEY_BACKSPACE
    {0x09,  155},   // 0x2B HID_KEY_TAB
    {' ', ' '},     // 0x2C HID_KEY_SPACE
    {'-', '_'},     // 0x2D HID_KEY_MINUS
    {'=', '+'},     // 0x2E HID_KEY_EQUAL
    {'[', '{'},     // 0x2F HID_KEY_BRACKET_LEFT
    {']', '}'},     // 0x30 HID_KEY_BRACKET_RIGHT
    {'\\', '|'},    // 0x31 HID_KEY_BACKSLASH
    {'#', '~'},     // 0x32 HID_KEY_EUROPE_1
    {';', ':'},     // 0x33 HID_KEY_SEMICOLON
    {'\'', '@'},    // 0x34 HID_KEY_APOSTROPHE
    {'`', '~'},     // 0x35 HID_KEY_GRAVE
    {',', '<'},     // 0x36 HID_KEY_COMMA
    {'.', '>'},     // 0x37 HID_KEY_PERIOD
    {'/', '?'},     // 0x38 HID_KEY_SLASH
    };

static const uint8_t editmap[] = {
    134,            // 0x49 HID_KEY_INSERT
    130,            // 0x4A HID_KEY_HOME
    132,            // 0x4B HID_KEY_PAGE_UP
    135,            // 0x4C HID_KEY_DELETE
    131,            // 0x4D HID_KEY_END
    133,            // 0x4E HID_KEY_PAGE_DOWN
    137,            // 0x4F HID_KEY_ARROW_RIGHT
    136,            // 0x50 HID_KEY_ARROW_LEFT
    138,            // 0x51 HID_KEY_ARROW_DOWN
    139,            // 0x52 HID_KEY_ARROW_UP
    };

static const KEYMAP padmap[] =
    {
    {0x00, '/'},   // 0x54 HID_KEY_KEYPAD_DIVIDE
    {0x00, '*'},   // 0x55 HID_KEY_KEYPAD_MULTIPLY
    {0x00, '-'},   // 0x56 HID_KEY_KEYPAD_SUBTRACT
    {0x00, '+'},   // 0x57 HID_KEY_KEYPAD_ADD
    {0x0D, 0x0D},  // 0x58 HID_KEY_KEYPAD_ENTER
    { 131, '1'},   // 0x59 HID_KEY_KEYPAD_1
    { 138, '2'},   // 0x5A HID_KEY_KEYPAD_2
    { 133, '3'},   // 0x5B HID_KEY_KEYPAD_3
    { 136, '4'},   // 0x5C HID_KEY_KEYPAD_4
    {0x00, '5'},   // 0x5D HID_KEY_KEYPAD_5
    { 137, '6'},   // 0x5E HID_KEY_KEYPAD_6
    { 130, '7'},   // 0x5F HID_KEY_KEYPAD_7
    { 139, '8'},   // 0x60 HID_KEY_KEYPAD_8
    { 132, '9'},   // 0x61 HID_KEY_KEYPAD_9
    { 134, '0'},   // 0x62 HID_KEY_KEYPAD_0
    { 135, '.'}    // 0x63 HID_KEY_KEYPAD_DECIMAL
    };

static uint8_t keydn[HID_KEY_F20 + 1];
static hid_keyboard_report_t prev_report = {0, 0, {0}}; // previous report to check key released

static void key_press (uint8_t modifier, uint8_t key)
    {
#if DEBUG > 0
    printf ("key_press (0x%02X)\n", key);
#endif
    uint8_t leds = led_flags;
    if ( key <= HID_KEY_F20 ) keydn[key] = 1;
    if ( key < HID_KEY_A )
        {
        key = 0;
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
        }
    else if ( key == HID_KEY_CAPS_LOCK )
        {
        leds ^= KEYBOARD_LED_CAPSLOCK;
        set_leds (leds);
        key = 0;
        }
    else if ( key <= HID_KEY_F12 )
        {
        key = key - HID_KEY_F1 + 145;
        if ( modifier & ( KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT ))
            key += 16;
        if ( modifier & ( KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTCTRL ))
            key += 32;
        }
    else if ( key == HID_KEY_PRINT_SCREEN )
        {
        key = 0;
        }
    else if ( key ==  HID_KEY_SCROLL_LOCK )
        {
        leds ^= KEYBOARD_LED_SCROLLLOCK;
        set_leds (leds);
        key = 0;
        }
    else if ( key == HID_KEY_PAUSE )
        {
        key = 0;
        }
    else if ( key <= HID_KEY_ARROW_UP )
        {
        key = editmap[key - HID_KEY_INSERT];
        }
    else if ( key == HID_KEY_NUM_LOCK )
        {
        leds ^= KEYBOARD_LED_NUMLOCK;
        set_leds (leds);
        key = 0;
        }
    else if ( key <= HID_KEY_KEYPAD_DECIMAL )
        {
        key -= HID_KEY_KEYPAD_DIVIDE;
        if (( modifier & ( KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT ))
            || ( led_flags & KEYBOARD_LED_NUMLOCK ))
            {
            key = padmap[key].shft;
            if ( modifier & ( KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTCTRL ))
                key &= 0x1F;
            }
        else
            {
            key = padmap[key].unsh;
            }
        }
    else if ( key == HID_KEY_APPLICATION )
        {
        key = 9;
        }
    else
        {
        key = 0;
        }
    if (( key >= 130 ) && ( key <= 133 ))
        {
        // Home, End, PageUp, PageDown
        if ( modifier & ( KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTCTRL ))
            key += 26;
        }
    else if (( key == 136 ) || ( key == 137 ))
        {
        // Left, Right
        if ( modifier & ( KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTCTRL ))
            key -= 6;
        else if ( modifier & ( KEYBOARD_MODIFIER_LEFTGUI | KEYBOARD_MODIFIER_RIGHTGUI ))
            key -= 8;
        }
    else if (( key == 138 ) || ( key == 139 ))
        {
        // Down, Up
        if ( modifier & ( KEYBOARD_MODIFIER_LEFTGUI | KEYBOARD_MODIFIER_RIGHTGUI ))
            key ^= 23;
        }
    if ( key > 0 ) putkey (key);
    }

static void key_release (uint8_t key)
    {
#if DEBUG > 0
    printf ("key_release (0x%02X)\n", key);
#endif
    if ( key <= HID_KEY_F20 ) keydn[key] = 0;
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
    for (int i = 0; i < 6; ++i)
        {
        uint8_t key = p_new_report->keycode[i];
        if ( key )
            {
#if DEBUG == 1
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
            key_release (key);
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
#if DEBUG == 1
    printf("A Keyboard device (address %d) is mounted\r\n", dev_addr);
#endif
    tuh_hid_keyboard_get_report(dev_addr, &usb_keyboard_report);
    }

void tuh_hid_keyboard_unmounted_cb(uint8_t dev_addr)
    {
    // application tear-down
#if DEBUG == 1
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
#if DEBUG == 1
        printf ("Keyboard mounted: dev_addr = %d\n", dev_addr);
#endif
    
        _report_count = tuh_hid_parse_report_descriptor(_report_info_arr, MAX_REPORT,
            desc_report, desc_len);
#if DEBUG == 1
        printf ("%d reports defined\n", _report_count);
#endif
        }
    }

// Invoked when device with hid interface is un-mounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t __attribute__((unused)) instance)
    {
#if DEBUG == 1
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
#if DEBUG == 1
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
#if DEBUG == 1
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
#if DEBUG > 0
    printf ("setup_keyboard " __DATE__ " " __TIME__ "\n");
#endif
    memset (keydn, 0, sizeof (keydn));
    tusb_init();
    add_repeating_timer_ms (100, keyboard_periodic, NULL, &s_kbd_timer);
    }

#define HID_KEY_CONTROL_ALL 0xE8
#define HID_KEY_SHIFT_ALL   0xE9
#define HID_KEY_ALT_ALL     0xEA

// Translation table for negative INKEY:
static const uint8_t xkey[] = {
    HID_KEY_SHIFT_ALL,              // -1 (SHIFT)
    HID_KEY_CONTROL_ALL,            // -2 (CTRL)
    HID_KEY_ALT_ALL,                // -3 (ALT)
    HID_KEY_SHIFT_LEFT,             // -4 (left SHIFT)
    HID_KEY_CONTROL_LEFT,           // -5 (left CTRL)
    HID_KEY_ALT_LEFT,               // -6 (left ALT)
    HID_KEY_SHIFT_RIGHT,            // -7 (right SHIFT)
    HID_KEY_CONTROL_RIGHT,          // -8 (right CTRL)
    HID_KEY_ALT_RIGHT,              // -9 (right ALT)
    0,                              // -10 (mouse left button)
    0,                              // -11 (mouse middle button)
    0,                              // -12 (mouse right button)
    0,                              // -13 (CANCEL) HID_KEY_CANCEL
    0,                              // -14
    0,                              // -15
    0,                              // -16
    HID_KEY_Q,                      // -17 (Q)
    HID_KEY_3,                      // -18 (3)
    HID_KEY_4,                      // -19 (4)
    HID_KEY_5,                      // -20 (5)
    HID_KEY_F4,                     // -21 (f4)
    HID_KEY_8,                      // -22 (8)
    HID_KEY_F7,                     // -23 (f7)
    HID_KEY_MINUS,                  // -24 (-)
    0,                              // -25
    HID_KEY_ARROW_LEFT,             // -26 (LEFT)
    HID_KEY_KEYPAD_6,               // -27 (keypad 6)
    HID_KEY_KEYPAD_7,               // -28 (keypad 7)
    HID_KEY_F11,                    // -29 (f11)
    HID_KEY_F12,                    // -30 (f12)
    HID_KEY_F10,                    // -31 (f10)
    HID_KEY_SCROLL_LOCK,            // -32 (SCROLL LOCK)
    HID_KEY_PRINT_SCREEN,           // -33 (PRINT SCREEN)
    HID_KEY_W,                      // -34 (W)
    HID_KEY_E,                      // -35 (E)
    HID_KEY_T,                      // -36 (T)
    HID_KEY_7,                      // -37 (7)
    HID_KEY_I,                      // -38 (I)
    HID_KEY_9,                      // -39 (9)
    HID_KEY_0,                      // -40 (0)
    0,                              // -41
    HID_KEY_ARROW_DOWN,             // -42 (DOWN)
    HID_KEY_KEYPAD_8,               // -43 (keypad 8)
    HID_KEY_KEYPAD_9,               // -44 (keypad 9)
    HID_KEY_PAUSE,                  // -45 (BREAK/PAUSE)
    HID_KEY_GRAVE,                  // -46 (`)
    HID_KEY_EUROPE_1,               // -47 (#)
    HID_KEY_BACKSPACE,              // -48 (BACKSPACE)
    HID_KEY_1,                      // -49 (1)
    HID_KEY_2,                      // -50 (2)
    HID_KEY_D,                      // -51 (D)
    HID_KEY_R,                      // -52 (R)
    HID_KEY_6,                      // -53 (6)
    HID_KEY_U,                      // -54 (U)
    HID_KEY_O,                      // -55 (O)
    HID_KEY_P,                      // -56 (P)
    HID_KEY_BRACKET_LEFT,           // -57 ([)
    HID_KEY_ARROW_UP,               // -58 (UP)
    HID_KEY_KEYPAD_ADD,             // -59 (keypad +)
    HID_KEY_KEYPAD_SUBTRACT,        // -60 (keypad -)
    HID_KEY_KEYPAD_ENTER,           // -61 (keypad ENTER)
    HID_KEY_INSERT,                 // -62 (INSERT)
    HID_KEY_HOME,                   // -63 (HOME)
    HID_KEY_PAGE_UP,                // -64 (PAGE UP)
    HID_KEY_CAPS_LOCK,              // -65 (CAPS LOCK)
    HID_KEY_A,                      // -66 (A)
    HID_KEY_X,                      // -67 (X)
    HID_KEY_F,                      // -68 (F)
    HID_KEY_Y,                      // -69 (Y)
    HID_KEY_J,                      // -70 (J)
    HID_KEY_K,                      // -71 (K)
    0,                              // -72
    0,                              // -73 (SELECT) HID_KEY_SELECT
    HID_KEY_ENTER,                  // -74 (RETURN)
    HID_KEY_KEYPAD_DIVIDE,          // -75 (keypad /)
    0,                              // -76
    HID_KEY_KEYPAD_DECIMAL,         // -77 (keypad .)
    HID_KEY_NUM_LOCK,               // -78 (NUM LOCK)
    HID_KEY_PAGE_DOWN,              // -79 (PAGE DOWN)
    HID_KEY_APOSTROPHE,             // -80 (')
    0,                              // -81 (was SHIFT LOCK)
    HID_KEY_S,                      // -82 (S)
    HID_KEY_C,                      // -83 (C)
    HID_KEY_G,                      // -84 (G)
    HID_KEY_H,                      // -85 (H)
    HID_KEY_N,                      // -86 (N)
    HID_KEY_L,                      // -87 (L)
    HID_KEY_SEMICOLON,              // -88 (;)
    HID_KEY_BRACKET_RIGHT,          // -89 (])
    HID_KEY_DELETE,                 // -90 (DELETE)
    HID_KEY_EUROPE_1,               // -91 (#)
    HID_KEY_KEYPAD_MULTIPLY,        // -92 (keypad *)
    0,                              // -93 (HELP)   HID_KEY_HELP
    HID_KEY_EQUAL,                  // -94 (=)
    0,                              // -95
    0,                              // -96
    HID_KEY_TAB,                    // -97 (TAB)
    HID_KEY_Z,                      // -98 (Z)
    HID_KEY_SPACE,                  // -99 (SPACE)
    HID_KEY_V,                      // -100 (V)
    HID_KEY_B,                      // -101 (B)
    HID_KEY_M,                      // -102 (M)
    HID_KEY_COMMA,                  // -103 (,)
    HID_KEY_PERIOD,                 // -104 (.)
    HID_KEY_SLASH,                  // -105 (/)
    HID_KEY_END,                    // -106 (END) (COPY)
    HID_KEY_KEYPAD_0,               // -107 (keypad 0)
    HID_KEY_KEYPAD_1,               // -108 (keypad 1)
    HID_KEY_KEYPAD_3,               // -109 (keypad 3)
    HID_KEY_GUI_LEFT,               // -110 (left Windows key)
    HID_KEY_GUI_RIGHT,              // -111 (right Windows key)
    HID_KEY_APPLICATION,            // -112 (context menu key)
    HID_KEY_ESCAPE,                 // -113 (ESCAPE)
    HID_KEY_F1,                     // -114 (f1)
    HID_KEY_F2,                     // -115 (f2)
    HID_KEY_F3,                     // -116 (f3)
    HID_KEY_F5,                     // -117 (f5)
    HID_KEY_F6,                     // -118 (f6)
    HID_KEY_F8,                     // -119 (f8)
    HID_KEY_F9,                     // -120 (f9)
    HID_KEY_BACKSLASH,              // -121 (\)
    HID_KEY_ARROW_RIGHT,            // -122 (RIGHT)
    HID_KEY_KEYPAD_4,               // -123 (keypad 4)
    HID_KEY_KEYPAD_5,               // -124 (keypad 5)
    HID_KEY_KEYPAD_2,               // -125 (keypad 2)
    HID_KEY_F13,                    // -126 (f13)
    HID_KEY_F14,                    // -127 (f14)
    HID_KEY_F15                     // -128 (f15)
    };

static const uint8_t modmsk[] = {
    KEYBOARD_MODIFIER_LEFTCTRL,
    KEYBOARD_MODIFIER_LEFTSHIFT,
    KEYBOARD_MODIFIER_LEFTALT,
    KEYBOARD_MODIFIER_LEFTGUI,
    KEYBOARD_MODIFIER_RIGHTCTRL,
    KEYBOARD_MODIFIER_RIGHTSHIFT,
    KEYBOARD_MODIFIER_RIGHTALT,
    KEYBOARD_MODIFIER_RIGHTGUI,
    KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTCTRL,
    KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT,
    KEYBOARD_MODIFIER_LEFTALT | KEYBOARD_MODIFIER_RIGHTALT,
    KEYBOARD_MODIFIER_LEFTGUI | KEYBOARD_MODIFIER_RIGHTGUI
    };

int testkey (int key)
    {
#if DEBUG == 2
    printf ("testkey (%d)\n", key);
#endif
    key = xkey[key-1];
#if DEBUG == 2
    printf ("key = 0x%02X\n", key);
#endif
    if ( key >= HID_KEY_CONTROL_LEFT )
        {
#if DEBUG == 2
        printf ("Modifiers = 0x%02X\n", prev_report.modifier);
#endif
        key = ( prev_report.modifier & modmsk[key - HID_KEY_CONTROL_LEFT] ) ? -1 : 0;
        }
    else if ( key > 0 )
        {
        key = - keydn[key];
        }
#if DEBUG == 2
    printf ("Result = %d\n", key);
#endif
    return key;
    }
