#include "storage.h"
#include "globals.h"
#include "json_util.h"
#include "utils.h"
#include <iostream>
#include <fstream>
#include <iomanip>

/* ==========================================================================
   5) LOAD (Create) FUNCTIONS  -- อ่านจาก JSON เก็บลง Array
   ========================================================================== */
void loadCustomers() {
    customerCount = 0;
    string content = readFileToString(F_CUSTOMERS);
    if (content.empty()) return;
    string objs[MAX_CUSTOMERS]; int n;
    splitJsonObjects(content, objs, n, MAX_CUSTOMERS);
    for (int i = 0; i < n; i++) {
        customers[customerCount].code = jsonGetString(objs[i], "code");
        customers[customerCount].name = jsonGetString(objs[i], "name");
        customers[customerCount].phone = jsonGetString(objs[i], "phone");
        customers[customerCount].address = jsonGetString(objs[i], "address");
        int num = extractNumber(customers[customerCount].code);
        if (num + 1 > nextCustomerId) nextCustomerId = num + 1;
        customerCount++;
    }
}

void loadMaterials() {
    materialCount = 0;
    string content = readFileToString(F_MATERIALS);
    if (content.empty()) return;
    string objs[MAX_MATERIALS]; int n;
    splitJsonObjects(content, objs, n, MAX_MATERIALS);
    for (int i = 0; i < n; i++) {
        materials[materialCount].code = jsonGetString(objs[i], "code");
        materials[materialCount].name = jsonGetString(objs[i], "name");
        materials[materialCount].color = jsonGetString(objs[i], "color");
        materials[materialCount].pricePerGram = jsonGetNumber(objs[i], "pricePerGram");
        materials[materialCount].stockGram = jsonGetNumber(objs[i], "stockGram");
        int num = extractNumber(materials[materialCount].code);
        if (num + 1 > nextMaterialId) nextMaterialId = num + 1;
        materialCount++;
    }
}

void loadPrinters() {
    printerCount = 0;
    string content = readFileToString(F_PRINTERS);
    if (content.empty()) return;
    string objs[MAX_PRINTERS]; int n;
    splitJsonObjects(content, objs, n, MAX_PRINTERS);
    for (int i = 0; i < n; i++) {
        printers[printerCount].code = jsonGetString(objs[i], "code");
        printers[printerCount].name = jsonGetString(objs[i], "name");
        printers[printerCount].type = jsonGetString(objs[i], "type");
        printers[printerCount].status = jsonGetString(objs[i], "status");
        printers[printerCount].currentOrder = jsonGetString(objs[i], "currentOrder");
        int num = extractNumber(printers[printerCount].code);
        if (num + 1 > nextPrinterId) nextPrinterId = num + 1;
        printerCount++;
    }
}

void loadOrders() {
    orderCount = 0;
    string content = readFileToString(F_ORDERS);
    if (content.empty()) return;
    string objs[MAX_ORDERS]; int n;
    splitJsonObjects(content, objs, n, MAX_ORDERS);
    for (int i = 0; i < n; i++) {
        orders[orderCount].code = jsonGetString(objs[i], "code");
        orders[orderCount].customerCode = jsonGetString(objs[i], "customerCode");
        orders[orderCount].materialCode = jsonGetString(objs[i], "materialCode");
        orders[orderCount].printerCode = jsonGetString(objs[i], "printerCode");
        orders[orderCount].fileName = jsonGetString(objs[i], "fileName");
        orders[orderCount].weight = jsonGetNumber(objs[i], "weight");
        orders[orderCount].hours = jsonGetNumber(objs[i], "hours");
        orders[orderCount].price = jsonGetNumber(objs[i], "price");
        orders[orderCount].status = jsonGetString(objs[i], "status");
        orders[orderCount].stockDeducted = jsonGetBool(objs[i], "stockDeducted");
        orders[orderCount].startTime = (time_t) jsonGetNumber(objs[i], "startTime");
        orders[orderCount].paid = jsonGetBool(objs[i], "paid");
        orders[orderCount].paymentMethod = jsonGetString(objs[i], "paymentMethod");
        orders[orderCount].fulfillment = jsonGetString(objs[i], "fulfillment");
        // migration: ไฟล์ข้อมูลเก่าเคยใช้ status = "Paid" แทนการชำระเงิน (ก่อนแยก field paid ออกมา)
        if (orders[orderCount].status == "Paid") {
            orders[orderCount].status = "Completed";
            orders[orderCount].paid = true;
            if (orders[orderCount].paymentMethod.empty()) orders[orderCount].paymentMethod = "Cash";
            if (orders[orderCount].fulfillment.empty()) orders[orderCount].fulfillment = "Pickup";
        } else if (orders[orderCount].status == "PickedUp" && orders[orderCount].paymentMethod.empty()) {
            orders[orderCount].paid = true;
            orders[orderCount].paymentMethod = "Cash";
            orders[orderCount].fulfillment = "Pickup";
        }
        int num = extractNumber(orders[orderCount].code);
        if (num + 1 > nextOrderId) nextOrderId = num + 1;
        orderCount++;
    }
}

