#include "utils.h"
#include "types.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <cctype>
#include <limits>

// forward declaration: saveAll() นิยามจริงอยู่ใน storage.cpp
// ใช้บันทึกข้อมูลก่อนออกจากโปรแกรมเมื่อเจอ EOF ระหว่างรับข้อมูลจากผู้ใช้
void saveAll();

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

double readPositiveDouble(const string &prompt) {
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
    string line;
    while (true) {
        cout << prompt;
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
