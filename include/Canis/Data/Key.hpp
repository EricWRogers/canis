#pragma once

namespace Canis {
namespace Key
{
    const unsigned int UNKNOWN = 0;

    /**
     *  \name Usage page 0x07
     *
     *  These values are from usage page 0x07 (USB keyboard page).
     */
    /* @{ */

    const unsigned int A = 4;
    const unsigned int B = 5;
    const unsigned int C = 6;
    const unsigned int D = 7;
    const unsigned int E = 8;
    const unsigned int F = 9;
    const unsigned int G = 10;
    const unsigned int H = 11;
    const unsigned int I = 12;
    const unsigned int J = 13;
    const unsigned int K = 14;
    const unsigned int L = 15;
    const unsigned int M = 16;
    const unsigned int N = 17;
    const unsigned int O = 18;
    const unsigned int P = 19;
    const unsigned int Q = 20;
    const unsigned int R = 21;
    const unsigned int S = 22;
    const unsigned int T = 23;
    const unsigned int U = 24;
    const unsigned int V = 25;
    const unsigned int W = 26;
    const unsigned int X = 27;
    const unsigned int Y = 28;
    const unsigned int Z = 29;

    const unsigned int ALPHA1 = 30;
    const unsigned int ALPHA2 = 31;
    const unsigned int ALPHA3 = 32;
    const unsigned int ALPHA4 = 33;
    const unsigned int ALPHA5 = 34;
    const unsigned int ALPHA6 = 35;
    const unsigned int ALPHA7 = 36;
    const unsigned int ALPHA8 = 37;
    const unsigned int ALPHA9 = 38;
    const unsigned int ALPHA0 = 39;

    const unsigned int RETURN = 40;
    const unsigned int ESCAPE = 41;
    const unsigned int BACKSPACE = 42;
    const unsigned int TAB = 43;
    const unsigned int SPACE = 44;

    const unsigned int MINUS = 45;
    const unsigned int EQUALS = 46;
    const unsigned int LEFTBRACKET = 47;
    const unsigned int RIGHTBRACKET = 48;
    const unsigned int BACKSLASH = 49; /**< Located at the lower left of the return
                                  *   key on ISO keyboards and at the right end
                                  *   of the QWERTY row on ANSI keyboards.
                                  *   Produces REVERSE SOLIDUS (backslash) and
                                  *   VERTICAL LINE in a US layout; REVERSE
                                  *   SOLIDUS and VERTICAL LINE in a UK Mac
                                  *   layout; NUMBER SIGN and TILDE in a UK
                                  *   Windows layout; DOLLAR SIGN and POUND SIGN
                                  *   in a Swiss German layout; NUMBER SIGN and
                                  *   APOSTROPHE in a German layout; GRAVE
                                  *   ACCENT and POUND SIGN in a French Mac
                                  *   layout; and ASTERISK and MICRO SIGN in a
                                  *   French Windows layout.
                                  */
    const unsigned int NONUSHASH = 50; /**< ISO USB keyboards actually use this code
                                  *   instead of 49 for the same key; but all
                                  *   OSes I've seen treat the two codes
                                  *   identically. So; as an implementor; unless
                                  *   your keyboard generates both of those
                                  *   codes and your OS treats them differently;
                                  *   you should generate BACKSLASH
                                  *   instead of this code. As a user; you
                                  *   should not rely on this code because SDL
                                  *   will never generate it with most (all?)
                                  *   keyboards.
                                  */
    const unsigned int SEMICOLON = 51;
    const unsigned int APOSTROPHE = 52;
    const unsigned int GRAVE = 53; /**< Located in the top left corner (on both ANSI
                              *   and ISO keyboards). Produces GRAVE ACCENT and
                              *   TILDE in a US Windows layout and in US and UK
                              *   Mac layouts on ANSI keyboards; GRAVE ACCENT
                              *   and NOT SIGN in a UK Windows layout; SECTION
                              *   SIGN and PLUS-MINUS SIGN in US and UK Mac
                              *   layouts on ISO keyboards; SECTION SIGN and
                              *   DEGREE SIGN in a Swiss German layout (Mac:
                              *   only on ISO keyboards); CIRCUMFLEX ACCENT and
                              *   DEGREE SIGN in a German layout (Mac: only on
                              *   ISO keyboards); SUPERSCRIPT TWO and TILDE in a
                              *   French Windows layout; COMMERCIAL AT and
                              *   NUMBER SIGN in a French Mac layout on ISO
                              *   keyboards; and LESS-THAN SIGN and GREATER-THAN
                              *   SIGN in a Swiss German; German; or French Mac
                              *   layout on ANSI keyboards.
                              */
    const unsigned int COMMA = 54;
    const unsigned int PERIOD = 55;
    const unsigned int SLASH = 56;

