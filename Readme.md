# 🖨️ 3D Printing Shop Management System

A console-based (TUI) point-of-sale and shop management system for a 3D printing service, written in C++. It manages customers, materials, printers, print orders, job queues, payments/receipts, and sales reports — all persisted to local JSON files, with **zero external dependencies** (no STL containers, no third-party JSON library).

Built as a Data Structures course project, this system intentionally uses **plain C-style arrays** instead of `std::vector` or other STL containers to demonstrate manual data structure management, linear search, and array-based CRUD operations.

## Features

**Customer Mode (self-service kiosk)**
- Customer login (by phone lookup) or new account creation
- Place a print order (choose material, upload file name, enter weight)
- View "My Orders" and their live status
- Cancel a pending order
- Pay online for an order

**Owner / Staff Mode** (password protected)
- **Customers** — add, search, list, delete
- **Materials** — add, search, list, delete (tracks color, price/gram, stock in grams)
- **Printers** — add, search, list, delete, change status (Idle / Printing / Maintenance)
- **Orders & Job Queue**
  - Create orders, auto-estimate print time & price
  - Match queued orders to available printers
  - Auto-complete printing based on elapsed time (or force-complete manually)
  - Confirm delivery (pickup / shipping)
  - Cancel orders (with automatic stock refund if already deducted)
- **POS (Point of Sale)** — checkout, cash or online payment, printed receipt
- **Reports** — sales history, revenue summaries
- Manual "Save All" plus autosave on exit

## Order Lifecycle

```
Queued → Printing → Completed → PickedUp / Shipped
                                      ↘ Cancelled (from Queued/Printing)
```

Payment status (`paid`, `paymentMethod`: Cash/Online) is tracked independently of order status, so an order can be paid online at any stage before pickup.

## Full Feature Reference (Every Menu & Option)

This section documents **every screen, menu number, and behavior** in the program, end to end — exactly what happens for each choice.

### 0. Startup

On launch, the program loads `customers.json`, `materials.json`, `printers.json`, `orders.json`, and `sales_history.json` into memory (arrays). If no printers exist yet, it auto-seeds two sample FDM printers (`Ender-3 V2`, `Prusa MK3S`) so the workflow can be tested immediately. Every time a menu is (re)entered, the program also silently runs the **auto-complete check** (see §5.7) before drawing the screen, so printing jobs that have finished their estimated time flip to `Completed` automatically, with no key press needed.

### 1. Mode Select Menu (first screen)

- **[1] Customer mode** — enters the self-service kiosk (§2).
- **[2] Owner/Staff mode** — prompts for the owner password (§3) before showing the staff menu.
- **[0] Exit program** — saves all data to disk and closes.

### 2. Customer Mode (self-service kiosk)

**Login/registration gate (always shown first):**
- Enter a phone number. If it matches an existing customer, you're logged in immediately.
- If it's not found, the system asks "Create a new account?" (y/n). Answering `y` walks you through name and address, generates a new customer code, saves it, and logs you in. Answering anything else lets you re-enter a phone number, or type `0` to cancel back to the mode-select screen.

