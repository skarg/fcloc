/**************************************************************************
*
* Filename:    fcloc.c
*
* Description: Reads in a C program file and counts the number of 
*              logical lines of code.  Also displays each function
*              and displays the number of logical lines of code
*              for that function.
*
* History: 1: 05-Dec-1996: Copied loc.c to create file - Steve Karg
*          2: 09-Jan-1997: Corrected flaw.  The single quote when used
*                          with control codes would skip if the control
*                          code was a \'. - Steve Karg
*          3: 16-Jan-1997: Added code to check for old style of function
*                          declarations.  Added some additional keywords
*                          for the symbols that aren't counted.  This will
*                          help avoid false starts for function detection
*                          and counting. - Steve Karg
*          4: 03-Feb-1997: Added code to the function check routing to 
*                          allow counting of the function line and the
*                          last brace.  This will enable better counts
*                          for estimate purposes. - Steve Karg
*          5: 18-Feb-1997: Added code that counts the number of comment
*                          lines of code, and determines the comment
*                          density. Added code that uses the filename
*                          as entered from the command line and removes
*                          any extraneous logicals or directory paths
*                          for the help screen (Usage function) - Steve Karg
*          6: 26-Aug-1997: Modified the # precompiler to allow for non-first
*                          column entry.  Modified the code for the MS-DOS
*                          operating system.  Added checking for C++ comment
*                          style. - Steve Karg
*          7: 23-Oct-1997: Added C++ keywords.  Changed the constant size
*                          of the keyword array to a variable size.  Found
*                          out in the process that sizeof() returns the
*                          memory size of the array, not the array size.
*                          - Steve Karg
*          8: 05-Jun-1998: Modified the # precompiler to ignore anything after
*                          #ELIF or #ELSE until it reached #ENDIF.  This keeps
*                          the parser from getting lost counting braces and
*                          parenthesis. - Steve Karg
*
**************************************************************************/
static char version_date[]   = {"05-Jun-1998"};
static char version_number[] = {"1.08"};

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <string.h>

/* LOCAL CONTSTANTS */
#define MAX_LINE_SIZE (80)
#define MAX_FUNCTION_NAME (32)
#define TRUE (1)
#define FALSE (0)

#if defined(__GNUC__)
  #define stricmp strcasecmp
#endif

/* set up counter type */
typedef unsigned long int COUNTER;

/* structure for keyword list */
typedef struct keyword
{
  char data[MAX_LINE_SIZE];  /* string containing the key word */
  unsigned char valid_flag;  /* is this used for LOC count? */
} KEYWORD;

/* array for keywords */
static KEYWORD c_keywords[] =
{
  /* ANSI C LOC countable reserved words and symbols (15) */
  {"case",     TRUE},  {"default",  TRUE},  {"do",       TRUE},
  {"else",     TRUE},  {"enum",     TRUE},  {"for",      TRUE},
  {"if",       TRUE},  {"struct",   TRUE},  {"switch",   TRUE},
  {"union",    TRUE},  {"while",    TRUE},  {"#",        TRUE},
  {";",        TRUE},  {",",        TRUE},  {"}",        TRUE},

  /* ANSI C++ LOC reserved words (26) */
  {"bool",     FALSE},  {"catch",   TRUE},  {"class",    TRUE},
  {"const_cast",FALSE}, {"delete",  FALSE}, {"dynamic_cast",FALSE},
  {"false",    FALSE},  {"friend",  TRUE},  {"inline",   TRUE},
  {"mutable",  FALSE},  {"namespace",FALSE},{"new",      FALSE},
  {"operator", TRUE},   {"private", TRUE},  {"protected",TRUE},
  {"public",   TRUE},   {"reinterpret_cast",FALSE},{"static_cast",FALSE},
  {"template", TRUE},   {"this",    FALSE}, {"throw",   FALSE},
  {"true",     FALSE},  {"try",     TRUE},  {"typeid",  FALSE},
  {"using",    FALSE},  {"virtual", TRUE},

  /* ANSI C Reserved words that are not LOC countable (28) */
  {"asm",      FALSE},  {"auto",     FALSE},  {"break",    FALSE},
  {"char",     FALSE},  {"const",    FALSE},  {"continue", FALSE},
  {"do",       FALSE},  {"double",   FALSE},  {"entry",    FALSE},
  {"enum",     FALSE},  {"extern",   FALSE},  {"float",    FALSE},
  {"fortran",  FALSE},  {"goto",     FALSE},  {"int",      FALSE},
  {"long",     FALSE},  {"register", FALSE},  {"return",   FALSE},
  {"short",    FALSE},  {"signed",   FALSE},  {"sizeof",   FALSE},
  {"static",   FALSE},  {"unsigned", FALSE},  {"void",     FALSE},
  {"volatile", FALSE},

  /* ANSI C symbols that are not countable (25) */
  {"!", FALSE}, {"%", FALSE}, {"^",  FALSE}, {"&",  FALSE}, {"*",  FALSE},
  {"(", FALSE}, {")", FALSE}, {"-",  FALSE}, {"_",  FALSE}, {"+",  FALSE},
  {"=", FALSE}, {"~", FALSE}, {"[",  FALSE}, {"]",  FALSE}, {"\\", FALSE},
  {"|", FALSE}, {":", FALSE}, {"'",  FALSE}, {"\"", FALSE}, {"{",  FALSE},
  {".", FALSE}, {"<", FALSE}, {">",  FALSE}, {"/",  FALSE}, {"?",  FALSE},

  /* ANSI C compound symbols that are not countable (21) */
  /* NOTE: the program as currently written
     does not recognize compound symbols */
  {"+=",  FALSE}, {"-=",  FALSE}, {"*=", FALSE}, {"/=", FALSE}, {"%=", FALSE},
  {"<<=", FALSE}, {">>=", FALSE}, {"&=", FALSE}, {"^=", FALSE}, {"|=", FALSE},
  {"->",  FALSE}, {"++",  FALSE}, {"--", FALSE}, {"<<", FALSE}, {"<<", FALSE},
  {"<=",  FALSE}, {">=",  FALSE}, {"==", FALSE}, {"!=", FALSE}, {"&&", FALSE},
  {"||",  FALSE}
};

