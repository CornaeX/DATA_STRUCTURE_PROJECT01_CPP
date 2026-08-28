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