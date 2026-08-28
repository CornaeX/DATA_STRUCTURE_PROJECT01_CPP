#ifndef STORAGE_H
#define STORAGE_H

/* ============================================================================
   storage.h
   โหลด (Create) ข้อมูลจากไฟล์ .json เข้าสู่ Array ตอนเริ่มโปรแกรม และบันทึกข้อมูล
   กลับลงไฟล์ .json ทุกครั้งที่มีการแก้ไข
   ============================================================================ */

#include "types.h"

/* ---- load: อ่านจาก JSON เก็บลง Array ---- */
void loadCustomers();
void loadMaterials();
void loadPrinters();
void loadOrders();
void loadSalesHistory();

/* ---- save: บันทึกกลับลง JSON ---- */
void saveCustomers();
void saveMaterials();
void savePrinters();
void saveOrders();
void saveSalesHistory();
void saveAll();

void appendSalesHistory(const SalesRecord &r); // บันทึก record ใหม่ + เขียนไฟล์ทันที

#endif // STORAGE_H
