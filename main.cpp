#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <queue>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <cstdlib>
#include <cctype>
#include "httplib.h"

using namespace std;

// -------------------- Data structures --------------------

struct DriverRecord {
    string driverName;
    vector<string> driverRoute;
    string travelDate;       // YYYY-MM-DD
    int startTime;           // HHMM
    int endTime;             // HHMM (kept for compatibility)
    string carType;
};

struct BookingRecord {
    string passengerName;
    string contactNumber;
    string emailAddress;
    string travelDate;
    string passengerCount;
    string driverName;
    string accountUsername;
};

struct UserRecord {
    string username;
    string password;
    string accountType;
};

struct MatchResult {
    string driverName;
    string sharedPath;
    string carType;
    string travelDate;
    string pickupEtaFormatted;
    string dropoffEtaFormatted;
    int sharedDistanceKm = 0;
    int sharedTravelMinutes = 0;
    int calculatedPrice = 0;
    int compatibilityScore = 0;
    int daysDifference = 0;
    int extraStops = 0;
    int pickupIndex = 0;
};

map<string, vector<pair<string, int>>> cityGraph;
vector<DriverRecord> driverDatabase;
vector<BookingRecord> bookingDatabase;
vector<UserRecord> userDatabase;

// -------------------- General helpers --------------------

string trim(const string& value) {
    size_t first = value.find_first_not_of(" \t\r\n");
    if (first == string::npos) return "";
    size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

string toUpper(string value) {
    for (char& ch : value) ch = static_cast<char>(toupper(static_cast<unsigned char>(ch)));
    return value;
}

string jsonEscape(const string& value) {
    string out;
    for (char ch : value) {
        switch (ch) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(ch) < 32) out += ' ';
                else out += ch;
        }
    }
    return out;
}

vector<string> splitString(const string& text, char delimiter) {
    vector<string> parts;
    string item;
    stringstream ss(text);
    while (getline(ss, item, delimiter)) parts.push_back(trim(item));
    if (!text.empty() && text.back() == delimiter) parts.push_back("");
    return parts;
}

bool validDate(const string& date) {
    if (date.size() != 10 || date[4] != '-' || date[7] != '-') return false;
    for (int i : {0,1,2,3,5,6,8,9}) if (!isdigit(static_cast<unsigned char>(date[i]))) return false;
    int y = stoi(date.substr(0, 4));
    int m = stoi(date.substr(5, 2));
    int d = stoi(date.substr(8, 2));
    if (m < 1 || m > 12) return false;
    static const int days[] = {0,31,28,31,30,31,30,31,30,31,30,31};
    int maxDay = days[m];
    bool leap = (y % 400 == 0 || (y % 4 == 0 && y % 100 != 0));
    if (m == 2 && leap) maxDay = 29;
    return d >= 1 && d <= maxDay;
}

int dateToDays(const string& date) {
    int y = stoi(date.substr(0, 4));
    unsigned m = static_cast<unsigned>(stoi(date.substr(5, 2)));
    unsigned d = static_cast<unsigned>(stoi(date.substr(8, 2)));
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<int>(doe);
}

int getDaysDifference(const string& requestedDate, const string& driverDate) {
    if (!validDate(requestedDate) || !validDate(driverDate)) return 9999;
    return dateToDays(driverDate) - dateToDays(requestedDate);
}

bool parseHHMM(const string& value, int& hhmm) {
    if (value.size() != 5 || value[2] != ':') return false;
    if (!isdigit(static_cast<unsigned char>(value[0])) ||
        !isdigit(static_cast<unsigned char>(value[1])) ||
        !isdigit(static_cast<unsigned char>(value[3])) ||
        !isdigit(static_cast<unsigned char>(value[4]))) return false;
    int h = stoi(value.substr(0, 2));
    int m = stoi(value.substr(3, 2));
    if (h < 0 || h > 23 || m < 0 || m > 59) return false;
    hhmm = h * 100 + m;
    return true;
}

int hhmmToMinutes(int hhmm) {
    return (hhmm / 100) * 60 + (hhmm % 100);
}