/* Rather than use a constant (#define), this is easier to maintain */
static size_t Max_Keywords = sizeof(c_keywords)/sizeof(KEYWORD);

/* This element will hold function names and size of function */
typedef struct function
{
  char name[MAX_FUNCTION_NAME]; /* function name */
  COUNTER loc_count;            /* number of logical lines of code */
  struct function *next;        /* next element in list - NULL if none */
} FUNCTION;

/* set up generic element */
typedef struct function ELEMENT;

/* set up the start of the linked list */
static ELEMENT *head;

/* set up debug */
static unsigned char Debug_Flag = FALSE;
static FILE *debug_file_ptr = NULL;
/*static char debug_string[256];*/
static char Append_File_Name[256] = {""};
static char C_Filename[256] = {""};
static unsigned char WKS_Flag = FALSE;
static unsigned char WKS_Header_Flag = FALSE;

/* FUNCTION PROTOTYPES */
ELEMENT *create_list_element(void);
void add_element(ELEMENT *e);
void delete_elements(void);
ELEMENT *last_element(void);
FILE *open_input_file(char *filename);
FILE *open_debug_file(void);
char *debug_file_date(void);
void Interpret_Arguments(int argc, char *argv[]);
void Usage(char *filename);

void print_functions(char *filename,COUNTER loc,COUNTER ploc,COUNTER cloc);
void print_functions_wks(char *filename,COUNTER loc,unsigned char header);
void check_token(char *token,char *prev_token,COUNTER *count,COUNTER ploc);
void check_for_function(char *token,char *prev_token);
int keyword_compare(const char *word);
int function_name_compare(char *word);
void keyword_print(void);

