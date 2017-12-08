#include "ljson.h"
#include <string.h> /* memset, memcpy, memcmp */
#include <stdio.h> /* _snprintf */
#include <math.h> /* pow */

#define LJSON_AWAIT_KEY         0x00    /* '"', '}', pop LJSON_IN_KEY */
#define LJSON_IN_KEY            0x01    /* push LJSON_IN_KEY */
#define LJSON_AWAIT_COLON       0x02    /* ':' */
#define LJSON_AWAIT_VALUE       0x03    /* '{', '[', ']', '"', other, pop LJSON_IN_VAL_ARRAY */
#define LJSON_IN_VAL_OBJECT     0x04    /* push LJSON_IN_VAL_OBJECT */
#define LJSON_IN_VAL_ARRAY      0x05    /* push LJSON_IN_VAL_ARRAY */
#define LJSON_IN_VAL_STRING     0x06    /* push LJSON_IN_VAL_STRING */
#define LJSON_IN_VAL_TOKEN      0x07    /* '}', ']', ',' */
#define LJSON_AWAIT_COMMA       0x08    /* '}', ']', ',' */
#define LJSON_IN_STRING         0x09    /* '"', '\\', pop LJSON_IN_KEY, pop LJSON_IN_VAL_STRING */
#define LJSON_IN_STR_ESCAPE     0x0A    /* \b \f \n \r \t \u1234 */

#define lowcase(ch) ((((ch) >= 'A') && ((ch) <= 'Z')) ? ((ch) + 'a' - 'A') : (ch))

////////////////////////////////////////////////////////////////////////////////

void lstack_init(lstack_t *stack, uint8_t *buffer, uint16_t size)
{
    memset(stack, 0, sizeof(lstack_t));

    stack->top = size;
    stack->size = size;
    stack->buffer = (uint8_t*)buffer;
}

void *lstack_top_buffer(lstack_t *stack, void *buffer, uint16_t size)
{
    uint8_t *ptop = &stack->buffer[stack->top];

    if (buffer != 0)
    {
        memcpy(buffer, ptop, size);
    }

    return ptop;
}

void lstack_push_buffer(lstack_t *stack, const void *buffer, uint16_t size)
{
    stack->top -= size;
    memcpy(&stack->buffer[stack->top], buffer, size);
}

void *lstack_pop_buffer(lstack_t *stack, void *buffer, uint16_t size)
{
    uint8_t *ptop = &stack->buffer[stack->top];

    if (buffer != 0)
    {
        memcpy(buffer, ptop, size);
    }
    stack->top += size;

    return ptop;
}

////////////////////////////////////////////////////////////////////////////////

void numcpy(void *dst, uint64_t src, uint8_t size)
{
    switch (size)
    {
    case sizeof(uint8_t) :
        *(uint8_t*)dst = (uint8_t)src;
        break;
    case sizeof(uint16_t) :
        *(uint16_t*)dst = (uint16_t)src;
        break;
    case sizeof(uint32_t) :
        *(uint32_t*)dst = (uint32_t)src;
        break;
    case sizeof(uint64_t) :
        *(uint64_t*)dst = (uint64_t)src;
        break;
    default:
        memset(dst, 0, size);
        break;
    }
}

void realcpy(void *dst, double src, uint8_t size)
{
    switch (size)
    {
    case sizeof(float) :
        *(float*)dst = (float)src;
        break;
    case sizeof(double) :
        *(double*)dst = (double)src;
        break;
    default:
        memset(dst, 0, size);
        break;
    }
}

uint8_t hex_to_num(const char *str, void *num, uint8_t size, uint8_t width)
{
    uint8_t i;
    uint64_t n_tmp = 0;
    const char *str_tmp = str;

    if (width == 0)
    {
        width = 19;
    }

    for (i = 0; i < width; i++, str++)
    {
        if ((*str >= '0') && (*str <= '9'))
        {
            n_tmp = (n_tmp << 4) + (*str - '0');
        }
        else if ((*str >= 'a') && (*str <= 'f'))
        {
            n_tmp = (n_tmp << 4) + (*str - 'a' + 0x0A);
        }
        else if ((*str >= 'A') && (*str <= 'F'))
        {
            n_tmp = (n_tmp << 4) + (*str - 'A' + 0x0A);
        }
        else
        {
            break;
        }
    }

    numcpy(num, n_tmp, size);

    return str - str_tmp;
}

