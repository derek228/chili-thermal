#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE_LEN 1024

int parse_ini_file(const char* filename);

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s filename.ini\n", argv[0]);
        return 1;
    }
    if (!parse_ini_file(argv[1])) {
        fprintf(stderr, "Error parsing file: %s\n", argv[1]);
        return 1;
    }
    return 0;
}

int parse_ini_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        return 0;
    }

    char line[MAX_LINE_LEN];
    char current_section[MAX_LINE_LEN] = "";
    while (fgets(line, MAX_LINE_LEN, file)) {
        // Remove leading and trailing whitespace
        char* trimmed_line = line;
        while (*trimmed_line == ' ' || *trimmed_line == '\t' || *trimmed_line == '\r' || *trimmed_line == '\n') {
            ++trimmed_line;
        }
        char* end = trimmed_line + strlen(trimmed_line) - 1;
        while (end > trimmed_line && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
            --end;
        }
        *(end + 1) = '\0';

        // Skip empty lines and comments
        if (*trimmed_line == '\0' || *trimmed_line == '#') {
            continue;
        }

        // Check for section headers
        if (*trimmed_line == '[' && *end == ']') {
            *end = '\0';
            strcpy(current_section, trimmed_line + 1);
        } else {
            // Parse key-value pairs
            char* equals_sign = strchr(trimmed_line, '=');
            if (equals_sign) {
                *equals_sign = '\0';
                char* key = trimmed_line;
                char* value = equals_sign + 1;
                printf("[%s] %s=%s\n", current_section, key, value);
            }
        }
    }

    fclose(file);
    return 1;
}
