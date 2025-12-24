// TypingTrainer


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>   // gettimeofday()
#include <ctype.h>
#include <time.h>

#define STATS_FILE  "stats.txt"
#define MWORDS_FILE "mistakes_words.txt"
#define MCHARS_FILE "mistakes_chars.txt"
#define MAX_LINE 512
#define TOP_N 10

// Einfache Wort und Satzlisten
static const char *word_bank[] = {
    "Haus","Baum","Wasser","Feuer","Erde","Luft","Himmel","Sonne","Mond","Stern",
    "Mensch","Tier","Freund","Strasse","Auto","Zug","Bus","Fahrrad","Schule","Lehrer",
    "Schüler","Buch","Papier","Stift","Computer","Tastatur","Maus","Bildschirm","Tisch","Stuhl",
    "Fenster","Tür","Küche","Bad","Garten","Blume","Gras","Wald","Berg","Tal",
    "Fluss","See","Meer","Insel","Stadt","Dorf","Markt","Laden","Arbeit","Urlaub",
    "Spiel","Musik","Lied","Stimme","Sprache","Wort","Satz","Frage","Antwort","Zeit",
    "Tag","Nacht","Woche","Monat","Jahr","Uhr","Minute","Sekunde","Familie","Mutter",
    "Vater","Bruder","Schwester","Kind","Baby","Essen","Trinken","Brot","Wasserflasche","Kaffee",
    "Tee","Zucker","Salz","Pfeffer","Messer","Gabel","Löffel","Teller","Tasse","Kleid",
    "Hose","Jacke","Schuh","Tasche","Schlüssel","Telefon","Nachricht","Arbeitstag","Feierabend","Gesundheit"
};

static const size_t word_bank_count = sizeof(word_bank) / sizeof(word_bank[0]);

static const char *sentence_bank[] = {
    "Der schnelle braune Fuchs springt ueber den faulen Hund.",
    "Uebung macht den Meister und regelmaessiges Training bringt Erfolg.",
    "Schnelles Tippen erfordert zuerst Genauigkeit, dann folgt die Geschwindigkeit.",
    "C Programmierung lehrt sorgfaeltiges Denken ueber Speicher und Verhalten.",
    "Konzentriere dich auf die Grundreihe und halte deine Finger entspannt.",
    "Jeder Tag bietet eine neue Chance, etwas dazu zu lernen.",
    "Geduld und Ausdauer sind der Schluessel zu langfristigem Fortschritt.",
    "Eine gute Haltung verbessert sowohl Komfort als auch Tippgeschwindigkeit.",
    "Kleine Schritte fuehren oft zu grossen Veraenderungen.",
    "Fehler sind Teil des Prozesses und helfen beim Lernen.",
    "Ein klarer Kopf erleichtert das Arbeiten am Computer.",
    "Wiederholung festigt das Gelernte und staerkt das Vertrauen.",
    "Ein geordneter Arbeitsplatz steigert die Konzentration.",
    "Kurze Pausen helfen dabei, die Haende zu entspannen.",
    "Regelmaessiges Training verbessert Praezision und Geschwindigkeit.",
    "Konsequentes Lernen fuehrt zu spuerbaren Ergebnissen.",
    "Ein ruhiges Umfeld macht das Tippen angenehmer.",
    "Denke vor jedem Anschlag ueber die richtige Fingerposition nach.",
    "Je mehr du tippst, desto natuerlicher fuehlt es sich an.",
    "Achte beim Schreiben auf fluessige Bewegungen und gleichmaessigen Rhythmus."
};
static const size_t sentence_bank_count = sizeof(sentence_bank) / sizeof(sentence_bank[0]);

//Einfache Map für Fehler (key -> count)
typedef struct {
    char *key;
    long count;
} KeyCount;

typedef struct {
    KeyCount *items;
    size_t n;
} Map;

static void map_init(Map *m) {
    m->items = NULL;
    m->n = 0;
}

static void map_free(Map *m) {
    size_t i;
    for (i = 0; i < m->n; i++) {
        free(m->items[i].key);
    }
    free(m->items);
    m->items = NULL;
    m->n = 0;
}

