/*
TypingTrainer - console typing practice with persistent stats and mistake analysis
Single-file C program (C11). No external libraries required.

Compile (Linux / Cygwin / WSL / macOS):
    gcc -std=c11 typing_trainer.c -o typing_trainer

If your system lacks gettimeofday (e.g., some MSVC Windows builds), replace timing code:
- Use clock() and CLOCKS_PER_SEC or QueryPerformanceCounter on Windows.
- If you need a Windows port, tell me and I'll add a small compatibility shim.

Files created/used (in working directory):
- stats.txt             : append-only session stats (CSV)
- mistakes_words.txt    : "word count" pairs
- mistakes_chars.txt    : "char count" pairs
*/

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>   // gettimeofday()
#include <ctype.h>
#include <time.h>

#define STATS_FILE "stats.txt"
#define MWORDS_FILE "mistakes_words.txt"
#define MCHARS_FILE "mistakes_chars.txt"

#define WORDS_LIST_SIZE 200
#define MAX_LINE 512
#define TOP_N 10

/* ----------------------
   Simple built-in word and sentence banks.
   You can extend these arrays or load from a file.
   ---------------------- */
static const char *word_bank[] = {
    "the","quick","brown","fox","jumps","over","lazy","dog","keyboard","practice",
    "function","variable","pointer","memory","array","string","compile","debug",
    "project","program","structure","coding","computer","process","thread",
    "input","output","speed","accuracy","challenge","training","exercise",
    "typist","development","education","system","design","language","data",
    "persistent","statistics","analysis","improve","learning","interface",
    "session","record","history","mistake","correct","wrong","practice"
};
static const size_t word_bank_count = sizeof(word_bank)/sizeof(word_bank[0]);

static const char *sentence_bank[] = {
    "The quick brown fox jumps over the lazy dog.",
    "Practice makes progress and consistent effort brings improvement.",
    "Typing fast requires accuracy before speed will follow.",
    "C programming teaches careful thinking about memory and behavior.",
    "Focus on home row, keep your fingers relaxed and eyes on the screen."
};
static const size_t sentence_bank_count = sizeof(sentence_bank)/sizeof(sentence_bank[0]);

/* ----------------------
   Simple dynamic maps for mistakes (word -> count, char -> count)
   ---------------------- */
typedef struct {
    char *key;
    long count;
} KeyCount;

typedef struct {
    KeyCount *items;
    size_t n;
    size_t cap;
} Map;

static void map_init(Map *m) {
    m->items = NULL; m->n = 0; m->cap = 0;
}
static void map_free(Map *m) {
    for (size_t i = 0; i < m->n; ++i) free(m->items[i].key);
    free(m->items);
    m->items = NULL; m->n = m->cap = 0;
}
static void map_add(Map *m, const char *key, long delta) {
    if (!key) return;
    for (size_t i = 0; i < m->n; ++i) {
        if (strcmp(m->items[i].key, key) == 0) { m->items[i].count += delta; return; }
    }
    if (m->n == m->cap) {
        size_t newcap = (m->cap == 0) ? 8 : m->cap * 2;
        KeyCount *tmp = realloc(m->items, newcap * sizeof(KeyCount));
        if (!tmp) { perror("realloc"); exit(1); }
        m->items = tmp; m->cap = newcap;
    }
    m->items[m->n].key = strdup(key);
    m->items[m->n].count = delta;
    m->n++;
}
static void map_add_char(Map *m, char ch, long delta) {
    char buf[2] = { (char)ch, '\0' };
    map_add(m, buf, delta);
}
static int cmp_kc_desc(const void *a, const void *b) {
    const KeyCount *A = a, *B = b;
    if (B->count < A->count) return -1;
    if (B->count > A->count) return 1;
    return strcmp(A->key, B->key);
}

/* ----------------------
   File IO for maps (simple text format: key TAB count newline)
   ---------------------- */
static void load_map_from_file(Map *m, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) return; // no file yet
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        char *tab = strchr(line, '\t');
        if (!tab) continue;
        *tab = '\0';
        char *key = line;
        char *num = tab + 1;
        long cnt = atol(num);
        if (cnt != 0) map_add(m, key, cnt);
    }
    fclose(f);
}
static void save_map_to_file(Map *m, const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) { perror("fopen"); return; }
    for (size_t i = 0; i < m->n; ++i) {
        fprintf(f, "%s\t%ld\n", m->items[i].key, m->items[i].count);
    }
    fclose(f);
}

