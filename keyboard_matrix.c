#include "keyboard_matrix.h"
#include "usb_descriptors.h"
#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "tusb.h"

#include "pico/binary_info.h"
bi_decl(bi_pin_mask_with_names(MATRIX_COLUMN_MASK, "Keyboard matrix columns"));
bi_decl(bi_pin_mask_with_names(MATRIX_ROW_MASK, "Keyboard matrix rows"));
bi_decl(bi_1pin_with_name(MATRIX_NUM_LOCK_PIN, "Num lock LED"));
bi_decl(bi_1pin_with_name(MATRIX_CAPS_LOCK_PIN, "Caps lock LED"));
bi_decl(bi_1pin_with_name(MATRIX_SCROLL_LOCK_PIN, "Scroll lock LED"));


#define ENABLE_FN

#ifdef ENABLE_FN
#define HID_USAGE_CONSUMER_PLAY 0x00B0
#define HID_USAGE_CONSUMER_PAUSE 0x00B1
#define HID_USAGE_CONSUMER_FAST_FORWARD 0x00B3
#define HID_USAGE_CONSUMER_REWIND 0x00B4
#endif

#define K(X) HID_KEY_ ## X ,
#define HID_KEY_UNUSED_1 HID_KEY_NONE
#define HID_KEY_UNUSED_2 HID_KEY_NONE
#define HID_KEY_UNUSED_3 HID_KEY_NONE
#define HID_KEY_UNUSED_4 HID_KEY_NONE
#define HID_KEY_UNUSED_5 HID_KEY_NONE
#define HID_KEY_UNUSED_6 HID_KEY_NONE
#define HID_KEY_UNUSED_7 HID_KEY_NONE
#define HID_KEY_FN 0xFE
/* You'll need a diagram of the physical key matrix for this to make any sense. */
const hid_key_t key_map[MATRIX_ROWS][MATRIX_COLUMNS] = 
{
            /*  1 5 9 13                2 6 10 14               3 7 11 15               4 8 12 16 */
    /* 17 */  { K(ESCAPE)               K(PAUSE)                K(F3)                   K(F1)
                K(INSERT)               K(3)                    K(4)                    K(6)
                K(F5)                   K(F7)                   K(F9)                   K(F11)                  
                K(PRINT_SCREEN)         K(ARROW_RIGHT)          K(SCROLL_LOCK)          K(NONE)                 },
    /* 18 */  { K(TAB)                  K(NONE)                 K(DELETE)               K(PAGE_DOWN)
                K(NONE)                 K(E)                    K(T)                    K(U)
                K(BACKSPACE)            K(MINUS)                K(ARROW_DOWN)           K(END)
                K(HOME)                 K(GUI_RIGHT)            K(NONE)                 K(CAPS_LOCK)            },
    /* 19 */  { K(1)                    K(UNUSED_1)             K(NUM_LOCK)             K(PAGE_UP)
                K(NONE)                 K(I)                    K(R)                    K(Y)
                K(EQUAL)                K(9)                    K(0)                    K(KEYPAD_MULTIPLY)
                K(KEYPAD_DIVIDE)        K(ARROW_UP)             K(GUI_LEFT)             K(NONE)                 },
    /* 20 */  { K(Q)                    K(SHIFT_LEFT)           K(KEYPAD_9)             K(2)
                K(NONE)                 K(K)                    K(F)                    K(H)
                K(BRACKET_RIGHT)        K(O)                    K(SEMICOLON)            K(KEYPAD_8)
                K(KEYPAD_7)             K(NONE)                 K(NONE)                 K(CONTROL_LEFT)         },
    /* 21 */  { K(A)                    K(NONE)                 K(KEYPAD_6)             K(W)
                K(ALT_LEFT)             K(D)                    K(G)                    K(J)
                K(BACKSLASH)            K(BRACKET_LEFT)         K(APOSTROPHE)           K(KEYPAD_5)
#ifdef ENABLE_FN
                K(KEYPAD_4)             K(UNUSED_5)             K(KEYPAD_ADD)           K(NONE)                 },
#else
                K(KEYPAD_4)             K(UNUSED_5)             K(UNUSED_3)             K(NONE)                 },
