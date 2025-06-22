#include "codegen.h"
#include <assert.h>
#include "lexer.h"

void generate_html_js_program_prolog(Nob_String_Builder *output)
{
    nob_sb_appendf(output, "<!DOCTYPE html>\n");
    nob_sb_appendf(output, "<html>\n");
    nob_sb_appendf(output, "<head>\n");
    nob_sb_appendf(output, "    <title>Bulan's Generated HTML-JS</title>\n");
    nob_sb_appendf(output, "</head>\n");
    nob_sb_appendf(output, "<body>\n");
    nob_sb_appendf(output, "<div id=\"console\"></div>\n");
    nob_sb_appendf(output, "    <script>\n");
    nob_sb_appendf(output, "    const putchar = (c) => {\n");
    nob_sb_appendf(output, "        if(c === 10) document.getElementById(\"console\").innerHTML += \"<br/>\"\n");
    nob_sb_appendf(output, "        document.getElementById(\"console\").innerHTML += String.fromCharCode(c)\n");
    nob_sb_appendf(output, "    }\n");
}

void generate_html_js_program_epilog(Nob_String_Builder *output)
{
    nob_sb_appendf(output, "    main();\n");
    nob_sb_appendf(output, "    </script>\n");
    nob_sb_appendf(output, "</body>\n");
    nob_sb_appendf(output, "</html>\n");
}

bool generate_html_js_function(Nob_String_Builder *output, Function *fn)
{
    nob_sb_appendf(output, "function %s() {\n", fn->name);
    nob_sb_appendf(output, "    let vars = []\n");
    nob_sb_appendf(output, "    let acc  = 0\n");
    size_t block_counter = 0;
    for(Block *b = fn->begin; b != NULL; b = b->next) {
        for(size_t i = 0; i < b->count; ++i) {
            Inst inst = b->items[i];
            switch(inst.kind) {
                case INST_NOP:
                    break;
                case INST_ADD:
                    {

                        if(!expect_inst_arg(inst, 0, ARG_LOCAL_INDEX)) return false;
                        switch(inst.args[1].kind) {
                            case ARG_LOCAL_INDEX:
                                nob_sb_appendf(output, "    acc = vars[%zu];\n",  inst.args[1].local_index);
                                break;
                            case ARG_INT_VALUE:
                                nob_sb_appendf(output, "    acc = %lld\n", inst.args[1].int_value);
                                break;
                            default:
                                compiler_diagf(inst.loc, "Invalid instruction argument 1 with type %s\n", 
                                        display_arg_kind(inst.args[1].kind));
                                break;
                        }
                        switch(inst.args[2].kind) {
                            case ARG_LOCAL_INDEX:
                                nob_sb_appendf(output, "    acc += vars[%zu]\n", inst.args[2].local_index);
                                break;
                            case ARG_INT_VALUE:
                                nob_sb_appendf(output, "    acc += %lld\n", inst.args[2].int_value);
                                break;
                            default:
                                compiler_diagf(inst.loc, "Invalid instruction argument 2 with type %s\n", 
                                        display_arg_kind(inst.args[2].kind));
                                break;
                        }
                        nob_sb_appendf(output, "    vars[%zu] = rax\n", inst.args[0].local_index);
                    }
                    break;
                case INST_SUB:
                    {

                        if(!expect_inst_arg(inst, 0, ARG_LOCAL_INDEX)) return false;
                        switch(inst.args[1].kind) {
                            case ARG_LOCAL_INDEX:
                                nob_sb_appendf(output, "    acc = vars[%zu];\n",  inst.args[1].local_index);
                                break;
                            case ARG_INT_VALUE:
                                nob_sb_appendf(output, "    acc = %lld\n", inst.args[1].int_value);
                                break;
                            default:
                                compiler_diagf(inst.loc, "Invalid instruction argument 1 with type %s\n", 
                                        display_arg_kind(inst.args[1].kind));
                                break;
                        }
                        switch(inst.args[2].kind) {
                            case ARG_LOCAL_INDEX:
                                nob_sb_appendf(output, "    acc -= vars[%zu]\n", inst.args[2].local_index);
                                break;
                            case ARG_INT_VALUE:
                                nob_sb_appendf(output, "    acc -= %lld\n", inst.args[2].int_value);
                                break;
                            default:
                                compiler_diagf(inst.loc, "Invalid instruction argument 2 with type %s\n", 
                                        display_arg_kind(inst.args[2].kind));
                                break;
                        }
                        nob_sb_appendf(output, "    vars[%zu] = rax\n", inst.args[0].local_index);
                    }
                    break;
                case INST_LOCAL_INIT:
                    if(!expect_inst_arg(inst, 0, ARG_LOCAL_INDEX)) return false;
                    nob_sb_appendf(output, "    vars.push(0);\n");
                    break;
                case INST_LOCAL_ASSIGN:
                    if(!expect_inst_arg(inst, 0, ARG_LOCAL_INDEX)) return false;
                    if(!expect_inst_arg(inst, 1, ARG_INT_VALUE)) return false;
                    nob_sb_appendf(output, "    vars[%zu] = %lld;\n", inst.args[0].local_index, inst.args[1].int_value);
                    break;
                case INST_EXTERN:
                    if(!expect_inst_arg(inst, 0, ARG_NAME)) return false;
                    break;
                case INST_FUNCALL:
                    if(!expect_inst_arg(inst, 0, ARG_NAME)) return false;
                    nob_sb_appendf(output, "    %s(", inst.args[0].name);
                    if(inst.args[1].kind == ARG_LOCAL_INDEX) {
                        nob_sb_appendf(output, "vars[%zu]", inst.args[1].local_index);
                    } else if(inst.args[1].kind == ARG_INT_VALUE) {
                        nob_sb_appendf(output, "%lld", inst.args[1].int_value);
                    }
                    nob_sb_appendf(output, ")\n");
                    break;
            }
        }

        block_counter += 1;
    }
    nob_sb_appendf(output, "}\n");
    return true;
}
