#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <set>
#include <random>
#include <chrono>
#include <iomanip>
#include <algorithm>
using namespace std;

// ---------------------------------------------------------------
// SwiftCargo - Multistage Graph Route Optimizer (Dynamic Programming)
// Backward DP: cost[i] = min over (i,j) { w(i,j) + cost[j] }
// ---------------------------------------------------------------

const double INF = 1e18;

struct Node  { int id; string name; int stage; string type; };
struct Route { int to; double cost; };
struct Request { int id, src, dest; string pkg; };

class MultistageGraph {
    int N = 0, K = 0;                    // nodes (ids 1..N), stages (1..K)
    vector<Node> nodes;                  // index = id
    vector<vector<Route>> adj;           // outgoing edges only (directed)
    vector<vector<int>> stageNodes;      // nodes of each stage
public:
    void init(int n, int k) {
        N = n; K = k;
        nodes.assign(n + 1, Node{0, "", 0, ""});
        adj.assign(n + 1, {});
        stageNodes.assign(k + 1, {});
    }
    void setNode(const Node& nd) { nodes[nd.id] = nd; stageNodes[nd.stage].push_back(nd.id); }

    bool addRoute(int u, int v, double c) {
        if (nodes[v].stage != nodes[u].stage + 1) return false;  // must go to next stage
        adj[u].push_back({v, c});
        return true;
    }

    // Real-time update: newCost < 0 means the road is closed
    bool updateRoute(int u, int v, double newCost) {
        auto& L = adj[u];
        for (size_t i = 0; i < L.size(); i++)
            if (L[i].to == v) {
                if (newCost < 0) L.erase(L.begin() + i);   // road closure
                else L[i].cost = newCost;
                return true;
            }
        return false;
    }

    const Node& node(int id) const { return nodes[id]; }
    int numNodes() const { return N; }
    int numStages() const { return K; }

    // Backward DP for one destination: fills cost[] and next[]
    void solve(int dest, vector<double>& cost, vector<int>& nxt) const {
        cost.assign(N + 1, INF);
        nxt.assign(N + 1, -1);
        cost[dest] = 0;
        for (int s = nodes[dest].stage - 1; s >= 1; s--)          // stage by stage, backwards
            for (int i : stageNodes[s])
                for (auto& r : adj[i])
                    if (cost[r.to] < INF && r.cost + cost[r.to] < cost[i]) {
                        cost[i] = r.cost + cost[r.to];
                        nxt[i] = r.to;
                    }
    }
};

vector<string> splitCSV(const string& line) {
    vector<string> out; string tok; stringstream ss(line);
    while (getline(ss, tok, ',')) { if (!tok.empty() && tok.back() == '\r') tok.pop_back(); out.push_back(tok); }
    return out;
}

// Batch processing: one DP per distinct destination serves all its requests
void processBatch(const MultistageGraph& g, const vector<Request>& reqs, const string& label) {
    cout << "\n---------- " << label << " ----------\n";
    map<int, pair<vector<double>, vector<int>>> cache;
    for (auto& r : reqs) {
        if (!cache.count(r.dest)) {
            auto& e = cache[r.dest];
            g.solve(r.dest, e.first, e.second);
        }
        auto& cost = cache[r.dest].first;
        auto& nxt  = cache[r.dest].second;
        cout << "Request " << r.id << " [" << r.pkg << "]: "
             << g.node(r.src).name << " -> " << g.node(r.dest).name << "\n";
        if (cost[r.src] >= INF) { cout << "   No route available\n"; continue; }
        cout << "   Minimum cost: " << fixed << setprecision(2) << cost[r.src] << "\n   Path: ";
        for (int v = r.src; v != -1; v = (v == r.dest ? -1 : nxt[v]))
            cout << v << (v == r.dest ? "\n" : " -> ");
    }
}

