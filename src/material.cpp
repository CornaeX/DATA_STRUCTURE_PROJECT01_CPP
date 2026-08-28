#include "material.h"
#include "globals.h"
#include "utils.h"
#include "search.h"
#include "storage.h"
#include "display.h"
#include <iostream>
#include <iomanip>

/* ==========================================================================
   10) MATERIAL MANAGEMENT
   ========================================================================== */
void insertMaterial() {
    printHeader("เพิ่มวัสดุใหม่");
    cout << YELLOW << "  (พิมพ์ 0 แล้ว Enter ในช่องใดก็ได้ เพื่อยกเลิกและกลับเมนูก่อนหน้า)\n" << RESET;
    if (materialCount >= MAX_MATERIALS) { cout << RED << "  ข้อมูลวัสดุเต็มแล้ว\n" << RESET; return; }

    Material m;

    string name = readLineTrim("  ชื่อวัสดุ (เช่น PLA, ABS, PETG) [0=ยกเลิก]: ");
    if (name == "0") { cout << YELLOW << "  ยกเลิกการเพิ่มวัสดุ\n" << RESET; return; }
    m.name = name;

    string color = readLineTrim("  สี (เช่น Red, Black, White) [0=ยกเลิก]: ");
    if (color == "0") { cout << YELLOW << "  ยกเลิกการเพิ่มวัสดุ\n" << RESET; return; }
    m.color = color;

    double price;
    if (!readPositiveDoubleCancelable("  ราคาต่อกรัม (บาท) [0=ยกเลิก]: ", price)) {
        cout << YELLOW << "  ยกเลิกการเพิ่มวัสดุ\n" << RESET; return;
    }
    m.pricePerGram = price;

    double stock;
    if (!readPositiveDoubleCancelable("  จำนวนคงเหลือ (กรัม) [0=ยกเลิก]: ", stock)) {
        cout << YELLOW << "  ยกเลิกการเพิ่มวัสดุ\n" << RESET; return;
    }
    m.stockGram = stock;

    m.code = genCode("M", nextMaterialId++);
    materials[materialCount++] = m;
    saveMaterials();
    cout << GREEN << "  เพิ่มวัสดุสำเร็จ รหัส: " << m.code << RESET << "\n";
}

void searchMaterial() {
    printHeader("ค้นหาวัสดุ (รหัสหรือชื่อ)");
    string key = readLineTrim("  กรอกรหัสหรือชื่อ: ");
    bool found = false;
    cout << padRight("รหัส", 8) << padRight("ชื่อวัสดุ", 14) << padRight("สี", 10)
         << padRight("ราคา/กรัม", 14) << "คงเหลือ(กรัม)\n";
    printLine();
    for (int i = 0; i < materialCount; i++) {
        if (toUpperStr(materials[i].code) == toUpperStr(key) || containsIgnoreCase(materials[i].name, key)
            || containsIgnoreCase(materials[i].color, key)) {
            cout << padRight(materials[i].code, 8) << padRight(materials[i].name, 14)
                 << padRight(materials[i].color, 10) << setw(14) << left << fixed << setprecision(2)
                 << materials[i].pricePerGram << materials[i].stockGram << "\n";
            found = true;
        }
    }
    if (!found) cout << RED << "  ไม่พบข้อมูลวัสดุ\n" << RESET;
}

void deleteMaterial() {
    printHeader("ลบวัสดุ");
    string key = readLineTrim("  กรอกรหัสวัสดุที่ต้องการลบ [0=ยกเลิก]: ");
    if (key == "0") { cout << YELLOW << "  ยกเลิกการลบ\n" << RESET; return; }
    int idx = findMaterialIndex(key);
    if (idx == -1) { cout << RED << "  ไม่พบรหัสวัสดุนี้\n" << RESET; return; }
    cout << "  วัสดุ: " << materials[idx].name << " สี " << materials[idx].color << "\n";
    string conf = readLineTrim("  ยืนยันการลบ? (y/n): ");
    if (toUpperStr(conf) != "Y") { cout << YELLOW << "  ยกเลิกการลบ\n" << RESET; return; }
    for (int i = idx; i < materialCount - 1; i++) materials[i] = materials[i + 1];
    materialCount--;
    saveMaterials();
    cout << GREEN << "  ลบวัสดุสำเร็จ\n" << RESET;
}

void materialMenu() {
    while (true) {
        clearScreen();
        printHeader("จัดการข้อมูลวัสดุ");
        cout << "  1. แสดงรายการวัสดุทั้งหมด\n";
        cout << "  2. ค้นหาวัสดุ\n";
        cout << "  3. เพิ่มวัสดุใหม่\n";
        cout << "  4. ลบวัสดุ\n";
        cout << "  0. กลับเมนูหลัก\n";
        int c = readIntInRange("\n  เลือกเมนู: ", 0, 4);
        clearScreen();
        if (c == 1) listMaterials();
        else if (c == 2) searchMaterial();
        else if (c == 3) insertMaterial();
        else if (c == 4) deleteMaterial();
        else if (c == 0) return;
        pause();
    }
}
