//
// Created by kiron on 3/25/25.
//
#include "configuration.h"

void loadFileType(struct editorSyntax* syntax, char* line) {
    //Loads all information about each filetype into a syntax structure
    char* token = strtok(line, "=");
    if (!strcmp(token, "TYPE")) {
        char* type = strtok(NULL, "=");
        if (!type) {
            type = "no ft";
        }
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
        char* slc = strtok(NULL, "=");
        if (!slc) {
            //This check ensures that when highlighting syntax, a random character is not used as a comment start
            syntax->singleLineCommentStart = malloc(1);
            syntax->singleLineCommentStart[0] = '\0';
            return;
        }
        syntax->singleLineCommentStart = malloc(strlen(slc) + 1);
        strcpy(syntax->singleLineCommentStart, slc);

    } else if (!strcmp(token, "MLC_START")) {
        char* mlcStart = strtok(NULL, "=");
        if (!mlcStart) {
            syntax->multiLineCommentStart = malloc(1);
            syntax->multiLineCommentStart[0] = '\0';
            return;
        }
        syntax->multiLineCommentStart = malloc(strlen(mlcStart) + 1);
        strcpy(syntax->multiLineCommentStart, mlcStart);

    } else if (!strcmp(token, "MLC_END")) {
        char* mlcEnd = strtok(NULL, "=");
        if (!mlcEnd) {
            syntax->multiLineCommentEnd = malloc(1);
            syntax->multiLineCommentEnd[0] = '\0';
            return;
        }
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

void setColorHighlights(struct Configuration* config, char* token, int value) {
    if (!strcmp(token, "HL_NUMBER")) {
        config->hl_number = value;
    } else if (!strcmp(token, "HL_KEYWORD1")) {
        config->hl_keyword1 = value;
    } else if (!strcmp(token, "HL_KEYWORD2")) {
        config->hl_keyword2 = value;
    } else if (!strcmp(token, "HL_STRING")) {
        config->hl_string = value;
    } else if (!strcmp(token, "HL_COMMENT")) {
        config->hl_comment = value;
    } else if (!strcmp(token, "HL_MULTILINE_COMMENT")) {
        config->hl_multiline_comment = value;
    } else if (!strcmp(token, "HL_FIND")) {
        config->hl_match = value;
    } else {
        config->hl_default = value;
    }
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
            } else if (!strcmp(token, "QUIT_TIMES")) {
                config->quit_times = value;
            } else if (!strcmp(token, "AUTO_INDENT")) {
                config->auto_indent = value;
            } else if (!strcmp(token, "DEFAULT_UNDO")) {
                config->default_undo = value;
            } else if (!strcmp(token, "INF_UNDO")) {
                config->inf_undo = value;
            } else if (!strcmp(token, "CURSOR_SAVE")) {
                config->cursor_save = value;
            } else if (!strcmp(token, "USE_256_COLORS")) {
                config->use_256_colors = value;
            } else if (strstr(token, "HL")) {
                setColorHighlights(config, token, value);
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