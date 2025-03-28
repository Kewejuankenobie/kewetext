//
// Created by kiron on 3/25/25.
//

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

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
    struct editorSyntax* syntax;
    int highlight_entries;
};

void loadFileType(struct editorSyntax* syntax, char* line);
void loadConfig(struct Configuration* config);
void destroyConfig(struct Configuration* config);


#endif //CONFIGURATION_H