/**************************************************************************
*
* Function:    main
*
* Description: Reads in a C program file and counts the number of
*              logical lines of code.  Also displays each function
*              and displays the number of logical lines of code
*              for that function.
*
* Parameters:  arcc - number of arguments passed into the program when
*                     run from the command line.
*              argv - a pointer to each of the arguments passed into
*                     the program from the command line.
*
* Return:      none
*
**************************************************************************/
int main(int argc,char *argv[])
{
  COUNTER loc_count;      /* number of logical lines of code */
  COUNTER physical_loc;   /* number of physical lines of code */
  COUNTER comment_loc;    /* number of comment lines of code */
  COUNTER comment_nospace;/* count of white space */
  COUNTER comment_char;   /* count of total comment characters */

  FILE *file_ptr;      /* C program file stream */

  char last_char;      /* the previous char read in from file */
  char new_char;       /* the current char read in from file */

  char token[MAX_LINE_SIZE]; /* word in file */
  char token1[MAX_LINE_SIZE]; /* used for tokenizing characters */
  char token2[MAX_LINE_SIZE];/* another word in a file. */
  char last_token[MAX_LINE_SIZE];/* previous token */
  unsigned short token_len;  /* length of the token string */

  unsigned char comment;     /* flag used for skipping stuff inside comments */
  unsigned char quotation;   /* flag used for skipping stuff inside quotes */
  unsigned char single_quote;/* skip stuff inside single quotes */
  unsigned char precompiler; /* skip pre-compiler defines */
  unsigned char precompiler_end; /* skip pre-compiler else to endif defines */
  unsigned char precompiler_else; /* skip pre-compiler else to endif defines */
  unsigned char precompiler_if; /* skip pre-compiler else to endif defines */
  unsigned char control_code;/* skip control codes started with \ */
  unsigned char c_plus_plus_comment; /* flag used to discern comment type */

  /* INITIALIZE */
  new_char = 0;
  last_char = 0;

  token[0] = 0;
  token2[0] = 0;
  last_token[0] = 0;

  comment = FALSE;
  quotation = FALSE;
  single_quote = FALSE;
  precompiler = FALSE;
  precompiler_end = FALSE; 
  precompiler_else = FALSE;
  precompiler_if = FALSE;
  control_code = FALSE;
  c_plus_plus_comment = FALSE;

  loc_count = 0;
  token_len = 0;
  physical_loc = 0;

  comment_loc = 0;
  comment_nospace = 0;
  comment_char = 0;

  Interpret_Arguments(argc,argv);

  /* === COUNT LOGICAL LOC === */

  /* open the file */
  file_ptr = open_input_file(C_Filename);
  if (file_ptr == NULL)
    return (1);

  if (Debug_Flag)
  {
    debug_file_ptr = open_debug_file();
    fprintf(debug_file_ptr,"Reading file: %s\n",C_Filename);
  }

  /* read the file one character at a time */
  while ((new_char = fgetc(file_ptr)) != EOF)
  {
    /* count physical lines of code */
    if (new_char == '\n')
      physical_loc++;

    /* COMMENT - skip until end of comment */
    if (comment != FALSE)
    {
      /* count the number of characters in the comment */
      comment_char++;
      /* conditions to end the comment */
      if ((last_char == '*') && (new_char == '/'))
      {
        comment = FALSE;
      }

      /* End of line */
      if (new_char == '\n')
      {
        /* conditions to end the comment */
        if (c_plus_plus_comment != FALSE)
        {
          c_plus_plus_comment = FALSE;
          comment = FALSE;
        }
        /* count comment lines of code */
        else
        {
          comment_loc++;
        }
      }
      /* count the number of whitespace chars in the comment */
      if (!(isspace(new_char)))
        comment_nospace++;
    } /* end of comment */

    /* QUOTES - skip until end of quotation */
    else if (quotation != FALSE)
    {
      /* conditions to end the quotation */
      if (control_code != FALSE)
        control_code = FALSE;
      else if (new_char == '\\')
        control_code = TRUE;
      else if (new_char == '"')
        quotation = FALSE;
    }

    /* SINGLE QUOTES - skip until end of quotes */
    else if (single_quote != FALSE)
    {
      /* conditions to end the quotation */
      if (control_code != FALSE)
        control_code = FALSE;
      else if (new_char == '\\')
        control_code = TRUE;
      else if (new_char == '\'')
        single_quote = FALSE;
    }

    /* PRE-COMPILER - skip until end of line but not \ eol */
    else if (precompiler != FALSE)
    {
      /* conditions to end the pre compile */

      /* skip else or elif to endif on pre-compile */
      if (precompiler_if && precompiler_else && precompiler_end)
      {
        /* is it an endif? */
        if ((new_char != ' ') &&
            (new_char != '\\') &&
            (new_char != '\n'))
        {
          sprintf(token1,"%s%c",token,new_char);
          strcpy(token,token1);
        }
        else
        {
          if (stricmp(token,"endif") == 0)
          {
            precompiler = FALSE;
            precompiler_if = FALSE;
            precompiler_else = FALSE;
            precompiler_end = FALSE;
          }
          else
            precompiler_end = FALSE;
        }
      }
      else if (precompiler_if && precompiler_else)
      {
        /* look for Pre-compiler endif */
        if (new_char == '#')
        {
          precompiler_end = TRUE;
          token[0] = 0;
        }
      }
      else if (precompiler_if && (precompiler_else == FALSE))
      {
        if ((new_char != ' ') &&
            (new_char != '\\') &&
            (new_char != '\n'))
        {
          sprintf(token1,"%s%c",token,new_char);
          strcpy(token,token1);
        }
        else
        {
          if ((stricmp(token,"else") == 0) ||
              (stricmp(token,"elif") == 0))
          {
            precompiler_else = TRUE;
          }
          else
          {
            if (new_char == '\n')
            {
              precompiler = FALSE;
              precompiler_if = FALSE;
              precompiler_else = FALSE;
              precompiler_end = FALSE;
            }
            else
              precompiler_if = FALSE;
          }
        }
      }
      else if ((last_char != '\\') && (new_char == '\n'))
      {
        precompiler = FALSE;
        precompiler_if = FALSE;
        precompiler_else = FALSE;
        precompiler_end = FALSE;
      }
    }

    else
    {
      /* Turn on Comment Flag */
      if (((new_char == '*') || (new_char == '/')) && (last_char == '/'))
      {
        if (new_char == '/')
          c_plus_plus_comment = TRUE;
        /* count comment lines of code */
        comment_loc++;
        /* count the number of characters in the comment */
        comment_char++;
        comment_char++;
        /* turn on flag to start looking for end of comment */
        comment = TRUE;
        /* shrink token by 1 to remove / */
        token_len = strlen(token);
        /* check to see if tokens are countable */
        switch(token_len)
        {
          case 0:
            break;
          case 1:
            token[0] = 0;
            break;
          default:
            token[token_len-1] = 0;
            check_token(token,last_token,&loc_count,physical_loc);
            break;
        }
      }

      /* Turn on Quotation Flag */
      else if (new_char == '"')
      {
        quotation = TRUE;
        check_token(token,last_token,&loc_count,physical_loc);
      }

      /* Turn on Single Quotation Flag */
      else if (new_char == '\'')
      {
        single_quote = TRUE;
        check_token(token,last_token,&loc_count,physical_loc);
      }

      /* Turn on Pre-compiler Flag */
      else if (new_char == '#')
      {
        precompiler = TRUE;
        precompiler_if = TRUE;
        precompiler_else = FALSE;
        precompiler_end = FALSE;
        sprintf(token2,"%c",new_char);
        check_token(token2,last_token,&loc_count,physical_loc);
        token[0] = 0;
      }

      /* Force token check - EOL */
      else if (new_char == '\n')
        check_token(token,last_token,&loc_count,physical_loc);

      /* Force token check - WHITE SPACE */
      else if (isspace(new_char))
        check_token(token,last_token,&loc_count,physical_loc);

      /* Force token check - PUNCTUATION */
      else if (ispunct(new_char))
      {
        /* VALID NON-DELIMITER */
        if ((new_char == '_') ||
            (new_char == '~')) /* used in c++ destructors */
        {
          /* valid character - include with token */
          sprintf(token1,"%s%c",token,new_char);
          strcpy(token,token1);
        }
        /* DELIMITER FOUND */
        else
        {
          check_token(token,last_token,&loc_count,physical_loc);
          /* check punct to see if it is a countable token */
          sprintf(token2,"%c",new_char);
          check_token(token2,last_token,&loc_count,physical_loc);
        } /* end of token delimiter */
      }
      /* BUILD TOKEN */
      else
      {
        sprintf(token1,"%s%c",token,new_char);
        strcpy(token,token1);
      }
    }

    /* update any previous variables */
    last_char = new_char;
  }

  /* close the file */
  fclose(file_ptr);
  if (Debug_Flag)
  {
    fprintf(debug_file_ptr,"%-32s %6lu\n","PROGRAM TOTAL",loc_count);
    fclose(debug_file_ptr);
  }

  /* Print the results */
  if (WKS_Flag)
    print_functions_wks(C_Filename,loc_count,WKS_Header_Flag);
  else
    print_functions(C_Filename,loc_count,physical_loc,comment_loc);

  /* House Keeping */
  delete_elements();

  return 0;
}

