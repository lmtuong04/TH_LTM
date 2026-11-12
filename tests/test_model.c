#include "test.h"
#include "model.h"

static DeviceRef make_dev(const char *id, const char *type)
{
    DeviceRef d;
    memset(&d, 0, sizeof d);
    snprintf(d.id, sizeof d.id, "%s", id);
    snprintf(d.type, sizeof d.type, "%s", type);
    snprintf(d.ip, sizeof d.ip, "192.168.1.10");
    d.port = 50001;
    return d;
}

static void test_name_valid(void)
{
    char long_name[MODEL_NAME_LEN + 1];
    memset(long_name, 'x', MODEL_NAME_LEN);
    long_name[MODEL_NAME_LEN] = '\0';

    CHECK(model_name_valid("Phong khach", MODEL_NAME_LEN));
    CHECK(!model_name_valid(NULL, MODEL_NAME_LEN));
    CHECK(!model_name_valid("", MODEL_NAME_LEN));
    CHECK(!model_name_valid(long_name, MODEL_NAME_LEN));
    long_name[MODEL_NAME_LEN - 1] = '\0';          /* vừa khít 63 ký tự */
    CHECK(model_name_valid(long_name, MODEL_NAME_LEN));
    CHECK(!model_name_valid("a\nb", MODEL_NAME_LEN));
    CHECK(!model_name_valid("a\tb", MODEL_NAME_LEN));
    CHECK(!model_name_valid("a\x7f", MODEL_NAME_LEN));
}

static void test_house_crud(void)
{
    Model m;
    model_init(&m);

    CHECK(model_add_house(&m, "Nha 1") != NULL);
    CHECK(model_add_house(&m, "Nha 2") != NULL);
    CHECK(model_add_house(&m, "Nha 1") == NULL);   /* trùng */
    CHECK(model_add_house(&m, "") == NULL);
    CHECK(m.n_houses == 2);

    CHECK(model_find_house(&m, "Nha 2") != NULL);
    CHECK(model_find_house(&m, "Khong co") == NULL);

    CHECK(model_rename_house(&m, "Nha 1", "Nha A") == 0);
    CHECK(model_find_house(&m, "Nha 1") == NULL);
    CHECK(model_find_house(&m, "Nha A") != NULL);
    CHECK(model_rename_house(&m, "Nha A", "Nha A") == 0);   /* đổi thành chính nó */
    CHECK(model_rename_house(&m, "Nha A", "Nha 2") == -1);  /* trùng nhà khác */
    CHECK(model_rename_house(&m, "Khong co", "X") == -1);
    CHECK(model_rename_house(&m, "Nha A", "") == -1);

    CHECK(model_remove_house(&m, "Nha A") == 0);
    CHECK(model_remove_house(&m, "Nha A") == -1);
    CHECK(m.n_houses == 1);
    CHECK_STR(m.houses[0].name, "Nha 2");

    model_free(&m);
    CHECK(m.houses == NULL && m.n_houses == 0 && m.cap_houses == 0);
}

static void test_room_crud(void)
{
    Model m;
    model_init(&m);
    House *h = model_add_house(&m, "Nha");

    CHECK(house_add_room(h, "Khach") != NULL);
    CHECK(house_add_room(h, "Ngu") != NULL);
    CHECK(house_add_room(h, "Bep") != NULL);
    CHECK(house_add_room(h, "Ngu") == NULL);
    CHECK(house_add_room(h, "a\nb") == NULL);
    CHECK(h->n_rooms == 3);

    CHECK(house_find_room(h, "Bep") != NULL);
    CHECK(house_find_room(h, "Tam") == NULL);

    CHECK(house_rename_room(h, "Bep", "Nha bep") == 0);
    CHECK(house_find_room(h, "Nha bep") != NULL);
    CHECK(house_rename_room(h, "Nha bep", "Nha bep") == 0);
    CHECK(house_rename_room(h, "Nha bep", "Khach") == -1);
    CHECK(house_rename_room(h, "Tam", "X") == -1);
    CHECK(house_rename_room(h, "Khach", NULL) == -1);

    /* xoá phòng ở giữa: thứ tự các phòng còn lại giữ nguyên */
    CHECK(house_remove_room(h, "Ngu") == 0);
    CHECK(house_remove_room(h, "Ngu") == -1);
    CHECK(h->n_rooms == 2);
    CHECK_STR(h->rooms[0].name, "Khach");
    CHECK_STR(h->rooms[1].name, "Nha bep");

    model_free(&m);
}