void loadSalesHistory() {
    salesCount = 0;
    string content = readFileToString(F_SALES);
    if (content.empty()) return;
    string objs[MAX_SALES]; int n;
    splitJsonObjects(content, objs, n, MAX_SALES);
    for (int i = 0; i < n; i++) {
        salesHistory[salesCount].date = jsonGetString(objs[i], "date");
        salesHistory[salesCount].orderCode = jsonGetString(objs[i], "orderCode");
        salesHistory[salesCount].customerName = jsonGetString(objs[i], "customerName");
        salesHistory[salesCount].materialName = jsonGetString(objs[i], "materialName");
        salesHistory[salesCount].color = jsonGetString(objs[i], "color");
        salesHistory[salesCount].price = jsonGetNumber(objs[i], "price");
        salesHistory[salesCount].cash = jsonGetNumber(objs[i], "cash");
        salesHistory[salesCount].change = jsonGetNumber(objs[i], "change");
        salesHistory[salesCount].paymentMethod = jsonGetString(objs[i], "paymentMethod");
        if (salesHistory[salesCount].paymentMethod.empty()) salesHistory[salesCount].paymentMethod = "Cash";
        salesCount++;
    }
}

/* ==========================================================================
   6) SAVE FUNCTIONS -- บันทึกกลับลง JSON
   ========================================================================== */
void saveCustomers() {
    ofstream fout(F_CUSTOMERS.c_str());
    fout << "[\n";
    for (int i = 0; i < customerCount; i++) {
        fout << "  {\n";
        fout << "    \"code\": \"" << jsonEscape(customers[i].code) << "\",\n";
        fout << "    \"name\": \"" << jsonEscape(customers[i].name) << "\",\n";
        fout << "    \"phone\": \"" << jsonEscape(customers[i].phone) << "\",\n";
        fout << "    \"address\": \"" << jsonEscape(customers[i].address) << "\"\n";
        fout << "  }" << (i < customerCount - 1 ? "," : "") << "\n";
    }
    fout << "]\n";
    fout.close();
}

void saveMaterials() {
    ofstream fout(F_MATERIALS.c_str());
    fout << "[\n";
    for (int i = 0; i < materialCount; i++) {
        fout << "  {\n";
        fout << "    \"code\": \"" << jsonEscape(materials[i].code) << "\",\n";
        fout << "    \"name\": \"" << jsonEscape(materials[i].name) << "\",\n";
        fout << "    \"color\": \"" << jsonEscape(materials[i].color) << "\",\n";
        fout << fixed << setprecision(2);
        fout << "    \"pricePerGram\": " << materials[i].pricePerGram << ",\n";
        fout << "    \"stockGram\": " << materials[i].stockGram << "\n";
        fout << "  }" << (i < materialCount - 1 ? "," : "") << "\n";
    }
    fout << "]\n";
    fout.close();
}

