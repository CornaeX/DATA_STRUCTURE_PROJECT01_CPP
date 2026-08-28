#ifndef SEARCH_H
#define SEARCH_H

/* ============================================================================
   search.h
   ค้นหา index ของข้อมูลใน Array ด้วยรหัส (หรือเบอร์โทรสำหรับลูกค้า)
   คืนค่า index หรือ -1 ถ้าไม่พบ
   ============================================================================ */

#include <string>
using namespace std;

int findCustomerIndex(const string &key);
int findCustomerIndexByPhone(const string &phone); // ใช้ให้ลูกค้าค้นหาบัญชีตัวเองในโหมด self-service
int findMaterialIndex(const string &key);
int findPrinterIndex(const string &key);
int findOrderIndex(const string &key);

#endif // SEARCH_H