/**************************************************************************
*
* Function:    create_list_element
*
* Description: Allocates memory for a linked list element and returns
*              a pointer to that memory.
*
* Parameters:  none
*
* Globals:     none
*
* Locals:      typedef of ELEMENT.
*
* Return:      p - pointer to the memory allocated for that element.
*
**************************************************************************/
ELEMENT *create_list_element(void)
{
  ELEMENT *p;

  p = (ELEMENT *) malloc( sizeof(ELEMENT));
  if (p==NULL)
  {
    printf("create_list_element: malloc failed.\n");
    exit(1);
  }
  p->next = NULL;

  return p;
} /* end of function */

/**************************************************************************
*
* Function:    add_element
*
* Description: Attaches an ELEMENT to the tail end of the linked list
*              by attaching it to the first ELEMENT that is NULL.
*              
* Parameters:  e - reference of ELEMENT e.
*
* Globals:     none
*
* Locals:      typedef of ELEMENT
*              head - first ELEMENT of the linked list
*
* Return:      none
*
**************************************************************************/
void add_element(ELEMENT *e)
{
  ELEMENT *p;

  /* if the first element has not been created, create it now */
  if (head == NULL)
  {
    head = e;
    return;
  }

  /* otherwise, find the last element in the list */
  for (p=head;p->next != NULL;p=p->next) {}

  p->next = e;
  return;
}

