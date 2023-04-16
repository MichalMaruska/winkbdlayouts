//---------------------------------------------------------------------------
//
// Windows Keyboards Layouts (WKL)
// Copyright (c) 2023, Thierry Lelegard
// BSD-2-Clause license, see the LICENSE file.
//
// Utility to analyze an installed keyboard layout DLL.
// Generate a C source file for this keyboard.
//
//---------------------------------------------------------------------------

#include "options.h"
#include "strutils.h"
#include "winutils.h"
#include "registry.h"
#include "grid.h"
#include "fileversion.h"

// Tables of values => symbols
typedef __int64 Value;
typedef std::map<Value, WString> SymbolTable;
#define SYM(e) {e, L#e}

// Configure the terminal console on init, restore on exit.
ConsoleState state;


//----------------------------------------------------------------------------
// Command line options.
//----------------------------------------------------------------------------

class ReverseOptions : public Options
{
public:
    // Constructor.
    ReverseOptions(int argc, wchar_t* argv[]);

    // Command line options.
    WString dashed;
    WString input;
    WString output;
    WString comment;
    int     kbd_type;
    bool    num_only;
    bool    hexa_dump;
    bool    gen_resources;
};

ReverseOptions::ReverseOptions(int argc, wchar_t* argv[]) :
    Options(argc, argv,
        L"[options] kbd-name-or-file\n"
        L"\n"
        L"  kbd-name-or-file : Either the file name of a keyboard layout DLL or the\n"
        L"  name of a keyboard layout, for instance \"fr\" for C:\\Windows\\System32\\kbdfr.dll\n"
        L"\n"
        L"Options:\n"
        L"\n"
        L"  -c \"string\" : comment string in the header\n"
        L"  -d : add hexa dump in final comments\n"
        L"  -h : display this help text\n"
        L"  -n : numerical output only, do not attempt to translate to source macros\n"
        L"  -o file : output file name, default is standard output\n"
        L"  -r : generate a resource file instead of a C source file\n"
        L"  -t value : keyboard type, defaults to dwType in kbd table or 4 if unspecified"),
    dashed(75, L'-'),
    input(),
    output(),
    comment(L"Windows Keyboards Layouts (WKL)"),
    kbd_type(0),
    num_only(false),
    hexa_dump(false),
    gen_resources(false)
{
    // Parse arguments.
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == L"--help" || args[i] == L"-h") {
            usage();
        }
        else if (args[i] == L"-d") {
            hexa_dump = true;
        }
        else if (args[i] == L"-n") {
            num_only = true;
        }
        else if (args[i] == L"-r") {
            gen_resources = true;
        }
        else if (args[i] == L"-o" && i + 1 < args.size()) {
            output = args[++i];
        }
        else if (args[i] == L"-c" && i + 1 < args.size()) {
            comment = args[++i];
        }
        else if (args[i] == L"-t" && i + 1 < args.size()) {
            kbd_type = ToInt(args[++i]);
        }
        else if (!args[i].empty() && args[i].front() != '-' && input.empty()) {
            input = args[i];
        }
        else {
            fatal("invalid option '" + args[i] + "', try --help");
        }
    }
    if (input.empty()) {
        fatal(L"no keyboard layout specified, try --help");
    }
}


//---------------------------------------------------------------------------
// Common symbol tables.
//---------------------------------------------------------------------------

// Full description of a modifier state, for use in comments in MODIFIERS structure.
static const WStringVector modifiers_comments {
    L"000 = <none>",
    L"001 = Shift",
    L"010 = Control",
    L"011 = Shift Control",
    L"100 = Alt",
    L"101 = Shift Alt",
    L"110 = Control Alt (AltGr)",
    L"111 = Shift Control Alt"
};

// Top of columns of VK_TO_WCHARSx structures.
static const WStringVector modifiers_headers {
    L"",
    L"Shift",
    L"Ctrl",
    L"Shift/Ctrl",
    L"Alt",
    L"Shift/Alt",
    L"Ctrl/Alt",
    L"Shift/Ctrl/Alt"
};

const SymbolTable shift_state_symbols {
    SYM(KBDBASE),
    SYM(KBDSHIFT),
    SYM(KBDCTRL),
    SYM(KBDALT),
    SYM(KBDKANA),
    SYM(KBDROYA),
    SYM(KBDLOYA),
    SYM(KBDGRPSELTAP)
};

