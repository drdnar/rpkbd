#ifndef KEYBOARD_SCAN_H
#define KEYBOARD_SCAN_H
#include <stdbool.h>
#include <stdint.h>

#define NUM_LOCK_PIN 24                     /** Num lock LED */
#define CAPS_LOCK_PIN 25                    /** Caps lock LED */
#define SCROLL_LOCK_PIN 26                  /** Scroll lock LED */
#define TEST_LED SCROLL_LOCK_PIN

#define HID_KEY_ROLL_OVER 1                 /** HID code for key roll over error. */

#define KEYBOARD_SCAN_ALARM_NUM 0           /** Alarm number that the keyboard scan timer uses. */

/**
 * @brief Initialize keyboard matrix.
 */
void keyboard_init_matrix(void);

/**
 * @brief Turns off the keyboard hardware.
 */
void keyboard_deinit_matrix(void);

/**
 * @brief Sets the keyboard's LEDs.
 * 
 * @param kbd_leds HID report byte for keyboard LEDs.
 */
void keyboard_set_leds(uint8_t kbd_leds);

/**
 * @brief Returns true if the matrix is currently being scanned.
 */
bool scan_in_progress(void);

/**
 * @brief Internally used for check_scan_complete_flag() and clear_scan_complete_flag().
 */
extern volatile bool keyboard_scan_complete_flag;

/**
 * @brief Completion of a scan cycle sets this flag.  It remains set until cleared by clear_scan_complete_flag().
 * 
 * This is used to allow the main thread to track scan cycles.
 */
static inline bool check_scan_complete_flag(void) { return keyboard_scan_complete_flag; }

/**
 * @brief Clears check_scan_complete_flag().
 */
static inline void clear_scan_complete_flag(void) { keyboard_scan_complete_flag = false; }

/**
 * @brief Scan keyboard matrix.
 * 
 * Processes debounced keyboard state, but no reports are sent until keyboard_send_next_report().
 */
void keyboard_scan_matrix(void);

/**
 * @brief Start sending, or continue sending, keyboard reports.
 */
void keyboard_send_next_report(void);

/**
 * @brief Checks if the keyboard report sending cycle is done.
 */
bool keyboard_reports_done(void);

typedef uint8_t hid_key_t;                  /** Type of an HID keyboard code. */
#define MATRIX_COLUMNS 16                   /** Number of columns in matrix. */
#define MATRIX_COLUMN_FIRST_PIN 0           /** GPIO of first pin in columns block. */
#define MATRIX_COLUMN_MASK 0xFFFF           /** GPIO mask for columns. */
#define MATRIX_ROWS 8                       /** Number of rows in matrix. */
#define MATRIX_ROW_FIRST_PIN 16             /** GPIO of first pin in row block. */
#define MATRIX_ROW_MASK 0xFF0000            /** GPIO mask for rows. */
#define MATRIX_LEDS_MASK 0x7000000          /** GPIO mask for LEDs. */
#define MATRIX_NUM_LOCK_PIN 24              /** GPIO pin for num lock. */
#define MATRIX_NUM_LOCK_MASK 0x1000000      /** GPIO mask for num lock. */
#define MATRIX_CAPS_LOCK_PIN 25             /** GPIO pin for caps lock. */
#define MATRIX_CAPS_LOCK_MASK 0x2000000     /** GPIO mask for caps lock. */
#define MATRIX_SCROLL_LOCK_PIN 26           /** GPIO pin for scroll lock. */
#define MATRIX_SCROLL_LOCK_MASK 0x4000000   /** GPIO mask for scroll lock. */

/**
 * @brief Maps locations in the keyboard matrix to HID codes.
 */
extern const hid_key_t key_map[MATRIX_ROWS][MATRIX_COLUMNS];

#endif /* KEYBOARD_SCAN_H */