// Accepts minutes since midnight, NOT HHMM.
string formatTimeAMPM(int totalMinutes) {
    totalMinutes %= 1440;
    if (totalMinutes < 0) totalMinutes += 1440;
    int hours = totalMinutes / 60;
    int minutes = totalMinutes % 60;
    string suffix = hours >= 12 ? "PM" : "AM";
    int displayHour = hours % 12;
    if (displayHour == 0) displayHour = 12;
    ostringstream out;
    out << displayHour << ":" << setw(2) << setfill('0') << minutes << " " << suffix;
    return out.str();
}

string currentDateString() {
    time_t now = time(nullptr);
    tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif
    ostringstream out;
    out << put_time(&localTime, "%Y-%m-%d");
    return out.str();
}

// -------------------- Graph and Dijkstra --------------------

void addRoad(const string& a, const string& b, int km) {
    cityGraph[a].push_back({b, km});
    cityGraph[b].push_back({a, km});
}

void buildCityGraph() {
    cityGraph.clear();

    // Delhi NCR and north India routes (approximate demonstration distances in km).
    addRoad("DELHI", "GURGAON", 30);
    addRoad("DELHI", "NOIDA", 25);
    addRoad("DELHI", "PANIPAT", 85);
    addRoad("DELHI", "MUZAFFARNAGAR", 130);
    addRoad("MUZAFFARNAGAR", "DEHRADUN", 135);
    addRoad("PANIPAT", "AMBALA", 110);
    addRoad("AMBALA", "CHANDIGARH", 50);
    addRoad("PANIPAT", "CHANDIGARH", 175);
    addRoad("GURGAON", "JAIPUR", 240);
    addRoad("JAIPUR", "AJMER", 135);
    addRoad("AJMER", "UDAIPUR", 265);
    addRoad("UDAIPUR", "AHMEDABAD", 260);
    addRoad("NOIDA", "AGRA", 190);
    addRoad("AGRA", "MATHURA", 55);
    addRoad("MATHURA", "DELHI", 160);
    addRoad("AGRA", "GWALIOR", 120);
    addRoad("AGRA", "KANPUR", 280);
    addRoad("KANPUR", "LUCKNOW", 90);

    // West India routes.
    addRoad("MUMBAI", "PUNE", 150);
    addRoad("PUNE", "LONAVALA", 65);
    addRoad("LONAVALA", "MUMBAI", 85);
    addRoad("SURAT", "MUMBAI", 280);
    addRoad("AHMEDABAD", "SURAT", 265);

    // Common naming variants from location autocomplete.
    addRoad("GURGAON", "GURUGRAM", 1);
}

int getGraphDistance(const string& startNode, const string& endNode) {
    if (startNode == endNode) return 0;
    if (!cityGraph.count(startNode) || !cityGraph.count(endNode)) return -1;

    map<string, int> distance;
    for (const auto& node : cityGraph) distance[node.first] = 1000000000;

    priority_queue<pair<int, string>,
                   vector<pair<int, string>>,
                   greater<pair<int, string>>> pq;

    distance[startNode] = 0;
    pq.push({0, startNode});

    while (!pq.empty()) {
        int currentDistance = pq.top().first;
        string currentCity = pq.top().second;
        pq.pop();

        if (currentDistance != distance[currentCity]) continue;
        if (currentCity == endNode) return currentDistance;

        for (const auto& edge : cityGraph[currentCity]) {
            const string& nextCity = edge.first;
            int newDistance = currentDistance + edge.second;
            if (newDistance < distance[nextCity]) {
                distance[nextCity] = newDistance;
                pq.push({newDistance, nextCity});
            }
        }
    }
    return -1; // no route exists
}

vector<string> getShortestPath(const string& startNode, const string& endNode) {
    if (startNode == endNode) return {startNode};
    if (!cityGraph.count(startNode) || !cityGraph.count(endNode)) return {};

    map<string, int> distance;
    map<string, string> previous;
    for (const auto& node : cityGraph) distance[node.first] = 1000000000;

    priority_queue<pair<int, string>,
                   vector<pair<int, string>>,
                   greater<pair<int, string>>> pq;

    distance[startNode] = 0;
    pq.push({0, startNode});

    while (!pq.empty()) {
        int currentDistance = pq.top().first;
        string currentCity = pq.top().second;
        pq.pop();

        if (currentDistance != distance[currentCity]) continue;
        if (currentCity == endNode) break;

        for (const auto& edge : cityGraph[currentCity]) {
            int newDistance = currentDistance + edge.second;
            if (newDistance < distance[edge.first]) {
                distance[edge.first] = newDistance;
                previous[edge.first] = currentCity;
                pq.push({newDistance, edge.first});
            }
        }
    }

    if (distance[endNode] == 1000000000) return {};

    vector<string> path;
    string at = endNode;
    while (at != startNode) {
        path.push_back(at);
        if (!previous.count(at)) return {};
        at = previous[at];
    }
    path.push_back(startNode);
    reverse(path.begin(), path.end());
    return path;
}

