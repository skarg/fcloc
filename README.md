fcloc
=====

A command line application that counts the number of lines 
of code for each function in a C programming language source 
file.  Useful for estimating 

The source is written in portable C, and compiles and runs on 
on DOS, iRMX, Linux (GCC), and Windows (Borland, MinGW).

~~~
C:\code\fcloc>fcloc fcloc.c
Program      Function                         Function Total
Name         Name                             LOC      LOC
============ ================================ ======== ========
fcloc.c
    main                                  284
    create_list_element                     9
    add_element                            12
    delete_elements                        10
    print_functions_wks                    31
    print_functions                        55
    last_element                           12
    open_input_file                         8
    check_token                            23
    check_for_function                    100
    function_name_compare                  19
    keyword_compare                        13
    keyword_print                          19
    open_debug_file                        17
    debug_file_date                        13
    Interpret_Arguments                    48
    Usage                                  36
                                     --------
TOTAL        17                                    709     1116
============ ================================ ======== ========
Physical LOC                                               1285
Comment LOC                                                 478
~~~