/**************************************************************************
*
* Function:    delete_elements
*
* Description: De-allocates memory for all linked list elements starting with
*              head.
*
* Parameters:  none
*
* Globals:     none
*
* Locals:      typedef of ELEMENT
*              head - first ELEMENT of the linked list
*
* Return:      none
*
**************************************************************************/
void delete_elements(void)
{
  ELEMENT *current;
  ELEMENT *next;

  current = head;
  while(current != NULL)
  {
    next = current->next;
    free(current);
    current = next;
  }
  return;
}

/**************************************************************************
*
* Function:    print_functions_wks
*
* Description: Prints all linked list elements starting with head.
*
* Parameters:  filename (IN) name of file
*              loc (IN) total logicial lines of code in file.
*
* Globals:     none
*
* Locals:      typedef of ELEMENT
*              head - first ELEMENT of the linked list
*
* Return:      none
*
**************************************************************************/
void print_functions_wks(char *filename,COUNTER loc,unsigned char header)
{
  ELEMENT *current = NULL;
  char *str = NULL;
  char *name = NULL;

  /* Find the last token - that is the actual filename */
  str = strtok(filename,":\\");
  while (str != NULL)
  {
    name = str;
    str = strtok(NULL,":\\");
  }

  if (header)
    printf("Program Name,Function Name,Function LOC,Total LOC\n");
  if (name != NULL)
    printf("%s,,,%lu\n",name,loc);
  else
    printf("\n");

  current = head;

  while(current != NULL)
  {
    if (current->loc_count > 0)
    {
      printf(",%s,%lu\n",current->name,current->loc_count);
    }
    current = current->next;
  }

  return;
}

/**************************************************************************
*
* Function:    print_functions
*
* Description: Prints all linked list elements starting with head.
*
* Parameters:  none
*
* Globals:     none
*
* Locals:      typedef of ELEMENT
*              head - first ELEMENT of the linked list
*
* Return:      none
*
**************************************************************************/
void print_functions(char *filename,COUNTER loc,COUNTER ploc,COUNTER cloc)
{
  ELEMENT *current = NULL;
  COUNTER floc = 0; /* function line of code */
  char *str = NULL;
  char *name = NULL;
  COUNTER methods = 0;

  /* Find the last token - that is the actual filename */
  str = strtok(filename,":\\");
  while (str != NULL)
  {
    name = str;
    str = strtok(NULL,":\\");
  }

  printf("Program      Function                         Function Total\n");
  printf("Name         Name                             LOC      LOC\n");
  printf("============ ================================ ======== ========\n");
  if (name != NULL)
    printf("%s\n",name);
  else
    printf("\n");

  current = head;
  floc = 0;

  while(current != NULL)
  {
    if (current->loc_count > 0)
    {
      printf("%-12s %-32s %8lu\n"," ",current->name,current->loc_count);
      floc += current->loc_count;
      methods++;
    }
    current = current->next;
  }

  printf("                                              --------         \n");
  printf("TOTAL        %-8lu                         %8lu %8lu",
    methods,floc,loc);
  printf("\n");
  printf("============ ================================ ======== ========\n");
  printf("%-12s %-32s %-8s %8lu\n","Physical LOC"," "," ",ploc);
  printf("%-12s %-32s %-8s %8lu\n","Comment LOC"," "," ",cloc);

  return;
}

/**************************************************************************
*
* Function:    last_element
*
* Description: Returns a pointer to the last element of the list. If the
*              list is empty, it returns the NULL pointer.
*
* Parameters:  none
*
* Globals:     none
*
* Locals:      typedef of ELEMENT.
*              head - first ELEMENT of the linked list
*
* Return:      p - pointer to the last element.
*
**************************************************************************/
ELEMENT *last_element(void)
{
  ELEMENT *p;

  /* if the first element has not been created, return NULL */
  if (head == NULL)
  {
    p = head;
  }

  /* otherwise, find the last element in the list */
  else
  {
    for (p=head;p->next != NULL;p=p->next) {}
  }

  return p;
} /* end of function */

