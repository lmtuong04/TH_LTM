#ifndef STORAGE_H
#define STORAGE_H

#include "model.h"

/* Save/load the complete model in a line-oriented text format. */
int storage_save(const char *path, Model *model);
int storage_load(const char *path, Model *model);

#endif
