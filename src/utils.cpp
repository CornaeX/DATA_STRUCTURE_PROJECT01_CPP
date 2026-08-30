#include "utils.h"
#include "types.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <cctype>
#include <limits>
#include <climits>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

// forward declaration: saveAll() นิยามจริงอยู่ใน storage.cpp
// ใช้บันทึกข้อมูลก่อนออกจากโปรแกรมเมื่อเจอ EOF ระหว่างรับข้อมูลจากผู้ใช้
void saveAll();

/* ==========================================================================
   เมนูคลิกได้ด้วยเมาส์ (Windows console เท่านั้น) - ผู้ใช้ยังพิมพ์ตัวเลขเองได้ตามปกติ
   แนวคิด: ทุกครั้งที่พิมพ์ตัวเลือกเมนูด้วย printMenuOption() จะจำตำแหน่งบนจอ (แถว/คอลัมน์)
   ของ "[n]" ไว้ใน g_clickRegions จากนั้น readIntInRange() จะดักฟังทั้งคีย์บอร์ด (พิมพ์เลข+Enter
   ตามปกติ) และเมาส์ (คลิกซ้ายตรงตำแหน่งที่จำไว้ = เท่ากับพิมพ์เลขนั้นแล้ว Enter)
   ========================================================================== */
#ifdef _WIN32
struct ClickRegion { SHORT row; SHORT colStart; SHORT colEnd; int value; };
static vector<ClickRegion> g_clickRegions;
static bool g_isRealConsole = false; // false เช่นตอน stdin ถูก redirect จากไฟล์/pipe -> ไม่มี mouse event ให้อ่าน ต้องใช้ cin แบบเดิม

// เปิดโหมดรับ mouse event ของ console และปิด Quick Edit Mode ทุกครั้งก่อนจะรออ่านค่าจากผู้ใช้
// (ไม่ใช่แค่ครั้งแรกครั้งเดียว) เพราะบางกรณี terminal/OS อาจรีเซ็ตโหมดกลับไปเองระหว่างทาง
// เช่นถ้าเปิดแค่ครั้งเดียวแล้วมีอะไรไปรีเซ็ตโหมดทีหลัง จะทำให้คลิกใช้ไม่ได้อีกเลยตลอดโปรแกรม
static void applyMouseConsoleMode() {
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(hIn, &mode)) {
        g_isRealConsole = true;
        mode |= ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS;
        mode &= ~ENABLE_QUICK_EDIT_MODE;
        SetConsoleMode(hIn, mode);
    } else {
        g_isRealConsole = false; // stdin ไม่ใช่ console จริง (เช่นถูก redirect)
    }
}

// เอา event ที่ไม่ใช่คีย์บอร์ด (mouse/focus/resize) ที่ค้างอยู่ "หน้าคิว" input buffer ออกก่อน
// ไม่งั้นถ้าไปเรียก cin/getline() ต่อ (เช่น readLineTrim) แล้วเจอ mouse event ค้างอยู่ อาจทำให้
// การอ่านบรรทัดถัดไปพัง หรือทำให้ console มีพฤติกรรมเพี้ยนจนคลิกใช้ไม่ได้อีกในรอบถัดไป
// หยุดทันทีที่เจอ KEY_EVENT ตัวแรก เพื่อไม่ไปแตะต้อง/กินคีย์ที่ผู้ใช้พิมพ์จริง ๆ
static void flushPendingMouseEvents() {
    if (!g_isRealConsole) return;
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    while (true) {
        DWORD count = 0;
        if (!GetNumberOfConsoleInputEvents(hIn, &count) || count == 0) break;
        INPUT_RECORD rec;
        DWORD nPeek = 0;
        if (!PeekConsoleInputW(hIn, &rec, 1, &nPeek) || nPeek == 0) break;
        if (rec.EventType == KEY_EVENT) break; // เจอคีย์บอร์ดจริง หยุด ปล่อยให้ cin อ่านต่อตามปกติ
        DWORD nRead = 0;
        ReadConsoleInputW(hIn, &rec, 1, &nRead); // ทิ้ง event ที่ไม่ใช่คีย์บอร์ดทิ้งไป
    }
}

// หาว่าตำแหน่งที่คลิก (row, col) ตรงกับตัวเลือกเมนูที่จำไว้ตัวไหนหรือไม่ ไม่เจอคืน INT_MIN
static int findClickedMenuValue(SHORT row, SHORT col) {
    for (size_t i = 0; i < g_clickRegions.size(); i++) {
        const ClickRegion &r = g_clickRegions[i];
        if (r.row == row && col >= r.colStart && col < r.colEnd) return r.value;
    }
    return INT_MIN;
}
#endif