/* ----------------------
   Stats persistence: simple CSV rows: date_iso,wpm,accuracy_percent,ch_count
   ---------------------- */
static void append_session_stats(double wpm, double accuracy, long chars) {
    FILE *f = fopen(STATS_FILE, "a");
    if (!f) { perror("fopen stats"); return; }
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", tm);
    fprintf(f, "%s,%.2f,%.2f,%ld\n", buf, wpm, accuracy, chars);
    fclose(f);
}

static void compute_aggregate_stats(double *avg_wpm, double *best_wpm, double *avg_acc, size_t *sessions) {
    *avg_wpm = *best_wpm = *avg_acc = 0.0;
    *sessions = 0;
    FILE *f = fopen(STATS_FILE, "r");
    if (!f) return;
    double wpm, acc;
    long ch;
    while (fscanf(f, "%*[^,],%lf,%lf,%ld\n", &wpm, &acc, &ch) == 3) {
        (*sessions)++;
        *avg_wpm += wpm;
        *avg_acc += acc;
        if (wpm > *best_wpm) *best_wpm = wpm;
    }
    fclose(f);
    if (*sessions > 0) {
        *avg_wpm /= (double)(*sessions);
        *avg_acc /= (double)(*sessions);
    }
}

/* ----------------------
   Utility: trim newline, lower-case
   ---------------------- */
static void trim_newline(char *s) {
    if (!s) return;
    size_t l = strlen(s);
    if (l == 0) return;
    if (s[l-1] == '\n') s[l-1] = '\0';
    if (l >= 2 && s[l-2] == '\r') s[l-2] = '\0'; // windows CRLF safety
}
static void to_lower_inplace(char *s) {
    for (size_t i = 0; s && s[i]; ++i) s[i] = (char)tolower((unsigned char)s[i]);
}

/* ----------------------
   Timing helper
   ---------------------- */
static double elapsed_seconds(struct timeval start, struct timeval end) {
    double s = (double)(end.tv_sec - start.tv_sec);
    double us = ((double)(end.tv_usec - start.tv_usec)) / 1e6;
    return s + us;
}

/* ----------------------
   Typing comparison: returns correct character count and populates errors
   For words: we treat entire token vs reference
   ---------------------- */
typedef struct {
    size_t correct_chars;
    size_t total_chars;
    size_t correct_words;
    size_t total_words;
} CompareResult;

/* Compare reference and typed; update word/char mistake maps */
static CompareResult compare_and_update(const char *ref, const char *typed, Map *mwords, Map *mchars) {
    CompareResult res = {0, 0, 0, 0};
    if (!ref) ref = "";
    if (!typed) typed = "";
    size_t rlen = strlen(ref);
    size_t tlen = strlen(typed);
    res.total_chars = rlen;
    size_t minlen = (rlen < tlen) ? rlen : tlen;
    for (size_t i = 0; i < minlen; ++i) {
        if (typed[i] == ref[i]) res.correct_chars++;
        else {
            // record char mistake: the target char is ref[i]
            map_add_char(mchars, (char)ref[i], 1);
        }
    }
    // characters beyond minlen are mistakes (missing or extra)
    if (tlen < rlen) {
        // missing characters
        for (size_t i = tlen; i < rlen; ++i) map_add_char(mchars, ref[i], 1);
    } else if (tlen > rlen) {
        // extra characters typed: we can count them as mistakes against a special marker
        // but we'll record them as mistakes for the typed character (lowercase)
        for (size_t i = rlen; i < tlen; ++i) map_add_char(mchars, typed[i], 1);
    }
    // Word-level compare: split by spaces in ref and typed
    // We'll do a simple tokenization; for word tests ref is a single word anyway.
    char ref_copy[MAX_LINE]; strncpy(ref_copy, ref, MAX_LINE-1); ref_copy[MAX_LINE-1]=0;
    char typed_copy[MAX_LINE]; strncpy(typed_copy, typed, MAX_LINE-1); typed_copy[MAX_LINE-1]=0;
    char *rptr = ref_copy, *tptr = typed_copy;
    char *rtok, *ttok;
    while ( (rtok = strtok_r(rptr, " \t", &rptr)) != NULL ) {
        ttok = strtok_r(tptr, " \t", &tptr);
        res.total_words++;
        if (ttok && strcmp(rtok, ttok) == 0) {
            res.correct_words++;
        } else {
            // record word mistake (target word)
            map_add(mwords, rtok, 1);
        }
    }
    // any remaining typed tokens beyond ref considered wrong; can count but not needed
    return res;
}

