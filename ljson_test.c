#include <stdio.h>
#include "ljson.h"

////////////////////////////////////////

typedef struct _student
{
    char name[32];
    uint8_t old;
    float height;
    double width;
    uint8_t boy;
} student_t;

typedef struct _school
{
    char name[32];
    uint16_t number;
    student_t teacher;
    student_t student[30];
    uint16_t height[20];
    double width[10][10];
} school_t;

////////////////////////////////////////////////////////////////////////////////

school_t school;

////////////////////////////////////////

uint8_t ljson_item_callback(uint8_t type, uint8_t *buffer, uint16_t length, void *user)
{
    ljson_contex_t *ctx = (ljson_contex_t *)user;

    return LJSON_ERROR_NONE;
}

////////////////////////////////////////

static const ljson_item_t ljson_teacher[] =
{
    { "name", LJSON_ITEM_STRING, sizeof(school.student[0].name), school.student[0].name },
    { "old", LJSON_ITEM_INTEGER, sizeof(school.student[0].old), &school.student[0].old },
    { "height", LJSON_ITEM_REAL, sizeof(school.student[0].height), &school.student[0].height },
    { "width", LJSON_ITEM_REAL, sizeof(school.student[0].width), &school.student[0].width },
    { "boy", LJSON_ITEM_BOOLEAN, sizeof(school.student[0].boy), &school.student[0].boy },
};

////////////////////////////////////////

static const ljson_item_t ljson_student[] =
{
    { "name", LJSON_ITEM_STRING, sizeof(school.student[0].name), school.student[0].name },
    { "old", LJSON_ITEM_INTEGER, sizeof(school.student[0].old), &school.student[0].old },
    { "height", LJSON_ITEM_REAL, sizeof(school.student[0].height), &school.student[0].height },
    { "width", LJSON_ITEM_REAL, sizeof(school.student[0].width), &school.student[0].width },
    { "boy", LJSON_ITEM_BOOLEAN, sizeof(school.student[0].boy), &school.student[0].boy },
};

static const ljson_item_t ljson_array_student[] =
{
    { 0, LJSON_ITEM_OBJECT, countof(ljson_student), (void *)ljson_student },
};

////////////////////////////////////////

static const ljson_item_t ljson_array_height[] =
{
    //{ 0, LJSON_ITEM_INTEGER, sizeof(school.height[0]), school.height },
    { 0, LJSON_ITEM_CALLBACK, 0, ljson_item_callback },
};

////////////////////////////////////////

static const ljson_item_t ljson_array_array_width[] =
{
    { 0, LJSON_ITEM_REAL, sizeof(school.width[0][0]), school.width },
};

static const ljson_item_t ljson_array_width[] =
{
    { 0, LJSON_ITEM_ARRAY, countof(school.width[0]), (void *)ljson_array_array_width, sizeof(school.width[0][0]) },
};

////////////////////////////////////////

static const ljson_item_t ljson_school[] =
{
    { "name", LJSON_ITEM_STRING, sizeof(school.name), school.name },
    { "number", LJSON_ITEM_INTEGER, sizeof(school.number), &school.number },
    { "teacher", LJSON_ITEM_OBJECT, countof(ljson_teacher), (void *)ljson_teacher },
    { "student", LJSON_ITEM_ARRAY, countof(school.student), (void *)ljson_array_student, sizeof(school.student[0]) },
    { "height", LJSON_ITEM_ARRAY, 0/*countof(school.height)*/, (void *)ljson_array_height, sizeof(school.height[0]) },
    { "width", LJSON_ITEM_ARRAY, countof(school.width), (void *)ljson_array_width, sizeof(school.width[0]) },
};

////////////////////////////////////////

static const ljson_item_t ljson_top[] =
{
    { 0, LJSON_ITEM_OBJECT, countof(ljson_school), (void *)ljson_school },
};

////////////////////////////////////////////////////////////////////////////////

const char str_json[] =
    "{\"name\":\"abc\",\"number\":123,\"student\":[{\"name\":\"no1\",\"old\":111,\"height\":163.2,\"width\":23.6,\"boy\":true},"
    "{\"name\":\"no2\",\"old\":22,\"height\":163.2,\"width\":23.6,\"boy\":true},{\"name\":\"no3\",\"old\":123,\"height\":163.2,"
    "\"width\":23.6,\"boy\":true},{\"name\":\"no4\",\"old\":123,\"height\":163.2,\"width\":23.6,\"boy\":true}],"
    "\"height\":[1,2,3,4,5,6,7,8],\"width\":[[1,2,3],[4,5,6],[7,8,9]]}";

int main(int argc, char* argv[])
{
    ljson_parser_t parser;
    ljson_contex_t contex;

    ljson_contex_init(&contex, ljson_top);
    ljson_parser_init(&parser, ljson_callback_default, &contex);

    uint8_t res = ljson_parser_feed(&parser, str_json, sizeof(str_json) - 1);
    printf("ljson_parser_feed:0x%02X\n", res);

    char buffer[6395];
    ljson_contex_init(&contex, ljson_top);
    int len = ljson_contex_snprintf(&contex, buffer, sizeof(buffer), 1);
    printf("ljson_contex_snprintf:%d\n", len);
    printf("%s\n", buffer);

    return 0;
}