const SymbolTable vk_symbols {
    SYM(VK_LBUTTON),
    SYM(VK_RBUTTON),
    SYM(VK_CANCEL),
    SYM(VK_MBUTTON),
    SYM(VK_XBUTTON1),
    SYM(VK_XBUTTON2),
    SYM(VK_BACK),
    SYM(VK_TAB),
    SYM(VK_CLEAR),
    SYM(VK_RETURN),
    SYM(VK_SHIFT),
    SYM(VK_CONTROL),
    SYM(VK_MENU),
    SYM(VK_PAUSE),
    SYM(VK_CAPITAL),
    SYM(VK_KANA),
    SYM(VK_IME_ON),
    SYM(VK_JUNJA),
    SYM(VK_FINAL),
    SYM(VK_HANJA),
    SYM(VK_KANJI),
    SYM(VK_IME_OFF),
    SYM(VK_ESCAPE),
    SYM(VK_CONVERT),
    SYM(VK_NONCONVERT),
    SYM(VK_ACCEPT),
    SYM(VK_MODECHANGE),
    SYM(VK_SPACE),
    SYM(VK_PRIOR),
    SYM(VK_NEXT),
    SYM(VK_END),
    SYM(VK_HOME),
    SYM(VK_LEFT),
    SYM(VK_UP),
    SYM(VK_RIGHT),
    SYM(VK_DOWN),
    SYM(VK_SELECT),
    SYM(VK_PRINT),
    SYM(VK_EXECUTE),
    SYM(VK_SNAPSHOT),
    SYM(VK_INSERT),
    SYM(VK_DELETE),
    SYM(VK_HELP),
    SYM('0'),
    SYM('1'),
    SYM('2'),
    SYM('3'),
    SYM('4'),
    SYM('5'),
    SYM('6'),
    SYM('7'),
    SYM('8'),
    SYM('9'),
    SYM('A'),
    SYM('B'),
    SYM('C'),
    SYM('D'),
    SYM('E'),
    SYM('F'),
    SYM('G'),
    SYM('H'),
    SYM('I'),
    SYM('J'),
    SYM('K'),
    SYM('L'),
    SYM('M'),
    SYM('N'),
    SYM('O'),
    SYM('P'),
    SYM('Q'),
    SYM('R'),
    SYM('S'),
    SYM('T'),
    SYM('U'),
    SYM('V'),
    SYM('W'),
    SYM('X'),
    SYM('Y'),
    SYM('Z'),
    SYM(VK_LWIN),
    SYM(VK_RWIN),
    SYM(VK_APPS),
    SYM(VK_SLEEP),
    SYM(VK_NUMPAD0),
    SYM(VK_NUMPAD1),
    SYM(VK_NUMPAD2),
    SYM(VK_NUMPAD3),
    SYM(VK_NUMPAD4),
    SYM(VK_NUMPAD5),
    SYM(VK_NUMPAD6),
    SYM(VK_NUMPAD7),
    SYM(VK_NUMPAD8),
    SYM(VK_NUMPAD9),
    SYM(VK_MULTIPLY),
    SYM(VK_ADD),
    SYM(VK_SEPARATOR),
    SYM(VK_SUBTRACT),
    SYM(VK_DECIMAL),
    SYM(VK_DIVIDE),
    SYM(VK_F1),
    SYM(VK_F2),
    SYM(VK_F3),
    SYM(VK_F4),
    SYM(VK_F5),
    SYM(VK_F6),
    SYM(VK_F7),
    SYM(VK_F8),
    SYM(VK_F9),
    SYM(VK_F10),
    SYM(VK_F11),
    SYM(VK_F12),
    SYM(VK_F13),
    SYM(VK_F14),
    SYM(VK_F15),
    SYM(VK_F16),
    SYM(VK_F17),
    SYM(VK_F18),
    SYM(VK_F19),
    SYM(VK_F20),
    SYM(VK_F21),
    SYM(VK_F22),
    SYM(VK_F23),
    SYM(VK_F24),
    SYM(VK_NAVIGATION_VIEW),
    SYM(VK_NAVIGATION_MENU),
    SYM(VK_NAVIGATION_UP),
    SYM(VK_NAVIGATION_DOWN),
    SYM(VK_NAVIGATION_LEFT),
    SYM(VK_NAVIGATION_RIGHT),
    SYM(VK_NAVIGATION_ACCEPT),
    SYM(VK_NAVIGATION_CANCEL),
    SYM(VK_NUMLOCK),
    SYM(VK_SCROLL),
    SYM(VK_OEM_NEC_EQUAL),
    SYM(VK_OEM_FJ_JISHO),
    SYM(VK_OEM_FJ_MASSHOU),
    SYM(VK_OEM_FJ_TOUROKU),
    SYM(VK_OEM_FJ_LOYA),
    SYM(VK_OEM_FJ_ROYA),
    SYM(VK_LSHIFT),
    SYM(VK_RSHIFT),
    SYM(VK_LCONTROL),
    SYM(VK_RCONTROL),
    SYM(VK_LMENU),
    SYM(VK_RMENU),
    SYM(VK_BROWSER_BACK),
    SYM(VK_BROWSER_FORWARD),
    SYM(VK_BROWSER_REFRESH),
    SYM(VK_BROWSER_STOP),
    SYM(VK_BROWSER_SEARCH),
    SYM(VK_BROWSER_FAVORITES),
    SYM(VK_BROWSER_HOME),
    SYM(VK_VOLUME_MUTE),
    SYM(VK_VOLUME_DOWN),
    SYM(VK_VOLUME_UP),
    SYM(VK_MEDIA_NEXT_TRACK),
    SYM(VK_MEDIA_PREV_TRACK),
    SYM(VK_MEDIA_STOP),
    SYM(VK_MEDIA_PLAY_PAUSE),
    SYM(VK_LAUNCH_MAIL),
    SYM(VK_LAUNCH_MEDIA_SELECT),
    SYM(VK_LAUNCH_APP1),
    SYM(VK_LAUNCH_APP2),
    SYM(VK_OEM_1),
    SYM(VK_OEM_PLUS),
    SYM(VK_OEM_COMMA),
    SYM(VK_OEM_MINUS),
    SYM(VK_OEM_PERIOD),
    SYM(VK_OEM_2),
    SYM(VK_OEM_3),
    SYM(VK_GAMEPAD_A),
    SYM(VK_GAMEPAD_B),
    SYM(VK_GAMEPAD_X),
    SYM(VK_GAMEPAD_Y),
    SYM(VK_GAMEPAD_RIGHT_SHOULDER),
    SYM(VK_GAMEPAD_LEFT_SHOULDER),
    SYM(VK_GAMEPAD_LEFT_TRIGGER),
    SYM(VK_GAMEPAD_RIGHT_TRIGGER),
    SYM(VK_GAMEPAD_DPAD_UP),
    SYM(VK_GAMEPAD_DPAD_DOWN),
    SYM(VK_GAMEPAD_DPAD_LEFT),
    SYM(VK_GAMEPAD_DPAD_RIGHT),
    SYM(VK_GAMEPAD_MENU),
    SYM(VK_GAMEPAD_VIEW),
    SYM(VK_GAMEPAD_LEFT_THUMBSTICK_BUTTON),
    SYM(VK_GAMEPAD_RIGHT_THUMBSTICK_BUTTON),
    SYM(VK_GAMEPAD_LEFT_THUMBSTICK_UP),
    SYM(VK_GAMEPAD_LEFT_THUMBSTICK_DOWN),
    SYM(VK_GAMEPAD_LEFT_THUMBSTICK_RIGHT),
    SYM(VK_GAMEPAD_LEFT_THUMBSTICK_LEFT),
    SYM(VK_GAMEPAD_RIGHT_THUMBSTICK_UP),
    SYM(VK_GAMEPAD_RIGHT_THUMBSTICK_DOWN),
    SYM(VK_GAMEPAD_RIGHT_THUMBSTICK_RIGHT),
    SYM(VK_GAMEPAD_RIGHT_THUMBSTICK_LEFT),
    SYM(VK_OEM_4),
    SYM(VK_OEM_5),
    SYM(VK_OEM_6),
    SYM(VK_OEM_7),
    SYM(VK_OEM_8),
    SYM(VK_OEM_AX),
    SYM(VK_OEM_102),
    SYM(VK_ICO_HELP),
    SYM(VK_ICO_00),
    SYM(VK_PROCESSKEY),
    SYM(VK_ICO_CLEAR),
    SYM(VK_PACKET),
    SYM(VK_OEM_RESET),
    SYM(VK_OEM_JUMP),
    SYM(VK_OEM_PA1),
    SYM(VK_OEM_PA2),
    SYM(VK_OEM_PA3),
    SYM(VK_OEM_WSCTRL),
    SYM(VK_OEM_CUSEL),
    SYM(VK_OEM_ATTN),
    SYM(VK_OEM_FINISH),
    SYM(VK_OEM_COPY),
    SYM(VK_OEM_AUTO),
    SYM(VK_OEM_ENLW),
    SYM(VK_OEM_BACKTAB),
    SYM(VK_ATTN),
    SYM(VK_CRSEL),
    SYM(VK_EXSEL),
    SYM(VK_EREOF),
    SYM(VK_PLAY),
    SYM(VK_ZOOM),
    SYM(VK_NONAME),
    SYM(VK_PA1),
    SYM(VK_OEM_CLEAR),
    SYM(VK__none_)
};

const SymbolTable vk_flags_symbols {
    SYM(KBDEXT),
    SYM(KBDMULTIVK),
    SYM(KBDSPECIAL),
    SYM(KBDNUMPAD),
    SYM(KBDUNICODE),
    SYM(KBDINJECTEDVK),
    SYM(KBDMAPPEDVK),
    SYM(KBDBREAK)
};

const SymbolTable vk_attr_symbols {
    SYM(CAPLOK),
    SYM(SGCAPS),
    SYM(CAPLOKALTGR),
    SYM(KANALOK),
    SYM(GRPSELTAP)
};

// Complete symbol for a WCHAR (a character literal).
const SymbolTable wchar_symbols {
    {'\t', L"L'\\t'"},
    {'\n', L"L'\\n'"},
    {'\r', L"L'\\r'"},
    {'\'', L"L'\\\''"},
    {'\\', L"L'\\\\'"},
    SYM(WCH_NONE),
    SYM(WCH_DEAD),
    SYM(WCH_LGTR)
};

// WCHAR representation when inserted in strings literals.
const SymbolTable wchar_literals {
    {'\t', L"\\t"},
    {'\n', L"\\n"},
    {'\r', L"\\r"},
    {'"',  L"\\\""},
    {'\\', L"\\\\"},
};

