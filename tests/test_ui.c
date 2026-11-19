#define _GNU_SOURCE

#include "test.h"
#include "model.h"
#include "ui.h"

#include <stdio.h>

static void test_menu_crud(void)
{
    Model model;
    char input_text[] =
        "2\nNha 1\n"
        "5\nNha 1\nPhong khach\n"
        "3\nNha 1\nNha moi\n"
        "6\nNha moi\nPhong khach\nPhong bep\n"
        "4\nNha moi\n"
        "0\n";
    char output_text[4096] = {0};
    FILE *in = fmemopen(input_text, sizeof input_text - 1, "r");
    FILE *out = fmemopen(output_text, sizeof output_text, "w");

    model_init(&model);
    CHECK(in != NULL && out != NULL);
    if (in && out) {
        CHECK(ui_menu(&model, in, out) == 0);
        fflush(out);
        CHECK(model.n_houses == 0);
        CHECK(strstr(output_text, "Da them nha.") != NULL);
        CHECK(strstr(output_text, "Da them phong.") != NULL);
        CHECK(strstr(output_text, "Da sua nha.") != NULL);
        CHECK(strstr(output_text, "Da sua phong.") != NULL);
        CHECK(strstr(output_text, "Da xoa nha.") != NULL);
    }
    if (in)
        fclose(in);
    if (out)
        fclose(out);
    model_free(&model);
}

static void test_menu_rejects_bad_input(void)
{
    Model model;
    char input_text[] =
        "x\n"
        "2\n"
        "\n"
        "0\n";
    char output_text[2048] = {0};
    FILE *in = fmemopen(input_text, sizeof input_text - 1, "r");
    FILE *out = fmemopen(output_text, sizeof output_text, "w");

    model_init(&model);
    CHECK(ui_menu(&model, in, out) == 0);
    fflush(out);
    CHECK(model.n_houses == 0);
    CHECK(strstr(output_text, "lua chon khong hop le") != NULL);
    CHECK(strstr(output_text, "khong the them nha") != NULL);
    fclose(in);
    fclose(out);
    model_free(&model);
}

static void test_menu_errors_and_listing(void)
{
    Model model;
    char input_text[] =
        "1\n"
        "5\nKhong co\n"
        "6\nKhong co\n"
        "7\nKhong co\n"
        "2\nNha 1\n"
        "2\nNha 1\n"
        "3\nKhong co\nNha moi\n"
        "4\nKhong co\n"
        "5\nNha 1\nPhong\n"
        "5\nNha 1\nPhong\n"
        "6\nNha 1\nKhong co\nPhong moi\n"
        "7\nNha 1\nKhong co\n"
        "1\n"
        "0\n";
    char output_text[8192] = {0};
    FILE *in = fmemopen(input_text, sizeof input_text - 1, "r");
    FILE *out = fmemopen(output_text, sizeof output_text, "w");

    model_init(&model);
    CHECK(ui_menu(&model, in, out) == 0);
    fflush(out);
    CHECK(model.n_houses == 1);
    CHECK(model.houses[0].n_rooms == 1);
    CHECK(strstr(output_text, "Chua co nha.") != NULL);
    CHECK(strstr(output_text, "khong tim thay nha") != NULL);
    CHECK(strstr(output_text, "khong the them nha") != NULL);
    CHECK(strstr(output_text, "khong the sua nha") != NULL);
    CHECK(strstr(output_text, "khong tim thay phong") != NULL);
    fclose(in);
    fclose(out);
    model_free(&model);
}

static void test_menu_handles_eof_and_long_input(void)
{
    Model model;
    char long_input[] = "12345678901234567\n0\n";
    char output_text[1024] = {0};
    FILE *in = fmemopen(long_input, sizeof long_input - 1, "r");
    FILE *out = fmemopen(output_text, sizeof output_text, "w");

    model_init(&model);
    CHECK(ui_menu(&model, in, out) == 0);
    fflush(out);
    CHECK(strstr(output_text, "du lieu qua dai") != NULL);
    fclose(in);
    fclose(out);

    in = fmemopen("", 0, "r");
    out = fmemopen(output_text, sizeof output_text, "w");
    CHECK(ui_menu(&model, in, out) == 0);
    fclose(in);
    fclose(out);
    model_free(&model);
}

static void test_menu_invalid_arguments(void)
{
    Model model;

    model_init(&model);
    CHECK(ui_menu(NULL, stdin, stdout) == -1);
    CHECK(ui_menu(&model, NULL, stdout) == -1);
    CHECK(ui_menu(&model, stdin, NULL) == -1);
    model_free(&model);
}

int main(void)
{
    puts("test_ui");
    RUN(test_menu_crud);
    RUN(test_menu_rejects_bad_input);
    RUN(test_menu_errors_and_listing);
    RUN(test_menu_handles_eof_and_long_input);
    RUN(test_menu_invalid_arguments);
    return TEST_DONE();
}
