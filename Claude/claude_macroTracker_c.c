/*
 * macro_tracker.c
 *
 * Command-line macro tracker. Given sex, height (in), weight (lb), age (years)
 * and a goal (lose / maintain / gain), it estimates daily calorie and macro
 * targets from BMR using two standard equations, side by side:
 *
 *   - Mifflin-St Jeor        (mifflin_st_jeor_bmr)
 *   - Harris-Benedict, 1984  (harris_benedict_bmr)
 *
 * BMR only: no activity multiplier. No food data or food logging.
 *
 * Build: gcc -std=c11 -Wall -Wextra -pedantic -o claude_macroTracker_c.c -o macro_tracker
 * 
 * new -> gcc claude_macroTracker_c.c -o macroTracker_c
 * works fine
 */

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------ */
/* Constants                                                                */
/* ------------------------------------------------------------------------ */

static const double CM_PER_INCH = 2.54;
static const double KG_PER_LB = 0.45359237;

/* ~3500 kcal is the usual estimate for one pound of body weight, so a
 * 1 lb/week change is 3500 / 7 = 500 kcal per day. */
static const double KCAL_PER_POUND = 3500.0;
static const double DAYS_PER_WEEK = 7.0;
static const double TARGET_LB_PER_WEEK = 1.0;

/* Never recommend a deficit that lands below this many kcal/day. */
static const int MINIMUM_DAILY_CALORIES = 1200;

/* Energy density of each macronutrient (kcal per gram). */
static const double KCAL_PER_GRAM_PROTEIN = 4.0;
static const double KCAL_PER_GRAM_CARBS = 4.0;
static const double KCAL_PER_GRAM_FAT = 9.0;

/* Accepted input ranges (the equations are intended for adults). */
static const double MIN_HEIGHT_IN = 48.0, MAX_HEIGHT_IN = 90.0;
static const double MIN_WEIGHT_LB = 70.0, MAX_WEIGHT_LB = 700.0;
static const double MIN_AGE_YEARS = 18.0, MAX_AGE_YEARS = 100.0;

/* Report layout. */
#define LABEL_WIDTH 24
#define COLUMN_WIDTH 18
#define LINE_BUFFER_SIZE 128

/* ------------------------------------------------------------------------ */
/* Types                                                                    */
/* ------------------------------------------------------------------------ */

typedef enum { SEX_MALE, SEX_FEMALE } Sex;
typedef enum { GOAL_LOSE, GOAL_MAINTAIN, GOAL_GAIN } Goal;

/* A person, stored in the units the user typed (in / lb). */
typedef struct {
    Sex sex;
    double height_in;
    double weight_lb;
    int age_years;
    Goal goal;
} Person;

/* Percent of daily calories; must sum to 100. */
typedef struct {
    int protein_pct;
    int carbs_pct;
    int fat_pct;
} MacroSplit;

typedef struct {
    int calories;
    int protein_g;
    int carbs_g;
    int fat_g;
    int floor_applied; /* 1 if the minimum-calorie floor kicked in */
} MacroTargets;

/* A named BMR formula, so the report can loop over all of them. */
typedef double (*BmrFunction)(const Person *person);

typedef struct {
    const char *name;
    BmrFunction bmr;
} BmrEquation;

/* ------------------------------------------------------------------------ */
/* Unit conversion                                                          */
/* ------------------------------------------------------------------------ */

static double height_cm(const Person *p) { return p->height_in * CM_PER_INCH; }
static double weight_kg(const Person *p) { return p->weight_lb * KG_PER_LB; }

/* ------------------------------------------------------------------------ */
/* BMR equations (kcal/day), one function per formula                       */
/* ------------------------------------------------------------------------ */

/* Mifflin-St Jeor (1990):
 *   men:   10*kg + 6.25*cm - 5*age + 5
 *   women: 10*kg + 6.25*cm - 5*age - 161
 */
static double mifflin_st_jeor_bmr(const Person *p) {
    const double sex_offset = (p->sex == SEX_MALE) ? 5.0 : -161.0;
    return 10.0 * weight_kg(p)
         + 6.25 * height_cm(p)
         - 5.0 * p->age_years
         + sex_offset;
}

/* Harris-Benedict, as revised by Roza & Shizgal (1984):
 *   men:   88.362  + 13.397*kg + 4.799*cm - 5.677*age
 *   women: 447.593 +  9.247*kg + 3.098*cm - 4.330*age
 */