// Names of some usual non-ASCII WCHAR, for insertion in comments.
const SymbolTable wchar_descriptions {
    {0x0008, L"BS"},
    {0x0009, L"TAB"},
    {0x000A, L"LF"},
    {0x000B, L"VT"},
    {0x000C, L"FF"},
    {0x000D, L"CR"},
    {0x001B, L"ESC"},
    {0x007F, L"DEL"},
    {0x00A0, L"Nbrk space"},
    {0x00A1, L"Inv !"},
    {0x00A2, L"Cent"},
    {0x00A3, L"Pound"},
    {0x00A4, L"Currency"},
    {0x00A5, L"Yen"},
    {0x00A6, L"Broken bar"},
    {0x00A7, L"Section"},
    {0x00A8, L"Diaeresis"},
    {0x00A9, L"Copyright"},
    {0x00AA, L"Fem ord"},
    {0x00AB, L"<<"},
    {0x00AC, L"Not"},
    {0x00AD, L"Soft hyphen"},
    {0x00AE, L"Registered"},
    {0x00AF, L"Macron"},
    {0x00B0, L"Degree"},
    {0x00B1, L"+/-"},
    {0x00B2, L"Superscr two"},
    {0x00B3, L"Superscr three"},
    {0x00B4, L"Acute"},
    {0x00B5, L"Micro"},
    {0x00B6, L"Pilcrow"},
    {0x00B7, L"Middle dot"},
    {0x00B8, L"Cedilla"},
    {0x00B9, L"Superscr one"},
    {0x00BA, L"Masc ord"},
    {0x00BB, L">>"},
    {0x00BC, L"1/4"},
    {0x00BD, L"1/2"},
    {0x00BE, L"3/4"},
    {0x00BF, L"Inv ?"},
    {0x00C0, L"A grave"},
    {0x00C1, L"A acute"},
    {0x00C2, L"A circumflex"},
    {0x00C3, L"A tilde"},
    {0x00C4, L"A diaeresis"},
    {0x00C5, L"A ring above"},
    {0x00C6, L"AE"},
    {0x00C7, L"C cedilla"},
    {0x00C8, L"E grave"},
    {0x00C9, L"E acute"},
    {0x00CA, L"E circumflex"},
    {0x00CB, L"E diaeresis"},
    {0x00CC, L"I grave"},
    {0x00CD, L"I acute"},
    {0x00CE, L"I circumflex"},
    {0x00CF, L"I diaeresis"},
    {0x00D0, L"ETH"},
    {0x00D1, L"N tilde"},
    {0x00D2, L"O grave"},
    {0x00D3, L"O acute"},
    {0x00D4, L"O circumflex"},
    {0x00D5, L"O tilde"},
    {0x00D6, L"O diaeresis"},
    {0x00D7, L"Multiplication"},
    {0x00D8, L"O stroke"},
    {0x00D9, L"U grave"},
    {0x00DA, L"U acute"},
    {0x00DB, L"U circumflex"},
    {0x00DC, L"U diaeresis"},
    {0x00DD, L"Y acute"},
    {0x00DE, L"THORN"},
    {0x00DF, L"sharp S"},
    {0x00E0, L"a grave"},
    {0x00E1, L"a acute"},
    {0x00E2, L"a circumflex"},
    {0x00E3, L"a tilde"},
    {0x00E4, L"a diaeresis"},
    {0x00E5, L"a ring above"},
    {0x00E6, L"ae"},
    {0x00E7, L"c cedilla"},
    {0x00E8, L"e grave"},
    {0x00E9, L"e acute"},
    {0x00EA, L"e circumflex"},
    {0x00EB, L"e diaeresis"},
    {0x00EC, L"i grave"},
    {0x00ED, L"i acute"},
    {0x00EE, L"i circumflex"},
    {0x00EF, L"i diaeresis"},
    {0x00F0, L"eth"},
    {0x00F1, L"n tilde"},
    {0x00F2, L"o grave"},
    {0x00F3, L"o acute"},
    {0x00F4, L"o circumflex"},
    {0x00F5, L"o tilde"},
    {0x00F6, L"o diaeresis"},
    {0x00F7, L"Division"},
    {0x00F8, L"o stroke"},
    {0x00F9, L"u grave"},
    {0x00FA, L"u acute"},
    {0x00FB, L"u circumflex"},
    {0x00FC, L"u diaeresis"},
    {0x00FD, L"y acute"},
    {0x00FE, L"thorn"},
    {0x00FF, L"y diaeresis"},
    {0x0100, L"A macron"},
    {0x0101, L"a macron"},
    {0x0102, L"A breve"},
    {0x0103, L"a breve"},
    {0x0104, L"A ogonek"},
    {0x0105, L"a ogonek"},
    {0x0106, L"C acute"},
    {0x0107, L"c acute"},
    {0x0108, L"C circumflex"},
    {0x0109, L"c circumflex"},
    {0x010A, L"C dot above"},
    {0x010B, L"c dot above"},
    {0x010C, L"C caron"},
    {0x010D, L"c caron"},
    {0x010E, L"D caron"},
    {0x010F, L"d caron"},
    {0x0110, L"D stroke"},
    {0x0111, L"d stroke"},
    {0x0112, L"E macron"},
    {0x0113, L"e macron"},
    {0x0116, L"E dot above"},
    {0x0117, L"e dot above"},
    {0x0118, L"E ogonek"},
    {0x0119, L"e ogonek"},
    {0x011A, L"E caron"},
    {0x011B, L"e caron"},
    {0x011C, L"G circumflex"},
    {0x011D, L"g circumflex"},
    {0x011E, L"G breve"},
    {0x011F, L"g breve"},
    {0x0120, L"G dot above"},
    {0x0121, L"g dot above"},
    {0x0122, L"G cedilla"},
    {0x0123, L"g cedilla"},
    {0x0124, L"H circumflex"},
    {0x0125, L"h circumflex"},
    {0x0126, L"H stroke"},
    {0x0127, L"h stroke"},
    {0x0128, L"I tilde"},
    {0x0129, L"i tilde"},
    {0x012A, L"I macron"},
    {0x012B, L"i macron"},
    {0x012E, L"I ogonek"},
    {0x012F, L"i ogonek"},
    {0x0130, L"I dot above"},
    {0x0131, L"Dotless I"},
    {0x0134, L"J circumflex"},
    {0x0135, L"j circumflex"},
    {0x0136, L"K cedilla"},
    {0x0137, L"k cedilla"},
    {0x0138, L"kra"},
    {0x0139, L"L acute"},
    {0x013A, L"l acute"},
    {0x013B, L"L cedilla"},
    {0x013C, L"l cedilla"},
    {0x013D, L"L caron"},
    {0x013E, L"l caron"},
    {0x0141, L"L stroke"},
    {0x0142, L"l stroke"},
    {0x0143, L"N acute"},
    {0x0144, L"n acute"},
    {0x0145, L"N cedilla"},
    {0x0146, L"n cedilla"},
    {0x0147, L"N caron"},
    {0x0148, L"n caron"},
    {0x014A, L"ENG"},
    {0x014B, L"eng"},
    {0x014C, L"O macron"},
    {0x014D, L"o macron"},
    {0x0150, L"O double acute"},
    {0x0151, L"o double acute"},
    {0x0152, L"OE"},
    {0x0153, L"oe"},
    {0x0154, L"R acute"},
    {0x0155, L"r acute"},
    {0x0156, L"R cedilla"},
    {0x0157, L"r cedilla"},
    {0x0158, L"R caron"},
    {0x0159, L"r caron"},
    {0x015A, L"S acute"},
    {0x015B, L"s acute"},
    {0x015C, L"S circumflex"},
    {0x015D, L"s circumflex"},
    {0x015E, L"S cedilla"},
    {0x015F, L"s cedilla"},
    {0x0160, L"S caron"},
    {0x0161, L"s caron"},
    {0x0162, L"T cedilla"},
    {0x0163, L"t cedilla"},
    {0x0164, L"T caron"},
    {0x0165, L"t caron"},
    {0x0166, L"T stroke"},
    {0x0167, L"t stroke"},
    {0x0168, L"U tilde"},
    {0x0169, L"u tilde"},
    {0x016A, L"U macron"},
    {0x016B, L"u macron"},
    {0x016C, L"U breve"},
    {0x016D, L"u breve"},
    {0x016E, L"U ring above"},
    {0x016F, L"u ring above"},
    {0x0170, L"U double acute"},
    {0x0171, L"u double acute"},
    {0x0172, L"U ogonek"},
    {0x0173, L"u ogonek"},
    {0x0174, L"W circumflex"},
    {0x0175, L"w circumflex"},
    {0x0176, L"Y circumflex"},
    {0x0177, L"y circumflex"},
    {0x0178, L"Y diaeresis"},
    {0x0179, L"Z acute"},
    {0x017A, L"z acute"},
    {0x017B, L"Z dot above"},
    {0x017C, L"z dot above"},
    {0x017D, L"Z caron"},
    {0x017E, L"z caron"},
    {0x0192, L"f HOOK"},
    {0x0218, L"S comma below"},
    {0x0219, L"s comma below"},
    {0x021A, L"T comma below"},
    {0x021B, L"t comma below"},
    {0x02C6, L"Circumflex"},
    {0x02C7, L"Caron"},
    {0x02D8, L"Breve"},
    {0x02D9, L"Dot above"},
    {0x02DB, L"Ogonek"},
    {0x02DC, L"Small tilde"},
    {0x02DD, L"Double acute"},
};


