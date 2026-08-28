#ifndef POS_H
#define POS_H

/* ============================================================================
   pos.h
   POS - ชำระเงิน / ออกใบเสร็จ (เงินสดหน้าร้าน และชำระเงินออนไลน์ที่จ่ายได้ทุกช่วง
   สถานะออเดอร์ ยกเว้น Cancelled/PickedUp/Shipped)
   ============================================================================ */

#include "types.h"
#include <string>
using namespace std;

void printReceiptBorder();
void printReceipt(Order &o, Customer &c, Material &m, const string &paymentMethod,
                   const string &fulfillment, double cash, double change);

void posCheckout();          // ชำระเงินสดหน้าร้าน
void payOnlineCore(int oi);  // ตรรกะหลักชำระเงินออนไลน์ เมื่อทราบ index ออเดอร์แล้ว
void ownerPayOnline();       // เมนูฝั่งเจ้าของ/พนักงาน: ชำระเงินออนไลน์แทนลูกค้าคนใดก็ได้
void customerPayOnline(int ci); // เมนูฝั่งลูกค้า: ชำระเงินออนไลน์เฉพาะออเดอร์ของตัวเอง

#endif // POS_H
