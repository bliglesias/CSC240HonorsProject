// macro_tracker.cpp
//
// Command-line macro tracker. Given sex, height (in), weight (lb), age (years)
// and a goal (lose / maintain / gain), it estimates daily calorie and macro
// targets from BMR using two standard equations, side by side:
//
//   - Mifflin-St Jeor        (class MifflinStJeor)
//   - Harris-Benedict, 1984  (class HarrisBenedict)
//
// BMR only: no activity multiplier. No food data or food logging.
//
// Build: g++ -std=c++17 -Wall -Wextra claude_macroTracker_cpp.cpp -o macro_tracker

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
namespace constants {

constexpr double kCmPerInch = 2.54;
constexpr double kKgPerLb = 0.45359237;

// ~3500 kcal is the usual estimate for one pound of body weight, so a
// 1 lb/week change is 3500 / 7 = 500 kcal per day.
constexpr double kKcalPerPound = 3500.0;
constexpr double kDaysPerWeek = 7.0;
constexpr double kTargetLbPerWeek = 1.0;

// Never recommend a deficit that lands below this many kcal/day.
constexpr int kMinimumDailyCalories = 1200;

// Energy density of each macronutrient (kcal per gram).
constexpr double kKcalPerGramProtein = 4.0;
constexpr double kKcalPerGramCarbs = 4.0;
constexpr double kKcalPerGramFat = 9.0;

// Accepted input ranges (the equations are intended for adults).
constexpr double kMinHeightIn = 48.0, kMaxHeightIn = 90.0;
constexpr double kMinWeightLb = 70.0, kMaxWeightLb = 700.0;
constexpr double kMinAgeYears = 18.0, kMaxAgeYears = 100.0;

}  // namespace constants

// ---------------------------------------------------------------------------
// Domain types
// ---------------------------------------------------------------------------
enum class Sex { Male, Female };
enum class Goal { Lose, Maintain, Gain };

std::string toString(Goal goal) {
    switch (goal) {
        case Goal::Lose:     return "lose";
        case Goal::Maintain: return "maintain";
        case Goal::Gain:     return "gain";
    }
    return "";
}

std::string toString(Sex sex) { return sex == Sex::Male ? "male" : "female"; }

// A person, stored in the units the user typed (in / lb) and converted to
// metric on demand, because both BMR equations work in cm and kg.
class Person {
public:
    Person(Sex sex, double heightIn, double weightLb, int ageYears, Goal goal)
        : sex_(sex), heightIn_(heightIn), weightLb_(weightLb),
          ageYears_(ageYears), goal_(goal) {}

    Sex sex() const { return sex_; }
    int ageYears() const { return ageYears_; }
    Goal goal() const { return goal_; }
    double heightIn() const { return heightIn_; }
    double weightLb() const { return weightLb_; }

    double heightCm() const { return heightIn_ * constants::kCmPerInch; }
    double weightKg() const { return weightLb_ * constants::kKgPerLb; }

private:
    Sex sex_;
    double heightIn_;
    double weightLb_;
    int ageYears_;
    Goal goal_;
};

// ---------------------------------------------------------------------------
// BMR equations: one class per formula, behind a common interface
// ---------------------------------------------------------------------------
class BmrEquation {
public:
    virtual ~BmrEquation() = default;
    virtual std::string name() const = 0;
    // Basal metabolic rate in kcal/day.
    virtual double bmr(const Person& person) const = 0;
};

// Mifflin-St Jeor (1990):
//   men:   10*kg + 6.25*cm - 5*age + 5
//   women: 10*kg + 6.25*cm - 5*age - 161
class MifflinStJeor : public BmrEquation {
public:
    std::string name() const override { return "Mifflin-St Jeor"; }

    double bmr(const Person& p) const override {
        const double sexOffset = (p.sex() == Sex::Male) ? 5.0 : -161.0;
        return 10.0 * p.weightKg()
             + 6.25 * p.heightCm()
             - 5.0 * p.ageYears()
             + sexOffset;
    }
};

// Harris-Benedict, as revised by Roza & Shizgal (1984):
//   men:   88.362  + 13.397*kg + 4.799*cm - 5.677*age
//   women: 447.593 +  9.247*kg + 3.098*cm - 4.330*age
class HarrisBenedict : public BmrEquation {
public:
    std::string name() const override { return "Harris-Benedict"; }

