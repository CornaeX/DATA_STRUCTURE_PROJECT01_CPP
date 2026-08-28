#ifndef UTILS_H
#define UTILS_H

/* ============================================================================
   utils.h
   ฟังก์ชันช่วยเหลือทั่วไป: จัดการหน้าจอ/คอนโซล, string, การรับข้อมูลจากผู้ใช้แบบมี
   validation, และการจัดคอลัมน์ตารางให้รองรับข้อความภาษาไทย (UTF-8 display width)
   ============================================================================ */

#include <string>
using namespace std;

/* ---- screen / console ---- */
void clearScreen();
void pause();

/* ---- string helpers ---- */
string trim(const string &s);
int extractNumber(const string &code);           // "C007" -> 7
string genCode(const string &prefix, int id);     // ("C", 7) -> "C007"
string toUpperStr(string s);
bool containsIgnoreCase(const string &haystack, const string &needle);

/* ---- user input (with EOF-safe save & exit) ---- */
int readIntInRange(const string &prompt, int lo, int hi);
double readPositiveDouble(const string &prompt);
bool readPositiveDoubleCancelable(const string &prompt, double &result); // 0 = ยกเลิก
string readLineTrim(const string &prompt);

/* ---- console table formatting ---- */
void printHeader(const string &title);
void printLine();
int displayWidth(const string &s);                // ความกว้างที่แสดงผลจริงของ UTF-8/ไทย
string padRight(const string &s, int width);       // จัดคอลัมน์ให้ตรงกันแม้มีภาษาไทย

#endif // UTILS_H
