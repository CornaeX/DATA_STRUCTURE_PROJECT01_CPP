/* ============================================================================
   ระบบจัดการร้าน 3D Printing (3D Printing Shop Management System)
   Text-based / Console UI (TUI/CUI)
   - ใช้ Array ล้วนในการเก็บข้อมูล (ไม่ใช้ vector/STL container)
   - Create (โหลดจากไฟล์ .txt), Search, Insert, Delete
   - จัดการ ลูกค้า / วัสดุ(พร้อมสี) / เครื่องพิมพ์ / ออเดอร์ / คิวงาน / POS
   ============================================================================ */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <iomanip>
#include <cstdlib>
#include <ctime>
#include <limits>

using namespace std;

/* ==========================================================================
   0) CONSTANTS / ANSI COLOR / GLOBAL SIZES
   ========================================================================== */
const int MAX_CUSTOMERS = 200;
const int MAX_MATERIALS = 100;
const int MAX_PRINTERS  = 50;
const int MAX_ORDERS    = 500;

const double HOURLY_RATE   = 20.0;  // บาท/ชั่วโมง (ค่าไฟ+ค่าเสื่อมเครื่อง)
const double BASE_FEE      = 20.0;  // ค่าดำเนินการเริ่มต้นต่อออเดอร์
const double PRINT_SPEED_G_PER_HR = 15.0; // ความเร็วพิมพ์โดยประมาณ (กรัม/ชม.)

const string F_CUSTOMERS = "customers.txt";
const string F_MATERIALS = "materials.txt";
const string F_PRINTERS  = "printers.txt";
const string F_ORDERS    = "orders.txt";
const string F_SALES     = "sales_history.txt";

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
    string code, name, status;      // status: Idle / Printing / Maintenance
    string currentOrder;            // รหัสออเดอร์ที่กำลังพิมพ์อยู่ ("-" ถ้าว่าง)
};