    const unsigned int CAPSLOCK = 57;

    const unsigned int F1 = 58;
    const unsigned int F2 = 59;
    const unsigned int F3 = 60;
    const unsigned int F4 = 61;
    const unsigned int F5 = 62;
    const unsigned int F6 = 63;
    const unsigned int F7 = 64;
    const unsigned int F8 = 65;
    const unsigned int F9 = 66;
    const unsigned int F10 = 67;
    const unsigned int F11 = 68;
    const unsigned int F12 = 69;

    const unsigned int PRINTSCREEN = 70;
    const unsigned int SCROLLLOCK = 71;
    const unsigned int PAUSE = 72;
    const unsigned int INSERT = 73; /**< insert on PC; help on some Mac keyboards (but
                                   does send code 73; not 117) */
    const unsigned int HOME = 74;
    const unsigned int PAGEUP = 75;
    const unsigned int DELETE = 76;
    const unsigned int END = 77;
    const unsigned int PAGEDOWN = 78;
    const unsigned int RIGHT = 79;
    const unsigned int LEFT = 80;
    const unsigned int DOWN = 81;
    const unsigned int UP = 82;

    const unsigned int NUMLOCKCLEAR = 83; /**< num lock on PC; clear on Mac keyboards
                                     */
    const unsigned int KP_DIVIDE = 84;
    const unsigned int KP_MULTIPLY = 85;
    const unsigned int KP_MINUS = 86;
    const unsigned int KP_PLUS = 87;
    const unsigned int KP_ENTER = 88;
    const unsigned int KP_1 = 89;
    const unsigned int KP_2 = 90;
    const unsigned int KP_3 = 91;
    const unsigned int KP_4 = 92;
    const unsigned int KP_5 = 93;
    const unsigned int KP_6 = 94;
    const unsigned int KP_7 = 95;
    const unsigned int KP_8 = 96;
    const unsigned int KP_9 = 97;
    const unsigned int KP_0 = 98;
    const unsigned int KP_PERIOD = 99;

    const unsigned int NONUSBACKSLASH = 100; /**< This is the additional key that ISO
                                        *   keyboards have over ANSI ones;
                                        *   located between left shift and Z.
                                        *   Produces GRAVE ACCENT and TILDE in a
                                        *   US or UK Mac layout; REVERSE SOLIDUS
                                        *   (backslash) and VERTICAL LINE in a
                                        *   US or UK Windows layout; and
                                        *   LESS-THAN SIGN and GREATER-THAN SIGN
                                        *   in a Swiss German; German; or French
                                        *   layout. */
    const unsigned int APPLICATION = 101; /**< windows contextual menu; compose */
    const unsigned int POWER = 102; /**< The USB document says this is a status flag;
                               *   not a physical key - but some Mac keyboards
                               *   do have a power key. */
    const unsigned int KP_EQUALS = 103;
    const unsigned int F13 = 104;
    const unsigned int F14 = 105;
    const unsigned int F15 = 106;
    const unsigned int F16 = 107;
    const unsigned int F17 = 108;
    const unsigned int F18 = 109;
    const unsigned int F19 = 110;
    const unsigned int F20 = 111;
    const unsigned int F21 = 112;
    const unsigned int F22 = 113;
    const unsigned int F23 = 114;
    const unsigned int F24 = 115;
    const unsigned int EXECUTE = 116;
    const unsigned int HELP = 117;    /**< AL Integrated Help Center */
    const unsigned int MENU = 118;    /**< Menu (show menu) */
    const unsigned int SELECT = 119;
    const unsigned int STOP = 120;    /**< AC Stop */
    const unsigned int AGAIN = 121;   /**< AC Redo/Repeat */
    const unsigned int UNDO = 122;    /**< AC Undo */
    const unsigned int CUT = 123;     /**< AC Cut */
    const unsigned int COPY = 124;    /**< AC Copy */
    const unsigned int PASTE = 125;   /**< AC Paste */
    const unsigned int FIND = 126;    /**< AC Find */
    const unsigned int MUTE = 127;
    const unsigned int VOLUMEUP = 128;
    const unsigned int VOLUMEDOWN = 129;
/* not sure whether there's a reason to enable these */
/*     LOCKINGCAPSLOCK = 130;  */
/*     LOCKINGNUMLOCK = 131; */
/*     LOCKINGSCROLLLOCK = 132; */
    const unsigned int KP_COMMA = 133;
    const unsigned int KP_EQUALSAS400 = 134;