/**************************************************************************
*
* Function:    open_input_file
*
* Description: Opens a file stream for input (read only) and returns a 
*              file pointer to that stream.
*
* Parameters:  filename - string containing the name of the file to be
*                         opened.
*
* Globals:     none
*
* Locals:      none
*
* Return:      fp - pointer to the stream of the file opened.
*
**************************************************************************/
FILE *open_input_file(char *filename)
{
  FILE *fp;

  fp = fopen(filename,"r");
  if (fp == NULL)
    printf("open_input_file: error opening %s.\n",filename);

  return fp;
}

/**************************************************************************
*
* Function:    check_token
*
* Description: Compares tokens that contain something to the keywords and
*              increments a counter if there is a match.
*
* Parameters:  token - reference to a string that contains some characters.
*              prev_token - reference to a string that gets loaded with the 
*                  token.
*              count - reference to a counter that gets incremented upon a 
*                  match.
*
* Globals:     none
*
* Locals:      keyword_compare function.
*
* Return:      none
*
**************************************************************************/
void check_token(char *token,char *prev_token,COUNTER *count,COUNTER ploc)
{
  char string[MAX_LINE_SIZE]; /* word in file */

  if (token[0] != 0)
  {
    check_for_function(token,prev_token);
    strcpy(prev_token,token);
    if (keyword_compare(token))
    {
      (*count)++;
      if (Debug_Flag)
      {
        strcpy(string,token);
        fprintf(debug_file_ptr,"Line %04lu => %s\n",ploc,string);
      }
    }
    token[0] = 0;
  }
}

/**************************************************************************
*
* Function:    check_for_function
*
* Description: Looks for a last token that is not a reserved word, punctuation,
*              or digit, and a token that is a '('.  If this is found, then the
*              last token,which should contain the function name, is loaded
*              into a linked list.  The countable tokens are counted until
*              the brace level returns to 0.  This is saved with the function
*              name in a linked list.
*
* Parameters:  token - reference to a string that contains some characters.
*              prev_token - reference to a string that gets loaded with the 
*                  token.
*              count - reference to a counter that gets incremented upon a 
*                  match.
*
* Globals:     none
*
* Locals:      function_name_compare function.
*
* Return:      none
*
**************************************************************************/
void check_for_function(char *token,char *prev_token)
{
  static ELEMENT *temp_node = {NULL};
  static COUNTER loc_count = {0};
  static unsigned char count_flag = {FALSE};
  static unsigned char start_flag = {FALSE};
  static COUNTER brace_count = {0};
  static COUNTER parenthesis_count = {0};

  /* if a '(' is found, and last token is not a keyword,
     then load the function name, turn on the flag */
  /* === Function flag is not set === */
  if ((start_flag == FALSE) && (brace_count == 0))
  {
    if (strcmp(token,"(") == 0)
    {
      if (function_name_compare(prev_token))
      {
        /* create element and load list */
        temp_node = create_list_element();
        /* safe string copy - in case the token is very large. */
        strncpy(temp_node->name,prev_token,MAX_FUNCTION_NAME);
        /*(temp_node->name+MAX_FUNCTION_NAME-1)=0;*/
        temp_node->loc_count = 0;
        loc_count = 0;
        add_element(temp_node);
        start_flag = TRUE;
        parenthesis_count = 1;
        if (Debug_Flag)
        {
          fprintf(debug_file_ptr,"Possible Function=> %s\n",prev_token);
        }
      }
    } /* end of function start */
  } /* end of no function flag */

  /* === Function flag is set, but not a real function yet === */
  else if (count_flag == FALSE)
  {
    if (strcmp(token,"(") == 0)
      parenthesis_count++;
    else if (strcmp(token,")") == 0)
    {
      if (parenthesis_count != 0)
        parenthesis_count--;
    }

    if (Debug_Flag)
    {
      fprintf(debug_file_ptr,"Function Set, Parenthesis Level=> %lu "
                             "LOC Count=>%lu\n",
              parenthesis_count,loc_count);
    }
    
    if ((strcmp(prev_token,")") == 0) && (parenthesis_count == 0))
    {
      /* Look for end of function call or prototype */
      if (strcmp(token,";") == 0)
      {
        /* reset the function to look for new function */
        start_flag = FALSE;
      }

      /* Look for end of function */
      else if (strcmp(token,"{") == 0)
      {
        /* found valid function - start counting lines */
        count_flag = TRUE;
        brace_count = 1;
      }
    } /* end of normal end parenthesis */

    /* Look for start of 'old' style of functions */   
    else if ((strcmp(prev_token,";") == 0) && (parenthesis_count == 0))
    {
      if (strcmp(token,"{") == 0)
      {
        /* found valid function - start counting lines */
        count_flag = TRUE;
        brace_count = 1;
      }
    } /* end of old style function */

    /* Count valid, countable tokens, including the stuff 
       in the function call during this preliminary stage */
    if (keyword_compare(token))
      loc_count++;
    
  } /* end of function flag set */

  /* === Function flag is set and started counting === */
  else if ((count_flag != FALSE) && (start_flag != FALSE))
  {
    if (strcmp(token,"{") == 0)
      brace_count++;
    else if (strcmp(token,"}") == 0)
      brace_count--;

    if (Debug_Flag)
    {
      fprintf(debug_file_ptr,"Function Set, Brace Level=> %lu "
                             "LOC Count=>%lu\n",
              brace_count,loc_count);
    }
    
    /* Count valid, countable tokens, including the last brace */
    if (keyword_compare(token))
      loc_count++;
    
    /* Found the end of Function Method */
    if (brace_count == 0)
    {
      /* turn off the function counter */
      count_flag = FALSE;
      start_flag = FALSE;
      /* load the linked list with the results */
      temp_node->loc_count = loc_count;
    }

  } /* end of function flag set and count flag set */
    
} /* end of function */

