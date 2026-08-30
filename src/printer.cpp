#include "printer.h"
#include "globals.h"
#include "utils.h"
#include "search.h"
#include "storage.h"
#include "display.h"
#include "order.h" // ใช้ autoAssignQueue() เพื่อจับคู่คิวที่รออยู่กับเครื่องพิมพ์ที่เพิ่งว่าง/เพิ่งเพิ่มทันที
#include <iostream>

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
    autoAssignQueue(); // เครื่องใหม่ว่างอยู่ ถ้ามีคิวรออยู่พอดี ให้จับคู่ทันที ไม่ต้องรอ
}

void searchPrinter() {
    printHeader("ค้นหาเครื่องพิมพ์ (รหัสหรือชื่อ)");
    string key = readLineTrim("  กรอกรหัสหรือชื่อ: ");
    bool found = false;
    cout << padRight("รหัส", 8) << padRight("ชื่อเครื่อง", 16) << padRight("ประเภท", 10)
         << padRight("สถานะ", 14) << "ออเดอร์ปัจจุบัน\n";
    printLine();
    for (int i = 0; i < printerCount; i++) {
        if (toUpperStr(printers[i].code) == toUpperStr(key) || containsIgnoreCase(printers[i].name, key)) {
            cout << padRight(printers[i].code, 8) << padRight(printers[i].name, 16)
                 << padRight(printers[i].type, 10) << padRight(printers[i].status, 14)
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
    printMenuOption(1, "Idle (พร้อมใช้งาน)");
    printMenuOption(2, "Maintenance (ซ่อมบำรุง)");
    int c = readIntInRange("  เลือก: ", 1, 2);
    printers[idx].status = (c == 1) ? "Idle" : "Maintenance";
    savePrinters();
    cout << GREEN << "  อัปเดตสถานะสำเร็จ\n" << RESET;
    if (printers[idx].status == "Idle") autoAssignQueue(); // เพิ่งว่าง ลองจับคู่กับคิวที่รออยู่ทันที
}

void printerMenu() {
    while (true) {
        autoCompletePrinting();
        clearScreen();
        printHeader("จัดการเครื่องพิมพ์");
        printMenuOption(1, "แสดงรายการเครื่องพิมพ์ทั้งหมด");
        printMenuOption(2, "ค้นหาเครื่องพิมพ์");
        printMenuOption(3, "เพิ่มเครื่องพิมพ์ใหม่");
        printMenuOption(4, "ลบเครื่องพิมพ์");
        printMenuOption(5, "เปลี่ยนสถานะเครื่อง (Idle/Maintenance)");
        printMenuOption(0, "กลับเมนูหลัก", RED);
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