// Fügt key hinzu oder erhöht den Zähler, falls key schon existiert
static void map_add(Map *m, const char *key, long delta) {
    size_t i;
    if (key == NULL) {
        return;
    }

    // Prüfen, ob key bereits existiert
    for (i = 0; i < m->n; i++) {
        if (strcmp(m->items[i].key, key) == 0) {
            m->items[i].count += delta;
            return;
        }
    }

    // Array um 1 Element vergrössern z.B. Erhöhung von Platz für 2 KeyCount Strukturen auf 3 KeyCount Strukturen (nur die für die Pointer)
    KeyCount *tmp = realloc(m->items, (m->n + 1) * sizeof(KeyCount));
    if (tmp == NULL) {
        printf("Fehler bei realloc\n");
        exit(1);
    }
    //Neuer Pointer übernehmen
    m->items = tmp;

    //Speicher für Kopie von key reservieren, jetzt ist Platz für den neuen Eintrag
    {
        size_t len = strlen(key) + 1; //+1 wegen '\0'
        m->items[m->n].key = (char*)malloc(len);
        if (m->items[m->n].key == NULL) {
            printf("Fehler bei malloc\n");
            exit(1);
        }
        strcpy(m->items[m->n].key, key);
    }
    //Zähler eintragen
    m->items[m->n].count = delta;
    m->n++;
}

// Char als 1-Zeichen-String in Map zählen
static void map_add_char(Map *m, char ch, long delta) {
    char buf[2];
    buf[0] = ch;
    buf[1] = '\0';
    map_add(m, buf, delta);
}

// Für qsort: sortiert nach count absteigend, bei Gleichstand nach key
// Muss folgendes Format haben > int cmp(const void *a, const void *b); 
// Cast notwendig (innerhalb der Funktion), da diese Signatur von qsort so benötigt wird
static int cmp_kc_desc(const void *a, const void *b) {
    
    const KeyCount *A = (const KeyCount*)a;
    const KeyCount *B = (const KeyCount*)b;

    if (B->count < A->count) {
        return -1;
    }
    if (B->count > A->count) {
        return 1;
    }
    return strcmp(A->key, B->key);
}

// File IO für Maps: "key TAB count\n"
static void load_map_from_file(Map *m, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (f == NULL) {
        return; // Datei existiert noch nicht
    }

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f) != NULL) { //fgets liest bis zum ersten \n, beim zweiten Durchlauf bis zum zweiten \n usw.
        char *tab = strchr(line, '\t');
        if (tab == NULL) {
            continue;
        }
        *tab = '\0';//"apple\t5\n\0" > "apple\05\n\0"
        // key ist ab Zeilenanfang
        {
            char *key = line; //bis zum \0 das ersetzt wurde ""apple\0"
            char *num = tab + 1; //+1 da ohne \0 ab dieser Stelle gelesen wird > "5\n\0"
            long cnt = atol(num); //konvertiert "5\n\0" to Long 5 
            if (cnt != 0) {
                map_add(m, key, cnt);
            }
        }
    }
    fclose(f);
}

static void save_map_to_file(Map *m, const char *filename) {
    FILE *f = fopen(filename, "w"); //Im Write Modus öffnen
    size_t i;
    if (f == NULL) {
        perror("fopen"); //built in error function, mit eigenem Text
        return;
    }
    for (i = 0; i < m->n; i++) {
        fprintf(f, "%s\t%ld\n", m->items[i].key, m->items[i].count);
    }
    fclose(f);
}

//Stats: CSV: date_iso,wpm,accuracy_percent,ch_count
static void append_session_stats(double wpm, double accuracy, long chars) {
    FILE *f = fopen(STATS_FILE, "a"); //Create if exists, otherwise open and append
    if (f == NULL) {
        perror("fopen stats"); //built in error function, mit eigenem Text
        return;
    }
    
    time_t t = time(NULL); //time_t = Ein Typalias für einen ganzzahligen Datentyp (Integer).
    struct tm *tm_info = localtime(&t); 
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", tm_info);
    
    fprintf(f, "%s,%.2f,%.2f,%ld\n", buf, wpm, accuracy, chars);
    fclose(f);
}

