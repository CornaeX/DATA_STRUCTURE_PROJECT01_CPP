/* ============================================================================
   ระบบจัดการร้าน 3D Printing (3D Printing Shop Management System)
   Text-based / Console UI (TUI/CUI)
   - ใช้ Array ล้วนในการเก็บข้อมูล (ไม่ใช้ vector/STL container)
   - Create (โหลดจากไฟล์ .json), Search, Insert, Delete
   - จัดการ ลูกค้า / วัสดุ(พร้อมสี) / เครื่องพิมพ์(พร้อมประเภท) / ออเดอร์(พร้อมไฟล์งาน) / คิวงาน / POS
   - สถานะออเดอร์: Queued -> Printing -> Completed -> Paid -> PickedUp (หรือ Cancelled)
   - ข้อมูลทั้งหมดบันทึกเป็นไฟล์ .json (เขียน/อ่านด้วยฟังก์ชัน JSON เล็ก ๆ ที่เขียนขึ้นเอง
     ไม่พึ่งไลบรารีภายนอก เพื่อให้คอมไพล์ได้ด้วย g++ ธรรมดา)
   ============================================================================ */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <iomanip>
#include <cstdlib>
#include <ctime>
#include <limits>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;

/* ==========================================================================
   0) CONSTANTS / ANSI COLOR / GLOBAL SIZES
   ========================================================================== */
const int MAX_CUSTOMERS = 200;
const int MAX_MATERIALS = 100;
const int MAX_PRINTERS  = 50;
const int MAX_ORDERS    = 500;
const int MAX_SALES     = 1000;

const double HOURLY_RATE   = 20.0;  // บาท/ชั่วโมง (ค่าไฟ+ค่าเสื่อมเครื่อง)
const double BASE_FEE      = 20.0;  // ค่าดำเนินการเริ่มต้นต่อออเดอร์
const double PRINT_SPEED_G_PER_HR = 15.0; // ความเร็วพิมพ์โดยประมาณ (กรัม/ชม.)

const string F_CUSTOMERS = "customers.json";
const string F_MATERIALS = "materials.json";
const string F_PRINTERS  = "printers.json";
const string F_ORDERS    = "orders.json";
const string F_SALES     = "sales_history.json";

#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define CYAN    "\033[36m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define RED     "\033[31m"
#define MAGENTA "\033[35m"
#define BLUE    "\033[34m"

/* ==========================================================================
   1) STRUCTS
   ========================================================================== */
struct Customer {
    string code, name, phone, address;
};

struct Material {
    string code, name, color;
    double pricePerGram;
    double stockGram;
};

struct Printer {
    string code, name, type, status;  // status: Idle / Printing / Maintenance
    string currentOrder;              // รหัสออเดอร์ที่กำลังพิมพ์อยู่ ("-" ถ้าว่าง)
};

struct Order {
    string code, customerCode, materialCode, printerCode, fileName;
    double weight;    // กรัม
    double hours;     // ชั่วโมงประมาณการ
    double price;     // ราคารวม
    string status;    // Queued / Printing / Completed / Paid / PickedUp / Cancelled
    bool stockDeducted; // true เมื่อหักสต็อกไปแล้ว (ตอนเริ่มพิมพ์จริง) ใช้ตัดสินใจตอนคืนสต็อก
    time_t startTime;  // เวลาที่เริ่มพิมพ์จริง (Unix timestamp) ใช้คำนวณเวลาที่เหลือ / เช็คว่าพิมพ์เสร็จหรือยัง (0 = ยังไม่เริ่ม)
};

struct SalesRecord {
    string date, orderCode, customerName, materialName, color;
    double price, cash, change;
};

/* ==========================================================================
   2) GLOBAL ARRAYS + COUNTERS
   ========================================================================== */
Customer customers[MAX_CUSTOMERS];
int customerCount = 0;
int nextCustomerId = 1;

Material materials[MAX_MATERIALS];
int materialCount = 0;
int nextMaterialId = 1;

Printer printers[MAX_PRINTERS];
int printerCount = 0;
int nextPrinterId = 1;

Order orders[MAX_ORDERS];
int orderCount = 0;
int nextOrderId = 1;

SalesRecord salesHistory[MAX_SALES];
int salesCount = 0;

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

void saveAll(); // forward declaration (นิยามจริงอยู่ด้านล่าง) ใช้บันทึกข้อมูลก่อนออกเมื่อเจอ EOF ระหว่างรับข้อมูล

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
    cout << "==================================================================\n";
    cout << "   " << title << "\n";
    cout << "==================================================================\n" << RESET;
}

void printLine() {
    cout << "------------------------------------------------------------------\n";
}

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

/* ==========================================================================
   5) LOAD (Create) FUNCTIONS  -- อ่านจาก JSON เก็บลง Array
   ========================================================================== */
void loadCustomers() {
    customerCount = 0;
    string content = readFileToString(F_CUSTOMERS);
    if (content.empty()) return;
    string objs[MAX_CUSTOMERS]; int n;
    splitJsonObjects(content, objs, n, MAX_CUSTOMERS);
    for (int i = 0; i < n; i++) {
        customers[customerCount].code = jsonGetString(objs[i], "code");
        customers[customerCount].name = jsonGetString(objs[i], "name");
        customers[customerCount].phone = jsonGetString(objs[i], "phone");
        customers[customerCount].address = jsonGetString(objs[i], "address");
        int num = extractNumber(customers[customerCount].code);
        if (num + 1 > nextCustomerId) nextCustomerId = num + 1;
        customerCount++;
    }
}

void loadMaterials() {
    materialCount = 0;
    string content = readFileToString(F_MATERIALS);
    if (content.empty()) return;
    string objs[MAX_MATERIALS]; int n;
    splitJsonObjects(content, objs, n, MAX_MATERIALS);
    for (int i = 0; i < n; i++) {
        materials[materialCount].code = jsonGetString(objs[i], "code");
        materials[materialCount].name = jsonGetString(objs[i], "name");
        materials[materialCount].color = jsonGetString(objs[i], "color");
        materials[materialCount].pricePerGram = jsonGetNumber(objs[i], "pricePerGram");
        materials[materialCount].stockGram = jsonGetNumber(objs[i], "stockGram");
        int num = extractNumber(materials[materialCount].code);
        if (num + 1 > nextMaterialId) nextMaterialId = num + 1;
        materialCount++;
    }
}

void loadPrinters() {
    printerCount = 0;
    string content = readFileToString(F_PRINTERS);
    if (content.empty()) return;
    string objs[MAX_PRINTERS]; int n;
    splitJsonObjects(content, objs, n, MAX_PRINTERS);
    for (int i = 0; i < n; i++) {
        printers[printerCount].code = jsonGetString(objs[i], "code");
        printers[printerCount].name = jsonGetString(objs[i], "name");
        printers[printerCount].type = jsonGetString(objs[i], "type");
        printers[printerCount].status = jsonGetString(objs[i], "status");
        printers[printerCount].currentOrder = jsonGetString(objs[i], "currentOrder");
        int num = extractNumber(printers[printerCount].code);
        if (num + 1 > nextPrinterId) nextPrinterId = num + 1;
        printerCount++;
    }
}

void loadOrders() {
    orderCount = 0;
    string content = readFileToString(F_ORDERS);
    if (content.empty()) return;
    string objs[MAX_ORDERS]; int n;
    splitJsonObjects(content, objs, n, MAX_ORDERS);
    for (int i = 0; i < n; i++) {
        orders[orderCount].code = jsonGetString(objs[i], "code");
        orders[orderCount].customerCode = jsonGetString(objs[i], "customerCode");
        orders[orderCount].materialCode = jsonGetString(objs[i], "materialCode");
        orders[orderCount].printerCode = jsonGetString(objs[i], "printerCode");
        orders[orderCount].fileName = jsonGetString(objs[i], "fileName");
        orders[orderCount].weight = jsonGetNumber(objs[i], "weight");
        orders[orderCount].hours = jsonGetNumber(objs[i], "hours");
        orders[orderCount].price = jsonGetNumber(objs[i], "price");
        orders[orderCount].status = jsonGetString(objs[i], "status");
        orders[orderCount].stockDeducted = jsonGetBool(objs[i], "stockDeducted");
        orders[orderCount].startTime = (time_t) jsonGetNumber(objs[i], "startTime");
        int num = extractNumber(orders[orderCount].code);
        if (num + 1 > nextOrderId) nextOrderId = num + 1;
        orderCount++;
    }
}