static double harris_benedict_bmr(const Person *p) {
    if (p->sex == SEX_MALE) {
        return 88.362 + 13.397 * weight_kg(p)
             + 4.799 * height_cm(p) - 5.677 * p->age_years;
    }
    return 447.593 + 9.247 * weight_kg(p)
         + 3.098 * height_cm(p) - 4.330 * p->age_years;
}

/* ------------------------------------------------------------------------ */
/* Calorie and macro planning                                               */
/* ------------------------------------------------------------------------ */

/* +/- 500 kcal/day == roughly 1 lb per week. */
static double daily_calorie_adjustment(Goal goal) {
    const double per_day = TARGET_LB_PER_WEEK * KCAL_PER_POUND / DAYS_PER_WEEK;
    switch (goal) {
        case GOAL_LOSE: return -per_day;
        case GOAL_GAIN: return per_day;
        case GOAL_MAINTAIN: break;
    }
    return 0.0;
}

/* Higher protein while cutting to help preserve muscle; more carbs while
 * gaining to fuel training. */
static MacroSplit macro_split_for(Goal goal) {
    switch (goal) {
        case GOAL_LOSE:     return (MacroSplit){40, 30, 30};
        case GOAL_MAINTAIN: return (MacroSplit){30, 40, 30};
        case GOAL_GAIN:     return (MacroSplit){30, 45, 25};
    }
    return (MacroSplit){30, 40, 30};
}

static int grams_for(double calories, int percent, double kcal_per_gram) {
    return (int)lround(calories * percent / 100.0 / kcal_per_gram);
}

/* Turns a BMR and a goal into calorie and macro targets. */
static MacroTargets plan_macros(double bmr, Goal goal) {
    MacroTargets t;
    const double wanted = bmr + daily_calorie_adjustment(goal);
    const double calories =
        (wanted < MINIMUM_DAILY_CALORIES) ? MINIMUM_DAILY_CALORIES : wanted;
    const MacroSplit split = macro_split_for(goal);

    t.floor_applied = wanted < MINIMUM_DAILY_CALORIES;
    t.calories = (int)lround(calories);
    t.protein_g = grams_for(calories, split.protein_pct, KCAL_PER_GRAM_PROTEIN);
    t.carbs_g = grams_for(calories, split.carbs_pct, KCAL_PER_GRAM_CARBS);
    t.fat_g = grams_for(calories, split.fat_pct, KCAL_PER_GRAM_FAT);
    return t;
}

/* ------------------------------------------------------------------------ */
/* Input: prompts, parsing, and validation (re-prompts until valid)         */
/* ------------------------------------------------------------------------ */

/* Trims surrounding whitespace and lowercases in place. */
static void normalize(char *s) {
    char *start = s;
    while (isspace((unsigned char)*start)) start++;

    size_t len = strlen(start);
    while (len > 0 && isspace((unsigned char)start[len - 1])) len--;

    memmove(s, start, len);
    s[len] = '\0';
    for (size_t i = 0; i < len; i++) s[i] = (char)tolower((unsigned char)s[i]);
}

/* Prompts and reads one line. Exits with an error if input has ended. */
static void read_line(const char *prompt, char *buf, size_t size) {
    fputs(prompt, stdout);
    fflush(stdout);
    if (fgets(buf, (int)size, stdin) == NULL) {
        fprintf(stderr, "\nError: input ended before all values were entered\n");
        exit(1);
    }
}

static Sex read_sex(void) {
    char line[LINE_BUFFER_SIZE];
    for (;;) {
        read_line("Sex (male/female): ", line, sizeof line);
        normalize(line);
        if (strcmp(line, "m") == 0 || strcmp(line, "male") == 0) return SEX_MALE;
        if (strcmp(line, "f") == 0 || strcmp(line, "female") == 0) return SEX_FEMALE;
        puts("  Please enter 'male' or 'female'.");
    }
}

static Goal read_goal(void) {
    char line[LINE_BUFFER_SIZE];
    for (;;) {
        read_line("Goal (lose/maintain/gain): ", line, sizeof line);
        normalize(line);
        if (strcmp(line, "l") == 0 || strcmp(line, "lose") == 0) return GOAL_LOSE;
        if (strcmp(line, "m") == 0 || strcmp(line, "maintain") == 0) return GOAL_MAINTAIN;
        if (strcmp(line, "g") == 0 || strcmp(line, "gain") == 0) return GOAL_GAIN;
        puts("  Please enter 'lose', 'maintain', or 'gain'.");
    }
}