int main() {
    ifstream fn("nodes.csv"), fr("routes.csv"), fq("requests.csv");
    if (!fn || !fr || !fq) { cout << "Error: nodes.csv / routes.csv / requests.csv not found\n"; return 1; }

    string line; vector<Node> nodes; int maxId = 0, maxStage = 0;
    getline(fn, line);
    while (getline(fn, line)) {
        if (line.empty()) continue;
        auto f = splitCSV(line);
        Node nd{stoi(f[0]), f[1], stoi(f[2]), f[3]};
        maxId = max(maxId, nd.id); maxStage = max(maxStage, nd.stage);
        nodes.push_back(nd);
    }
    MultistageGraph g; g.init(maxId, maxStage);
    for (auto& nd : nodes) g.setNode(nd);

    int edgeCount = 0, closed = 0, invalid = 0;
    getline(fr, line);
    while (getline(fr, line)) {
        if (line.empty()) continue;
        auto f = splitCSV(line);
        if (f[6] == "closed") { closed++; continue; }          // skip closed roads
        double c = stod(f[3]) * stod(f[4]) * stod(f[5]);       // base x traffic x weather
        if (g.addRoute(stoi(f[1]), stoi(f[2]), c)) edgeCount++; else invalid++;
    }
    vector<Request> reqs;
    getline(fq, line);
    while (getline(fq, line)) {
        if (line.empty()) continue;
        auto f = splitCSV(line);
        reqs.push_back({stoi(f[0]), stoi(f[1]), stoi(f[2]), f[3]});
    }

    cout << "Total nodes read    : " << nodes.size() << "\n";
    cout << "Total stages        : " << maxStage << "\n";
    cout << "Total routes loaded : " << edgeCount << " (closed: " << closed << ", invalid: " << invalid << ")\n";
    cout << "Delivery requests   : " << reqs.size() << "\n";

    cout << "\n==============================================\n";
    cout << "     SWIFTCARGO ROUTE OPTIMIZATION SYSTEM\n";
    cout << "==============================================";
    processBatch(g, reqs, "Initial optimal routes");

    // ---------- REAL-TIME UPDATE ----------
    int u, v; double nc;
    cout << "\nEnter route to update (u v newCost, -1 = road closed): ";
    cin >> u >> v >> nc;
    if (!g.updateRoute(u, v, nc)) { cout << "Error: no route " << u << " -> " << v << "\n"; return 1; }
    if (nc < 0) cout << "\n>> ALERT: route " << u << " -> " << v << " is now CLOSED. Re-routing...\n";
    else        cout << "\n>> Traffic update: route " << u << " -> " << v << " now costs " << nc << ". Re-routing...\n";
    processBatch(g, reqs, "Re-computed optimal routes");

    // ---------- SCALABILITY TEST ----------
    {
        const int STAGES = 50, WIDTH = 40, DEG = 4;          // 2000 nodes
        mt19937 rng(42);
        MultistageGraph big; big.init(STAGES * WIDTH, STAGES);
        for (int s = 1; s <= STAGES; s++)
            for (int j = 0; j < WIDTH; j++) { int id = (s - 1) * WIDTH + j + 1; big.setNode({id, "", s, ""}); }
        long long E = 0;
        for (int s = 1; s < STAGES; s++)
            for (int j = 0; j < WIDTH; j++)
                for (int d = 0; d < DEG; d++) {
                    int to = s * WIDTH + (int)(rng() % WIDTH) + 1;
                    big.addRoute((s - 1) * WIDTH + j + 1, to, 1 + rng() % 100); E++;
                }
        vector<double> cost; vector<int> nxt;
        auto t0 = chrono::high_resolution_clock::now();
        big.solve(STAGES * WIDTH, cost, nxt);
        auto t1 = chrono::high_resolution_clock::now();
        double ms = chrono::duration<double, milli>(t1 - t0).count();
        cout << "\n---------- Scalability test ----------\n";
        cout << "Synthetic network: " << STAGES * WIDTH << " nodes, " << STAGES << " stages, " << E << " routes\n";
        cout << "DP solved in " << fixed << setprecision(3) << ms << " ms\n";
    }
    cout << "==============================================\n";
    return 0;
}
