#include "json_util.h"
#include <fstream>
#include <sstream>
#include <cctype>
#include <cstdlib>

/* ==========================================================================
   4) MINI JSON READER / WRITER (เขียนเองแบบง่าย ไม่ใช้ไลบรารีภายนอก)
   เหมาะกับโครงสร้างข้อมูลคงที่ของโปรเจกต์นี้: อ่าน/เขียน array ของ object แบน ๆ
   ========================================================================== */
string readFileToString(const string &path) {
    ifstream fin(path.c_str());
    if (!fin.is_open()) return "";
    stringstream ss;
    ss << fin.rdbuf();
    return ss.str();
}

string jsonEscape(const string &s) {
    string out;
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else out += c;
    }
    return out;
}

// ดึงค่าฟิลด์แบบ string จาก object JSON เช่น "name": "PLA"
string jsonGetString(const string &obj, const string &key) {
    string pattern = "\"" + key + "\"";
    size_t p = obj.find(pattern);
    if (p == string::npos) return "";
    size_t colon = obj.find(':', p + pattern.size());
    if (colon == string::npos) return "";
    size_t q1 = obj.find('"', colon + 1);
    if (q1 == string::npos) return "";
    size_t i = q1 + 1;
    string result;
    while (i < obj.size() && obj[i] != '"') {
        if (obj[i] == '\\' && i + 1 < obj.size()) {
            char nc = obj[i + 1];
            if (nc == 'n') result += '\n';
            else result += nc;
            i += 2;
        } else {
            result += obj[i];
            i++;
        }
    }
    return result;
}

// ดึงค่าฟิลด์แบบตัวเลขจาก object JSON เช่น "price": 12.5
double jsonGetNumber(const string &obj, const string &key) {
    string pattern = "\"" + key + "\"";
    size_t p = obj.find(pattern);
    if (p == string::npos) return 0.0;
    size_t colon = obj.find(':', p + pattern.size());
    if (colon == string::npos) return 0.0;
    size_t i = colon + 1;
    while (i < obj.size() && (obj[i] == ' ' || obj[i] == '\n' || obj[i] == '\t' || obj[i] == '\r')) i++;
    size_t start = i;
    while (i < obj.size() && (isdigit((unsigned char)obj[i]) || obj[i] == '-' || obj[i] == '.'
           || obj[i] == 'e' || obj[i] == 'E' || obj[i] == '+')) i++;
    string numStr = obj.substr(start, i - start);
    if (numStr.empty()) return 0.0;
    return atof(numStr.c_str());
}

// ดึงค่าฟิลด์แบบ bool จาก object JSON เช่น "stockDeducted": true
bool jsonGetBool(const string &obj, const string &key) {
    string pattern = "\"" + key + "\"";
    size_t p = obj.find(pattern);
    if (p == string::npos) return false;
    size_t colon = obj.find(':', p + pattern.size());
    if (colon == string::npos) return false;
    size_t truePos = obj.find("true", colon);
    size_t falsePos = obj.find("false", colon);
    size_t commaOrBrace = obj.find_first_of(",}", colon);
    if (truePos != string::npos && truePos < commaOrBrace) return true;
    if (falsePos != string::npos && falsePos < commaOrBrace) return false;
    return false;
}

// แยก array ของ object ระดับบนสุดใน JSON ออกเป็น string ของแต่ละ object (นับวงเล็บปีกกา)
void splitJsonObjects(const string &content, string result[], int &count, int maxItems) {
    count = 0;
    size_t i = content.find('[');
    if (i == string::npos) return;
    i++;
    int depth = 0;
    size_t objStart = string::npos;
    for (; i < content.size() && count < maxItems; i++) {
        char c = content[i];
        if (c == '{') {
            if (depth == 0) objStart = i;
            depth++;
        } else if (c == '}') {
            depth--;
            if (depth == 0 && objStart != string::npos) {
                result[count++] = content.substr(objStart, i - objStart + 1);
                objStart = string::npos;
            }
        } else if (c == ']' && depth == 0) {
            break;
        }
    }
}
