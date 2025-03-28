//
// Created by kiron on 3/25/25.
//
#include "configuration.h"

void loadFileType(struct editorSyntax* syntax, char* line) {
    //Loads all information about each filetype into a syntax structure
    char* token = strtok(line, "=");
    if (!strcmp(token, "TYPE")) {
        char* type = strtok(NULL, "=");
        syntax->filetype = malloc(strlen(type) + 1);
        strcpy(syntax->filetype, type);

    } else if (!strcmp(token, "EXTENSIONS")) {
        char* extension;
        int exC = 0;
        int size = 2;
        syntax->filematch = malloc(size * sizeof(char*));
        while ((extension = strtok(NULL, ","))) {
            syntax->filematch[exC] = malloc(strlen(extension) + 1);
            strcpy(syntax->filematch[exC], extension);
            exC++;
            if (exC == size) {
                size *= 2;
                char** tmp = realloc(syntax->filematch, size * sizeof(char*));
                syntax->filematch = tmp;
            }
        }
        syntax->filematchSize = size;

    } else if (!strcmp(token, "KEYWORDS")) {
        char* keyword;
        int exC = 0;
        int size = 2;
        syntax->keywords = malloc(size * sizeof(char*));
        while ((keyword = strtok(NULL, ","))) {
            syntax->keywords[exC] = malloc(strlen(keyword) + 1);
            strcpy(syntax->keywords[exC], keyword);
            exC++;
            if (exC == size) {
                size *= 2;
                char** tmp = realloc(syntax->keywords, size * sizeof(char*));
                syntax->keywords = tmp;
            }
        }
        syntax->keywordSize = size;

    } else if (!strcmp(token, "SLC")) {
        char* scl = strtok(NULL, "=");
        syntax->singleLineCommentStart = malloc(strlen(scl) + 1);
        strcpy(syntax->singleLineCommentStart, scl);

    } else if (!strcmp(token, "MLC_START")) {
        char* mlcStart = strtok(NULL, "=");
        syntax->multiLineCommentStart = malloc(strlen(mlcStart) + 1);
        strcpy(syntax->multiLineCommentStart, mlcStart);

    } else if (!strcmp(token, "MLC_END")) {
        char* mlcEnd = strtok(NULL, "=");
        syntax->multiLineCommentEnd = malloc(strlen(mlcEnd) + 1);
        strcpy(syntax->multiLineCommentEnd, mlcEnd);

    } else if (!strcmp(token, "HIGHLIGHT_NUM")) {
        int value = atoi(strtok(NULL, "="));
        if (value) {
            syntax->flags = syntax->flags | HL_HIGHLIGHT_NUMBERS;
        }

    } else if (!strcmp(token, "HIGHLIGHT_STR")) {
        int value = atoi(strtok(NULL, "="));
        if (value) {
            syntax->flags = syntax->flags | HL_HIGHLIGHT_STRINGS;
        }
    }

}

void freeSyntax(struct editorSyntax* syntax) {
    //Deallocates heap memory used by syntax variables
    free(syntax->filetype);
    int i;
    for (i = 0; i < syntax->filematchSize; i++) {
        free(syntax->filematch[i]);
    }
    free(syntax->filematch);
    for (i = 0; i < syntax->keywordSize; i++) {
        free(syntax->keywords[i]);
    }
    free(syntax->keywords);
    free(syntax->singleLineCommentStart);
    free(syntax->multiLineCommentStart);
    free(syntax->multiLineCommentEnd);
}

void loadConfig(struct Configuration* config) {
    //Loads the configuration file and stores all settings in the configuration struct
    FILE* file = fopen("../kewetextrc", "r");
    if (!file) {
        printf("Failed to open file config file or does not exist\n");
        exit(EXIT_FAILURE);
    }
    char* line = NULL;
    size_t bufCap = 0;
    int isLoadingFileType = 0;
    int count = 1;
    struct editorSyntax* syntax;
    config->syntax = NULL;
    config->highlight_entries = 0;
    while (getline(&line, &bufCap, file) != -1) {
        if (line[0] == '#' || line[0] == '\n') {
            //Skip comments
            continue;
        }
        char* token = strtok(line, "\n");
        if (!strcmp(token, "[FStart]")) {
            //Begin the process of allocating syntax
            isLoadingFileType = 1;
            syntax = malloc(sizeof(struct editorSyntax));
            syntax->flags = 0;
            continue;
        }
        if (!strcmp(token, "[FEnd]")) {
            //End allocating syntax and add to the end of the syntax linked list
            isLoadingFileType = 0;
            syntax->next = NULL;
            if (!config->syntax) {
                config->syntax = syntax;
            } else {
                struct editorSyntax* itr = config->syntax;
                while (itr->next) {
                    itr = itr->next;
                };
                itr->next = syntax;
            }
            ++count;
            config->highlight_entries++;
            continue;
        }
        if (isLoadingFileType) {
            loadFileType(syntax, line);
        } else {
            //When not editing the syntax, we are editing the main settings
            token = strtok(line, "=");
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
    }

    fclose(file);
}

void destroyConfig(struct Configuration* config) {
    //Deallocates the configuration, in this case being just the syntax
    if (!config->syntax) {
        return;
    }
    struct editorSyntax* itr = config->syntax;
    while (itr) {
        struct editorSyntax* next = itr->next;
        freeSyntax(itr);
        itr = next;
    }
}