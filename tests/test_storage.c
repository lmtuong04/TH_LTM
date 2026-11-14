#include "test.h"
#include "model.h"
#include "storage.h"

#include <unistd.h>

static void test_storage_round_trip(void)
{
    const char *path = "/tmp/th_ltm_storage_test.txt";
    Model original;
    Model loaded;
    House *house;
    Room *room;
    DeviceRef device = {0};

    model_init(&original);
    house = model_add_house(&original, "Nha 1");
    room = house_add_room(house, "Phong khach");
    snprintf(device.id, sizeof device.id, "LIGHT-01");
    snprintf(device.type, sizeof device.type, "light");
    snprintf(device.ip, sizeof device.ip, "192.168.1.10");
    device.port = 50001;
    snprintf(device.token, sizeof device.token, "token-abc");
    CHECK(room_add_device(room, &device) != NULL);
    snprintf(device.id, sizeof device.id, "FAN-01");
    device.token[0] = '\0';
    CHECK(room_add_device(room, &device) != NULL);
    CHECK(storage_save(path, &original) == 0);
    CHECK(strlen(original.app_id) == MODEL_APP_ID_LEN - 1);

    model_init(&loaded);
    CHECK(storage_load(path, &loaded) == 0);
    if (loaded.app_id[0]) {
        CHECK_STR(loaded.app_id, original.app_id);
        CHECK(loaded.n_houses == 1);
        CHECK_STR(loaded.houses[0].name, "Nha 1");
        CHECK(loaded.houses[0].n_rooms == 1);
        CHECK_STR(loaded.houses[0].rooms[0].name, "Phong khach");
        CHECK(loaded.houses[0].rooms[0].n_devices == 2);
        CHECK_STR(loaded.houses[0].rooms[0].devices[0].id, "LIGHT-01");
        CHECK_STR(loaded.houses[0].rooms[0].devices[0].token, "token-abc");
        CHECK(loaded.houses[0].rooms[0].devices[0].port == 50001);
        CHECK_STR(loaded.houses[0].rooms[0].devices[1].id, "FAN-01");
        CHECK_STR(loaded.houses[0].rooms[0].devices[1].token, "");
    }

    CHECK(storage_save(path, &loaded) == 0);
    CHECK(storage_load(path, &original) == 0);
    CHECK_STR(original.app_id, loaded.app_id);
    model_free(&loaded);
    model_free(&original);
    CHECK(unlink(path) == 0);
}

static void test_storage_missing_file_generates_id(void)
{
    const char *path = "/tmp/th_ltm_storage_missing.txt";
    Model model;

    unlink(path);
    model_init(&model);
    CHECK(storage_load(path, &model) == 0);
    CHECK(strlen(model.app_id) == MODEL_APP_ID_LEN - 1);
    CHECK(model.n_houses == 0);
    model_free(&model);
}

static void test_storage_rejects_invalid_file(void)
{
    const char *path = "/tmp/th_ltm_storage_invalid.txt";
    Model model;
    FILE *file = fopen(path, "w");

    CHECK(file != NULL);
    if (!file)
        return;
    fputs("APP_ID\tinvalid\n", file);
    CHECK(fclose(file) == 0);
    model_init(&model);
    CHECK(storage_load(path, &model) == -1);
    CHECK(model.n_houses == 0);
    model_free(&model);
    CHECK(unlink(path) == 0);
}

static void test_storage_rejects_invalid_arguments(void)
{
    Model model;

    model_init(&model);
    CHECK(storage_save(NULL, &model) == -1);
    CHECK(storage_save("/tmp/th_ltm_storage_invalid_args.txt", NULL) == -1);
    CHECK(storage_load(NULL, &model) == -1);
    CHECK(storage_load("/tmp/th_ltm_storage_invalid_args.txt", NULL) == -1);
    snprintf(model.app_id, sizeof model.app_id, "not-an-app-id");
    CHECK(storage_save("/tmp/th_ltm_storage_invalid_args.txt", &model) == -1);
    model_free(&model);
}

int main(void)
{
    puts("test_storage");
    RUN(test_storage_round_trip);
    RUN(test_storage_missing_file_generates_id);
    RUN(test_storage_rejects_invalid_file);
    RUN(test_storage_rejects_invalid_arguments);
    return TEST_DONE();
}
