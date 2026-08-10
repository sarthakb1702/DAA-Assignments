#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
using namespace std;

struct Order {
    int orderId;
    int customerId;
    string timestamp;
    double amount;
};

// Merge two sorted parts
void merge(Order orders[], int left, int mid, int right) {

    int n1 = mid - left + 1;
    int n2 = right - mid;

    Order* L = new Order[n1];
    Order* R = new Order[n2];

    // Copy data
    for (int i = 0; i < n1; i++)
        L[i] = orders[left + i];

    for (int i = 0; i < n2; i++)
        R[i] = orders[mid + 1 + i];

    int i = 0;
    int j = 0;
    int k = left;

    // Compare timestamps
    while (i < n1 && j < n2) {

        if (L[i].timestamp <= R[j].timestamp) {
            orders[k] = L[i];
            i++;
        }
        else {
            orders[k] = R[j];
            j++;
        }

        k++;
    }

    // Copy remaining elements
    while (i < n1) {
        orders[k] = L[i];
        i++;
        k++;
    }

    while (j < n2) {
        orders[k] = R[j];
        j++;
        k++;
    }

    delete[] L;
    delete[] R;
}

// Merge Sort
void mergeSort(Order orders[], int left, int right) {

    if (left < right) {

        int mid = left + (right - left) / 2;

        mergeSort(orders, left, mid);
        mergeSort(orders, mid + 1, right);

        merge(orders, left, mid, right);
    }
}

int main() {

    const int MAX_ORDERS = 10000;

    Order* orders = new Order[MAX_ORDERS];

    ifstream file("orders_sample.csv");

    if (!file) {
        cout << "Error: Could not open orders_sample.csv" << endl;
        return 1;
    }

    string line;

    // Skip header
    getline(file, line);

    int count = 0;

    // Read CSV file
    while (getline(file, line) && count < MAX_ORDERS) {

        stringstream ss(line);
        string value;

        // order_id
        getline(ss, value, ',');
        orders[count].orderId = stoi(value);

        // customer_id
        getline(ss, value, ',');
        orders[count].customerId = stoi(value);

        // order_timestamp
        getline(ss, value, ',');
        orders[count].timestamp = value;

        // amount
        getline(ss, value, ',');
        orders[count].amount = stod(value);

        count++;
    }

    file.close();

    cout << "Total orders read: " << count << endl;

    // Sort using Merge Sort
    mergeSort(orders, 0, count - 1);

    cout << "\nOrders sorted by timestamp:\n\n";

    // Display first 20 orders
    int displayCount = min(count, 20);

    for (int i = 0; i < displayCount; i++) {

        cout << "Order ID: " << orders[i].orderId
             << " | Customer ID: " << orders[i].customerId
             << " | Timestamp: " << orders[i].timestamp
             << " | Amount: " << orders[i].amount
             << endl;
    }

    delete[] orders;

    return 0;
}