vector<string> expandDriverRoute(const vector<string>& route) {
    vector<string> expanded;
    if (route.empty()) return expanded;
    expanded.push_back(route.front());

    for (size_t i = 0; i + 1 < route.size(); ++i) {
        vector<string> segment = getShortestPath(route[i], route[i + 1]);
        if (segment.empty()) return {};
        for (size_t j = 1; j < segment.size(); ++j) expanded.push_back(segment[j]);
    }
    return expanded;
}

// -------------------- Sample data --------------------

void loadInitialData() {
    buildCityGraph();

    // Demonstration ride: supports testing DEHRADUN -> GURGAON.
    // Its date follows the current local date so the default search can find it.
    driverDatabase.push_back({
        "Demo Driver",
        {"DEHRADUN", "GURGAON"},
        currentDateString(),
        400, 2300,
        "SUV"
    });

    driverDatabase.push_back({
        "Madhvan",
        {"DELHI", "GURGAON", "JAIPUR", "AJMER"},
        currentDateString(),
        800, 1800,
        "SUV"
    });

    driverDatabase.push_back({
        "Neha",
        {"CHANDIGARH", "PANIPAT", "DELHI", "NOIDA"},
        currentDateString(),
        900, 1800,
        "Sedan"
    });

    driverDatabase.push_back({
        "Rahul",
        {"NOIDA", "AGRA", "KANPUR", "LUCKNOW"},
        currentDateString(),
        700, 1800,
        "Hatchback"
    });
}

// -------------------- Persistence --------------------

void saveUsers() {
    ofstream file("users.txt");
    for (const auto& user : userDatabase) {
        file << user.username << "," << user.password << "," << user.accountType << "\n";
    }
}

void loadUsers() {
    ifstream file("users.txt");
    if (!file.is_open()) return;

    userDatabase.clear();
    string line;
    while (getline(file, line)) {
        vector<string> parts = splitString(line, ',');
        if (parts.size() >= 3) userDatabase.push_back({parts[0], parts[1], parts[2]});
    }
}

void saveBookings() {
    ofstream file("bookings.txt");
    for (const auto& booking : bookingDatabase) {
        file << booking.passengerName << ","
             << booking.contactNumber << ","
             << booking.emailAddress << ","
             << booking.travelDate << ","
             << booking.passengerCount << ","
             << booking.driverName << ","
             << booking.accountUsername << "\n";
    }
}

void loadBookings() {
    ifstream file("bookings.txt");
    if (!file.is_open()) return;

    bookingDatabase.clear();
    string line;
    while (getline(file, line)) {
        vector<string> parts = splitString(line, ',');
        if (parts.size() >= 6) {
            string username = parts.size() >= 7 ? parts[6] : "unknown";
            bookingDatabase.push_back({
                parts[0], parts[1], parts[2], parts[3],
                parts[4], parts[5], username
            });
        }
    }
}

void saveDrivers() {
    ofstream file("drivers.txt");
    for (const auto& driver : driverDatabase) {
        string route;
        for (size_t i = 0; i < driver.driverRoute.size(); ++i) {
            if (i) route += "|";
            route += driver.driverRoute[i];
        }
        file << driver.driverName << "," << route << ","
             << driver.travelDate << "," << driver.startTime << ","
             << driver.endTime << "," << driver.carType << "\n";
    }
}

void loadDrivers() {
    ifstream file("drivers.txt");
    if (!file.is_open()) return;

    // Keep the demonstration drivers and append saved user-published rides.
    string line;
    while (getline(file, line)) {
        vector<string> parts = splitString(line, ',');
        if (parts.size() < 6) continue;
        vector<string> route = splitString(parts[1], '|');
        if (route.size() < 2) continue;
        driverDatabase.push_back({
            parts[0], route, parts[2], stoi(parts[3]), stoi(parts[4]), parts[5]
        });
    }
}

