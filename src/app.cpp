#include "app.h"
#include "globals.h"
#include "utils.h"
#include "storage.h"
#include "display.h"
#include "customer.h"
#include "material.h"
#include "printer.h"
#include "order.h"
#include "pos.h"
#include "report.h"
#include <iostream>

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

// --------------------------------------------------------------------------
// โหมดเจ้าของร้าน/พนักงาน: ต้องกรอกรหัสผ่านก่อนเข้าใช้งาน (ดูค่า OWNER_PASSWORD ด้านบนไฟล์)
// --------------------------------------------------------------------------
bool ownerLogin() {
    printHeader("เข้าสู่ระบบเจ้าของร้าน/พนักงาน");
    for (int attempt = 0; attempt < 3; attempt++) {
        string pw = readLineTrim("  กรอกรหัสผ่าน [0=ยกเลิก]: ");
        if (pw == "0") return false;
        if (pw == OWNER_PASSWORD) return true;
        cout << RED << "  รหัสผ่านไม่ถูกต้อง (เหลืออีก " << (2 - attempt) << " ครั้ง)\n" << RESET;
    }
    cout << RED << "  กรอกรหัสผ่านผิดครบจำนวนครั้งที่กำหนดแล้ว\n" << RESET;
    return false;
}

// เมนูหลักของโหมดเจ้าของร้าน/พนักงาน (เข้าถึงฟังก์ชันจัดการระบบทั้งหมด)
void ownerMainMenu() {
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
        printHeader("ระบบจัดการร้าน 3D PRINTING - เมนูเจ้าของร้าน/พนักงาน");
        printMenuOption(1, "จัดการข้อมูลลูกค้า");
        printMenuOption(2, "จัดการข้อมูลวัสดุ");
        printMenuOption(3, "จัดการเครื่องพิมพ์");
        printMenuOption(4, "จัดการออเดอร์งานพิมพ์ / คิวงาน");
        printMenuOption(5, "POS - ชำระเงิน / ออกใบเสร็จ");
        printMenuOption(6, "รายงาน / ประวัติการขาย");
        printMenuOption(9, "บันทึกข้อมูลทั้งหมด", YELLOW);
        printMenuOption(0, "ออกจากระบบ", RED);

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
            return; // กลับไปหน้าเลือกโหมด ไม่ปิดโปรแกรม
        }
    }
}
