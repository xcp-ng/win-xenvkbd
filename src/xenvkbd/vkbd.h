/* Copyright (c) Citrix Systems Inc.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, 
 * with or without modification, are permitted provided 
 * that the following conditions are met:
 * 
 * *   Redistributions of source code must retain the above 
 *     copyright notice, this list of conditions and the 
 *     following disclaimer.
 * *   Redistributions in binary form must reproduce the above 
 *     copyright notice, this list of conditions and the 
 *     following disclaimer in the documentation and/or other 
 *     materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND 
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR 
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 */

#ifndef _XENVKBD_VKBD_H
#define _XENVKBD_VKBD_H

#include <ntddk.h>
#include <hidport.h>

typedef struct _XENVKBD_HID_KEYBOARD {
    UCHAR   ReportId; // = 1
    UCHAR   Modifiers;
    UCHAR   Keys[6];
} XENVKBD_HID_KEYBOARD;

typedef struct _XENVKBD_HID_ABSMOUSE {
    UCHAR   ReportId; // = 2
    UCHAR   Buttons;
    USHORT  X;
    USHORT  Y;
    CHAR    dZ;
} XENVKBD_HID_ABSMOUSE;

static const UCHAR VkbdReportDescriptor[] = {
    /* ReportId 1 : Keyboard                                               */
    0x05, 0x01,         /* USAGE_PAGE (Generic Desktop)                    */
    0x09, 0x06,         /* USAGE (Keyboard 6)                              */
    0xa1, 0x01,         /* COLLECTION (Application)                        */
    0x85, 0x01,         /*   REPORT_ID (1)                                 */
    0x05, 0x07,         /*   USAGE_PAGE (Keyboard)                         */
    0x19, 0xe0,         /*   USAGE_MINIMUM (Keyboard LeftControl)          */
    0x29, 0xe7,         /*   USAGE_MAXIMUM (Keyboard Right GUI)            */
    0x15, 0x00,         /*   LOGICAL_MINIMUM (0)                           */
    0x25, 0x01,         /*   LOGICAL_MAXIMUM (1)                           */
    0x75, 0x01,         /*   REPORT_SIZE (1)                               */
    0x95, 0x08,         /*   REPORT_COUNT (8)                              */
    0x81, 0x02,         /*   INPUT (Data,Var,Abs)                          */
    0x95, 0x06,         /*   REPORT_COUNT (6)                              */
    0x75, 0x08,         /*   REPORT_SIZE (8)                               */
    0x15, 0x00,         /*   LOGICAL_MINIMUM (0)                           */
    0x25, 0x65,         /*   LOGICAL_MAXIMUM (101)                         */
    0x05, 0x07,         /*   USAGE_PAGE (Keyboard)                         */
    0x19, 0x00,         /*   USAGE_MINIMUM (Reserved (no event indicated)) */
    0x29, 0x65,         /*   USAGE_MAXIMUM (Keyboard Application)          */
    0x81, 0x00,         /*   INPUT (Data,Ary,Abs)                          */
    0xc0,               /* END_COLLECTION                                  */
    /* Report Id 2 : Absolute Mouse                                        */
    0x05, 0x01,         /* USAGE_PAGE (Generic Desktop)                    */
    0x09, 0x02,         /* USAGE (Mouse 2)                                 */
    0xa1, 0x01,         /* COLLECTION (Application)                        */
    0x85, 0x02,         /*   REPORT_ID (2)                                 */
    0x09, 0x01,         /*   USAGE (Pointer)                               */
    0xa1, 0x00,         /*   COLLECTION (Physical)                         */
    0x05, 0x09,         /*     USAGE_PAGE (Button)                         */
    0x19, 0x01,         /*     USAGE_MINIMUM (Button 1)                    */
    0x29, 0x05,         /*     USAGE_MAXIMUM (Button 5)                    */
    0x15, 0x00,         /*     LOGICAL_MINIMUM (0)                         */
    0x25, 0x01,         /*     LOGICAL_MAXIMUM (1)                         */
    0x95, 0x05,         /*     REPORT_COUNT (5)                            */
    0x75, 0x01,         /*     REPORT_SIZE (1)                             */
    0x81, 0x02,         /*     INPUT (Data,Var,Abs)                        */
    0x95, 0x01,         /*     REPORT_COUNT (1)                            */
    0x75, 0x03,         /*     REPORT_SIZE (3)                             */
    0x81, 0x03,         /*     INPUT (Cnst,Var,Abs)                        */
    0x05, 0x01,         /*     USAGE_PAGE (Generic Desktop)                */
    0x09, 0x30,         /*     USAGE (X)                                   */
    0x09, 0x31,         /*     USAGE (Y)                                   */
    0x16, 0x00, 0x00,   /*     LOGICAL_MINIMUM (0)                         */
    0x26, 0xff, 0x7f,   /*     LOGICAL_MAXIMUM (32767)                     */
    0x75, 0x10,         /*     REPORT_SIZE (16)                            */
    0x95, 0x02,         /*     REPORT_COUNT (2)                            */
    0x81, 0x02,         /*     INPUT (Data,Var,Abs)                        */
    0x09, 0x38,         /*     USAGE (Z)                                   */
    0x15, 0x81,         /*     LOGICAL_MINIMUM (-127)                      */
    0x25, 0x7f,         /*     LOGICAL_MAXIMUM (127)                       */
    0x75, 0x08,         /*     REPORT_SIZE (8)                             */
    0x95, 0x01,         /*     REPORT_COUNT (1)                            */
    0x81, 0x06,         /*     INPUT (Data,Var,Rel)                        */
    0xc0,               /*   END_COLLECTION                                */
    0xc0                /* END_COLLECTION                                  */

};

