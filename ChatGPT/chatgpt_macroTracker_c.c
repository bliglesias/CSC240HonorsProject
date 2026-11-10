#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    SEX_MALE,
    SEX_FEMALE
} Sex;

typedef enum {
    GOAL_LOSE,
    GOAL_MAINTAIN,
    GOAL_GAIN
} Goal;

typedef struct {
    Sex sex;
    double height_inches;
    double weight_pounds;
    int age_years;
    Goal goal;
} Person;

typedef struct {
    double bmr;
    double daily_calories;
    double protein_grams;
    double carbs_grams;
    double fat_grams;
} Targets;

#define PROTEIN_SHARE 0.30
#define CARBS_SHARE   0.40
#define FAT_SHARE     0.30

static double inches_to_centimeters(double inches) {
    return inches * 2.54;
}

static double pounds_to_kilograms(double pounds) {
    return pounds * 0.45359237;
}

/* Mifflin-St Jeor equation. Returns BMR in kcal/day. */
static double mifflin_st_jeor_bmr(const Person *person) {
    const double weight_kg = pounds_to_kilograms(person->weight_pounds);
    const double height_cm = inches_to_centimeters(person->height_inches);
    const double age = (double)person->age_years;

    if (person->sex == SEX_MALE) {
        return 10.0 * weight_kg + 6.25 * height_cm - 5.0 * age + 5.0;
    }
    return 10.0 * weight_kg + 6.25 * height_cm - 5.0 * age - 161.0;
}

/* Revised Harris-Benedict equation. Returns BMR in kcal/day. */
static double harris_benedict_bmr(const Person *person) {
    const double weight_kg = pounds_to_kilograms(person->weight_pounds);
    const double height_cm = inches_to_centimeters(person->height_inches);
    const double age = (double)person->age_years;

    if (person->sex == SEX_MALE) {
        return 88.362 + 13.397 * weight_kg + 4.799 * height_cm
               - 5.677 * age;
    }
    return 447.593 + 9.247 * weight_kg + 3.098 * height_cm
           - 4.330 * age;
}

static double goal_adjustment(Goal goal) {
    switch (goal) {
        case GOAL_LOSE: return -500.0;
        case GOAL_MAINTAIN: return 0.0;
        case GOAL_GAIN: return 500.0;
    }
    return 0.0;
}

static Targets calculate_targets(double bmr, Goal goal) {
    /* BMR only: intentionally no activity multiplier. */
    double calories = bmr + goal_adjustment(goal);
    double protein_calories, carbs_calories, fat_calories;
    Targets result;

    if (calories < 0.0) {
        calories = 0.0;
    }

    protein_calories = calories * PROTEIN_SHARE;
    carbs_calories = calories * CARBS_SHARE;
    fat_calories = calories * FAT_SHARE;

    result.bmr = bmr;
    result.daily_calories = calories;
    result.protein_grams = protein_calories / 4.0;
    result.carbs_grams = carbs_calories / 4.0;
    result.fat_grams = fat_calories / 9.0;
    return result;
}

static int read_line(const char *prompt, char *buffer, size_t size) {
    size_t length;
    int ch;

    printf("%s", prompt);
    if (fgets(buffer, (int)size, stdin) == NULL) {
        return 0;
    }

    length = strlen(buffer);
    if (length > 0 && buffer[length - 1] == '\n') {
        buffer[length - 1] = '\0';
    } else {
        /* Reject an input line that did not fit in the buffer. */
        while ((ch = getchar()) != '\n' && ch != EOF) {
            /* Discard remainder of overlong line. */
        }
        return 0;
    }

    return 1;
}

static void trim_and_lower(char *text) {
    size_t start = 0;
    size_t end = strlen(text);
    size_t i;

    while (isspace((unsigned char)text[start])) {
        start++;
    }
    while (end > start && isspace((unsigned char)text[end - 1])) {
        end--;
    }

    if (start > 0) {
        memmove(text, text + start, end - start);
    }
    text[end - start] = '\0';

    for (i = 0; text[i] != '\0'; i++) {
        text[i] = (char)tolower((unsigned char)text[i]);
    }
}

