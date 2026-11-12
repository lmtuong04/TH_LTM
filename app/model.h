/* Mô hình dữ liệu của app: Model -> House -> Room -> DeviceRef.
 * Mỗi cấp là một mảng động (malloc/realloc), tự giãn gấp đôi khi đầy.
 *
 * Lưu ý: con trỏ trả về từ *_add / *_find chỉ còn hợp lệ tới lần
 * thêm/xoá tiếp theo ở cùng cấp (realloc có thể dời mảng). */
#ifndef MODEL_H
#define MODEL_H

#include <stddef.h>

#define MODEL_NAME_LEN   64
#define MODEL_ID_LEN     32
#define MODEL_TYPE_LEN   16
#define MODEL_IP_LEN     64
#define MODEL_TOKEN_LEN  64

typedef struct {
    char id[MODEL_ID_LEN];        /* LIGHT-01, FAN-01 ... */
    char type[MODEL_TYPE_LEN];    /* light | fan | ac */
    char ip[MODEL_IP_LEN];
    int  port;
    char token[MODEL_TOKEN_LEN];  /* rỗng nếu chưa kết nối */
} DeviceRef;

typedef struct {
    char       name[MODEL_NAME_LEN];
    DeviceRef *devices;
    size_t     n_devices;
    size_t     cap_devices;
} Room;

typedef struct {
    char   name[MODEL_NAME_LEN];
    Room  *rooms;
    size_t n_rooms;
    size_t cap_rooms;
} House;

typedef struct {
    House *houses;
    size_t n_houses;
    size_t cap_houses;
} Model;

/* Tên hợp lệ: không rỗng, vừa bộ đệm, không có ký tự điều khiển. */
int model_name_valid(const char *name, size_t max_len);

void   model_init(Model *m);
void   model_free(Model *m);

/* Trả NULL nếu tên không hợp lệ, đã tồn tại hoặc hết bộ nhớ. */
House *model_add_house(Model *m, const char *name);
House *model_find_house(Model *m, const char *name);
/* 0 nếu thành công, -1 nếu không tìm thấy / tên mới không hợp lệ hoặc trùng. */
int    model_rename_house(Model *m, const char *old_name, const char *new_name);
int    model_remove_house(Model *m, const char *name);

Room  *house_add_room(House *h, const char *name);
Room  *house_find_room(House *h, const char *name);
int    house_rename_room(House *h, const char *old_name, const char *new_name);
int    house_remove_room(House *h, const char *name);

/* Sao chép *d vào phòng. NULL nếu id rỗng/không hợp lệ hoặc trùng trong phòng. */
DeviceRef *room_add_device(Room *r, const DeviceRef *d);
DeviceRef *room_find_device(Room *r, const char *id);
int        room_remove_device(Room *r, const char *id);

/* Tìm thiết bị trong toàn bộ model; house_out/room_out có thể NULL. */
DeviceRef *model_find_device(Model *m, const char *id, House **house_out, Room **room_out);

#endif