//---------------------------------------------------------------------------
// Description of one data structure.
//---------------------------------------------------------------------------

class DataStructure
{
public:
    WString name;
    const void*  address;
    size_t       size;

    // Constructors with address or integer.
    DataStructure(const WString& n = L"", const void* a = nullptr, size_t s = 0)
        : name(n), address(a), size(s) {}
    DataStructure(const WString& n, const void* a, const void* end)
        : name(n), address(a), size(uintptr_t(end) - uintptr_t(a)) {}
    DataStructure(const WString& n, uintptr_t a, size_t s = 0)
        : name(n), address(reinterpret_cast<const void*>(a)), size(s) {}

    // Get/set address after last byte.
    const void* end() const { return reinterpret_cast<const uint8_t*>(address) + size; }
    void setEnd(const void* e) { size = uintptr_t(e) - uintptr_t(address); }

    // Sort operator.
    bool operator<(const DataStructure& s) const { return address < s.address; }

    // Hexa dump of the structure.
    void dump(std::ostream&) const;
};

void DataStructure::dump(std::ostream& out) const
{
    const WString header(name + Format(L" (%d bytes)", int(size)));
    out << "//" << std::endl
        << "// " << header << std::endl
        << "// " << std::string(header.length(), '-') << std::endl;
    PrintHexa(out, address, size, L"// ", true);
}


//---------------------------------------------------------------------------
// Generate various parts of the source file.
//---------------------------------------------------------------------------

class SourceGenerator
{
public:
    // Constructor.
    SourceGenerator(ReverseOptions& opt) : _ou(opt.out()), _opt(opt), _alldata() {}

    // Generate the source 
    void generate(const KBDTABLES&);

private:
    std::ostream&            _ou;
    ReverseOptions&          _opt;
    std::list<DataStructure> _alldata;

    // Format an integer as a decimal or hexadecimal string.
    // If hex_digits is zero, format in decimal.
    WString integer(Value value, int hex_digits = 0);

    // Format an integer as a string, using a table of symbols.
    // If no symbol found or option -n, return a number.
    // If hex_digits is zero, format in decimal.
    WString symbol(const SymbolTable& symbols, Value value, int hex_digits = 0);

    // Format a bit mask of symbols, same principle as symbol().
    WString bitMask(const SymbolTable& symbols, Value value, int hex_digits = 0);

    // Format a symbol and a bit mask of attributes, same principle as Symbol().
    WString attributes(const SymbolTable& symbols, const SymbolTable& attributes, Value value, int hex_digits = 0);

    // Format locale flags according to symbols.
    WString localeFlags(DWORD flags);

    // Format a Pointer
    WString pointer(const void* value, const WString& name);

    // Format a WCHAR. Add description in descs if one exists.
    WString wchar(wchar_t value, WStringList& descs);

    // Format a string of WCHAR
    WString wstring(const wchar_t*);

    // Sort and merge adjacent data structures with same names (typically "Strings in ...").
    void sortDataStructures();

    // Generate the various data structures.
    void genVkToBits(const VK_TO_BIT*, const WString& name);
    void genCharModifiers(const MODIFIERS&, const WString& name);
    void genSubVkToWchar(const VK_TO_WCHARS10*, size_t count, size_t size, const WString& name, const MODIFIERS&);
    void genVkToWchar(const VK_TO_WCHAR_TABLE*, const WString& name, const::MODIFIERS& mods);
    void genLgToWchar(const LIGATURE1*, size_t count, size_t size, const WString& name);
    void genDeadKeys(const DEADKEY*, const WString& name);
    void genVscToString(const VSC_LPWSTR*, const WString& name, const WString& comment = L"");
    void genKeyNames(const DEADKEY_LPWSTR*, const WString& name);
    void genScanToVk(const USHORT* vk, size_t vk_count, const WString& name);
    void genVscToVk(const VSC_VK*, const WString& name, const WString& comment = L"");
    void genHexaDump();
};

//---------------------------------------------------------------------------

WString SourceGenerator::integer(Value value, int hex_digits)
{
    return hex_digits <= 0 ? Format(L"%lld", value) : Format(L"0x%0*llX", hex_digits, value);
}

//---------------------------------------------------------------------------

WString SourceGenerator::symbol(const SymbolTable& symbols, Value value, int hex_digits)
{
    if (!_opt.num_only) {
        const auto it = symbols.find(value);
        if (it != symbols.end()) {
            return it->second;
        }
    }
    return integer(value, hex_digits);
}

//---------------------------------------------------------------------------

WString SourceGenerator::bitMask(const SymbolTable& symbols, Value value, int hex_digits)
{
    if (!_opt.num_only) {
        WString str;
        Value bits = 0;
        for (const auto& sym : symbols) {
            if (sym.first == 0 && value == 0) {
                // Specific symbol for zero (no flag)
                return sym.second;
            }
            if (sym.first != 0 && (value & sym.first) == sym.first) {
                // Found one flag.
                if (!str.empty()) {
                    str += L" | ";
                }
                str += sym.second;
                bits |= sym.first;
            }
        }
        if (bits != 0) {
            // Found at least some bits, add remaining bits.
            if ((value & ~bits) != 0) {
                if (!str.empty()) {
                    str += L" | ";
                }
                str += Format(L"0x%0*lld", hex_digits, value & ~bits);
            }
            return str;
        }
    }
    return integer(value, hex_digits);
}

//---------------------------------------------------------------------------

