#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

class MacroTracker {
public:
    enum class Sex { Male, Female };
    enum class Goal { Lose, Maintain, Gain };

    struct Person {
        Sex sex;
        double heightInches;
        double weightPounds;
        int ageYears;
        Goal goal;
    };

    struct Targets {
        double bmr;
        double dailyCalories;
        double proteinGrams;
        double carbsGrams;
        double fatGrams;
    };

    // Macro split: 30% protein, 40% carbohydrates, 30% fat.
    static constexpr double ProteinShare = 0.30;
    static constexpr double CarbsShare = 0.40;
    static constexpr double FatShare = 0.30;

    static double inchesToCentimeters(double inches) {
        return inches * 2.54;
    }

    static double poundsToKilograms(double pounds) {
        return pounds * 0.45359237;
    }

    // Mifflin-St Jeor equation. Returns BMR in kcal/day.
    static double mifflinStJeorBmr(const Person& person) {
        const double weightKg = poundsToKilograms(person.weightPounds);
        const double heightCm = inchesToCentimeters(person.heightInches);
        const double age = static_cast<double>(person.ageYears);

        if (person.sex == Sex::Male) {
            return 10.0 * weightKg + 6.25 * heightCm - 5.0 * age + 5.0;
        }
        return 10.0 * weightKg + 6.25 * heightCm - 5.0 * age - 161.0;
    }

    // Revised Harris-Benedict equation. Returns BMR in kcal/day.
    static double harrisBenedictBmr(const Person& person) {
        const double weightKg = poundsToKilograms(person.weightPounds);
        const double heightCm = inchesToCentimeters(person.heightInches);
        const double age = static_cast<double>(person.ageYears);

        if (person.sex == Sex::Male) {
            return 88.362 + 13.397 * weightKg + 4.799 * heightCm
                   - 5.677 * age;
        }
        return 447.593 + 9.247 * weightKg + 3.098 * heightCm
               - 4.330 * age;
    }

    static double goalAdjustment(const Goal goal) {
        switch (goal) {
            case Goal::Lose: return -500.0;
            case Goal::Maintain: return 0.0;
            case Goal::Gain: return 500.0;
        }
        return 0.0;
    }

    static Targets calculateTargets(double bmr, Goal goal) {
        // Deliberately no activity multiplier: this is BMR plus the goal adjustment.
        const double calories = std::max(0.0, bmr + goalAdjustment(goal));
        const double proteinCalories = calories * ProteinShare;
        const double carbsCalories = calories * CarbsShare;
        const double fatCalories = calories * FatShare;

        return {
            bmr,
            calories,
            proteinCalories / 4.0,
            carbsCalories / 4.0,
            fatCalories / 9.0
        };
    }

    static void run() {
        Person person{};
        person.sex = readSex();
        person.heightInches = readDouble("Height (inches, 36-96): ", 36.0, 96.0);
        person.weightPounds = readDouble("Weight (lbs, 50-700): ", 50.0, 700.0);
        person.ageYears = readAge();
        person.goal = readGoal();

        const Targets mifflin = calculateTargets(mifflinStJeorBmr(person), person.goal);
        const Targets harris = calculateTargets(harrisBenedictBmr(person), person.goal);

        std::cout << "\nDaily estimates (BMR only; no activity multiplier)\n";
        std::cout << "Goal adjustment: ";
        const double adjustment = goalAdjustment(person.goal);
        if (adjustment > 0.0) std::cout << '+';
        std::cout << std::fixed << std::setprecision(0) << adjustment
                  << " kcal/day\n\n";

        std::cout << std::left << std::setw(29) << "Metric"
                  << std::right << std::setw(18) << "Mifflin-St Jeor"
                  << std::setw(22) << "Harris-Benedict" << '\n';
        std::cout << std::string(69, '-') << '\n';
        printRow("BMR (kcal/day)", mifflin.bmr, harris.bmr);
        printRow("Target calories/day", mifflin.dailyCalories, harris.dailyCalories);
        printRow("Protein (g/day)", mifflin.proteinGrams, harris.proteinGrams);
        printRow("Carbs (g/day)", mifflin.carbsGrams, harris.carbsGrams);
        printRow("Fat (g/day)", mifflin.fatGrams, harris.fatGrams);

        std::cout << "\nMacro split: 30% protein / 40% carbs / 30% fat.\n"
                  << "Weight-change estimate: about 1 lb/week for a 500 kcal/day adjustment;\n"
                  << "actual results vary, and BMR is not the same as total daily energy use.\n";
    }

private:
    static std::string readLine(const std::string& prompt) {
        std::string value;
        while (true) {
            std::cout << prompt;
            if (!std::getline(std::cin, value)) {
                throw std::runtime_error("Input ended unexpectedly.");
            }
            const auto first = value.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) return "";
            const auto last = value.find_last_not_of(" \t\r\n");
            return value.substr(first, last - first + 1);
        }
    }

    static std::string lowercase(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char ch) {
                           return static_cast<char>(std::tolower(ch));
                       });
        return value;
    }

    static double readDouble(const std::string& prompt, double minimum, double maximum) {
        while (true) {
            const std::string input = readLine(prompt);
            try {
                std::size_t used = 0;
                const double value = std::stod(input, &used);
                if (used == input.size() && value >= minimum && value <= maximum) {
                    return value;
                }
            } catch (const std::exception&) {
                // Fall through to the validation message.
            }
            std::cout << "Enter a number from " << minimum << " to " << maximum
                      << ".\n";
        }
    }

    static int readAge() {
        while (true) {
            const std::string input = readLine("Age (years, 18-120): ");
            try {
                std::size_t used = 0;
                const int age = std::stoi(input, &used);
                if (used == input.size() && age >= 18 && age <= 120) {
                    return age;
                }
            } catch (const std::exception&) {
                // Fall through to the validation message.
            }
            std::cout << "Enter a whole-number age from 18 to 120.\\n";
        }
    }

    static Sex readSex() {
        while (true) {
            const std::string value = lowercase(readLine("Sex for BMR equation (male/female): "));
            if (value == "male" || value == "m") return Sex::Male;
            if (value == "female" || value == "f") return Sex::Female;
            std::cout << "Please enter male or female.\n";
        }
    }

    static Goal readGoal() {
        while (true) {
            const std::string value = lowercase(readLine("Goal (lose/maintain/gain): "));
            if (value == "lose") return Goal::Lose;
            if (value == "maintain") return Goal::Maintain;
            if (value == "gain") return Goal::Gain;
            std::cout << "Please enter lose, maintain, or gain.\n";
        }
    }

    static void printRow(const std::string& label, double first, double second) {
        std::cout << std::left << std::setw(29) << label
                  << std::right << std::setw(18) << std::fixed << std::setprecision(1)
                  << first << std::setw(22) << second << '\n';
    }
};

int main() {
    try {
        MacroTracker::run();
    } catch (const std::exception& error) {
        std::cerr << "\nError: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