**Customer kiosk menu**, once logged in:
- **[1] Create a new print order** — same core flow as staff order creation (§5.1), but the customer is fixed to their own account (they can't order for someone else). You'll be asked for: file name → material/color → weight in grams. The system checks material stock, computes estimated time and price, shows a summary, and asks for confirmation before creating the order.
- **[2] View my orders** — lists only this customer's orders with file name, price, current status, and (if printing) a live "time remaining" label, plus a `[Paid]`/`[Unpaid]` tag.
- **[3] Cancel my order** — cancels one of your own orders by code. Blocked if the order is already `PickedUp`, `Shipped`, or `Cancelled`, or if it has already been paid (in that case you must contact the shop for a refund). If the order had already started printing, the deducted material stock is refunded and the printer is freed.
- **[4] Pay online** — pay for any of your own unpaid orders that aren't cancelled/picked-up/shipped yet (i.e., you can pay before or during printing, not just after). Asks how you'll receive the item — **1. Pickup** or **2. Shipping** (shipping requires/updates a delivery address) — then simulates an online payment confirmation and prints a receipt.
- **[0] Log out** — returns to the mode-select screen.

### 3. Owner/Staff Login

Password-protected (default password `1234`, stored in `include/types.h` as `OWNER_PASSWORD`). You get **3 attempts**; entering `0` at any point cancels back to the mode-select screen. After 3 failed attempts, access is denied and you're bounced back.

### 4. Owner Main Menu

- **[1] Manage customers** → Customer submenu (§5.2)
- **[2] Manage materials** → Material submenu (§5.3)
- **[3] Manage printers** → Printer submenu (§5.4)
- **[4] Manage print orders / job queue** → Order submenu (§5.5)
- **[5] POS — checkout / print receipt** → goes straight into checkout (§5.6)
- **[6] Reports / sales history** → Report submenu (§5.8)
- **[9] Save all data** — force-saves every in-memory array (customers, materials, printers, orders, sales history) to their JSON files immediately, without waiting for exit.
- **[0] Log out** — saves all data, then returns to the mode-select screen (the program itself keeps running).

### 5. Menu-by-menu Details

#### 5.1 Order creation logic (used by both staff "Create new order" and customer "Create a new print order")
1. (Staff only) pick which customer the order is for from the customer list; customers create orders only for themselves.
2. Enter the print file name (e.g. `model.stl`).
3. Pick a material by code — each material entry is a specific name+color combo (e.g. `PLA / Red`) with its own price-per-gram and stock.
4. Enter the estimated model weight in grams. If it exceeds that material's current stock, the order is rejected on the spot.
5. The system calculates:
   - **Estimated print time (hours)** = weight ÷ 15 g/hour (the fixed print-speed constant).
   - **Price** = (weight × material price/gram) + (hours × 20 baht/hour) + a 20-baht base fee.
6. A summary (customer, file, material/color, weight, estimated time, price) is shown; you confirm with `y` or cancel.
7. **Printer assignment happens automatically — you never pick a printer yourself:**
   - **If any printer is currently `Idle`** → the order is assigned to the first idle printer found, its status flips to `Printing`, the material stock is deducted **right now**, a start timestamp is recorded, and the printer's status becomes `Printing`.
   - **If no printer is idle** (all are busy or in maintenance) → the order is created with status `Queued` ("waiting for print"). Nothing is deducted from stock yet — stock is only ever deducted at the moment a job actually starts printing, not when it's merely created or queued.
8. Either way, the order can be paid online immediately from the payment menu, or paid in cash at the shop later once it's done.

#### 5.2 Customer submenu
- **1. List all customers**
- **2. Search customer** — by code or partial name match
- **3. Add new customer** — name, phone, address; auto-generates a customer code
- **4. Delete customer** — blocked if that customer still has any order that isn't `PickedUp`, `Shipped`, or `Cancelled`
- **0. Back to main menu**

#### 5.3 Material submenu
- **1. List all materials** — stock quantities show in **red** once they drop to 100 g or below, as a low-stock warning
- **2. Search material** — by code, name, or color
- **3. Add new material** — name (e.g. PLA/ABS/PETG), color, price per gram, stock in grams; auto-generates a material code
- **4. Delete material**
- **0. Back to main menu**

#### 5.4 Printer submenu
- **1. List all printers** — shows code, name, type, status (color-coded: green=Idle, yellow=Printing, red=Maintenance), and current order (with live time-remaining if printing)
- **2. Search printer** — by code or name
- **3. Add new printer** — name/model and type (e.g. FDM/SLA/DLP); new printers always start as `Idle`
- **4. Delete printer** — blocked while the printer's status is `Printing`
- **5. Change printer status** — only allowed when the printer is not currently `Printing`; you then pick **1. Idle (ready)** or **2. Maintenance**
- **0. Back to main menu**

#### 5.5 Order submenu (staff)
- **1. List all orders** — full table: customer, material, printer, file, weight, hours, price, status, live time-remaining if printing, and paid/unpaid tag
- **2. Search order** — by order code, customer code, or customer name
- **3. Create new order** — see §5.1
- **4. Cancel order** — staff version; same rules as the customer's own cancel (blocked once `PickedUp`/`Shipped`/`Cancelled`, blocked if already paid), but staff can cancel any customer's order. Refunds stock if it had already been deducted and frees the printer if it was mid-print.
- **5. View queue/printer status (with time remaining)** — shows every printer with what it's currently printing and how much time is left, plus a list of all orders still sitting in the `Queued` ("waiting for print") state.
- **6. Process queue (match waiting jobs to free printers)** — manually sweeps every `Queued` order and, for each one, checks whether its material still has enough stock and whether any printer is `Idle`; if so it assigns the job, deducts stock, starts the print timer, and marks the printer `Printing`. Reports how many jobs were newly assigned (or that there was nothing to match).
- **7. Mark job completed early (force-complete manually)** — for the rare case a print finished faster than the time estimate. Only works on orders currently `Printing`; shows the estimated time remaining, asks for confirmation, then moves the order to `Completed` and frees its printer. (Normally you don't need this — see the automatic completion in §5.7.)
- **8. Confirm delivery (picked up at shop / shipped)** — only works on orders that are `Completed` **and already paid**. Automatically sets the order to `Shipped` if its fulfillment method was "Shipping", or `PickedUp` otherwise. This closes out the order lifecycle.
- **9. Pay online (on behalf of a customer)** — staff-side version of online payment; lets staff pick any unpaid, non-cancelled/non-fulfilled order and run it through the same pickup-or-shipping + payment-confirmation flow as the customer self-service option.
- **0. Back to main menu**

#### 5.6 POS — Checkout (cash payments)
Reached directly from the owner main menu's option 5. Shows two lists:
- Orders that are `Completed` and unpaid (eligible for cash checkout right now).
- Orders still `Printing` (shown for reference only, tagged with time remaining — these can't be picked up/paid in cash yet, though they may already be paid online).

You then enter an order code, and the system: confirms it's `Completed` and unpaid, asks for the cash amount received (rejecting anything less than the price), computes change, marks the order paid via `Cash` with fulfillment always `Pickup`, prints a formatted receipt to the console (date, order #, customer, file, material/color, weight, time, total, cash given, change), and appends a record to the sales history.

#### 5.7 Automatic print completion ("waiting for print" lifecycle, in full)
This runs quietly every time you open the main menu, the owner menu, the order menu, the printer menu, or the customer kiosk menu — you never have to trigger it:
- Every order currently `Printing` has its elapsed time (now − start time) compared against its estimated hours.
- Once elapsed time reaches the estimate, the order is auto-flipped to `Completed` and its printer is freed back to `Idle`, with a green `[Automatic]` message printed to the console.
- While an order is still `Printing`, it displays a live label such as `(2h 15m remaining)`, or `(almost done, updating status...)` once the remaining time drops to zero but the sweep hasn't run yet.
- An order that is `Queued` has no printer and no timer yet — it is purely "waiting for print" until a printer frees up (handled automatically at order-creation time if one is free, or manually via **Process queue**, §5.5 option 6).

#### 5.8 Reports submenu
- **1. Sales history** — every completed sale (date, order code, customer, material, color, payment method, price), plus a running total of bill count and total revenue.
- **2. Material stock summary** — reuses the material list view (§5.3 option 1), including the red low-stock highlighting.
- **0. Back to main menu**

### 6. Order Status vs. Payment Status
Order **status** (`Queued` → `Printing` → `Completed` → `PickedUp`/`Shipped`, or `Cancelled`) tracks physical progress only. **Payment** (`paid`, `paymentMethod`: Cash/Online, `fulfillment`: Pickup/Shipping) is tracked completely independently — an order can be paid online at any point from creation up until it's picked up/shipped/cancelled, regardless of whether it's still queued, currently printing, or already completed. Cash payments, by contrast, can only happen once an order is `Completed`, and are always treated as in-store pickup.

## Project Structure

```
.
├── include/            # Header files (declarations)
│   ├── types.h          # Constants + core structs (Customer, Material, Printer, Order, SalesRecord)
│   ├── globals.h         # Global arrays / counters
│   ├── utils.h            # String/input/console helpers
│   ├── json_util.h         # Minimal hand-written JSON reader/writer
│   ├── storage.h           # Load/save data to .json files
│   ├── search.h             # Linear search helpers (find index by key)
│   ├── display.h            # List views + print-time calculations
│   ├── customer.h           # Customer management
│   ├── material.h           # Material management
│   ├── printer.h            # Printer management
│   ├── order.h              # Order / job-queue management (owner + customer self-service)
│   ├── pos.h                 # POS checkout / receipt
│   ├── report.h              # Sales reports
│   └── app.h                  # Seeding, login, owner main menu
├── src/                 # Implementation files (mirrors include/)
├── Makefile             # Build configuration
└── *.json                # Generated data files (created at runtime)
```

## Data Storage

All data is persisted as plain JSON files in the working directory, read/written by a small hand-rolled JSON utility (no external library required):

| File                 | Contents            |
|-----------------------|----------------------|
| `customers.json`      | Customer records     |
| `materials.json`      | Material/filament stock |
| `printers.json`       | Printer inventory & status |
| `orders.json`         | Print orders / job queue |
| `sales_history.json`  | Completed sales records |

## Requirements

- A C++11-compatible compiler (`g++` recommended)
- `make`

## Build & Run

```bash
git clone https://github.com/CornaeX/DATA_STRUCTURE_PROJECT01_CPP.git
cd DATA_STRUCTURE_PROJECT01_CPP
make
./shop
```

To clean build artifacts:

```bash
make clean
```

### Windows

The Makefile uses `g++`; on Windows this works out of the box with **MinGW-w64**. The program also detects `_WIN32` at compile time and switches the console to UTF-8 automatically, so Thai text displays correctly in `cmd.exe` / PowerShell.

For a one-click option that doesn't require `make`, just double-click **`run.bat`** (or run it from `cmd.exe`). It compiles the sources with `g++` directly and launches `shop.exe`, closing any previously running instance first.

**Recommended:** Run the program from **Windows Terminal** rather than the legacy `cmd.exe`/PowerShell console host. Windows Terminal has better Unicode/UTF-8 support, so Thai text and box-drawing characters render more reliably.

## Default Login

Owner/staff mode is password-protected. The default password is set in `include/types.h`:

```cpp
const string OWNER_PASSWORD = "1234";
```

Change this constant (and rebuild) before any real-world use.

## Notes

- On first run, if no printers exist, the system seeds two sample printers so the workflow can be tested immediately.
- The UI and in-code comments are primarily in Thai, reflecting the project's original context; core identifiers (function/variable names) are in English.

## License

This project was created for educational purposes as part of a Data Structures course assignment.

---

# 🖨️ ระบบจัดการร้าน 3D Printing (ฉบับภาษาไทย)

ระบบขายหน้าร้าน (POS) และจัดการร้าน 3D Printing แบบ console (TUI) เขียนด้วยภาษา C++ จัดการข้อมูลลูกค้า วัสดุ เครื่องพิมพ์ ออเดอร์งานพิมพ์ คิวงาน การชำระเงิน/ใบเสร็จ และรายงานยอดขาย — บันทึกข้อมูลทั้งหมดลงไฟล์ JSON ในเครื่อง โดย **ไม่พึ่งพาไลบรารีภายนอกเลย** (ไม่ใช้ STL container ไม่ใช้ไลบรารี JSON จากภายนอก)

โปรเจกต์นี้จัดทำขึ้นเป็นงานวิชาโครงสร้างข้อมูล (Data Structures) โดยตั้งใจใช้ **array แบบ C ธรรมดา** แทน `std::vector` หรือ STL container อื่น ๆ เพื่อสาธิตการจัดการโครงสร้างข้อมูลด้วยมือ การค้นหาแบบเชิงเส้น (linear search) และการทำ CRUD บน array

## ฟีเจอร์หลัก

**โหมดลูกค้า (ตู้บริการตนเอง)**
- เข้าสู่ระบบด้วยเบอร์โทร หรือสมัครสมาชิกใหม่
- สั่งพิมพ์งาน (เลือกวัสดุ ระบุชื่อไฟล์ กรอกน้ำหนัก)
- ดู "ออเดอร์ของฉัน" พร้อมสถานะแบบเรียลไทม์
- ยกเลิกออเดอร์ที่ยังค้างอยู่
- ชำระเงินออนไลน์สำหรับออเดอร์

**โหมดเจ้าของร้าน/พนักงาน** (ต้องใส่รหัสผ่าน)
- **ลูกค้า** — เพิ่ม ค้นหา แสดงรายการ ลบ
- **วัสดุ** — เพิ่ม ค้นหา แสดงรายการ ลบ (เก็บสี ราคา/กรัม สต็อกเป็นกรัม)
- **เครื่องพิมพ์** — เพิ่ม ค้นหา แสดงรายการ ลบ เปลี่ยนสถานะ (ว่าง / กำลังพิมพ์ / ซ่อมบำรุง)
- **ออเดอร์และคิวงาน**
  - สร้างออเดอร์ พร้อมประเมินเวลาพิมพ์และราคาให้อัตโนมัติ
  - จับคู่ออเดอร์ที่รอคิวกับเครื่องพิมพ์ที่ว่าง
  - เปลี่ยนสถานะเป็นพิมพ์เสร็จอัตโนมัติตามเวลาที่ผ่านไป (หรือบังคับให้เสร็จเองก็ได้)
  - ยืนยันการส่งมอบสินค้า (รับที่ร้าน / จัดส่ง)
  - ยกเลิกออเดอร์ (คืนสต็อกวัสดุอัตโนมัติถ้าหักไปแล้ว)
- **POS (จุดขาย)** — ชำระเงิน เงินสดหรือออนไลน์ พิมพ์ใบเสร็จ
- **รายงาน** — ประวัติการขาย สรุปยอดขาย
- บันทึกข้อมูลทั้งหมดด้วยตนเองได้ทุกเมื่อ และบันทึกอัตโนมัติเมื่อออกจากโปรแกรม

## วงจรสถานะออเดอร์

```
Queued (รอคิว) → Printing (กำลังพิมพ์) → Completed (เสร็จแล้ว) → PickedUp / Shipped (รับแล้ว/จัดส่งแล้ว)
                                                                        ↘ Cancelled (ยกเลิก จาก Queued/Printing)
```

สถานะการชำระเงิน (`paid`, `paymentMethod`: Cash/Online) ถูกติดตามแยกต่างหากจากสถานะออเดอร์ ดังนั้นออเดอร์สามารถชำระเงินออนไลน์ได้ทุกช่วงเวลาก่อนที่จะรับสินค้า

## รายละเอียดฟีเจอร์ทั้งหมด (ทุกเมนู ทุกตัวเลือก)

หัวข้อนี้อธิบาย **ทุกหน้าจอ ทุกหมายเลขเมนู และพฤติกรรมของโปรแกรม** อย่างละเอียด — เลือกอะไรแล้วเกิดอะไรขึ้นบ้าง

### 0. ตอนเริ่มโปรแกรม

เมื่อเปิดโปรแกรม ระบบจะโหลดข้อมูลจาก `customers.json`, `materials.json`, `printers.json`, `orders.json`, และ `sales_history.json` เข้าสู่หน่วยความจำ (array) ถ้ายังไม่มีเครื่องพิมพ์ในระบบเลย ระบบจะสร้างเครื่องพิมพ์ตัวอย่าง 2 เครื่องให้อัตโนมัติ (`Ender-3 V2`, `Prusa MK3S`) ประเภท FDM เพื่อให้ทดสอบการทำงานได้ทันที ทุกครั้งที่เข้า (หรือกลับเข้า) เมนูใด ๆ โปรแกรมจะรัน **การตรวจสอบพิมพ์เสร็จอัตโนมัติ** เงียบ ๆ ก่อนแสดงหน้าจอเสมอ (ดูข้อ 5.7) ทำให้งานพิมพ์ที่ครบเวลาประมาณการแล้วเปลี่ยนเป็น `Completed` เองโดยไม่ต้องกดอะไรเลย

### 1. เมนูเลือกโหมด (หน้าจอแรก)

- **[1] โหมดลูกค้า** — เข้าสู่ตู้บริการตนเอง (ข้อ 2)
- **[2] โหมดเจ้าของร้าน/พนักงาน** — ต้องกรอกรหัสผ่านก่อน (ข้อ 3) จึงจะเข้าเมนูพนักงานได้
- **[0] ออกจากโปรแกรม** — บันทึกข้อมูลทั้งหมดลงดิสก์แล้วปิดโปรแกรม

### 2. โหมดลูกค้า (ตู้บริการตนเอง)

**ด่านเข้าสู่ระบบ/สมัครสมาชิก (แสดงก่อนเสมอ):**
- กรอกเบอร์โทรศัพท์ ถ้าตรงกับลูกค้าที่มีอยู่แล้ว จะเข้าสู่ระบบทันที
- ถ้าไม่พบเบอร์นี้ ระบบจะถามว่า "สมัครสมาชิกใหม่หรือไม่?" (y/n) ถ้าตอบ `y` จะให้กรอกชื่อและที่อยู่ สร้างรหัสลูกค้าใหม่ บันทึกข้อมูล แล้วเข้าสู่ระบบให้ทันที ถ้าตอบอย่างอื่นจะให้กรอกเบอร์โทรใหม่อีกครั้ง หรือพิมพ์ `0` เพื่อยกเลิกกลับไปหน้าเลือกโหมด

**เมนูตู้บริการตนเองของลูกค้า** เมื่อเข้าสู่ระบบแล้ว:
- **[1] สร้างออเดอร์พิมพ์งานใหม่** — ใช้ตรรกะเดียวกับการสร้างออเดอร์ของพนักงาน (ข้อ 5.1) แต่ผูกกับบัญชีของลูกค้าคนนั้นเท่านั้น (สั่งแทนคนอื่นไม่ได้) จะถูกถามตามลำดับ: ชื่อไฟล์ → วัสดุ/สี → น้ำหนักเป็นกรัม ระบบจะตรวจสอบสต็อกวัสดุ คำนวณเวลาและราคาโดยประมาณ แสดงสรุป แล้วให้ยืนยันก่อนสร้างออเดอร์จริง
- **[2] ดูสถานะออเดอร์ของฉัน** — แสดงเฉพาะออเดอร์ของลูกค้าคนนี้ พร้อมชื่อไฟล์ ราคา สถานะปัจจุบัน และ (ถ้ากำลังพิมพ์อยู่) ป้ายบอก "เวลาที่เหลือ" แบบเรียลไทม์ พร้อมป้าย `[ชำระแล้ว]`/`[ยังไม่ชำระ]`
- **[3] ยกเลิกออเดอร์ของฉัน** — ยกเลิกออเดอร์ของตัวเองด้วยรหัสออเดอร์ จะยกเลิกไม่ได้ถ้าออเดอร์อยู่ในสถานะ `PickedUp`, `Shipped`, หรือ `Cancelled` แล้ว หรือถ้าชำระเงินไปแล้ว (กรณีนี้ต้องติดต่อร้านเพื่อขอคืนเงิน) ถ้าออเดอร์เริ่มพิมพ์ไปแล้ว วัสดุที่หักสต็อกไปจะถูกคืนกลับ และเครื่องพิมพ์จะถูกปลดให้ว่าง
- **[4] ชำระเงินออนไลน์** — ชำระเงินสำหรับออเดอร์ของตัวเองที่ยังไม่ได้ชำระและยังไม่ถูกยกเลิก/รับ/จัดส่ง (คือชำระได้ทั้งก่อนพิมพ์และระหว่างพิมพ์ ไม่ใช่แค่หลังพิมพ์เสร็จ) ระบบจะถามวิธีรับสินค้า — **1. รับที่ร้าน** หรือ **2. จัดส่ง** (การจัดส่งต้องมี/อัปเดตที่อยู่จัดส่ง) — จากนั้นจำลองการยืนยันชำระเงินออนไลน์และพิมพ์ใบเสร็จ
- **[0] ออกจากระบบ** — กลับไปหน้าเลือกโหมด

### 3. เข้าสู่ระบบเจ้าของร้าน/พนักงาน

ต้องใส่รหัสผ่าน (ค่าเริ่มต้นคือ `1234` เก็บไว้ในไฟล์ `include/types.h` ในชื่อตัวแปร `OWNER_PASSWORD`) มีสิทธิ์กรอกได้ **3 ครั้ง** พิมพ์ `0` เมื่อใดก็ได้เพื่อยกเลิกกลับไปหน้าเลือกโหมด ถ้ากรอกผิดครบ 3 ครั้ง ระบบจะปฏิเสธการเข้าใช้งานและพากลับไปหน้าเดิม

### 4. เมนูหลักของเจ้าของร้าน/พนักงาน

- **[1] จัดการข้อมูลลูกค้า** → เมนูย่อยลูกค้า (ข้อ 5.2)
- **[2] จัดการข้อมูลวัสดุ** → เมนูย่อยวัสดุ (ข้อ 5.3)
- **[3] จัดการเครื่องพิมพ์** → เมนูย่อยเครื่องพิมพ์ (ข้อ 5.4)
- **[4] จัดการออเดอร์งานพิมพ์ / คิวงาน** → เมนูย่อยออเดอร์ (ข้อ 5.5)
- **[5] POS - ชำระเงิน / ออกใบเสร็จ** — เข้าสู่หน้าชำระเงินโดยตรง (ข้อ 5.6)
- **[6] รายงาน / ประวัติการขาย** → เมนูย่อยรายงาน (ข้อ 5.8)
- **[9] บันทึกข้อมูลทั้งหมด** — บังคับบันทึกข้อมูลทุก array ในหน่วยความจำ (ลูกค้า วัสดุ เครื่องพิมพ์ ออเดอร์ ประวัติการขาย) ลงไฟล์ JSON ทันที โดยไม่ต้องรอออกจากโปรแกรม
- **[0] ออกจากระบบ** — บันทึกข้อมูลทั้งหมด แล้วกลับไปหน้าเลือกโหมด (ตัวโปรแกรมยังทำงานอยู่ ไม่ได้ปิด)

### 5. รายละเอียดแต่ละเมนู

#### 5.1 ตรรกะการสร้างออเดอร์ (ใช้ร่วมกันทั้งเมนู "สร้างออเดอร์ใหม่" ของพนักงาน และ "สร้างออเดอร์พิมพ์งานใหม่" ของลูกค้า)
1. (เฉพาะพนักงาน) เลือกว่าออเดอร์นี้เป็นของลูกค้าคนไหนจากรายชื่อลูกค้า; ลูกค้าที่สร้างออเดอร์เองจะสร้างให้ตัวเองเท่านั้น
2. กรอกชื่อไฟล์งานพิมพ์ (เช่น `model.stl`)
3. เลือกวัสดุด้วยรหัส — วัสดุแต่ละรายการคือชื่อ+สีเฉพาะเจาะจง (เช่น `PLA / Red`) มีราคาต่อกรัมและสต็อกของตัวเอง
4. กรอกน้ำหนักโมเดลโดยประมาณเป็นกรัม ถ้าน้ำหนักเกินสต็อกคงเหลือของวัสดุนั้น ระบบจะปฏิเสธออเดอร์ทันที
5. ระบบจะคำนวณ:
   - **เวลาพิมพ์โดยประมาณ (ชั่วโมง)** = น้ำหนัก ÷ 15 กรัม/ชั่วโมง (ค่าคงที่ความเร็วพิมพ์)
   - **ราคา** = (น้ำหนัก × ราคาวัสดุ/กรัม) + (จำนวนชั่วโมง × 20 บาท/ชั่วโมง) + ค่าดำเนินการเริ่มต้น 20 บาท
6. ระบบแสดงสรุป (ลูกค้า ไฟล์งาน วัสดุ/สี น้ำหนัก เวลาโดยประมาณ ราคา) แล้วให้ยืนยันด้วย `y` หรือยกเลิก
7. **การมอบหมายเครื่องพิมพ์เกิดขึ้นอัตโนมัติทั้งหมด — ผู้ใช้ไม่ต้องเลือกเครื่องเอง:**
   - **ถ้ามีเครื่องพิมพ์ที่ว่าง (`Idle`) อยู่** → ออเดอร์จะถูกมอบหมายให้เครื่องว่างเครื่องแรกที่พบ สถานะเครื่องเปลี่ยนเป็น `Printing` หักสต็อกวัสดุ **ทันที** บันทึกเวลาที่เริ่มพิมพ์ และสถานะเครื่องพิมพ์เปลี่ยนเป็น `Printing`
   - **ถ้าไม่มีเครื่องว่างเลย** (ทุกเครื่องกำลังพิมพ์อยู่หรืออยู่ระหว่างซ่อมบำรุง) → ออเดอร์จะถูกสร้างด้วยสถานะ `Queued` ("รอคิวพิมพ์") โดยยังไม่หักสต็อกใด ๆ — สต็อกจะถูกหักก็ต่อเมื่องานเริ่มพิมพ์จริงเท่านั้น ไม่ใช่ตอนสร้างหรือตอนอยู่ในคิว
8. ไม่ว่ากรณีใด ออเดอร์สามารถชำระเงินออนไลน์ได้ทันทีจากเมนูชำระเงิน หรือชำระเงินสดที่ร้านภายหลังเมื่องานเสร็จก็ได้

#### 5.2 เมนูย่อยลูกค้า
- **1. แสดงรายชื่อลูกค้าทั้งหมด**
- **2. ค้นหาลูกค้า** — ค้นด้วยรหัส หรือชื่อบางส่วน
- **3. เพิ่มลูกค้าใหม่** — ชื่อ เบอร์โทร ที่อยู่; สร้างรหัสลูกค้าให้อัตโนมัติ
- **4. ลบลูกค้า** — ลบไม่ได้ถ้าลูกค้าคนนั้นยังมีออเดอร์ที่ไม่ใช่ `PickedUp`, `Shipped`, หรือ `Cancelled` ค้างอยู่
- **0. กลับเมนูหลัก**

#### 5.3 เมนูย่อยวัสดุ
- **1. แสดงรายการวัสดุทั้งหมด** — จำนวนสต็อกจะแสดงเป็น **สีแดง** เมื่อเหลือ 100 กรัมหรือน้อยกว่า เป็นการเตือนสต็อกใกล้หมด
- **2. ค้นหาวัสดุ** — ค้นด้วยรหัส ชื่อ หรือสี
- **3. เพิ่มวัสดุใหม่** — ชื่อ (เช่น PLA/ABS/PETG) สี ราคาต่อกรัม สต็อกเป็นกรัม; สร้างรหัสวัสดุให้อัตโนมัติ
- **4. ลบวัสดุ**
- **0. กลับเมนูหลัก**

#### 5.4 เมนูย่อยเครื่องพิมพ์
- **1. แสดงรายการเครื่องพิมพ์ทั้งหมด** — แสดงรหัส ชื่อ ประเภท สถานะ (แยกสี: เขียว=ว่าง, เหลือง=กำลังพิมพ์, แดง=ซ่อมบำรุง) และออเดอร์ปัจจุบัน (พร้อมเวลาที่เหลือแบบเรียลไทม์ถ้ากำลังพิมพ์)
- **2. ค้นหาเครื่องพิมพ์** — ค้นด้วยรหัสหรือชื่อ
- **3. เพิ่มเครื่องพิมพ์ใหม่** — ชื่อ/รุ่น และประเภท (เช่น FDM/SLA/DLP); เครื่องพิมพ์ใหม่จะเริ่มต้นเป็นสถานะ `Idle` เสมอ
- **4. ลบเครื่องพิมพ์** — ลบไม่ได้ในขณะที่เครื่องมีสถานะ `Printing`
- **5. เปลี่ยนสถานะเครื่อง** — ทำได้เฉพาะเมื่อเครื่องนั้นไม่ได้อยู่ในสถานะ `Printing` แล้วให้เลือก **1. Idle (พร้อมใช้งาน)** หรือ **2. Maintenance (ซ่อมบำรุง)**
- **0. กลับเมนูหลัก**

#### 5.5 เมนูย่อยออเดอร์ (ฝั่งพนักงาน)
- **1. แสดงออเดอร์ทั้งหมด** — ตารางแบบเต็ม: ลูกค้า วัสดุ เครื่องพิมพ์ ไฟล์งาน น้ำหนัก ชั่วโมง ราคา สถานะ เวลาที่เหลือแบบเรียลไทม์ถ้ากำลังพิมพ์ และป้ายชำระแล้ว/ยังไม่ชำระ
- **2. ค้นหาออเดอร์** — ค้นด้วยรหัสออเดอร์ รหัสลูกค้า หรือชื่อลูกค้า
- **3. สร้างออเดอร์ใหม่** — ดูข้อ 5.1
- **4. ยกเลิกออเดอร์** — เวอร์ชันของพนักงาน; กติกาเดียวกับที่ลูกค้ายกเลิกออเดอร์ของตัวเอง (ยกเลิกไม่ได้เมื่อเป็น `PickedUp`/`Shipped`/`Cancelled` แล้ว หรือชำระเงินไปแล้ว) แต่พนักงานยกเลิกออเดอร์ของลูกค้าคนไหนก็ได้ คืนสต็อกให้ถ้าหักไปแล้ว และปลดเครื่องพิมพ์ให้ว่างถ้ากำลังพิมพ์อยู่
- **5. ดูสถานะคิว/เครื่องพิมพ์ (พร้อมเวลาที่เหลือ)** — แสดงเครื่องพิมพ์ทุกเครื่องพร้อมงานที่กำลังพิมพ์อยู่และเวลาที่เหลือ พร้อมรายชื่อออเดอร์ทั้งหมดที่ยังอยู่ในสถานะ `Queued` ("รอคิวพิมพ์")
- **6. ประมวลผลคิว (จับคู่งานที่รอกับเครื่องว่าง)** — กวาดตรวจออเดอร์ที่เป็น `Queued` ทุกตัวด้วยตนเอง สำหรับแต่ละตัวจะตรวจว่าวัสดุยังพอไหมและมีเครื่องพิมพ์ที่ `Idle` หรือไม่ ถ้ามีจะมอบหมายงาน หักสต็อก เริ่มจับเวลา และเปลี่ยนเครื่องพิมพ์เป็น `Printing` ให้ทันที รายงานว่ามอบหมายงานไปกี่รายการ (หรือไม่มีอะไรให้จับคู่)
- **7. แจ้งพิมพ์งานเสร็จก่อนเวลา (บังคับ Completed ด้วยตนเอง)** — สำหรับกรณีที่งานพิมพ์เสร็จเร็วกว่าเวลาประมาณการ ใช้ได้เฉพาะออเดอร์ที่กำลัง `Printing` เท่านั้น จะแสดงเวลาที่เหลือตามประมาณการ ให้ยืนยัน แล้วเปลี่ยนออเดอร์เป็น `Completed` และปลดเครื่องพิมพ์ให้ว่าง (ปกติไม่จำเป็นต้องใช้เมนูนี้ เพราะมีการเปลี่ยนสถานะอัตโนมัติอยู่แล้ว ดูข้อ 5.7)
- **8. ยืนยันส่งมอบสินค้า (รับที่ร้าน/จัดส่งแล้ว)** — ใช้ได้เฉพาะออเดอร์ที่เป็น `Completed` **และชำระเงินแล้ว** เท่านั้น ระบบจะเปลี่ยนสถานะเป็น `Shipped` อัตโนมัติถ้าวิธีรับสินค้าคือ "จัดส่ง" หรือเป็น `PickedUp` ถ้ารับที่ร้าน ถือเป็นการปิดวงจรของออเดอร์นั้น
- **9. ชำระเงินออนไลน์ (แทนลูกค้า)** — เวอร์ชันของพนักงานสำหรับการชำระเงินออนไลน์; ให้พนักงานเลือกออเดอร์ที่ยังไม่ชำระและยังไม่ถูกยกเลิก/ส่งมอบตัวไหนก็ได้ แล้วดำเนินการตามขั้นตอนเลือกวิธีรับสินค้า + ยืนยันชำระเงิน เหมือนกับที่ลูกค้าทำเอง
- **0. กลับเมนูหลัก**

#### 5.6 POS — ชำระเงิน (การชำระด้วยเงินสด)
เข้าถึงได้โดยตรงจากเมนูหลักของเจ้าของร้าน ตัวเลือกที่ 5 แสดงรายการ 2 กลุ่ม:
- ออเดอร์ที่เป็น `Completed` และยังไม่ชำระ (พร้อมให้ชำระเงินสดได้ทันที)
- ออเดอร์ที่ยังอยู่ในสถานะ `Printing` (แสดงไว้เพื่ออ้างอิงเท่านั้น พร้อมป้ายเวลาที่เหลือ — ยังรับสินค้า/ชำระเงินสดไม่ได้ แม้จะอาจชำระเงินออนไลน์ไปแล้วก็ตาม)

จากนั้นกรอกรหัสออเดอร์ ระบบจะ: ตรวจสอบว่าเป็น `Completed` และยังไม่ชำระ ถามจำนวนเงินสดที่รับมา (ปฏิเสธถ้าน้อยกว่าราคาที่ต้องชำระ) คำนวณเงินทอน บันทึกว่าชำระแล้วด้วยวิธี `Cash` และวิธีรับสินค้าเป็น `Pickup` เสมอ พิมพ์ใบเสร็จออกทางหน้าจอ (วันที่ เลขที่ออเดอร์ ลูกค้า ไฟล์งาน วัสดุ/สี น้ำหนัก เวลา ยอดรวม เงินสดที่รับ เงินทอน) และบันทึกรายการลงประวัติการขาย

#### 5.7 การเปลี่ยนสถานะพิมพ์เสร็จอัตโนมัติ (วงจร "รอคิวพิมพ์" แบบละเอียด)
การทำงานนี้เกิดขึ้นเงียบ ๆ ทุกครั้งที่เปิดเมนูหลัก เมนูเจ้าของร้าน เมนูออเดอร์ เมนูเครื่องพิมพ์ หรือเมนูตู้บริการตนเองของลูกค้า — ไม่ต้องสั่งเอง:
- ทุกออเดอร์ที่อยู่ในสถานะ `Printing` จะถูกเช็คเวลาที่ผ่านไป (เวลาปัจจุบัน − เวลาที่เริ่มพิมพ์) เทียบกับจำนวนชั่วโมงที่ประมาณการไว้
- เมื่อเวลาที่ผ่านไปครบตามประมาณการแล้ว ออเดอร์จะถูกเปลี่ยนเป็น `Completed` อัตโนมัติ และเครื่องพิมพ์จะถูกปลดกลับเป็น `Idle` พร้อมข้อความสีเขียว `[อัตโนมัติ]` แสดงขึ้นบนหน้าจอ
- ขณะที่ออเดอร์ยังเป็น `Printing` อยู่ จะมีป้ายแสดงเวลาแบบเรียลไทม์ เช่น `(เหลืออีก 2ชม 15นาที)` หรือ `(ใกล้เสร็จ กำลังปรับสถานะ...)` เมื่อเวลาที่เหลือลดลงเป็นศูนย์แต่ยังไม่ถูกกวาดตรวจ
- ออเดอร์ที่อยู่ในสถานะ `Queued` จะยังไม่มีเครื่องพิมพ์และยังไม่เริ่มจับเวลา ถือว่า "รอคิวพิมพ์" อยู่จนกว่าจะมีเครื่องว่าง (จะถูกจับคู่ให้อัตโนมัติตอนสร้างออเดอร์ถ้ามีเครื่องว่างพอดี หรือจับคู่ด้วยมือผ่านเมนู **ประมวลผลคิว** ข้อ 5.5 ตัวเลือกที่ 6)

#### 5.8 เมนูย่อยรายงาน
- **1. ประวัติการขาย** — รายการขายที่เสร็จสมบูรณ์ทั้งหมด (วันที่ รหัสออเดอร์ ลูกค้า วัสดุ สี วิธีชำระเงิน ราคา) พร้อมยอดรวมจำนวนบิลและยอดขายรวมทั้งหมด
- **2. สรุปสต็อกวัสดุ** — ใช้หน้าจอแสดงรายการวัสดุแบบเดียวกับข้อ 5.3 ตัวเลือกที่ 1 รวมถึงการไฮไลต์สีแดงเมื่อสต็อกใกล้หมด
- **0. กลับเมนูหลัก**

### 6. สถานะออเดอร์ กับ สถานะการชำระเงิน
**สถานะออเดอร์** (`Queued` → `Printing` → `Completed` → `PickedUp`/`Shipped` หรือ `Cancelled`) ใช้ติดตามความคืบหน้าทางกายภาพเท่านั้น ส่วน **การชำระเงิน** (`paid`, `paymentMethod`: Cash/Online, `fulfillment`: Pickup/Shipping) ถูกติดตามแยกจากกันโดยสิ้นเชิง — ออเดอร์สามารถชำระเงินออนไลน์ได้ทุกจุดตั้งแต่สร้างออเดอร์จนถึงก่อนรับ/จัดส่ง/ยกเลิก ไม่ว่าจะยังรอคิว กำลังพิมพ์ หรือเสร็จแล้วก็ตาม ในทางกลับกัน การชำระเงินสดทำได้เฉพาะเมื่อออเดอร์เป็น `Completed` แล้วเท่านั้น และถือเป็นการรับสินค้าที่ร้านเสมอ

## โครงสร้างโปรเจกต์

```
.
├── include/            # ไฟล์ header (ประกาศฟังก์ชัน/โครงสร้าง)
│   ├── types.h          # ค่าคงที่ + struct หลัก (Customer, Material, Printer, Order, SalesRecord)
│   ├── globals.h         # global array / ตัวนับ
│   ├── utils.h            # ฟังก์ชันช่วยเหลือด้าน string/input/console
│   ├── json_util.h         # ตัวอ่าน/เขียน JSON แบบเรียบง่ายที่เขียนขึ้นเอง
│   ├── storage.h           # โหลด/บันทึกข้อมูลไฟล์ .json
│   ├── search.h             # ฟังก์ชันค้นหา index ในแต่ละ array
│   ├── display.h            # แสดงรายการ + คำนวณเวลาการพิมพ์
│   ├── customer.h           # จัดการลูกค้า
│   ├── material.h           # จัดการวัสดุ
│   ├── printer.h            # จัดการเครื่องพิมพ์
│   ├── order.h              # จัดการออเดอร์/คิวงาน (เจ้าของร้าน + ลูกค้า self-service)
│   ├── pos.h                 # POS ชำระเงิน/ใบเสร็จ
│   ├── report.h              # รายงานยอดขาย
│   └── app.h                  # การ seed ข้อมูล, login, เมนูหลักเจ้าของร้าน
├── src/                 # ไฟล์ implementation (โครงสร้างเหมือน include/)
├── Makefile             # การตั้งค่าการคอมไพล์
└── *.json                # ไฟล์ข้อมูลที่สร้างขึ้นตอนรันโปรแกรม
```

## การจัดเก็บข้อมูล

ข้อมูลทั้งหมดถูกบันทึกเป็นไฟล์ JSON ธรรมดาในโฟลเดอร์ที่รันโปรแกรม อ่าน/เขียนด้วยฟังก์ชัน JSON เล็ก ๆ ที่เขียนขึ้นเอง (ไม่ต้องใช้ไลบรารีภายนอก):

| ไฟล์                 | เนื้อหา            |
|-----------------------|----------------------|
| `customers.json`      | ข้อมูลลูกค้า     |
| `materials.json`      | สต็อกวัสดุ/เส้นพลาสติก |
| `printers.json`       | คลังเครื่องพิมพ์ & สถานะ |
| `orders.json`         | ออเดอร์งานพิมพ์ / คิวงาน |
| `sales_history.json`  | ประวัติการขายที่เสร็จสมบูรณ์แล้ว |

## ความต้องการของระบบ

- คอมไพเลอร์ที่รองรับ C++11 (แนะนำ `g++`)
- `make`

## วิธีคอมไพล์และรันโปรแกรม

```bash
git clone https://github.com/CornaeX/DATA_STRUCTURE_PROJECT01_CPP.git
cd DATA_STRUCTURE_PROJECT01_CPP
make
./shop
```

ล้างไฟล์ที่คอมไพล์แล้ว:

```bash
make clean
```

### Windows

Makefile ใช้ `g++` ซึ่งบน Windows จะใช้งานได้ทันทีถ้ามี **MinGW-w64** โปรแกรมยังตรวจจับ `_WIN32` ตอนคอมไพล์ และสลับ console ให้เป็น UTF-8 ให้อัตโนมัติ เพื่อให้ข้อความภาษาไทยแสดงผลได้ถูกต้องใน `cmd.exe` / PowerShell

ถ้าต้องการวิธีที่ไม่ต้องใช้ `make` เพียงดับเบิลคลิก **`run.bat`** (หรือรันจาก `cmd.exe`) ก็ได้ โปรแกรมจะคอมไพล์ซอร์สโค้ดด้วย `g++` โดยตรงและเปิด `shop.exe` ขึ้นมา พร้อมปิดโปรแกรมที่รันค้างอยู่ก่อนหน้า (ถ้ามี) ให้อัตโนมัติ

**คำแนะนำ:** ควรรันโปรแกรมผ่าน **Windows Terminal** แทนที่จะใช้ `cmd.exe`/PowerShell รุ่นเก่า เพราะ Windows Terminal รองรับ Unicode/UTF-8 ได้ดีกว่า ทำให้ข้อความภาษาไทยและเส้นกรอบตารางแสดงผลได้ถูกต้องแม่นยำกว่า

## รหัสผ่านเริ่มต้น

โหมดเจ้าของร้าน/พนักงานต้องใส่รหัสผ่าน ค่าเริ่มต้นถูกกำหนดไว้ในไฟล์ `include/types.h`:

```cpp
const string OWNER_PASSWORD = "1234";
```

ควรเปลี่ยนค่าคงที่นี้ (แล้วคอมไพล์ใหม่) ก่อนนำไปใช้งานจริง

## หมายเหตุ

- เมื่อรันโปรแกรมครั้งแรก ถ้ายังไม่มีเครื่องพิมพ์ในระบบ ระบบจะสร้างเครื่องพิมพ์ตัวอย่าง 2 เครื่องให้ เพื่อให้สามารถทดสอบขั้นตอนการทำงานได้ทันที
- หน้าจอ UI และคอมเมนต์ในโค้ดส่วนใหญ่เป็นภาษาไทย ตามบริบทดั้งเดิมของโปรเจกต์ ส่วนชื่อฟังก์ชัน/ตัวแปรหลักในโค้ดยังคงเป็นภาษาอังกฤษ

## สัญญาอนุญาต

โปรเจกต์นี้จัดทำขึ้นเพื่อการศึกษา เป็นส่วนหนึ่งของงานที่ได้รับมอบหมายในวิชาโครงสร้างข้อมูล