#endif
    /* 22 */  { K(Z)                    K(SHIFT_RIGHT)          K(KEYPAD_DECIMAL)       K(X)
                K(NONE)                 K(COMMA)                K(B)                    K(M)
                K(ENTER)                K(PERIOD)               K(P)                    K(KEYPAD_0)
                K(KEYPAD_1)             K(NONE)                 K(UNUSED_6)             K(CONTROL_RIGHT)        },
    /* 23 */  { K(UNUSED_4)             K(NONE)                 K(KEYPAD_3)             K(S)
                K(ALT_RIGHT)            K(C)                    K(V)                    K(N)
                K(ARROW_LEFT)           K(L)                    K(SLASH)                K(KEYPAD_2)
#ifdef ENABLE_FN
                K(FN)                   K(UNUSED_2)             K(UNUSED_7)             K(NONE)                 },
#else
                K(KEYPAD_ADD)           K(UNUSED_2)             K(UNUSED_7)             K(NONE)                 },
#endif
    /* 24 */  { K(GRAVE)                K(NONE)                 K(KEYPAD_SUBTRACT)      K(F2)
                K(F4)                   K(8)                    K(5)                    K(7)
                K(F6)                   K(F8)                   K(F10)                  K(F12)
                K(APPLICATION)          K(SPACE)                K(KEYPAD_ENTER)         K(NONE)                 },
};
#undef K


bool num_lock = false;                                      /** Num lock LED should be on. */
bool caps_lock = false;                                     /** Caps lock LED should be on. */
bool scroll_lock = false;                                   /** Scroll lock LED should be on. */

bool keyboard_report = false;                               /** True if a keyboard report needs to be or was sent. */
ptrdiff_t keys_active = 0;                                  /** Number of keyboard keys being pressed. */
#define KEYS_MAX 6                                          /** Maximum number of non-modifier keys that can be sent in a report. */
hid_key_t last_keys[KEYS_MAX] = {0, 0, 0, 0, 0, 0};         /** Last set of sent keycodes. */
uint8_t last_modifiers = 0;                                 /** Last set of HID modifiers. */

typedef uint16_t hid_remote_code_t;                         /** Type of a remote control code. */
bool remote_report = false;                                 /** True if a consumer control ("remote control") report needs to be or was sent. */
hid_remote_code_t remote_code = 0;                          /** Remote control code to send. */

uint_fast8_t next_report = 0;                               /** Next HID report ID that needs to be sent. */
bool keyboard_reports_done(void) { return next_report < 1 || next_report >= REPORT_ID_COUNT; }

typedef uint_fast32_t matrix_column_data_t;                 /** Type of a matrix column scan. */
volatile matrix_column_data_t matrix_cache[MATRIX_COLUMNS]; /** Most recent matrix readings. */
volatile matrix_column_data_t key_pressed_cache;            /** Non-zero if any key pressed during scan. */
volatile uint_fast8_t matrix_current_column;                /** Current matrix column being scanned. */
matrix_column_data_t matrix_ghostless[MATRIX_COLUMNS];      /** Most recent readings without any ghosting. */
bool ghosting_detected;                                     /** Flag for extra ghosting logic. */
bool rollover;                                              /** Flag for when too many keys are pressed. */
typedef uint_fast8_t debounce_timer_t;                      /** Type used for debounce timers. */
debounce_timer_t matrix_debounce[MATRIX_ROWS][MATRIX_COLUMNS];/** Per-key debounce timers. */
#define DEBOUNCE_TIME 15                                    /** How many scan cycles a key must be released to register as released. */

absolute_time_t scan_timer;                                 /** Tracks when the next group should be scanned. */
#define SCAN_TIME_INCREMENT 30                              /** How long to wait between each group scan (us). */
#define SCAN_CYCLE_PERIOD 1000                              /** Total duration of each scan cycle (us). */
#define SCAN_GROUPS MATRIX_COLUMNS                          /** Total number of groups (columns) scanned each cycle. */



