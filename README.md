# RideEaze Management System

RideEaze is a fast, simplified ride-booking management architecture built as a highly readable student-project codebase. It demonstrates a full-stack architecture using a lightweight C++ server backend and an elegant Vanilla Javascript frontend, with all core logic securely processed on the server.

## ✨ Core Features

- **C++ Native Backend Logic:** All core algorithms, including Dijkstra's Graph algorithm for distance calculation, fare estimation, and driver match sorting, are handled natively in the C++ backend for security and speed.
- **Persistent Data Storage:** Uses simple, readable CSV (`.txt`) files (`users.txt` and `bookings.txt`) to securely save user accounts and ride histories across server restarts.
- **Account Isolation:** Implements a strict User vs. Driver dashboard system. Drivers log in with their exact name to see only the rides assigned to them, while passengers see their own booking history.
- **Live City Mapping:** Integrates real-world OpenStreetMap (Nominatim) endpoints for a live autocomplete city search on the frontend.
- **Highly Humanized Codebase:** Explicitly avoids overly complex abstractions in favor of clean, standardized structs, modern `for` loops, and CSS classes to ensure the code is incredibly transparent and viva-ready.

## 🚀 Tech Stack

- **Backend:** C++ (with `httplib.h` header-only library)
- **Frontend:** HTML5, Modern CSS, Vanilla JavaScript 
- **APIs:** OpenStreetMap (Nominatim)

## 💻 Local Setup

1. **Compile Backend**
Use `clang++` or `g++` to compile the server code. 
```bash
clang++ -std=c++11 -o main main.cpp
```

2. **Run the Server**
```bash
./main
```
*The backend will boot up, load the persistent text files, and load default fallback data. It will start listening on port 8080.*

3. **Open the Front End**
Simply double-click the `RideEaze_UI.html` file in your system to load the dashboard. Allow the scripts to run and make requests to `localhost:8080`.

## 🗺️ Getting Started

When you land on the page, the system will prompt you for authentication.
- **Passenger Account:** `madhvan` / `1234`
- **Driver Account:** `Speedy John` / `1234`

To try a test search:
1. Search for `DELHI` as the pickup location from the live dropdown.
2. Search for `JAIPUR` as the dropoff location.
3. Hit Find Rides and watch the C++ backend algorithm evaluate the drivers, calculate fares and ETAs, and sort the matches!

---
*Built to deliver a seamless functional presentation for student review.*
