#include <stdio.h>
#include "ljson.h"
#include <string.h>

const char str_json[] =
    "{\"name\":\"abc\",\"number\":123,\"student\":[{\"name\":\"no1\",\"old\":111,\"height\":163.2,"
    "\"home\":[{\"row\":1,\"col\":2}],\"width\":23.6,\"boy\":true},],\"height\":[1,2,3,4,5,6,7,8],\"width\":[[1,2,3,4],[],[]]}";

#define LJSON_CUSTOM_STACK_SIZE     1024

typedef struct _ljson_custom
{
    lstack_t stack; // 解析主堆栈，存结构体，数组等层级关系
    uint8_t stack_buffer[LJSON_CUSTOM_STACK_SIZE];

    lstack_t astack; // 数组倒序堆栈
    uint8_t astack_buffer[LJSON_CUSTOM_STACK_SIZE];

    lstack_t nstack; // 成员堆栈
    uint8_t nstack_buffer[LJSON_CUSTOM_STACK_SIZE];

    lstack_t pstack; // 路径堆栈
    uint8_t pstack_buffer[LJSON_CUSTOM_STACK_SIZE];

    lstack_t tstack; // 临时堆栈，用于局部倒序
    uint8_t tstack_buffer[LJSON_CUSTOM_STACK_SIZE];

    uint16_t ljson_item_skip;
    ljson_item_t item_top;
    uint32_t ljson_item_level;

    /* push pop */
    uint16_t ljson_item_index;
} ljson_custom_t;


#define lowcase(ch) ((((ch) >= 'A') && ((ch) <= 'Z')) ? ((ch) + 'a' - 'A') : (ch))

static uint8_t _lowcase_cmp(const char *s1, const char *s2)
{
    while ((*s1 != '\0') && (lowcase(*s1) == *s2))
    {
        s1++;
        s2++;
    }

    return lowcase(*s1) - *s2;
}

const char *ljson_token_name(uint8_t type)
{
    switch (type)
    {
    case LJSON_ITEM_STRING:
        return "char";
    case LJSON_ITEM_INTEGER:
        return "long";
    case LJSON_ITEM_REAL:
        return "double";
    case LJSON_ITEM_BOOLEAN:
        return "int";
    }

    return "";
}

const char *ljson_item_name(uint8_t type)
{
    switch (type)
    {
    case LJSON_ITEM_STRING:
        return "LJSON_ITEM_STRING";
    case LJSON_ITEM_INTEGER:
        return "LJSON_ITEM_INTEGER";
    case LJSON_ITEM_REAL:
        return "LJSON_ITEM_REAL";
    case LJSON_ITEM_BOOLEAN:
        return "LJSON_ITEM_BOOLEAN";
    }

    return "";
}

uint8_t ljson_token_type(uint8_t type, const uint8_t *buffer, uint16_t length)
{
    uint16_t i;

    if (type == LJSON_TYPE_STRING)
    {
        return LJSON_ITEM_STRING;
    }
    if (_lowcase_cmp((const char *)buffer, "null") == 0)
    {
        return LJSON_ITEM_STRING;
    }

    if (_lowcase_cmp((const char *)buffer, "true") == 0)
    {
        return LJSON_ITEM_BOOLEAN;
    }
    if (_lowcase_cmp((const char *)buffer, "false") == 0)
    {
        return LJSON_ITEM_BOOLEAN;
    }

    for (i = 0; i < length; i++)
    {
        if (!((buffer[i] >= '0') && (buffer[i] <= '9')))
        {
            break;
        }
    }
    if (i >= length)
    {
        return LJSON_ITEM_INTEGER;
    }

    for (i = 0; i < length; i++)
    {
        if (!((buffer[i] >= '0') && (buffer[i] <= '9'))
            && (buffer[i] != '.')
            && (buffer[i] != '+')
            && (buffer[i] != '-')
            && (lowcase(buffer[i]) != 'e'))
        {
            break;
        }
    }
    if (i >= length)
    {
        return LJSON_ITEM_REAL;
    }

    return LJSON_ITEM_INTEGER;
}

