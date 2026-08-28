#ifndef DISPLAY_H
#define DISPLAY_H

/* ============================================================================
   display.h
   แสดงรายการข้อมูล (list) ทั้งหมดของลูกค้า/วัสดุ/เครื่องพิมพ์/ออเดอร์ และ
   ฟังก์ชันเกี่ยวกับเวลาการพิมพ์ (คำนวณเวลาที่เหลือ, ปรับสถานะ Printing -> Completed
   อัตโนมัติเมื่อครบเวลาประมาณการ)
   ============================================================================ */

#include "types.h"
#include <string>
using namespace std;

/* ---- list ---- */
void listCustomers();
void listMaterials();
void listPrinters();
void listOrders();

/* ---- print timing helpers ---- */
string formatDuration(double hours);         // "Xชม Yนาที"
double elapsedHours(const Order &o);          // เวลาที่พิมพ์ไปแล้ว
double remainingHours(const Order &o);        // เวลาที่เหลือโดยประมาณ
string printingTimeLabel(const Order &o);     // ข้อความสรุปเวลาสำหรับสถานะ Printing

// ตรวจสอบออเดอร์ Printing ทุกตัว ปรับเป็น Completed อัตโนมัติเมื่อครบเวลาประมาณการ
void autoCompletePrinting();

#endif // DISPLAY_H