void loadSalesHistory() {
    salesCount = 0;
    string content = readFileToString(F_SALES);
    if (content.empty()) return;
    string objs[MAX_SALES]; int n;
    splitJsonObjects(content, objs, n, MAX_SALES);
    for (int i = 0; i < n; i++) {
        salesHistory[salesCount].date = jsonGetString(objs[i], "date");
        salesHistory[salesCount].orderCode = jsonGetString(objs[i], "orderCode");
        salesHistory[salesCount].customerName = jsonGetString(objs[i], "customerName");
        salesHistory[salesCount].materialName = jsonGetString(objs[i], "materialName");
        salesHistory[salesCount].color = jsonGetString(objs[i], "color");
        salesHistory[salesCount].price = jsonGetNumber(objs[i], "price");
        salesHistory[salesCount].cash = jsonGetNumber(objs[i], "cash");
        salesHistory[salesCount].change = jsonGetNumber(objs[i], "change");
        salesCount++;
    }
}

/* ==========================================================================
   6) SAVE FUNCTIONS -- บันทึกกลับลง JSON
   ========================================================================== */
void saveCustomers() {
    ofstream fout(F_CUSTOMERS.c_str());
    fout << "[\n";
    for (int i = 0; i < customerCount; i++) {
        fout << "  {\n";
        fout << "    \"code\": \"" << jsonEscape(customers[i].code) << "\",\n";
        fout << "    \"name\": \"" << jsonEscape(customers[i].name) << "\",\n";
        fout << "    \"phone\": \"" << jsonEscape(customers[i].phone) << "\",\n";
        fout << "    \"address\": \"" << jsonEscape(customers[i].address) << "\"\n";
        fout << "  }" << (i < customerCount - 1 ? "," : "") << "\n";
    }
    fout << "]\n";
    fout.close();
}

void saveMaterials() {
    ofstream fout(F_MATERIALS.c_str());
    fout << "[\n";
    for (int i = 0; i < materialCount; i++) {
        fout << "  {\n";
        fout << "    \"code\": \"" << jsonEscape(materials[i].code) << "\",\n";
        fout << "    \"name\": \"" << jsonEscape(materials[i].name) << "\",\n";
        fout << "    \"color\": \"" << jsonEscape(materials[i].color) << "\",\n";
        fout << fixed << setprecision(2);
        fout << "    \"pricePerGram\": " << materials[i].pricePerGram << ",\n";
        fout << "    \"stockGram\": " << materials[i].stockGram << "\n";
        fout << "  }" << (i < materialCount - 1 ? "," : "") << "\n";
    }
    fout << "]\n";
    fout.close();
}

void savePrinters() {
    ofstream fout(F_PRINTERS.c_str());
    fout << "[\n";
    for (int i = 0; i < printerCount; i++) {
        fout << "  {\n";
        fout << "    \"code\": \"" << jsonEscape(printers[i].code) << "\",\n";
        fout << "    \"name\": \"" << jsonEscape(printers[i].name) << "\",\n";
        fout << "    \"type\": \"" << jsonEscape(printers[i].type) << "\",\n";
        fout << "    \"status\": \"" << jsonEscape(printers[i].status) << "\",\n";
        fout << "    \"currentOrder\": \"" << jsonEscape(printers[i].currentOrder) << "\"\n";
        fout << "  }" << (i < printerCount - 1 ? "," : "") << "\n";
    }
    fout << "]\n";
    fout.close();
}

void saveOrders() {
    ofstream fout(F_ORDERS.c_str());
    fout << "[\n";
    for (int i = 0; i < orderCount; i++) {
        fout << "  {\n";
        fout << "    \"code\": \"" << jsonEscape(orders[i].code) << "\",\n";
        fout << "    \"customerCode\": \"" << jsonEscape(orders[i].customerCode) << "\",\n";
        fout << "    \"materialCode\": \"" << jsonEscape(orders[i].materialCode) << "\",\n";
        fout << "    \"printerCode\": \"" << jsonEscape(orders[i].printerCode) << "\",\n";
        fout << "    \"fileName\": \"" << jsonEscape(orders[i].fileName) << "\",\n";
        fout << fixed << setprecision(2);
        fout << "    \"weight\": " << orders[i].weight << ",\n";
        fout << "    \"hours\": " << orders[i].hours << ",\n";
        fout << "    \"price\": " << orders[i].price << ",\n";
        fout << "    \"status\": \"" << jsonEscape(orders[i].status) << "\",\n";
        fout << "    \"stockDeducted\": " << (orders[i].stockDeducted ? "true" : "false") << ",\n";
        fout << "    \"startTime\": " << (long) orders[i].startTime << "\n";
        fout << "  }" << (i < orderCount - 1 ? "," : "") << "\n";
    }
    fout << "]\n";
    fout.close();
}

void saveSalesHistory() {
    ofstream fout(F_SALES.c_str());
    fout << "[\n";
    for (int i = 0; i < salesCount; i++) {
        fout << "  {\n";
        fout << "    \"date\": \"" << jsonEscape(salesHistory[i].date) << "\",\n";
        fout << "    \"orderCode\": \"" << jsonEscape(salesHistory[i].orderCode) << "\",\n";
        fout << "    \"customerName\": \"" << jsonEscape(salesHistory[i].customerName) << "\",\n";
        fout << "    \"materialName\": \"" << jsonEscape(salesHistory[i].materialName) << "\",\n";
        fout << "    \"color\": \"" << jsonEscape(salesHistory[i].color) << "\",\n";
        fout << fixed << setprecision(2);
        fout << "    \"price\": " << salesHistory[i].price << ",\n";
        fout << "    \"cash\": " << salesHistory[i].cash << ",\n";
        fout << "    \"change\": " << salesHistory[i].change << "\n";
        fout << "  }" << (i < salesCount - 1 ? "," : "") << "\n";
    }
    fout << "]\n";
    fout.close();
}

void saveAll() {
    saveCustomers();
    saveMaterials();
    savePrinters();
    saveOrders();
    saveSalesHistory();
    cout << GREEN << "  บันทึกข้อมูลทั้งหมดลงไฟล์ JSON เรียบร้อยแล้ว\n" << RESET;
}

void appendSalesHistory(const SalesRecord &r) {
    if (salesCount < MAX_SALES) {
        salesHistory[salesCount++] = r;
        saveSalesHistory();
    }
}

/* ==========================================================================
   7) SEARCH (return index หรือ -1)
   ========================================================================== */
int findCustomerIndex(const string &key) {
    for (int i = 0; i < customerCount; i++)
        if (toUpperStr(customers[i].code) == toUpperStr(key)) return i;
    return -1;
}
int findMaterialIndex(const string &key) {
    for (int i = 0; i < materialCount; i++)
        if (toUpperStr(materials[i].code) == toUpperStr(key)) return i;
    return -1;
}
int findPrinterIndex(const string &key) {
    for (int i = 0; i < printerCount; i++)
        if (toUpperStr(printers[i].code) == toUpperStr(key)) return i;
    return -1;
}
int findOrderIndex(const string &key) {
    for (int i = 0; i < orderCount; i++)
        if (toUpperStr(orders[i].code) == toUpperStr(key)) return i;
    return -1;
}

/* ==========================================================================
   8) DISPLAY (LIST) FUNCTIONS
   ========================================================================== */
void listCustomers() {
    printHeader("รายชื่อลูกค้าทั้งหมด");
    if (customerCount == 0) { cout << "  (ไม่มีข้อมูล)\n"; return; }
    cout << left << setw(8) << "รหัส" << setw(20) << "ชื่อ"
         << setw(15) << "เบอร์โทร" << "ที่อยู่\n";
    printLine();
    for (int i = 0; i < customerCount; i++) {
        cout << left << setw(8) << customers[i].code << setw(20) << customers[i].name
             << setw(15) << customers[i].phone << customers[i].address << "\n";
    }
}

void listMaterials() {
    printHeader("รายการวัสดุทั้งหมด");
    if (materialCount == 0) { cout << "  (ไม่มีข้อมูล)\n"; return; }
    cout << left << setw(8) << "รหัส" << setw(14) << "ชื่อวัสดุ" << setw(10) << "สี"
         << setw(14) << "ราคา/กรัม" << "คงเหลือ(กรัม)\n";
    printLine();
    for (int i = 0; i < materialCount; i++) {
        cout << left << setw(8) << materials[i].code << setw(14) << materials[i].name
             << setw(10) << materials[i].color
             << setw(14) << fixed << setprecision(2) << materials[i].pricePerGram;
        if (materials[i].stockGram <= 100) cout << RED;
        cout << materials[i].stockGram << RESET << "\n";
    }
}

