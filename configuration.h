//
// Created by kiron on 3/25/25.
//

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Configuration {
    int tab_stop;
    int quit_times;
    int auto_indent;
    int default_undo;
    int inf_undo;
};

void loadConfig(struct Configuration* config);


#endif //CONFIGURATION_H