    double bmr(const Person& p) const override {
        if (p.sex() == Sex::Male) {
            return 88.362 + 13.397 * p.weightKg()
                 + 4.799 * p.heightCm() - 5.677 * p.ageYears();
        }
        return 447.593 + 9.247 * p.weightKg()
             + 3.098 * p.heightCm() - 4.330 * p.ageYears();
    }
};

// ---------------------------------------------------------------------------
// Calorie and macro planning
// ---------------------------------------------------------------------------
struct MacroSplit {  // percent of daily calories; must sum to 100
    int proteinPct;
    int carbsPct;
    int fatPct;
};

struct MacroTargets {
    int calories = 0;
    int proteinG = 0;
    int carbsG = 0;
    int fatG = 0;
    bool floorApplied = false;  // true if the minimum-calorie floor kicked in
};

class MacroPlanner {
public:
    // Turns a BMR into calorie and macro targets for the given goal.
    static MacroTargets plan(double bmr, Goal goal) {
        MacroTargets t;

        const double wanted = bmr + dailyCalorieAdjustment(goal);
        const double floor = constants::kMinimumDailyCalories;
        t.floorApplied = wanted < floor;
        const double calories = std::max(wanted, floor);

        const MacroSplit split = splitFor(goal);
        t.calories = static_cast<int>(std::lround(calories));
        t.proteinG = gramsFor(calories, split.proteinPct, constants::kKcalPerGramProtein);
        t.carbsG   = gramsFor(calories, split.carbsPct,   constants::kKcalPerGramCarbs);
        t.fatG     = gramsFor(calories, split.fatPct,     constants::kKcalPerGramFat);
        return t;
    }

private:
    // +/- 500 kcal/day  ==  roughly 1 lb per week.
    static double dailyCalorieAdjustment(Goal goal) {
        const double perDay = constants::kTargetLbPerWeek * constants::kKcalPerPound
                            / constants::kDaysPerWeek;
        switch (goal) {
            case Goal::Lose: return -perDay;
            case Goal::Gain: return +perDay;
            case Goal::Maintain: break;
        }
        return 0.0;
    }

    // Higher protein while cutting to help preserve muscle; more carbs while
    // gaining to fuel training.
    static MacroSplit splitFor(Goal goal) {
        switch (goal) {
            case Goal::Lose:     return {40, 30, 30};
            case Goal::Maintain: return {30, 40, 30};
            case Goal::Gain:     return {30, 45, 25};
        }
        return {30, 40, 30};
    }

    static int gramsFor(double calories, int percent, double kcalPerGram) {
        return static_cast<int>(std::lround(calories * percent / 100.0 / kcalPerGram));
    }
};

// ---------------------------------------------------------------------------
// Input: prompts, parsing, and validation (re-prompts until valid)
// ---------------------------------------------------------------------------
class InputReader {
public:
    Sex readSex() {
        while (true) {
            const std::string s = lower(readLine("Sex (male/female): "));
            if (s == "m" || s == "male")   return Sex::Male;
            if (s == "f" || s == "female") return Sex::Female;
            std::cout << "  Please enter 'male' or 'female'.\n";
        }
    }

    Goal readGoal() {
        while (true) {
            const std::string s = lower(readLine("Goal (lose/maintain/gain): "));
            if (s == "lose" || s == "l")     return Goal::Lose;
            if (s == "maintain" || s == "m") return Goal::Maintain;
            if (s == "gain" || s == "g")     return Goal::Gain;
            std::cout << "  Please enter 'lose', 'maintain', or 'gain'.\n";
        }
    }

    double readNumber(const std::string& prompt, double min, double max,
                      bool wholeNumberOnly = false) {
        while (true) {
            std::istringstream in(readLine(prompt));
            double value = 0.0;
            const bool parsed = static_cast<bool>(in >> value) && (in >> std::ws).eof();
            const bool whole = !wholeNumberOnly || value == std::floor(value);

            if (parsed && whole && value >= min && value <= max) return value;

            std::cout << "  Please enter a " << (wholeNumberOnly ? "whole " : "")
                      << "number between " << min << " and " << max << ".\n";
        }
    }

private:
    std::string readLine(const std::string& prompt) {
        std::cout << prompt << std::flush;
        std::string line;
        if (!std::getline(std::cin, line)) {
            throw std::runtime_error("input ended before all values were entered");
        }
        return line;
    }