// -------------------- JSON responses --------------------

string buildDriversJSON() {
    string json = "[";
    for (size_t i = 0; i < driverDatabase.size(); ++i) {
        if (i) json += ",";
        string route;
        for (size_t j = 0; j < driverDatabase[i].driverRoute.size(); ++j) {
            if (j) route += "->";
            route += driverDatabase[i].driverRoute[j];
        }
        json += "{\"driverName\":\"" + jsonEscape(driverDatabase[i].driverName) + "\",";
        json += "\"travelDate\":\"" + jsonEscape(driverDatabase[i].travelDate) + "\",";
        json += "\"startTime\":\"" + formatTimeAMPM(hhmmToMinutes(driverDatabase[i].startTime)) + "\",";
        json += "\"carType\":\"" + jsonEscape(driverDatabase[i].carType) + "\",";
        json += "\"driverRoute\":\"" + jsonEscape(route) + "\"}";
    }
    json += "]";
    return json;
}

string buildBookingsJSON(const string& targetUser) {
    string json = "[";
    bool first = true;
    for (const auto& booking : bookingDatabase) {
        if (targetUser != "admin" &&
            booking.accountUsername != targetUser &&
            booking.driverName != targetUser) continue;

        if (!first) json += ",";
        first = false;

        json += "{\"passengerName\":\"" + jsonEscape(booking.passengerName) + "\",";
        json += "\"contactNumber\":\"" + jsonEscape(booking.contactNumber) + "\",";
        json += "\"emailAddress\":\"" + jsonEscape(booking.emailAddress) + "\",";
        json += "\"travelDate\":\"" + jsonEscape(booking.travelDate) + "\",";
        json += "\"passengerCount\":\"" + jsonEscape(booking.passengerCount) + "\",";
        json += "\"driverName\":\"" + jsonEscape(booking.driverName) + "\"}";
    }
    json += "]";
    return json;
}

// -------------------- Ride matching and scoring --------------------