static void compute_aggregate_stats(double *avg_wpm, double *best_wpm, double *avg_acc, size_t *sessions) {
    FILE *f;
    double wpm, acc;
    long ch;

    *avg_wpm = 0.0;
    *best_wpm = 0.0;
    *avg_acc = 0.0;
    *sessions = 0;

    f = fopen(STATS_FILE, "r"); //öffnen im read modus
    if (f == NULL) {
        return;
    }

    //fscanf > gibt die Anzahl erfolgreich eingelesener Felder zurück
    while (fscanf(f, "%*[^,],%lf,%lf,%ld\n", &wpm, &acc, &ch) == 3) { //Datum wird eingelesen (Kommas werden ignoriert) auser am Ende werden zwei Double und ein Long mit Kommatrennung und dann ein Zeilenumbruch
        (*sessions)++;
        *avg_wpm += wpm;
        *avg_acc += acc;
        if (wpm > *best_wpm) {
            *best_wpm = wpm;
        }
    }
    fclose(f);

    if (*sessions > 0) {
        *avg_wpm /= (double)(*sessions);
        *avg_acc  /= (double)(*sessions);
    }
}

// Zeilenende entfernen
static void trim_newline(char *s) {
    size_t l;

    if (s == NULL) return;
    l = strlen(s);
    if (l == 0) return;

    if (s[l-1] == '\n') {
        s[l-1] = '\0';
        l--;
    }
    if (l > 0 && s[l-1] == '\r') { //weil bei Windows manchmal noch \r\n eingelsen wird
        s[l-1] = '\0';
    }
}

// Zeitdifferenz in Sekunden
static double elapsed_seconds(struct timeval start, struct timeval end) {
    double s = (double)(end.tv_sec - start.tv_sec);
    double us = (double)(end.tv_usec - start.tv_usec) / 1000000.0; //Mikrosekunden in Sekunden
    return s + us;
}

// Zusammenfassen des Compare Results in einem Struct
typedef struct {
    size_t correct_chars;
    size_t total_chars;
    size_t correct_words;
    size_t total_words;
} CompareResult;

static CompareResult compare_and_update(const char *ref, const char *typed, Map *mwords, Map *mchars) {
    CompareResult res;
    res.correct_chars = 0;
    res.total_chars = 0;
    res.correct_words = 0;
    res.total_words = 0;
    size_t i;
    size_t rlen, tlen, minlen;


    if (ref == NULL) ref = "";
    if (typed == NULL) typed = "";

    rlen = strlen(ref);
    tlen = strlen(typed);
    res.total_chars = rlen;
    minlen = (rlen < tlen) ? rlen : tlen;

    // Zeichenweise vergleichen (nur für accuracy)
    for (i = 0; i < minlen; i++) {
        if (typed[i] == ref[i]) {
            res.correct_chars++;
        }
    }

    // Word comparison: split into words and compare each
    res.total_words = 0;
    res.correct_words = 0;
    {
        const char *rp = ref;
        const char *tp = typed;
        char ref_word[256];
        char typed_word[256];
        while (*rp) {
            // skip spaces in ref
            while (*rp && isspace((unsigned char)*rp)) rp++;
            if (!*rp) break;
            // collect ref word
            char *rw = ref_word;
            while (*rp && !isspace((unsigned char)*rp) && rw < ref_word + sizeof(ref_word) - 1) {
                *rw++ = *rp++;
            }
            *rw = '\0';
            res.total_words++;
            // skip spaces in typed
            while (*tp && isspace((unsigned char)*tp)) tp++;
            // collect typed word
            char *tw = typed_word;
            while (*tp && !isspace((unsigned char)*tp) && tw < typed_word + sizeof(typed_word) - 1) {
                *tw++ = *tp++;
            }
            *tw = '\0';
            // compare
            if (strcmp(ref_word, typed_word) == 0) {
                res.correct_words++;
            } else {
                map_add(mwords, ref_word, 1);
                // Add wrong chars from this word
                size_t rwlen = strlen(ref_word);
                size_t twlen = strlen(typed_word);
                size_t minw = (rwlen < twlen) ? rwlen : twlen;
                for (size_t j = 0; j < minw; j++) {
                    if (typed_word[j] != ref_word[j]) {
                        map_add_char(mchars, typed_word[j], 1);
                    }
                }
                for (size_t j = rwlen; j < twlen; j++) {
                    map_add_char(mchars, typed_word[j], 1);
                }
            }
        }
    }

    return res;
}