static int read_number(const char *prompt, double minimum, double maximum,
                       double *result) {
    char input[128];
    char *end_pointer;
    double value;

    for (;;) {
        if (!read_line(prompt, input, sizeof(input))) {
            fprintf(stderr, "\nInput ended or line was too long.\n");
            return 0;
        }

        trim_and_lower(input);
        if (input[0] == '\0') {
            printf("Please enter a number from %.0f to %.0f.\n",
                   minimum, maximum);
            continue;
        }

        value = strtod(input, &end_pointer);
        while (isspace((unsigned char)*end_pointer)) {
            end_pointer++;
        }

        if (end_pointer != input && *end_pointer == '\0'
            && value >= minimum && value <= maximum) {
            *result = value;
            return 1;
        }

        printf("Please enter a number from %.0f to %.0f.\n",
               minimum, maximum);
    }
}

static int read_age(int *age) {
    double value;
    if (!read_number("Age (whole years, 18-120): ", 18.0, 120.0, &value)) {
        return 0;
    }

    while (value != (int)value) {
        printf("Age must be a whole number.\n");
        if (!read_number("Age (whole years, 18-120): ", 18.0, 120.0, &value)) {
            return 0;
        }
    }

    *age = (int)value;
    return 1;
}

static int read_sex(Sex *sex) {
    char input[128];

    for (;;) {
        if (!read_line("Sex for BMR equation (male/female): ",
                       input, sizeof(input))) {
            fprintf(stderr, "\nInput ended or line was too long.\n");
            return 0;
        }
        trim_and_lower(input);

        if (strcmp(input, "male") == 0 || strcmp(input, "m") == 0) {
            *sex = SEX_MALE;
            return 1;
        }
        if (strcmp(input, "female") == 0 || strcmp(input, "f") == 0) {
            *sex = SEX_FEMALE;
            return 1;
        }
        printf("Please enter male or female.\n");
    }
}

static int read_goal(Goal *goal) {
    char input[128];

    for (;;) {
        if (!read_line("Goal (lose/maintain/gain): ", input, sizeof(input))) {
            fprintf(stderr, "\nInput ended or line was too long.\n");
            return 0;
        }
        trim_and_lower(input);

        if (strcmp(input, "lose") == 0) {
            *goal = GOAL_LOSE;
            return 1;
        }
        if (strcmp(input, "maintain") == 0) {
            *goal = GOAL_MAINTAIN;
            return 1;
        }
        if (strcmp(input, "gain") == 0) {
            *goal = GOAL_GAIN;
            return 1;
        }
        printf("Please enter lose, maintain, or gain.\n");
    }
}

static void print_row(const char *label, double first, double second) {
    printf("%-29s %18.1f %22.1f\n", label, first, second);
}

static int run_tracker(void) {
    Person person;
    Targets mifflin;
    Targets harris;
    double value;
    double adjustment;

    if (!read_sex(&person.sex)) {
        return 1;
    }

    if (!read_number("Height (inches, 36-96): ", 36.0, 96.0, &value)) {
        return 1;
    }
    person.height_inches = value;

    if (!read_number("Weight (lbs, 50-700): ", 50.0, 700.0, &value)) {
        return 1;
    }
    person.weight_pounds = value;

    if (!read_age(&person.age_years)) {
        return 1;
    }
    if (!read_goal(&person.goal)) {
        return 1;
    }

    mifflin = calculate_targets(mifflin_st_jeor_bmr(&person), person.goal);
    harris = calculate_targets(harris_benedict_bmr(&person), person.goal);
    adjustment = goal_adjustment(person.goal);

    printf("\nDaily estimates (BMR only; no activity multiplier)\n");
    printf("Goal adjustment: %+.0f kcal/day\n\n", adjustment);
    printf("%-29s %18s %22s\n", "Metric", "Mifflin-St Jeor", "Harris-Benedict");
    printf("---------------------------------------------------------------------\n");
    print_row("BMR (kcal/day)", mifflin.bmr, harris.bmr);
    print_row("Target calories/day", mifflin.daily_calories, harris.daily_calories);
    print_row("Protein (g/day)", mifflin.protein_grams, harris.protein_grams);
    print_row("Carbs (g/day)", mifflin.carbs_grams, harris.carbs_grams);
    print_row("Fat (g/day)", mifflin.fat_grams, harris.fat_grams);

    printf("\nMacro split: 30%% protein / 40%% carbs / 30%% fat.\n");
    printf("A 500 kcal/day adjustment approximates 1 lb/week; actual results vary.\n");
    printf("BMR is not the same as total daily energy expenditure.\n");
    return 0;
}

int main(void) {
    return run_tracker();
}