char message_buffer[2][10240];

void message_show(uint8_t index, uint8_t level, const char *msg)
{
    while (level--)
    {
        strcat(message_buffer[index], "    ");
    }

    strcat(message_buffer[index], msg);
}

#define append_sprintf(dst,fmt,...) sprintf(&((char*)(dst))[strlen(dst)],fmt,__VA_ARGS__)

uint8_t ljson_callback_custom(uint8_t type, uint8_t *buffer, uint16_t length, void *user)
{
    static char str_tmp[1024];
    static char str_path[1024];
    static char str_member[1024];
    static char buffer_tmp[1024];

    ljson_custom_t *custom = (ljson_custom_t *)user;
    uint8_t type_tmp;

    ljson_item_t *item_top;
    uint16_t ljson_item_count;
    uint16_t length_tmp;

    item_top = &custom->item_top;
    switch (type)
    {
    case LJSON_TYPE_OBJECT_L:
    case LJSON_TYPE_ARRAY_L:
        if (custom->ljson_item_skip > 0)
        {
            custom->ljson_item_skip++;
            break;
        }
        if (!lstack_is_empty(&custom->stack))
        {
            if (item_top->type == LJSON_TYPE_ARRAY_L)
            {
                if (custom->ljson_item_index > 0)
                {
                    custom->ljson_item_skip++;
                    break;
                }
            }
        }

        ////////////////////////////////////////////////////////////////////////

        if (!lstack_is_empty(&custom->stack) && (lstack_top(&custom->stack) == LJSON_TYPE_KEY))
        {
            lstack_pop(&custom->stack);
            lstack_pop_buffer(&custom->stack, &length_tmp, sizeof(length_tmp));
            lstack_pop_buffer(&custom->stack, buffer_tmp, length_tmp);

            lstack_push_buffer(&custom->stack, buffer_tmp, length_tmp);
            lstack_push_buffer(&custom->stack, &length_tmp, sizeof(length_tmp));
            lstack_push(&custom->stack, LJSON_TYPE_KEY);

            lstack_push_buffer(&custom->pstack, buffer_tmp, length_tmp);
            lstack_push_buffer(&custom->pstack, &length_tmp, sizeof(length_tmp));
            lstack_push(&custom->pstack, LJSON_TYPE_KEY);
        }
        lstack_push(&custom->pstack, type);

        lstack_push(&custom->nstack, 0x80 | ((type == LJSON_TYPE_OBJECT_L) ? LJSON_ITEM_OBJECT : LJSON_ITEM_ARRAY));
        lstack_push(&custom->nstack, type);

        ////////////////////////////////////////////////////////////////////////

        lstack_push_buffer(&custom->stack, &custom->ljson_item_index, sizeof(custom->ljson_item_index));
        lstack_push(&custom->stack, type);
        item_top->type = lstack_top(&custom->stack);
        custom->ljson_item_index = 0;
        if (type == LJSON_TYPE_OBJECT_L)
        {
            message_show(0,custom->ljson_item_level, "struct\r\n");
            message_show(0,custom->ljson_item_level, "{\r\n");
            custom->ljson_item_level++;
        }
        break;
    case LJSON_TYPE_OBJECT_R:
    case LJSON_TYPE_ARRAY_R:
        if (custom->ljson_item_skip > 0)
        {
            custom->ljson_item_skip--;
            if (!custom->ljson_item_skip)
            {
                custom->ljson_item_index++;
            }
            break;
        }
        if (type == LJSON_TYPE_OBJECT_R)
        {
            custom->ljson_item_level--;
        }
        ljson_item_count = custom->ljson_item_index;

        ////////////////////////////////////////////////////////////////////////

        while (!lstack_is_empty(&custom->pstack))
        {
            type_tmp = lstack_pop(&custom->pstack);
            if (lstack_top(&custom->pstack) == LJSON_TYPE_KEY)
            {
                lstack_pop(&custom->pstack);
                lstack_pop_buffer(&custom->pstack, &length_tmp, sizeof(length_tmp));
                lstack_pop_buffer(&custom->pstack, buffer_tmp, length_tmp);

                lstack_push_buffer(&custom->tstack, buffer_tmp, length_tmp);
                lstack_push_buffer(&custom->tstack, &length_tmp, sizeof(length_tmp));
                lstack_push(&custom->tstack, LJSON_TYPE_KEY);
            }
            lstack_push(&custom->tstack, type_tmp);
        }
        strcpy(str_path, "ljson_top");
        strcpy(str_member, "top");
        while (!lstack_is_empty(&custom->tstack))
        {
            type_tmp = lstack_pop(&custom->tstack);
            if (lstack_top(&custom->tstack) == LJSON_TYPE_KEY)
            {
                lstack_pop(&custom->tstack);
                lstack_pop_buffer(&custom->tstack, &length_tmp, sizeof(length_tmp));
                lstack_pop_buffer(&custom->tstack, buffer_tmp, length_tmp);

                lstack_push_buffer(&custom->pstack, buffer_tmp, length_tmp);
                lstack_push_buffer(&custom->pstack, &length_tmp, sizeof(length_tmp));
                lstack_push(&custom->pstack, LJSON_TYPE_KEY);

                buffer_tmp[length_tmp] = '\0';
                append_sprintf(str_path, "_%s", buffer_tmp);
                append_sprintf(str_member, ".%s", buffer_tmp);
            }
            lstack_push(&custom->pstack, type_tmp);

            if (type_tmp == LJSON_TYPE_OBJECT_L)
            {
                append_sprintf(str_path, "_object", buffer_tmp);
            }
            else
            {
                append_sprintf(str_path, "_array", buffer_tmp);
                append_sprintf(str_member, "[0]");
            }
        }
        strcpy(str_tmp, "static const ljson_item_t ");
        append_sprintf(str_tmp, "%s", str_path);
        append_sprintf(str_tmp, "%s", "[] =\r\n{\r\n");
        message_show(1,0, str_tmp);

        {
            uint8_t type_wait = (type == LJSON_TYPE_OBJECT_R) ? LJSON_TYPE_OBJECT_L : LJSON_TYPE_ARRAY_L;
            while ((type_tmp = lstack_top(&custom->nstack)) != type_wait)
            {
                lstack_pop(&custom->nstack);
                if (lstack_top(&custom->nstack) == LJSON_TYPE_KEY)
                {
                    lstack_pop(&custom->nstack);
                    lstack_pop_buffer(&custom->nstack, &length_tmp, sizeof(length_tmp));
                    lstack_pop_buffer(&custom->nstack, buffer_tmp, length_tmp);

                    lstack_push_buffer(&custom->tstack, buffer_tmp, length_tmp);
                    lstack_push_buffer(&custom->tstack, &length_tmp, sizeof(length_tmp));
                    lstack_push(&custom->tstack, LJSON_TYPE_KEY);
                }
                lstack_push(&custom->tstack, type_tmp);
            }
            lstack_pop(&custom->nstack);

            while (!lstack_is_empty(&custom->tstack))
            {
                type_tmp = lstack_pop(&custom->tstack);

                strcpy(str_tmp, "{");
                if (lstack_top(&custom->tstack) == LJSON_TYPE_KEY)
                {
                    lstack_pop(&custom->tstack);
                    lstack_pop_buffer(&custom->tstack, &length_tmp, sizeof(length_tmp));
                    lstack_pop_buffer(&custom->tstack, buffer_tmp, length_tmp);

                    buffer_tmp[length_tmp] = '\0';
                    append_sprintf(str_tmp, "\"%s\",", buffer_tmp);
                }
                else
                {
                    length_tmp = 0;
                    append_sprintf(str_tmp, "0,");
                }

                switch (type_tmp & 0x7F)
                {
                case LJSON_ITEM_OBJECT:
                    append_sprintf(str_tmp, "LJSON_ITEM_OBJECT,");
                    if (length_tmp > 0)
                    {
                        append_sprintf(str_tmp, "countof(%s_%s_object),", str_path, buffer_tmp);
                        append_sprintf(str_tmp, "(void*)%s_%s_object", str_path, buffer_tmp);
                    }
                    else
                    {
                        append_sprintf(str_tmp, "countof(%s_object),", str_path);
                        append_sprintf(str_tmp, "(void*)%s_object", str_path);
                    }
                    break;
                case LJSON_ITEM_ARRAY:
                    append_sprintf(str_tmp, "LJSON_ITEM_ARRAY,");
                    if (length_tmp > 0)
                    {
                        append_sprintf(str_tmp, "countof(%s.%s),", str_member, buffer_tmp);
                        append_sprintf(str_tmp, "(void*)%s_%s_array,", str_path, buffer_tmp);
                        append_sprintf(str_tmp, "sizeof(%s.%s[0])", str_member, buffer_tmp);
                    }
                    else
                    {
                        append_sprintf(str_tmp, "countof(%s),", str_member);
                        append_sprintf(str_tmp, "(void*)%s_array,", str_path);
                        append_sprintf(str_tmp, "sizeof(%s[0])", str_member);
                    }
                    break;
                case LJSON_ITEM_STRING:
                case LJSON_ITEM_INTEGER:
                case LJSON_ITEM_REAL:
                case LJSON_ITEM_BOOLEAN:
                    append_sprintf(str_tmp, "%s,", ljson_item_name(type_tmp & 0x7F));
                    if (length_tmp > 0)
                    {
                        append_sprintf(str_tmp, "sizeof(%s.%s),", str_member, buffer_tmp);
                        append_sprintf(str_tmp, "&%s.%s", str_member, buffer_tmp);
                    }
                    else
                    {
                        append_sprintf(str_tmp, "sizeof(%s),", str_member);
                        append_sprintf(str_tmp, "&%s", str_member);
                    }
                    break;
                }
                append_sprintf(str_tmp, "},\r\n");
                message_show(1,1, str_tmp);
            }
        }

        strcpy(str_tmp, "};\r\n\r\n");
        message_show(1,0, str_tmp);

        lstack_pop(&custom->pstack);
        if (lstack_top(&custom->pstack) == LJSON_TYPE_KEY)
        {
            lstack_pop(&custom->pstack);
            lstack_pop_buffer(&custom->pstack, &length_tmp, sizeof(length_tmp));
            lstack_pop_buffer(&custom->pstack, buffer_tmp, length_tmp);
        }
        if (lstack_is_empty(&custom->pstack))
        {
            strcpy(str_path, "ljson_top");

            strcpy(str_tmp, "static const ljson_item_t ");
            append_sprintf(str_tmp, "%s", str_path);
            append_sprintf(str_tmp, "%s", "[] =\r\n{\r\n");
            message_show(1,0, str_tmp);

            strcpy(str_tmp, "{");
            append_sprintf(str_tmp, "0,");

            type_tmp = lstack_top(&custom->nstack);
            switch (type_tmp & 0x7F)
            {
            case LJSON_ITEM_OBJECT:
                append_sprintf(str_tmp, "LJSON_ITEM_OBJECT,");
                append_sprintf(str_tmp, "countof(%s_object),", str_path);
                append_sprintf(str_tmp, "(void*)%s_object", str_path);
                break;
            case LJSON_ITEM_ARRAY:
                append_sprintf(str_tmp, "LJSON_ITEM_ARRAY,");
                append_sprintf(str_tmp, "countof(%s_array),", str_path);
                append_sprintf(str_tmp, "(void*)%s_array", str_path);
                break;
            }
            append_sprintf(str_tmp, "},\r\n");
            message_show(1,1, str_tmp);
            lstack_pop(&custom->nstack);

            strcpy(str_tmp, "};\r\n\r\n");
            message_show(1,0, str_tmp);
        }


        ////////////////////////////////////////////////////////////////////////

        lstack_pop(&custom->stack);
        lstack_pop_buffer(&custom->stack, &custom->ljson_item_index, sizeof(custom->ljson_item_index));
        if (lstack_top(&custom->stack) == LJSON_TYPE_KEY)
        {
            lstack_pop(&custom->stack);
            lstack_pop_buffer(&custom->stack, &length_tmp, sizeof(length_tmp));
            lstack_pop_buffer(&custom->stack, buffer_tmp, length_tmp);
            buffer_tmp[length_tmp] = '\0';
            if (type == LJSON_TYPE_OBJECT_R)
            {
                sprintf(str_tmp, "} %s;\r\n", buffer_tmp);
                message_show(0,custom->ljson_item_level, str_tmp);
            }
            else
            {
                sprintf(str_tmp, "%s[%d]", buffer_tmp, ljson_item_count);
                message_show(0,0, str_tmp);
                while (!lstack_is_empty(&custom->astack))
                {
                    lstack_pop_buffer(&custom->astack, &ljson_item_count, sizeof(ljson_item_count));
                    sprintf(str_tmp, "[%d]", ljson_item_count);
                    message_show(0,0, str_tmp);
                }
                sprintf(str_tmp, ";\r\n");
                message_show(0,0, str_tmp);
            }

        }
        else
        {
            if (lstack_is_empty(&custom->stack))
            {
                if (type == LJSON_TYPE_OBJECT_R)
                {
                    sprintf(str_tmp, "} top;\r\n");
                    message_show(0,custom->ljson_item_level, str_tmp);
                }
                else
                {
                    sprintf(str_tmp, "top[%d]", ljson_item_count);
                    message_show(0,0, str_tmp);
                    while (!lstack_is_empty(&custom->astack))
                    {
                        lstack_pop_buffer(&custom->astack, &ljson_item_count, sizeof(ljson_item_count));
                        sprintf(str_tmp, "[%d]", ljson_item_count);
                        message_show(0,0, str_tmp);
                    }
                    sprintf(str_tmp, ";\r\n");
                    message_show(0,0, str_tmp);
                }
            }
            else
            {
                if (type == LJSON_TYPE_OBJECT_R)
                {
                    sprintf(str_tmp, "} ");
                    message_show(0,custom->ljson_item_level, str_tmp);
                }
                else
                {
                    lstack_push_buffer(&custom->astack, &ljson_item_count, sizeof(ljson_item_count));
                }
            }

        }

        custom->ljson_item_index++;
        if (!lstack_is_empty(&custom->stack))
        {
            item_top->type = lstack_top(&custom->stack);
        }
        break;
    case LJSON_TYPE_KEY:
        if (custom->ljson_item_skip == 0)
        {
            lstack_push_buffer(&custom->stack, buffer, length);
            lstack_push_buffer(&custom->stack, &length, sizeof(length));
            lstack_push(&custom->stack, LJSON_TYPE_KEY);

            lstack_push_buffer(&custom->nstack, buffer, length);
            lstack_push_buffer(&custom->nstack, &length, sizeof(length));
            lstack_push(&custom->nstack, LJSON_TYPE_KEY);
        }
        break;
    case LJSON_TYPE_TOKEN:
    case LJSON_TYPE_STRING:
        if (custom->ljson_item_skip == 0)
        {
            if (item_top->type == LJSON_TYPE_OBJECT_L)
            {
                lstack_pop(&custom->stack);
                lstack_pop_buffer(&custom->stack, &length_tmp, sizeof(length_tmp));
                lstack_pop_buffer(&custom->stack, buffer_tmp, length_tmp);
                buffer_tmp[length_tmp] = '\0';

                type_tmp = ljson_token_type(type, buffer, length);
                if (type_tmp != LJSON_ITEM_STRING)
                {
                    sprintf(str_tmp, "%s %s;\r\n", ljson_token_name(type_tmp), buffer_tmp);
                }
                else
                {
                    sprintf(str_tmp, "%s %s[%d];\r\n", ljson_token_name(type_tmp), buffer_tmp, length);
                }
                message_show(0,custom->ljson_item_level, str_tmp);

                lstack_push(&custom->nstack, 0x80 | type_tmp);
            }
            else
            {
                if (custom->ljson_item_index == 0)
                {
                    type_tmp = ljson_token_type(type, buffer, length);
                    if (type_tmp != LJSON_ITEM_STRING)
                    {
                        sprintf(str_tmp, "%s ", ljson_token_name(type_tmp));
                    }
                    else
                    {
                        sprintf(str_tmp, "%s ", ljson_token_name(type_tmp));
                        lstack_push_buffer(&custom->astack, &length, sizeof(length));
                    }
                    message_show(0,custom->ljson_item_level, str_tmp);

                    lstack_push(&custom->nstack, 0x80 | type_tmp);
                }
            }

            custom->ljson_item_index++;
        }
        break;
    }

    return LJSON_ERROR_NONE;
}