// แปลงจำนวนชั่วโมง (double) เป็นข้อความ "Xชม Yนาที" อ่านง่าย
string formatDuration(double hours) {
    if (hours < 0) hours = 0;
    int totalMinutes = (int) (hours * 60.0 + 0.5);
    int h = totalMinutes / 60;
    int m = totalMinutes % 60;
    stringstream ss;
    if (h > 0) ss << h << "ชม ";
    ss << m << "นาที";
    return ss.str();
}

// เวลาที่พิมพ์ไปแล้ว (ชั่วโมง) นับจากเวลาที่เริ่มพิมพ์จริงจนถึงตอนนี้
double elapsedHours(const Order &o) {
    if (o.startTime == 0) return 0.0;
    double secs = difftime(time(0), o.startTime);
    if (secs < 0) secs = 0;
    return secs / 3600.0;
}

// เวลาที่เหลือโดยประมาณ (ชั่วโมง) ของออเดอร์ที่กำลังพิมพ์อยู่ ไม่ต่ำกว่า 0
double remainingHours(const Order &o) {
    double left = o.hours - elapsedHours(o);
    if (left < 0) left = 0;
    return left;
}

// ข้อความสรุปเวลาสำหรับออเดอร์ที่สถานะ Printing เช่น "กำลังพิมพ์ - เหลืออีก 2ชม 15นาที"
// สำหรับสถานะอื่นคืนค่าว่าง (ไม่ต้องแสดงเวลาที่เหลือ)
string printingTimeLabel(const Order &o) {
    if (o.status != "Printing") return "";
    double left = remainingHours(o);
    if (left <= 0.0) return " (ใกล้เสร็จ กำลังปรับสถานะ...)";
    return " (เหลืออีก " + formatDuration(left) + ")";
}

// ตรวจสอบออเดอร์ที่สถานะ "Printing" ทุกตัว ถ้าเวลาผ่านไปครบตามเวลาประมาณการแล้ว
// จะปรับสถานะเป็น "Completed" และคืนเครื่องพิมพ์เป็น Idle ให้อัตโนมัติ โดยไม่ต้องกดยืนยันเอง
// เรียกใช้ทุกครั้งที่เข้าเมนูที่เกี่ยวข้อง เพื่อให้สถานะอัปเดตตามเวลาจริงเสมอ
void autoCompletePrinting() {
    bool changed = false;
    for (int i = 0; i < orderCount; i++) {
        if (orders[i].status != "Printing") continue;
        if (elapsedHours(orders[i]) < orders[i].hours) continue;

        orders[i].status = "Completed";
        int pi = findPrinterIndex(orders[i].printerCode);
        if (pi != -1) {
            printers[pi].status = "Idle";
            printers[pi].currentOrder = "-";
        }
        cout << GREEN << "  [อัตโนมัติ] ออเดอร์ " << orders[i].code
             << " พิมพ์ครบเวลาประมาณการแล้ว -> เปลี่ยนสถานะเป็น Completed (พร้อมส่งมอบ/ชำระเงิน)\n" << RESET;
        changed = true;
    }
    if (changed) {
        saveOrders();
        savePrinters();
    }
}


void listPrinters() {
    autoCompletePrinting();
    printHeader("รายการเครื่องพิมพ์ทั้งหมด");
    if (printerCount == 0) { cout << "  (ไม่มีข้อมูล)\n"; return; }
    cout << left << setw(8) << "รหัส" << setw(16) << "ชื่อเครื่อง" << setw(10) << "ประเภท"
         << setw(14) << "สถานะ" << "ออเดอร์ปัจจุบัน\n";
    printLine();
    for (int i = 0; i < printerCount; i++) {
        cout << left << setw(8) << printers[i].code << setw(16) << printers[i].name
             << setw(10) << printers[i].type;
        if (printers[i].status == "Idle") cout << GREEN;
        else if (printers[i].status == "Printing") cout << YELLOW;
        else cout << RED;
        cout << setw(14) << printers[i].status << RESET << printers[i].currentOrder;
        if (printers[i].status == "Printing") {
            int oi = findOrderIndex(printers[i].currentOrder);
            if (oi != -1) cout << printingTimeLabel(orders[oi]);
        }
        cout << "\n";
    }
}

void listOrders() {
    autoCompletePrinting();
    printHeader("รายการออเดอร์ทั้งหมด");
    if (orderCount == 0) { cout << "  (ไม่มีข้อมูล)\n"; return; }
    cout << left << setw(8) << "รหัส" << setw(8) << "ลูกค้า" << setw(8) << "วัสดุ"
         << setw(8) << "เครื่อง" << setw(16) << "ไฟล์งาน" << setw(10) << "น.นัก(g)"
         << setw(8) << "ชม." << setw(10) << "ราคา" << "สถานะ\n";
    printLine();
    for (int i = 0; i < orderCount; i++) {
        cout << left << setw(8) << orders[i].code << setw(8) << orders[i].customerCode
             << setw(8) << orders[i].materialCode << setw(8) << orders[i].printerCode
             << setw(16) << orders[i].fileName
             << setw(10) << fixed << setprecision(1) << orders[i].weight
             << setw(8) << orders[i].hours
             << setw(10) << setprecision(2) << orders[i].price
             << orders[i].status << printingTimeLabel(orders[i]) << "\n";
    }
}

/* ==========================================================================
   9) CUSTOMER MANAGEMENT
   ========================================================================== */
void insertCustomer() {
    printHeader("เพิ่มลูกค้าใหม่");
    cout << YELLOW << "  (พิมพ์ 0 แล้ว Enter ในช่องใดก็ได้ เพื่อยกเลิกและกลับเมนูก่อนหน้า)\n" << RESET;
    if (customerCount >= MAX_CUSTOMERS) { cout << RED << "  ข้อมูลลูกค้าเต็มแล้ว\n" << RESET; return; }

    Customer c;

    string name = readLineTrim("  ชื่อลูกค้า [0=ยกเลิก]: ");
    if (name == "0") { cout << YELLOW << "  ยกเลิกการเพิ่มลูกค้า\n" << RESET; return; }
    c.name = name;

    string phone = readLineTrim("  เบอร์โทร [0=ยกเลิก]: ");
    if (phone == "0") { cout << YELLOW << "  ยกเลิกการเพิ่มลูกค้า\n" << RESET; return; }
    c.phone = phone;

    string address = readLineTrim("  ที่อยู่ [0=ยกเลิก]: ");
    if (address == "0") { cout << YELLOW << "  ยกเลิกการเพิ่มลูกค้า\n" << RESET; return; }
    c.address = address;

    c.code = genCode("C", nextCustomerId++);
    customers[customerCount++] = c;
    saveCustomers();
    cout << GREEN << "  เพิ่มลูกค้าสำเร็จ รหัส: " << c.code << RESET << "\n";
}

void searchCustomer() {
    printHeader("ค้นหาลูกค้า (รหัสหรือชื่อ)");
    string key = readLineTrim("  กรอกรหัสหรือชื่อ: ");
    bool found = false;
    cout << left << setw(8) << "รหัส" << setw(20) << "ชื่อ" << setw(15) << "เบอร์โทร" << "ที่อยู่\n";
    printLine();
    for (int i = 0; i < customerCount; i++) {
        if (toUpperStr(customers[i].code) == toUpperStr(key) || containsIgnoreCase(customers[i].name, key)) {
            cout << left << setw(8) << customers[i].code << setw(20) << customers[i].name
                 << setw(15) << customers[i].phone << customers[i].address << "\n";
            found = true;
        }
    }
    if (!found) cout << RED << "  ไม่พบข้อมูลลูกค้า\n" << RESET;
}

