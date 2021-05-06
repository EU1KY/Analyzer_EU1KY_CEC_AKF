#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"
#include "shell.h"
#include "build_timestamp.h"
#include "aauart.h"

#define KEY_TAB                 9
#define KEY_ESC                 27
#define KEY_SPECIAL_FIRST       91
#define KEY_SPECIAL_UP          65
#define KEY_SPECIAL_DOWN        66
#define KEY_CTRL_Z              26

typedef enum
{
    SHELL_SPECIAL_KEY_STEP_NONE = 0,
    SHELL_SPECIAL_KEY_GETCH_STEP_1,
    SHELL_SPECIAL_KEY_STEP_1,
    SHELL_SPECIAL_KEY_STEP_2,
} shell_special_key_step_t;

typedef enum
{
    SHELL_SPECIAL_KEY_NONE = 0,
    SHELL_SPECIAL_KEY_ESC,
    SHELL_SPECIAL_KEY_UP,
    SHELL_SPECIAL_KEY_DOWN,
} shell_special_key_t;

static struct _shell_history
{
    char      log[SHELL_MAX_CMD_LINE_HISTORY][SHELL_MAX_COMMAND_LINE];
    uint32_t  next;
    uint32_t  size;
} s_shell_history __attribute__((section(".user_sdram")));

static struct _shell
{
    uint32_t                        num_tokens;                 // number of parsed tokens in command line
    char*                           token[SHELL_MAX_TOKENS];          // command token pointer list
    char                            cmdline[SHELL_MAX_COMMAND_LINE];  // user command string
    uint32_t                        cmdline_tail;               // command string point < SHELL_MAX_COMMAND_LINE
    char                            prompt[8];                  // prompt string
    const shell_cmd_t*              cmd_pair;
} s_shell __attribute__((section(".user_sdram")));

// Defined in the linker script
extern char __shell_area_start;
extern char __shell_area_end;
#define __shell_area_len (uint32_t)(&__shell_area_end - &__shell_area_start)

#define SHELLCMDPAIR_TBL_BASE_ADDR          (uint32_t)&(__shell_area_start)
#define SHELLCMDPAIR_TBL_ENTRY_NUM          (__shell_area_len/sizeof(shell_cmd_t))

static const char* _shell_print_cmd_suggestions(const char* cmdline_part, uint32_t size);
static void _shell_print_cmd_completition(const char* cmdline_part, uint32_t typed, const char* suggested);
static shell_special_key_t _shell_check_special_key(char c);
static volatile bool is_in_shell_rx_proc = false;

void shell_rx_proc(void)
{
    if (is_in_shell_rx_proc)
    { //  prevent recursion
        return;
    }

    int ch;
    is_in_shell_rx_proc = true;

    while (1)
    {
        ch = AAUART_Getchar();
        if (EOF == ch)
        {
            is_in_shell_rx_proc = false;
            return;
        }
        shell_task((char)ch);
    }
    is_in_shell_rx_proc = false;
}

static uint8_t _asc2num_hex(uint8_t asc)
{
    if ('0'<=asc && asc<='9') return asc-'0';

    if ('A'<=asc && asc<='F') return asc-'A'+10;

    if ('a'<=asc && asc<='f') return asc-'a'+10;

    return 16;
}

static uint8_t _asc2num_dec(uint8_t asc)
{
    if ('0' <= asc && asc <= '9') return asc - '0';
    return 10;
}

static void _str2num(char* p_str, uint32_t* pNum)
{
    uint32_t num;
    uint32_t value = 0;
    int32_t sign = 1;

    if (!strncmp(p_str, "0x", 2))
    {
        p_str += 2;
        while ((num = _asc2num_hex(*p_str)) < 16)
        {
            value = (value << 4) + num;
            p_str++;
        }
    }
    else
    {
        if (*p_str == '-')
        {
            sign = -1;
            p_str++;
        }
        while ((num = _asc2num_dec(*p_str)) < 10)
        {
            value = (value * 10) + num;
            p_str++;
        }
    }

    *pNum = (value * sign);
}

static void _shell_clear_line(void)
{
    while (s_shell.cmdline_tail > 0)
    {
        printf("\b \b");
        s_shell.cmdline_tail--;
    }
}

static void _shell_terminate_cmd(void)
{
    const shell_cmd_t* cmd_pair = s_shell.cmd_pair;

    for (uint32_t i = 0; i < SHELLCMDPAIR_TBL_ENTRY_NUM; i++)
    {
        size_t sz1 = strlen("terminate");
        size_t sz2 = strlen(cmd_pair->cmd_name);
        size_t szmax = (sz1 > sz2) ? sz1 : sz2;
        if (strncmp("terminate", cmd_pair->cmd_name, szmax) == 0)
        {
            cmd_pair->cmd_proc(s_shell.num_tokens, s_shell.token);
            break;
        }
        cmd_pair++;
    }
}

