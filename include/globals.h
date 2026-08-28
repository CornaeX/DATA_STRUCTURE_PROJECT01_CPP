#ifndef GLOBALS_H
#define GLOBALS_H

/* ============================================================================
   globals.h
   Array หลักของระบบ + ตัวนับจำนวน/รหัสถัดไป (นิยามจริงอยู่ใน globals.cpp)
   ใช้ extern ล้วน เพราะทั้งโปรแกรมเก็บข้อมูลด้วย Array ตามข้อกำหนดของโปรเจกต์
   (ไม่ใช้ vector/STL container)
   ============================================================================ */

#include "types.h"

/* ==========================================================================
   2) GLOBAL ARRAYS + COUNTERS
   ========================================================================== */
extern Customer customers[MAX_CUSTOMERS];
extern int customerCount;
extern int nextCustomerId;

extern Material materials[MAX_MATERIALS];
extern int materialCount;
extern int nextMaterialId;

extern Printer printers[MAX_PRINTERS];
extern int printerCount;
extern int nextPrinterId;

extern Order orders[MAX_ORDERS];
extern int orderCount;
extern int nextOrderId;

extern SalesRecord salesHistory[MAX_SALES];
extern int salesCount;

#endif // GLOBALS_H
