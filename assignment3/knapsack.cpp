#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <iomanip>
using namespace std;

struct Item {
    int itemId;
    string name;
    string category;
    double weight;
    double utility;
    string divisible;
    double ratio;
    double selectedWeight;
    double selectedUtility;
};

// Compare items based on utility/weight ratio
bool compareRatio(const Item& a, const Item& b) {
    return a.ratio > b.ratio;
}

int main() {

    vector<Item> items;

    // Open CSV file
    ifstream file("relief_items.csv");

    if (!file) {
        cout << "Error: Could not open relief_items.csv" << endl;
        return 1;
    }

    string line;

    // Skip header
    getline(file, line);

    // Read dataset
    while (getline(file, line)) {

        stringstream ss(line);
        string value;
        Item item;

        // item_id
        getline(ss, value, ',');
        item.itemId = stoi(value);

        // name
        getline(ss, value, ',');
        item.name = value;

        // category
        getline(ss, value, ',');
        item.category = value;

        // weight_kg
        getline(ss, value, ',');
        item.weight = stod(value);

        // utility_value
        getline(ss, value, ',');
        item.utility = stod(value);

        // divisible
        getline(ss, value, ',');
        item.divisible = value;

        // Calculate utility/weight ratio
        item.ratio = item.utility / item.weight;

        item.selectedWeight = 0;
        item.selectedUtility = 0;

        items.push_back(item);
    }

    file.close();

    cout << "Total items read: " << items.size() << endl;

    // Display capacity
    double capacity;

    cout << "\nEnter boat capacity (kg): ";
    cin >> capacity;

    // Sort according to utility/weight ratio
    sort(items.begin(), items.end(), compareRatio);

    double remainingCapacity = capacity;
    double totalUtility = 0;

    // Select items
    for (auto& item : items) {

        if (remainingCapacity <= 0)
            break;

        // Divisible item
        if (item.divisible == "yes") {

            if (item.weight <= remainingCapacity) {

                // Take complete item
                item.selectedWeight = item.weight;
                item.selectedUtility = item.utility;

                remainingCapacity -= item.weight;
                totalUtility += item.utility;
            }
            else {

                // Take fraction of item
                double fraction = remainingCapacity / item.weight;

                item.selectedWeight = remainingCapacity;
                item.selectedUtility = item.utility * fraction;

                totalUtility += item.selectedUtility;

                remainingCapacity = 0;
            }
        }

        // Non-divisible item
        else {

            if (item.weight <= remainingCapacity) {

                // Must take complete item
                item.selectedWeight = item.weight;
                item.selectedUtility = item.utility;

                remainingCapacity -= item.weight;
                totalUtility += item.utility;
            }
        }
    }

    // Display results
    cout << fixed << setprecision(2);

    cout << "\n==============================================\n";
    cout << "       EMERGENCY RELIEF SUPPLY PLAN\n";
    cout << "==============================================\n";

    cout << "Boat Capacity       : " << capacity << " kg\n";
    cout << "Used Capacity       : "
         << capacity - remainingCapacity << " kg\n";
    cout << "Remaining Capacity  : "
         << remainingCapacity << " kg\n";
    cout << "Total Utility       : "
         << totalUtility << "\n";

    cout << "\nSelected Items:\n";
    cout << "----------------------------------------------\n";

    for (const auto& item : items) {

        if (item.selectedWeight > 0) {

            cout << "ID: " << item.itemId
                 << " | " << item.name
                 << " | Weight: " << item.selectedWeight << " kg"
                 << " | Utility: " << item.selectedUtility;

            if (item.selectedWeight < item.weight) {
                cout << " | PARTIAL";
            }
            else {
                cout << " | FULL";
            }

            cout << endl;
        }
    }

    cout << "==============================================\n";

    return 0;
}