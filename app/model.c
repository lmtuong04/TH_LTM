#include "model.h"

#include <stdlib.h>
#include <string.h>

/* Đảm bảo mảng *arr còn chỗ cho thêm 1 phần tử. */
static int grow(void **arr, size_t *cap, size_t count, size_t elem_size)
{
    if (count < *cap)
        return 0;
    size_t new_cap = *cap ? *cap * 2 : 4;
    void *p = realloc(*arr, new_cap * elem_size);
    if (!p)
        return -1;
    *arr = p;
    *cap = new_cap;
    return 0;
}

/* Xoá phần tử idx bằng cách dồn các phần tử phía sau lên. */
static void remove_at(void *arr, size_t *count, size_t idx, size_t elem_size)
{
    char *base = arr;
    memmove(base + idx * elem_size, base + (idx + 1) * elem_size,
            (*count - idx - 1) * elem_size);
    (*count)--;
}

int model_name_valid(const char *name, size_t max_len)
{
    if (!name || !name[0])
        return 0;
    size_t len = strlen(name);
    if (len >= max_len)
        return 0;
    for (size_t i = 0; i < len; i++)
        if ((unsigned char)name[i] < 0x20 || name[i] == 0x7f)
            return 0;
    return 1;
}

static void room_free(Room *r)
{
    free(r->devices);
    r->devices = NULL;
    r->n_devices = r->cap_devices = 0;
}

static void house_free(House *h)
{
    for (size_t i = 0; i < h->n_rooms; i++)
        room_free(&h->rooms[i]);
    free(h->rooms);
    h->rooms = NULL;
    h->n_rooms = h->cap_rooms = 0;
}

void model_init(Model *m)
{
    m->houses = NULL;
    m->n_houses = m->cap_houses = 0;
}

void model_free(Model *m)
{
    for (size_t i = 0; i < m->n_houses; i++)
        house_free(&m->houses[i]);
    free(m->houses);
    model_init(m);
}

/* ---- House ------------------------------------------------------------ */

static long house_index(const Model *m, const char *name)
{
    for (size_t i = 0; i < m->n_houses; i++)
        if (strcmp(m->houses[i].name, name) == 0)
            return (long)i;
    return -1;
}

House *model_add_house(Model *m, const char *name)
{
    if (!model_name_valid(name, MODEL_NAME_LEN) || house_index(m, name) >= 0)
        return NULL;
    if (grow((void **)&m->houses, &m->cap_houses, m->n_houses, sizeof(House)) < 0)
        return NULL;
    House *h = &m->houses[m->n_houses++];
    memset(h, 0, sizeof *h);
    strcpy(h->name, name);
    return h;
}

House *model_find_house(Model *m, const char *name)
{
    long i = house_index(m, name);
    return i < 0 ? NULL : &m->houses[i];
}

int model_rename_house(Model *m, const char *old_name, const char *new_name)
{
    long i = house_index(m, old_name);
    if (i < 0 || !model_name_valid(new_name, MODEL_NAME_LEN))
        return -1;
    long j = house_index(m, new_name);
    if (j >= 0 && j != i)
        return -1;
    strcpy(m->houses[i].name, new_name);
    return 0;
}

int model_remove_house(Model *m, const char *name)
{
    long i = house_index(m, name);
    if (i < 0)
        return -1;
    house_free(&m->houses[i]);
    remove_at(m->houses, &m->n_houses, (size_t)i, sizeof(House));
    return 0;
}

/* ---- Room ------------------------------------------------------------- */

static long room_index(const House *h, const char *name)
{
    for (size_t i = 0; i < h->n_rooms; i++)
        if (strcmp(h->rooms[i].name, name) == 0)
            return (long)i;
    return -1;
}

Room *house_add_room(House *h, const char *name)
{
    if (!model_name_valid(name, MODEL_NAME_LEN) || room_index(h, name) >= 0)
        return NULL;
    if (grow((void **)&h->rooms, &h->cap_rooms, h->n_rooms, sizeof(Room)) < 0)
        return NULL;
    Room *r = &h->rooms[h->n_rooms++];
    memset(r, 0, sizeof *r);
    strcpy(r->name, name);
    return r;
}

Room *house_find_room(House *h, const char *name)
{
    long i = room_index(h, name);
    return i < 0 ? NULL : &h->rooms[i];
}

int house_rename_room(House *h, const char *old_name, const char *new_name)
{
    long i = room_index(h, old_name);
    if (i < 0 || !model_name_valid(new_name, MODEL_NAME_LEN))
        return -1;
    long j = room_index(h, new_name);
    if (j >= 0 && j != i)
        return -1;
    strcpy(h->rooms[i].name, new_name);
    return 0;
}

int house_remove_room(House *h, const char *name)
{
    long i = room_index(h, name);
    if (i < 0)
        return -1;
    room_free(&h->rooms[i]);
    remove_at(h->rooms, &h->n_rooms, (size_t)i, sizeof(Room));
    return 0;
}

/* ---- DeviceRef -------------------------------------------------------- */

static long device_index(const Room *r, const char *id)
{
    for (size_t i = 0; i < r->n_devices; i++)
        if (strcmp(r->devices[i].id, id) == 0)
            return (long)i;
    return -1;
}

DeviceRef *room_add_device(Room *r, const DeviceRef *d)
{
    if (!d || !model_name_valid(d->id, MODEL_ID_LEN) || device_index(r, d->id) >= 0)
        return NULL;
    if (grow((void **)&r->devices, &r->cap_devices, r->n_devices, sizeof(DeviceRef)) < 0)
        return NULL;
    DeviceRef *slot = &r->devices[r->n_devices++];
    *slot = *d;
    return slot;
}

DeviceRef *room_find_device(Room *r, const char *id)
{
    long i = device_index(r, id);
    return i < 0 ? NULL : &r->devices[i];
}

int room_remove_device(Room *r, const char *id)
{
    long i = device_index(r, id);
    if (i < 0)
        return -1;
    remove_at(r->devices, &r->n_devices, (size_t)i, sizeof(DeviceRef));
    return 0;
}

DeviceRef *model_find_device(Model *m, const char *id, House **house_out, Room **room_out)
{
    for (size_t i = 0; i < m->n_houses; i++) {
        House *h = &m->houses[i];
        for (size_t j = 0; j < h->n_rooms; j++) {
            DeviceRef *d = room_find_device(&h->rooms[j], id);
            if (d) {
                if (house_out)
                    *house_out = h;
                if (room_out)
                    *room_out = &h->rooms[j];
                return d;
            }
        }
    }
    return NULL;
}
