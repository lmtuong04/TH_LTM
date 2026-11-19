#include "ui.h"

#include <stdlib.h>
#include <string.h>

static int read_line(FILE *in, FILE *out, const char *prompt,
                     char *buffer, size_t size)
{
    if (fputs(prompt, out) == EOF || !fgets(buffer, size, in))
        return -1;
    if (!strchr(buffer, '\n') && !feof(in)) {
        int c;
        while ((c = fgetc(in)) != '\n' && c != EOF)
            ;
        fputs("Loi: du lieu qua dai.\n", out);
        return -1;
    }
    buffer[strcspn(buffer, "\r\n")] = '\0';
    return 0;
}

static int read_choice(FILE *in, FILE *out)
{
    char line[16];
    char *end;
    long choice;

    if (read_line(in, out, "Chon: ", line, sizeof line) < 0)
        return feof(in) ? -1 : -2;
    choice = strtol(line, &end, 10);
    if (*line == '\0' || *end != '\0' || choice < 0 || choice > 7) {
        fputs("Loi: lua chon khong hop le.\n", out);
        return -2;
    }
    return (int)choice;
}

static void print_model(const Model *model, FILE *out)
{
    if (model->n_houses == 0) {
        fputs("Chua co nha.\n", out);
        return;
    }
    for (size_t i = 0; i < model->n_houses; i++) {
        const House *house = &model->houses[i];
        fprintf(out, "Nha: %s\n", house->name);
        for (size_t j = 0; j < house->n_rooms; j++)
            fprintf(out, "  Phong: %s\n", house->rooms[j].name);
    }
}

static void add_house(Model *model, FILE *in, FILE *out)
{
    char name[MODEL_NAME_LEN];

    if (read_line(in, out, "Ten nha: ", name, sizeof name) < 0)
        return;
    if (model_add_house(model, name))
        fputs("Da them nha.\n", out);
    else
        fputs("Loi: khong the them nha.\n", out);
}

static void rename_house(Model *model, FILE *in, FILE *out)
{
    char old_name[MODEL_NAME_LEN];
    char new_name[MODEL_NAME_LEN];

    if (read_line(in, out, "Ten nha cu: ", old_name, sizeof old_name) < 0 ||
        read_line(in, out, "Ten nha moi: ", new_name, sizeof new_name) < 0)
        return;
    if (model_rename_house(model, old_name, new_name) == 0)
        fputs("Da sua nha.\n", out);
    else
        fputs("Loi: khong the sua nha.\n", out);
}

static void remove_house(Model *model, FILE *in, FILE *out)
{
    char name[MODEL_NAME_LEN];

    if (read_line(in, out, "Ten nha: ", name, sizeof name) < 0)
        return;
    if (model_remove_house(model, name) == 0)
        fputs("Da xoa nha.\n", out);
    else
        fputs("Loi: khong tim thay nha.\n", out);
}

static House *read_house(Model *model, FILE *in, FILE *out,
                         char name[MODEL_NAME_LEN])
{
    if (read_line(in, out, "Ten nha: ", name, MODEL_NAME_LEN) < 0)
        return NULL;
    return model_find_house(model, name);
}

static void add_room(Model *model, FILE *in, FILE *out)
{
    char house_name[MODEL_NAME_LEN];
    char room_name[MODEL_NAME_LEN];
    House *house = read_house(model, in, out, house_name);

    if (!house) {
        fputs("Loi: khong tim thay nha.\n", out);
        return;
    }
    if (read_line(in, out, "Ten phong: ", room_name, sizeof room_name) < 0)
        return;
    if (house_add_room(house, room_name))
        fputs("Da them phong.\n", out);
    else
        fputs("Loi: khong the them phong.\n", out);
}

static void rename_room(Model *model, FILE *in, FILE *out)
{
    char house_name[MODEL_NAME_LEN];
    char old_name[MODEL_NAME_LEN];
    char new_name[MODEL_NAME_LEN];
    House *house = read_house(model, in, out, house_name);

    if (!house) {
        fputs("Loi: khong tim thay nha.\n", out);
        return;
    }
    if (read_line(in, out, "Ten phong cu: ", old_name, sizeof old_name) < 0 ||
        read_line(in, out, "Ten phong moi: ", new_name, sizeof new_name) < 0)
        return;
    if (house_rename_room(house, old_name, new_name) == 0)
        fputs("Da sua phong.\n", out);
    else
        fputs("Loi: khong the sua phong.\n", out);
}

static void remove_room(Model *model, FILE *in, FILE *out)
{
    char house_name[MODEL_NAME_LEN];
    char room_name[MODEL_NAME_LEN];
    House *house = read_house(model, in, out, house_name);

    if (!house) {
        fputs("Loi: khong tim thay nha.\n", out);
        return;
    }
    if (read_line(in, out, "Ten phong: ", room_name, sizeof room_name) < 0)
        return;
    if (house_remove_room(house, room_name) == 0)
        fputs("Da xoa phong.\n", out);
    else
        fputs("Loi: khong tim thay phong.\n", out);
}

int ui_menu(Model *model, FILE *in, FILE *out)
{
    if (!model || !in || !out)
        return -1;
    for (;;) {
        fputs("\n1. Xem nha/phong\n"
              "2. Them nha\n"
              "3. Sua ten nha\n"
              "4. Xoa nha\n"
              "5. Them phong\n"
              "6. Sua ten phong\n"
              "7. Xoa phong\n"
              "0. Thoat\n", out);
        int choice = read_choice(in, out);
        switch (choice) {
        case 0:
            return 0;
        case 1:
            print_model(model, out);
            break;
        case 2:
            add_house(model, in, out);
            break;
        case 3:
            rename_house(model, in, out);
            break;
        case 4:
            remove_house(model, in, out);
            break;
        case 5:
            add_room(model, in, out);
            break;
        case 6:
            rename_room(model, in, out);
            break;
        case 7:
            remove_room(model, in, out);
            break;
        default:
            if (choice == -1)
                return 0;
            break;
        }
    }
}