// Zufallszahl [a,b] 
static int randint(int a, int b) {
    return a + rand() % (b - a + 1); // +1 damit das obere Ende b inklusiv ist (Intervall [a,b] statt [a,b))
}

// Zeile einlesen
static char *read_line(void) {
    //Buffer nur innerhalb der Funktion
    char buffer[MAX_LINE];
    char *line;
    size_t len;

    //Wenn fgets Null zurückgibt > keine Zeile gelesen
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        return NULL;
    }
    trim_newline(buffer);
    len = strlen(buffer);
    
    // Genau passend Speicher len Zeichen + 1 Byte für das '\0'
    line = (char*)malloc(len + 1);
    if (line == NULL) {
        printf("Fehler bei malloc\n");
        exit(1); //Abbrechen wenn kein Speicher verfügbar ist
    }
    strcpy(line, buffer);
    //buffer verschwindet nach Funktionsende (Stack daher kein Free), line bleibt gültig Heap dynamischer Speicher bis free verwendet wird
    return line;
}

// Map anzeigen (Top N)
static void show_top_map(Map *m, int n) {
    size_t i;
    int limit;
    KeyCount *copy;

    if (m->n == 0) {
        printf("  (none)\n");
        return;
    }
    //Originaldaten der Map nicht verändern > Deshalb eine Kopie
    copy = malloc(m->n * sizeof(KeyCount));
    if (copy == NULL) {
        printf("Fehler bei malloc\n");
        return;
    }

    for (i = 0; i < m->n; i++) {
        copy[i] = m->items[i];
    }
    qsort(copy, m->n, sizeof(KeyCount), cmp_kc_desc); //generisches Sortieren per Compare-Funktion (Array-Pointer, Anzahl Elemente, Grösse eine Elements, Vergleichsfunktion)

    //Wenn n grösser als das eigentliche Array ist
    if (n < (int)m->n) {
        limit = n;
    } else {
        limit = (int)m->n;
    }

    for (i = 0; i < (size_t)limit; i++) {
        //%d = int, %-12s = mind. Länge von 12 Zeichen daher alle ":" untereinander, %ld  = Long Count Wert
        printf("  %d) %-12s : %ld\n", (int)i + 1, copy[i].key, copy[i].count);
    }

    free(copy);
}

// Statistik anzeigen
static void view_statistics(Map *mwords, Map *mchars) {
    double avg_wpm, best_wpm, avg_acc;
    size_t sessions;

    compute_aggregate_stats(&avg_wpm, &best_wpm, &avg_acc, &sessions);

    printf("\n=== Statistics ===\n");
    printf("Sessions recorded: %zu\n", sessions); //%zu Format Specifier für size_t
    if (sessions > 0) {
        printf("Average WPM: %.2f\n", avg_wpm);
        printf("Best WPM   : %.2f\n", best_wpm);
        printf("Average Accuracy: %.2f%%\n", avg_acc);
    }
    printf("\nTop mistyped words:\n");
    show_top_map(mwords, TOP_N);
    printf("\nTop mistyped characters:\n");
    show_top_map(mchars, TOP_N);
    printf("=====================\n\n");
}