WString SourceGenerator::attributes(const SymbolTable& symbols, const SymbolTable& attributes, Value value, int hex_digits)
{
    if (!_opt.num_only) {
        // Compute mask of all possible attributes.
        Value all_attributes = 0;
        for (const auto& sym : attributes) {
            all_attributes |= sym.first;
        }
        // Base value.
        WString str(symbol(symbols, value & ~all_attributes, hex_digits));
        // Add attributes.
        if ((value & all_attributes) != 0) {
            str += L" | " + bitMask(attributes, value & all_attributes, hex_digits);
        }
        return str;
    }
    return integer(value, hex_digits);
}

//---------------------------------------------------------------------------

WString SourceGenerator::localeFlags(const DWORD flags)
{
    if (_opt.num_only) {
        return Format(L"0x%08X", flags);
    }
    else {
        WString lostr(bitMask({ SYM(KLLF_ALTGR), SYM(KLLF_SHIFTLOCK), SYM(KLLF_LRM_RLM) }, LOWORD(flags), 4));
        WString histr(symbol({ SYM(KBD_VERSION) }, HIWORD(flags), 4));
        return L"MAKELONG(" + lostr + L", " + histr + L")";
    }
}

//---------------------------------------------------------------------------

WString SourceGenerator::pointer(const void* value, const WString& name)
{
    return value == nullptr ? L"NULL" : name;
}

//---------------------------------------------------------------------------

WString SourceGenerator::wchar(wchar_t value, WStringList& descs)
{
    // Format a WCHAR. Add description in descs if one exists.
    if (!_opt.num_only) {
        const auto sym = wchar_symbols.find(value);
        if (sym != wchar_symbols.end()) {
            return sym->second;
        }
    }
    if (value == L'\'' || value == L'\\') {
        WString res(L"L'\\ '");
        res[3] = value;
        return res;
    }
    else if (value >= L' ' && value < 0x007F) {
        WString res(L"L' '");
        res[2] = value;
        return res;
    }
    else {
        // No symbol found, add a comment 
        if (!_opt.num_only) {
            const auto dsc = wchar_descriptions.find(value);
            if (dsc != wchar_descriptions.end()) {
                descs.push_back(dsc->second);
            }
        }
        return Format(L"0x%04X", value);
    }
}

//---------------------------------------------------------------------------

WString SourceGenerator::wstring(const wchar_t* value)
{
    // Format a string of WCHAR
    if (value == nullptr) {
        return L"NULL";
    }
    WString str(L"L\"");
    for (; *value != 0; ++value) {
        const auto it = wchar_literals.find(*value);
        if (it != wchar_literals.end()) {
            str.append(it->second);
        }
        else if (*value >= L' ' && *value < 0x007F) {
            str.push_back(char(*value));
        }
        else {
            str.append(Format(L"\\x%04x", *value));
        }
    }
    str.push_back(L'"');
    return str;
}

//---------------------------------------------------------------------------

void SourceGenerator::sortDataStructures()
{
    // Sort all data structures by address.
    _alldata.sort();

    // Merge adjacent data structures with same names (typically "Strings in ...").
    auto current = _alldata.begin();
    auto previous = current++;
    while (current != _alldata.end()) {
        const bool inter_zero = IsZero(previous->end(), current->address);
        // Merge if the two data structures have the same name and are adjacent or
        // only separated by zeroes (typpically padding).
        if (previous->name == current->name && (previous->end() == current->address || inter_zero)) {
            // Merge previous and current structure.
            previous->size = uintptr_t(current->end()) - uintptr_t(previous->address);
            current = _alldata.erase(current);
        }
        else {
            // If there is empty space between the two structures, create a structure for it.
            if (previous->end() < current->address) {
                DataStructure inter(inter_zero ? L"Padding" : L"Unreferenced", previous->end(), current->address);
                current = _alldata.insert(current, inter);
            }
            // Move to next pair of structures.
            previous = current;
            ++current;
        }
    }
}

//---------------------------------------------------------------------------

void SourceGenerator::genVkToBits(const VK_TO_BIT* vtb, const WString& name)
{
    DataStructure ds(name, vtb);

    Grid grid;
    for (; vtb->Vk != 0; vtb++) {
        grid.addLine({
            L"{" + symbol(vk_symbols, vtb->Vk, 2) + L",",
            bitMask(shift_state_symbols, vtb->ModBits, 4) + L"},"
        });
    }
    grid.addLine({L"{0,", L"0}"});
    vtb++;

    ds.setEnd(vtb);
    _alldata.push_back(ds);

    _ou << "//" << _opt.dashed << std::endl
        << "// Associate a virtual key with a modifier bitmask" << std::endl
        << "//" << _opt.dashed << std::endl
        << std::endl
        << "static VK_TO_BIT " << name << "[] = {" << std::endl;
    grid.setMargin(4);
    grid.print(_ou);
    _ou << "};" << std::endl << std::endl;
}

//---------------------------------------------------------------------------

void SourceGenerator::genCharModifiers(const MODIFIERS& mods, const WString& name)
{
    const wchar_t* vk_to_bits_name = L"vk_to_bits";
    if (mods.pVkToBit != nullptr) {
        genVkToBits(mods.pVkToBit, vk_to_bits_name);
    }

    Grid grid;
    // Note: wMaxModBits is the "max value", ie. size = wMaxModBits + 1
    for (WORD i = 0; i <= mods.wMaxModBits; ++i) {
        grid.addLine({symbol({SYM(SHFT_INVALID)}, mods.ModNumber[i]) + ","});
        if (!_opt.num_only && i < modifiers_comments.size()) {
            grid.addColumn(L"// " + modifiers_comments[i]);
        }
    }

    DataStructure ds(name, &mods);
    ds.setEnd(&mods.ModNumber[0] + mods.wMaxModBits + 1);
    _alldata.push_back(ds);

    _ou << "//" << _opt.dashed << std::endl
        << "// Map character modifier bits to modification number" << std::endl
        << "//" << _opt.dashed << std::endl
        << std::endl
        << "static MODIFIERS " << name << " = {" << std::endl
        << "    .pVkToBit    = " << (mods.pVkToBit != nullptr ? vk_to_bits_name : L"NULL") << "," << std::endl
        << "    .wMaxModBits = " << mods.wMaxModBits << "," << std::endl
        << "    .ModNumber   = {" << std::endl;
    grid.setMargin(8);
    grid.print(_ou);
    _ou << "    }" << std::endl
        << "};" << std::endl
        << std::endl;
}

//---------------------------------------------------------------------------

void SourceGenerator::genSubVkToWchar(const VK_TO_WCHARS10* vtwc, size_t count, size_t size, const WString& name, const MODIFIERS& mods)
{
    DataStructure ds(name, vtwc);

    // Add header lines of comments to indicate the type of modifier on top of each column.
    Grid::Line headers(2 + count);
    headers[0] = L"//";
    bool not_empty = false;
    for (size_t i = 0; i <= mods.wMaxModBits && i < modifiers_headers.size(); ++i) {
        const size_t index = mods.ModNumber[i];
        if (2 + index < headers.size()) {
            headers[2 + index] = modifiers_headers[i];
            not_empty = not_empty || !modifiers_headers[i].empty();
        }
    }

    Grid grid;
    if (not_empty && !_opt.num_only) {
        grid.addLine(headers);
        grid.addUnderlines({L"//"});
    }

    while (vtwc->VirtualKey != 0) {
        grid.addLine({
            "{" + symbol(vk_symbols, vtwc->VirtualKey, 2) + ",",
            bitMask(vk_attr_symbols, vtwc->Attributes, 2) + ","
        });
        WStringList comments;
        for (size_t i = 0; i < count; ++i) {
            WString str(wchar(vtwc->wch[i], comments));
            if (i == 0) {
                str.insert(0, 1, L'{');
            }
            str.append(i == count - 1 ? L"}}," : L",");
            grid.addColumn(str);
        }
        if (!comments.empty()) {
            grid.addColumn(L"// " + Join(comments, L", "));
        }

        // Move to next structure (variable size).
        vtwc = reinterpret_cast<const VK_TO_WCHARS10*>(reinterpret_cast<const char*>(vtwc) + size);
    }

    // Last null element.
    Grid::Line line({L"{0,"});
    line.resize(count + 1, L"0,");
    line.push_back(L"0}");
    grid.addLine(line);
    vtwc = reinterpret_cast<const VK_TO_WCHARS10*>(reinterpret_cast<const char*>(vtwc) + size);

    ds.setEnd(vtwc);
    _alldata.push_back(ds);

    _ou << "//" << _opt.dashed << std::endl
        << "// Virtual Key to WCHAR translations for " << count << " shift states" << std::endl
        << "//" << _opt.dashed << std::endl
        << std::endl
        << "static VK_TO_WCHARS" << count << " " << name << "[] = {" << std::endl;
    grid.setMargin(4);
    grid.print(_ou);
    _ou << "};" << std::endl << std::endl;
}

