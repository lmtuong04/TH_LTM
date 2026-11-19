#ifndef UI_H
#define UI_H

#include <stdio.h>

#include "model.h"

/* Run the interactive menu until the user selects 0 or input reaches EOF. */
int ui_menu(Model *model, FILE *in, FILE *out);

#endif