string executeSearchAlgorithm(const string& pickupNode,
                              const string& dropoffNode,
                              const string& requestedDate,
                              int requestedTimeFormat) {
    const int requestedMinutes = hhmmToMinutes(requestedTimeFormat);
    vector<MatchResult> matches;

    for (const auto& driver : driverDatabase) {
        if (!validDate(driver.travelDate)) continue;

        vector<string> expandedRoute = expandDriverRoute(driver.driverRoute);
        if (expandedRoute.empty()) continue;

        int pickupIndex = -1;
        int dropoffIndex = -1;

        // Find the first valid pickup and a later drop-off in the expanded route.
        for (int i = 0; i < static_cast<int>(expandedRoute.size()); ++i) {
            if (expandedRoute[i] == pickupNode && pickupIndex == -1) pickupIndex = i;
            if (expandedRoute[i] == dropoffNode && pickupIndex != -1 && i > pickupIndex) {
                dropoffIndex = i;
                break;
            }
        }

        if (pickupIndex == -1 || dropoffIndex == -1) continue;

        int sharedDistance = getGraphDistance(pickupNode, dropoffNode);
        int distanceToPickup = getGraphDistance(expandedRoute.front(), pickupNode);
        if (sharedDistance < 0 || distanceToPickup < 0) continue;

        int daysDifference = getDaysDifference(requestedDate, driver.travelDate);
        if (abs(daysDifference) > 2) continue; // only show rides within two days

        // Approximate travel time using 40 km/h average speed (1.5 min/km).
        int travelToPickupMinutes = static_cast<int>(round(distanceToPickup * 1.5));
        int pickupEtaMinutes = hhmmToMinutes(driver.startTime) + travelToPickupMinutes;
        int sharedTravelMinutes = static_cast<int>(round(sharedDistance * 1.5));
        int dropoffEtaMinutes = pickupEtaMinutes + sharedTravelMinutes;

        int timeDifference = abs(pickupEtaMinutes - requestedMinutes);
        int extraStops = max(0, static_cast<int>(expandedRoute.size()) -
                                (dropoffIndex - pickupIndex + 1));

        int pricePerMinute = 5;
        if (driver.carType == "Hatchback") pricePerMinute = 4;
        else if (driver.carType == "SUV") pricePerMinute = 7;

        int calculatedPrice = 50 + sharedTravelMinutes * pricePerMinute;

        // Heuristic score: route detour, time difference and date difference.
        int compatibilityScore = 100
            - extraStops * 4
            - (timeDifference / 10)
            - abs(daysDifference) * 20;
        compatibilityScore = max(0, min(100, compatibilityScore));

        string sharedPath;
        for (int i = pickupIndex; i <= dropoffIndex; ++i) {
            if (!sharedPath.empty()) sharedPath += "->";
            sharedPath += expandedRoute[i];
        }

        MatchResult result;
        result.driverName = driver.driverName;
        result.sharedPath = sharedPath;
        result.carType = driver.carType;
        result.travelDate = driver.travelDate;
        result.sharedDistanceKm = sharedDistance;
        result.sharedTravelMinutes = sharedTravelMinutes;
        result.calculatedPrice = calculatedPrice;
        result.compatibilityScore = compatibilityScore;
        result.daysDifference = daysDifference;
        result.extraStops = extraStops;
        result.pickupIndex = pickupIndex;
        result.pickupEtaFormatted = formatTimeAMPM(pickupEtaMinutes);
        result.dropoffEtaFormatted = formatTimeAMPM(dropoffEtaMinutes);
        matches.push_back(result);
    }

    sort(matches.begin(), matches.end(), [](const MatchResult& a, const MatchResult& b) {
        if (a.compatibilityScore != b.compatibilityScore)
            return a.compatibilityScore > b.compatibilityScore;
        return a.calculatedPrice < b.calculatedPrice;
    });

    string json = "[";
    for (size_t i = 0; i < matches.size(); ++i) {
        if (i) json += ",";
        const auto& m = matches[i];
        json += "{";
        json += "\"driverName\":\"" + jsonEscape(m.driverName) + "\",";
        json += "\"sharedPath\":\"" + jsonEscape(m.sharedPath) + "\",";
        json += "\"carType\":\"" + jsonEscape(m.carType) + "\",";
        json += "\"travelDate\":\"" + jsonEscape(m.travelDate) + "\",";
        json += "\"sharedDistanceKm\":" + to_string(m.sharedDistanceKm) + ",";
        json += "\"sharedTravelMinutes\":" + to_string(m.sharedTravelMinutes) + ",";
        json += "\"calculatedPrice\":" + to_string(m.calculatedPrice) + ",";
        json += "\"compatibilityScore\":" + to_string(m.compatibilityScore) + ",";
        json += "\"daysDifference\":" + to_string(m.daysDifference) + ",";
        json += "\"extraStops\":" + to_string(m.extraStops) + ",";
        json += "\"pickupEtaFormatted\":\"" + jsonEscape(m.pickupEtaFormatted) + "\",";
        json += "\"dropoffEtaFormatted\":\"" + jsonEscape(m.dropoffEtaFormatted) + "\"";
        json += "}";
    }
    json += "]";
    return json;
}

// -------------------- HTTP server --------------------

void addCommonHeaders(httplib::Response& response) {
    response.set_header("Access-Control-Allow-Origin", "*");
    response.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    response.set_header("Access-Control-Allow-Headers", "Content-Type");
}