uint8_t str_to_num(const char *str, void *num, uint8_t size, uint8_t width)
{
    uint8_t i;
    uint8_t neg = 0;
    uint64_t n_tmp = 0;
    const char *str_tmp = str;

    if (width == 0)
    {
        width = 19;
    }

    if (*str == '+')
    {
        str++;
    }
    else if (*str == '-')
    {
        neg = 1;
        str++;
    }
    for (i = 0; i < width; i++, str++)
    {
        if ((*str >= '0') && (*str <= '9'))
        {
            n_tmp = (n_tmp * 10) + (*str - '0');
        }
        else
        {
            break;
        }
    }
    if (neg)
    {
        n_tmp = -(int64_t)n_tmp;
    }

    numcpy(num, n_tmp, size);

    return str - str_tmp;
}

uint8_t str_to_real(const char *str, void *num, uint8_t size)
{
    double r_tmp = 0;
    int64_t n_tmp = 0;
    const char *str_tmp = str;

    str += str_to_num(str, &n_tmp, sizeof(n_tmp), 0);
    if (*str == '.')
    {
        uint64_t f_tmp;
        uint8_t f = str_to_num(++str, &f_tmp, sizeof(n_tmp), 0);
        str += f;
        r_tmp = (double)f_tmp / pow(10, f);
    }
    r_tmp += n_tmp;

    realcpy(num, r_tmp, size);

    return str - str_tmp;
}

uint8_t str_to_exp(const char *str, void *num, uint8_t size)
{
    double r_tmp = 1;
    double n_tmp = 0;
    const char *str_tmp = str;

    str += str_to_real(str, &n_tmp, sizeof(n_tmp));
    if ((*str == 'e') || (*str == 'E'))
    {
        double f_tmp;
        str += str_to_real(++str, &f_tmp, sizeof(n_tmp));
        r_tmp = pow(10, f_tmp);
    }
    r_tmp *= n_tmp;

    realcpy(num, r_tmp, size);

    return str - str_tmp;
}

static uint8_t _lowcase_cmp(const char *s1, const char *s2)
{
    while ((*s1 != '\0') && (lowcase(*s1) == *s2))
    {
        s1++;
        s2++;
    }

    return lowcase(*s1) - *s2;
}

