#ifndef ORDER_H
#define ORDER_H

/* ============================================================================
   order.h
   จัดการออเดอร์งานพิมพ์ / คิวงาน (ฝั่งเจ้าของ/พนักงาน และฝั่งลูกค้า self-service)
   สถานะ: Queued -> Printing -> Completed -> PickedUp / Shipped (หรือ Cancelled)
   การชำระเงิน (paid/paymentMethod/fulfillment) แยกอิสระจากสถานะข้างบน
   ชำระออนไลน์ได้ทุกช่วงสถานะ หักวัสดุออกจาก Stock "เมื่อเริ่มพิมพ์จริง" เท่านั้น
   ============================================================================ */

/* ---- ฝั่งเจ้าของ/พนักงาน ---- */
void createOrder();
void searchOrder();
void processQueue();          // จับคู่ออเดอร์ที่รอ (Queued) กับเครื่องว่าง
void markOrderCompleted();    // บังคับ Printing -> Completed ด้วยตนเอง (พิมพ์เสร็จก่อนเวลา)
void markOrderDelivered();    // Completed -> PickedUp / Shipped
void cancelOrder();
void queueStatusView();
void orderMenu();

/* ---- ตรรกะหลักที่ใช้ร่วมกันทั้งสองฝั่ง ---- */
void createOrderCore(int ci); // สร้างออเดอร์เมื่อทราบลูกค้า (ci) แล้ว
void cancelOrderCore(int oi); // ยกเลิกออเดอร์เมื่อทราบ index (oi) และผ่านการยืนยันแล้ว

/* ---- ฝั่งลูกค้า (self-service kiosk) ---- */
void customerCreateOrder(int ci);
void customerMyOrders(int ci);
void customerCancelOrder(int ci);
void customerKioskMenu();

#endif // ORDER_H