/**
 * @brief Set the matrix to "idle" mode.
 * Any keypress will trigger a column to read an input, though you won't be able to detect from which row.
 */
static void idle_matrix(void)
{
    matrix_current_column = UINT8_MAX;
    key_pressed_cache = 0;
    for (int i = MATRIX_COLUMN_FIRST_PIN; i < MATRIX_COLUMN_FIRST_PIN + MATRIX_COLUMNS; i++)
        gpio_disable_pulls(i);
    gpio_set_dir_masked(MATRIX_COLUMN_MASK | MATRIX_ROW_MASK, MATRIX_COLUMN_MASK);
    gpio_put_masked(MATRIX_COLUMN_MASK, MATRIX_COLUMN_MASK);
}


/**
 * @brief Check if any key is pressed.
 * Only valid if the matrix is in idle mode.
 * 
 * @return true At least one key is being pressed.
 */
static bool anykey(void)
{
    return (gpio_get_all() & MATRIX_ROW_MASK) != 0;
}


/**
 * @brief Check if the matrix is finished being scanned.
 */
static bool scan_done(void)
{
    return matrix_current_column >= MATRIX_COLUMN_FIRST_PIN + MATRIX_COLUMNS;
}


bool scan_in_progress(void)
{
    return !scan_done();
}


volatile bool keyboard_scan_complete_flag;


/**
 * @brief Sets check_scan_complete_flag() for the main thread.
 */
static inline void set_scan_complete_flag(void)
{
    keyboard_scan_complete_flag = true;
}


static void process_scan(void)
{
    // Copy over results of previous scan before starting next scan, checking for any key ghosting.
    ghosting_detected = false;
    for (int column = MATRIX_COLUMN_FIRST_PIN; column < MATRIX_COLUMN_FIRST_PIN + MATRIX_COLUMNS; column++)
    {
        matrix_column_data_t data = matrix_cache[column - MATRIX_COLUMN_FIRST_PIN];
        matrix_column_data_t coldat = data & MATRIX_COLUMN_MASK;
        matrix_column_data_t rowdat = data & MATRIX_ROW_MASK;
        if ((coldat & MATRIX_COLUMN_MASK ^ (1 << column)) == 0  // No ghosting if the only pulled-up column is the one being asserted actively.
            || !(rowdat & (rowdat - 1)))                        // No ghosting if columns got pulled up but only one row was pulled up.  (No need for zero test because that just means no keys pressed.)
        {
            matrix_ghostless[column - MATRIX_COLUMN_FIRST_PIN] = data;
        }
        else
            ghosting_detected = true;
    }
    // TODO: We can also make some assumptions about key combinations that are unlikely, and therefore disambiguate
    // potential ghosting.  E.g. the same modifier on opposite sides of the keyboard probably isn't being pressed.
    // Process debouncing
    for (int col = 0; col < MATRIX_COLUMNS; col++)
    {
        for (int row = 0; row < MATRIX_ROWS; row++)
        {
            debounce_timer_t* timer = &matrix_debounce[row][col];
            if (matrix_ghostless[col] & (1 << (row + MATRIX_ROW_FIRST_PIN)))
                *timer = DEBOUNCE_TIME;
            else if (*timer)
                (*timer)--;
        }
    }
}



/**
 * @brief Start a matrix scan cycle from the idle_matrix() state.
 * 
 * A fairly large delay (from the CPU's perspective) is needed when scanning the keyboard matrix due to capacitive and
 * inductive effects inside the physical traces of the matrix wiring.  However, scanning the matrix is just a matter of
 * simple GPIO I/Os, so instead of busy-waiting, an interrupt routine does the work.
 */
static void begin_scan(uint alarm_num);


/**
 * @brief Start a matrix scan cycle when the matrix is not idle.
 */
static void next_scan(uint alarm_num);


/**
 * @brief Continues scanning the keyboard matrix.
 */