const char *ljson_error_name(uint8_t error)
{
    switch (error)
    {
    case LJSON_ERROR_NONE:
        return "LJSON_ERROR_NONE";
    case LJSON_ERROR_MORE:
        return "LJSON_ERROR_MORE";
    case LJSON_ERROR_OBJECT_R:
        return "LJSON_ERROR_OBJECT_R";
    case LJSON_ERROR_ARRAY_R:
        return "LJSON_ERROR_ARRAY_R";
    case LJSON_ERROR_KEY_L:
        return "LJSON_ERROR_KEY_L";
    case LJSON_ERROR_COLON_L:
        return "LJSON_ERROR_COLON_L";
    case LJSON_ERROR_STACK_OVER:
        return "LJSON_ERROR_STACK_OVER";
    case LJSON_ERROR_BUFFER_OVER:
        return "LJSON_ERROR_BUFFER_OVER";
    case LJSON_ERROR_COMMA_R:
        return "LJSON_ERROR_COMMA_R";
    case LJSON_ERROR_STRING_R:
        return "LJSON_ERROR_STRING_R";
    case LJSON_ERROR_OBJECT_L:
        return "LJSON_ERROR_OBJECT_L";
    case LJSON_ERROR_ARRAY_L:
        return "LJSON_ERROR_ARRAY_L";
    case LJSON_ERROR_ARRAY_OVER:
        return "LJSON_ERROR_ARRAY_OVER";
    case LJSON_ERROR_STRING_OVER:
        return "LJSON_ERROR_STRING_OVER";
    case LJSON_ERROR_ITEM_NAME:
        return "LJSON_ERROR_ITEM_NAME";
    case LJSON_ERROR_ITEM_MISS:
        return "LJSON_ERROR_ITEM_MISS";
    case LJSON_ERROR_ITEM_TYPE:
        return "LJSON_ERROR_ITEM_TYPE";
    }

    return "";
}

