#ifndef _LJSON_H_
#define _LJSON_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LJSON_ERROR_ITEM_MISS_IGNORE    /* ignore LJSON_ERROR_ITEM_MISS */
#define LJSON_ERROR_ARRAY_OVER_IGNORE   /* ignore LJSON_ERROR_ARRAY_OVER */
#define LJSON_ERROR_STRING_OVER_IGNORE  /* ignore LJSON_ERROR_STRING_OVER */

#define LJSON_FMT_TAB_SIZE      4       /* ljson_contex_snprintf fmt */

#define SIZE_OF_STACK_TYPE      (sizeof(uint8_t))   /* for LJSON_TYPE_XXX */
#define SIZE_OF_STACK_CONTEX    (sizeof(void *) + sizeof(uint16_t) + sizeof(uint32_t))   /* see ljson_contex_t */

#define LJSON_BUFFER_SIZE       256     /* buffer (with '\0') for key, value */
#define LJSON_TYPE_STACK_SIZE   (SIZE_OF_STACK_TYPE * 10)   /* type  for object '{', array '[', string '"' */
#define LJSON_CONTEX_STACK_SIZE (SIZE_OF_STACK_CONTEX * 9)  /* contex for object '{', array '[' */

#define LJSON_TYPE_OBJECT_L     0x00    /* '{' */
#define LJSON_TYPE_OBJECT_R     0x01    /* '}' */
#define LJSON_TYPE_ARRAY_L      0x02    /* '[' */
#define LJSON_TYPE_ARRAY_R      0x03    /* ']' */
#define LJSON_TYPE_KEY          0x04    /* '"' */
#define LJSON_TYPE_TOKEN        0x05    /* */
#define LJSON_TYPE_STRING       0x06    /* '"' */

#define LJSON_ITEM_OBJECT       0x00    /* struct {} */
#define LJSON_ITEM_ARRAY        0x01    /* array [] */
#define LJSON_ITEM_STRING       0x02    /* "chars" null */
#define LJSON_ITEM_INTEGER      0x03    /* int */
#define LJSON_ITEM_REAL         0x04    /* real // . e e+ e- E E+ E- */
#define LJSON_ITEM_BOOLEAN      0x05    /* true false TRUE FALSE */
#define LJSON_ITEM_CALLBACK     0x06    /* ljson_callback_t callback */

#define LJSON_ERROR_NONE        0x00
#define LJSON_ERROR_MORE        0x01
#define LJSON_ERROR_OBJECT_R    0x02
#define LJSON_ERROR_ARRAY_R     0x03
#define LJSON_ERROR_KEY_L       0x04
#define LJSON_ERROR_COLON_L     0x05
#define LJSON_ERROR_STACK_OVER  0x06
#define LJSON_ERROR_BUFFER_OVER 0x07
#define LJSON_ERROR_COMMA_R     0x08
#define LJSON_ERROR_STRING_R    0x09
#define LJSON_ERROR_OBJECT_L    0x0A
#define LJSON_ERROR_ARRAY_L     0x0B
#define LJSON_ERROR_ARRAY_OVER  0x0C
#define LJSON_ERROR_STRING_OVER 0x0D
#define LJSON_ERROR_ITEM_NAME   0x0E
#define LJSON_ERROR_ITEM_MISS   0x0F
#define LJSON_ERROR_ITEM_TYPE   0x10

////////////////////////////////////////

typedef struct _lstack
{
    uint16_t top;
    uint16_t size;
    uint8_t *buffer;
} lstack_t;

typedef uint8_t(*ljson_callback_t)(uint8_t type, uint8_t *buffer, uint16_t length, void *user);

typedef struct _ljson_parser
{
    uint8_t state;
    lstack_t stack;
    uint8_t stack_buffer[LJSON_TYPE_STACK_SIZE];
    uint8_t *pval;
    uint8_t parser_buffer[LJSON_BUFFER_SIZE];

    void *user;
    ljson_callback_t callback;
} ljson_parser_t;

////////////////////////////////////////

typedef struct _ljson_item
{
    const char *name;
    uint8_t type;
    uint16_t length;
    void *buffer;
    uint16_t offset;
} ljson_item_t;

typedef struct _ljson_contex
{
    lstack_t stack;
    uint8_t stack_buffer[LJSON_CONTEX_STACK_SIZE];

    uint16_t ljson_item_miss;

    /* push pop */
    ljson_item_t *ljson_item;
    uint16_t ljson_item_index;
    uint32_t ljson_array_offset;
} ljson_contex_t;

////////////////////////////////////////////////////////////////////////////////

#define countof(a)  (sizeof(a) / sizeof((a)[0]))

#define lstack_free(stack)          ((stack)->top)
#define lstack_used(stack)          ((stack)->size - (stack)->top)
#define lstack_is_empty(stack)      ((stack)->top >= (stack)->size)
#define lstack_push(stack, value)   ((stack)->buffer[--((stack)->top)] = value)
#define lstack_pop(stack)           ((stack)->buffer[((stack)->top)++])
#define lstack_top(stack)           ((stack)->buffer[(stack)->top])

////////////////////////////////////////

void lstack_init(lstack_t *stack, uint8_t *buffer, uint16_t size);
void lstack_push_buffer(lstack_t *stack, const void *buffer, uint16_t size);
void *lstack_pop_buffer(lstack_t *stack, void *buffer, uint16_t size);
void *lstack_top_buffer(lstack_t *stack, void *buffer, uint16_t size);

////////////////////////////////////////

void numcpy(void *dst, uint64_t src, uint8_t size);
void realcpy(void *dst, double src, uint8_t size);
uint8_t hex_to_num(const char *str, void *num, uint8_t size, uint8_t width);
uint8_t str_to_num(const char *str, void *num, uint8_t size, uint8_t width);
uint8_t str_to_real(const char *str, void *num, uint8_t size);
uint8_t str_to_exp(const char *str, void *num, uint8_t size);
uint8_t str_to_bool(const char *str, void *num, uint8_t size);

////////////////////////////////////////

uint16_t snprintf_string(char *dst, uint16_t size, const void *src, uint16_t length);
uint16_t snprintf_token(char *dst, uint16_t size, const void *src);
uint16_t snprintf_integer(char *dst, uint16_t size, const void *src, uint16_t length);
uint16_t snprintf_real(char *dst, uint16_t size, const void *src, uint16_t length);

////////////////////////////////////////

void ljson_contex_init(ljson_contex_t *contex, const ljson_item_t *top);
uint8_t ljson_contex_push(ljson_contex_t *contex, uint8_t type);
uint8_t ljson_contex_pop(ljson_contex_t *contex, uint8_t type);
uint16_t ljson_contex_snprintf(ljson_contex_t *contex, void *buffer, uint16_t size, uint8_t fmt);

////////////////////////////////////////

void ljson_parser_init(ljson_parser_t *parser, ljson_callback_t callback, void *user);
uint8_t ljson_parser_feed(ljson_parser_t *parser, const void *buffer, uint16_t length);
uint8_t ljson_callback_default(uint8_t type, uint8_t *buffer, uint16_t length, void *user);

////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif // !_LJSON_H_