    static std::string lower(std::string s) {
        // Trim whitespace, then lowercase.
        const auto notSpace = [](unsigned char c) { return !std::isspace(c); };
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
        s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        return s;
    }
};

// ---------------------------------------------------------------------------
// Output: side-by-side comparison table
// ---------------------------------------------------------------------------
class Report {
public:
    static void print(const Person& person,
                      const std::vector<std::unique_ptr<BmrEquation>>& equations) {
        std::cout << "\n" << std::fixed << std::setprecision(1)
                  << "Profile: " << toString(person.sex()) << ", "
                  << person.ageYears() << " y, "
                  << person.heightIn() << " in (" << person.heightCm() << " cm), "
                  << person.weightLb() << " lb (" << person.weightKg() << " kg)\n"
                  << "Goal:    " << toString(person.goal()) << "\n\n";

        std::vector<double> bmrs;
        std::vector<MacroTargets> targets;
        for (const auto& eq : equations) {
            bmrs.push_back(eq->bmr(person));
            targets.push_back(MacroPlanner::plan(bmrs.back(), person.goal()));
        }

        constexpr int kLabelWidth = 24;
        constexpr int kColWidth = 18;

        std::cout << std::left << std::setw(kLabelWidth) << "";
        for (const auto& eq : equations) {
            std::cout << std::right << std::setw(kColWidth) << eq->name();
        }
        std::cout << "\n" << std::string(kLabelWidth + kColWidth * equations.size(), '-') << "\n";

        printRow("BMR (kcal/day)", kLabelWidth, kColWidth, bmrs.size(),
                 [&](size_t i) { return static_cast<int>(std::lround(bmrs[i])); });
        printRow("Target (kcal/day)", kLabelWidth, kColWidth, targets.size(),
                 [&](size_t i) { return targets[i].calories; });
        printRow("Protein (g)", kLabelWidth, kColWidth, targets.size(),
                 [&](size_t i) { return targets[i].proteinG; });
        printRow("Carbs (g)", kLabelWidth, kColWidth, targets.size(),
                 [&](size_t i) { return targets[i].carbsG; });
        printRow("Fat (g)", kLabelWidth, kColWidth, targets.size(),
                 [&](size_t i) { return targets[i].fatG; });

        const bool anyFloor = std::any_of(targets.begin(), targets.end(),
                                          [](const MacroTargets& t) { return t.floorApplied; });
        std::cout << "\nNotes:\n"
                  << "  - BMR only (no activity multiplier): this is what the body burns at rest.\n"
                  << "  - Lose/gain targets are -/+500 kcal/day (about 1 lb per week).\n";
        if (anyFloor) {
            std::cout << "  - Target raised to the " << constants::kMinimumDailyCalories
                      << " kcal/day safety minimum for at least one equation.\n";
        }
        std::cout << "  - Estimates only; consult a professional for medical or dietary advice.\n";
    }

private:
    template <typename ValueAt>
    static void printRow(const std::string& label, int labelWidth, int colWidth,
                         size_t columns, ValueAt valueAt) {
        std::cout << std::left << std::setw(labelWidth) << label << std::right;
        for (size_t i = 0; i < columns; ++i) std::cout << std::setw(colWidth) << valueAt(i);
        std::cout << "\n";
    }
};

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main() {
    try {
        InputReader in;
        std::cout << "=== BMR Macro Tracker ===\n";

        const Sex sex = in.readSex();
        const double heightIn = in.readNumber("Height (inches): ",
            constants::kMinHeightIn, constants::kMaxHeightIn);
        const double weightLb = in.readNumber("Weight (lbs): ",
            constants::kMinWeightLb, constants::kMaxWeightLb);
        const int age = static_cast<int>(in.readNumber("Age (years): ",
            constants::kMinAgeYears, constants::kMaxAgeYears, /*wholeNumberOnly=*/true));
        const Goal goal = in.readGoal();

        const Person person(sex, heightIn, weightLb, age, goal);

        std::vector<std::unique_ptr<BmrEquation>> equations;
        equations.push_back(std::make_unique<MifflinStJeor>());
        equations.push_back(std::make_unique<HarrisBenedict>());

        Report::print(person, equations);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\nError: " << e.what() << "\n";
        return 1;
    }
}