static const HID_DESCRIPTOR VkbdDeviceDescriptor = {
    sizeof(HID_DESCRIPTOR),
    0x09,
    0x0101,
    0x00,
    0x01,
    { 0x22, sizeof(VkbdReportDescriptor) }
};

static const HID_DEVICE_ATTRIBUTES VkbdDeviceAttributes = {
    sizeof(HID_DEVICE_ATTRIBUTES),
    0xF001, // Random Vendor ID - this may need changing to a valid USBIF designation
    0xF001, // Random Product ID
    0x0101
};

// Linux keycode definitions
#include <linux-keycodes.h>

#define DEFINE_USAGE_TABLE                      \
    DEFINE_USAGE(KEY_RESERVED, 0x00),           \
    DEFINE_USAGE(KEY_ESC, 0x29),                \
    DEFINE_USAGE(KEY_1, 0x1E),                  \
    DEFINE_USAGE(KEY_2, 0x1F),                  \
    DEFINE_USAGE(KEY_3, 0x20),                  \
    DEFINE_USAGE(KEY_4, 0x21),                  \
    DEFINE_USAGE(KEY_5, 0x22),                  \
    DEFINE_USAGE(KEY_6, 0x23),                  \
    DEFINE_USAGE(KEY_7, 0x24),                  \
    DEFINE_USAGE(KEY_8, 0x25),                  \
    DEFINE_USAGE(KEY_9, 0x26),                  \
    DEFINE_USAGE(KEY_0, 0x27),                  \
    DEFINE_USAGE(KEY_MINUS, 0x2D),              \
    DEFINE_USAGE(KEY_EQUAL, 0x2E),              \
    DEFINE_USAGE(KEY_BACKSPACE, 0x2A),          \
    DEFINE_USAGE(KEY_TAB, 0x2B),                \
    DEFINE_USAGE(KEY_Q, 0x14),                  \
    DEFINE_USAGE(KEY_W, 0x1A),                  \
    DEFINE_USAGE(KEY_E, 0x08),                  \
    DEFINE_USAGE(KEY_R, 0x15),                  \
    DEFINE_USAGE(KEY_T, 0x17),                  \
    DEFINE_USAGE(KEY_Y, 0x1C),                  \
    DEFINE_USAGE(KEY_U, 0x18),                  \
    DEFINE_USAGE(KEY_I, 0x0C),                  \
    DEFINE_USAGE(KEY_O, 0x12),                  \
    DEFINE_USAGE(KEY_P, 0x13),                  \
    DEFINE_USAGE(KEY_LEFTBRACE, 0x2F),          \
    DEFINE_USAGE(KEY_RIGHTBRACE, 0x30),         \
    DEFINE_USAGE(KEY_ENTER, 0x28),              \
    DEFINE_USAGE(KEY_LEFTCTRL, 0xE0),           \
    DEFINE_USAGE(KEY_A, 0x04),                  \
    DEFINE_USAGE(KEY_S, 0x16),                  \
    DEFINE_USAGE(KEY_D, 0x07),                  \
    DEFINE_USAGE(KEY_F, 0x09),                  \
    DEFINE_USAGE(KEY_G, 0x0A),                  \
    DEFINE_USAGE(KEY_H, 0x0B),                  \
    DEFINE_USAGE(KEY_J, 0x0D),                  \
    DEFINE_USAGE(KEY_K, 0x0E),                  \
    DEFINE_USAGE(KEY_L, 0x0F),                  \
    DEFINE_USAGE(KEY_SEMICOLON, 0x33),          \
    DEFINE_USAGE(KEY_APOSTROPHE, 0x34),         \
    DEFINE_USAGE(KEY_GRAVE, 0x35),              \
    DEFINE_USAGE(KEY_LEFTSHIFT, 0xE1),          \
    DEFINE_USAGE(KEY_BACKSLASH, 0x31),          \
    DEFINE_USAGE(KEY_Z, 0x1D),                  \
    DEFINE_USAGE(KEY_X, 0x1B),                  \
    DEFINE_USAGE(KEY_C, 0x06),                  \
    DEFINE_USAGE(KEY_V, 0x19),                  \
    DEFINE_USAGE(KEY_B, 0x05),                  \
    DEFINE_USAGE(KEY_N, 0x11),                  \
    DEFINE_USAGE(KEY_M, 0x10),                  \
    DEFINE_USAGE(KEY_COMMA, 0x36),              \
    DEFINE_USAGE(KEY_DOT, 0x37),                \
    DEFINE_USAGE(KEY_SLASH, 0x38),              \
    DEFINE_USAGE(KEY_RIGHTSHIFT, 0xE5),         \
    DEFINE_USAGE(KEY_KPASTERISK, 0x55),         \
    DEFINE_USAGE(KEY_LEFTALT, 0xE2),            \
    DEFINE_USAGE(KEY_SPACE, 0x2C),              \
    DEFINE_USAGE(KEY_CAPSLOCK, 0x39),           \
    DEFINE_USAGE(KEY_F1, 0x3A),                 \
    DEFINE_USAGE(KEY_F2, 0x3B),                 \
    DEFINE_USAGE(KEY_F3, 0x3C),                 \
    DEFINE_USAGE(KEY_F4, 0x3D),                 \
    DEFINE_USAGE(KEY_F5, 0x3E),                 \
    DEFINE_USAGE(KEY_F6, 0x3F),                 \
    DEFINE_USAGE(KEY_F7, 0x40),                 \
    DEFINE_USAGE(KEY_F8, 0x41),                 \
    DEFINE_USAGE(KEY_F9, 0x42),                 \
    DEFINE_USAGE(KEY_F10, 0x43),                \
    DEFINE_USAGE(KEY_NUMLOCK, 0x53),            \
    DEFINE_USAGE(KEY_SCROLLLOCK, 0x47),         \
    DEFINE_USAGE(KEY_KP7, 0x5F),                \
    DEFINE_USAGE(KEY_KP8, 0x60),                \
    DEFINE_USAGE(KEY_KP9, 0x61),                \
    DEFINE_USAGE(KEY_KPMINUS, 0x56),            \
    DEFINE_USAGE(KEY_KP4, 0x5C),                \
    DEFINE_USAGE(KEY_KP5, 0x5D),                \
    DEFINE_USAGE(KEY_KP6, 0x5E),                \
    DEFINE_USAGE(KEY_KPPLUS, 0x57),             \
    DEFINE_USAGE(KEY_KP1, 0x59),                \
    DEFINE_USAGE(KEY_KP2, 0x5A),                \
    DEFINE_USAGE(KEY_KP3, 0x5B),                \
    DEFINE_USAGE(KEY_KP0, 0x62),                \
    DEFINE_USAGE(KEY_KPDOT, 0x63),              \
    DEFINE_USAGE(KEY_ZENKAKUHANKAKU, 0x8F),     \
    DEFINE_USAGE(KEY_102ND, 0x64),              \
    DEFINE_USAGE(KEY_F11, 0x44),                \
    DEFINE_USAGE(KEY_F12, 0x45),                \
    DEFINE_USAGE(KEY_RO, 0x87),                 \
    DEFINE_USAGE(KEY_KATAKANA, 0x88),           \
    DEFINE_USAGE(KEY_HIRAGANA, 0x8A),           \
    DEFINE_USAGE(KEY_HENKAN, 0x8B),             \
    DEFINE_USAGE(KEY_KATAKANAHIRAGANA, 0x8C),   \
    DEFINE_USAGE(KEY_MUHENKAN, 0x8D),           \
    DEFINE_USAGE(KEY_KPJPCOMMA, 0x8E),          \
    DEFINE_USAGE(KEY_KPENTER, 0x58),            \
    DEFINE_USAGE(KEY_RIGHTCTRL, 0xE4),          \
    DEFINE_USAGE(KEY_KPSLASH, 0x54),            \
    DEFINE_USAGE(KEY_SYSRQ, 0x46),              \
    DEFINE_USAGE(KEY_PAUSE, 0x48),              \
    DEFINE_USAGE(KEY_RIGHTALT, 0xE6),           \
    DEFINE_USAGE(KEY_HOME, 0x4A),               \
    DEFINE_USAGE(KEY_UP, 0x52),                 \
    DEFINE_USAGE(KEY_PAGEUP, 0x4B),             \
    DEFINE_USAGE(KEY_LEFT, 0x50),               \
    DEFINE_USAGE(KEY_RIGHT, 0x4F),              \
    DEFINE_USAGE(KEY_END, 0x4D),                \
    DEFINE_USAGE(KEY_DOWN, 0x51),               \
    DEFINE_USAGE(KEY_PAGEDOWN, 0x4E),           \
    DEFINE_USAGE(KEY_INSERT, 0x49),             \
    DEFINE_USAGE(KEY_DELETE, 0x4C),             \
    DEFINE_USAGE(KEY_MUTE, 0x7F),               \
    DEFINE_USAGE(KEY_VOLUMEDOWN, 0x81),         \
    DEFINE_USAGE(KEY_VOLUMEUP, 0x80),           \
    DEFINE_USAGE(KEY_POWER, 0x66),              \
    DEFINE_USAGE(KEY_KPEQUAL, 0x67),            \
    DEFINE_USAGE(KEY_KPPLUSMINUS, 0x00),        \
    DEFINE_USAGE(KEY_KPCOMMA, 0x85),            \
    DEFINE_USAGE(KEY_HANGEUL, 0x90),            \
    DEFINE_USAGE(KEY_HANJA, 0x91),              \
    DEFINE_USAGE(KEY_YEN, 0x89),                \
    DEFINE_USAGE(KEY_LEFTMETA, 0xE3),           \
    DEFINE_USAGE(KEY_RIGHTMETA, 0xE7)

#endif  // _XENVKBD_VKBD_H
