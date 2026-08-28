#ifndef TYPES_H
#define TYPES_H

/* ============================================================================
   types.h
   ค่าคงที่ของระบบ (ขนาด array, ราคา, ชื่อไฟล์, รหัสสี ANSI) และโครงสร้างข้อมูลหลัก
   (Customer / Material / Printer / Order / SalesRecord) ที่ใช้ร่วมกันทุกโมดูล
   ============================================================================ */

#include <string>
#include <ctime>
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

// รหัสผ่านสำหรับเข้าโหมดเจ้าของร้าน/พนักงาน (แก้ไขค่านี้เพื่อเปลี่ยนรหัสผ่าน)
const string OWNER_PASSWORD = "1234";

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
    string status;    // Queued / Printing / Completed / PickedUp / Shipped / Cancelled
                       // (สถานะนี้ติดตามความคืบหน้าการพิมพ์/ส่งมอบเท่านั้น แยกจากการชำระเงิน)
    bool stockDeducted; // true เมื่อหักสต็อกไปแล้ว (ตอนเริ่มพิมพ์จริง) ใช้ตัดสินใจตอนคืนสต็อก
    time_t startTime;  // เวลาที่เริ่มพิมพ์จริง (Unix timestamp) ใช้คำนวณเวลาที่เหลือ / เช็คว่าพิมพ์เสร็จหรือยัง (0 = ยังไม่เริ่ม)
    bool paid;             // ชำระเงินแล้วหรือยัง (แยกจาก status เพื่อให้ชำระเงินออนไลน์ได้ทุกช่วงสถานะ)
    string paymentMethod;  // "" / "Cash" / "Online"
    string fulfillment;    // "" / "Pickup" / "Shipping" (มีความหมายเฉพาะออเดอร์ที่ชำระออนไลน์ ส่วนเงินสดถือเป็น Pickup เสมอ)
};

struct SalesRecord {
    string date, orderCode, customerName, materialName, color;
    double price, cash, change;
    string paymentMethod; // "Cash" / "Online"
};

#endif // TYPES_H