void deleteCustomer() {
    printHeader("ลบลูกค้า");
    string key = readLineTrim("  กรอกรหัสลูกค้าที่ต้องการลบ [0=ยกเลิก]: ");
    if (key == "0") { cout << YELLOW << "  ยกเลิกการลบ\n" << RESET; return; }
    int idx = findCustomerIndex(key);
    if (idx == -1) { cout << RED << "  ไม่พบรหัสลูกค้านี้\n" << RESET; return; }
    // กันลบลูกค้าที่ยังมีออเดอร์ค้างอยู่ (ยังไม่ Paid/PickedUp/Cancelled)
    for (int i = 0; i < orderCount; i++) {
        if (orders[i].customerCode == customers[idx].code &&
            orders[i].status != "Paid" && orders[i].status != "PickedUp" && orders[i].status != "Cancelled") {
            cout << RED << "  ไม่สามารถลบได้ ลูกค้ายังมีออเดอร์ค้างอยู่ (" << orders[i].code << ")\n" << RESET;
            return;
        }
    }
    string conf = readLineTrim("  ยืนยันการลบลูกค้า " + customers[idx].name + "? (y/n): ");
    if (toUpperStr(conf) != "Y") { cout << YELLOW << "  ยกเลิกการลบ\n" << RESET; return; }
    for (int i = idx; i < customerCount - 1; i++) customers[i] = customers[i + 1];
    customerCount--;
    saveCustomers();
    cout << GREEN << "  ลบลูกค้าสำเร็จ\n" << RESET;
}

void customerMenu() {
    while (true) {
        clearScreen();
        printHeader("จัดการข้อมูลลูกค้า");
        cout << "  1. แสดงรายชื่อลูกค้าทั้งหมด\n";
        cout << "  2. ค้นหาลูกค้า\n";
        cout << "  3. เพิ่มลูกค้าใหม่\n";
        cout << "  4. ลบลูกค้า\n";
        cout << "  0. กลับเมนูหลัก\n";
        int c = readIntInRange("\n  เลือกเมนู: ", 0, 4);
        clearScreen();
        if (c == 1) listCustomers();
        else if (c == 2) searchCustomer();
        else if (c == 3) insertCustomer();
        else if (c == 4) deleteCustomer();
        else if (c == 0) return;
        pause();
    }
}

/* ==========================================================================
   10) MATERIAL MANAGEMENT
   ========================================================================== */
void insertMaterial() {
    printHeader("เพิ่มวัสดุใหม่");
    cout << YELLOW << "  (พิมพ์ 0 แล้ว Enter ในช่องใดก็ได้ เพื่อยกเลิกและกลับเมนูก่อนหน้า)\n" << RESET;
    if (materialCount >= MAX_MATERIALS) { cout << RED << "  ข้อมูลวัสดุเต็มแล้ว\n" << RESET; return; }

    Material m;

    string name = readLineTrim("  ชื่อวัสดุ (เช่น PLA, ABS, PETG) [0=ยกเลิก]: ");
    if (name == "0") { cout << YELLOW << "  ยกเลิกการเพิ่มวัสดุ\n" << RESET; return; }
    m.name = name;

    string color = readLineTrim("  สี (เช่น Red, Black, White) [0=ยกเลิก]: ");
    if (color == "0") { cout << YELLOW << "  ยกเลิกการเพิ่มวัสดุ\n" << RESET; return; }
    m.color = color;

    double price;
    if (!readPositiveDoubleCancelable("  ราคาต่อกรัม (บาท) [0=ยกเลิก]: ", price)) {
        cout << YELLOW << "  ยกเลิกการเพิ่มวัสดุ\n" << RESET; return;
    }
    m.pricePerGram = price;

    double stock;
    if (!readPositiveDoubleCancelable("  จำนวนคงเหลือ (กรัม) [0=ยกเลิก]: ", stock)) {
        cout << YELLOW << "  ยกเลิกการเพิ่มวัสดุ\n" << RESET; return;
    }
    m.stockGram = stock;

    m.code = genCode("M", nextMaterialId++);
    materials[materialCount++] = m;
    saveMaterials();
    cout << GREEN << "  เพิ่มวัสดุสำเร็จ รหัส: " << m.code << RESET << "\n";
}

void searchMaterial() {
    printHeader("ค้นหาวัสดุ (รหัสหรือชื่อ)");
    string key = readLineTrim("  กรอกรหัสหรือชื่อ: ");
    bool found = false;
    cout << left << setw(8) << "รหัส" << setw(14) << "ชื่อวัสดุ" << setw(10) << "สี"
         << setw(14) << "ราคา/กรัม" << "คงเหลือ(กรัม)\n";
    printLine();
    for (int i = 0; i < materialCount; i++) {
        if (toUpperStr(materials[i].code) == toUpperStr(key) || containsIgnoreCase(materials[i].name, key)
            || containsIgnoreCase(materials[i].color, key)) {
            cout << left << setw(8) << materials[i].code << setw(14) << materials[i].name
                 << setw(10) << materials[i].color << setw(14) << fixed << setprecision(2)
                 << materials[i].pricePerGram << materials[i].stockGram << "\n";
            found = true;
        }
    }
    if (!found) cout << RED << "  ไม่พบข้อมูลวัสดุ\n" << RESET;
}

void deleteMaterial() {
    printHeader("ลบวัสดุ");
    string key = readLineTrim("  กรอกรหัสวัสดุที่ต้องการลบ [0=ยกเลิก]: ");
    if (key == "0") { cout << YELLOW << "  ยกเลิกการลบ\n" << RESET; return; }
    int idx = findMaterialIndex(key);
    if (idx == -1) { cout << RED << "  ไม่พบรหัสวัสดุนี้\n" << RESET; return; }
    cout << "  วัสดุ: " << materials[idx].name << " สี " << materials[idx].color << "\n";
    string conf = readLineTrim("  ยืนยันการลบ? (y/n): ");
    if (toUpperStr(conf) != "Y") { cout << YELLOW << "  ยกเลิกการลบ\n" << RESET; return; }
    for (int i = idx; i < materialCount - 1; i++) materials[i] = materials[i + 1];
    materialCount--;
    saveMaterials();
    cout << GREEN << "  ลบวัสดุสำเร็จ\n" << RESET;
}

void materialMenu() {
    while (true) {
        clearScreen();
        printHeader("จัดการข้อมูลวัสดุ");
        cout << "  1. แสดงรายการวัสดุทั้งหมด\n";
        cout << "  2. ค้นหาวัสดุ\n";
        cout << "  3. เพิ่มวัสดุใหม่\n";
        cout << "  4. ลบวัสดุ\n";
        cout << "  0. กลับเมนูหลัก\n";
        int c = readIntInRange("\n  เลือกเมนู: ", 0, 4);
        clearScreen();
        if (c == 1) listMaterials();
        else if (c == 2) searchMaterial();
        else if (c == 3) insertMaterial();
        else if (c == 4) deleteMaterial();
        else if (c == 0) return;
        pause();
    }
}

/* ==========================================================================
   11) PRINTER MANAGEMENT
   ========================================================================== */
void insertPrinter() {
    printHeader("เพิ่มเครื่องพิมพ์ใหม่");
    cout << YELLOW << "  (พิมพ์ 0 แล้ว Enter ในช่องใดก็ได้ เพื่อยกเลิกและกลับเมนูก่อนหน้า)\n" << RESET;
    if (printerCount >= MAX_PRINTERS) { cout << RED << "  ข้อมูลเครื่องพิมพ์เต็มแล้ว\n" << RESET; return; }

    Printer p;

    string name = readLineTrim("  ชื่อ/รุ่นเครื่องพิมพ์ [0=ยกเลิก]: ");
    if (name == "0") { cout << YELLOW << "  ยกเลิกการเพิ่มเครื่องพิมพ์\n" << RESET; return; }
    p.name = name;

    string type = readLineTrim("  ประเภทเครื่องพิมพ์ (เช่น FDM, SLA, DLP) [0=ยกเลิก]: ");
    if (type == "0") { cout << YELLOW << "  ยกเลิกการเพิ่มเครื่องพิมพ์\n" << RESET; return; }
    p.type = type;

    p.code = genCode("P", nextPrinterId++);
    p.status = "Idle";
    p.currentOrder = "-";
    printers[printerCount++] = p;
    savePrinters();
    cout << GREEN << "  เพิ่มเครื่องพิมพ์สำเร็จ รหัส: " << p.code << RESET << "\n";
}

