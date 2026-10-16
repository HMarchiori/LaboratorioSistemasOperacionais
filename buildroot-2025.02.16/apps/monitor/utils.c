#include "utils.h"

#include <ctype.h>
#include <stdio.h>
#include <unistd.h>

long read_clk_tck(void)
{
    long tck = sysconf(_SC_CLK_TCK);
    return tck > 0 ? tck : 100;
}

int is_all_digits(const char *s)
{
    if (s == NULL || *s == '\0')
        return 0;

    for (const char *p = s; *p; p++) {
        if (!isdigit((unsigned char)*p))
            return 0;
    }
    return 1;
}

void format_dhms(long total_seconds, char *out, size_t outlen)
{
    if (total_seconds < 0)
        total_seconds = 0;

    long days = total_seconds / 86400;
    long hours = (total_seconds % 86400) / 3600;
    long minutes = (total_seconds % 3600) / 60;
    long seconds = total_seconds % 60;

    snprintf(out, outlen, "%ldd %02ld:%02ld:%02ld", days, hours, minutes, seconds);
}

void print_escaped(const char *s)
{
    for (; *s; s++) {
        switch (*s) {
        case '<': fputs("&lt;", stdout); break;
        case '>': fputs("&gt;", stdout); break;
        case '&': fputs("&amp;", stdout); break;
        default: fputc(*s, stdout);
        }
    }
}

void print_page_head(const char *title)
{
    printf("<!DOCTYPE html>\n<html lang=\"pt-br\">\n<head>\n");
    printf("<meta charset=\"utf-8\">\n");
    printf("<meta http-equiv=\"refresh\" content=\"5\">\n");
    printf("<title>");
    print_escaped(title);
    printf("</title>\n");
    printf("<style>\n"
           "body { font-family: sans-serif; margin: 2em; }\n"
           "h2 { border-bottom: 1px solid #ccc; margin-top: 1.5em; }\n"
           "table { border-collapse: collapse; margin-bottom: 1em; width: 100%%; }\n"
           "th, td { border: 1px solid #ccc; padding: 4px 8px; text-align: left; }\n"
           "th { background: #f0f0f0; width: 220px; }\n"
           "</style>\n</head>\n<body>\n");
    printf("<h1>");
    print_escaped(title);
    printf("</h1>\n");
}

void print_page_foot(void)
{
    printf("</body>\n</html>\n");
}
