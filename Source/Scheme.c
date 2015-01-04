//
//  Scheme.c
//  Pd_Scheme
//
//  Created by etienne cella on 2014-12-31.
//
//
#include <stdlib.h>
#include <string.h>
#include "../Lib/m_pd.h"
#include "../s7/s7.h"

#define S7_PD_OBJECT_NAME "pd-object"

static t_class *scheme_class;

typedef struct _scheme {
    
    t_object  x_obj;
    s7_scheme * s7;
    t_outlet *out_f;
    t_outlet *out_l;
    t_atom *listdata;
    
} t_scheme;

// convert pd atom to s7 value
static s7_pointer pd_atom_to_s7_value(s7_scheme * s7, t_atom * atom)
{
    switch(atom->a_type){
            
        case A_FLOAT:
            return s7_make_real(s7, atom_getfloat(atom));
            
        case A_SYMBOL:
            return s7_make_string(s7, atom_getsymbol(atom)->s_name);
            
        default:
            error("pd atoms to s7 values: unexpected atom type");
            return s7_nil(s7);
    }
}

// convert s7 value to pd atom
/*static t_atom s7_value_pd_atom_to(s7_scheme * s7, s7_pointer val)
{
    if (s7_is_integer(val)){
        t_atom a;
        SETSYMBOL(a, gensym("readraw"));
    }
    
    if (s7_is_real(val)){
    
    
    }
    
    if (s7_is_string(val)){
        t_atom a;
        SETSYMBOL(a, gensym(s7_string(val)));
        return a;
    }
    
    SETSYMBOL(listdata + ldex, gensym("readraw")); ++ldex;
    
    SETFLOAT(listdata + ldex, (float)pnum);  ++ldex;
}*/


static void s7_to_pd_output(s7_scheme *sc, unsigned char c, s7_pointer port)
{
    #define S7_OUT_BUF_SIZE 80
    static char text[S7_OUT_BUF_SIZE];
    static int char_index = 0;
    text[char_index] = (char)c;
    char_index++;
    
    if (char_index > S7_OUT_BUF_SIZE - 1)
    {
        post(text);
        memset(text, ' ', S7_OUT_BUF_SIZE * sizeof(char));
        char_index = 0;
    }
}

/*static void s7_to_pd_error(s7_scheme *sc, unsigned char c, s7_pointer port)
{
    error(c);
}*/

static s7_pointer s7_to_pd_outlet(s7_scheme *s7, s7_pointer args)
{
    post("s7_to_pd_outlet");
    
    // first retrieve pd object
    t_scheme * x = s7_c_pointer(s7_name_to_value(s7, S7_PD_OBJECT_NAME));
   
    
    /*
    s7_pointer val = s7_car(args);
    if (s7_is_real(val)){
        outlet_float(x->out_f, s7_real(val));
    }*/
    
    int ldex = 0;
    t_atom *listdata = x->listdata;
    s7_pointer arguments = args;
    
    
    while (s7_car(arguments) != s7_nil(s7)){
        
        s7_pointer val = s7_car(arguments);
        arguments = s7_cdr(arguments);
        
        if (s7_is_integer(val)){
            SETFLOAT(listdata + ldex, (float)s7_integer(val));
        } else if (s7_is_real(val)){
            SETFLOAT(listdata + ldex, s7_real(val));
        } else if (s7_is_string(val)){
            SETSYMBOL(listdata + ldex, gensym(s7_string(val)));
        } else {
            break;
        }
        ldex++;
    }
    
    outlet_list(x->out_l,0,ldex,listdata);
    
    return s7_nil(s7);
}

void scheme_bang(t_scheme *x)
{
    // call bang handler on script
    if (s7_is_defined(x->s7, "bang")){
        s7_call(x->s7, s7_name_to_value(x->s7, "bang"), s7_nil(x->s7));
    } else {
        post("script does not defines [bang]");
    }
}

void scheme_load(t_scheme *x, t_symbol *s, int argc, t_atom *argv)
{
	char file_name[1024];
	atom_string(argv, file_name, 1024);
    s7_load(x->s7, file_name);
}

void scheme_call(t_scheme *x, t_symbol *s, int argc, t_atom *argv)
{
    if (s7_is_defined(x->s7, s->s_name)) {
        int i  = argc;
        s7_pointer args = s7_nil(x->s7);
        while(i--){
            args = s7_cons(x->s7,
                           pd_atom_to_s7_value(x->s7, &argv[i]),
                           args);
        }
        s7_call(x->s7, s7_name_to_value(x->s7, s->s_name), args);
    } else {
        post("script call: symbol %s not defined", s->s_name);
    }
}

void *scheme_new(t_symbol *s, int argc, t_atom *argv)
{
    t_scheme *x = (t_scheme *)pd_new(scheme_class);
    
    x->listdata = (t_atom *) malloc(1024 * sizeof(t_atom));
	
    
    x->s7 = s7_init();
    
    s7_set_current_output_port(x->s7, s7_open_output_function(x->s7, s7_to_pd_output));
    s7_set_current_error_port(x->s7, s7_open_output_function(x->s7, s7_to_pd_output));
    
    s7_define_function(x->s7, "outlet", s7_to_pd_outlet, 0, 0, true, "");
    
    // we store a reference to our pd object in s7
    // it lets us refer the pd object from c code called by s7
    s7_define_variable(x->s7, S7_PD_OBJECT_NAME, s7_make_c_pointer(x->s7, x));
    
    x->out_f = outlet_new(&x->x_obj, &s_float);
    x->out_l = outlet_new(&x->x_obj, &s_list);

    return (void *)x;
}

void scheme_free(t_scheme *x)
{
    outlet_free(x->out_f);
    outlet_free(x->out_l);
    free(x->listdata);
    
    // should free open s7 ports?
    s7_quit(x->s7);
    free(x->s7);
}

void scheme_setup(void)
{
    
    scheme_class = class_new(gensym("scheme"),
                              (t_newmethod)scheme_new,
                              (t_method)scheme_free, sizeof(t_scheme),
                              CLASS_DEFAULT,
                              A_GIMME, 0);
    
    class_addbang(scheme_class, scheme_bang);
    
    class_addmethod(scheme_class, (t_method)scheme_load, gensym("load"), A_GIMME, 0);
    
    class_addmethod(scheme_class, (t_method)scheme_call, gensym("call"), A_GIMME, 0);
}