void searchPrinter() {
    printHeader("ค้นหาเครื่องพิมพ์ (รหัสหรือชื่อ)");
    string key = readLineTrim("  กรอกรหัสหรือชื่อ: ");
    bool found = false;
    cout << left << setw(8) << "รหัส" << setw(16) << "ชื่อเครื่อง" << setw(10) << "ประเภท"
         << setw(14) << "สถานะ" << "ออเดอร์ปัจจุบัน\n";
    printLine();
    for (int i = 0; i < printerCount; i++) {
        if (toUpperStr(printers[i].code) == toUpperStr(key) || containsIgnoreCase(printers[i].name, key)) {
            cout << left << setw(8) << printers[i].code << setw(16) << printers[i].name
                 << setw(10) << printers[i].type << setw(14) << printers[i].status
                 << printers[i].currentOrder << "\n";
            found = true;
        }
    }
    if (!found) cout << RED << "  ไม่พบข้อมูลเครื่องพิมพ์\n" << RESET;
}

void deletePrinter() {
    printHeader("ลบเครื่องพิมพ์");
    string key = readLineTrim("  กรอกรหัสเครื่องพิมพ์ที่ต้องการลบ [0=ยกเลิก]: ");
    if (key == "0") { cout << YELLOW << "  ยกเลิกการลบ\n" << RESET; return; }
    int idx = findPrinterIndex(key);
    if (idx == -1) { cout << RED << "  ไม่พบรหัสเครื่องพิมพ์นี้\n" << RESET; return; }
    if (printers[idx].status == "Printing") {
        cout << RED << "  ไม่สามารถลบได้ เครื่องกำลังพิมพ์งานอยู่\n" << RESET;
        return;
    }
    string conf = readLineTrim("  ยืนยันการลบเครื่อง " + printers[idx].name + "? (y/n): ");
    if (toUpperStr(conf) != "Y") { cout << YELLOW << "  ยกเลิกการลบ\n" << RESET; return; }
    for (int i = idx; i < printerCount - 1; i++) printers[i] = printers[i + 1];
    printerCount--;
    savePrinters();
    cout << GREEN << "  ลบเครื่องพิมพ์สำเร็จ\n" << RESET;
}

void setPrinterMaintenance() {
    printHeader("เปลี่ยนสถานะเครื่องพิมพ์");
    string key = readLineTrim("  กรอกรหัสเครื่องพิมพ์: ");
    int idx = findPrinterIndex(key);
    if (idx == -1) { cout << RED << "  ไม่พบรหัสเครื่องพิมพ์นี้\n" << RESET; return; }
    if (printers[idx].status == "Printing") {
        cout << RED << "  เครื่องกำลังพิมพ์งานอยู่ ไม่สามารถเปลี่ยนสถานะได้\n" << RESET;
        return;
    }
    cout << "  สถานะปัจจุบัน: " << printers[idx].status << "\n";
    cout << "  1. Idle (พร้อมใช้งาน)\n  2. Maintenance (ซ่อมบำรุง)\n";
    int c = readIntInRange("  เลือก: ", 1, 2);
    printers[idx].status = (c == 1) ? "Idle" : "Maintenance";
    savePrinters();
    cout << GREEN << "  อัปเดตสถานะสำเร็จ\n" << RESET;
}

void printerMenu() {
    while (true) {
        autoCompletePrinting();
        clearScreen();
        printHeader("จัดการเครื่องพิมพ์");
        cout << "  1. แสดงรายการเครื่องพิมพ์ทั้งหมด\n";
        cout << "  2. ค้นหาเครื่องพิมพ์\n";
        cout << "  3. เพิ่มเครื่องพิมพ์ใหม่\n";
        cout << "  4. ลบเครื่องพิมพ์\n";
        cout << "  5. เปลี่ยนสถานะเครื่อง (Idle/Maintenance)\n";
        cout << "  0. กลับเมนูหลัก\n";
        int c = readIntInRange("\n  เลือกเมนู: ", 0, 5);
        clearScreen();
        if (c == 1) listPrinters();
        else if (c == 2) searchPrinter();
        else if (c == 3) insertPrinter();
        else if (c == 4) deletePrinter();
        else if (c == 5) setPrinterMaintenance();
        else if (c == 0) return;
        pause();
    }
}

/* ==========================================================================
   12) ORDER MANAGEMENT  (Insert order = create print job, select material+color,
        check stock, calc price, assign/queue printer)
   สถานะ: Queued -> Printing -> Completed -> Paid -> PickedUp (หรือ Cancelled)
   หมายเหตุ: หักวัสดุออกจาก Stock "เมื่อเริ่มพิมพ์จริง" (ตอนสถานะเปลี่ยนเป็น Printing)
             ไม่ใช่ตอนสร้างออเดอร์ ถ้าออเดอร์ยังอยู่ในคิว (Queued) จะยังไม่หักสต็อก
   ========================================================================== */
void createOrder() {
    printHeader("สร้างออเดอร์งานพิมพ์ใหม่");
    cout << YELLOW << "  (พิมพ์ 0 แล้ว Enter ในช่องใดก็ได้ เพื่อยกเลิกและกลับเมนูก่อนหน้า)\n" << RESET;
    if (orderCount >= MAX_ORDERS) { cout << RED << "  ออเดอร์เต็มแล้ว\n" << RESET; return; }
    if (customerCount == 0) { cout << RED << "  กรุณาเพิ่มลูกค้าก่อนสร้างออเดอร์\n" << RESET; return; }
    if (materialCount == 0) { cout << RED << "  กรุณาเพิ่มวัสดุก่อนสร้างออเดอร์\n" << RESET; return; }

    // --- เลือกลูกค้า ---
    listCustomers();
    string custKey = readLineTrim("\n  กรอกรหัสลูกค้า [0=ยกเลิก]: ");
    if (custKey == "0") { cout << YELLOW << "  ยกเลิกการสร้างออเดอร์\n" << RESET; return; }
    int ci = findCustomerIndex(custKey);
    if (ci == -1) { cout << RED << "  ไม่พบรหัสลูกค้านี้\n" << RESET; return; }

    // --- ไฟล์งานที่จะพิมพ์ ---
    string fileName = readLineTrim("  ชื่อไฟล์งานพิมพ์ (เช่น model.stl) [0=ยกเลิก]: ");
    if (fileName == "0") { cout << YELLOW << "  ยกเลิกการสร้างออเดอร์\n" << RESET; return; }

    // --- เลือกวัสดุ + สี ---
    clearScreen();
    listMaterials();
    string matKey = readLineTrim("\n  กรอกรหัสวัสดุ (รวมสีที่ต้องการ) [0=ยกเลิก]: ");
    if (matKey == "0") { cout << YELLOW << "  ยกเลิกการสร้างออเดอร์\n" << RESET; return; }
    int mi = findMaterialIndex(matKey);
    if (mi == -1) { cout << RED << "  ไม่พบรหัสวัสดุนี้\n" << RESET; return; }

    // --- น้ำหนักงานพิมพ์ ---
    double weight;
    if (!readPositiveDoubleCancelable("  น้ำหนักโมเดลโดยประมาณ (กรัม) [0=ยกเลิก]: ", weight)) {
        cout << YELLOW << "  ยกเลิกการสร้างออเดอร์\n" << RESET; return;
    }
    if (weight > materials[mi].stockGram) {
        cout << RED << "  วัสดุคงเหลือไม่พอ! คงเหลือ " << materials[mi].stockGram << " กรัม\n" << RESET;
        return;
    }

    // --- คำนวณเวลาพิมพ์โดยประมาณ ---
    double hours = weight / PRINT_SPEED_G_PER_HR;

    // --- คำนวณราคา ---
    double price = (weight * materials[mi].pricePerGram) + (hours * HOURLY_RATE) + BASE_FEE;

    cout << "\n" << MAGENTA << "  --- สรุปงานพิมพ์ ---\n" << RESET;
    cout << "  ลูกค้า      : " << customers[ci].name << "\n";
    cout << "  ไฟล์งาน     : " << fileName << "\n";
    cout << "  วัสดุ/สี    : " << materials[mi].name << " / " << materials[mi].color << "\n";
    cout << "  น้ำหนัก     : " << fixed << setprecision(1) << weight << " กรัม\n";
    cout << "  เวลาโดยประมาณ: " << setprecision(2) << hours << " ชั่วโมง\n";
    cout << "  ราคารวม     : " << setprecision(2) << price << " บาท\n";
    string conf = readLineTrim("  ยืนยันสร้างออเดอร์? (y/n, หรือ 0=ยกเลิก): ");
    if (conf == "0" || toUpperStr(conf) != "Y") { cout << YELLOW << "  ยกเลิกการสร้างออเดอร์\n" << RESET; return; }

    // --- แสดงเครื่องพิมพ์ที่ว่าง ให้ผู้ใช้เลือกเอง ---
    bool hasIdle = false;
    cout << "\n" << BOLD << "  เครื่องพิมพ์ที่พร้อมใช้งาน (Idle):\n" << RESET;
    for (int i = 0; i < printerCount; i++) {
        if (printers[i].status == "Idle") {
            cout << "    - " << printers[i].code << "  " << printers[i].name
                 << " (" << printers[i].type << ")\n";
            hasIdle = true;
        }
    }

    Order o;
    o.code = genCode("O", nextOrderId++);
    o.customerCode = customers[ci].code;
    o.materialCode = materials[mi].code;
    o.fileName = fileName;
    o.weight = weight;
    o.hours = hours;
    o.price = price;
    o.stockDeducted = false;
    o.startTime = 0; // ยังไม่เริ่มพิมพ์ (จะตั้งค่าตอนมอบหมายเครื่องจริงด้านล่าง)

    string pickedPrinter = "-";
    if (hasIdle) {
        pickedPrinter = readLineTrim("  เลือกรหัสเครื่องพิมพ์ (Enter ว่าง = เข้าคิวรอ): ");
    }

    int pi = -1;
    if (!pickedPrinter.empty() && pickedPrinter != "-") {
        pi = findPrinterIndex(pickedPrinter);
        if (pi == -1 || printers[pi].status != "Idle") {
            cout << YELLOW << "  รหัสเครื่องพิมพ์ไม่ถูกต้องหรือไม่ว่าง -> เข้าคิวรอแทน\n" << RESET;
            pi = -1;
        }
    }

    if (pi != -1) {
        // มอบหมายเครื่องพิมพ์ทันที -> เริ่มพิมพ์จริง -> หักสต็อกตอนนี้ -> เริ่มจับเวลานับถอยหลังอัตโนมัติ
        o.printerCode = printers[pi].code;
        o.status = "Printing";
        o.stockDeducted = true;
        o.startTime = time(0);
        printers[pi].status = "Printing";
        printers[pi].currentOrder = o.code;
        materials[mi].stockGram -= weight;
        cout << GREEN << "  มอบหมายให้เครื่อง " << printers[pi].name << " เริ่มพิมพ์ทันที (หักสต็อกวัสดุแล้ว) "
             << "ประมาณเสร็จใน " << formatDuration(hours) << " (ระบบจะเปลี่ยนสถานะเป็น Completed ให้อัตโนมัติ)\n" << RESET;
    } else {
        // ยังไม่มีเครื่องว่าง หรือผู้ใช้ไม่เลือก -> เข้าคิว ยังไม่หักสต็อก
        o.printerCode = "-";
        o.status = "Queued";
        cout << YELLOW << "  ออเดอร์เข้าคิวรอ (ยังไม่หักสต็อกวัสดุจนกว่าจะเริ่มพิมพ์จริง)\n" << RESET;
    }

    orders[orderCount++] = o;
    saveOrders();
    saveMaterials();
    savePrinters();
    cout << GREEN << "  สร้างออเดอร์สำเร็จ รหัส: " << o.code << RESET << "\n";
}