//---------------------------------------------------------------------------

void SourceGenerator::genVkToWchar(const VK_TO_WCHAR_TABLE* vtwc, const WString& name, const::MODIFIERS& mods)
{
    DataStructure ds(name, vtwc);

    Grid grid;
    for (; vtwc->pVkToWchars != nullptr; vtwc++) {
        const WString sub_name(Format(L"vk_to_wchar%d", vtwc->nModifications));
        genSubVkToWchar(reinterpret_cast<PVK_TO_WCHARS10>(vtwc->pVkToWchars), vtwc->nModifications, vtwc->cbSize, sub_name, mods);
        grid.addLine({
            L"{(PVK_TO_WCHARS1)" + sub_name + L",",
            Format(L"%d,", vtwc->nModifications),
            L"sizeof(" + sub_name + L"[0])},"
        });
    }
    grid.addLine({L"{NULL,", L"0,", L"0}"});
    vtwc++;

    ds.setEnd(vtwc);
    _alldata.push_back(ds);

    _ou << "//" << _opt.dashed << std::endl
        << "// Virtual Key to WCHAR translations with shift states" << std::endl
        << "//" << _opt.dashed << std::endl
        << std::endl
        << "static VK_TO_WCHAR_TABLE " << name << "[] = {" << std::endl;
    grid.setMargin(4);
    grid.print(_ou);
    _ou << "};" << std::endl << std::endl;
}

//---------------------------------------------------------------------------

void SourceGenerator::genLgToWchar(const LIGATURE1* ligatures, size_t count, size_t size, const WString& name)
{
    DataStructure ds(name, ligatures);
    const LIGATURE5* lg = reinterpret_cast<const LIGATURE5*>(ligatures);

    Grid grid;
    while (lg->VirtualKey != 0) {
        grid.addLine({
            L"{" + symbol(vk_symbols, lg->VirtualKey, 2) + L",",
            Format(L"%d,", lg->ModificationNumber)
        });
        WStringList comments;
        for (size_t i = 0; i < count; ++i) {
            WString str(wchar(lg->wch[i], comments));
            if (i == 0) {
                str.insert(0, 1, '{');
            }
            str.append(i == count - 1 ? L"}}," : L",");
            grid.addColumn(str);
        }
        if (!comments.empty()) {
            grid.addColumn(L"// " + Join(comments, L", "));
        }

        // Move to next structure (variable size).
        lg = reinterpret_cast<const LIGATURE5*>(reinterpret_cast<const char*>(lg) + size);
    }

    // Last null element.
    Grid::Line line({L"{0,"});
    line.resize(count, L"0,");
    line.push_back(L"0}");
    grid.addLine(line);
    lg = reinterpret_cast<const LIGATURE5*>(reinterpret_cast<const char*>(lg) + size);

    ds.setEnd(lg);
    _alldata.push_back(ds);

    _ou << "//" << _opt.dashed << std::endl
        << "// Ligatures to WCHAR translations" << std::endl
        << "//" << _opt.dashed << std::endl
        << std::endl
        << "static LIGATURE" << count << " " << name << "[] = {" << std::endl;
    grid.setMargin(4);
    grid.print(_ou);
    _ou << "};" << std::endl << std::endl;
}

//---------------------------------------------------------------------------

void SourceGenerator::genDeadKeys(const DEADKEY* dk, const WString& name)
{
    DataStructure ds(name, dk);

    Grid grid;
    for (; dk->dwBoth != 0; dk++) {
        WStringList comments;
        grid.addLine({
            L"DEADTRANS(" + wchar(LOWORD(dk->dwBoth), comments) + ",",
            wchar(HIWORD(dk->dwBoth), comments) + ",",
            wchar(dk->wchComposed, comments) + ",",
            bitMask({ SYM(DKF_DEAD) }, dk->uFlags, 4) + "),"
        });
        if (!comments.empty()) {
            grid.addColumn("// " + Join(comments, L", "));
        }
    }
    dk++; // last null element

    ds.setEnd(dk);
    _alldata.push_back(ds);

    _ou << "//" << _opt.dashed << std::endl
        << "// Dead keys sequences translations" << std::endl
        << "//" << _opt.dashed << std::endl
        << std::endl
        << "static DEADKEY " << name << "[] = {" << std::endl;
    grid.setMargin(4);
    grid.print(_ou);
    _ou << "    {0, 0, 0}" << std::endl
        << "};" << std::endl
        << std::endl;
}

//---------------------------------------------------------------------------

void SourceGenerator::genVscToString(const VSC_LPWSTR* vts, const WString& name, const WString& comment)
{
    DataStructure ds(name, vts);

    Grid grid;
    for (; vts->vsc != 0; vts++) {
        grid.addLine({
            L"{" + Format(L"0x%02X", vts->vsc) + L",",
            wstring(vts->pwsz) + L"},"
        });
        _alldata.push_back(DataStructure("Strings in " + name, vts->pwsz, StringSize(vts->pwsz)));
    }
    grid.addLine({L"{0x00,", L"NULL}"});
    vts++;

    ds.setEnd(vts);
    _alldata.push_back(ds);

    _ou << "//" << _opt.dashed << std::endl
        << "// Scan codes to key names" << comment << std::endl
        << "//" << _opt.dashed << std::endl
        << std::endl
        << "static VSC_LPWSTR " << name << "[] = {" << std::endl;
    grid.setMargin(4);
    grid.print(_ou);
    _ou << "};" << std::endl << std::endl;
}

//---------------------------------------------------------------------------

void SourceGenerator::genKeyNames(const DEADKEY_LPWSTR* names, const WString& name)
{
    DataStructure ds(name, names);

    Grid grid;
    for (; *names != nullptr; ++names) {
        if (**names != 0) {
            WCHAR prefix[2]{ **names, L'\0' };
            grid.addLine({wstring(prefix), wstring(*names + 1) + ","});
            _alldata.push_back(DataStructure("Strings in " + name, *names, StringSize(*names)));
        }
    }
    ++names; // skip last null pointer

    ds.setEnd(names);
    _alldata.push_back(ds);

    _ou << "//" << _opt.dashed << std::endl
        << "// Names of dead keys" << std::endl
        << "//" << _opt.dashed << std::endl
        << std::endl
        << "static DEADKEY_LPWSTR " << name << "[] = {" << std::endl;
    grid.setMargin(4);
    grid.print(_ou);
    _ou << "    NULL" << std::endl << "};" << std::endl << std::endl;
}