/**************************************************************************
*
* Function:    function_name_compare
*
* Description: Compares a string with a list of valid strings and checks
*              for a match.  Also checks for a punctuation or digit if
*              it is only a digit.
*
* Parameters:  word - reference to a string that contains some characters.
*
* Globals:     none
*
* Locals:      c_keywords - structure containing C reserved words
*
* Return:      status - FALSE if the reserved word or punct match is found.
*                       TRUE if no match is found.
*
**************************************************************************/
int function_name_compare(char *word)
{
  int status = TRUE;      /* FALSE if reserve word or punct found */
  int key_index = 0;      /* index into the keyword structure */

  if (strlen(word) > 1)
  {
    for (key_index=0;key_index < Max_Keywords;key_index++)
    {
      if (strcmp(word,c_keywords[key_index].data) == 0)
        status = FALSE;
    }
  }
  else if (ispunct(word[0]))
    status = FALSE;
  else if (isdigit(word[0]))
    status = FALSE;

  return status;
}

/**************************************************************************
*
* Function:    keyword_compare
*
* Description: Compares a string with a list of valid strings and checks
*              for a match.
*
* Parameters:  word - reference to a string that contains some characters.
*
* Globals:     none
*
* Locals:      c_keywords - structure containing C keywords
*              Max_Keywords - total number of keywords
*
* Return:      status - TRUE if the keyword match is found.
*                       FALSE if no match is found.
*
**************************************************************************/
int keyword_compare(const char *word)
{
  int status = FALSE;      /* TRUE if keyword match found */
  int key_index = 0;      /* index into the keyword structure */

  for (key_index=0;key_index < Max_Keywords;key_index++)
  {
    if (c_keywords[key_index].valid_flag)
    {
      if (strcmp(word,c_keywords[key_index].data) == 0)
        status = TRUE;
    }
  }

  return status;
}

/**************************************************************************
*
* Function:    keyword_print
*
* Description: Prints a list of valid strings on the screen 5 across.
*
* Parameters:  none
*
* Globals:     none
*
* Locals:      c_keywords - structure containing C keywords
*              Max_Keywords - total number of keywords
*
* Return:      none
*
**************************************************************************/
void keyword_print(void)
{
  int key_index = 0;   /* index into the keyword structure */
  int count = 0;       /* counter that counts number of words across screen */

  for (key_index=0;key_index < Max_Keywords;key_index++)
  {
    if (c_keywords[key_index].valid_flag)
    {
      printf("%s",c_keywords[key_index].data);
      count++;
      if (count > 4)
      {
        printf("\n");
        count = 0;
      }
      else
        printf("\t");
    }
  }

  return;
}