void searchOrder() {
    printHeader("ค้นหาออเดอร์ (รหัสออเดอร์ หรือ ชื่อ/รหัสลูกค้า)");
    string key = readLineTrim("  กรอกคำค้นหา: ");
    bool found = false;
    cout << left << setw(8) << "รหัส" << setw(8) << "ลูกค้า" << setw(8) << "วัสดุ"
         << setw(8) << "เครื่อง" << setw(16) << "ไฟล์งาน" << setw(10) << "น.นัก(g)"
         << setw(8) << "ชม." << setw(10) << "ราคา" << "สถานะ\n";
    printLine();
    for (int i = 0; i < orderCount; i++) {
        int ci = findCustomerIndex(orders[i].customerCode);
        bool nameMatch = (ci != -1) && containsIgnoreCase(customers[ci].name, key);
        if (toUpperStr(orders[i].code) == toUpperStr(key) ||
            toUpperStr(orders[i].customerCode) == toUpperStr(key) || nameMatch) {
            cout << left << setw(8) << orders[i].code << setw(8) << orders[i].customerCode
                 << setw(8) << orders[i].materialCode << setw(8) << orders[i].printerCode
                 << setw(16) << orders[i].fileName
                 << setw(10) << fixed << setprecision(1) << orders[i].weight
                 << setw(8) << setprecision(2) << orders[i].hours
                 << setw(10) << orders[i].price << orders[i].status << "\n";
            found = true;
        }
    }
    if (!found) cout << RED << "  ไม่พบออเดอร์\n" << RESET;
}

// ประมวลผลคิว: ดึงออเดอร์ที่สถานะ Queued ไปให้เครื่องที่ว่าง (Idle) และหักสต็อก ณ จุดนี้ (เริ่มพิมพ์จริง)
void processQueue() {
    autoCompletePrinting();
    printHeader("ประมวลผลคิวงานพิมพ์ (จับคู่ออเดอร์ที่รอกับเครื่องว่าง)");
    int assigned = 0;
    for (int i = 0; i < orderCount; i++) {
        if (orders[i].status != "Queued") continue;
        int mi = findMaterialIndex(orders[i].materialCode);
        if (mi == -1) continue;
        if (orders[i].weight > materials[mi].stockGram) {
            cout << RED << "  ออเดอร์ " << orders[i].code << " วัสดุไม่พอ (ข้ามไปก่อน)\n" << RESET;
            continue;
        }
        for (int p = 0; p < printerCount; p++) {
            if (printers[p].status == "Idle") {
                orders[i].printerCode = printers[p].code;
                orders[i].status = "Printing";
                orders[i].stockDeducted = true;
                orders[i].startTime = time(0);
                printers[p].status = "Printing";
                printers[p].currentOrder = orders[i].code;
                materials[mi].stockGram -= orders[i].weight;
                cout << GREEN << "  ออเดอร์ " << orders[i].code << " -> เครื่อง "
                     << printers[p].name << " (หักสต็อกวัสดุแล้ว) ประมาณเสร็จใน "
                     << formatDuration(orders[i].hours) << RESET << "\n";
                assigned++;
                break;
            }
        }
    }
    if (assigned == 0) cout << YELLOW << "  ไม่มีคิวที่จับคู่ได้ (ไม่มีคิวรอ หรือไม่มีเครื่องว่าง)\n" << RESET;
    saveOrders();
    saveMaterials();
    savePrinters();
}

// ทำเครื่องหมายว่าออเดอร์พิมพ์เสร็จแล้ว (Printing -> Completed) และคืนเครื่องเป็น Idle
void markOrderCompleted() {
    printHeader("แจ้งพิมพ์งานเสร็จก่อนเวลา (บังคับ Printing -> Completed ด้วยตนเอง)");
    cout << YELLOW << "  หมายเหตุ: ปกติระบบจะเปลี่ยนสถานะเป็น Completed ให้อัตโนมัติเมื่อครบเวลาประมาณการ\n"
         << "  ใช้เมนูนี้เฉพาะกรณีพิมพ์เสร็จก่อนเวลาที่ประมาณไว้เท่านั้น\n" << RESET;
    string key = readLineTrim("  กรอกรหัสออเดอร์ [0=ยกเลิก]: ");
    if (key == "0") { cout << YELLOW << "  ยกเลิกการดำเนินการ\n" << RESET; return; }
    int oi = findOrderIndex(key);
    if (oi == -1) { cout << RED << "  ไม่พบออเดอร์นี้\n" << RESET; return; }
    if (orders[oi].status != "Printing") {
        cout << RED << "  ออเดอร์นี้ไม่ได้อยู่ในสถานะกำลังพิมพ์\n" << RESET;
        return;
    }
    cout << "  เวลาที่เหลือตามประมาณการ: " << formatDuration(remainingHours(orders[oi])) << "\n";
    string conf = readLineTrim("  ยืนยันว่าพิมพ์เสร็จแล้วจริง? (y/n): ");
    if (toUpperStr(conf) != "Y") { cout << YELLOW << "  ยกเลิกการดำเนินการ\n" << RESET; return; }
    orders[oi].status = "Completed";
    int pi = findPrinterIndex(orders[oi].printerCode);
    if (pi != -1) {
        printers[pi].status = "Idle";
        printers[pi].currentOrder = "-";
    }
    saveOrders();
    savePrinters();
    cout << GREEN << "  ออเดอร์ " << orders[oi].code << " พิมพ์เสร็จแล้ว พร้อมส่งมอบ/ชำระเงิน\n" << RESET;
}

