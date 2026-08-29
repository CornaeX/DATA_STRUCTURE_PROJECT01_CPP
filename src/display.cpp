#include "display.h"
#include "globals.h"
#include "utils.h"
#include "search.h"
#include "storage.h"
#include "order.h" // ใช้ autoAssignQueue() เพื่อจับคู่คิวกับเครื่องว่างอัตโนมัติทุกครั้งที่เรียก autoCompletePrinting()
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>

/* ==========================================================================
   8) DISPLAY (LIST) FUNCTIONS
   ========================================================================== */
void listCustomers() {
    printHeader("รายชื่อลูกค้าทั้งหมด");
    if (customerCount == 0) { cout << "  (ไม่มีข้อมูล)\n"; return; }
    cout << padRight("รหัส", 8) << padRight("ชื่อ", 20)
         << padRight("เบอร์โทร", 15) << "ที่อยู่\n";
    printLine();
    for (int i = 0; i < customerCount; i++) {
        cout << padRight(customers[i].code, 8) << padRight(customers[i].name, 20)
             << padRight(customers[i].phone, 15) << customers[i].address << "\n";
    }
}

void listMaterials() {
    printHeader("รายการวัสดุทั้งหมด");
    if (materialCount == 0) { cout << "  (ไม่มีข้อมูล)\n"; return; }
    cout << padRight("รหัส", 8) << padRight("ชื่อวัสดุ", 14) << padRight("สี", 10)
         << padRight("ราคา/กรัม", 14) << "คงเหลือ(กรัม)\n";
    printLine();
    for (int i = 0; i < materialCount; i++) {
        cout << padRight(materials[i].code, 8) << padRight(materials[i].name, 14)
             << padRight(materials[i].color, 10)
             << setw(14) << left << fixed << setprecision(2) << materials[i].pricePerGram;
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

    // เครื่องที่เพิ่งว่างจากขั้นตอนด้านบนอาจมีคิวรออยู่พอดี -> จับคู่ให้ทันทีในตัวนี้เลย
    // ทำให้ทุกเมนูที่เรียก autoCompletePrinting() (แทบทุกเมนูในระบบ) จับคู่คิวให้อัตโนมัติไปในตัว
    // ผู้ใช้จึงไม่ต้องเข้าเมนู "ประมวลผลคิว" ด้วยตนเองอีกต่อไป
    autoAssignQueue();
}


void listPrinters() {
    autoCompletePrinting();
    printHeader("รายการเครื่องพิมพ์ทั้งหมด");
    if (printerCount == 0) { cout << "  (ไม่มีข้อมูล)\n"; return; }
    cout << padRight("รหัส", 8) << padRight("ชื่อเครื่อง", 16) << padRight("ประเภท", 10)
         << padRight("สถานะ", 14) << "ออเดอร์ปัจจุบัน\n";
    printLine();
    for (int i = 0; i < printerCount; i++) {
        cout << padRight(printers[i].code, 8) << padRight(printers[i].name, 16)
             << padRight(printers[i].type, 10);
        if (printers[i].status == "Idle") cout << GREEN;
        else if (printers[i].status == "Printing") cout << YELLOW;
        else cout << RED;
        cout << padRight(printers[i].status, 14) << RESET << printers[i].currentOrder;
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
    cout << padRight("รหัส", 8) << padRight("ลูกค้า", 8) << padRight("วัสดุ", 8)
         << padRight("เครื่อง", 8) << padRight("ไฟล์งาน", 16) << padRight("น.นัก(g)", 10)
         << padRight("ชม.", 8) << padRight("ราคา", 10) << "สถานะ\n";
    printLine();
    for (int i = 0; i < orderCount; i++) {
        cout << padRight(orders[i].code, 8) << padRight(orders[i].customerCode, 8)
             << padRight(orders[i].materialCode, 8) << padRight(orders[i].printerCode, 8)
             << padRight(orders[i].fileName, 16)
             << setw(10) << left << fixed << setprecision(1) << orders[i].weight
             << setw(8) << left << orders[i].hours
             << setw(10) << left << setprecision(2) << orders[i].price
             << orders[i].status << printingTimeLabel(orders[i])
             << (orders[i].paid ? (GREEN " [ชำระแล้ว-" + orders[i].paymentMethod + "]" RESET) : YELLOW " [ยังไม่ชำระ]" RESET)
             << "\n";
    }
}
