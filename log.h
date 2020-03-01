#ifndef DOKAN_LOG_H
#define DOKAN_LOG_H
#include <string>

#define LOG_RETURN(func_name, ret_val)             \
    log_msg(#func_name " return: " #ret_val "\n"); \
    return (ret_val)

#define log_struct(indent, st, field, format, ...) \
    log_msg(indent "(" #st ")->" #field " = " format "\n", __VA_ARGS__(st->field))

#define log_int32(indent, name) log_msg(indent #name " = %d\n", name)

#define log_int64(indent, name) log_msg(indent #name " = %lld\n", name)

#define log_uint32(indent, name) log_msg(indent #name " = 0x%08x\n", name)

#define log_uint64(indent, name) log_msg(indent #name " = 0x%016x\n", name)

#define log_bool(indent, name) log_msg(indent #name " = %s\n", name ? "true" : "false");

#define log_pointer(indent, name) log_msg(indent #name " = %p\n", name)

const char *set_log_name(const char *name);

void log_msg(const char *format, ...);

#endif