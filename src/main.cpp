/* ============================================================================
   main.cpp
   ระบบจัดการร้าน 3D Printing (3D Printing Shop Management System)
   Text-based / Console UI (TUI/CUI)
   - ใช้ Array ล้วนในการเก็บข้อมูล (ไม่ใช้ vector/STL container)
   - Create (โหลดจากไฟล์ .json), Search, Insert, Delete
   - จัดการ ลูกค้า / วัสดุ(พร้อมสี) / เครื่องพิมพ์(พร้อมประเภท) / ออเดอร์(พร้อมไฟล์งาน) / คิวงาน / POS
   - สถานะออเดอร์: Queued -> Printing -> Completed -> PickedUp / Shipped (หรือ Cancelled)
     การชำระเงินแยกต่างหาก (paid: Cash/Online)
   - ข้อมูลทั้งหมดบันทึกเป็นไฟล์ .json (เขียน/อ่านด้วยฟังก์ชัน JSON เล็ก ๆ ที่เขียนขึ้นเอง
     ไม่พึ่งไลบรารีภายนอก เพื่อให้คอมไพล์ได้ด้วย g++ ธรรมดา)

   โครงสร้างไฟล์ของโปรเจกต์ (แยกจาก main.cpp ไฟล์เดียวเดิม เพื่อให้อ่าน/แก้ไขง่ายขึ้น):
     types.h            ค่าคงที่ + struct หลัก
     globals.h/.cpp      global array/counter
     utils.h/.cpp         ฟังก์ชันช่วยเหลือทั่วไป (string, input, console)
     json_util.h/.cpp     mini JSON reader/writer
     storage.h/.cpp       โหลด/บันทึกข้อมูลจากไฟล์ .json
     search.h/.cpp        ค้นหา index ในแต่ละ array
     display.h/.cpp       แสดงรายการ + ฟังก์ชันเวลาการพิมพ์
     customer.h/.cpp      จัดการลูกค้า
     material.h/.cpp      จัดการวัสดุ
     printer.h/.cpp       จัดการเครื่องพิมพ์
     order.h/.cpp         จัดการออเดอร์/คิวงาน (เจ้าของ + ลูกค้า self-service)
     pos.h/.cpp           POS ชำระเงิน/ใบเสร็จ
     report.h/.cpp        รายงาน
     app.h/.cpp           seed ข้อมูล, login, เมนูเจ้าของร้าน
     main.cpp             จุดเริ่มโปรแกรม + เมนูเลือกโหมด
   ============================================================================ */

#include "types.h"
#include "utils.h"
#include "storage.h"
#include "display.h"
#include "order.h"
#include "app.h"
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

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
        printHeader("ระบบจัดการร้าน 3D PRINTING - เลือกโหมดการใช้งาน");
        cout << GREEN << "  [1] " << RESET << "โหมดลูกค้า\n";
        cout << GREEN << "  [2] " << RESET << "โหมดเจ้าของร้าน/พนักงาน\n";
        cout << RED   << "  [0] " << RESET << "ออกจากโปรแกรม\n";

        int mode = readIntInRange("\n  กรุณาเลือกโหมด: ", 0, 2);

        if (mode == 1) {
            customerKioskMenu();
        } else if (mode == 2) {
            clearScreen();
            if (ownerLogin()) {
                clearScreen();
                ownerMainMenu();
            } else {
                pause();
            }
        } else if (mode == 0) {
            saveAll();
            cout << GREEN << "\n  ขอบคุณที่ใช้งานระบบ ลาก่อน!\n" << RESET;
            break;
        }
    }
    return 0;
}