void savePrinters() {
    ofstream fout(F_PRINTERS.c_str());
    fout << "[\n";
    for (int i = 0; i < printerCount; i++) {
        fout << "  {\n";
        fout << "    \"code\": \"" << jsonEscape(printers[i].code) << "\",\n";
        fout << "    \"name\": \"" << jsonEscape(printers[i].name) << "\",\n";
        fout << "    \"type\": \"" << jsonEscape(printers[i].type) << "\",\n";
        fout << "    \"status\": \"" << jsonEscape(printers[i].status) << "\",\n";
        fout << "    \"currentOrder\": \"" << jsonEscape(printers[i].currentOrder) << "\"\n";
        fout << "  }" << (i < printerCount - 1 ? "," : "") << "\n";
    }
    fout << "]\n";
    fout.close();
}

void saveOrders() {
    ofstream fout(F_ORDERS.c_str());
    fout << "[\n";
    for (int i = 0; i < orderCount; i++) {
        fout << "  {\n";
        fout << "    \"code\": \"" << jsonEscape(orders[i].code) << "\",\n";
        fout << "    \"customerCode\": \"" << jsonEscape(orders[i].customerCode) << "\",\n";
        fout << "    \"materialCode\": \"" << jsonEscape(orders[i].materialCode) << "\",\n";
        fout << "    \"printerCode\": \"" << jsonEscape(orders[i].printerCode) << "\",\n";
        fout << "    \"fileName\": \"" << jsonEscape(orders[i].fileName) << "\",\n";
        fout << fixed << setprecision(2);
        fout << "    \"weight\": " << orders[i].weight << ",\n";
        fout << "    \"hours\": " << orders[i].hours << ",\n";
        fout << "    \"price\": " << orders[i].price << ",\n";
        fout << "    \"status\": \"" << jsonEscape(orders[i].status) << "\",\n";
        fout << "    \"stockDeducted\": " << (orders[i].stockDeducted ? "true" : "false") << ",\n";
        fout << "    \"startTime\": " << (long) orders[i].startTime << ",\n";
        fout << "    \"paid\": " << (orders[i].paid ? "true" : "false") << ",\n";
        fout << "    \"paymentMethod\": \"" << jsonEscape(orders[i].paymentMethod) << "\",\n";
        fout << "    \"fulfillment\": \"" << jsonEscape(orders[i].fulfillment) << "\"\n";
        fout << "  }" << (i < orderCount - 1 ? "," : "") << "\n";
    }
    fout << "]\n";
    fout.close();
}

void saveSalesHistory() {
    ofstream fout(F_SALES.c_str());
    fout << "[\n";
    for (int i = 0; i < salesCount; i++) {
        fout << "  {\n";
        fout << "    \"date\": \"" << jsonEscape(salesHistory[i].date) << "\",\n";
        fout << "    \"orderCode\": \"" << jsonEscape(salesHistory[i].orderCode) << "\",\n";
        fout << "    \"customerName\": \"" << jsonEscape(salesHistory[i].customerName) << "\",\n";
        fout << "    \"materialName\": \"" << jsonEscape(salesHistory[i].materialName) << "\",\n";
        fout << "    \"color\": \"" << jsonEscape(salesHistory[i].color) << "\",\n";
        fout << fixed << setprecision(2);
        fout << "    \"price\": " << salesHistory[i].price << ",\n";
        fout << "    \"cash\": " << salesHistory[i].cash << ",\n";
        fout << "    \"change\": " << salesHistory[i].change << ",\n";
        fout << "    \"paymentMethod\": \"" << jsonEscape(salesHistory[i].paymentMethod) << "\"\n";
        fout << "  }" << (i < salesCount - 1 ? "," : "") << "\n";
    }
    fout << "]\n";
    fout.close();
}

void saveAll() {
    saveCustomers();
    saveMaterials();
    savePrinters();
    saveOrders();
    saveSalesHistory();
    cout << GREEN << "  บันทึกข้อมูลทั้งหมดลงไฟล์ JSON เรียบร้อยแล้ว\n" << RESET;
}

void appendSalesHistory(const SalesRecord &r) {
    if (salesCount < MAX_SALES) {
        salesHistory[salesCount++] = r;
        saveSalesHistory();
    }
}