static void continue_scan(uint alarm_num)
{
    uint_fast8_t column = matrix_current_column;
    matrix_column_data_t data = gpio_get_all();
    matrix_cache[column - MATRIX_COLUMN_FIRST_PIN] = data;
    key_pressed_cache |= data & MATRIX_ROW_MASK;
    gpio_put(column, 0);
    gpio_pull_down(column);
    gpio_set_dir_masked(MATRIX_COLUMN_MASK, 0);
    column = ++matrix_current_column;
    if (scan_done())
    {
        set_scan_complete_flag();
        if (key_pressed_cache)
            hardware_alarm_set_callback(alarm_num, &next_scan);
        else
        {
            hardware_alarm_set_callback(alarm_num, &begin_scan);
            idle_matrix();
        }
        scan_timer = delayed_by_us(scan_timer, SCAN_CYCLE_PERIOD - (SCAN_GROUPS * SCAN_TIME_INCREMENT));
        hardware_alarm_set_target(alarm_num, scan_timer);
        process_scan();
        return;
    }    
    gpio_disable_pulls(column);
    gpio_set_dir(column, 1);
    gpio_put(column, 1);
    scan_timer = delayed_by_us(scan_timer, SCAN_TIME_INCREMENT);
    hardware_alarm_set_target(alarm_num, scan_timer);
}


static void next_scan(uint alarm_num)
{
    hardware_alarm_set_callback(alarm_num, &continue_scan);
    scan_timer = delayed_by_us(scan_timer, SCAN_TIME_INCREMENT);
    hardware_alarm_set_target(alarm_num, scan_timer);
    matrix_current_column = MATRIX_COLUMN_FIRST_PIN;
    gpio_disable_pulls(MATRIX_COLUMN_FIRST_PIN);
    gpio_set_dir_masked(MATRIX_COLUMN_MASK | MATRIX_ROW_MASK, 1 << MATRIX_COLUMN_FIRST_PIN);
    gpio_put(MATRIX_COLUMN_FIRST_PIN, 1);
    for (int i = MATRIX_COLUMN_FIRST_PIN + 1; i < MATRIX_COLUMN_FIRST_PIN + MATRIX_COLUMNS; i++)
        gpio_pull_down(i); // for ghosting detection
    key_pressed_cache = 0;
}


static void begin_scan(uint alarm_num)
{
    if (anykey())
        next_scan(alarm_num);
    else
    {
        set_scan_complete_flag();
        for (int i = 0; i < MATRIX_COLUMNS; i++)
            matrix_cache[i] = 0;
        scan_timer = delayed_by_us(scan_timer, SCAN_CYCLE_PERIOD);
        //update_us_since_boot(&scan_timer, to_us_since_boot(scan_timer) + SCAN_CYCLE_PERIOD);
        hardware_alarm_set_target(alarm_num, scan_timer);
        process_scan();
    }
}


void keyboard_init_matrix(void)
{
    gpio_init_mask(MATRIX_COLUMN_MASK | MATRIX_ROW_MASK | MATRIX_LEDS_MASK);
    gpio_set_dir_masked(MATRIX_COLUMN_MASK | MATRIX_ROW_MASK | MATRIX_LEDS_MASK, MATRIX_LEDS_MASK);
    // Rows are all inputs and stay that way all the time.
    for (int i = MATRIX_ROW_FIRST_PIN; i < MATRIX_ROW_FIRST_PIN + MATRIX_ROWS; i++)
        gpio_pull_down(i);
    // Reduce EMI; this is already the default
    //for (int i = MATRIX_COLUMN_FIRST_PIN; i < MATRIX_COLUMN_FIRST_PIN + MATRIX_COLUMNS; i++)
    //    gpio_set_slew_rate(i, GPIO_SLEW_RATE_SLOW);
    // LEDs are all actively driven.
    // The diagram in the datasheet implies that setting a pad to output does not disable pulls.
    gpio_disable_pulls(MATRIX_NUM_LOCK_PIN);
    gpio_disable_pulls(MATRIX_CAPS_LOCK_PIN);
    gpio_disable_pulls(MATRIX_SCROLL_LOCK_PIN);
    for (int i = 0; i < MATRIX_ROWS; i++)
        for (int j = 0; j < MATRIX_COLUMNS; j++)
            matrix_debounce[i][j] = 0;
    gpio_set_drive_strength(CAPS_LOCK_PIN, GPIO_DRIVE_STRENGTH_12MA);
    gpio_put(CAPS_LOCK_PIN, !num_lock);
    gpio_set_drive_strength(NUM_LOCK_PIN, GPIO_DRIVE_STRENGTH_12MA);
    gpio_put(NUM_LOCK_PIN, !caps_lock);
    gpio_set_drive_strength(SCROLL_LOCK_PIN, GPIO_DRIVE_STRENGTH_12MA);
    gpio_put(SCROLL_LOCK_PIN, !scroll_lock);
    hardware_alarm_claim(KEYBOARD_SCAN_ALARM_NUM);
    hardware_alarm_set_callback(KEYBOARD_SCAN_ALARM_NUM, &begin_scan);
    scan_timer = delayed_by_us(get_absolute_time(), SCAN_TIME_INCREMENT);
    hardware_alarm_set_target(KEYBOARD_SCAN_ALARM_NUM, scan_timer);
    idle_matrix();
}