//---------------------------------------------------------------------------

void SourceGenerator::genScanToVk(const USHORT* vk, size_t vk_count, const WString& name)
{
    DataStructure ds(name, vk, vk_count * sizeof(*vk));
    _alldata.push_back(ds);

    _ou << "//" << _opt.dashed << std::endl
        << "// Scan code to virtual key conversion table" << std::endl
        << "//" << _opt.dashed << std::endl
        << std::endl
        << "static USHORT " << name << "[] = {" << std::endl;
 
    for (size_t i = 0; i < vk_count; ++i) {
        _ou << Format(L"    /* %02X */ ", i) << attributes(vk_symbols, vk_flags_symbols, vk[i], 4) << "," << std::endl;
    }

    _ou << "};" << std::endl << std::endl;
}

//---------------------------------------------------------------------------

void SourceGenerator::genVscToVk(const VSC_VK* vtvk, const WString& name, const WString& comment)
{
    DataStructure ds(name, vtvk);

    Grid grid;
    for (; vtvk->Vsc != 0; vtvk++) {
        grid.addLine({
            L"{" + Format(L"0x%02X", vtvk->Vsc) + L",",
            attributes(vk_symbols, vk_flags_symbols, vtvk->Vk, 4) + "},"
        });
    }
    grid.addLine({L"{0x00,", L"0x0000}"});
    vtvk++;

    ds.setEnd(vtvk);
    _alldata.push_back(ds);

    _ou << "//" << _opt.dashed << std::endl
        << "// Scan code to virtual key conversion table" << comment << std::endl
        << "//" << _opt.dashed << std::endl
        << std::endl
        << "static VSC_VK " << name << "[] = {" << std::endl;
    grid.setMargin(4);
    grid.print(_ou);
    _ou << "};" << std::endl << std::endl;
}

//---------------------------------------------------------------------------

void SourceGenerator::generate(const KBDTABLES& tables)
{
    // Keyboard type are typically lower than 42. The field dwType was not used in older
    // versions and may contain crap. Try to guess a realistic value for keyboard type.
    // The last default keyord type is 4 (classical 101/102-key keyboard).
    const int kbd_type = _opt.kbd_type > 0 ? _opt.kbd_type : (tables.dwType > 0 && tables.dwType < 48 ? tables.dwType : 4);

    _ou << "//" << _opt.dashed << std::endl
        << "// " << _opt.comment << std::endl
        << "// Automatically generated from " << FileName(_opt.input) << std::endl
        << "//" << _opt.dashed << std::endl
        << std::endl
        << "#define KBD_TYPE " << kbd_type << std::endl
        << std::endl
        << "#include <windows.h>" << std::endl
        << "#include <kbd.h>" << std::endl
        << "#include <dontuse.h>" << std::endl
        << std::endl;

    const WString char_modifiers_name(L"char_modifiers");
    if (tables.pCharModifiers != nullptr) {
        genCharModifiers(*tables.pCharModifiers, char_modifiers_name);
    }

    const WString vk_to_wchar_name(L"vk_to_wchar");
    if (tables.pVkToWcharTable != nullptr) {
        genVkToWchar(tables.pVkToWcharTable, vk_to_wchar_name, *tables.pCharModifiers);
    }

    const WString dead_keys_name(L"dead_keys");
    if (tables.pDeadKey != nullptr) {
        genDeadKeys(tables.pDeadKey, dead_keys_name);
    }

    const WString key_names_name(L"key_names");
    if (tables.pKeyNames != nullptr) {
        genVscToString(tables.pKeyNames, key_names_name);
    }

    const WString key_names_ext_name(L"key_names_ext");
    if (tables.pKeyNamesExt != nullptr) {
        genVscToString(tables.pKeyNamesExt, key_names_ext_name, L" (extended keypad)");
    }

    const WString key_names_dead_name(L"key_names_dead");
    if (tables.pKeyNamesDead != nullptr) {
        genKeyNames(tables.pKeyNamesDead, key_names_dead_name);
    }

    const WString scancode_to_vk_name(L"scancode_to_vk");
    if (tables.pusVSCtoVK != nullptr) {
        genScanToVk(tables.pusVSCtoVK, tables.bMaxVSCtoVK, scancode_to_vk_name);
    }

    const WString scancode_to_vk_e0_name(L"scancode_to_vk_e0");
    if (tables.pVSCtoVK_E0 != nullptr) {
        genVscToVk(tables.pVSCtoVK_E0, scancode_to_vk_e0_name, L" (scancodes with E0 prefix)");
    }

    const WString scancode_to_vk_e1_name(L"scancode_to_vk_e1");
    if (tables.pVSCtoVK_E1 != nullptr) {
        genVscToVk(tables.pVSCtoVK_E1, scancode_to_vk_e1_name, L" (scancodes with E1 prefix)");
    }

    const WString ligatures_name(L"ligatures");
    if (tables.pLigature != nullptr) {
        genLgToWchar(tables.pLigature, tables.nLgMax, tables.cbLgEntry, ligatures_name);
    }

    // Generate main table.
    const WString kbd_table_name(L"kbd_tables");
    _alldata.push_back(DataStructure(kbd_table_name, &tables, sizeof(tables)));
    _ou << "//" << _opt.dashed << std::endl
        << "// Main keyboard layout structure, point to all tables" << std::endl
        << "//" << _opt.dashed << std::endl
        << std::endl
        << "static KBDTABLES " << kbd_table_name << " = {" << std::endl
        << "    .pCharModifiers  = " << pointer(tables.pCharModifiers, L"&" + char_modifiers_name) << "," << std::endl
        << "    .pVkToWcharTable = " << pointer(tables.pVkToWcharTable, vk_to_wchar_name) << "," << std::endl
        << "    .pDeadKey        = " << pointer(tables.pDeadKey, dead_keys_name) << "," << std::endl
        << "    .pKeyNames       = " << pointer(tables.pKeyNames, key_names_name) << "," << std::endl
        << "    .pKeyNamesExt    = " << pointer(tables.pKeyNamesExt, key_names_ext_name) << "," << std::endl
        << "    .pKeyNamesDead   = " << pointer(tables.pKeyNamesDead, key_names_dead_name) << "," << std::endl
        << "    .pusVSCtoVK      = " << pointer(tables.pusVSCtoVK, scancode_to_vk_name) << "," << std::endl
        << "    .bMaxVSCtoVK     = " << (tables.pusVSCtoVK == nullptr ? L"0," : "ARRAYSIZE(" + scancode_to_vk_name + "),") << std::endl
        << "    .pVSCtoVK_E0     = " << pointer(tables.pVSCtoVK_E0, scancode_to_vk_e0_name) << "," << std::endl
        << "    .pVSCtoVK_E1     = " << pointer(tables.pVSCtoVK_E1, scancode_to_vk_e1_name) << "," << std::endl
        << "    .fLocaleFlags    = " << localeFlags(tables.fLocaleFlags) << "," << std::endl
        << "    .nLgMax          = " << int(tables.nLgMax) << "," << std::endl
        << "    .cbLgEntry       = " << (tables.pLigature == nullptr ? L"0," : "sizeof(" + ligatures_name + "[0]),") << std::endl
        << "    .pLigature       = " << pointer(tables.pLigature, L"(PLIGATURE1)" + ligatures_name) << "," << std::endl
        << "    .dwType          = " << tables.dwType << "," << std::endl
        << "    .dwSubType       = " << tables.dwSubType << "," << std::endl
        << "};" << std::endl
        << std::endl
        << "//" << _opt.dashed << std::endl
        << "// Keyboard layout entry point" << std::endl
        << "//" << _opt.dashed << std::endl
        << std::endl
        << "__declspec(dllexport) PKBDTABLES " KBD_DLL_ENTRY_NAME "(void)" << std::endl
        << "{" << std::endl
        << "    return &" << kbd_table_name << ";" << std::endl
        << "}" << std::endl;

    // Dump file content.
    if (_opt.hexa_dump) {
        genHexaDump();
    }
}