/**************************************************************************
*
* Function:    open_debug_file
*
* Description: Opens a file stream for output and returns a 
*              file pointer to that stream.
*
* Parameters:  none
*
* Globals:     none
*
* Locals:      none
*
* Return:      fp - pointer to the stream of the file opened.
*
**************************************************************************/
FILE *open_debug_file(void)
{
  FILE *fp;
  char filename[256];
  char *date_buffer;

  /* Create a date string file name */
  date_buffer = debug_file_date();
  sprintf(filename,"%s.dbg",date_buffer);

  fp = fopen(filename,"w");
  if (fp == NULL)
    printf("open_debug_file: error opening %s.\n",filename);
  else
    printf("open_debug_file: opened debug file %s.\n",filename);

  return fp;
}

/**************************************************************************
*
* Function:    debug_file_date
*
* Description: Creates a string that will be the name of the debug file
*              using the current date.
*
* Parameters:  none
*
* Globals:     none
*
* Locals:      none
*
* Return:      fp - pointer to the stream of the file opened.
*
**************************************************************************/
char *debug_file_date(void)
{
  static char time_buffer[80] = {""};
  time_t timesec;
  struct tm *currtime;

  timesec = time(NULL);
  currtime = localtime(&timesec);
  /* filename string to be YYYYMMDD */
  strftime(time_buffer,sizeof(time_buffer),"%Y%m%d",currtime);

  return time_buffer;
}

/******************************************************************
* NAME:         Interpret_Arguments
* DESCRIPTION:  Takes one of the arguments passed by the main function
*               and sets flags if it matches one of the predefined args.
* PARAMETERS:   argc (IN) number of arguments.
*               argv (IN) an array of arguments in string form.
* GLOBALS:      none
* RETURN:       none
* ALGORITHM:    none
* NOTES:        none
*
******************************************************************/
void Interpret_Arguments(int argc, char *argv[])
{
  int i = 0;
  char *p_arg = NULL;

  if (argc < 2)
  {
    Usage(argv[0]);
    exit(1);
  }

  /* skip 1st one - its the command line for the filename */
  for (i=1;i<argc;i++)
  {
    p_arg = argv[i];
    if (p_arg[0] == '?')
    {
      Usage(argv[0]);
      exit(1);
    }
    else if (p_arg[0] == '-')
    {
      switch(p_arg[1])
      {
        /* debug */
        case 'd':
        case 'D':
          Debug_Flag = TRUE;
          break;

        /* save info to a file */
        case 'f':
        case 'F':
          sscanf(p_arg+2,"%s",&Append_File_Name);
          break;

        /* Header with WKS files */
        case 'h':
        case 'H':
          WKS_Header_Flag = TRUE;
          WKS_Flag = TRUE;
          break;

        /* debug */
        case 'w':
        case 'W':
          WKS_Flag = TRUE;
          break;

        default:
          break;
      } /* end of arguments beginning with - */
    } /* dash arguments */
    else
    {
      /* standard arg is the C filename to be counted */
      sscanf(p_arg,"%s",&C_Filename);
    }
  } /* end of arg loop */
}

/**************************************************************************
*
* Function:    Usage
*
* Description: Prints a help message to the user about how to use this
*              program.
*
* Parameters:  filename - name entered to get this program to run.
*
* Globals:     none
*
* Locals:      none
*
* Return:      none
*
**************************************************************************/
void Usage(char *filename)
{
  char *str;
  char *name;

  /* Find the last token - that is the actual filename */
  str = strtok(filename,":\\");
  while (str != NULL)
  {
    name = str;
    str = strtok(NULL,":\\");
  }

  printf("Logical Line of Code (LOC) Counter for C/C++ - w/Function Count\n");
  printf("Version %s by Steve Karg. Last update %s.\n",
    version_number,version_date);
  printf("\n");
  printf("This program counts the following key words while ignoring\n");
  printf("blank lines and comments.  Also ignores things in quotes and\n");
  printf("pre-compiler options.  Prints out logical line of code count\n");
  printf("for functions, program total, physical line of code count, and\n");
  printf("comment line of code count.\n");
  printf("\n");
  keyword_print();
  printf("\n");
  printf("\n");
  printf("Usage:\n");
  if (name != NULL)
    printf("%s filename [-f] [-d]\n",name);
  else
    printf("FCLOC filename [[d]ebug]\n");
  printf("-f  place into a file\n");
  printf("-w  WKS format (CSV)\n");
  printf("-h  WKS format with header (CSV)\n");
  printf("-d  place debug info into a file\n");
  printf("\n");
  return;
}