/* ----------------------
   Simple random pick helpers
   ---------------------- */
static int randint(int a, int b) { // inclusive
    return a + rand() % (b - a + 1);
}

/* ----------------------
   UI / Menu / Practice loops
   ---------------------- */
static void show_top_map(Map *m, int n) {
    if (m->n == 0) { printf("  (none)\n"); return; }
    // make a copy and sort
    KeyCount *copy = malloc(m->n * sizeof(KeyCount));
    for (size_t i = 0; i < m->n; ++i) copy[i] = m->items[i];
    qsort(copy, m->n, sizeof(KeyCount), cmp_kc_desc);
    int limit = (n < (int)m->n) ? n : (int)m->n;
    for (int i = 0; i < limit; ++i) {
        printf("  %d) %-12s : %ld\n", i+1, copy[i].key, copy[i].count);
    }
    free(copy);
}

static void view_statistics(Map *mwords, Map *mchars) {
    double avg_wpm, best_wpm, avg_acc;
    size_t sessions;
    compute_aggregate_stats(&avg_wpm, &best_wpm, &avg_acc, &sessions);
    printf("\n=== Statistics ===\n");
    printf("Sessions recorded: %zu\n", sessions);
    if (sessions > 0) {
        printf("Average WPM: %.2f\n", avg_wpm);
        printf("Best WPM   : %.2f\n", best_wpm);
        printf("Average Accuracy: %.2f%%\n", avg_acc);
    }
    printf("\nTop mistyped words:\n"); show_top_map(mwords, TOP_N);
    printf("\nTop mistyped characters:\n"); show_top_map(mchars, TOP_N);
    printf("=====================\n\n");
}

/* Read a full line from stdin safely; returns malloc'd string which caller must free */
static char *read_line(void) {
    char *line = NULL;
    size_t cap = 0;
    ssize_t len = getline(&line, &cap, stdin);
    if (len < 0) {
        free(line); return NULL;
    }
    trim_newline(line);
    return line;
}