uint8_t str_to_bool(const char *str, void *num, uint8_t size)
{
    if (_lowcase_cmp(str, "true") == 0)
    {
        numcpy(num, 1, size);
        return 4;
    }

    numcpy(num, 0, size);
    if (_lowcase_cmp(str, "false") == 0)
    {
        return 5;
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////

uint16_t snprintf_string(char *dst, uint16_t size, const void *src, uint16_t length)
{
    char *dst_tmp = dst;
    char *eod = dst + size - 1;
    const char *cp = (const char *)src;

    if (!length)
    {
        length--;
    }

    if (dst < eod) *dst = '"';
    dst++;
    for (; *cp && length--; cp++)
    {
        if (((uint8_t)(*cp) >= 0x20) && ((uint8_t)(*cp) != 0x7F) && (*cp != '\"') && (*cp != '\\'))
        {
            if (dst < eod) *dst = *cp;
            dst++;
        }
        else
        {
            if (dst < eod) *dst = '\\';
            dst++;
            switch (*cp)
            {
            case '\\':
                if (dst < eod) *dst = '\\';
                dst++;
                break;
            case '\"':
                if (dst < eod) *dst = '"';
                dst++;
                break;
            case '\b':
                if (dst < eod) *dst = 'b';
                dst++;
                break;
            case '\f':
                if (dst < eod) *dst = 'f';
                dst++;
                break;
            case '\r':
                if (dst < eod) *dst = 'r';
                dst++;
                break;
            case '\n':
                if (dst < eod) *dst = 'n';
                dst++;
                break;
            case '\t':
                if (dst < eod) *dst = 't';
                dst++;
                break;
            default:
                if (dst + (5 - 1) < eod)
                {
                    dst += _snprintf(dst, (5 + 1), "u%04x", (uint8_t)*cp);
                }
                break;
            }
        }
    }
    if (dst < eod) *dst = '"';
    dst++;
    if (eod >= dst_tmp)
    {
        if (dst <= eod) *dst = '\0';
        else *eod = '\0';
    }

    return dst - dst_tmp;
}

uint16_t snprintf_token(char *dst, uint16_t size, const void *src)
{
    uint16_t res = _snprintf(dst, size, src);

    if ((res >= size) && ((dst != 0) || (size != 0)))
    {
        if (size > 0) *dst = '\0';
        return snprintf_token(0, 0, src);
    }

    return res;
}

uint16_t snprintf_integer(char *dst, uint16_t size, const void *src, uint16_t length)
{
    uint16_t res = 0;

    switch (length)
    {
    case sizeof(int8_t) :
        res = _snprintf(dst, size, "%d", *(int8_t*)src);
        break;
    case sizeof(int16_t) :
        res = _snprintf(dst, size, "%d", *(int16_t*)src);
        break;
    case sizeof(int32_t) :
        res = _snprintf(dst, size, "%d", *(int32_t*)src);
        break;
    case sizeof(int64_t) :
        res = _snprintf(dst, size, "%ld", *(int64_t*)src);
        break;
    }

    if ((res >= size) && ((dst != 0) || (size != 0)))
    {
        if (size > 0) *dst = '\0';
        return snprintf_integer(0, 0, src, length);
    }

    return res;
}

uint16_t snprintf_real(char *dst, uint16_t size, const void *src, uint16_t length)
{
    uint16_t res = 0;

    switch (length)
    {
    case sizeof(float) :
        res = _snprintf(dst, size, "%g", *(float*)src);
        break;
    case sizeof(double) :
        res = _snprintf(dst, size, "%g", *(double*)src);
        break;
    }

    if ((res >= size) && ((dst != 0) || (size != 0)))
    {
        if (size > 0) *dst = '\0';
        return snprintf_real(0, 0, src, length);
    }

    return res;
}

////////////////////////////////////////////////////////////////////////////////

void ljson_contex_init(ljson_contex_t *contex, const ljson_item_t *top)
{
    memset(contex, 0, sizeof(ljson_contex_t));

    contex->ljson_item = (ljson_item_t *)top;
    lstack_init(&contex->stack, contex->stack_buffer, LJSON_CONTEX_STACK_SIZE);
}

uint8_t ljson_contex_push(ljson_contex_t *contex, uint8_t type)
{
    if (contex->ljson_item == 0)
    {
        contex->ljson_item_miss++;
        return LJSON_ERROR_NONE;
    }
    switch (type)
    {
    case LJSON_TYPE_OBJECT_L:
        if (contex->ljson_item->type != LJSON_ITEM_OBJECT)
        {
            return LJSON_ERROR_OBJECT_L;
        }
        break;
    case LJSON_TYPE_ARRAY_L:
        if (contex->ljson_item->type != LJSON_ITEM_ARRAY)
        {
            return LJSON_ERROR_ARRAY_L;
        }
        break;
    default:
        break;
    }
    if (!lstack_is_empty(&contex->stack))
    {
        ljson_item_t *item_top;

        lstack_top_buffer(&contex->stack, &item_top, sizeof(item_top));
        if ((item_top->type == LJSON_ITEM_ARRAY) && (item_top->length > 0) && (contex->ljson_item_index >= item_top->length))
        {
#ifdef LJSON_ERROR_ARRAY_OVER_IGNORE
            contex->ljson_item = 0;
            contex->ljson_item_miss++;
            return LJSON_ERROR_NONE;
#else
            return LJSON_ERROR_ARRAY_OVER;
#endif
        }
    }
    if (lstack_free(&contex->stack) < (sizeof(contex->ljson_array_offset) + sizeof(contex->ljson_item_index) + sizeof(contex->ljson_item)))
    {
        return LJSON_ERROR_STACK_OVER;
    }
    lstack_push_buffer(&contex->stack, &contex->ljson_array_offset, sizeof(contex->ljson_array_offset));
    lstack_push_buffer(&contex->stack, &contex->ljson_item_index, sizeof(contex->ljson_item_index));
    lstack_push_buffer(&contex->stack, &contex->ljson_item, sizeof(contex->ljson_item));

    contex->ljson_item_index = 0;

    return LJSON_ERROR_NONE;
}

uint8_t ljson_contex_pop(ljson_contex_t *contex, uint8_t type)
{
    if (contex->ljson_item_miss > 0)
    {
        contex->ljson_item_miss--;
        return LJSON_ERROR_NONE;
    }
    lstack_pop_buffer(&contex->stack, &contex->ljson_item, sizeof(contex->ljson_item));
    lstack_pop_buffer(&contex->stack, &contex->ljson_item_index, sizeof(contex->ljson_item_index));
    lstack_pop_buffer(&contex->stack, &contex->ljson_array_offset, sizeof(contex->ljson_array_offset));
    if (!lstack_is_empty(&contex->stack))
    {
        ljson_item_t *item_top;

        contex->ljson_item_index++;
        lstack_top_buffer(&contex->stack, &item_top, sizeof(item_top));
        if (item_top->type == LJSON_ITEM_ARRAY)
        {
            contex->ljson_array_offset += item_top->offset;
        }
    }

    return LJSON_ERROR_NONE;
}

uint16_t ljson_contex_snprintf(ljson_contex_t *contex, void *buffer, uint16_t size)
{
    char *cp = (char *)buffer;
    char *cp_tmp = cp;
    char *eob = cp + size;

    uint8_t res;
    ljson_item_t *item_top;
    uint8_t *item_buffer;

    while (1)
    {
        if (!lstack_is_empty(&contex->stack))
        {
            lstack_top_buffer(&contex->stack, &item_top, sizeof(item_top));
            if (contex->ljson_item_index >= item_top->length)
            {
                cp += snprintf_token(cp, ((eob > cp) ? (eob - cp) : 0), (item_top->type == LJSON_ITEM_OBJECT) ? "}" : "]");
                res = ljson_contex_pop(contex, (item_top->type == LJSON_ITEM_OBJECT) ? LJSON_TYPE_OBJECT_R : LJSON_TYPE_ARRAY_R);
                if (res != LJSON_ERROR_NONE)
                {
                    return 0;
                }

                if (lstack_is_empty(&contex->stack))
                {
                    break;
                }

                continue;
            }
            else
            {
                if (item_top->type == LJSON_ITEM_OBJECT)
                {
                    contex->ljson_item = &((ljson_item_t *)item_top->buffer)[contex->ljson_item_index];
                }
                else /* if (item_top->type == LJSON_ITEM_ARRAY)) */
                {
                    contex->ljson_item = (ljson_item_t *)item_top->buffer;
                }

                if (contex->ljson_item_index > 0)
                {
                    cp += snprintf_token(cp, ((eob > cp) ? (eob - cp) : 0), ",");
                }
            }
        }

        if ((contex->ljson_item->type == LJSON_ITEM_OBJECT) || (contex->ljson_item->type == LJSON_ITEM_ARRAY))
        {
            if (contex->ljson_item->name != 0)
            {
                cp += snprintf_string(cp, ((eob > cp) ? (eob - cp) : 0), contex->ljson_item->name, 0);
                cp += snprintf_token(cp, ((eob > cp) ? (eob - cp) : 0), ":");
            }
            res = ljson_contex_push(contex, (contex->ljson_item->type == LJSON_ITEM_OBJECT) ? LJSON_TYPE_OBJECT_L : LJSON_TYPE_ARRAY_L);
            if (res != LJSON_ERROR_NONE)
            {
                return 0;
            }
            cp += snprintf_token(cp, ((eob > cp) ? (eob - cp) : 0), (contex->ljson_item->type == LJSON_ITEM_OBJECT) ? "{" : "[");

            continue;
        }

        if (lstack_is_empty(&contex->stack))
        {
            /* error */
            return 0;
        }

        lstack_top_buffer(&contex->stack, &item_top, sizeof(item_top));
        if (item_top->type == LJSON_ITEM_OBJECT)
        {
            if (contex->ljson_item->name == 0)
            {
                /* error */
                return 0;
            }

            cp += snprintf_string(cp, ((eob > cp) ? (eob - cp) : 0), contex->ljson_item->name, 0);
            cp += snprintf_token(cp, ((eob > cp) ? (eob - cp) : 0), ":");
        }

        item_buffer = (uint8_t*)contex->ljson_item->buffer;
        if (contex->ljson_item->type != LJSON_ITEM_CALLBACK)
        {
            item_buffer += contex->ljson_array_offset;
        }
        switch (contex->ljson_item->type)
        {
        case LJSON_ITEM_STRING: /* "chars" null */
            cp += snprintf_string(cp, ((eob > cp) ? (eob - cp) : 0), item_buffer, contex->ljson_item->length);
            break;
        case LJSON_ITEM_INTEGER: /* int */
            cp += snprintf_integer(cp, ((eob > cp) ? (eob - cp) : 0), item_buffer, contex->ljson_item->length);
            break;
        case LJSON_ITEM_REAL: /* real, . e e+ e- E E+ E- */
            cp += snprintf_real(cp, ((eob > cp) ? (eob - cp) : 0), item_buffer, contex->ljson_item->length);
            break;
        case LJSON_ITEM_BOOLEAN: /* true false TRUE FALSE */
            cp += snprintf_token(cp, ((eob > cp) ? (eob - cp) : 0), (*(uint8_t*)item_buffer) ? "true" : "false");
            break;
        case LJSON_ITEM_CALLBACK: /* callback */
            cp += snprintf_token(cp, ((eob > cp) ? (eob - cp) : 0), "callback");
            break;
        default:
            /* never here */
            break;
        }

        contex->ljson_item_index++;

        if (item_top->type == LJSON_ITEM_ARRAY)
        {
            contex->ljson_array_offset += item_top->offset;
        }
    }

    return cp - cp_tmp;
}

////////////////////////////////////////////////////////////////////////////////

void ljson_parser_init(ljson_parser_t *parser, ljson_callback_t callback, void *user)
{
    memset(parser, 0, sizeof(ljson_parser_t));

    parser->state = LJSON_AWAIT_VALUE;
    parser->user = user;
    parser->callback = callback;
    lstack_init(&parser->stack, parser->stack_buffer, LJSON_STACK_SIZE);
}

uint8_t ljson_parser_feed(ljson_parser_t *parser, const void *buffer, uint16_t length)
{
    const char *cp = (const char *)buffer;
    const char *eob = cp + length;
    uint8_t res;

    for (; cp < eob; cp++)
    {
        if (((uint8_t)(*cp) < 0x20) || ((uint8_t)(*cp) == 0x7F))
        {
            continue;
        }
        if ((parser->state != LJSON_IN_STRING) && (*cp == ' '))
        {
            continue;
        }

        switch (parser->state)
        {
        case LJSON_AWAIT_KEY: /* '"', '}' */
            if (*cp == '"')
            {
                if (lstack_free(&parser->stack) < 1)
                {
                    return LJSON_ERROR_STACK_OVER;
                }
                lstack_push(&parser->stack, LJSON_IN_KEY);
                parser->pval = parser->parser_buffer;
                parser->state = LJSON_IN_STRING;
                break;
            }
            if (*cp == '}')
            {
                if ((lstack_is_empty(&parser->stack)) || (lstack_top(&parser->stack) != LJSON_IN_VAL_OBJECT))
                {
                    return LJSON_TYPE_OBJECT_R;
                }

                res = parser->callback(LJSON_TYPE_OBJECT_R, 0, 0, parser->user);
                if (res != LJSON_ERROR_NONE)
                {
                    return res;
                }
                lstack_pop(&parser->stack);
                parser->state = LJSON_AWAIT_COMMA;
                break;
            }
            return LJSON_ERROR_KEY_L;
            /* break; */
        case LJSON_AWAIT_COLON: /* ':' */
            if (*cp == ':')
            {
                parser->state = LJSON_AWAIT_VALUE;
                break;
            }
            return LJSON_ERROR_COLON_L;
            /* break; */
        case LJSON_AWAIT_VALUE: /* '{', '[', ']', '"', other */
            if (*cp == '{')
            {
                res = parser->callback(LJSON_TYPE_OBJECT_L, 0, 0, parser->user);
                if (res != LJSON_ERROR_NONE)
                {
                    return res;
                }
                if (lstack_free(&parser->stack) < 1)
                {
                    return LJSON_ERROR_STACK_OVER;
                }
                lstack_push(&parser->stack, LJSON_IN_VAL_OBJECT);
                parser->state = LJSON_AWAIT_KEY;
                break;
            }
            if (*cp == '[')
            {
                res = parser->callback(LJSON_TYPE_ARRAY_L, 0, 0, parser->user);
                if (res != LJSON_ERROR_NONE)
                {
                    return res;
                }
                if (lstack_free(&parser->stack) < 1)
                {
                    return LJSON_ERROR_STACK_OVER;
                }
                lstack_push(&parser->stack, LJSON_IN_VAL_ARRAY);
                parser->state = LJSON_AWAIT_VALUE;
                break;
            }
            if (*cp == ']')
            {
                if ((lstack_is_empty(&parser->stack)) || (lstack_top(&parser->stack) != LJSON_IN_VAL_ARRAY))
                {
                    return LJSON_ERROR_ARRAY_R;
                }

                res = parser->callback(LJSON_TYPE_ARRAY_R, 0, 0, parser->user);
                if (res != LJSON_ERROR_NONE)
                {
                    return res;
                }
                lstack_pop(&parser->stack);
                parser->state = LJSON_AWAIT_COMMA;
                break;
            }
            if (*cp == '"')
            {
                if (lstack_free(&parser->stack) < 1)
                {
                    return LJSON_ERROR_STACK_OVER;
                }
                lstack_push(&parser->stack, LJSON_IN_VAL_STRING);
                parser->pval = parser->parser_buffer;
                parser->state = LJSON_IN_STRING;
                break;
            }

            parser->pval = parser->parser_buffer;
            cp--;
            parser->state = LJSON_IN_VAL_TOKEN;
            break;
        case LJSON_IN_VAL_TOKEN: /* '}', ']', ',' */
            if ((*cp == '}') || (*cp == ']') || (*cp == ','))
            {
                *parser->pval = '\0';
                res = parser->callback(LJSON_TYPE_TOKEN, parser->parser_buffer, parser->pval - parser->parser_buffer, parser->user);
                if (res != LJSON_ERROR_NONE)
                {
                    return res;
                }
                cp--;
                parser->state = LJSON_AWAIT_COMMA;
                break;
            }

            if (parser->pval >= parser->parser_buffer + LJSON_BUFFER_SIZE - 1)
            {
                return LJSON_ERROR_BUFFER_OVER;
            }
            *parser->pval++ = *cp;
            break;
        case LJSON_AWAIT_COMMA: /* '}', ']', ',' */
            if (*cp == '}')
            {
                cp--;
                parser->state = LJSON_AWAIT_KEY;
                break;
            }
            if (*cp == ']')
            {
                cp--;
                parser->state = LJSON_AWAIT_VALUE;
                break;
            }
            if (*cp == ',')
            {
                if (lstack_is_empty(&parser->stack))
                {
                    return LJSON_ERROR_COMMA_R;
                }
                switch (lstack_top(&parser->stack))
                {
                case LJSON_IN_VAL_OBJECT:
                    parser->state = LJSON_AWAIT_KEY;
                    break;
                case LJSON_IN_VAL_ARRAY:
                    parser->state = LJSON_AWAIT_VALUE;
                    break;
                default:
                    return LJSON_ERROR_COMMA_R;
                }
                break;
            }
            break;
        case LJSON_IN_STRING: /* '"', '\\' */
            if (*cp == '"')
            {
                if (lstack_is_empty(&parser->stack))
                {
                    return LJSON_ERROR_STRING_R;
                }
                switch (lstack_pop(&parser->stack))
                {
                case LJSON_IN_KEY:
                    *parser->pval = '\0';
                    res = parser->callback(LJSON_TYPE_KEY, parser->parser_buffer, parser->pval - parser->parser_buffer, parser->user);
                    if (res != LJSON_ERROR_NONE)
                    {
                        return res;
                    }
                    parser->state = LJSON_AWAIT_COLON;
                    break;
                case LJSON_IN_VAL_STRING:
                    *parser->pval = '\0';
                    res = parser->callback(LJSON_TYPE_STRING, parser->parser_buffer, parser->pval - parser->parser_buffer, parser->user);
                    if (res != LJSON_ERROR_NONE)
                    {
                        return res;
                    }
                    parser->state = LJSON_AWAIT_COMMA;
                    break;
                default:
                    return LJSON_ERROR_STRING_R;
                }
                break;
            }
            if (parser->pval >= parser->parser_buffer + LJSON_BUFFER_SIZE - 1)
            {
                return LJSON_ERROR_BUFFER_OVER;
            }
            if (*cp == '\\')
            {
                parser->state = LJSON_IN_STR_ESCAPE;
                break;
            }
            *parser->pval++ = *cp;
            break;
        case LJSON_IN_STR_ESCAPE: /* \b \f \n \r \t \u1234 */
            switch (*cp)
            {
            case 'b':
                *parser->pval++ = '\b';
                break;
            case 'f':
                *parser->pval++ = '\f';
                break;
            case 'n':
                *parser->pval++ = '\n';
                break;
            case 'r':
                *parser->pval++ = '\r';
                break;
            case 't':
                *parser->pval++ = '\t';
                break;
            case 'u': /* four-hex-digits, \u1234  */
                if (parser->pval + 1 >= parser->parser_buffer + LJSON_BUFFER_SIZE - 1)
                {
                    return LJSON_ERROR_BUFFER_OVER;
                }
                cp++;
                /* will truncate values above 0xFF */
                cp += hex_to_num(cp, parser->pval, 1, 4);
                parser->pval += 1;
                cp--;
                break;
            default: /* handles double quote and solidus */
                *parser->pval++ = *cp;
                break;
            }
            parser->state = LJSON_IN_STRING;
            break;
        }
    }

    if (!lstack_is_empty(&parser->stack))
    {
        return LJSON_ERROR_MORE;
    }

    return LJSON_ERROR_NONE;
}

uint8_t ljson_callback_default(uint8_t type, uint8_t *buffer, uint16_t length, void *user)
{
    ljson_contex_t *contex = (ljson_contex_t *)user;

    uint16_t i;
    uint8_t res;
    ljson_item_t *item_top;
    uint8_t *item_buffer;

    switch (type)
    {
    case LJSON_TYPE_OBJECT_L:
    case LJSON_TYPE_ARRAY_L:
        res = ljson_contex_push(contex, type);
        if (res != LJSON_ERROR_NONE)
        {
            return res;
        }
        if (type == LJSON_TYPE_ARRAY_L)
        {
            lstack_top_buffer(&contex->stack, &item_top, sizeof(item_top));
            contex->ljson_item = (ljson_item_t *)item_top->buffer;
        }
        break;
    case LJSON_TYPE_OBJECT_R:
    case LJSON_TYPE_ARRAY_R:
        res = ljson_contex_pop(contex, type);
        if (res != LJSON_ERROR_NONE)
        {
            return res;
        }
        break;
    case LJSON_TYPE_KEY:
        if (contex->ljson_item_miss > 0)
        {
            break;
        }
        /* item_top must be LJSON_ITEM_OBJECT */
        lstack_top_buffer(&contex->stack, &item_top, sizeof(item_top));
        for (i = 0; i < item_top->length; i++)
        {
            if (contex->ljson_item_index >= item_top->length)
            {
                contex->ljson_item_index = 0;
            }
            contex->ljson_item = &((ljson_item_t*)(item_top->buffer))[contex->ljson_item_index];
            if (contex->ljson_item->name == 0)
            {
                return LJSON_ERROR_ITEM_NAME;
            }
            if (memcmp(contex->ljson_item->name, buffer, length + 1) == 0)
            {
                break;
            }
            contex->ljson_item_index++;
        }
        if (i >= item_top->length)
        {
            contex->ljson_item = 0;
#ifdef LJSON_ERROR_ITEM_MISS_IGNORE
            break;
#else
            return LJSON_ERROR_ITEM_MISS;
#endif
        }
        break;
    case LJSON_TYPE_TOKEN:
    case LJSON_TYPE_STRING:
        if (contex->ljson_item == 0)
        {
            /* skip key, then skip value */
            break;
        }
        if (contex->ljson_item->buffer == 0)
        {
            /* skip value */
            break;
        }
        lstack_top_buffer(&contex->stack, &item_top, sizeof(item_top));
        if (item_top->type == LJSON_ITEM_ARRAY)
        {
            if ((item_top->length > 0) && (contex->ljson_item_index >= item_top->length))
            {
#ifdef LJSON_ERROR_ARRAY_OVER_IGNORE
                contex->ljson_item = 0;
                return LJSON_ERROR_NONE;
#else
                return LJSON_ERROR_ARRAY_OVER;
#endif
            }
        }
        item_buffer = (uint8_t*)contex->ljson_item->buffer;
        if (contex->ljson_item->type != LJSON_ITEM_CALLBACK)
        {
            item_buffer += contex->ljson_array_offset;
            memset(item_buffer, 0, contex->ljson_item->length);
        }
        switch (contex->ljson_item->type)
        {
        case LJSON_ITEM_STRING: /* "chars" null */
            if (type == LJSON_TYPE_STRING)
            {
                if (length > contex->ljson_item->length)
                {
#ifdef LJSON_ERROR_STRING_OVER_IGNORE
                    length = contex->ljson_item->length;
#else
                    return LJSON_ERROR_STRING_OVER;
#endif
                }
                memcpy(item_buffer, buffer, length);
            }
            break;
        case LJSON_ITEM_INTEGER: /* int */
            str_to_num(buffer, item_buffer, (uint8_t)contex->ljson_item->length, 0);
            break;
        case LJSON_ITEM_REAL: /* real, . e e+ e- E E+ E- */
            str_to_exp(buffer, item_buffer, (uint8_t)contex->ljson_item->length);
            break;
        case LJSON_ITEM_BOOLEAN: /* true false TRUE FALSE */
            str_to_bool(buffer, item_buffer, (uint8_t)contex->ljson_item->length);
            break;
        case LJSON_ITEM_CALLBACK:
            res = ((ljson_callback_t)item_buffer)(type, buffer, length, user);
            if (res != LJSON_ERROR_NONE)
            {
                return res;
            }
            break;
        default:
            return LJSON_ERROR_ITEM_TYPE;
            /* break; */
        }
        contex->ljson_item_index++;
        if (item_top->type == LJSON_ITEM_ARRAY)
        {
            contex->ljson_array_offset += item_top->offset;
        }
        break;
    }

    return LJSON_ERROR_NONE;
}