//---------------------------------------------------------------------------

void SourceGenerator::genHexaDump()
{
    // Rearrange, merge, describe inter-structure spaces, etc.
    sortDataStructures();

    // Get system page size.
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    const size_t page_size = size_t(sysinfo.dwPageSize);

    const uintptr_t first_address = uintptr_t(_alldata.front().address);
    const uintptr_t last_address = uintptr_t(_alldata.back().end());
    const uintptr_t first_page = first_address - first_address % page_size;
    const uintptr_t last_page = last_address + (page_size - last_address % page_size) % page_size;

    _ou << std::endl
        << "//" << _opt.dashed << std::endl
        << "// Data structures dump" << std::endl
        << "//" << _opt.dashed << std::endl
        << "//" << std::endl
        << "// Total size: " << (last_page - first_page) << " bytes (" << ((last_page - first_page) / page_size) << " pages)" << std::endl
        << Format(L"// Base: 0x%08llX", size_t(first_page)) << std::endl
        << Format(L"// End:  0x%08llX", size_t(last_page)) << std::endl;

    // Dump start of memory page, before the first data structure.
    if (first_page < first_address) {
        const DataStructure ds(L"Start of memory page before first data structure", first_page, first_address - first_page);
        ds.dump(_ou);
    }

    // Dump all data structures.
    for (const auto& data : _alldata) {
        data.dump(_ou);
    }

    // Dump end of memory page after last structure.
    if (last_address < last_page) {
        const DataStructure ds(L"End of memory page after last data structure", last_address, last_page - last_address);
        ds.dump(_ou);
    }
}


//---------------------------------------------------------------------------
// Generate the partial resource file for WKL project.
//---------------------------------------------------------------------------

void GenerateResourceFile(ReverseOptions& opt, HMODULE hmod)
{
    // Extract file information from the file.
    FileVersionInfo info(opt);
    if (!info.load(hmod)) {
        opt.fatal("Error loading version information from " + opt.input);
    }

    // This is the information we need for the resource file.
    WString wkl_text(info.LayoutText);
    WString wkl_lang(info.BaseLanguage);

    // All possible matches from registry.
    WStringVector ids;
    WStringVector texts;

    // These strings are not empty if we reverse a WKL keyboard layout DLL.
    // Otherwise, look for the information somewhere else.
    if (wkl_text.empty() || wkl_lang.empty()) {

        // Get DLL name.
        const WString dllname(ToLower(FileName(opt.input)));

        // Enumerate keyboard layouts in registry to find an entry matching the DLL name.
        // Count the number matching entries, some DLL's are registered several times.
        Registry reg(opt);
        WStringList all_lang_ids;
        if (reg.getSubKeys(REGISTRY_LAYOUT_KEY, all_lang_ids)) {
            for (const auto& id : all_lang_ids) {
                // The base language is the last 4 hexa digits in layout id. need at least 4 chars.
                if (id.size() >= 4 && ToLower(reg.getValue(REGISTRY_LAYOUT_KEY "\\" + id, REGISTRY_LAYOUT_FILE, L"", true)) == dllname) {
                    WString text(reg.getValue(REGISTRY_LAYOUT_KEY "\\" + id, REGISTRY_LAYOUT_DISPLAY, L"", true));
                    if (text.empty()) {
                        text = reg.getValue(REGISTRY_LAYOUT_KEY "\\" + id, REGISTRY_LAYOUT_TEXT, L"", true);
                    }
                    ids.push_back(id);
                    texts.push_back(text);
                }
            }
        }
        if (wkl_lang.empty() && ids.empty()) {
            opt.fatal("unable to identify the base language for " + opt.input);
        }

        if (wkl_lang.empty()) {
            // The language is not known, keep the entry with the shortest (non empty) description.
            // When there are multiple entry, the shortest description is the base one, usually.
            // What would be the right strategy?
            size_t index = 0;
            size_t desc_length = WString::npos;
            for (size_t i = 0; i < ids.size(); i++) {
                const size_t len = texts[i].length();
                if (len > 0 && len < desc_length) {
                    index = i;
                    desc_length = len;
                }
            }
            wkl_lang = ids[index].substr(ids[index].size() - 4);
            if (wkl_text.empty()) {
                wkl_text = texts[index];
            }
        }
        else {
            // If the base language is already known, find a matching description.
            if (!wkl_lang.empty()) {
                for (size_t i = 0; wkl_text.empty() && i < ids.size(); i++) {
                    if (EndsWith(ToLower(wkl_lang), ToLower(ids[i]))) {
                        wkl_text = texts[i];
                    }
                }
            }
        }

        // If still nothing for description, fallback to version info.
        if (wkl_text.empty()) {
            wkl_text = info.FileDescription;
        }
    }

    // Content of the resource file.
    opt.out() << "#define WKL_TEXT \"" << wkl_text << "\"" << std::endl;
    opt.out() << "#define WKL_LANG \"" << wkl_lang << "\"" << std::endl;
    if (ids.size() > 1) {
        opt.out() << std::endl << "// Other possible matching entries:" << std::endl;
        for (size_t i = 0; i < ids.size(); i++) {
            opt.out() << "// " << ids[i] << ": \"" << texts[i] << "\"" << std::endl;
        }
    }
}


//---------------------------------------------------------------------------
// Application entry point.
//---------------------------------------------------------------------------

int wmain(int argc, wchar_t* argv[])
{
    // Parse command line options.
    ReverseOptions opt(argc, argv);

    // Resolve keyboard DLL file name.
    if (opt.input.find_first_of(L":\\/.") == WString::npos) {
        // No separator, must be a keyboard name, not a DLL file name.
        opt.input = GetEnv(L"SYSTEMROOT", L"C:\\Windows") + L"\\System32\\kbd" + opt.input + L".dll";
    }
 
    // Load the DLL in our virtual memory space.
    HMODULE dll = LoadLibraryW(opt.input.c_str());
    if (dll == nullptr) {
        const DWORD err = GetLastError();
        opt.fatal("error opening " + opt.input + ": " + ErrorText(err));
    }

    // Get the DLL entry point.
    FARPROC proc_addr = GetProcAddress(dll, KBD_DLL_ENTRY_NAME);
    if (proc_addr == nullptr) {
        const DWORD err = GetLastError();
        opt.fatal("cannot find " KBD_DLL_ENTRY_NAME " in " + opt.input + ": " + ErrorText(err));
    }

    // Call the entry point to get the keyboard tables.
    // The entry point profile is: PKBDTABLES KbdLayerDescriptor()
    PKBDTABLES tables = reinterpret_cast<PKBDTABLES(*)()>(proc_addr)();
    if (tables == nullptr) {
        opt.fatal(KBD_DLL_ENTRY_NAME "() returned null in " + opt.input);
    }

    // Open the output file when specified.
    opt.setOutput(opt.output);

    // Generate the source file.
    if (opt.gen_resources) {
        GenerateResourceFile(opt, dll);
    }
    else {
        SourceGenerator gen(opt);
        gen.generate(*tables);
    }
    opt.exit(EXIT_SUCCESS);
}