int main() {
    loadInitialData();
    loadUsers();
    loadBookings();
    loadDrivers();

    httplib::Server server;

    server.Options(R"(.*)", [](const httplib::Request&, httplib::Response& response) {
        addCommonHeaders(response);
        response.status = 204;
    });

    server.Get("/api/signup", [](const httplib::Request& request, httplib::Response& response) {
        addCommonHeaders(response);
        string username = trim(request.get_param_value("u"));
        string password = request.get_param_value("p");
        string type = request.get_param_value("t");
        if (type != "DRIVER" && type != "PASSENGER") type = "PASSENGER";
        if (username.empty() || password.empty()) {
            response.status = 400;
            response.set_content("Missing username or password", "text/plain");
            return;
        }
        for (const auto& user : userDatabase) {
            if (user.username == username) {
                response.set_content("Exists", "text/plain");
                return;
            }
        }
        userDatabase.push_back({username, password, type});
        saveUsers();
        response.set_content("Success", "text/plain");
    });

    server.Get("/api/login", [](const httplib::Request& request, httplib::Response& response) {
        addCommonHeaders(response);
        string username = request.get_param_value("u");
        string password = request.get_param_value("p");
        for (const auto& user : userDatabase) {
            if (user.username == username && user.password == password) {
                response.set_content("Success," + user.accountType, "text/plain");
                return;
            }
        }
        response.set_content("Failed", "text/plain");
    });

    server.Get("/api/search", [](const httplib::Request& request, httplib::Response& response) {
        addCommonHeaders(response);
        string pickup = toUpper(trim(request.get_param_value("pickupLocation")));
        string dropoff = toUpper(trim(request.get_param_value("dropoffLocation")));
        string date = request.get_param_value("travelDate");
        string time = request.get_param_value("preferredTime");

        int hhmm = 0;
        if (pickup.empty() || dropoff.empty() || pickup == dropoff ||
            !validDate(date) || !parseHHMM(time, hhmm)) {
            response.status = 400;
            response.set_content("[]", "application/json");
            return;
        }
        response.set_content(executeSearchAlgorithm(pickup, dropoff, date, hhmm), "application/json");
    });

    server.Get("/api/drivers", [](const httplib::Request&, httplib::Response& response) {
        addCommonHeaders(response);
        response.set_content(buildDriversJSON(), "application/json");
    });

    server.Get("/api/add", [](const httplib::Request& request, httplib::Response& response) {
        addCommonHeaders(response);
        string name = trim(request.get_param_value("nameInput"));
        string routeText = request.get_param_value("routeInput");
        string date = request.get_param_value("dateInput");
        string time = request.get_param_value("startInput");
        string car = trim(request.get_param_value("carInput"));

        int hhmm = 0;
        if (name.empty() || !validDate(date) || !parseHHMM(time, hhmm)) {
            response.status = 400;
            response.set_content("Invalid driver details", "text/plain");
            return;
        }

        vector<string> route;
        for (string city : splitString(routeText, ',')) {
            city = toUpper(trim(city));
            if (!city.empty()) route.push_back(city);
        }
        if (route.size() < 2) {
            response.status = 400;
            response.set_content("Route must contain at least two cities", "text/plain");
            return;
        }

        driverDatabase.push_back({name, route, date, hhmm, 2359, car});
        saveDrivers();
        response.set_content("Success", "text/plain");
    });

    server.Get("/api/bookings", [](const httplib::Request& request, httplib::Response& response) {
        addCommonHeaders(response);
        string user = request.get_param_value("user");
        response.set_content(buildBookingsJSON(user), "application/json");
    });

    server.Get("/api/book", [](const httplib::Request& request, httplib::Response& response) {
        addCommonHeaders(response);
        BookingRecord booking;
        booking.passengerName = trim(request.get_param_value("passengerName"));
        booking.contactNumber = trim(request.get_param_value("contactNumber"));
        booking.emailAddress = trim(request.get_param_value("emailAddress"));
        booking.travelDate = request.get_param_value("travelDate");
        booking.passengerCount = request.get_param_value("passengerCount");
        booking.driverName = trim(request.get_param_value("driverName"));
        booking.accountUsername = trim(request.get_param_value("accountUsername"));

        if (booking.passengerName.empty() || booking.contactNumber.empty() ||
            booking.emailAddress.empty() || !validDate(booking.travelDate) ||
            booking.passengerCount.empty() || booking.driverName.empty()) {
            response.status = 400;
            response.set_content("Invalid booking details", "text/plain");
            return;
        }

        bookingDatabase.push_back(booking);
        saveBookings();
        response.set_content("Success", "text/plain");
    });

    int port = 8082;
    if (getenv("PORT")) {
        try { port = stoi(getenv("PORT")); } catch (...) { port = 8080; }
    }

    cout << "RideEaze C++ server running on port " << port << "\n";
    cout << "Open index.html in your browser to use the application.\n";
    if (!server.listen("0.0.0.0", port)) {
        cerr << "Unable to start server on port " << port << ". Is the port already in use?\n";
        return 1;
    }
    return 0;
}