static void test_device_crud(void)
{
    Model m;
    model_init(&m);
    House *h = model_add_house(&m, "Nha");
    Room *r = house_add_room(h, "Khach");

    DeviceRef light = make_dev("LIGHT-01", "light");
    DeviceRef fan = make_dev("FAN-01", "fan");
    DeviceRef empty = make_dev("", "ac");

    DeviceRef *p = room_add_device(r, &light);
    CHECK(p != NULL);
    CHECK_STR(p->type, "light");
    CHECK(p->port == 50001);
    CHECK(room_add_device(r, &fan) != NULL);
    CHECK(room_add_device(r, &light) == NULL);     /* trùng id */
    CHECK(room_add_device(r, &empty) == NULL);
    CHECK(room_add_device(r, NULL) == NULL);
    CHECK(r->n_devices == 2);

    /* sửa qua con trỏ tìm được (lưu token) */
    p = room_find_device(r, "FAN-01");
    CHECK(p != NULL);
    snprintf(p->token, sizeof p->token, "abc123");
    CHECK_STR(room_find_device(r, "FAN-01")->token, "abc123");
    CHECK(room_find_device(r, "AC-01") == NULL);

    CHECK(room_remove_device(r, "LIGHT-01") == 0);
    CHECK(room_remove_device(r, "LIGHT-01") == -1);
    CHECK(r->n_devices == 1);
    CHECK_STR(r->devices[0].id, "FAN-01");

    model_free(&m);
}

static void test_find_device_global(void)
{
    Model m;
    model_init(&m);
    House *h1 = model_add_house(&m, "Nha 1");
    house_add_room(h1, "Khach");
    House *h2 = model_add_house(&m, "Nha 2");
    house_add_room(h2, "Ngu");
    Room *r = house_add_room(h2, "Bep");
    h1 = model_find_house(&m, "Nha 1");             /* có thể đã bị dời do realloc */

    DeviceRef ac = make_dev("AC-01", "ac");
    room_add_device(r, &ac);

    House *fh = NULL;
    Room *fr = NULL;
    DeviceRef *d = model_find_device(&m, "AC-01", &fh, &fr);
    CHECK(d != NULL);
    CHECK(fh != NULL && strcmp(fh->name, "Nha 2") == 0);
    CHECK(fr != NULL && strcmp(fr->name, "Bep") == 0);

    CHECK(model_find_device(&m, "AC-01", NULL, NULL) == d);
    fh = NULL;
    CHECK(model_find_device(&m, "FAN-09", &fh, &fr) == NULL);
    CHECK(fh == NULL);
    (void)h1;

    model_free(&m);
}

/* "Xong khi: thêm 100 phòng không lỗi" – kiểm tra realloc giãn mảng đúng */
static void test_many(void)
{
    Model m;
    model_init(&m);
    char name[MODEL_NAME_LEN];

    for (int i = 0; i < 20; i++) {
        snprintf(name, sizeof name, "Nha %d", i);
        CHECK(model_add_house(&m, name) != NULL);
    }
    House *h = model_find_house(&m, "Nha 7");
    for (int i = 0; i < 100; i++) {
        snprintf(name, sizeof name, "Phong %d", i);
        CHECK(house_add_room(h, name) != NULL);
    }
    CHECK(h->n_rooms == 100);
    CHECK(h->cap_rooms >= 100);
    CHECK_STR(h->rooms[0].name, "Phong 0");
    CHECK_STR(h->rooms[99].name, "Phong 99");

    Room *r = house_find_room(h, "Phong 42");
    for (int i = 0; i < 50; i++) {
        snprintf(name, sizeof name, "LIGHT-%02d", i);
        DeviceRef d = make_dev(name, "light");
        CHECK(room_add_device(r, &d) != NULL);
    }
    CHECK(r->n_devices == 50);
    CHECK(model_find_device(&m, "LIGHT-49", NULL, NULL) != NULL);

    /* xoá nhà có nhiều phòng/thiết bị: valgrind không được báo rò */
    CHECK(model_remove_house(&m, "Nha 7") == 0);
    CHECK(m.n_houses == 19);
    CHECK(model_find_device(&m, "LIGHT-49", NULL, NULL) == NULL);

    model_free(&m);
}

int main(void)
{
    puts("test_model");
    RUN(test_name_valid);
    RUN(test_house_crud);
    RUN(test_room_crud);
    RUN(test_device_crud);
    RUN(test_find_device_global);
    RUN(test_many);
    return TEST_DONE();
}
