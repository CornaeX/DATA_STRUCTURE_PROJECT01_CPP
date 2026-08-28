#ifndef JSON_UTIL_H
#define JSON_UTIL_H

/* ============================================================================
   json_util.h
   Mini JSON reader/writer ที่เขียนขึ้นเอง (ไม่พึ่งไลบรารีภายนอก) สำหรับอ่าน/เขียน
   array ของ object แบน ๆ ตามโครงสร้างข้อมูลคงที่ของโปรเจกต์นี้
   ============================================================================ */

#include <string>
using namespace std;

string readFileToString(const string &path);
string jsonEscape(const string &s);

// ดึงค่าฟิลด์จาก object JSON เช่น "name": "PLA"
string jsonGetString(const string &obj, const string &key);
double jsonGetNumber(const string &obj, const string &key);
bool jsonGetBool(const string &obj, const string &key);

// แยก array ของ object ระดับบนสุดใน JSON ออกเป็น string ของแต่ละ object (นับวงเล็บปีกกา)
void splitJsonObjects(const string &content, string result[], int &count, int maxItems);

#endif // JSON_UTIL_H
