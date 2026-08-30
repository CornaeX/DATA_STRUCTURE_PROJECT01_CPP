#include "customer.h"
#include "globals.h"
#include "utils.h"
#include "search.h"
#include "storage.h"
#include "display.h"
#include <iostream>

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

// ให้ลูกค้าค้นหาบัญชีตัวเองด้วยเบอร์โทรในโหมด self-service ถ้าไม่พบ ให้เสนอสมัครสมาชิกใหม่ทันที
// คืนค่า index ใน customers[] ของบัญชีที่ใช้งาน หรือ -1 ถ้าผู้ใช้ยกเลิก
int customerSelfLookupOrRegister() {
    while (true) {
        printHeader("เข้าสู่ระบบลูกค้า");
        cout << YELLOW << "  (พิมพ์ 0 แล้ว Enter เพื่อยกเลิกและกลับเมนูเลือกโหมด)\n" << RESET;
        string phone = readLineTrim("  กรอกเบอร์โทรของคุณ [0=ยกเลิก]: ");
        if (phone == "0") return -1;

        int ci = findCustomerIndexByPhone(phone);
        if (ci != -1) {
            cout << GREEN << "  ยินดีต้อนรับกลับ คุณ " << customers[ci].name << " (รหัสลูกค้า " << customers[ci].code << ")\n" << RESET;
            return ci;
        }

        cout << YELLOW << "  ไม่พบบัญชีที่ใช้เบอร์นี้ ต้องการสมัครสมาชิกใหม่หรือไม่?\n" << RESET;
        string conf = readLineTrim("  สมัครสมาชิกใหม่? (y/n): ");
        if (toUpperStr(conf) != "Y") continue; // วนกลับไปให้กรอกเบอร์ใหม่ (หรือพิมพ์ 0 เพื่อยกเลิก)

        if (customerCount >= MAX_CUSTOMERS) { cout << RED << "  ข้อมูลลูกค้าเต็มแล้ว กรุณาติดต่อพนักงาน\n" << RESET; return -1; }

        Customer c;
        c.phone = phone;
        string name = readLineTrim("  ชื่อของคุณ [0=ยกเลิก]: ");
        if (name == "0") continue;
        c.name = name;
        string address = readLineTrim("  ที่อยู่ [0=ยกเลิก]: ");
        if (address == "0") continue;
        c.address = address;
        c.code = genCode("C", nextCustomerId++);
        customers[customerCount++] = c;
        saveCustomers();
        cout << GREEN << "  สมัครสมาชิกสำเร็จ รหัสลูกค้าของคุณคือ " << c.code << "\n" << RESET;
        return customerCount - 1;
    }
}

void searchCustomer() {
    printHeader("ค้นหาลูกค้า (รหัสหรือชื่อ)");
    string key = readLineTrim("  กรอกรหัสหรือชื่อ: ");
    bool found = false;
    cout << padRight("รหัส", 8) << padRight("ชื่อ", 20) << padRight("เบอร์โทร", 15) << "ที่อยู่\n";
    printLine();
    for (int i = 0; i < customerCount; i++) {
        if (toUpperStr(customers[i].code) == toUpperStr(key) || containsIgnoreCase(customers[i].name, key)) {
            cout << padRight(customers[i].code, 8) << padRight(customers[i].name, 20)
                 << padRight(customers[i].phone, 15) << customers[i].address << "\n";
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
    // กันลบลูกค้าที่ยังมีออเดอร์ค้างอยู่ (ยังไม่ PickedUp/Shipped/Cancelled)
    for (int i = 0; i < orderCount; i++) {
        if (orders[i].customerCode == customers[idx].code &&
            orders[i].status != "PickedUp" && orders[i].status != "Shipped" && orders[i].status != "Cancelled") {
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
        printMenuOption(1, "แสดงรายชื่อลูกค้าทั้งหมด");
        printMenuOption(2, "ค้นหาลูกค้า");
        printMenuOption(3, "เพิ่มลูกค้าใหม่");
        printMenuOption(4, "ลบลูกค้า");
        printMenuOption(0, "กลับเมนูหลัก", RED);
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