static double read_number(const char *prompt, double min, double max, int whole_only) {
    char line[LINE_BUFFER_SIZE];
    for (;;) {
        read_line(prompt, line, sizeof line);

        char *end = NULL;
        const double value = strtod(line, &end);
        while (end != NULL && isspace((unsigned char)*end)) end++;

        const int parsed = (end != line) && (*end == '\0') && isfinite(value);
        const int whole = !whole_only || value == floor(value);

        if (parsed && whole && value >= min && value <= max) return value;

        printf("  Please enter a %snumber between %g and %g.\n",
               whole_only ? "whole " : "", min, max);
    }
}

static Person read_person(void) {
    Person p;
    p.sex = read_sex();
    p.height_in = read_number("Height (inches): ", MIN_HEIGHT_IN, MAX_HEIGHT_IN, 0);
    p.weight_lb = read_number("Weight (lbs): ", MIN_WEIGHT_LB, MAX_WEIGHT_LB, 0);
    p.age_years = (int)read_number("Age (years): ", MIN_AGE_YEARS, MAX_AGE_YEARS, 1);
    p.goal = read_goal();
    return p;
}

/* ------------------------------------------------------------------------ */
/* Output: side-by-side comparison table                                    */
/* ------------------------------------------------------------------------ */

static const char *sex_name(Sex sex) { return sex == SEX_MALE ? "male" : "female"; }

static const char *goal_name(Goal goal) {
    switch (goal) {
        case GOAL_LOSE:     return "lose";
        case GOAL_MAINTAIN: return "maintain";
        case GOAL_GAIN:     return "gain";
    }
    return "";
}

static void print_row(const char *label, const int *values, size_t count) {
    printf("%-*s", LABEL_WIDTH, label);
    for (size_t i = 0; i < count; i++) printf("%*d", COLUMN_WIDTH, values[i]);
    putchar('\n');
}

static void print_report(const Person *p, const BmrEquation *equations, size_t count) {
    int bmr[8], calories[8], protein[8], carbs[8], fat[8];
    int any_floor = 0;

    printf("\nProfile: %s, %d y, %.1f in (%.1f cm), %.1f lb (%.1f kg)\n",
           sex_name(p->sex), p->age_years, p->height_in, height_cm(p),
           p->weight_lb, weight_kg(p));
    printf("Goal:    %s\n\n", goal_name(p->goal));

    for (size_t i = 0; i < count; i++) {
        const double raw_bmr = equations[i].bmr(p);
        const MacroTargets t = plan_macros(raw_bmr, p->goal);
        bmr[i] = (int)lround(raw_bmr);
        calories[i] = t.calories;
        protein[i] = t.protein_g;
        carbs[i] = t.carbs_g;
        fat[i] = t.fat_g;
        if (t.floor_applied) any_floor = 1;
    }

    printf("%-*s", LABEL_WIDTH, "");
    for (size_t i = 0; i < count; i++) printf("%*s", COLUMN_WIDTH, equations[i].name);
    putchar('\n');
    for (size_t i = 0; i < LABEL_WIDTH + COLUMN_WIDTH * count; i++) putchar('-');
    putchar('\n');

    print_row("BMR (kcal/day)", bmr, count);
    print_row("Target (kcal/day)", calories, count);
    print_row("Protein (g)", protein, count);
    print_row("Carbs (g)", carbs, count);
    print_row("Fat (g)", fat, count);

    puts("\nNotes:");
    puts("  - BMR only (no activity multiplier): this is what the body burns at rest.");
    puts("  - Lose/gain targets are -/+500 kcal/day (about 1 lb per week).");
    if (any_floor) {
        printf("  - Target raised to the %d kcal/day safety minimum for at least one equation.\n",
               MINIMUM_DAILY_CALORIES);
    }
    puts("  - Estimates only; consult a professional for medical or dietary advice.");
}

/* ------------------------------------------------------------------------ */
/* Main                                                                     */
/* ------------------------------------------------------------------------ */

int main(void) {
    const BmrEquation equations[] = {
        {"Mifflin-St Jeor", mifflin_st_jeor_bmr},
        {"Harris-Benedict", harris_benedict_bmr},
    };
    const size_t equation_count = sizeof equations / sizeof equations[0];

    puts("=== BMR Macro Tracker ===");
    const Person person = read_person();
    print_report(&person, equations, equation_count);
    return 0;
}