// ยืนยันว่าลูกค้ามารับสินค้าแล้ว (Paid -> PickedUp) ปิดจบวงจรออเดอร์
void markOrderPickedUp() {
    printHeader("ยืนยันลูกค้ารับสินค้าแล้ว (Paid -> PickedUp)");
    string key = readLineTrim("  กรอกรหัสออเดอร์ [0=ยกเลิก]: ");
    if (key == "0") { cout << YELLOW << "  ยกเลิกการดำเนินการ\n" << RESET; return; }
    int oi = findOrderIndex(key);
    if (oi == -1) { cout << RED << "  ไม่พบออเดอร์นี้\n" << RESET; return; }
    if (orders[oi].status != "Paid") {
        cout << RED << "  ออเดอร์นี้ยังไม่ได้ชำระเงิน (สถานะ: " << orders[oi].status << ")\n" << RESET;
        return;
    }
    orders[oi].status = "PickedUp";
    saveOrders();
    cout << GREEN << "  ออเดอร์ " << orders[oi].code << " ส่งมอบให้ลูกค้าเรียบร้อยแล้ว (PickedUp)\n" << RESET;
}

void cancelOrder() {
    printHeader("ยกเลิกออเดอร์");
    string key = readLineTrim("  กรอกรหัสออเดอร์ที่ต้องการยกเลิก [0=ไม่ยกเลิก/กลับเมนู]: ");
    if (key == "0") { cout << YELLOW << "  ยกเลิกการดำเนินการ\n" << RESET; return; }
    int oi = findOrderIndex(key);
    if (oi == -1) { cout << RED << "  ไม่พบออเดอร์นี้\n" << RESET; return; }
    if (orders[oi].status == "Paid" || orders[oi].status == "PickedUp" || orders[oi].status == "Cancelled") {
        cout << RED << "  ออเดอร์นี้ไม่สามารถยกเลิกได้ (สถานะ: " << orders[oi].status << ")\n" << RESET;
        return;
    }
    string conf = readLineTrim("  ยืนยันยกเลิกออเดอร์ " + orders[oi].code + "? (y/n): ");
    if (toUpperStr(conf) != "Y") { cout << YELLOW << "  ยกเลิกการดำเนินการ\n" << RESET; return; }

    // คืนวัสดุกลับสต็อก เฉพาะกรณีที่หักสต็อกไปแล้วเท่านั้น (เริ่มพิมพ์จริงแล้ว)
    if (orders[oi].stockDeducted) {
        int mi = findMaterialIndex(orders[oi].materialCode);
        if (mi != -1) materials[mi].stockGram += orders[oi].weight;
    }

    // ปลดเครื่องพิมพ์ถ้ากำลังพิมพ์อยู่
    if (orders[oi].status == "Printing") {
        int pi = findPrinterIndex(orders[oi].printerCode);
        if (pi != -1) { printers[pi].status = "Idle"; printers[pi].currentOrder = "-"; }
    }

    orders[oi].status = "Cancelled";
    saveOrders();
    saveMaterials();
    savePrinters();
    cout << GREEN << "  ยกเลิกออเดอร์สำเร็จ" << (orders[oi].stockDeducted ? " (คืนวัสดุเข้าสต็อกแล้ว)" : "") << "\n" << RESET;
}

void queueStatusView() {
    autoCompletePrinting();
    printHeader("สถานะคิวงานพิมพ์ (ตามเครื่องพิมพ์)");
    for (int p = 0; p < printerCount; p++) {
        cout << BOLD << "  เครื่อง " << printers[p].code << " (" << printers[p].name << ") - "
             << printers[p].status << RESET << "\n";
        bool any = false;
        for (int i = 0; i < orderCount; i++) {
            if (orders[i].printerCode == printers[p].code &&
                (orders[i].status == "Printing")) {
                cout << "     -> กำลังพิมพ์ออเดอร์: " << orders[i].code
                     << YELLOW << printingTimeLabel(orders[i]) << RESET << "\n";
                any = true;
            }
        }
        if (!any) cout << "     -> ว่าง\n";
    }
    printLine();
    cout << BOLD << "  ออเดอร์ที่รอคิว (Queued):\n" << RESET;
    bool anyQ = false;
    for (int i = 0; i < orderCount; i++) {
        if (orders[i].status == "Queued") {
            cout << "     - " << orders[i].code << " (ลูกค้า: " << orders[i].customerCode << ")\n";
            anyQ = true;
        }
    }
    if (!anyQ) cout << "     (ไม่มีคิวรอ)\n";
}

void orderMenu() {
    while (true) {
        autoCompletePrinting();
        clearScreen();
        printHeader("จัดการออเดอร์งานพิมพ์");
        cout << "  1. แสดงออเดอร์ทั้งหมด\n";
        cout << "  2. ค้นหาออเดอร์\n";
        cout << "  3. สร้างออเดอร์ใหม่\n";
        cout << "  4. ยกเลิกออเดอร์\n";
        cout << "  5. ดูสถานะคิว/เครื่องพิมพ์ (พร้อมเวลาที่เหลือ)\n";
        cout << "  6. ประมวลผลคิว (จับคู่งานรอกับเครื่องว่าง)\n";
        cout << "  7. แจ้งพิมพ์งานเสร็จก่อนเวลา (บังคับ Completed ด้วยตนเอง)\n";
        cout << "  8. ยืนยันลูกค้ารับสินค้าแล้ว (Paid -> PickedUp)\n";
        cout << "  0. กลับเมนูหลัก\n";
        int c = readIntInRange("\n  เลือกเมนู: ", 0, 8);
        clearScreen();
        if (c == 1) listOrders();
        else if (c == 2) searchOrder();
        else if (c == 3) createOrder();
        else if (c == 4) cancelOrder();
        else if (c == 5) queueStatusView();
        else if (c == 6) processQueue();
        else if (c == 7) markOrderCompleted();
        else if (c == 8) markOrderPickedUp();
        else if (c == 0) return;
        pause();
    }
}

/* ==========================================================================
   13) POS -- CHECKOUT / RECEIPT
   ========================================================================== */
void printReceipt(Order &o, Customer &c, Material &m, double cash, double change) {
    time_t now = time(0);
    char buf[64];
    strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M", localtime(&now));

    cout << "\n" << CYAN;
    cout << "     ****************************************\n";
    cout << "         ใบเสร็จรับเงิน - ร้าน 3D PRINTING\n";
    cout << "     ****************************************" << RESET << "\n";
    cout << "     วันที่       : " << buf << "\n";
    cout << "     เลขที่ออเดอร์ : " << o.code << "\n";
    cout << "     ลูกค้า       : " << c.name << " (" << c.phone << ")\n";
    printLine();
    cout << "     ไฟล์งาน      : " << o.fileName << "\n";
    cout << "     รายการ       : " << m.name << " สี " << m.color << "\n";
    cout << "     น้ำหนัก      : " << fixed << setprecision(1) << o.weight << " กรัม\n";
    cout << "     เวลาพิมพ์    : " << setprecision(2) << o.hours << " ชม.\n";
    printLine();
    cout << "     ยอดรวม       : " << setprecision(2) << o.price << " บาท\n";
    cout << "     รับเงินสด    : " << cash << " บาท\n";
    cout << GREEN << "     เงินทอน      : " << change << " บาท" << RESET << "\n";
    cout << CYAN << "     ****************************************\n";
    cout << "            ขอบคุณที่ใช้บริการค่ะ/ครับ\n";
    cout << "     ****************************************\n" << RESET;
}