static void _shell_put_char(char c)
{
    // tolower()
    if (c >= 'A' && c <= 'Z')
    {
        c += 0x20;
    }

    if (c == '\b')
    {
        if (s_shell.cmdline_tail > 0)
        {
            printf("\b \b");
            s_shell.cmdline_tail--;
        }
    }
    else if (s_shell.cmdline_tail < (SHELL_MAX_COMMAND_LINE - 1) && (c > 0x1F && c < 0x7F))
    {
        s_shell.cmdline[s_shell.cmdline_tail] = c;
        s_shell.cmdline_tail++;

        printf("%c", c);
    }
}

static void _shell_cmdline_history(uint32_t rewind)
{
    _shell_clear_line();

    if (s_shell_history.size)
    {
        uint32_t next = s_shell_history.next;
        uint32_t index = (next >= rewind) ? (next - rewind) : (SHELL_MAX_CMD_LINE_HISTORY + next - rewind);

        char *str = s_shell_history.log[index];
        while (*str)
        {
            _shell_put_char(*str++);
        }
    }
}

static shell_special_key_t _shell_check_special_key(char c)
{
    static shell_special_key_step_t special_key_step = SHELL_SPECIAL_KEY_STEP_NONE;

    shell_special_key_step_t prev_special_key_step = special_key_step;
    shell_special_key_t special_key = SHELL_SPECIAL_KEY_NONE;

    special_key_step = SHELL_SPECIAL_KEY_STEP_NONE;
    if (c == KEY_ESC)
    {
        special_key_step = SHELL_SPECIAL_KEY_STEP_1;
        special_key = SHELL_SPECIAL_KEY_ESC;
    }
    else
    {
        if (prev_special_key_step == SHELL_SPECIAL_KEY_STEP_1)
        {
            if(c == KEY_SPECIAL_FIRST )
            {
                special_key_step = SHELL_SPECIAL_KEY_STEP_2;
                special_key = SHELL_SPECIAL_KEY_ESC;
            }
        }
        else if (prev_special_key_step == SHELL_SPECIAL_KEY_STEP_2)
        {
            if(c == KEY_SPECIAL_UP )
            {
                special_key = SHELL_SPECIAL_KEY_UP;
            }
            else if(c == KEY_SPECIAL_DOWN )
            {
                special_key = SHELL_SPECIAL_KEY_DOWN;
            }
        }
    }
    return special_key;
}
static bool _shell_get_line(char c)
{
    char* cmd_string = s_shell.cmdline;

    if (c == 0)
    {
        return false;
    }

    if (c == KEY_ESC)
    {
        _shell_clear_line();
    }
    else if (c == KEY_CTRL_Z)
    {
        _shell_terminate_cmd();
    }
    else if (c == KEY_TAB)
    {
        const char* single_suggestion = _shell_print_cmd_suggestions(s_shell.cmdline, s_shell.cmdline_tail);
        _shell_print_cmd_completition(s_shell.cmdline, s_shell.cmdline_tail, single_suggestion);
    }
    else
    {
        _shell_put_char(c);
    }

    static uint32_t s_rewind = 0;
    shell_special_key_t special_key = _shell_check_special_key(c);

    // reset history current
    if (special_key == SHELL_SPECIAL_KEY_NONE)
    {
        s_rewind = 0;
    }

    if (special_key > SHELL_SPECIAL_KEY_ESC)
    {
        switch (special_key)
        {
            case SHELL_SPECIAL_KEY_UP:
                if(s_rewind < s_shell_history.size)
                {
                    s_rewind++;
                }
                _shell_cmdline_history(s_rewind);
                break;

            case SHELL_SPECIAL_KEY_DOWN:
                if(s_rewind > 1)
                {
                    s_rewind--;
                }
                _shell_cmdline_history(s_rewind);
                break;
            default:
                break;
        }
    }

    if (c != '\r')
    {
        return false;
    }
    else
    {
        *(cmd_string + s_shell.cmdline_tail) = '\0';

        // history log
        bool duplicate = false;
        if (s_shell_history.size)
        {
            uint32_t recent = (s_shell_history.next) ? (s_shell_history.next - 1) : (SHELL_MAX_CMD_LINE_HISTORY - 1);
            if (strncmp(s_shell_history.log[recent], cmd_string, SHELL_MAX_COMMAND_LINE) == 0)
            {
                duplicate = true;
            }
        }

        if (s_shell.cmdline_tail && duplicate == false)
        {
            memcpy(s_shell_history.log[s_shell_history.next], s_shell.cmdline, SHELL_MAX_COMMAND_LINE);

            if (++s_shell_history.next >= SHELL_MAX_CMD_LINE_HISTORY)
            {
                s_shell_history.next = 0;
            }

            if (s_shell_history.size < SHELL_MAX_CMD_LINE_HISTORY)
            {
                s_shell_history.size++;
            }
        }

        s_shell.cmdline_tail = 0;
        printf("\r\n");
        return true;
    }
}

void shell_prompt(void)
{
    printf("%s> ", s_shell.prompt);
}