void printMenuOption(int number, const string &label, const string &color) {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info;
    bool haveInfo = GetConsoleScreenBufferInfo(hOut, &info) != 0;
    SHORT row = 0, colStart = 0;
    if (haveInfo) { row = info.dwCursorPosition.Y; colStart = info.dwCursorPosition.X; }
#endif
    cout << color << "  [" << number << "] " << RESET;
#ifdef _WIN32
    if (haveInfo && GetConsoleScreenBufferInfo(hOut, &info)) {
        g_clickRegions.push_back({row, colStart, info.dwCursorPosition.X, number});
    }
#endif
    cout << label << "\n";
}

/* ==========================================================================
   3) UTILITY FUNCTIONS
   ========================================================================== */
void clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void pause() {
#ifdef _WIN32
    applyMouseConsoleMode();
    flushPendingMouseEvents();
#endif
    cout << YELLOW << "\n  กด Enter เพื่อดำเนินการต่อ..." << RESET;
    cin.clear();
    string dummy;
    getline(cin, dummy);
}

string trim(const string &s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// ดึงตัวเลขท้ายรหัส เช่น "C007" -> 7
int extractNumber(const string &code) {
    string digits = "";
    for (size_t i = 0; i < code.size(); i++) {
        if (isdigit((unsigned char)code[i])) digits += code[i];
    }
    if (digits.empty()) return 0;
    return atoi(digits.c_str());
}

string genCode(const string &prefix, int id) {
    stringstream ss;
    ss << prefix << setw(3) << setfill('0') << id;
    return ss.str();
}

string toUpperStr(string s) {
    for (size_t i = 0; i < s.size(); i++) s[i] = toupper((unsigned char)s[i]);
    return s;
}

bool containsIgnoreCase(const string &haystack, const string &needle) {
    string h = toUpperStr(haystack), n = toUpperStr(needle);
    return h.find(n) != string::npos;
}

#ifdef _WIN32
// เวอร์ชัน Windows: รับได้ทั้งพิมพ์ตัวเลข+Enter ตามปกติ "และ" คลิกเมาส์ซ้ายที่ตัวเลือกเมนู
// (ตำแหน่งที่คลิกได้มาจาก printMenuOption() ที่เรียกไว้ก่อนหน้า) ถ้าตัวเลือกนั้นไม่ได้ลงทะเบียนไว้
// (เช่น prompt ตัวเลขที่ไม่ใช่เมนู) ก็ยังพิมพ์ตัวเลขเองได้เหมือนเดิมทุกประการ
static int readIntInRangeClassic(const string &prompt, int lo, int hi) {
    int v;
    while (true) {
        cout << prompt;
        if (cin >> v && v >= lo && v <= hi) {
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            return v;
        }
        if (cin.eof()) {
            cout << RED << "\n  ไม่มีข้อมูลนำเข้าเหลือแล้ว (EOF) กำลังบันทึกและออกจากโปรแกรม...\n" << RESET;
            saveAll();
            exit(0);
        }
        cout << RED << "  ค่าไม่ถูกต้อง กรุณากรอกใหม่ (" << lo << "-" << hi << ")\n" << RESET;
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
}

int readIntInRange(const string &prompt, int lo, int hi) {
    applyMouseConsoleMode();
    if (!g_isRealConsole) return readIntInRangeClassic(prompt, lo, hi); // stdin ถูก redirect -> ไม่มี mouse event ให้ดัก ใช้วิธีเดิม
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);

    while (true) {
        cout << prompt << flush;
        string buf;
        bool gotEnter = false;
        int clickedValue = INT_MIN;

        while (true) {
            INPUT_RECORD rec;
            DWORD nRead = 0;
            if (!ReadConsoleInputW(hIn, &rec, 1, &nRead) || nRead == 0) continue;

            if (rec.EventType == MOUSE_EVENT) {
                const MOUSE_EVENT_RECORD &m = rec.Event.MouseEvent;
                // dwEventFlags == 0 คือ event การกดปุ่มเมาส์ (ไม่ใช่ move/double-click/wheel)
                if (m.dwEventFlags == 0 && (m.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED)) {
                    int v = findClickedMenuValue(m.dwMousePosition.Y, m.dwMousePosition.X);
                    if (v != INT_MIN && v >= lo && v <= hi) {
                        clickedValue = v;
                        break;
                    }
                }
                continue;
            }

            if (rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown) continue;
            wchar_t ch = rec.Event.KeyEvent.uChar.UnicodeChar;

            if (ch == L'\r') {
                if (buf.empty()) continue; // Enter เปล่า ๆ ไม่มีผล เหมือนพฤติกรรมเดิมของ cin
                cout << "\n";
                gotEnter = true;
                break;
            } else if (ch == 8 || ch == 127) { // Backspace
                if (!buf.empty()) { buf.erase(buf.size() - 1); cout << "\b \b" << flush; }
            } else if (ch >= L'0' && ch <= L'9') {
                buf += (char) ch;
                cout << (char) ch << flush;
            }
            // อักขระอื่น (ตัวอักษร, Ctrl combos ฯลฯ) ไม่รับ เหมือน cin >> int เดิมที่รับเฉพาะตัวเลข
        }

        if (clickedValue != INT_MIN) return clickedValue;

        if (gotEnter) {
            try {
                size_t pos;
                int v = stoi(buf, &pos);
                if (pos == buf.size() && v >= lo && v <= hi) return v;
            } catch (...) { /* fall through to error message below */ }
        }

        cout << RED << "  ค่าไม่ถูกต้อง กรุณากรอกใหม่ (" << lo << "-" << hi << ")\n" << RESET;
    }
}
#else
int readIntInRange(const string &prompt, int lo, int hi) {
    int v;
    while (true) {
        cout << prompt;
        if (cin >> v && v >= lo && v <= hi) {
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            return v;
        }
        if (cin.eof()) {
            cout << RED << "\n  ไม่มีข้อมูลนำเข้าเหลือแล้ว (EOF) กำลังบันทึกและออกจากโปรแกรม...\n" << RESET;
            saveAll();
            exit(0);
        }
        cout << RED << "  ค่าไม่ถูกต้อง กรุณากรอกใหม่ (" << lo << "-" << hi << ")\n" << RESET;
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
}
#endif

double readPositiveDouble(const string &prompt) {
#ifdef _WIN32
    applyMouseConsoleMode();
    flushPendingMouseEvents();
#endif
    double v;
    while (true) {
        cout << prompt;
        if (cin >> v && v > 0) {
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            return v;
        }
        if (cin.eof()) {
            cout << RED << "\n  ไม่มีข้อมูลนำเข้าเหลือแล้ว (EOF) กำลังบันทึกและออกจากโปรแกรม...\n" << RESET;
            saveAll();
            exit(0);
        }
        cout << RED << "  กรุณากรอกตัวเลขที่มากกว่า 0\n" << RESET;
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
}

// เหมือน readPositiveDouble แต่ถ้าผู้ใช้พิมพ์ 0 แล้ว Enter จะถือว่า "ยกเลิก" (คืนค่า false)
bool readPositiveDoubleCancelable(const string &prompt, double &result) {
#ifdef _WIN32
    applyMouseConsoleMode();
#endif
    string line;
    while (true) {
        cout << prompt;
#ifdef _WIN32
        flushPendingMouseEvents();
#endif
        if (!getline(cin, line)) {
            cout << RED << "\n  ไม่มีข้อมูลนำเข้าเหลือแล้ว (EOF) กำลังบันทึกและออกจากโปรแกรม...\n" << RESET;
            saveAll();
            exit(0);
        }
        line = trim(line);
        if (line == "0") return false; // ผู้ใช้ขอยกเลิก
        stringstream ss(line);
        double v;
        if ((ss >> v) && v > 0) {
            result = v;
            return true;
        }
        cout << RED << "  กรุณากรอกตัวเลขที่มากกว่า 0 (หรือพิมพ์ 0 เพื่อยกเลิก)\n" << RESET;
    }
}

string readLineTrim(const string &prompt) {
#ifdef _WIN32
    applyMouseConsoleMode();
    flushPendingMouseEvents();
#endif
    cout << prompt;
    string s;
    if (!getline(cin, s)) {
        cout << RED << "\n  ไม่มีข้อมูลนำเข้าเหลือแล้ว (EOF) กำลังบันทึกและออกจากโปรแกรม...\n" << RESET;
        saveAll();
        exit(0);
    }
    return trim(s);
}

void printHeader(const string &title) {
#ifdef _WIN32
    // หน้าจอเมนูใหม่เริ่มต้นแล้ว -> ล้างตำแหน่งตัวเลือกที่คลิกได้ของหน้าจอก่อนหน้าทิ้ง
    g_clickRegions.clear();
#endif
    cout << CYAN << BOLD;
    cout << "=========================================================================================\n";
    cout << "   " << title << "\n";
    cout << "=========================================================================================\n" << RESET;
}

void printLine() {
    cout << "-----------------------------------------------------------------------------------------\n";
}

// --------------------------------------------------------------------------
// จัดคอลัมน์ตารางให้ตรงกัน (แก้บั๊กหัวตารางไม่ตรงกับข้อมูล)
// ปัญหาเดิม: std::setw() นับความกว้างจาก "จำนวนไบต์" ไม่ใช่ "จำนวนตัวอักษรที่เห็นบนจอ"
// ภาษาไทยใน UTF-8 กินไบต์ตัวละ 3 ไบต์ (เช่น "รหัส" = 4 ตัวอักษร แต่ 12 ไบต์)
// พอ setw(8) เจอสตริงยาว 12 ไบต์ มันคิดว่ากว้างเกินคอลัมน์แล้ว เลยไม่เติมช่องว่างให้เลย
// ทำให้หัวตารางภาษาไทยแต่ละคอลัมน์ติดกันเป็นพืด ในขณะที่แถวข้อมูล (ซึ่งมักเป็นรหัส/ตัวเลขภาษาอังกฤษ
// ที่ 1 ไบต์ = 1 ตัวอักษรพอดี) กลับเติมช่องว่างถูกต้อง จึงทำให้หัวตารางกับข้อมูลไม่ตรงคอลัมน์กัน
// วิธีแก้: คำนวณ "ความกว้างที่แสดงผลจริง" เอง (นับทีละอักขระ UTF-8 ไม่ใช่ทีละไบต์)
//         แล้วเติมช่องว่างเองแทนการใช้ std::setw() กับข้อความที่อาจมีภาษาไทย
// --------------------------------------------------------------------------

// คำนวณความกว้างที่แสดงผลจริงของสตริง UTF-8 (จำนวนอักขระที่เห็นบนจอ ไม่ใช่จำนวนไบต์)
// สระ/วรรณยุกต์ลอยของภาษาไทย (เช่น ่ ้ ๊ ๋ ั ิ ี ึ ื ุ ู ์) เป็นอักขระประกอบ (combining mark)
// ที่วางซ้อนบนพยัญชนะ ไม่กินความกว้างหน้าจอเพิ่ม จึงนับเป็น 0 ช่อง
int displayWidth(const string &s) {
    int width = 0;
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = (unsigned char) s[i];
        int len = 1;
        unsigned int cp = c;
        if ((c & 0x80) == 0x00) { len = 1; cp = c; }
        else if ((c & 0xE0) == 0xC0) { len = 2; cp = c & 0x1F; }
        else if ((c & 0xF0) == 0xE0) { len = 3; cp = c & 0x0F; }
        else if ((c & 0xF8) == 0xF0) { len = 4; cp = c & 0x07; }
        for (int k = 1; k < len && i + k < s.size(); k++) {
            cp = (cp << 6) | ((unsigned char) s[i + k] & 0x3F);
        }
        bool isThaiCombining =
            (cp == 0x0E31) ||                 // สระอำ/ไม้หันอากาศ (MAI HAN-AKAT)
            (cp >= 0x0E34 && cp <= 0x0E3A) ||  // สระอิ อี อึ อื อุ อู พินทุ
            (cp >= 0x0E47 && cp <= 0x0E4E);    // ไม้ไต่คู้ วรรณยุกต์เอก-โท-ตรี-จัตวา ทัณฑฆาต นิคหิต
        if (!isThaiCombining) width += 1;
        i += (size_t) len;
    }
    return width;
}

// จัดข้อความชิดซ้าย เติมช่องว่างด้านขวาให้ครบ "ความกว้างที่แสดงผลจริง" ตามที่กำหนด (ใช้แทน std::setw ทุกจุดที่คอลัมน์อาจมีภาษาไทย)
string padRight(const string &s, int width) {
    int w = displayWidth(s);
    if (w >= width) return s; // ยาวเกินคอลัมน์แล้ว ปล่อยตามจริง (คอลัมน์ถัดไปอาจขยับบ้างถ้าข้อความยาวผิดปกติ)
    return s + string((size_t)(width - w), ' ');
}
