#include "pos.h"
#include "globals.h"
#include "utils.h"
#include "search.h"
#include "storage.h"
#include "display.h"
#include <iostream>
#include <iomanip>
#include <ctime>

/* ==========================================================================
   13) POS -- CHECKOUT / RECEIPT
   ========================================================================== */
// เส้นขอบใบเสร็จ - ใช้ความกว้างเท่ากับ printLine() (66 ตัวอักษร ไม่มีเว้นวรรคนำหน้า)
// เพื่อให้ทุกเส้นในใบเสร็จกว้างเท่ากันพอดีตอนพิมพ์ออกเครื่องพิมพ์สลิป
void printReceiptBorder() {
    cout << "******************************************************************\n";
}

void printReceipt(Order &o, Customer &c, Material &m, const string &paymentMethod,
                   const string &fulfillment, double cash, double change) {
    time_t now = time(0);
    char buf[64];
    strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M", localtime(&now));
    bool shipping = (fulfillment == "Shipping");

    cout << "\n" << CYAN;
    cout << "     ****************************************\n";
    cout << "         ใบเสร็จรับเงิน - ร้าน 3D PRINTING\n";
    cout << "     ****************************************" << RESET << "\n";
    cout << "     วันที่       : " << buf << "\n";
    cout << "     เลขที่ออเดอร์ : " << o.code << "\n";
    cout << "     ลูกค้า       : " << c.name << " (" << c.phone << ")\n";
    cout << CYAN << "     ----------------------------------------\n" << RESET;
    cout << "     ไฟล์งาน      : " << o.fileName << "\n";
    cout << "     รายการ       : " << m.name << " สี " << m.color << "\n";
    cout << "     น้ำหนัก      : " << fixed << setprecision(1) << o.weight << " กรัม\n";
    cout << "     เวลาพิมพ์    : " << setprecision(2) << o.hours << " ชม.\n";
    cout << CYAN << "     ----------------------------------------\n" << RESET;
    cout << "     ยอดรวม       : " << setprecision(2) << o.price << " บาท\n";
    cout << "     ชำระโดย      : " << (paymentMethod == "Online" ? "ชำระเงินออนไลน์" : "เงินสด") << "\n";
    if (paymentMethod == "Online") {
        cout << GREEN << "     สถานะ        : ชำระเงินเรียบร้อยแล้ว" << RESET << "\n";
    } else {
        cout << "     รับเงินสด    : " << fixed << setprecision(2) << cash << " บาท\n";
        cout << GREEN << "     เงินทอน      : " << change << " บาท" << RESET << "\n";
    }
    cout << "     รับสินค้าโดย : " << (shipping ? "จัดส่ง" : "รับที่ร้าน") << "\n";
    if (shipping) {
        cout << "     ที่อยู่จัดส่ง : " << c.address << "\n";
    }
    cout << CYAN << "     ****************************************\n";
    cout << "            ขอบคุณที่ใช้บริการค่ะ/ครับ\n";
    cout << "     ****************************************\n" << RESET;
}

