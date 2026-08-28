#include "search.h"
#include "globals.h"
#include "utils.h"

/* ==========================================================================
   7) SEARCH (return index หรือ -1)
   ========================================================================== */
int findCustomerIndex(const string &key) {
    for (int i = 0; i < customerCount; i++)
        if (toUpperStr(customers[i].code) == toUpperStr(key)) return i;
    return -1;
}
// ค้นหาลูกค้าด้วยเบอร์โทร (ใช้สำหรับให้ลูกค้าค้นหาบัญชีตัวเองในโหมด self-service)
int findCustomerIndexByPhone(const string &phone) {
    for (int i = 0; i < customerCount; i++)
        if (customers[i].phone == phone) return i;
    return -1;
}
int findMaterialIndex(const string &key) {
    for (int i = 0; i < materialCount; i++)
        if (toUpperStr(materials[i].code) == toUpperStr(key)) return i;
    return -1;
}
int findPrinterIndex(const string &key) {
    for (int i = 0; i < printerCount; i++)
        if (toUpperStr(printers[i].code) == toUpperStr(key)) return i;
    return -1;
}
int findOrderIndex(const string &key) {
    for (int i = 0; i < orderCount; i++)
        if (toUpperStr(orders[i].code) == toUpperStr(key)) return i;
    return -1;
}