//  Practice-Session (Wörter oder Sätze)
static void start_practice(Map *mwords, Map *mchars) {
    char *choice;
    int mode;
    char *numberitems;
    int n;
    int i;

    printf("\nStart Practice\n");
    printf("1) Word practice\n2) Sentence practice\nEnter choice: ");
    choice = read_line(); //malloc innerhalb der Funktion
    if (choice == NULL) return;
    mode = atoi(choice);
    free(choice);
    if (mode != 1 && mode != 2) {
        printf("Invalid choice.\n");
        return;
    }

    printf("How many items in this session? (e.g. 10): ");
    numberitems = read_line();
    if (numberitems == NULL) return;
    n = atoi(numberitems);
    free(numberitems);
    if (n <= 0) n = 10;

    { //Gültigkeitsbereich
        size_t total_chars_typed = 0;
        size_t total_correct_chars = 0;
        size_t total_words = 0;
        size_t total_correct_words = 0;
        double total_seconds = 0.0;

        for (i = 0; i < n; i++) {
            const char *ref; //Value ist Konstant, Adresse kann sicher ändern, Value kann nicht angepasst werden
            char *tmp;
            char *typed;
            struct timeval start, end;
            double secs;
            CompareResult cres;
            double minutes;
            double gross_wpm;
            double accuracy;

            if (mode == 1) {
                ref = word_bank[randint(0, (int)word_bank_count - 1)]; //-1 da von 0, eigene Funktion mit Modulo
            } else {
                ref = sentence_bank[randint(0, (int)sentence_bank_count - 1)]; //-1 da von 0, eigene Funktion mit Modulo
            }

            printf("\nItem %d/%d:\n%s\n", i + 1, n, ref);
            printf("Press ENTER when ready to start...");
            tmp = read_line();
            //free tmp wenn ungleich null da durch read_line ein malloc durchgeführt wurde, die Nummer wird nicht mehr benötigt
            if (tmp != NULL) free(tmp);

            printf("Type it and press ENTER when done:\n> ");
            gettimeofday(&start, NULL); //#include <sys/time.h>, time liefert nur Mikrosekunden, timeval ist ein Struct mit time_t tv_sec und susseconds_t tv_usec;
            typed = read_line();
            gettimeofday(&end, NULL); //#include <sys/time.h> 
            if (typed == NULL) {
                typed = (char*)malloc(1);
                if (typed == NULL) { printf("Fehler bei malloc\n"); exit(1); }
                typed[0] = '\0';
            }
            secs = elapsed_seconds(start, end);
            total_seconds += secs;

            Map item_mwords;
            map_init(&item_mwords);

            cres = compare_and_update(ref, typed, &item_mwords, mchars);
            total_chars_typed += strlen(typed);
            total_correct_chars += cres.correct_chars;
            total_words += cres.total_words;
            total_correct_words += cres.correct_words;

            
            minutes = secs / 60.0;
            //Gesamter Typing Speed egal ober fehlerhaft oder korrekt
            gross_wpm = 0.0;
            if (minutes > 0.0) {
                gross_wpm = ((double)strlen(typed) / 5.0) / minutes; //double da Kommazahlen, /5 da Standardwert für ein Wort
            }
            if (strlen(typed) > 0) {
                accuracy = ((double)cres.correct_chars / (double)strlen(typed)) * 100.0; //double da Kommazahlen
            } else {
                accuracy = 0.0;
            }

            printf("\nResult for item %d:\n", i + 1);
            //%.2f = 2 Kommastellen, %zu Format Specifier für einen size_t, %% für escaped % Zeichen
            printf("  Time: %.2fs  Chars typed: %zu  Accuracy: %.2f%%  WPM (gross): %.2f\n",secs, strlen(typed), accuracy, gross_wpm); 

            if (cres.total_words > 0) {
                printf("  Words correct: %zu / %zu\n", cres.correct_words, cres.total_words);
            }

            if (item_mwords.n > 0) {
                printf("  Wrong words:\n");
                show_top_map(&item_mwords, item_mwords.n);
            } else {
                printf("  All words correct!\n");
            }

            free(typed);

            // Add to global mistakes
            for (size_t j = 0; j < item_mwords.n; j++) {
                map_add(mwords, item_mwords.items[j].key, item_mwords.items[j].count);
            }
            map_free(&item_mwords);
        }

        {
            double minutes_total = total_seconds / 60.0;
            double gross_wpm_total = 0.0;
            double accuracy_total = 0.0;

            if (minutes_total > 0.0) {
                gross_wpm_total = ((double)total_chars_typed / 5.0) / minutes_total; // WPM: durch 5 teilen, da 1 Wort = 5 Zeichen (Standarddefinition für Tippgeschwindigkeit)
            }
            if (total_chars_typed > 0) {
                accuracy_total = ((double)total_correct_chars / (double)total_chars_typed) * 100.0; //Anteil der korrekten Zeichen von alle geschriebenen Zeichen
            }

            printf("\n=== Session Summary ===\n");
            printf("Items: %d  Total time: %.2fs  Total chars typed: %zu\n", n, total_seconds, total_chars_typed);
            printf("Gross WPM: %.2f   Accuracy: %.2f%%\n", gross_wpm_total, accuracy_total);

            append_session_stats(gross_wpm_total, accuracy_total, (long)total_chars_typed);
            save_map_to_file(mwords, MWORDS_FILE);
            save_map_to_file(mchars, MCHARS_FILE);
            printf("Session saved.\n");
        }
    }
}

