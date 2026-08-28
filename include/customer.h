#ifndef CUSTOMER_H
#define CUSTOMER_H

/* ============================================================================
   customer.h
   จัดการข้อมูลลูกค้า: เพิ่ม/ค้นหา/ลบ (ฝั่งเจ้าของ/พนักงาน) และการเข้าสู่ระบบ/สมัคร
   สมาชิกด้วยเบอร์โทรด้วยตนเอง (ฝั่งลูกค้า self-service)
   ============================================================================ */

void insertCustomer();
void searchCustomer();
void deleteCustomer();
void customerMenu();

// ให้ลูกค้าค้นหาบัญชีตัวเองด้วยเบอร์โทรในโหมด self-service ถ้าไม่พบ ให้เสนอสมัครสมาชิกใหม่ทันที
// คืนค่า index ใน customers[] ของบัญชีที่ใช้งาน หรือ -1 ถ้าผู้ใช้ยกเลิก
int customerSelfLookupOrRegister();

#endif // CUSTOMER_H