    const unsigned int INTERNATIONAL1 = 135; /**< used on Asian keyboards; see
                                            footnotes in USB doc */
    const unsigned int INTERNATIONAL2 = 136;
    const unsigned int INTERNATIONAL3 = 137; /**< Yen */
    const unsigned int INTERNATIONAL4 = 138;
    const unsigned int INTERNATIONAL5 = 139;
    const unsigned int INTERNATIONAL6 = 140;
    const unsigned int INTERNATIONAL7 = 141;
    const unsigned int INTERNATIONAL8 = 142;
    const unsigned int INTERNATIONAL9 = 143;
    const unsigned int LANG1 = 144; /**< Hangul/English toggle */
    const unsigned int LANG2 = 145; /**< Hanja conversion */
    const unsigned int LANG3 = 146; /**< Katakana */
    const unsigned int LANG4 = 147; /**< Hiragana */
    const unsigned int LANG5 = 148; /**< Zenkaku/Hankaku */
    const unsigned int LANG6 = 149; /**< reserved */
    const unsigned int LANG7 = 150; /**< reserved */
    const unsigned int LANG8 = 151; /**< reserved */
    const unsigned int LANG9 = 152; /**< reserved */

    const unsigned int ALTERASE = 153;    /**< Erase-Eaze */
    const unsigned int SYSREQ = 154;
    const unsigned int CANCEL = 155;      /**< AC Cancel */
    const unsigned int CLEAR = 156;
    const unsigned int PRIOR = 157;
    const unsigned int RETURN2 = 158;
    const unsigned int SEPARATOR = 159;
    const unsigned int OUT = 160;
    const unsigned int OPER = 161;
    const unsigned int CLEARAGAIN = 162;
    const unsigned int CRSEL = 163;
    const unsigned int EXSEL = 164;

    const unsigned int KP_00 = 176;
    const unsigned int KP_000 = 177;
    const unsigned int THOUSANDSSEPARATOR = 178;
    const unsigned int DECIMALSEPARATOR = 179;
    const unsigned int CURRENCYUNIT = 180;
    const unsigned int CURRENCYSUBUNIT = 181;
    const unsigned int KP_LEFTPAREN = 182;
    const unsigned int KP_RIGHTPAREN = 183;
    const unsigned int KP_LEFTBRACE = 184;
    const unsigned int KP_RIGHTBRACE = 185;
    const unsigned int KP_TAB = 186;
    const unsigned int KP_BACKSPACE = 187;
    const unsigned int KP_A = 188;
    const unsigned int KP_B = 189;
    const unsigned int KP_C = 190;
    const unsigned int KP_D = 191;
    const unsigned int KP_E = 192;
    const unsigned int KP_F = 193;
    const unsigned int KP_XOR = 194;
    const unsigned int KP_POWER = 195;
    const unsigned int KP_PERCENT = 196;
    const unsigned int KP_LESS = 197;
    const unsigned int KP_GREATER = 198;
    const unsigned int KP_AMPERSAND = 199;
    const unsigned int KP_DBLAMPERSAND = 200;
    const unsigned int KP_VERTICALBAR = 201;
    const unsigned int KP_DBLVERTICALBAR = 202;
    const unsigned int KP_COLON = 203;
    const unsigned int KP_HASH = 204;
    const unsigned int KP_SPACE = 205;
    const unsigned int KP_AT = 206;
    const unsigned int KP_EXCLAM = 207;
    const unsigned int KP_MEMSTORE = 208;
    const unsigned int KP_MEMRECALL = 209;
    const unsigned int KP_MEMCLEAR = 210;
    const unsigned int KP_MEMADD = 211;
    const unsigned int KP_MEMSUBTRACT = 212;
    const unsigned int KP_MEMMULTIPLY = 213;
    const unsigned int KP_MEMDIVIDE = 214;
    const unsigned int KP_PLUSMINUS = 215;
    const unsigned int KP_CLEAR = 216;
    const unsigned int KP_CLEARENTRY = 217;
    const unsigned int KP_BINARY = 218;
    const unsigned int KP_OCTAL = 219;
    const unsigned int KP_DECIMAL = 220;
    const unsigned int KP_HEXADECIMAL = 221;