void keyboard_deinit_matrix(void)
{
    // Cancel scanning
    hardware_alarm_cancel(KEYBOARD_SCAN_ALARM_NUM);
    hardware_alarm_unclaim(KEYBOARD_SCAN_ALARM_NUM);
    // Set all GPIOs to high impedance.
    gpio_set_dir_masked(MATRIX_COLUMN_MASK | MATRIX_ROW_MASK | MATRIX_LEDS_MASK, 0);
    // Enable pull-downs to prevent floating voltages from building up.
    for (int i = MATRIX_COLUMN_FIRST_PIN; i < MATRIX_COLUMN_FIRST_PIN + MATRIX_COLUMNS; i++)
        gpio_pull_down(i);
    for (int i = MATRIX_ROW_FIRST_PIN; i < MATRIX_ROW_FIRST_PIN + MATRIX_ROWS; i++)
        gpio_pull_down(i);
    gpio_pull_down(MATRIX_NUM_LOCK_PIN);
    gpio_pull_down(MATRIX_CAPS_LOCK_PIN);
    gpio_pull_down(MATRIX_SCROLL_LOCK_PIN);
}


void keyboard_set_leds(uint8_t kbd_leds)
{
    gpio_put(CAPS_LOCK_PIN, !(num_lock = !!(kbd_leds & KEYBOARD_LED_CAPSLOCK)));
    gpio_put(NUM_LOCK_PIN, !(caps_lock = !!(kbd_leds & KEYBOARD_LED_NUMLOCK)));
    gpio_put(SCROLL_LOCK_PIN, scroll_lock = !(!!(kbd_leds & KEYBOARD_LED_SCROLLLOCK)));
}