struct Order {
    string code, customerCode, materialCode, printerCode;
    double weight;    // กรัม
    double hours;     // ชั่วโมงประมาณการ
    double price;     // ราคารวม
    string status;    // Queued / Printing / Completed / Paid / Cancelled
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
    // หมายเหตุ: ทุกฟังก์ชันอ่านค่า (readIntInRange / readPositiveDouble / readLineTrim)
    // เคลียร์ '\n' ที่ค้างอยู่ใน buffer ให้เรียบร้อยแล้วก่อน return ทุกครั้ง
    // ดังนั้นตรงนี้แค่รอผู้ใช้กด Enter หนึ่งครั้งก็พอ (ไม่ต้อง ignore() ซ้ำ)
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

// แยกสตริงด้วยตัวคั่น เก็บผลลัพธ์ลง array (ไม่ใช้ vector)
void splitLine(const string &line, char delim, string result[], int &count, int maxFields) {
    count = 0;
    stringstream ss(line);
    string item;
    while (getline(ss, item, delim) && count < maxFields) {
        result[count++] = trim(item);
    }
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

double toDouble(const string &s) {
    if (s.empty()) return 0.0;
    return atof(s.c_str());
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
        cout << RED << "  กรุณากรอกตัวเลขที่มากกว่า 0\n" << RESET;
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
}

string readLineTrim(const string &prompt) {
    cout << prompt;
    string s;
    getline(cin, s);
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
   4) LOAD (Create) FUNCTIONS  -- อ่านจาก Text File เก็บลง Array
   ========================================================================== */
void loadCustomers() {
    ifstream fin(F_CUSTOMERS.c_str());
    if (!fin.is_open()) return;
    string line;
    customerCount = 0;
    while (getline(fin, line) && customerCount < MAX_CUSTOMERS) {
        if (trim(line).empty()) continue;
        string f[10]; int n;
        splitLine(line, '|', f, n, 10);
        if (n < 4) continue;
        customers[customerCount].code = f[0];
        customers[customerCount].name = f[1];
        customers[customerCount].phone = f[2];
        customers[customerCount].address = f[3];
        int num = extractNumber(f[0]);
        if (num + 1 > nextCustomerId) nextCustomerId = num + 1;
        customerCount++;
    }
    fin.close();
}

void loadMaterials() {
    ifstream fin(F_MATERIALS.c_str());
    if (!fin.is_open()) return;
    string line;
    materialCount = 0;
    while (getline(fin, line) && materialCount < MAX_MATERIALS) {
        if (trim(line).empty()) continue;
        string f[10]; int n;
        splitLine(line, '|', f, n, 10);
        if (n < 5) continue;
        materials[materialCount].code = f[0];
        materials[materialCount].name = f[1];
        materials[materialCount].color = f[2];
        materials[materialCount].pricePerGram = toDouble(f[3]);
        materials[materialCount].stockGram = toDouble(f[4]);
        int num = extractNumber(f[0]);
        if (num + 1 > nextMaterialId) nextMaterialId = num + 1;
        materialCount++;
    }
    fin.close();
}

void loadPrinters() {
    ifstream fin(F_PRINTERS.c_str());
    if (!fin.is_open()) return;
    string line;
    printerCount = 0;
    while (getline(fin, line) && printerCount < MAX_PRINTERS) {
        if (trim(line).empty()) continue;
        string f[10]; int n;
        splitLine(line, '|', f, n, 10);
        if (n < 4) continue;
        printers[printerCount].code = f[0];
        printers[printerCount].name = f[1];
        printers[printerCount].status = f[2];
        printers[printerCount].currentOrder = f[3];
        int num = extractNumber(f[0]);
        if (num + 1 > nextPrinterId) nextPrinterId = num + 1;
        printerCount++;
    }
    fin.close();
}

void loadOrders() {
    ifstream fin(F_ORDERS.c_str());
    if (!fin.is_open()) return;
    string line;
    orderCount = 0;
    while (getline(fin, line) && orderCount < MAX_ORDERS) {
        if (trim(line).empty()) continue;
        string f[10]; int n;
        splitLine(line, '|', f, n, 10);
        if (n < 8) continue;
        orders[orderCount].code = f[0];
        orders[orderCount].customerCode = f[1];
        orders[orderCount].materialCode = f[2];
        orders[orderCount].printerCode = f[3];
        orders[orderCount].weight = toDouble(f[4]);
        orders[orderCount].hours = toDouble(f[5]);
        orders[orderCount].price = toDouble(f[6]);
        orders[orderCount].status = f[7];
        int num = extractNumber(f[0]);
        if (num + 1 > nextOrderId) nextOrderId = num + 1;
        orderCount++;
    }
    fin.close();
}

/* ==========================================================================
   5) SAVE FUNCTIONS -- บันทึกกลับลง Text File
   ========================================================================== */
void saveCustomers() {
    ofstream fout(F_CUSTOMERS.c_str());
    for (int i = 0; i < customerCount; i++) {
        fout << customers[i].code << "|" << customers[i].name << "|"
             << customers[i].phone << "|" << customers[i].address << "\n";
    }
    fout.close();
}

void saveMaterials() {
    ofstream fout(F_MATERIALS.c_str());
    for (int i = 0; i < materialCount; i++) {
        fout << materials[i].code << "|" << materials[i].name << "|"
             << materials[i].color << "|" << fixed << setprecision(2)
             << materials[i].pricePerGram << "|" << materials[i].stockGram << "\n";
    }
    fout.close();
}

void savePrinters() {
    ofstream fout(F_PRINTERS.c_str());
    for (int i = 0; i < printerCount; i++) {
        fout << printers[i].code << "|" << printers[i].name << "|"
             << printers[i].status << "|" << printers[i].currentOrder << "\n";
    }
    fout.close();
}

void saveOrders() {
    ofstream fout(F_ORDERS.c_str());
    for (int i = 0; i < orderCount; i++) {
        fout << orders[i].code << "|" << orders[i].customerCode << "|"
             << orders[i].materialCode << "|" << orders[i].printerCode << "|"
             << fixed << setprecision(2) << orders[i].weight << "|"
             << orders[i].hours << "|" << orders[i].price << "|"
             << orders[i].status << "\n";
    }
    fout.close();
}

void saveAll() {
    saveCustomers();
    saveMaterials();
    savePrinters();
    saveOrders();
    cout << GREEN << "  บันทึกข้อมูลทั้งหมดลงไฟล์เรียบร้อยแล้ว\n" << RESET;
}

void appendSalesHistory(const string &entry) {
    ofstream fout(F_SALES.c_str(), ios::app);
    fout << entry << "\n";
    fout.close();
}

/* ==========================================================================
   6) SEARCH (return index หรือ -1)
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
   7) DISPLAY (LIST) FUNCTIONS
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

void listPrinters() {
    printHeader("รายการเครื่องพิมพ์ทั้งหมด");
    if (printerCount == 0) { cout << "  (ไม่มีข้อมูล)\n"; return; }
    cout << left << setw(8) << "รหัส" << setw(16) << "ชื่อเครื่อง"
         << setw(14) << "สถานะ" << "ออเดอร์ปัจจุบัน\n";
    printLine();
    for (int i = 0; i < printerCount; i++) {
        cout << left << setw(8) << printers[i].code << setw(16) << printers[i].name;
        if (printers[i].status == "Idle") cout << GREEN;
        else if (printers[i].status == "Printing") cout << YELLOW;
        else cout << RED;
        cout << setw(14) << printers[i].status << RESET << printers[i].currentOrder << "\n";
    }
}

void listOrders() {
    printHeader("รายการออเดอร์ทั้งหมด");
    if (orderCount == 0) { cout << "  (ไม่มีข้อมูล)\n"; return; }
    cout << left << setw(8) << "รหัส" << setw(8) << "ลูกค้า" << setw(8) << "วัสดุ"
         << setw(8) << "เครื่อง" << setw(10) << "น.นัก(g)" << setw(8) << "ชม."
         << setw(10) << "ราคา" << "สถานะ\n";
    printLine();
    for (int i = 0; i < orderCount; i++) {
        cout << left << setw(8) << orders[i].code << setw(8) << orders[i].customerCode
             << setw(8) << orders[i].materialCode << setw(8) << orders[i].printerCode
             << setw(10) << fixed << setprecision(1) << orders[i].weight
             << setw(8) << orders[i].hours
             << setw(10) << setprecision(2) << orders[i].price
             << orders[i].status << "\n";
    }
}

/* ==========================================================================
   8) CUSTOMER MANAGEMENT
   ========================================================================== */
void insertCustomer() {
    printHeader("เพิ่มลูกค้าใหม่");
    if (customerCount >= MAX_CUSTOMERS) { cout << RED << "  ข้อมูลลูกค้าเต็มแล้ว\n" << RESET; return; }
    Customer c;
    c.code = genCode("C", nextCustomerId++);
    c.name = readLineTrim("  ชื่อลูกค้า: ");
    c.phone = readLineTrim("  เบอร์โทร: ");
    c.address = readLineTrim("  ที่อยู่: ");
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

void customerMenu() {
    while (true) {
        clearScreen();
        printHeader("จัดการข้อมูลลูกค้า");
        cout << "  1. แสดงรายชื่อลูกค้าทั้งหมด\n";
        cout << "  2. ค้นหาลูกค้า\n";
        cout << "  3. เพิ่มลูกค้าใหม่\n";
        cout << "  0. กลับเมนูหลัก\n";
        int c = readIntInRange("\n  เลือกเมนู: ", 0, 3);
        clearScreen();
        if (c == 1) listCustomers();
        else if (c == 2) searchCustomer();
        else if (c == 3) insertCustomer();
        else if (c == 0) return;
        pause();
    }
}

/* ==========================================================================
   9) MATERIAL MANAGEMENT
   ========================================================================== */
void insertMaterial() {
    printHeader("เพิ่มวัสดุใหม่");
    if (materialCount >= MAX_MATERIALS) { cout << RED << "  ข้อมูลวัสดุเต็มแล้ว\n" << RESET; return; }
    Material m;
    m.code = genCode("M", nextMaterialId++);
    m.name = readLineTrim("  ชื่อวัสดุ (เช่น PLA, ABS, PETG): ");
    m.color = readLineTrim("  สี (เช่น Red, Black, White): ");
    m.pricePerGram = readPositiveDouble("  ราคาต่อกรัม (บาท): ");
    m.stockGram = readPositiveDouble("  จำนวนคงเหลือ (กรัม): ");
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
    string key = readLineTrim("  กรอกรหัสวัสดุที่ต้องการลบ: ");
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
   10) PRINTER MANAGEMENT
   ========================================================================== */
void insertPrinter() {
    printHeader("เพิ่มเครื่องพิมพ์ใหม่");
    if (printerCount >= MAX_PRINTERS) { cout << RED << "  ข้อมูลเครื่องพิมพ์เต็มแล้ว\n" << RESET; return; }
    Printer p;
    p.code = genCode("P", nextPrinterId++);
    p.name = readLineTrim("  ชื่อ/รุ่นเครื่องพิมพ์: ");
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
    cout << left << setw(8) << "รหัส" << setw(16) << "ชื่อเครื่อง" << setw(14) << "สถานะ" << "ออเดอร์ปัจจุบัน\n";
    printLine();
    for (int i = 0; i < printerCount; i++) {
        if (toUpperStr(printers[i].code) == toUpperStr(key) || containsIgnoreCase(printers[i].name, key)) {
            cout << left << setw(8) << printers[i].code << setw(16) << printers[i].name
                 << setw(14) << printers[i].status << printers[i].currentOrder << "\n";
            found = true;
        }
    }
    if (!found) cout << RED << "  ไม่พบข้อมูลเครื่องพิมพ์\n" << RESET;
}

void deletePrinter() {
    printHeader("ลบเครื่องพิมพ์");
    string key = readLineTrim("  กรอกรหัสเครื่องพิมพ์ที่ต้องการลบ: ");
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
   11) ORDER MANAGEMENT  (Insert order = create print job, select material+color,
        check stock, calc price, assign to queue/printer)
   ========================================================================== */
double calcPrice(double weight, double hours) {
    return BASE_FEE; // placeholder, real calc done inline (kept for clarity)
}

void createOrder() {
    printHeader("สร้างออเดอร์งานพิมพ์ใหม่");
    if (orderCount >= MAX_ORDERS) { cout << RED << "  ออเดอร์เต็มแล้ว\n" << RESET; return; }
    if (customerCount == 0) { cout << RED << "  กรุณาเพิ่มลูกค้าก่อนสร้างออเดอร์\n" << RESET; return; }
    if (materialCount == 0) { cout << RED << "  กรุณาเพิ่มวัสดุก่อนสร้างออเดอร์\n" << RESET; return; }

    // --- เลือกลูกค้า ---
    listCustomers();
    string custKey = readLineTrim("\n  กรอกรหัสลูกค้า: ");
    int ci = findCustomerIndex(custKey);
    if (ci == -1) { cout << RED << "  ไม่พบรหัสลูกค้านี้\n" << RESET; return; }

    // --- เลือกวัสดุ + สี ---
    clearScreen();
    listMaterials();
    string matKey = readLineTrim("\n  กรอกรหัสวัสดุ (รวมสีที่ต้องการ): ");
    int mi = findMaterialIndex(matKey);
    if (mi == -1) { cout << RED << "  ไม่พบรหัสวัสดุนี้\n" << RESET; return; }

    // --- น้ำหนักงานพิมพ์ ---
    double weight = readPositiveDouble("  น้ำหนักโมเดลโดยประมาณ (กรัม): ");
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
    cout << "  วัสดุ/สี    : " << materials[mi].name << " / " << materials[mi].color << "\n";
    cout << "  น้ำหนัก     : " << fixed << setprecision(1) << weight << " กรัม\n";
    cout << "  เวลาโดยประมาณ: " << setprecision(2) << hours << " ชั่วโมง\n";
    cout << "  ราคารวม     : " << setprecision(2) << price << " บาท\n";
    string conf = readLineTrim("  ยืนยันสร้างออเดอร์? (y/n): ");
    if (toUpperStr(conf) != "Y") { cout << YELLOW << "  ยกเลิกการสร้างออเดอร์\n" << RESET; return; }

    // --- หาเครื่องพิมพ์ว่าง ---
    int pi = -1;
    for (int i = 0; i < printerCount; i++) {
        if (printers[i].status == "Idle") { pi = i; break; }
    }

    Order o;
    o.code = genCode("O", nextOrderId++);
    o.customerCode = customers[ci].code;
    o.materialCode = materials[mi].code;
    o.weight = weight;
    o.hours = hours;
    o.price = price;

    if (pi != -1) {
        o.printerCode = printers[pi].code;
        o.status = "Printing";
        printers[pi].status = "Printing";
        printers[pi].currentOrder = o.code;
        cout << GREEN << "  ไม่มีคิวรอ -> มอบหมายให้เครื่อง " << printers[pi].name << " เริ่มพิมพ์ทันที\n" << RESET;
    } else {
        o.printerCode = "-";
        o.status = "Queued";
        cout << YELLOW << "  เครื่องพิมพ์ไม่ว่าง -> ออเดอร์เข้าคิวรอ\n" << RESET;
    }

    // ตัดสต็อกวัสดุทันทีเมื่อสร้างออเดอร์
    materials[mi].stockGram -= weight;

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
         << setw(8) << "เครื่อง" << setw(10) << "น.นัก(g)" << setw(8) << "ชม."
         << setw(10) << "ราคา" << "สถานะ\n";
    printLine();
    for (int i = 0; i < orderCount; i++) {
        int ci = findCustomerIndex(orders[i].customerCode);
        bool nameMatch = (ci != -1) && containsIgnoreCase(customers[ci].name, key);
        if (toUpperStr(orders[i].code) == toUpperStr(key) ||
            toUpperStr(orders[i].customerCode) == toUpperStr(key) || nameMatch) {
            cout << left << setw(8) << orders[i].code << setw(8) << orders[i].customerCode
                 << setw(8) << orders[i].materialCode << setw(8) << orders[i].printerCode
                 << setw(10) << fixed << setprecision(1) << orders[i].weight
                 << setw(8) << setprecision(2) << orders[i].hours
                 << setw(10) << orders[i].price << orders[i].status << "\n";
            found = true;
        }
    }
    if (!found) cout << RED << "  ไม่พบออเดอร์\n" << RESET;
}

// ประมวลผลคิว: ดึงออเดอร์ที่สถานะ Queued ไปให้เครื่องที่ว่าง (Idle)
void processQueue() {
    printHeader("ประมวลผลคิวงานพิมพ์ (จับคู่ออเดอร์ที่รอกับเครื่องว่าง)");
    int assigned = 0;
    for (int i = 0; i < orderCount; i++) {
        if (orders[i].status != "Queued") continue;
        for (int p = 0; p < printerCount; p++) {
            if (printers[p].status == "Idle") {
                orders[i].printerCode = printers[p].code;
                orders[i].status = "Printing";
                printers[p].status = "Printing";
                printers[p].currentOrder = orders[i].code;
                cout << GREEN << "  ออเดอร์ " << orders[i].code << " -> เครื่อง "
                     << printers[p].name << RESET << "\n";
                assigned++;
                break;
            }
        }
    }
    if (assigned == 0) cout << YELLOW << "  ไม่มีคิวที่จับคู่ได้ (ไม่มีคิวรอ หรือไม่มีเครื่องว่าง)\n" << RESET;
    saveOrders();
    savePrinters();
}

// ทำเครื่องหมายว่าออเดอร์พิมพ์เสร็จแล้ว (Printing -> Completed) และคืนเครื่องเป็น Idle
void markOrderCompleted() {
    printHeader("แจ้งพิมพ์งานเสร็จสิ้น (Printing -> Completed)");
    string key = readLineTrim("  กรอกรหัสออเดอร์: ");
    int oi = findOrderIndex(key);
    if (oi == -1) { cout << RED << "  ไม่พบออเดอร์นี้\n" << RESET; return; }
    if (orders[oi].status != "Printing") {
        cout << RED << "  ออเดอร์นี้ไม่ได้อยู่ในสถานะกำลังพิมพ์\n" << RESET;
        return;
    }
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

void cancelOrder() {
    printHeader("ยกเลิกออเดอร์");
    string key = readLineTrim("  กรอกรหัสออเดอร์ที่ต้องการยกเลิก: ");
    int oi = findOrderIndex(key);
    if (oi == -1) { cout << RED << "  ไม่พบออเดอร์นี้\n" << RESET; return; }
    if (orders[oi].status == "Paid" || orders[oi].status == "Cancelled") {
        cout << RED << "  ออเดอร์นี้ไม่สามารถยกเลิกได้ (สถานะ: " << orders[oi].status << ")\n" << RESET;
        return;
    }
    string conf = readLineTrim("  ยืนยันยกเลิกออเดอร์ " + orders[oi].code + "? (y/n): ");
    if (toUpperStr(conf) != "Y") { cout << YELLOW << "  ยกเลิกการดำเนินการ\n" << RESET; return; }

    // คืนวัสดุกลับสต็อก
    int mi = findMaterialIndex(orders[oi].materialCode);
    if (mi != -1) materials[mi].stockGram += orders[oi].weight;

    // ปลดเครื่องพิมพ์ถ้ากำลังพิมพ์อยู่
    if (orders[oi].status == "Printing") {
        int pi = findPrinterIndex(orders[oi].printerCode);
        if (pi != -1) { printers[pi].status = "Idle"; printers[pi].currentOrder = "-"; }
    }

    orders[oi].status = "Cancelled";
    saveOrders();
    saveMaterials();
    savePrinters();
    cout << GREEN << "  ยกเลิกออเดอร์สำเร็จ (คืนวัสดุเข้าสต็อกแล้ว)\n" << RESET;
}

void queueStatusView() {
    printHeader("สถานะคิวงานพิมพ์ (ตามเครื่องพิมพ์)");
    for (int p = 0; p < printerCount; p++) {
        cout << BOLD << "  เครื่อง " << printers[p].code << " (" << printers[p].name << ") - "
             << printers[p].status << RESET << "\n";
        bool any = false;
        for (int i = 0; i < orderCount; i++) {
            if (orders[i].printerCode == printers[p].code &&
                (orders[i].status == "Printing")) {
                cout << "     -> กำลังพิมพ์ออเดอร์: " << orders[i].code << "\n";
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
        clearScreen();
        printHeader("จัดการออเดอร์งานพิมพ์");
        cout << "  1. แสดงออเดอร์ทั้งหมด\n";
        cout << "  2. ค้นหาออเดอร์\n";
        cout << "  3. สร้างออเดอร์ใหม่\n";
        cout << "  4. ยกเลิกออเดอร์\n";
        cout << "  5. ดูสถานะคิว/เครื่องพิมพ์\n";
        cout << "  6. ประมวลผลคิว (จับคู่งานรอกับเครื่องว่าง)\n";
        cout << "  7. แจ้งพิมพ์งานเสร็จสิ้น (พร้อมส่งมอบ)\n";
        cout << "  0. กลับเมนูหลัก\n";
        int c = readIntInRange("\n  เลือกเมนู: ", 0, 7);
        clearScreen();
        if (c == 1) listOrders();
        else if (c == 2) searchOrder();
        else if (c == 3) createOrder();
        else if (c == 4) cancelOrder();
        else if (c == 5) queueStatusView();
        else if (c == 6) processQueue();
        else if (c == 7) markOrderCompleted();
        else if (c == 0) return;
        pause();
    }
}

/* ==========================================================================
   12) POS -- CHECKOUT / RECEIPT
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
    printHeader("POS - ชำระเงิน / ออกใบเสร็จ");
    cout << BOLD << "  ออเดอร์ที่พร้อมชำระเงิน (สถานะ Completed):\n" << RESET;
    bool any = false;
    for (int i = 0; i < orderCount; i++) {
        if (orders[i].status == "Completed") {
            cout << "   - " << orders[i].code << "  ราคา: " << fixed << setprecision(2)
                 << orders[i].price << " บาท\n";
            any = true;
        }
    }
    if (!any) { cout << YELLOW << "  ไม่มีออเดอร์ที่พร้อมชำระเงินขณะนี้\n" << RESET; return; }

    string key = readLineTrim("\n  กรอกรหัสออเดอร์ที่จะชำระเงิน: ");
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
        cash = readPositiveDouble("  รับเงินสด (บาท): ");
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

    stringstream ss;
    time_t now = time(0);
    char buf[64];
    strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M", localtime(&now));
    ss << buf << "|" << orders[oi].code << "|" << customers[ci].name << "|"
       << materials[mi].name << "|" << materials[mi].color << "|"
       << fixed << setprecision(2) << orders[oi].price << "|" << cash << "|" << change;
    appendSalesHistory(ss.str());
}

/* ==========================================================================
   13) REPORTS -- ประวัติการขาย
   ========================================================================== */
void showSalesHistory() {
    printHeader("ประวัติการขาย (Sales History)");
    ifstream fin(F_SALES.c_str());
    if (!fin.is_open()) { cout << "  (ยังไม่มีประวัติการขาย)\n"; return; }
    string line;
    double total = 0;
    int count = 0;
    cout << left << setw(17) << "วันที่" << setw(8) << "ออเดอร์" << setw(16) << "ลูกค้า"
         << setw(10) << "วัสดุ" << setw(8) << "สี" << setw(10) << "ยอด" << "\n";
    printLine();
    while (getline(fin, line)) {
        if (trim(line).empty()) continue;
        string f[10]; int n;
        splitLine(line, '|', f, n, 10);
        if (n < 8) continue;
        cout << left << setw(17) << f[0] << setw(8) << f[1] << setw(16) << f[2]
             << setw(10) << f[3] << setw(8) << f[4] << setw(10) << f[5] << "\n";
        total += toDouble(f[5]);
        count++;
    }
    fin.close();
    printLine();
    cout << GREEN << BOLD << "  จำนวนบิลทั้งหมด: " << count << "  ยอดขายรวม: "
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
   14) MAIN MENU
   ========================================================================== */
void seedSamplePrintersIfEmpty() {
    // ถ้ายังไม่มีเครื่องพิมพ์เลย ให้สร้างตัวอย่างไว้ 2 เครื่อง เพื่อให้ทดสอบระบบได้ทันที
    if (printerCount == 0) {
        Printer p1; p1.code = genCode("P", nextPrinterId++); p1.name = "Ender-3 V2";
        p1.status = "Idle"; p1.currentOrder = "-";
        Printer p2; p2.code = genCode("P", nextPrinterId++); p2.name = "Prusa MK3S";
        p2.status = "Idle"; p2.currentOrder = "-";
        printers[printerCount++] = p1;
        printers[printerCount++] = p2;
        savePrinters();
    }
}

int main() {
    // 1) Create: โหลดข้อมูลจาก Text File เข้าสู่ Array
    loadCustomers();
    loadMaterials();
    loadPrinters();
    loadOrders();
    seedSamplePrintersIfEmpty();

    while (true) {
        clearScreen();
        cout << BLUE << BOLD;
        cout << " ██████╗ ██████╗     ██████╗ ██████╗ ██╗███╗   ██╗████████╗\n";
        cout << " ╚════██╗██╔══██╗    ██╔══██╗██╔══██╗██║████╗  ██║╚══██╔══╝\n";
        cout << "  █████╔╝██║  ██║    ██████╔╝██████╔╝██║██╔██╗ ██║   ██║   \n";
        cout << "  ╚═══██╗██║  ██║    ██╔═══╝ ██╔══██╗██║██║╚██╗██║   ██║   \n";
        cout << " ██████╔╝██████╔╝    ██║     ██║  ██║██║██║ ╚████║   ██║   \n";
        cout << " ╚═════╝ ╚═════╝     ╚═╝     ╚═╝  ╚═╝╚═╝╚═╝  ╚═══╝   ╚═╝   \n";
        cout << RESET;
        printHeader("ระบบจัดการร้าน 3D PRINTING - เมนูหลัก");
        cout << GREEN << "  [1] " << RESET << "จัดการข้อมูลลูกค้า\n";
        cout << GREEN << "  [2] " << RESET << "จัดการข้อมูลวัสดุ (พร้อมสี/สต็อก)\n";
        cout << GREEN << "  [3] " << RESET << "จัดการเครื่องพิมพ์\n";
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