#include "storage.h"
#include "ui.h"

#include <stdio.h>

int main(void)
{
    const char *path = "app.dat";
    Model model;
    int status;

    model_init(&model);
    if (storage_load(path, &model) < 0) {
        fprintf(stderr, "Loi: khong the doc %s.\n", path);
        model_free(&model);
        return 1;
    }
    status = ui_menu(&model, stdin, stdout);
    if (status == 0 && storage_save(path, &model) < 0) {
        fprintf(stderr, "Loi: khong the luu %s.\n", path);
        status = -1;
    }
    model_free(&model);
    return status < 0 ? 1 : 0;
}
