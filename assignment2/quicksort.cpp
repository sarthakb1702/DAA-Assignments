#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
using namespace std;

struct Movie {
    int movieId;
    string title;
    string genre;
    int releaseYear;
    double imdbRating;
    long long watchTime;
};

// Swap two movies
void swapMovies(Movie &a, Movie &b) {
    Movie temp = a;
    a = b;
    b = temp;
}

// Get value according to user's choice
bool shouldComeBefore(const Movie &a, const Movie &b, int choice) {

    if (choice == 1) {
        // IMDb Rating
        return a.imdbRating < b.imdbRating;
    }
    else if (choice == 2) {
        // Release Year
        return a.releaseYear < b.releaseYear;
    }
    else {
        // Watch Time
        return a.watchTime < b.watchTime;
    }
}

// Partition function
int partition(Movie movies[], int low, int high, int choice) {

    Movie pivot = movies[high];

    int i = low - 1;

    for (int j = low; j < high; j++) {

        if (shouldComeBefore(movies[j], pivot, choice)) {
            i++;
            swapMovies(movies[i], movies[j]);
        }
    }

    swapMovies(movies[i + 1], movies[high]);

    return i + 1;
}

// Quick Sort
void quickSort(Movie movies[], int low, int high, int choice) {

    if (low < high) {

        int pivotIndex = partition(
            movies,
            low,
            high,
            choice
        );

        quickSort(
            movies,
            low,
            pivotIndex - 1,
            choice
        );

        quickSort(
            movies,
            pivotIndex + 1,
            high,
            choice
        );
    }
}

int main() {

    const int MAX_MOVIES = 1000000;

    Movie* movies = new Movie[MAX_MOVIES];

    ifstream file("movies_sample.csv");

    if (!file) {
        cout << "Error: Could not open movies_sample.csv" << endl;
        delete[] movies;
        return 1;
    }

    string line;

    // Skip CSV header
    getline(file, line);

    int count = 0;

    // Read CSV file
    while (getline(file, line) && count < MAX_MOVIES) {

        stringstream ss(line);
        string value;

        // movie_id
        getline(ss, value, ',');
        movies[count].movieId = stoi(value);

        // title
        getline(ss, value, ',');
        movies[count].title = value;

        // genre
        getline(ss, value, ',');
        movies[count].genre = value;

        // release_year
        getline(ss, value, ',');
        movies[count].releaseYear = stoi(value);

        // imdb_rating
        getline(ss, value, ',');
        movies[count].imdbRating = stod(value);

        // watch_time_minutes
        getline(ss, value, ',');
        movies[count].watchTime = stoll(value);

        count++;
    }

    file.close();

    cout << "Total movies read: " << count << endl;

    // Menu
    int choice;

    cout << "\nSort movies by:\n";
    cout << "1. IMDb Rating\n";
    cout << "2. Release Year\n";
    cout << "3. Watch Time Popularity\n";
    cout << "Enter your choice: ";
    cin >> choice;

    if (choice < 1 || choice > 3) {
        cout << "Invalid choice!" << endl;
        delete[] movies;
        return 1;
    }

    // Apply Quick Sort
    quickSort(movies, 0, count - 1, choice);

    cout << "\nMovies sorted successfully!\n";
    cout << "Showing first 20 movies:\n\n";

    // Display first 20 movies
    int displayCount = (count < 20) ? count : 20;

    for (int i = 0; i < displayCount; i++) {

        cout << "Movie ID: " << movies[i].movieId
             << " | Title: " << movies[i].title
             << " | Genre: " << movies[i].genre
             << " | Release Year: " << movies[i].releaseYear
             << " | IMDb Rating: " << movies[i].imdbRating
             << " | Watch Time: " << movies[i].watchTime
             << " minutes"
             << endl;
    }

    delete[] movies;

    return 0;
}