void keyboard_scan_matrix(void)
{
    if (!scan_done())
    {
        printf("aborted scan ");
        return;
    }
#ifdef ENABLE_FN
    bool fn = false;
#endif
    // Generate key codes
    hid_key_t keys[KEYS_MAX] = {0, 0, 0, 0, 0, 0};
    uint8_t modifiers = 0;
    ptrdiff_t key_count = 0;
    rollover = false;
    for (int col = 0; col < MATRIX_COLUMNS; col++)
    {
        for (int row = 0; row < MATRIX_ROWS; row++)
        {
            if (matrix_debounce[row][col])
            {
                hid_key_t k = key_map[row][col];
                if (k >= HID_KEY_CONTROL_LEFT && k <= HID_KEY_GUI_RIGHT)
                    modifiers |= 1 << (k - HID_KEY_CONTROL_LEFT);
                else if (k != HID_KEY_NONE)
                {
#ifdef ENABLE_FN
                    if (k == HID_KEY_FN)
                        fn = true;
                    else
#endif
                    if (key_count < KEYS_MAX)
                        keys[key_count++] = k;
                    else
                    {
                        key_count++;
                        rollover = true;
                    }
                }
            }
        }
    }
    
#ifdef ENABLE_FN
    hid_remote_code_t new_remote_code = 0;
    if (fn)
    {
        for (int i = 0; i < key_count; i++)
        {
            switch (keys[i])
            {
                case HID_KEY_KEYPAD_7:
                    new_remote_code = HID_USAGE_CONSUMER_SCAN_PREVIOUS;
                    keys[i] = HID_KEY_NONE;
                    break;
                case HID_KEY_KEYPAD_8:
                    new_remote_code = HID_USAGE_CONSUMER_SCAN_NEXT;
                    keys[i] = HID_KEY_NONE;
                    break;
                case HID_KEY_KEYPAD_4:
                    new_remote_code = HID_USAGE_CONSUMER_REWIND;
                    keys[i] = HID_KEY_NONE;
                    break;
                case HID_KEY_KEYPAD_5:
                    new_remote_code = HID_USAGE_CONSUMER_FAST_FORWARD;
                    keys[i] = HID_KEY_NONE;
                    break;
                case HID_KEY_KEYPAD_9:
                    new_remote_code = HID_USAGE_CONSUMER_VOLUME_INCREMENT;
                    keys[i] = HID_KEY_NONE;
                    break;
                case HID_KEY_KEYPAD_6:
                    new_remote_code = HID_USAGE_CONSUMER_VOLUME_DECREMENT;
                    keys[i] = HID_KEY_NONE;
                    break;
                case HID_KEY_KEYPAD_MULTIPLY:
                    new_remote_code = HID_USAGE_CONSUMER_MUTE;
                    keys[i] = HID_KEY_NONE;
                    break;
                case HID_KEY_KEYPAD_1:
                    new_remote_code = HID_USAGE_CONSUMER_PAUSE;
                    keys[i] = HID_KEY_NONE;
                    break;
                case HID_KEY_KEYPAD_2:
                    new_remote_code = HID_USAGE_CONSUMER_PLAY;
                    keys[i] = HID_KEY_NONE;
                    break;
                case HID_KEY_KEYPAD_DECIMAL:
                    new_remote_code = HID_USAGE_CONSUMER_STOP;
                    keys[i] = HID_KEY_NONE;
                    break;
                case HID_KEY_KEYPAD_0:
                case HID_KEY_SPACE:
                    new_remote_code = HID_USAGE_CONSUMER_PLAY_PAUSE;
                    keys[i] = HID_KEY_NONE;
                    break;
            }
        }
    }
#endif

    if (rollover)
        printf("ro %i ", key_count);
    keyboard_report = false;
    // Generate new report if necessary
    for (int i = 0; i < KEYS_MAX; i++)
        if (last_keys[i] != keys[i])
        {
            keyboard_report = true;
            last_keys[i] = keys[i];
        }
    if (modifiers != last_modifiers)
    {
        keyboard_report = true;
        last_modifiers = modifiers;
    }
    if (new_remote_code != remote_code)
    {
        remote_report = true;
        remote_code = new_remote_code;
    }
    else
        remote_report = false;
    if (remote_report)
        next_report = REPORT_ID_CONSUMER_CONTROL;
    if (keyboard_report)
        next_report = REPORT_ID_KEYBOARD;
}


void keyboard_send_next_report(void)
{
    switch (next_report)
    {
        case REPORT_ID_KEYBOARD:
            next_report = REPORT_ID_CONSUMER_CONTROL;
            if (keyboard_report)
            {
                //if (!keys_active)
                //    keyboard_report = false;
                tud_hid_keyboard_report(REPORT_ID_KEYBOARD, last_modifiers, last_keys);
                break;
            }
        //case REPORT_ID_MOUSE:
        case REPORT_ID_CONSUMER_CONTROL:
            next_report = REPORT_ID_COUNT;
            if (remote_report)
            {
                if (!remote_code)
                    remote_report = false;
                tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &remote_code, 2);
                break;
            }
        //case REPORT_ID_GAMEPAD:
        case REPORT_ID_COUNT:
            next_report = 0;
        case 0:
            return;
        default:
            return;
    }
}