/* Practice session: either words or sentences */
static void start_practice(Map *mwords, Map *mchars) {
    printf("\nStart Practice\n");
    printf("1) Word practice\n2) Sentence practice\nEnter choice: ");
    char *choice = read_line();
    if (!choice) return;
    int mode = atoi(choice);
    free(choice);
    if (mode != 1 && mode != 2) { printf("Invalid choice.\n"); return; }

    printf("How many items in this session? (e.g. 10): ");
    char *nstr = read_line(); if (!nstr) return;
    int n = atoi(nstr); free(nstr);
    if (n <= 0) n = 10;

    size_t total_chars_typed = 0;
    size_t total_correct_chars = 0;
    size_t total_words = 0;
    size_t total_correct_words = 0;
    double total_seconds = 0.0;

    for (int i = 0; i < n; ++i) {
        const char *ref;
        if (mode == 1) {
            ref = word_bank[randint(0, (int)word_bank_count-1)];
        } else {
            ref = sentence_bank[randint(0, (int)sentence_bank_count-1)];
        }
        printf("\nItem %d/%d:\n%s\n", i+1, n, ref);
        printf("Press ENTER when ready to start...");
        // wait for enter
        char *tmp = read_line(); if (tmp) { free(tmp); }
        printf("Type it and press ENTER when done:\n> ");

        struct timeval start, end;
        gettimeofday(&start, NULL);
        char *typed = read_line();
        gettimeofday(&end, NULL);
        if (!typed) typed = strdup("");
        double secs = elapsed_seconds(start, end);
        total_seconds += secs;

        CompareResult cres = compare_and_update(ref, typed, mwords, mchars);
        total_chars_typed += strlen(typed);
        total_correct_chars += cres.correct_chars;
        total_words += cres.total_words;
        total_correct_words += cres.correct_words;

        double minutes = secs / 60.0;
        double gross_wpm = 0.0;
        if (minutes > 0.0) gross_wpm = ((double)strlen(typed) / 5.0) / minutes;
        double accuracy = (strlen(typed) > 0) ? ((double)cres.correct_chars / (double)strlen(typed) * 100.0) : 0.0;

        printf("\nResult for item %d:\n", i+1);
        printf("  Time: %.2fs  Chars typed: %zu  Accuracy: %.2f%%  WPM (gross): %.2f\n",
               secs, strlen(typed), accuracy, gross_wpm);

        if (cres.total_words > 0) {
            printf("  Words correct: %zu / %zu\n", cres.correct_words, cres.total_words);
        }
        // Show character-level mismatches (simple)
        if (cres.correct_chars < cres.total_chars) {
            printf("  Mismatches (reference -> typed) at positions:\n");
            size_t rlen = strlen(ref ? ref : "");
            size_t tlen = strlen(typed);
            size_t maxp = (rlen > tlen) ? rlen : tlen;
            for (size_t p = 0; p < maxp; ++p) {
                char rc = (p < rlen) ? ref[p] : '?';
                char tc = (p < tlen) ? typed[p] : '?';
                if (rc != tc) {
                    char rs[3] = { (isprint((unsigned char)rc) ? rc : '?'), '\0', '\0'};
                    char ts[3] = { (isprint((unsigned char)tc) ? tc : '?'), '\0', '\0'};
                    printf("   pos %zu: '%s' -> '%s'\n", p+1, rs, ts);
                }
            }
        } else {
            printf("  Perfect for this item!\n");
        }

        free(typed);
    }

    // session aggregates
    double minutes_total = total_seconds / 60.0;
    double gross_wpm_total = (minutes_total > 0.0) ? ((double)total_chars_typed / 5.0) / minutes_total : 0.0;
    double accuracy_total = (total_chars_typed > 0) ? ((double)total_correct_chars / (double)total_chars_typed * 100.0) : 0.0;
    printf("\n=== Session Summary ===\n");
    printf("Items: %d  Total time: %.2fs  Total chars typed: %zu\n", n, total_seconds, total_chars_typed);
    printf("Gross WPM: %.2f   Accuracy: %.2f%%\n", gross_wpm_total, accuracy_total);

    // persist stats and maps
    append_session_stats(gross_wpm_total, accuracy_total, (long)total_chars_typed);

    // Save maps immediately
    save_map_to_file(mwords, MWORDS_FILE);
    save_map_to_file(mchars, MCHARS_FILE);

    printf("Session saved.\n");
}