void posCheckout() {
    autoCompletePrinting();
    printHeader("POS - ชำระเงิน / ออกใบเสร็จ");
    cout << YELLOW << "  (พิมพ์ 0 แล้ว Enter ในช่องใดก็ได้ เพื่อยกเลิกและกลับเมนูก่อนหน้า)\n" << RESET;
    cout << BOLD << "  ออเดอร์ที่พร้อมชำระเงิน (สถานะ Completed):\n" << RESET;
    bool any = false;
    for (int i = 0; i < orderCount; i++) {
        if (orders[i].status == "Completed") {
            cout << "   - " << orders[i].code << "  ราคา: " << fixed << setprecision(2)
                 << orders[i].price << " บาท\n";
            any = true;
        }
    }
    if (!any) cout << YELLOW << "   (ยังไม่มีออเดอร์พร้อมชำระเงิน)\n" << RESET;

    bool anyPrinting = false;
    for (int i = 0; i < orderCount; i++) {
        if (orders[i].status == "Printing") {
            if (!anyPrinting) { cout << "\n" << BOLD << "  ออเดอร์ที่กำลังพิมพ์อยู่ (ยังชำระเงินไม่ได้):\n" << RESET; anyPrinting = true; }
            cout << "   - " << orders[i].code << YELLOW << printingTimeLabel(orders[i]) << RESET << "\n";
        }
    }

    if (!any) return;

    string key = readLineTrim("\n  กรอกรหัสออเดอร์ที่จะชำระเงิน [0=ยกเลิก]: ");
    if (key == "0") { cout << YELLOW << "  ยกเลิกการชำระเงิน\n" << RESET; return; }
    int oi = findOrderIndex(key);
    if (oi == -1) { cout << RED << "  ไม่พบออเดอร์นี้\n" << RESET; return; }
    if (orders[oi].status != "Completed") {
        cout << RED << "  ออเดอร์นี้ยังไม่พร้อมชำระเงิน (สถานะ: " << orders[oi].status << ")\n" << RESET;
        return;
    }

    int ci = findCustomerIndex(orders[oi].customerCode);
    int mi = findMaterialIndex(orders[oi].materialCode);
    if (ci == -1 || mi == -1) { cout << RED << "  ข้อมูลลูกค้า/วัสดุไม่สมบูรณ์\n" << RESET; return; }

    cout << "  ยอดที่ต้องชำระ: " << fixed << setprecision(2) << orders[oi].price << " บาท\n";
    double cash;
    while (true) {
        if (!readPositiveDoubleCancelable("  รับเงินสด (บาท) [0=ยกเลิก]: ", cash)) {
            cout << YELLOW << "  ยกเลิกการชำระเงิน\n" << RESET; return;
        }
        if (cash < orders[oi].price) {
            cout << RED << "  เงินสดไม่พอ กรุณากรอกใหม่\n" << RESET;
            continue;
        }
        break;
    }
    double change = cash - orders[oi].price;

    orders[oi].status = "Paid";
    saveOrders();

    printReceipt(orders[oi], customers[ci], materials[mi], cash, change);

    time_t now = time(0);
    char buf[64];
    strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M", localtime(&now));

    SalesRecord rec;
    rec.date = buf;
    rec.orderCode = orders[oi].code;
    rec.customerName = customers[ci].name;
    rec.materialName = materials[mi].name;
    rec.color = materials[mi].color;
    rec.price = orders[oi].price;
    rec.cash = cash;
    rec.change = change;
    appendSalesHistory(rec);
}

/* ==========================================================================
   14) REPORTS -- ประวัติการขาย
   ========================================================================== */
void showSalesHistory() {
    printHeader("ประวัติการขาย (Sales History)");
    if (salesCount == 0) { cout << "  (ยังไม่มีประวัติการขาย)\n"; return; }
    double total = 0;
    cout << left << setw(17) << "วันที่" << setw(8) << "ออเดอร์" << setw(16) << "ลูกค้า"
         << setw(10) << "วัสดุ" << setw(8) << "สี" << setw(10) << "ยอด" << "\n";
    printLine();
    for (int i = 0; i < salesCount; i++) {
        cout << left << setw(17) << salesHistory[i].date << setw(8) << salesHistory[i].orderCode
             << setw(16) << salesHistory[i].customerName << setw(10) << salesHistory[i].materialName
             << setw(8) << salesHistory[i].color << fixed << setprecision(2)
             << setw(10) << salesHistory[i].price << "\n";
        total += salesHistory[i].price;
    }
    printLine();
    cout << GREEN << BOLD << "  จำนวนบิลทั้งหมด: " << salesCount << "  ยอดขายรวม: "
         << fixed << setprecision(2) << total << " บาท" << RESET << "\n";
}

void reportMenu() {
    while (true) {
        clearScreen();
        printHeader("รายงาน");
        cout << "  1. ประวัติการขาย (Sales History)\n";
        cout << "  2. สรุปสต็อกวัสดุ\n";
        cout << "  0. กลับเมนูหลัก\n";
        int c = readIntInRange("\n  เลือกเมนู: ", 0, 2);
        clearScreen();
        if (c == 1) showSalesHistory();
        else if (c == 2) listMaterials();
        else if (c == 0) return;
        pause();
    }
}

/* ==========================================================================
   15) MAIN MENU
   ========================================================================== */
void seedSamplePrintersIfEmpty() {
    // ถ้ายังไม่มีเครื่องพิมพ์เลย ให้สร้างตัวอย่างไว้ 2 เครื่อง เพื่อให้ทดสอบระบบได้ทันที
    if (printerCount == 0) {
        Printer p1; p1.code = genCode("P", nextPrinterId++); p1.name = "Ender-3 V2";
        p1.type = "FDM"; p1.status = "Idle"; p1.currentOrder = "-";
        Printer p2; p2.code = genCode("P", nextPrinterId++); p2.name = "Prusa MK3S";
        p2.type = "FDM"; p2.status = "Idle"; p2.currentOrder = "-";
        printers[printerCount++] = p1;
        printers[printerCount++] = p2;
        savePrinters();
    }
}

int main() {
#ifdef _WIN32
    // บังคับให้ console ของ Windows ใช้ UTF-8 ทั้งขาเข้า-ขาออก
    // (แก้ปัญหาภาษาไทยแสดงเป็นอักขระเพี้ยน ในกรณีรันผ่าน cmd.exe/PowerShell)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    // 1) Create: โหลดข้อมูลจาก JSON เข้าสู่ Array
    loadCustomers();
    loadMaterials();
    loadPrinters();
    loadOrders();
    loadSalesHistory();
    seedSamplePrintersIfEmpty();

    while (true) {
        autoCompletePrinting();
        clearScreen();
        cout << BLUE << BOLD;
        cout << " ██████╗ ██████╗     ██████╗ ██████╗ ██╗███╗   ██╗████████╗\n";
        cout << " ╚════██╗██╔══██╗    ██╔══██╗██╔══██╗██║████╗  ██║╚══██╔══╝\n";
        cout << "  █████╔╝██║  ██║    ██████╔╝██████╔╝██║██╔██╗ ██║   ██║   \n";
        cout << "  ╚═══██╗██║  ██║    ██╔═══╝ ██╔══██╗██║██║╚██╗██║   ██║   \n";
        cout << " ██████╔╝██████╔╝    ██║     ██║  ██║██║██║ ╚████║   ██║   \n";
        cout << " ╚═════╝ ╚═════╝     ╚═╝     ╚═╝  ╚═╝╚═╝╚═╝  ╚═══╝   ╚═╝   \n";
        cout << RESET;
        printHeader("ระบบจัดการร้าน 3D PRINTING - เมนูหลัก (JSON Storage)");
        cout << GREEN << "  [1] " << RESET << "จัดการข้อมูลลูกค้า\n";
        cout << GREEN << "  [2] " << RESET << "จัดการข้อมูลวัสดุ (พร้อมสี/สต็อก)\n";
        cout << GREEN << "  [3] " << RESET << "จัดการเครื่องพิมพ์ (พร้อมประเภท)\n";
        cout << GREEN << "  [4] " << RESET << "จัดการออเดอร์งานพิมพ์ / คิวงาน\n";
        cout << GREEN << "  [5] " << RESET << "POS - ชำระเงิน / ออกใบเสร็จ\n";
        cout << GREEN << "  [6] " << RESET << "รายงาน / ประวัติการขาย\n";
        cout << YELLOW << "  [9] " << RESET << "บันทึกข้อมูลทั้งหมด (Save)\n";
        cout << RED << "  [0] " << RESET << "บันทึกและออกจากโปรแกรม\n";

        int choice = readIntInRange("\n  กรุณาเลือกเมนู: ", 0, 9);

        if (choice == 1) customerMenu();
        else if (choice == 2) materialMenu();
        else if (choice == 3) printerMenu();
        else if (choice == 4) orderMenu();
        else if (choice == 5) { clearScreen(); posCheckout(); pause(); }
        else if (choice == 6) reportMenu();
        else if (choice == 9) { clearScreen(); saveAll(); pause(); }
        else if (choice == 0) {
            saveAll();
            cout << GREEN << "\n  ขอบคุณที่ใช้งานระบบ ลาก่อน!\n" << RESET;
            break;
        }
    }
    return 0;
}