    const unsigned int LCTRL = 224;
    const unsigned int LSHIFT = 225;
    const unsigned int LALT = 226; /**< alt; option */
    const unsigned int LGUI = 227; /**< windows; command (apple); meta */
    const unsigned int RCTRL = 228;
    const unsigned int RSHIFT = 229;
    const unsigned int RALT = 230; /**< alt gr; option */
    const unsigned int RGUI = 231; /**< windows; command (apple); meta */

    const unsigned int MODE = 257;    /**< I'm not sure if this is really not covered
                                 *   by any of the above; but since there's a
                                 *   special SDL_KMOD_MODE for it I'm adding it here
                                 */

    /* @} *//* Usage page 0x07 */

    /**
     *  \name Usage page 0x0C
     *
     *  These values are mapped from usage page 0x0C (USB consumer page).
     *
     *  There are way more keys in the spec than we can represent in the
     *  current scancode range; so pick the ones that commonly come up in
     *  real world usage.
     */
    /* @{ */

    const unsigned int SLEEP = 258;                   /**< Sleep */
    const unsigned int WAKE = 259;                    /**< Wake */

    const unsigned int CHANNEL_INCREMENT = 260;       /**< Channel Increment */
    const unsigned int CHANNEL_DECREMENT = 261;       /**< Channel Decrement */

    const unsigned int MEDIA_PLAY = 262;          /**< Play */
    const unsigned int MEDIA_PAUSE = 263;         /**< Pause */
    const unsigned int MEDIA_RECORD = 264;        /**< Record */
    const unsigned int MEDIA_FAST_FORWARD = 265;  /**< Fast Forward */
    const unsigned int MEDIA_REWIND = 266;        /**< Rewind */
    const unsigned int MEDIA_NEXT_TRACK = 267;    /**< Next Track */
    const unsigned int MEDIA_PREVIOUS_TRACK = 268; /**< Previous Track */
    const unsigned int MEDIA_STOP = 269;          /**< Stop */
    const unsigned int MEDIA_EJECT = 270;         /**< Eject */
    const unsigned int MEDIA_PLAY_PAUSE = 271;    /**< Play / Pause */
    const unsigned int MEDIA_SELECT = 272;        /* Media Select */

    const unsigned int AC_NEW = 273;              /**< AC New */
    const unsigned int AC_OPEN = 274;             /**< AC Open */
    const unsigned int AC_CLOSE = 275;            /**< AC Close */
    const unsigned int AC_EXIT = 276;             /**< AC Exit */
    const unsigned int AC_SAVE = 277;             /**< AC Save */
    const unsigned int AC_PRINT = 278;            /**< AC Print */
    const unsigned int AC_PROPERTIES = 279;       /**< AC Properties */

    const unsigned int AC_SEARCH = 280;           /**< AC Search */
    const unsigned int AC_HOME = 281;             /**< AC Home */
    const unsigned int AC_BACK = 282;             /**< AC Back */
    const unsigned int AC_FORWARD = 283;          /**< AC Forward */
    const unsigned int AC_STOP = 284;             /**< AC Stop */
    const unsigned int AC_REFRESH = 285;          /**< AC Refresh */
    const unsigned int AC_BOOKMARKS = 286;        /**< AC Bookmarks */

    /* @} *//* Usage page 0x0C */


    /**
     *  \name Mobile keys
     *
     *  These are values that are often used on mobile phones.
     */
    /* @{ */

    const unsigned int SOFTLEFT = 287; /**< Usually situated below the display on phones and
                                      used as a multi-function feature key for selecting
                                      a software defined function shown on the bottom left
                                      of the display. */
    const unsigned int SOFTRIGHT = 288; /**< Usually situated below the display on phones and
                                       used as a multi-function feature key for selecting
                                       a software defined function shown on the bottom right
                                       of the display. */
    const unsigned int CALL = 289; /**< Used for accepting phone calls. */
    const unsigned int ENDCALL = 290; /**< Used for rejecting phone calls. */

    /* @} *//* Mobile keys */

    /* Add any other keys here. */

    const unsigned int RESERVED = 400;    /**< 400-500 reserved for dynamic keycodes */

    const unsigned int COUNT = 512; /**< not a key; just marks the number of scancodes for array bounds */

};
}