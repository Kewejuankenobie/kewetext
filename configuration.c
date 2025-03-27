//
// Created by kiron on 3/25/25.
//
#include "configuration.h"

void loadConfig(struct Configuration* config) {
    FILE* file = fopen("../kewetextrc", "r");
    if (!file) {
        printf("Failed to open file config file or does not exist\n");
        exit(EXIT_FAILURE);
    }
    char* line = NULL;
    size_t bufCap = 0;
    while (getline(&line, &bufCap, file) != -1) {
        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }
        char* token = strtok(line, "=");
        int value = atoi(strtok(NULL, "="));
        if (strcmp(token, "TAB_STOP") == 0) {
            config->tab_stop = value;
        } else if (strcmp(token, "QUIT_TIMES") == 0) {
            config->quit_times = value;
        } else if (strcmp(token, "AUTO_INDENT") == 0) {
            config->auto_indent = value;
        } else if (strcmp(token, "DEFAULT_UNDO") == 0) {
            config->default_undo = value;
        } else if (strcmp(token, "INF_UNDO") == 0) {
            config->inf_undo = value;
        } else if (strcmp(token, "CURSOR_SAVE") == 0) {
            config->cursor_save = value;
        }
    }

    fclose(file);
}
