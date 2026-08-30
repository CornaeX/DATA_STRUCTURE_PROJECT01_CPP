#include "report.h"
#include "globals.h"
#include "utils.h"
#include "display.h"
#include <iostream>
#include <iomanip>

/* ==========================================================================
   14) REPORTS -- ประวัติการขาย
   ========================================================================== */
void showSalesHistory() {
    printHeader("ประวัติการขาย (Sales History)");
    if (salesCount == 0) { cout << "  (ยังไม่มีประวัติการขาย)\n"; return; }
    double total = 0;
    cout << padRight("วันที่", 17) << padRight("ออเดอร์", 8) << padRight("ลูกค้า", 16)
         << padRight("วัสดุ", 10) << padRight("สี", 8) << padRight("ชำระโดย", 10) << "ยอด\n";
    printLine();
    for (int i = 0; i < salesCount; i++) {
        cout << padRight(salesHistory[i].date, 17) << padRight(salesHistory[i].orderCode, 8)
             << padRight(salesHistory[i].customerName, 16) << padRight(salesHistory[i].materialName, 10)
             << padRight(salesHistory[i].color, 8) << padRight(salesHistory[i].paymentMethod, 10)
             << fixed << setprecision(2) << salesHistory[i].price << "\n";
        total += salesHistory[i].price;
    }
    printLine();
    cout << GREEN << BOLD << "  จำนวนบิลทั้งหมด: " << salesCount << "  ยอดขายรวม: "
         << fixed << setprecision(2) << total << " บาท" << RESET << "\n";
}

void reportMenu() {
    while (true) {
        clearScreen();
        printHeader("รายงาน");
        printMenuOption(1, "ประวัติการขาย (Sales History)");
        printMenuOption(2, "สรุปสต็อกวัสดุ");
        printMenuOption(0, "กลับเมนูหลัก", RED);
        int c = readIntInRange("\n  เลือกเมนู: ", 0, 2);
        clearScreen();
        if (c == 1) showSalesHistory();
        else if (c == 2) listMaterials();
        else if (c == 0) return;
        pause();
    }
}