// Training Mode (Mistakes-Fokus)
static void training_mode(Map *mwords, Map *mchars) {
    char *c;
    int choice;

    printf("\n=== Training Mode ===\n");
    if (mwords->n == 0 && mchars->n == 0) {
        printf("No mistakes recorded yet. Do some practice first.\n");
        return;
    }

    printf("Focus options:\n1) Mistyped words\n2) Mistyped characters\nEnter choice: ");
    c = read_line();
    if (c == NULL) return;
    //Convert String zu einem int
    choice = atoi(c);
    //Free C da Heap speicher
    free(c);

    if (choice == 1 && mwords->n > 0) { //mistyped words
        KeyCount *copy;
        size_t i;
        int n;
        char *s;
        int rounds;
        int r;

        //Copy um nicht die eigentliche Datenstruktur anzupassen
        copy = malloc(mwords->n * sizeof(KeyCount));
        if (copy == NULL) {
            printf("Fehler bei malloc\n");
            return;
        }
        for (i = 0; i < mwords->n; i++) {
            copy[i] = mwords->items[i];
        }
        qsort(copy, mwords->n, sizeof(KeyCount), cmp_kc_desc); //generisches Sortieren per Compare-Funktion (Array-Pointer, Anzahl Elemente, Grösse eine Elements, Vergleichsfunktion)

        //Top N misstyped Wörter anzeigen
        n = (mwords->n < TOP_N) ? (int)mwords->n : TOP_N;
        printf("Top %d mistyped words:\n", n);
        for (i = 0; i < (size_t)n; i++) {
            printf("  %d) %s (%ld)\n", (int)i + 1, copy[i].key, copy[i].count);
        }

        printf("How many rounds through the list? (e.g. 3): ");
        s = read_line();
        if (s == NULL) {
            free(copy);
            return;
        }
        rounds = atoi(s);
        free(s);
        
        //falls eine negative Rundenanzahl angegeben wurde
        if (rounds <= 0) rounds = 2;

        for (r = 0; r < rounds; r++) {
            for (i = 0; i < (size_t)n; i++) {
                const char *ref = copy[i].key;
                char *tmp;
                char *typed;
                struct timeval start, end; //#include <sys/time.h>, time liefert nur Mikrosekunden, timeval ist ein Struct mit time_t tv_sec und susseconds_t tv_usec
                double secs, minutes, gross_wpm, accuracy;
                CompareResult cres;

                printf("\n%s\nPress ENTER when ready...", ref);
                tmp = read_line();
                //free tmp wenn ungleich null da durch read_line ein malloc durchgeführt wurde, die Nummer wird nicht mehr benötigt
                if (tmp != NULL) free(tmp);

                printf("Type: ");
                gettimeofday(&start, NULL); //#include <sys/time.h>, time liefert nur Mikrosekunden, timeval ist ein Struct mit time_t tv_sec und susseconds_t tv_usec;
                typed = read_line();
                gettimeofday(&end, NULL); //#include <sys/time.h>, time liefert nur Mikrosekunden, timeval ist ein Struct mit time_t tv_sec und susseconds_t tv_usec;
                //Weil bei Typed == NULL würde das Programm beendet werden, wenn der User z.B. nur Enter drückt
                if (typed == NULL) {
                    typed = (char*)malloc(1);
                    if (typed == NULL) { printf("Fehler bei malloc\n"); exit(1); }
                    typed[0] = '\0';
                }

                cres = compare_and_update(ref, typed, mwords, mchars);
                secs = elapsed_seconds(start, end);
                minutes = secs / 60.0;
                if (minutes > 0.0) {
                    gross_wpm = ((double)strlen(typed) / 5.0) / minutes; //double da Kommazahlen, /5 da Standardwert für ein Wort
                } else {
                    gross_wpm = 0.0;
                }
                if (strlen(typed) > 0) {
                    accuracy = ((double)cres.correct_chars / (double)strlen(typed)) * 100.0;
                } else {
                    accuracy = 0.0;
                }
                //%.2f = 2 Kommastellen, %zu Format Specifier für einen size_t, %% für escaped % Zeichen
                printf("  Result: Time %.2fs  WPM %.2f  Accuracy %.2f%%\n",secs, gross_wpm, accuracy);

                free(typed);
            }
        }
        free(copy);
        save_map_to_file(mwords, MWORDS_FILE);
        save_map_to_file(mchars, MCHARS_FILE);
        printf("Training done. Mistake counts updated.\n");
    } else if (choice == 2 && mchars->n > 0) { //misstyped chars
        KeyCount *copy;
        size_t i;
        int n;
        char *s;
        int reps;

        //Copy um nicht die eigentliche Datenstruktur anzupassen
        copy = malloc(mchars->n * sizeof(KeyCount));
        if (copy == NULL) {
            printf("Fehler bei malloc\n");
            return;
        }
        for (i = 0; i < mchars->n; i++) {
            copy[i] = mchars->items[i];
        }
        qsort(copy, mchars->n, sizeof(KeyCount), cmp_kc_desc); //generisches Sortieren per Compare-Funktion (Array-Pointer, Anzahl Elemente, Grösse eine Elements, Vergleichsfunktion)

        //Top N misstyped Wörter anzeigen
        n = (mchars->n < TOP_N) ? (int)mchars->n : TOP_N;
        printf("Top %d mistyped chars:\n", n);
        for (i = 0; i < (size_t)n; i++) {
            printf("  %d) '%s' (%ld)\n", (int)i + 1, copy[i].key, copy[i].count);
        }
        
        printf("How many repetitions per char? (e.g. 5): ");
        s = read_line();
        if (s == NULL) {
            free(copy);
            return;
        }
        reps = atoi(s);
        free(s);
        //falls eine negative Repetitionsanzahl angegeben wurde
        if (reps <= 0) reps = 5;

        for (i = 0; i < (size_t)n; i++) {
            char target = copy[i].key[0];
            int r;
            char *tmp;
            printf("\nPractice character '%c' (%d times). Press ENTER when ready...", target, reps);
            //free tmp wenn ungleich null da durch read_line ein malloc durchgeführt wurde, die Nummer wird nicht mehr benötigt
            tmp = read_line();
            
            if (tmp != NULL) free(tmp);

            for (r = 0; r < reps; r++) {
                char *typed;
                printf("Type '%c': ", target);
                typed = read_line();
                if (typed == NULL) {
                    //Weil bei Typed == NULL würde das Programm beendet werden, wenn der User z.B. nur Enter drückt
                    typed = (char*)malloc(1);
                    if (typed == NULL) { printf("Fehler bei malloc\n"); exit(1); }
                    typed[0] = '\0';
                }
                if (typed[0] != target) {
                    map_add_char(mchars, target, 1);
                    printf("  Wrong. Expected '%c' got '%c'\n", target, (typed[0] ? typed[0] : '?'));
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

// Main
int main(void) {
    Map mistakes_words;
    Map mistakes_chars;
    char *choice;
    int c;

    //Immer andere Reihenfolge, das s Rand mit der aktuellen Uhrzeit seit 1970 immer einen anderen Startpunkt setzt (srand erwartet einen unsigend int deshalb das Parsing)
    srand((unsigned)time(NULL));

    map_init(&mistakes_words);
    map_init(&mistakes_chars);

    load_map_from_file(&mistakes_words, MWORDS_FILE);
    load_map_from_file(&mistakes_chars, MCHARS_FILE);

    while (1) {
        printf("\n=== TypingTrainer - Type-Celerate ===\n");
        printf("1) Start Practice\n");
        printf("2) View Statistics\n");
        printf("3) Training Mode (focus on mistakes)\n");
        printf("4) Exit\n");
        printf("Enter choice: ");

        choice = read_line();
        if (choice == NULL) {
            break;
        }
        c = atoi(choice); //String to Integer Funktion
        free(choice);

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

    save_map_to_file(&mistakes_words, MWORDS_FILE);
    save_map_to_file(&mistakes_chars, MCHARS_FILE);
    map_free(&mistakes_words);
    map_free(&mistakes_chars);

    printf("Goodbye — keep practicing!\n");
    return 0;
}