static bool _shell_parse_cmd(void)
{
    const shell_cmd_t* cmd_pair = s_shell.cmd_pair;
    bool ret = false;

    for (uint32_t i = 0; i < SHELLCMDPAIR_TBL_ENTRY_NUM; i++)
    {
        size_t sz1 = strlen(s_shell.cmdline);
        size_t sz2 = strlen(cmd_pair->cmd_name);
        size_t szmax = (sz1 > sz2) ? sz1 : sz2;
        if (strncmp(s_shell.cmdline, cmd_pair->cmd_name, szmax) == 0)
        {
            cmd_pair->cmd_proc(s_shell.num_tokens, s_shell.token);
            ret = true;
            break;
        }
        cmd_pair++;
    }

    return ret;
}

static const char* _shell_print_cmd_suggestions(const char* cmdline_part, uint32_t size)
{
    if (size < 1)
    {
        return (const char*)0;
    }

    printf("\r\n");
    uint32_t candidates = 0;
    const shell_cmd_t* candidate_cmd_pair = (const shell_cmd_t*)0;
    const shell_cmd_t* cmd_pair = s_shell.cmd_pair;
    for (uint32_t i = 0; i < SHELLCMDPAIR_TBL_ENTRY_NUM; i++)
    {
        if (strncmp(cmdline_part, cmd_pair->cmd_name, size) == 0)
        {
            printf("%s\r\n", cmd_pair->cmd_name);
            candidates++;
            candidate_cmd_pair = cmd_pair;
        }
        cmd_pair++;
    }
    return (1 == candidates) ? candidate_cmd_pair->cmd_name : (const char*)0;
}

static void _shell_print_cmd_completition(const char* cmdline_part, uint32_t typed, const char* suggested)
{
    printf("> ");
    for (unsigned int i = 0; i < typed; i++)
    {
        printf("%c", cmdline_part[i]);
    }

    if (suggested != NULL)
    {
        uint32_t complete_to = strlen(suggested);
        for (uint32_t i = typed; i < complete_to; i++)
        {
            _shell_put_char(suggested[i]);
        }
        _shell_put_char(' ');
    }
}

void shell_init(const char* prompt)
{
    // These structs are in uninit .axisram section, so they must be zeroized explicitly
    memset(&s_shell, 0, sizeof(s_shell));
    strncpy(s_shell.prompt, prompt, sizeof(s_shell.prompt));
    s_shell.cmd_pair = (shell_cmd_t *)SHELLCMDPAIR_TBL_BASE_ADDR;
    memset(&s_shell_history, 0, sizeof(s_shell_history));

    shell_prompt();
}

void shell_task(char key)
{
    if ( _shell_get_line(key) == false )
    {
        return;
    }

    s_shell.num_tokens = 0;
    s_shell.token[s_shell.num_tokens] = strtok(s_shell.cmdline, " ");

    while (s_shell.token[s_shell.num_tokens])
    {
        if (++s_shell.num_tokens >= SHELL_MAX_TOKENS)
        {
            break;
        }
        s_shell.token[s_shell.num_tokens] = strtok((char*)NULL, " ,");
    }

    if (s_shell.num_tokens)
    {
        if (!_shell_parse_cmd())
        {
            printf("Invalid command\r\n");
            _shell_print_cmd_suggestions(s_shell.token[0],strlen(s_shell.token[0]));
        }
        //printf("\r\n");
    }
    shell_prompt();
}

void shell_run(char* str)
{
    while (*str)
    {
        shell_task(*str);
        if ('\r' == *str)
        {
            break;
        }
        str++;
    }

    if ('\0' == *str)
    {
        shell_task('\r');
    }
}

void shell_str2num(char* p_str, uint32_t* pNum)
{
    _str2num(p_str, pNum);
}

void shell_get_args(uint32_t argv[])
{
    for (uint32_t i = 1; i < s_shell.num_tokens; i++)
    {
        _str2num((char *)s_shell.token[i], &argv[i-1]);
    }
}

int32_t shell_argtoi(char* arg)
{

    if ( (*arg) == '-'|| strncmp("0x", arg, 2))
    {
        return (int32_t)atoi(arg);
    }
    else
    {
        return (int32_t)strtoul(arg, NULL, 16);
    }
}

bool shell_cmd_help(uint32_t argc, char* const argv[])
{
    const shell_cmd_t* cmd_pair = s_shell.cmd_pair;

    for (uint32_t i = 0; i < SHELLCMDPAIR_TBL_ENTRY_NUM; i++)
    {
        if (argc == 1 || (argc == 2 && '\0' != argv[1][0] &&
            0 == strncmp(argv[1], cmd_pair->cmd_name,strlen(argv[1]))
        ))
        {
            printf("  %-20s - %-8s\r\n", cmd_pair->cmd_name, cmd_pair->cmd_help);
        }
        cmd_pair++;
    }
    return false;
}
SHELL_CMD(help, "show available commands", shell_cmd_help);

