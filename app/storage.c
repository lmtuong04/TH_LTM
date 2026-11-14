#include "storage.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define APP_ID_BYTES ((MODEL_APP_ID_LEN - 1) / 2)

static int generate_app_id(char app_id[MODEL_APP_ID_LEN])
{
    FILE *random = fopen("/dev/urandom", "rb");
    unsigned char bytes[APP_ID_BYTES];
    static const char hex[] = "0123456789abcdef";

    if (!random)
        return -1;
    if (fread(bytes, 1, sizeof bytes, random) != sizeof bytes) {
        fclose(random);
        return -1;
    }
    if (fclose(random) != 0)
        return -1;

    for (size_t i = 0; i < sizeof bytes; i++) {
        app_id[i * 2] = hex[bytes[i] >> 4];
        app_id[i * 2 + 1] = hex[bytes[i] & 0x0f];
    }
    app_id[MODEL_APP_ID_LEN - 1] = '\0';
    return 0;
}

static int app_id_valid(const char *app_id)
{
    if (!app_id || strlen(app_id) != MODEL_APP_ID_LEN - 1)
        return 0;
    for (size_t i = 0; i < MODEL_APP_ID_LEN - 1; i++) {
        if (!(app_id[i] >= '0' && app_id[i] <= '9') &&
            !(app_id[i] >= 'a' && app_id[i] <= 'f') &&
            !(app_id[i] >= 'A' && app_id[i] <= 'F'))
            return 0;
    }
    return 1;
}

static int ensure_app_id(Model *model)
{
    if (model->app_id[0])
        return app_id_valid(model->app_id) ? 0 : -1;
    return generate_app_id(model->app_id);
}

static int write_model(FILE *file, const Model *model)
{
    if (fprintf(file, "APP_ID\t%s\n", model->app_id) < 0)
        return -1;
    for (size_t i = 0; i < model->n_houses; i++) {
        const House *house = &model->houses[i];
        if (fprintf(file, "HOUSE\t%s\n", house->name) < 0)
            return -1;
        for (size_t j = 0; j < house->n_rooms; j++) {
            const Room *room = &house->rooms[j];
            if (fprintf(file, "ROOM\t%s\n", room->name) < 0)
                return -1;
            for (size_t k = 0; k < room->n_devices; k++) {
                const DeviceRef *device = &room->devices[k];
                if (fprintf(file, "DEVICE\t%s\t%s\t%s\t%d\t%s\n",
                            device->id, device->type, device->ip,
                            device->port, device->token) < 0)
                    return -1;
            }
        }
    }
    return ferror(file) ? -1 : 0;
}

static int parse_device(Room *room, char *line)
{
    char *fields[5];
    char *cursor = line;
    char *end;
    DeviceRef device = {0};
    long port;

    for (size_t i = 0; i < 5; i++) {
        fields[i] = strsep(&cursor, "\t");
        if (!fields[i])
            return -1;
    }
    if (cursor || fields[0][0] == '\0' || fields[1][0] == '\0' ||
        fields[2][0] == '\0')
        return -1;
    port = strtol(fields[3], &end, 10);
    if (*fields[3] == '\0' || *end != '\0' || port < 0 || port > UINT16_MAX)
        return -1;
    if (strlen(fields[0]) >= sizeof device.id ||
        strlen(fields[1]) >= sizeof device.type ||
        strlen(fields[2]) >= sizeof device.ip ||
        strlen(fields[4]) >= sizeof device.token)
        return -1;

    strcpy(device.id, fields[0]);
    strcpy(device.type, fields[1]);
    strcpy(device.ip, fields[2]);
    device.port = (int)port;
    strcpy(device.token, fields[4]);
    return room_add_device(room, &device) ? 0 : -1;
}

int storage_save(const char *path, Model *model)
{
    FILE *file;
    Model copy;
    int result;

    if (!path || !model)
        return -1;
    copy = *model;
    if (ensure_app_id(&copy) < 0)
        return -1;
    file = fopen(path, "w");
    if (!file)
        return -1;
    result = write_model(file, &copy);
    if (fclose(file) != 0)
        result = -1;
    if (result == 0 && !model->app_id[0])
        strcpy(model->app_id, copy.app_id);
    return result;
}

int storage_load(const char *path, Model *model)
{
    FILE *file;
    Model loaded;
    House *house = NULL;
    Room *room = NULL;
    char line[MODEL_NAME_LEN + MODEL_IP_LEN + MODEL_TOKEN_LEN + 64];
    int result = -1;

    if (!path || !model)
        return -1;
    model_init(&loaded);
    file = fopen(path, "r");
    if (!file) {
        if (errno == ENOENT && generate_app_id(loaded.app_id) == 0) {
            model_free(model);
            *model = loaded;
            return 0;
        }
        return -1;
    }

    while (fgets(line, sizeof line, file)) {
        char *newline = strchr(line, '\n');
        char *value;

        if (!newline)
            goto done;
        *newline = '\0';
        if (strncmp(line, "APP_ID\t", 7) == 0) {
            value = line + 7;
            if (loaded.app_id[0] || !app_id_valid(value))
                goto done;
            strcpy(loaded.app_id, value);
        } else if (strncmp(line, "HOUSE\t", 6) == 0) {
            house = model_add_house(&loaded, line + 6);
            if (!house)
                goto done;
            room = NULL;
        } else if (strncmp(line, "ROOM\t", 5) == 0) {
            if (!house || !(room = house_add_room(house, line + 5)))
                goto done;
        } else if (strncmp(line, "DEVICE\t", 7) == 0) {
            if (!room || parse_device(room, line + 7) < 0)
                goto done;
        } else {
            goto done;
        }
    }
    if (ferror(file) || !loaded.app_id[0])
        goto done;
    result = 0;

done:
    fclose(file);
    if (result == 0) {
        model_free(model);
        *model = loaded;
    } else {
        model_free(&loaded);
    }
    return result;
}
