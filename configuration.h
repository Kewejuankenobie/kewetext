//
// Created by kiron on 3/25/25.
//

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#define HL_HIGHLIGHT_NUMBERS (1<<0)
#define HL_HIGHLIGHT_STRINGS (1<<1)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//Editor syntax structure to store a syntax used for highlighting text in files supporting this syntax

struct editorSyntax {
    char* filetype;
    char** filematch;
    char** keywords;
    char* singleLineCommentStart;
    char* multiLineCommentStart;
    char* multiLineCommentEnd;
    int flags;
    int filematchSize;
    int keywordSize;
    struct editorSyntax* next;
};

//Configuration structure to store settings from kewetextrc

struct Configuration {
    int tab_stop;
    int quit_times;
    int auto_indent;
    int default_undo;
    int inf_undo;
    int cursor_save;
    int use_256_colors;
    int hl_number;
    int hl_keyword1;
    int hl_keyword2;
    int hl_string;
    int hl_comment;
    int hl_multiline_comment;
    int hl_match;
    int hl_default;
    struct editorSyntax* syntax;
    int highlight_entries;
};

void loadFileType(struct editorSyntax* syntax, char* line);
void setColorHighlights(struct Configuration* config, char* token, int value);
void loadConfig(struct Configuration* config);
void destroyConfig(struct Configuration* config);


#endif //CONFIGURATION_H
