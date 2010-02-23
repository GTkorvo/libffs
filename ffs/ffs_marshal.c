#include "config.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
#include <ffs.h>
#include "cercs_env.h"
#include "cod.h"
#include "ffs_internal.h"
#include "ffs_marshal.h"

extern field_marshal_info
get_marshal_info(FMFormat f, FMTypeDesc *t)
{
    format_marshal_info info = (format_marshal_info) f->ffs_info;
    int i;
    if (info == NULL) return NULL;
    for (i=0; i < info->count; i++) {
	if (info->field_info[i].t == t) {
	    return &info->field_info[i];
	}
    }
    return NULL;
}

extern sm_ref
cod_build_type_node(const char *name, FMFieldList field_list);
extern sm_ref
cod_build_param_node(const char *id, sm_ref typ, int param_num);

static void
add_param(cod_parse_context parse_context, char *name, int param_num,
	  FMFormat format)
{
    FMStructDescList list = format_list_of_FMFormat(format);
    int i = 1;
    sm_ref type, param;
    while (list[i].format_name != NULL) {
	FMFieldList fl = list[i].field_list;
	/* step through input formats */
	cod_add_struct_type(list[i].format_name, fl, parse_context);
	i++;
    }
    type = cod_build_type_node(list[0].format_name, list[0].field_list);
    cod_add_decl_to_parse_context(list[0].format_name, type, parse_context);

    param = cod_build_param_node(name, type, param_num);

    cod_add_decl_to_parse_context(name, param, parse_context);
}

field_marshal_info
add_marshal_info(FMFormat f)
{
    format_marshal_info info = (format_marshal_info) f->ffs_info;
    if (info == NULL) {
	f->ffs_info = info = malloc(sizeof(struct marshal_info));
	info->count = 1;
	info->field_info = malloc(sizeof(info->field_info[0]));
    } else {
	info->count++;
	info->field_info = realloc(info->field_info,
				   sizeof(info->field_info[0]) * info->count);
    }
    return &info->field_info[info->count - 1];
}

extern void
install_drop_code(FMFormat f, char *field, char*code_str)
{
    cod_code code;
    int (*func)(void*);
    field_marshal_info marshal_info;
    cod_parse_context parse_context = new_cod_parse_context();
    int i, field_num = -1;

    static char extern_string[] = "\
		int printf(string format, ...);\n\
		void *malloc(int size);\n\
		void free(void *pointer);";

    static cod_extern_entry externs[] = {
	{"printf", (void *) 0},
	{"malloc", (void*) 0},
	{"free", (void*) 0},
	{(void *) 0, (void *) 0}
    };

    /*
     * some compilers think it isn't a static initialization to put this
     * in the structure above, so do it explicitly.
     */
    externs[0].extern_value = (void *) (long) printf;
    externs[1].extern_value = (void *) (long) malloc;
    externs[2].extern_value = (void *) (long) free;


    for (i=0; i< f->field_count; i++) {
	if (strcmp(f->field_list[i].field_name, field) == 0) field_num = i;
    }
    if (field_num == -1) {
	printf("field \"%s\" not found in install drop code\n", field);
	return;
    }
    add_param(parse_context, "input", 0, f);
    cod_assoc_externs(parse_context, externs);
    cod_parse_for_context(extern_string, parse_context);

    code = cod_code_gen(code_str, parse_context);
    cod_free_parse_context(parse_context);
    if (code == NULL) {
	printf("Compilation failed, field \"%s\" in install drop code \n", field);
	return;
    }
    func = (int(*)(void *))code->func;
    marshal_info = add_marshal_info(f);
    marshal_info->t = &f->var_list[field_num].type_desc;
    marshal_info->type = FFSDropField;
    marshal_info->drop_row_func = func;
}