/* Training mode: build a practice list from top mistakes */
static void training_mode(Map *mwords, Map *mchars) {
    printf("\n=== Training Mode ===\n");
    // Sort copies to pick top items
    if (mwords->n == 0 && mchars->n == 0) {
        printf("No mistakes recorded yet. Do some practice first.\n");
        return;
    }
    // word-focused training if words exist
    printf("Focus options:\n1) Mistyped words\n2) Mistyped characters\nEnter choice: ");
    char *c = read_line();
    if (!c) return;
    int choice = atoi(c); free(c);
    if (choice == 1 && mwords->n > 0) {
        KeyCount *copy = malloc(mwords->n * sizeof(KeyCount));
        for (size_t i = 0; i < mwords->n; ++i) copy[i] = mwords->items[i];
        qsort(copy, mwords->n, sizeof(KeyCount), cmp_kc_desc);
        int n = (mwords->n < TOP_N) ? (int)mwords->n : TOP_N;
        printf("Top %d mistyped words:\n", n);
        for (int i = 0; i < n; ++i) printf("  %d) %s (%ld)\n", i+1, copy[i].key, copy[i].count);
        // do a focused practice of those words repeated
        printf("How many rounds through the list? (e.g. 3): ");
        char *s = read_line(); if (!s) { free(copy); return; }
        int rounds = atoi(s); free(s); if (rounds <= 0) rounds = 2;
        for (int r = 0; r < rounds; ++r) {
            for (int i = 0; i < n; ++i) {
                const char *ref = copy[i].key;
                printf("\n%s\nPress ENTER when ready...", ref);
                char *tmp = read_line(); if (tmp) free(tmp);
                printf("Type: ");
                struct timeval start, end;
                gettimeofday(&start, NULL);
                char *typed = read_line();
                gettimeofday(&end, NULL);
                if (!typed) typed = strdup("");
                CompareResult cres = compare_and_update(ref, typed, mwords, mchars);
                double secs = elapsed_seconds(start, end);
                double minutes = secs / 60.0;
                double gross_wpm = (minutes > 0.0) ? ((double)strlen(typed) / 5.0) / minutes : 0.0;
                double accuracy = (strlen(typed) > 0) ? ((double)cres.correct_chars / (double)strlen(typed) * 100.0) : 0.0;
                printf("  Result: Time %.2fs  WPM %.2f  Accuracy %.2f%%\n", secs, gross_wpm, accuracy);
                free(typed);
            }
        }
        free(copy);
        save_map_to_file(mwords, MWORDS_FILE);
        save_map_to_file(mchars, MCHARS_FILE);
        printf("Training done. Mistake counts updated.\n");
    } else if (choice == 2 && mchars->n > 0) {
        KeyCount *copy = malloc(mchars->n * sizeof(KeyCount));
        for (size_t i = 0; i < mchars->n; ++i) copy[i] = mchars->items[i];
        qsort(copy, mchars->n, sizeof(KeyCount), cmp_kc_desc);
        int n = (mchars->n < TOP_N) ? (int)mchars->n : TOP_N;
        printf("Top %d mistyped chars:\n", n);
        for (int i = 0; i < n; ++i) printf("  %d) '%s' (%ld)\n", i+1, copy[i].key, copy[i].count);
        printf("How many repetitions per char? (e.g. 5): ");
        char *s = read_line(); if (!s) { free(copy); return; }
        int reps = atoi(s); free(s); if (reps <= 0) reps = 5;
        for (int i = 0; i < n; ++i) {
            char target = copy[i].key[0];
            printf("\nPractice character '%c' (%d times). Press ENTER when ready...", target, reps);
            char *tmp = read_line(); if (tmp) free(tmp);
            for (int r = 0; r < reps; ++r) {
                printf("Type '%c': ", target);
                struct timeval start, end;
                gettimeofday(&start, NULL);
                char *typed = read_line();
                gettimeofday(&end, NULL);
                if (!typed) typed = strdup("");
                // check first character
                if (typed[0] != target) {
                    map_add_char(mchars, target, 1);
                    printf("  Wrong. Expected '%c' got '%c'\n", target, (typed[0]?typed[0]:'?'));
                } else {
                    printf("  Correct.\n");
                }
                free(typed);
            }
        }
        free(copy);
        save_map_to_file(mwords, MWORDS_FILE);
        save_map_to_file(mchars, MCHARS_FILE);
        printf("Character training complete.\n");
    } else {
        printf("No data for chosen option.\n");
    }
}

/* ----------------------
   Main menu loop
   ---------------------- */
int main(void) {
    srand((unsigned)time(NULL));
    Map mistakes_words; map_init(&mistakes_words);
    Map mistakes_chars; map_init(&mistakes_chars);

    // load existing mistakes
    load_map_from_file(&mistakes_words, MWORDS_FILE);
    load_map_from_file(&mistakes_chars, MCHARS_FILE);

    while (1) {
        printf("\n=== TypingTrainer - Type-Celerate ===\n");
        printf("1) Start Practice\n");
        printf("2) View Statistics\n");
        printf("3) Training Mode (focus on mistakes)\n");
        printf("4) Exit\n");
        printf("Enter choice: ");
        char *choice = read_line();
        if (!choice) break;
        int c = atoi(choice); free(choice);
        if (c == 1) {
            start_practice(&mistakes_words, &mistakes_chars);
        } else if (c == 2) {
            view_statistics(&mistakes_words, &mistakes_chars);
        } else if (c == 3) {
            training_mode(&mistakes_words, &mistakes_chars);
        } else if (c == 4) {
            break;
        } else {
            printf("Invalid choice.\n");
        }
    }

    // save maps on exit
    save_map_to_file(&mistakes_words, MWORDS_FILE);
    save_map_to_file(&mistakes_chars, MCHARS_FILE);
    map_free(&mistakes_words);
    map_free(&mistakes_chars);
    printf("Goodbye — keep practicing!\n");
    return 0;
}
