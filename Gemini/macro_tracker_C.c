#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Enumerations for structured categorization
enum Sex { SEX_MALE, SEX_FEMALE };
enum Goal { GOAL_LOSE, GOAL_MAINTAIN, GOAL_GAIN };

// User profile data structure
struct UserProfile {
    enum Sex sex;
    double heightInches;
    double weightLbs;
    int age;
    enum Goal goal;
};

// Result structure for calorie and macro breakdowns
struct PlanResult {
    double bmr;
    double targetCalories;
    double proteinGrams;
    double carbGrams;
    double fatGrams;
};

// Internal unit conversion helpers
double getHeightCm(double inches) {
    return inches * 2.54;
}

double getWeightKg(double lbs) {
    return lbs / 2.20462262;
}

// Generates target calories and macro split based on BMR and goal
struct PlanResult generatePlan(double bmr, enum Goal goal) {
    double targetCalories = bmr;
    
    // Apply ~1 lb/week adjustment (approx. 500 kcal/day)
    if (goal == GOAL_LOSE) {
        targetCalories -= 500.0;
    } else if (goal == GOAL_GAIN) {
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

    struct PlanResult result;
    result.bmr = bmr;
    result.targetCalories = targetCalories;
    result.proteinGrams = proteinCals / 4.0;
    result.carbGrams    = carbCals / 4.0;
    result.fatGrams     = fatCals / 9.0;

    return result;
}

/**
 * @brief Estimates BMR using the Mifflin-St Jeor Equation.
 */
struct PlanResult calculateMifflinStJeor(struct UserProfile profile) {
    double w = getWeightKg(profile.weightLbs);
    double h = getHeightCm(profile.heightInches);
    double bmr = (10.0 * w) + (6.25 * h) - (5.0 * profile.age);

    if (profile.sex == SEX_MALE) {
        bmr += 5.0;
    } else {
        bmr -= 161.0;
    }

    return generatePlan(bmr, profile.goal);
}

/**
 * @brief Estimates BMR using the Revised Harris-Benedict Equation.
 */
struct PlanResult calculateHarrisBenedict(struct UserProfile profile) {
    double w = getWeightKg(profile.weightLbs);
    double h = getHeightCm(profile.heightInches);
    double bmr = 0.0;

    if (profile.sex == SEX_MALE) {
        bmr = 88.362 + (13.397 * w) + (4.799 * h) - (5.677 * profile.age);
    } else {
        bmr = 447.593 + (9.247 * w) + (3.098 * h) - (4.330 * profile.age);
    }

    return generatePlan(bmr, profile.goal);
}

// Input validation helpers
enum Sex promptSex() {
    char input[10];
    while (1) {
        printf("Enter sex (M/F): ");
        if (scanf("%9s", input) == 1) {
            if (strcasecmp(input, "M") == 0 || strcasecmp(input, "Male") == 0) return SEX_MALE;
            if (strcasecmp(input, "F") == 0 || strcasecmp(input, "Female") == 0) return SEX_FEMALE;
        }
        printf("Invalid input. Please enter 'M' or 'F'.\n");
    }
}

enum Goal promptGoal() {
    char input[10];
    while (1) {
        printf("Enter goal ([l]ose, [m]aintain, [g]ain): ");
        if (scanf("%9s", input) == 1) {
            if (strcasecmp(input, "l") == 0 || strcasecmp(input, "lose") == 0) return GOAL_LOSE;
            if (strcasecmp(input, "m") == 0 || strcasecmp(input, "maintain") == 0) return GOAL_MAINTAIN;
            if (strcasecmp(input, "g") == 0 || strcasecmp(input, "gain") == 0) return GOAL_GAIN;
        }
        printf("Invalid input. Please choose lose, maintain, or gain.\n");
    }
}

double promptPositiveDouble(const char* prompt) {
    double value;
    while (1) {
        printf("%s", prompt);
        if (scanf("%lf", &value) == 1 && value > 0) {
            return value;
        }
        printf("Invalid input. Please enter a positive number.\n");
        // Clear input buffer
        while (getchar() != '\n');
    }
}

void printRow(const char* label, double val1, double val2) {
    printf("%-24s%-18.1f%-18.1f\n", label, val1, val2);
}

int main() {
    printf("===========================================\n");
    printf("       COMMAND-LINE MACRO TRACKER (C)      \n");
    printf("===========================================\n\n");

    struct UserProfile profile;
    profile.sex = promptSex();
    profile.heightInches = promptPositiveDouble("Enter height (in inches): ");
    profile.weightLbs = promptPositiveDouble("Enter weight (lbs): ");
    profile.age = (int)promptPositiveDouble("Enter age (years): ");
    profile.goal = promptGoal();

    struct PlanResult mifflin = calculateMifflinStJeor(profile);
    struct PlanResult harris = calculateHarrisBenedict(profile);

    printf("\n-------------------------------------------\n");
    printf("               RESULTS SUMMARY             \n");
    printf("-------------------------------------------\n");
    
    printf("Goal Target:   ");
    if (profile.goal == GOAL_LOSE) printf("Lose ~1 lb/week (-500 kcal deficit)\n");
    else if (profile.goal == GOAL_MAINTAIN) printf("Maintain weight\n");
    else printf("Gain ~1 lb/week (+500 kcal surplus)\n");
    printf("\n");

    printf("%-24s%-18s%-18s\n", "Metric", "Mifflin-St Jeor", "Harris-Benedict");
    printf("----------------------------------------------------\n");
    
    printRow("BMR (kcal)", mifflin.bmr, harris.bmr);
    printRow("Target Calories", mifflin.targetCalories, harris.targetCalories);
    printRow("Protein (g)", mifflin.proteinGrams, harris.proteinGrams);
    printRow("Carbs (g)", mifflin.carbGrams, harris.carbGrams);
    printRow("Fat (g)", mifflin.fatGrams, harris.fatGrams);
    printf("================================================----\n");

    return 0;
}