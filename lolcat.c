/* Copyright (C) 2014 jaseg <github@jaseg.net>
 *
 * DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
 * Version 2, December 2004
 *
 * Everyone is permitted to copy and distribute verbatim or modified
 * copies of this license document, and changing it is allowed as long
 * as the name is changed.
 *
 * DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
 * TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION
 *
 * 0. You just DO WHAT THE FUCK YOU WANT TO.
 */

#define _XOPEN_SOURCE

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <wchar.h>

static char helpstr[] = "\n"
                        "Usage: lolcat [-h horizontal_speed] [-v vertical_speed] [--] [FILES...]\n"
                        "\n"
                        "Concatenate FILE(s), or standard input, to standard output.\n"
                        "With no FILE, or when FILE is -, read standard input.\n"
                        "\n"
                        "              -h <d>:   Horizontal rainbow frequency (default: 0.23)\n"
                        "              -v <d>:   Vertical rainbow frequency (default: 0.1)\n"
                        "                  -f:   Force color even when stdout is not a tty\n"
                        "           --version:   Print version and exit\n"
                        "              --help:   Show this message\n"
                        "\n"
                        "Examples:\n"
                        "  lolcat f - g      Output f's contents, then stdin, then g's contents.\n"
                        "  lolcat            Copy standard input to standard output.\n"
                        "  fortune | lolcat  Display a rainbow cookie.\n"
                        "\n"
                        "Report lolcat bugs to <http://www.github.org/jaseg/lolcat/issues>\n"
                        "lolcat home page: <http://www.github.org/jaseg/lolcat/>\n"
                        "Original idea: <http://www.github.org/busyloop/lolcat/>\n";

#define ARRAY_SIZE(foo) (sizeof(foo) / sizeof(foo[0]))

const unsigned char codes_light[] = {
    218,
    223,
    193,
    158,
    153,
    183,
};

const unsigned char codes_dark[] = {
    125,
    130,
    70,
    35,
    25,
    55,
};

const unsigned char *codes = codes_light;

#define N_CODES 6


void find_escape_sequences(int c, int* state)
{
    if (c == '\033') { /* Escape sequence YAY */
        *state = 1;
    } else if (*state == 1) {
        if (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z'))
            *state = 2;
    } else {
        *state = 0;
    }
}

void usage()
{
    printf("Usage: lolcat [-h horizontal_speed] [-v vertical_speed] [--] [FILES...]\n");
    exit(1);
}

void version()
{
    printf("lolcat version 0.1, (c) 2014 jaseg\n");
    exit(0);
}

wint_t helpstr_hack(FILE * _ignored)
{
    (void)_ignored;
    static size_t idx = 0;
    char c = helpstr[idx++];
    if (c)
        return c;
    idx = 0;
    return WEOF;
}

char *color_scheme_command =
"gdbus call --session --timeout=1000 "
"         --dest=org.freedesktop.portal.Desktop "
"         --object-path /org/freedesktop/portal/desktop "
"         --method org.freedesktop.portal.Settings.Read org.freedesktop.appearance color-scheme";

int main(int argc, char** argv)
{
    char* default_argv[] = { "-" };
    int cc = -1, i, l = 0;
    wint_t c;
    int colors = 1;
    double freq_h = 0.23, freq_v = 0.1;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    srand(tv.tv_usec);
    double offset = rand() % N_CODES;

    // Check for light mode / dark mode
    char color_scheme_query_result[1024];
    char mode = '0';

    FILE *fp = popen(color_scheme_command, "r");
    if (fp == NULL) {
        printf("Failed to run command\n");
    } else if(fgets(color_scheme_query_result, sizeof(color_scheme_query_result), fp) != NULL) {
        mode = color_scheme_query_result[10];
    }

    switch (mode) {
    case '0':
        codes = codes_dark;
        break;
    case '1':
        codes = codes_light;
        break;
    case '2':
        codes = codes_dark;
        break;
    }

    for (i = 1; i < argc; i++) {
        char* endptr;
        if (!strcmp(argv[i], "-h")) {
            if ((++i) < argc) {
                freq_h = strtod(argv[i], &endptr);
                if (*endptr)
                    usage();
            } else {
                usage();
            }
        } else if (!strcmp(argv[i], "-v")) {
            if ((++i) < argc) {
                freq_v = strtod(argv[i], &endptr);
                if (*endptr)
                    usage();
            } else {
                usage();
            }
        } else if (!strcmp(argv[i], "-f")) {
            colors = 1;
        } else if (!strcmp(argv[i], "--version")) {
            version();
        } else {
            if (!strcmp(argv[i], "--"))
                i++;
            break;
        }
    }

    char** inputs = argv + i;
    char** inputs_end = argv + argc;
    if (inputs == inputs_end) {
        inputs = default_argv;
        inputs_end = inputs + 1;
    }

    setlocale(LC_ALL, "");

    i = 0;
    for (char** filename = inputs; filename < inputs_end; filename++) {
        wint_t (*this_file_read_wchar)(FILE*); /* Used for --help because fmemopen is universally broken when used with fgetwc */
        FILE* f;
        int escape_state = 0;

        if (!strcmp(*filename, "--help")) {
            this_file_read_wchar = &helpstr_hack;
            f = 0;

        } else if (!strcmp(*filename, "-")) {
            this_file_read_wchar = &fgetwc;
            f = stdin;

        } else {
            this_file_read_wchar = &fgetwc;
            f = fopen(*filename, "r");
            if (!f) {
                fprintf(stderr, "Cannot open input file \"%s\": %s\n", *filename, strerror(errno));
                return 2;
            }
        }

        while ((c = this_file_read_wchar(f)) != WEOF) {
            if (colors) {
                find_escape_sequences(c, &escape_state);

                if (!escape_state) {
                    if (c == '\n') {
                        l++;
                        i = 0;
                    } else {
                        int ncc = offset + (i += wcwidth(c)) * freq_h + l * freq_v;
                        if (cc != ncc)
                            printf("\033[38;5;%hhum", codes[(cc = ncc) % N_CODES]);
                    }
                }
            }

            printf("%lc", c);

            if (escape_state == 2)
                printf("\033[38;5;%hhum", codes[cc % N_CODES]);
        }
        printf("\n\033[0m");
        cc = -1;

        if (f) {
            fclose(f);

            if (ferror(f)) {
                fprintf(stderr, "Error reading input file \"%s\": %s\n", *filename, strerror(errno));
                return 2;
            }
        }
    }
}
