#include <iostream>
#include <string>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>

// Enumerations for structured categorization
enum class Sex { Male, Female };
enum class Goal { Lose, Maintain, Gain };

/**
 * @brief Encapsulates user profile details and handles internal unit conversions.
 */
class UserProfile {
private:
    Sex sex_;
    double heightInches_;
    double weightLbs_;
    int age_;
    Goal goal_;

public:
    UserProfile(Sex sex, double heightInches, double weightLbs, int age, Goal goal)
        : sex_(sex), heightInches_(heightInches), weightLbs_(weightLbs), age_(age), goal_(goal) {}

    Sex getSex() const { return sex_; }
    int getAge() const { return age_; }
    Goal getGoal() const { return goal_; }
    double getWeightLbs() const { return weightLbs_; }

    // Internal unit conversions
    double getHeightCm() const {
        return heightInches_ * 2.54;
    }

    double getWeightKg() const {
        return weightLbs_ / 2.20462262;
    }
};

/**
 * @brief Handles BMR estimations and macro target calculations.
 */
class MacroCalculator {
public:
    struct PlanResult {
        double bmr;
        double targetCalories;
        double proteinGrams;
        double carbGrams;
        double fatGrams;
    };

    /**
     * @brief Estimates BMR using the Mifflin-St Jeor Equation.
     */
    static PlanResult calculateMifflinStJeor(const UserProfile& profile) {
        double w = profile.getWeightKg();
        double h = profile.getHeightCm();
        int a = profile.getAge();
        
        // Base formula
        double bmr = (10.0 * w) + (6.25 * h) - (5.0 * a);
        if (profile.getSex() == Sex::Male) {
            bmr += 5.0;
        } else {
            bmr -= 161.0;
        }

        return generatePlan(bmr, profile.getGoal());
    }

    /**
     * @brief Estimates BMR using the Revised Harris-Benedict Equation.
     */
    static PlanResult calculateHarrisBenedict(const UserProfile& profile) {
        double w = profile.getWeightKg();
        double h = profile.getHeightCm();
        int a = profile.getAge();
        double bmr = 0.0;

        if (profile.getSex() == Sex::Male) {
            bmr = 88.362 + (13.397 * w) + (4.799 * h) - (5.677 * a);
        } else {
            bmr = 447.593 + (9.247 * w) + (3.098 * h) - (4.330 * a);
        }

        return generatePlan(bmr, profile.getGoal());
    }

private:
    static PlanResult generatePlan(double bmr, Goal goal) {
        double targetCalories = bmr;
        
        // Apply ~1 lb/week adjustment (approx. 500 kcal/day)
        if (goal == Goal::Lose) {
            targetCalories -= 500.0;
        } else if (goal == Goal::Gain) {
            targetCalories += 500.0;
        }

        // Enforce safety floor
        if (targetCalories < 1200.0) {
            targetCalories = 1200.0;
        }

        // Macro Split Default: 30% Protein, 40% Carbs, 30% Fat
        double proteinCals = targetCalories * 0.30;
        double carbCals    = targetCalories * 0.40;
        double fatCals     = targetCalories * 0.30;

        // Convert calories to grams (Protein: 4 kcal/g, Carbs: 4 kcal/g, Fat: 9 kcal/g)
        double proteinGrams = proteinCals / 4.0;
        double carbGrams    = carbCals / 4.0;
        double fatGrams     = fatCals / 9.0;

        return { bmr, targetCalories, proteinGrams, carbGrams, fatGrams };
    }
};

/**
 * @brief User Interface handler for command-line inputs and formatted outputs.
 */
class CommandLineInterface {
public:
    void run() {
        std::cout << "===========================================\n";
        std::cout << "       COMMAND-LINE MACRO TRACKER          \n";
        std::cout << "===========================================\n\n";

        Sex sex = promptSex();
        double height = promptPositiveDouble("Enter height (in inches): ");
        double weight = promptPositiveDouble("Enter weight (in lbs): ");
        int age = static_cast<int>(promptPositiveDouble("Enter age (years): "));
        Goal goal = promptGoal();

        UserProfile profile(sex, height, weight, age, goal);
        
        auto mifflinResult = MacroCalculator::calculateMifflinStJeor(profile);
        auto harrisResult = MacroCalculator::calculateHarrisBenedict(profile);

        displayResults(profile, mifflinResult, harrisResult);
    }

private:
    Sex promptSex() {
        std::string input;
        while (true) {
            std::cout << "Enter sex (M/F): ";
            std::cin >> input;
            if (input == "M" || input == "m" || input == "Male" || input == "male") return Sex::Male;
            if (input == "F" || input == "f" || input == "Female" || input == "female") return Sex::Female;
            std::cout << "Invalid input. Please enter 'M' or 'F'.\n";
        }
    }

    Goal promptGoal() {
        std::string input;
        while (true) {
            std::cout << "Enter goal ([l]ose, [m]aintain, [g]ain): ";
            std::cin >> input;
            if (input == "l" || input == "lose") return Goal::Lose;
            if (input == "m" || input == "maintain") return Goal::Maintain;
            if (input == "g" || input == "gain") return Goal::Gain;
            std::cout << "Invalid input. Please choose lose, maintain, or gain.\n";
        }
    }

    double promptPositiveDouble(const std::string& prompt) {
        double value;
        while (true) {
            std::cout << prompt;
            std::cin >> value;
            if (std::cin.fail() || value <= 0) {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "Invalid input. Please enter a positive number.\n";
            } else {
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                return value;
            }
        }
    }

    void displayResults(const UserProfile& profile, 
                        const MacroCalculator::PlanResult& mifflin, 
                        const MacroCalculator::PlanResult& harris) {
        std::cout << "\n-------------------------------------------\n";
        std::cout << "               RESULTS SUMMARY             \n";
        std::cout << "-------------------------------------------\n";
        std::cout << std::fixed << std::setprecision(1);
        std::cout << "Goal Target:   ";
        switch (profile.getGoal()) {
            case Goal::Lose: std::cout << "Lose ~1 lb/week (-500 kcal deficit)\n"; break;
            case Goal::Maintain: std::cout << "Maintain weight\n"; break;
            case Goal::Gain: std::cout << "Gain ~1 lb/week (+500 kcal surplus)\n"; break;
        }
        std::cout << "\n";

        // Side-by-side comparison table
        std::cout << std::left << std::setw(24) << "Metric" 
                  << std::setw(18) << "Mifflin-St Jeor" 
                  << std::setw(18) << "Harris-Benedict" << "\n";
        std::cout << "----------------------------------------------------\n";
        
        printRow("BMR (kcal)", mifflin.bmr, harris.bmr);
        printRow("Target Calories", mifflin.targetCalories, harris.targetCalories);
        printRow("Protein (g)", mifflin.proteinGrams, harris.proteinGrams);
        printRow("Carbs (g)", mifflin.carbGrams, harris.carbGrams);
        printRow("Fat (g)", mifflin.fatGrams, harris.fatGrams);
        std::cout << "================================================----\n";
    }

    void printRow(const std::string& label, double val1, double val2) {
        std::cout << std::left << std::setw(24) << label 
                  << std::setw(18) << val1 
                  << std::setw(18) << val2 << "\n";
    }
};

int main() {
    CommandLineInterface cli;
    cli.run();
    return 0;
}