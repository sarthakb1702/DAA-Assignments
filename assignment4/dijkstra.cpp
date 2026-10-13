#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <queue>
#include <map>
#include <set>
#include <algorithm>
#include <climits>
#include <iomanip>
using namespace std;

// ---------------------------------------------------------------
// Smart Traffic Management System for Ambulance Routing
// Dijkstra's Algorithm (Greedy) using a Min-Heap Priority Queue
// ---------------------------------------------------------------

struct Edge {
    int to;
    int weight;      // travel time in minutes
    string roadName;
    string traffic;
};

class TrafficNetwork {
    int V;
    vector<vector<Edge>> adj;       // adjacency list
    map<int, string> hospitals;     // hospital node -> name

public:
    TrafficNetwork(int v) : V(v), adj(v) {}

    // Add a bidirectional road
    void addEdge(int u, int v, int w, const string& name, const string& traffic) {
        adj[u].push_back({v, w, name, traffic});
        adj[v].push_back({u, w, name, traffic});
    }

    // Real-time traffic update (updates both directions)
    bool updateEdgeWeight(int u, int v, int newWeight, const string& newTraffic) {
        bool found = false;
        for (auto& e : adj[u])
            if (e.to == v) { e.weight = newWeight; e.traffic = newTraffic; found = true; }
        for (auto& e : adj[v])
            if (e.to == u) { e.weight = newWeight; e.traffic = newTraffic; }
        return found;
    }

    void addHospital(int node, const string& name) { hospitals[node] = name; }

    bool isHospital(int node) const { return hospitals.count(node) > 0; }

    string hospitalName(int node) const {
        auto it = hospitals.find(node);
        return it == hospitals.end() ? "" : it->second;
    }

    int size() const { return V; }

    // Dijkstra: stops at the first (i.e. nearest) hospital popped from the heap
    // Returns (distance, path, hospitalNode)
    pair<int, vector<int>> shortestPathToNearestHospital(int source, int& hospitalNode) {
        vector<int> dist(V, INT_MAX);
        vector<int> parent(V, -1);
        priority_queue<pair<int,int>, vector<pair<int,int>>, greater<pair<int,int>>> pq;

        dist[source] = 0;
        pq.push({0, source});
        hospitalNode = -1;

        while (!pq.empty()) {
            auto [curDist, u] = pq.top();
            pq.pop();

            if (curDist > dist[u]) continue;      // stale entry

            if (isHospital(u)) {                  // nearest hospital found
                vector<int> path;
                for (int v = u; v != -1; v = parent[v]) path.push_back(v);
                reverse(path.begin(), path.end());
                hospitalNode = u;
                return {dist[u], path};
            }

            for (auto& e : adj[u]) {
                if (dist[u] + e.weight < dist[e.to]) {
                    dist[e.to] = dist[u] + e.weight;
                    parent[e.to] = u;
                    pq.push({dist[e.to], e.to});
                }
            }
        }
        return {-1, {}};                          // no hospital reachable
    }

    // Print the route with road names and traffic levels
    void printRoute(const vector<int>& path) const {
        cout << "Path: ";
        for (size_t i = 0; i < path.size(); i++)
            cout << path[i] << (i + 1 < path.size() ? " -> " : "\n");

        cout << "Route details:\n";
        for (size_t i = 0; i + 1 < path.size(); i++) {
            for (auto& e : adj[path[i]]) {
                if (e.to == path[i + 1]) {
                    cout << "  " << setw(2) << path[i] << " -> " << setw(2) << path[i + 1]
                         << " | " << left << setw(22) << e.roadName << right
                         << " | " << setw(2) << e.weight << " min | "
                         << e.traffic << " traffic\n";
                    break;
                }
            }
        }
    }
};

vector<string> splitCSV(const string& line) {
    vector<string> out;
    string tok;
    stringstream ss(line);
    while (getline(ss, tok, ',')) {
        if (!tok.empty() && tok.back() == '\r') tok.pop_back();
        out.push_back(tok);
    }
    return out;
}

string trafficFromTime(int minutes, int oldMinutes) {
    if (minutes >= oldMinutes * 2) return "Severe";
    if (minutes > oldMinutes)      return "High";
    return "Low";
}

void report(TrafficNetwork& city, int src, const string& label) {
    int hosp;
    auto [dist, path] = city.shortestPathToNearestHospital(src, hosp);
    if (dist != -1) {
        cout << label << " shortest travel time: " << dist << " minutes\n";
        cout << "Nearest hospital: " << city.hospitalName(hosp)
             << " (node " << hosp << ")\n";
        city.printRoute(path);
    } else {
        cout << "No hospital reachable (" << label << ")\n";
    }
}

int main() {
    // ---------- READ ROADS ----------
    ifstream roads("city_roads.csv");
    if (!roads) { cout << "Error: Could not open city_roads.csv\n"; return 1; }

    struct Row { int u, v, w; string name, traffic; };
    vector<Row> rows;
    string line;
    getline(roads, line);                         // skip header
    int maxNode = -1;
    while (getline(roads, line)) {
        if (line.empty()) continue;
        auto f = splitCSV(line);
        if (f.size() < 6) continue;
        Row r{stoi(f[1]), stoi(f[2]), stoi(f[4]), f[3], f[5]};
        maxNode = max({maxNode, r.u, r.v});
        rows.push_back(r);
    }
    roads.close();

    TrafficNetwork city(maxNode + 1);
    for (auto& r : rows) city.addEdge(r.u, r.v, r.w, r.name, r.traffic);

    // ---------- READ HOSPITALS ----------
    ifstream hosp("hospitals.csv");
    if (!hosp) { cout << "Error: Could not open hospitals.csv\n"; return 1; }
    getline(hosp, line);                          // skip header
    int hospCount = 0;
    while (getline(hosp, line)) {
        if (line.empty()) continue;
        auto f = splitCSV(line);
        if (f.size() < 2) continue;
        city.addHospital(stoi(f[0]), f[1]);
        hospCount++;
    }
    hosp.close();

    cout << "Total intersections read: " << city.size() << endl;
    cout << "Total roads read        : " << rows.size() << endl;
    cout << "Total hospitals read    : " << hospCount << endl;

    // ---------- AMBULANCE START ----------
    int src;
    cout << "\nEnter ambulance starting location (0-" << city.size() - 1 << "): ";
    cin >> src;
    if (src < 0 || src >= city.size()) { cout << "Invalid location!\n"; return 1; }

    cout << "\n==============================================\n";
    cout << "     SMART AMBULANCE ROUTING SYSTEM\n";
    cout << "==============================================\n";
    report(city, src, "Initial");

    // ---------- REAL-TIME UPDATE ----------
    int u, v, newW;
    cout << "\nEnter edge to update (u v newWeight): ";
    cin >> u >> v >> newW;

    // find old weight to label the new traffic level
    int oldW = newW;
    for (auto& r : rows) if ((r.u == u && r.v == v) || (r.u == v && r.v == u)) oldW = r.w;

    if (!city.updateEdgeWeight(u, v, newW, trafficFromTime(newW, oldW))) {
        cout << "Error: no road exists between " << u << " and " << v << "\n";
        return 1;
    }

    cout << "\n--- Traffic update received: road " << u << " <-> " << v
         << " now takes " << newW << " minutes ---\n\n";
    report(city, src, "After update");
    cout << "==============================================\n";
    return 0;
}
