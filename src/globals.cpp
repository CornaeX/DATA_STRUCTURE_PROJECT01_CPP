#include "globals.h"

/* ==========================================================================
   2) GLOBAL ARRAYS + COUNTERS
   ========================================================================== */
Customer customers[MAX_CUSTOMERS];
int customerCount = 0;
int nextCustomerId = 1;

Material materials[MAX_MATERIALS];
int materialCount = 0;
int nextMaterialId = 1;

Printer printers[MAX_PRINTERS];
int printerCount = 0;
int nextPrinterId = 1;

Order orders[MAX_ORDERS];
int orderCount = 0;
int nextOrderId = 1;

SalesRecord salesHistory[MAX_SALES];
int salesCount = 0;
