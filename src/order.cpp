#include "order.h"
#include "globals.h"
#include "utils.h"
#include "search.h"
#include "storage.h"
#include "display.h"
#include "customer.h"
#include "pos.h"
#include <iostream>
#include <iomanip>
#include <ctime>

// จอเรียลไทม์ต้อง "เช็คว่ามีการกด Enter หรือยัง" แบบไม่บล็อกโปรแกรม (non-blocking) เพื่อสลับกับ
// การวาดหน้าจอใหม่ทุก ๆ ช่วงเวลาสั้น ๆ ได้ - ตั้งใจไม่ใช้ std::thread เพราะชุดคอมไพเลอร์ MinGW บาง
// รุ่นบน Windows (เช่นรุ่นที่มากับ Dev-C++/TDM-GCC เก่า ๆ) ถูกคอมไพล์มาแบบ "win32 threads" ไม่ใช่
// "posix threads" ทำให้ <thread>/std::thread ใช้งานไม่ได้เลย (คอมไพล์ไม่ผ่าน หรือ link ไม่ผ่านแม้เติม
// -pthread) จึงใช้วิธีเช็คคีย์บอร์ดแบบ non-blocking เฉพาะแพลตฟอร์ม (conio.h บน Windows,
// termios+fcntl บน Linux/Mac) แทน เพื่อให้คอมไพล์ได้ชัวร์ทุกที่โดยไม่ต้องพึ่ง threading model ใด ๆ
#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
// หมายเหตุ: <unistd.h> ของ POSIX มีฟังก์ชันชื่อ pause() ของตัวเอง (ใช้หยุดโปรเซสรอสัญญาณ)
// ซึ่งชนกับ void pause() ที่ประกาศไว้ใน utils.h ของโปรเจกต์นี้อยู่แล้ว (ฟังก์ชัน "กด Enter เพื่อดำเนินการต่อ")
// จึงต้องเปลี่ยนชื่อ pause() ของระบบชั่วคราวตอน include เพื่อไม่ให้ประกาศชนกัน (ไม่กระทบการเรียกใช้จริง
// เพราะโค้ดในไฟล์นี้ไม่ได้เรียก pause() ของระบบอยู่แล้ว)
#define pause pause_unistd_unused_alias
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#undef pause
#endif

