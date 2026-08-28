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