int main(int argc, char* argv[])
{
    ljson_parser_t parser;
    ljson_custom_t m_custom;

    memset(&m_custom, 0, sizeof(ljson_custom_t));

    lstack_init(&m_custom.stack, m_custom.stack_buffer, sizeof(m_custom.stack_buffer));
    lstack_init(&m_custom.astack, m_custom.astack_buffer, sizeof(m_custom.astack_buffer));
    lstack_init(&m_custom.nstack, m_custom.nstack_buffer, sizeof(m_custom.nstack_buffer));
    lstack_init(&m_custom.pstack, m_custom.pstack_buffer, sizeof(m_custom.pstack_buffer));
    lstack_init(&m_custom.tstack, m_custom.tstack_buffer, sizeof(m_custom.tstack_buffer));
    ljson_parser_init(&parser, ljson_callback_custom, &m_custom);

    uint8_t res = ljson_parser_feed(&parser, str_json, sizeof(str_json) - 1);
    printf("ljson_parser_feed: %s\r\n", ljson_error_name(res));

    printf("\r\n////////////////////////////////////////////////////////////////////////////////\r\n\r\n");
    printf(message_buffer[0]);
    printf("\r\n////////////////////////////////////////////////////////////////////////////////\r\n\r\n");
    printf(message_buffer[1]);
    printf("////////////////////////////////////////////////////////////////////////////////\r\n");

	return 0;
}