// คืนค่า true ถ้ามีการกด Enter ค้างอยู่ใน input buffer ตอนนี้ (เช็คแบบไม่บล็อกโปรแกรม)
static bool enterPressedNonBlocking() {
#ifdef _WIN32
    bool pressed = false;
    while (_kbhit()) {
        int ch = _getch();
        if (ch == '\r' || ch == '\n') pressed = true;
    }
    return pressed;
#else
    // สลับ terminal เป็นโหมด non-canonical + non-blocking ชั่วคราวเพื่ออ่านคีย์ที่กดค้างไว้ (ถ้ามี)
    // แล้วคืนค่าการตั้งค่า terminal กลับเป็นแบบเดิมก่อนออกจากฟังก์ชันเสมอ
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~((unsigned)ICANON | (unsigned)ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    int oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    bool pressed = false;
    char ch;
    while (read(STDIN_FILENO, &ch, 1) > 0) {
        if (ch == '\n' || ch == '\r') pressed = true;
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    return pressed;
#endif
}

// พักโปรแกรมสั้น ๆ (มิลลิวินาที) แบบไม่ต้องพึ่งไลบรารี thread ใด ๆ
static void sleepMs(int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep((useconds_t)ms * 1000);
#endif
}

/* ==========================================================================
   12) ORDER MANAGEMENT  (Insert order = create print job, select material+color,
        check stock, calc price, assign/queue printer)
   สถานะ: Queued -> Printing -> Completed -> PickedUp / Shipped (หรือ Cancelled)
   การชำระเงิน (paid/paymentMethod/fulfillment) แยกอิสระจากสถานะข้างบน ชำระออนไลน์ได้ทุกช่วงสถานะ
   หมายเหตุ: หักวัสดุออกจาก Stock "เมื่อเริ่มพิมพ์จริง" (ตอนสถานะเปลี่ยนเป็น Printing)
             ไม่ใช่ตอนสร้างออเดอร์ ถ้าออเดอร์ยังอยู่ในคิว (Queued) จะยังไม่หักสต็อก
   ========================================================================== */
void createOrder() {
    printHeader("สร้างออเดอร์งานพิมพ์ใหม่");
    cout << YELLOW << "  (พิมพ์ 0 แล้ว Enter ในช่องใดก็ได้ เพื่อยกเลิกและกลับเมนูก่อนหน้า)\n" << RESET;
    if (orderCount >= MAX_ORDERS) { cout << RED << "  ออเดอร์เต็มแล้ว\n" << RESET; return; }
    if (customerCount == 0) { cout << RED << "  กรุณาเพิ่มลูกค้าก่อนสร้างออเดอร์\n" << RESET; return; }
    if (materialCount == 0) { cout << RED << "  กรุณาเพิ่มวัสดุก่อนสร้างออเดอร์\n" << RESET; return; }

    // --- เลือกลูกค้า (เฉพาะโหมดเจ้าของ/พนักงาน ที่เลือกได้ทุกคน) ---
    listCustomers();
    string custKey = readLineTrim("\n  กรอกรหัสลูกค้า [0=ยกเลิก]: ");
    if (custKey == "0") { cout << YELLOW << "  ยกเลิกการสร้างออเดอร์\n" << RESET; return; }
    int ci = findCustomerIndex(custKey);
    if (ci == -1) { cout << RED << "  ไม่พบรหัสลูกค้านี้\n" << RESET; return; }

    createOrderCore(ci);
}

// ตรรกะหลักในการสร้างออเดอร์ เมื่อทราบตัวลูกค้า (ci) แล้ว
// ใช้ร่วมกันทั้งโหมดเจ้าของ/พนักงาน (createOrder ด้านบน เลือกลูกค้าเองได้ทุกคน)
// และโหมดลูกค้า self-service (customerCreateOrder ด้านล่าง ผูกกับลูกค้าที่ล็อกอินอยู่เท่านั้น)
void createOrderCore(int ci) {
    if (orderCount >= MAX_ORDERS) { cout << RED << "  ออเดอร์เต็มแล้ว\n" << RESET; return; }
    if (materialCount == 0) { cout << RED << "  ยังไม่มีวัสดุในระบบ กรุณาติดต่อพนักงาน\n" << RESET; return; }

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

    // --- ระบบเลือกเครื่องพิมพ์ให้อัตโนมัติ (ลูกค้าไม่ต้องเลือกเอง) ---
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
    o.paid = false;
    o.paymentMethod = "";
    o.fulfillment = "";

    // หาเครื่องพิมพ์ที่ว่าง (Idle) เครื่องแรกโดยอัตโนมัติ ถ้าไม่มี -> เข้าคิวรอ
    int pi = -1;
    for (int i = 0; i < printerCount; i++) {
        if (printers[i].status == "Idle") { pi = i; break; }
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
        cout << GREEN << "  มอบหมายให้เครื่อง " << printers[pi].name
             << "ประมาณเสร็จใน " << formatDuration(hours) << RESET;
    } else {
        // ยังไม่มีเครื่องว่าง หรือผู้ใช้ไม่เลือก -> เข้าคิว ยังไม่หักสต็อก
        o.printerCode = "-";
        o.status = "Queued";
        cout << YELLOW << "  ออเดอร์เข้าคิวรอ\n" << RESET;
    }

    orders[orderCount++] = o;
    saveOrders();
    saveMaterials();
    savePrinters();
    cout << GREEN << "  สร้างออเดอร์สำเร็จ รหัส: " << o.code << RESET << "\n";
    cout << CYAN << "  (สามารถชำระเงินออนไลน์ได้ทันทีจากเมนู หรือชำระเงินสดที่ร้านเมื่องานเสร็จก็ได้)\n" << RESET;
}

void searchOrder() {
    printHeader("ค้นหาออเดอร์ (รหัสออเดอร์ หรือ ชื่อ/รหัสลูกค้า)");
    string key = readLineTrim("  กรอกคำค้นหา: ");
    bool found = false;
    cout << padRight("รหัส", 8) << padRight("ลูกค้า", 8) << padRight("วัสดุ", 8)
         << padRight("เครื่อง", 8) << padRight("ไฟล์งาน", 16) << padRight("น.นัก(g)", 10)
         << padRight("ชม.", 8) << padRight("ราคา", 10) << "สถานะ\n";
    printLine();
    for (int i = 0; i < orderCount; i++) {
        int ci = findCustomerIndex(orders[i].customerCode);
        bool nameMatch = (ci != -1) && containsIgnoreCase(customers[ci].name, key);
        if (toUpperStr(orders[i].code) == toUpperStr(key) ||
            toUpperStr(orders[i].customerCode) == toUpperStr(key) || nameMatch) {
            cout << padRight(orders[i].code, 8) << padRight(orders[i].customerCode, 8)
                 << padRight(orders[i].materialCode, 8) << padRight(orders[i].printerCode, 8)
                 << padRight(orders[i].fileName, 16)
                 << setw(10) << left << fixed << setprecision(1) << orders[i].weight
                 << setw(8) << left << setprecision(2) << orders[i].hours
                 << setw(10) << left << orders[i].price << orders[i].status << "\n";
            found = true;
        }
    }
    if (!found) cout << RED << "  ไม่พบออเดอร์\n" << RESET;
}

// จับคู่ออเดอร์ที่รอคิว (Queued) กับเครื่องพิมพ์ที่ว่าง (Idle) โดยอัตโนมัติ และหักสต็อก ณ จุดนี้ (เริ่มพิมพ์จริง)
// ฟังก์ชันนี้เป็นตัวจริงที่ทำงานเบื้องหลัง เรียกซ้ำได้บ่อยเท่าที่ต้องการโดยไม่มีผลข้างเคียง
// (ถ้าไม่มีคิวรอ หรือไม่มีเครื่องว่าง ก็แค่ไม่ทำอะไร) - ไม่พิมพ์หัวข้อเมนูใด ๆ เพื่อให้เรียกจากที่อื่นได้อย่างเงียบ ๆ
bool autoAssignQueue() {
    bool assignedAny = false;
    for (int i = 0; i < orderCount; i++) {
        if (orders[i].status != "Queued") continue;
        int mi = findMaterialIndex(orders[i].materialCode);
        if (mi == -1) continue;
        if (orders[i].weight > materials[mi].stockGram) continue; // วัสดุไม่พอ รอรอบถัดไป (เผื่อมีการเติมสต็อก)

        for (int p = 0; p < printerCount; p++) {
            if (printers[p].status == "Idle") {
                orders[i].printerCode = printers[p].code;
                orders[i].status = "Printing";
                orders[i].stockDeducted = true;
                orders[i].startTime = time(0);
                printers[p].status = "Printing";
                printers[p].currentOrder = orders[i].code;
                materials[mi].stockGram -= orders[i].weight;
                cout << GREEN << "  [อัตโนมัติ] ออเดอร์ " << orders[i].code << " ที่รอคิวอยู่ -> จับคู่กับเครื่อง "
                     << printers[p].name << " ที่ว่างแล้ว (หักสต็อกวัสดุแล้ว) ประมาณเสร็จใน "
                     << formatDuration(orders[i].hours) << RESET << "\n";
                assignedAny = true;
                break;
            }
        }
    }
    if (assignedAny) {
        saveOrders();
        saveMaterials();
        savePrinters();
    }
    return assignedAny;
}

// เมนูสำหรับตรวจคิวซ้ำด้วยตนเอง - ปกติไม่จำเป็นต้องใช้แล้ว เพราะระบบจับคู่คิวกับเครื่องว่างให้อัตโนมัติอยู่แล้ว
// (ทุกครั้งที่พิมพ์เสร็จ, เพิ่มเครื่องพิมพ์ใหม่, ยกเลิกออเดอร์ หรือเข้าเมนูใดก็ตามที่เรียก autoCompletePrinting())
// เก็บเมนูนี้ไว้เผื่อผู้ใช้ต้องการตรวจสอบสถานะซ้ำหรือดูข้อความยืนยันด้วยตนเอง
void processQueue() {
    autoCompletePrinting(); // ตรวจงานพิมพ์ที่ครบเวลา + จับคู่คิวอัตโนมัติในตัว (เรียก autoAssignQueue ให้แล้ว)
    printHeader("ตรวจสอบคิวงานพิมพ์ (จับคู่ออเดอร์ที่รอกับเครื่องว่าง)");
    cout << CYAN << "  หมายเหตุ: ระบบจะจับคู่คิวกับเครื่องว่างให้อัตโนมัติอยู่แล้วทุกครั้งที่มีการเปลี่ยนแปลง\n"
         << "  ไม่จำเป็นต้องเข้าเมนูนี้อีกต่อไป (เก็บไว้ให้ตรวจสอบซ้ำได้ด้วยตนเองเท่านั้น)\n" << RESET;
    if (!autoAssignQueue()) {
        cout << YELLOW << "  ไม่มีคิวที่จับคู่ได้ (ไม่มีคิวรอ หรือไม่มีเครื่องว่าง)\n" << RESET;
    }
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
    autoAssignQueue(); // เครื่องว่างแล้ว ลองจับคู่กับคิวที่รออยู่ทันที ไม่ต้องรอเข้าเมนูอื่น
}

// ยืนยันว่าส่งมอบสินค้าแล้ว (พิมพ์เสร็จ + ชำระเงินแล้ว -> PickedUp หรือ Shipped ตามวิธีรับสินค้า) ปิดจบวงจรออเดอร์
void markOrderDelivered() {
    printHeader("ยืนยันส่งมอบสินค้า (รับที่ร้าน/จัดส่งแล้ว)");
    string key = readLineTrim("  กรอกรหัสออเดอร์ [0=ยกเลิก]: ");
    if (key == "0") { cout << YELLOW << "  ยกเลิกการดำเนินการ\n" << RESET; return; }
    int oi = findOrderIndex(key);
    if (oi == -1) { cout << RED << "  ไม่พบออเดอร์นี้\n" << RESET; return; }
    if (orders[oi].status != "Completed") {
        cout << RED << "  ออเดอร์นี้ยังไม่เสร็จสิ้นการพิมพ์ (สถานะ: " << orders[oi].status << ")\n" << RESET;
        return;
    }
    if (!orders[oi].paid) {
        cout << RED << "  ออเดอร์นี้ยังไม่ได้ชำระเงิน กรุณาชำระเงินก่อน\n" << RESET;
        return;
    }
    bool shipping = (orders[oi].fulfillment == "Shipping");
    orders[oi].status = shipping ? "Shipped" : "PickedUp";
    saveOrders();
    if (shipping) {
        cout << GREEN << "  ออเดอร์ " << orders[oi].code << " จัดส่งให้ลูกค้าเรียบร้อยแล้ว (Shipped)\n" << RESET;
    } else {
        cout << GREEN << "  ออเดอร์ " << orders[oi].code << " ส่งมอบให้ลูกค้าเรียบร้อยแล้ว (PickedUp)\n" << RESET;
    }
}

void cancelOrder() {
    printHeader("ยกเลิกออเดอร์");
    string key = readLineTrim("  กรอกรหัสออเดอร์ที่ต้องการยกเลิก [0=ไม่ยกเลิก/กลับเมนู]: ");
    if (key == "0") { cout << YELLOW << "  ยกเลิกการดำเนินการ\n" << RESET; return; }
    int oi = findOrderIndex(key);
    if (oi == -1) { cout << RED << "  ไม่พบออเดอร์นี้\n" << RESET; return; }
    if (orders[oi].status == "PickedUp" || orders[oi].status == "Shipped" || orders[oi].status == "Cancelled") {
        cout << RED << "  ออเดอร์นี้ไม่สามารถยกเลิกได้ (สถานะ: " << orders[oi].status << ")\n" << RESET;
        return;
    }
    if (orders[oi].paid) {
        cout << RED << "  ออเดอร์นี้ชำระเงินแล้ว ไม่สามารถยกเลิกได้ กรุณาติดต่อร้านเพื่อขอคืนเงิน\n" << RESET;
        return;
    }
    string conf = readLineTrim("  ยืนยันยกเลิกออเดอร์ " + orders[oi].code + "? (y/n): ");
    if (toUpperStr(conf) != "Y") { cout << YELLOW << "  ยกเลิกการดำเนินการ\n" << RESET; return; }
    cancelOrderCore(oi);
}

// ตรรกะหลักในการยกเลิกออเดอร์ (คืนสต็อก + ปลดเครื่องพิมพ์) เมื่อทราบ index (oi) และผ่านการยืนยัน/ตรวจสิทธิ์แล้ว
void cancelOrderCore(int oi) {
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
    autoAssignQueue(); // เครื่องอาจว่างแล้ว (ถ้ายกเลิกงานที่กำลังพิมพ์อยู่) ลองจับคู่กับคิวที่รออยู่ทันที
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

// --------------------------------------------------------------------------
// จอสถานะแบบเรียลไทม์: วาดหน้าจอสถานะเครื่องพิมพ์/คิวใหม่ทุก 1 วินาทีโดยไม่ต้องกด Enter
// (เวลาที่เหลือของงานที่กำลังพิมพ์จะขยับลงเรื่อย ๆ ให้เห็นสด ๆ) กด Enter เมื่อไหร่ก็ออกจากโหมดนี้
// หมายเหตุ: คอนโซลทั่วไปไม่มี "push event" ให้เรา จึงใช้วิธี clear หน้าจอ + วาดใหม่เป็นรอบ ๆ (polling)
// ใช้เธรดแยกไว้คอยรอผู้ใช้กด Enter เท่านั้น เพื่อไม่ให้การรอคีย์บอร์ดไปบล็อกการรีเฟรชหน้าจอ
// --------------------------------------------------------------------------
void liveQueueMonitor() {
    // เคลียร์ปุ่ม Enter ที่อาจกดค้างไว้จากเมนูก่อนหน้า ไม่ให้ทำให้ออกจากโหมดนี้ทันที
    enterPressedNonBlocking();

    bool stop = false;
    while (!stop) {
        autoCompletePrinting(); // ปรับ Printing -> Completed เมื่อครบเวลา + จับคู่คิวกับเครื่องว่างอัตโนมัติในตัว
        clearScreen();
        printHeader("สถานะเรียลไทม์ - เครื่องพิมพ์ / คิวงาน (อัปเดตอัตโนมัติทุก 1 วินาที)");
        cout << YELLOW << "  กด Enter เพื่อออกจากโหมดนี้ กลับเมนูก่อนหน้า\n" << RESET;
        printLine();

        for (int p = 0; p < printerCount; p++) {
            cout << BOLD << "  เครื่อง " << printers[p].code << " (" << printers[p].name << ") - " << RESET;
            if (printers[p].status == "Idle") cout << GREEN;
            else if (printers[p].status == "Printing") cout << YELLOW;
            else cout << RED;
            cout << printers[p].status << RESET << "\n";

            bool any = false;
            for (int i = 0; i < orderCount; i++) {
                if (orders[i].printerCode != printers[p].code || orders[i].status != "Printing") continue;
                double left = remainingHours(orders[i]);
                double pct = (orders[i].hours > 0.0) ? ((orders[i].hours - left) / orders[i].hours) * 100.0 : 100.0;
                if (pct < 0) pct = 0;
                if (pct > 100) pct = 100;
                cout << "     -> " << orders[i].code << YELLOW << printingTimeLabel(orders[i]) << RESET
                     << "  [" << fixed << setprecision(0) << pct << "%]\n";
                any = true;
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

        // เช็คปุ่ม Enter ทุก ๆ 100ms รวม 1 วินาที (แทนที่จะ sleep รวดเดียว 1 วิ) เพื่อให้กด Enter
        // ออกจากโหมดนี้ได้ไวขึ้น ไม่ต้องรอครบรอบวินาทีก่อน
        for (int tick = 0; tick < 10; tick++) {
            if (enterPressedNonBlocking()) { stop = true; break; }
            sleepMs(100);
        }
    }

    cout << GREEN << "\n  ออกจากโหมดเรียลไทม์แล้ว\n" << RESET;
}

// --------------------------------------------------------------------------
// โหมดลูกค้า (self-service): สร้างออเดอร์ของตัวเอง โดยผูกกับ ci ที่ล็อกอินอยู่เท่านั้น
// --------------------------------------------------------------------------
void customerCreateOrder(int ci) {
    printHeader("สร้างออเดอร์พิมพ์งานใหม่ (ของฉัน)");
    cout << YELLOW << "  (พิมพ์ 0 แล้ว Enter ในช่องใดก็ได้ เพื่อยกเลิกและกลับเมนูก่อนหน้า)\n" << RESET;
    if (materialCount == 0) { cout << RED << "  ขณะนี้ยังไม่มีวัสดุในระบบ กรุณาติดต่อพนักงาน\n" << RESET; return; }
    createOrderCore(ci);
}

// แสดงเฉพาะออเดอร์ของลูกค้าที่ล็อกอินอยู่ พร้อมเวลาที่เหลือถ้ากำลังพิมพ์
void customerMyOrders(int ci) {
    autoCompletePrinting();
    printHeader("ออเดอร์ของฉัน (" + customers[ci].name + ")");
    bool any = false;
    for (int i = 0; i < orderCount; i++) {
        if (orders[i].customerCode != customers[ci].code) continue;
        any = true;
        cout << "  " << orders[i].code << "  ไฟล์: " << orders[i].fileName
             << "  ราคา: " << fixed << setprecision(2) << orders[i].price << " บาท"
             << "  สถานะ: " << orders[i].status << printingTimeLabel(orders[i])
             << (orders[i].paid ? (GREEN " [ชำระแล้ว-" + orders[i].paymentMethod + "]" RESET) : YELLOW " [ยังไม่ชำระ]" RESET)
             << "\n";
    }
    if (!any) cout << "  (คุณยังไม่มีออเดอร์)\n";
}

// ให้ลูกค้ายกเลิกออเดอร์ของตัวเองเท่านั้น (ตรวจสอบความเป็นเจ้าของก่อนเรียก cancelOrderCore)
void customerCancelOrder(int ci) {
    printHeader("ยกเลิกออเดอร์ของฉัน");
    string key = readLineTrim("  กรอกรหัสออเดอร์ที่ต้องการยกเลิก [0=ยกเลิก]: ");
    if (key == "0") { cout << YELLOW << "  ยกเลิกการดำเนินการ\n" << RESET; return; }
    int oi = findOrderIndex(key);
    if (oi == -1) { cout << RED << "  ไม่พบออเดอร์นี้\n" << RESET; return; }
    if (orders[oi].customerCode != customers[ci].code) {
        cout << RED << "  ออเดอร์นี้ไม่ใช่ของคุณ\n" << RESET;
        return;
    }
    if (orders[oi].status == "PickedUp" || orders[oi].status == "Shipped" || orders[oi].status == "Cancelled") {
        cout << RED << "  ออเดอร์นี้ไม่สามารถยกเลิกได้ (สถานะ: " << orders[oi].status << ")\n" << RESET;
        return;
    }
    if (orders[oi].paid) {
        cout << RED << "  ออเดอร์นี้ชำระเงินแล้ว ไม่สามารถยกเลิกได้ กรุณาติดต่อร้านเพื่อขอคืนเงิน\n" << RESET;
        return;
    }
    string conf = readLineTrim("  ยืนยันยกเลิกออเดอร์ " + orders[oi].code + "? (y/n): ");
    if (toUpperStr(conf) != "Y") { cout << YELLOW << "  ยกเลิกการดำเนินการ\n" << RESET; return; }
    cancelOrderCore(oi);
}

// เมนูหลักของโหมดลูกค้า (self-service kiosk) - เห็นเฉพาะฟังก์ชันที่เกี่ยวกับตัวเอง
// ไม่มีสิทธิ์เข้าถึงข้อมูลลูกค้าคนอื่น, จัดการวัสดุ/เครื่องพิมพ์, POS/เงินสด, หรือรายงาน
void customerKioskMenu() {
    clearScreen();
    int ci = customerSelfLookupOrRegister();
    if (ci == -1) return; // ผู้ใช้ยกเลิก กลับไปหน้าเลือกโหมด
    pause();
    while (true) {
        autoCompletePrinting();
        clearScreen();
        printHeader("ระบบสั่งพิมพ์งานสำหรับลูกค้า - สวัสดีคุณ " + customers[ci].name);
        cout << "  1. สร้างออเดอร์พิมพ์งานใหม่\n";
        cout << "  2. ดูสถานะออเดอร์ของฉัน\n";
        cout << "  3. ยกเลิกออเดอร์ของฉัน (เฉพาะที่ยังไม่ชำระเงิน/รับของ)\n";
        cout << "  4. ชำระเงินออนไลน์\n";
        cout << "  0. ออกจากระบบ (กลับเมนูเลือกโหมด)\n";
        int c = readIntInRange("\n  เลือกเมนู: ", 0, 4);
        clearScreen();
        if (c == 1) customerCreateOrder(ci);
        else if (c == 2) customerMyOrders(ci);
        else if (c == 3) customerCancelOrder(ci);
        else if (c == 4) customerPayOnline(ci);
        else if (c == 0) return;
        pause();
    }
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
        cout << "  6. ตรวจสอบคิวซ้ำด้วยตนเอง (ปกติจับคู่ให้อัตโนมัติอยู่แล้ว)\n";
        cout << "  7. แจ้งพิมพ์งานเสร็จก่อนเวลา (บังคับ Completed ด้วยตนเอง)\n";
        cout << "  8. ยืนยันส่งมอบสินค้า (รับที่ร้าน/จัดส่งแล้ว)\n";
        cout << "  9. ชำระเงินออนไลน์ (แทนลูกค้า)\n";
        cout << "  10. ดูสถานะแบบเรียลไทม์ (นับเวลาถอยหลังสด ๆ อัปเดตทุกวินาที)\n";
        cout << "  0. กลับเมนูหลัก\n";
        int c = readIntInRange("\n  เลือกเมนู: ", 0, 10);
        clearScreen();
        if (c == 1) listOrders();
        else if (c == 2) searchOrder();
        else if (c == 3) createOrder();
        else if (c == 4) cancelOrder();
        else if (c == 5) queueStatusView();
        else if (c == 6) processQueue();
        else if (c == 7) markOrderCompleted();
        else if (c == 8) markOrderDelivered();
        else if (c == 9) ownerPayOnline();
        else if (c == 10) { liveQueueMonitor(); continue; } // ออกมาแล้ว (กด Enter ไปแล้ว) ไม่ต้อง pause() ซ้ำ
        else if (c == 0) return;
        pause();
    }
}