void posCheckout() {
    autoCompletePrinting();
    printHeader("POS - ชำระเงิน / ออกใบเสร็จ");
    cout << YELLOW << "  (พิมพ์ 0 แล้ว Enter ในช่องใดก็ได้ เพื่อยกเลิกและกลับเมนูก่อนหน้า)\n" << RESET;
    cout << BOLD << "  ออเดอร์ที่พร้อมชำระเงินสด (สถานะ Completed และยังไม่ได้ชำระเงิน):\n" << RESET;
    bool any = false;
    for (int i = 0; i < orderCount; i++) {
        if (orders[i].status == "Completed" && !orders[i].paid) {
            cout << "   - " << orders[i].code << "  ราคา: " << fixed << setprecision(2)
                 << orders[i].price << " บาท\n";
            any = true;
        }
    }
    if (!any) cout << YELLOW << "   (ยังไม่มีออเดอร์พร้อมชำระเงินสด)\n" << RESET;

    bool anyPrinting = false;
    for (int i = 0; i < orderCount; i++) {
        if (orders[i].status == "Printing") {
            if (!anyPrinting) { cout << "\n" << BOLD << "  ออเดอร์ที่กำลังพิมพ์อยู่ (ยังรับสินค้าไม่ได้):\n" << RESET; anyPrinting = true; }
            cout << "   - " << orders[i].code << YELLOW << printingTimeLabel(orders[i]) << RESET
                 << (orders[i].paid ? GREEN " [ชำระเงินออนไลน์แล้ว]" RESET : "") << "\n";
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
    if (orders[oi].paid) {
        cout << RED << "  ออเดอร์นี้ชำระเงินไปแล้ว (" << orders[oi].paymentMethod << ")\n" << RESET;
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

    orders[oi].paid = true;
    orders[oi].paymentMethod = "Cash";
    orders[oi].fulfillment = "Pickup"; // ชำระเงินสดถือว่ารับที่ร้านเสมอ
    saveOrders();

    printReceipt(orders[oi], customers[ci], materials[mi], "Cash", "Pickup", cash, change);

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
    rec.paymentMethod = "Cash";
    appendSalesHistory(rec);
}

/* --------------------------------------------------------------------------
   ชำระเงินออนไลน์ - ชำระได้ทุกช่วงสถานะที่ยังไม่ Cancelled/PickedUp/Shipped และยังไม่ได้ชำระ
   (แยกอิสระจากสถานะการพิมพ์ oorder.status เพื่อให้จ่ายได้ตั้งแต่ก่อนเริ่มพิมพ์จนถึงพิมพ์เสร็จแล้ว)
   -------------------------------------------------------------------------- */
void payOnlineCore(int oi) {
    if (orders[oi].paid) {
        cout << RED << "  ออเดอร์นี้ชำระเงินไปแล้ว (" << orders[oi].paymentMethod << ")\n" << RESET;
        return;
    }
    if (orders[oi].status == "Cancelled" || orders[oi].status == "PickedUp" || orders[oi].status == "Shipped") {
        cout << RED << "  ออเดอร์นี้ไม่สามารถชำระเงินได้ (สถานะ: " << orders[oi].status << ")\n" << RESET;
        return;
    }

    int ci = findCustomerIndex(orders[oi].customerCode);
    int mi = findMaterialIndex(orders[oi].materialCode);
    if (ci == -1 || mi == -1) { cout << RED << "  ข้อมูลลูกค้า/วัสดุไม่สมบูรณ์\n" << RESET; return; }

    cout << "  ออเดอร์: " << orders[oi].code << "  ยอดที่ต้องชำระ: " << fixed << setprecision(2)
         << orders[oi].price << " บาท\n";

    cout << "\n  เลือกวิธีรับสินค้า:\n";
    cout << "   1. รับที่ร้าน\n";
    cout << "   2. จัดส่ง\n";
    int fc = readIntInRange("  เลือก [0=ยกเลิก]: ", 0, 2);
    if (fc == 0) { cout << YELLOW << "  ยกเลิกการชำระเงิน\n" << RESET; return; }
    string fulfillment = (fc == 2) ? "Shipping" : "Pickup";

    if (fulfillment == "Shipping") {
        cout << "  ที่อยู่จัดส่งปัจจุบัน: "
             << (customers[ci].address.empty() ? "(ยังไม่มีข้อมูล)" : customers[ci].address) << "\n";
        string newAddr = readLineTrim("  กรอกที่อยู่จัดส่ง (Enter ว่าง = ใช้ที่อยู่เดิม, 0=ยกเลิก): ");
        if (newAddr == "0") { cout << YELLOW << "  ยกเลิกการชำระเงิน\n" << RESET; return; }
        if (!newAddr.empty()) {
            customers[ci].address = newAddr;
            saveCustomers();
        }
        if (customers[ci].address.empty()) {
            cout << RED << "  ต้องมีที่อยู่จัดส่งก่อนชำระเงินแบบจัดส่ง\n" << RESET;
            return;
        }
    }

    // จำลองขั้นตอนชำระเงินออนไลน์ (เช่น สแกน PromptPay QR) - ระบบจริงให้เชื่อมต่อ Payment Gateway ตรงนี้
    cout << YELLOW << "\n  กรุณาชำระเงินผ่านช่องทางออนไลน์\n" << RESET;
    string conf = readLineTrim("  ยืนยันการโอน? (y/n, หรือ 0=ยกเลิก): ");
    if (conf == "0" || toUpperStr(conf) != "Y") { cout << YELLOW << "  ยกเลิกการชำระเงิน\n" << RESET; return; }

    orders[oi].paid = true;
    orders[oi].paymentMethod = "Online";
    orders[oi].fulfillment = fulfillment;
    saveOrders();

    printReceipt(orders[oi], customers[ci], materials[mi], "Online", fulfillment, 0.0, 0.0);

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
    rec.cash = 0;
    rec.change = 0;
    rec.paymentMethod = "Online";
    appendSalesHistory(rec);

    cout << GREEN << "  ชำระเงินออนไลน์สำเร็จ (" << (fulfillment == "Shipping" ? "จัดส่ง" : "รับที่ร้าน") << ")\n" << RESET;
}

// เมนูฝั่งเจ้าของ/พนักงาน: เลือกออเดอร์ของลูกค้าคนใดก็ได้มาชำระเงินออนไลน์แทน
void ownerPayOnline() {
    autoCompletePrinting();
    printHeader("ชำระเงินออนไลน์ (เจ้าของ/พนักงาน)");
    cout << YELLOW << "  (พิมพ์ 0 แล้ว Enter ในช่องใดก็ได้ เพื่อยกเลิกและกลับเมนูก่อนหน้า)\n" << RESET;
    bool any = false;
    for (int i = 0; i < orderCount; i++) {
        if (!orders[i].paid && orders[i].status != "Cancelled" &&
            orders[i].status != "PickedUp" && orders[i].status != "Shipped") {
            cout << "   - " << orders[i].code << "  ลูกค้า: " << orders[i].customerCode
                 << "  ราคา: " << fixed << setprecision(2) << orders[i].price
                 << " บาท  สถานะ: " << orders[i].status << "\n";
            any = true;
        }
    }
    if (!any) { cout << YELLOW << "   (ไม่มีออเดอร์ที่รอชำระเงิน)\n" << RESET; return; }

    string key = readLineTrim("\n  กรอกรหัสออเดอร์ที่จะชำระเงินออนไลน์ [0=ยกเลิก]: ");
    if (key == "0") { cout << YELLOW << "  ยกเลิกการชำระเงิน\n" << RESET; return; }
    int oi = findOrderIndex(key);
    if (oi == -1) { cout << RED << "  ไม่พบออเดอร์นี้\n" << RESET; return; }
    payOnlineCore(oi);
}

// เมนูฝั่งลูกค้า (self-service): ชำระเงินออนไลน์ได้เฉพาะออเดอร์ของตัวเองเท่านั้น
void customerPayOnline(int ci) {
    autoCompletePrinting();
    printHeader("ชำระเงินออนไลน์สำหรับออเดอร์ของฉัน");
    cout << YELLOW << "  (พิมพ์ 0 แล้ว Enter ในช่องใดก็ได้ เพื่อยกเลิกและกลับเมนูก่อนหน้า)\n" << RESET;
    bool any = false;
    for (int i = 0; i < orderCount; i++) {
        if (orders[i].customerCode != customers[ci].code) continue;
        if (!orders[i].paid && orders[i].status != "Cancelled" &&
            orders[i].status != "PickedUp" && orders[i].status != "Shipped") {
            cout << "   - " << orders[i].code << "  ราคา: " << fixed << setprecision(2) << orders[i].price
                 << " บาท  สถานะ: " << orders[i].status << "\n";
            any = true;
        }
    }
    if (!any) { cout << YELLOW << "   (คุณไม่มีออเดอร์ที่รอชำระเงิน)\n" << RESET; return; }

    string key = readLineTrim("\n  กรอกรหัสออเดอร์ที่จะชำระเงินออนไลน์ [0=ยกเลิก]: ");
    if (key == "0") { cout << YELLOW << "  ยกเลิกการชำระเงิน\n" << RESET; return; }
    int oi = findOrderIndex(key);
    if (oi == -1) { cout << RED << "  ไม่พบออเดอร์นี้\n" << RESET; return; }
    if (orders[oi].customerCode != customers[ci].code) {
        cout << RED << "  ออเดอร์นี้ไม่ใช่ของคุณ\n" << RESET;
        return;
    }
    payOnlineCore(oi);
}
