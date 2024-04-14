// Copyright (C) 2022 Kyle Sylvestre
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

// sepples
#include <string>
#include <vector>

// cstd
#include <sys/wait.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

// linoox
#include <libgen.h>
#include <errno.h>
#include <pthread.h>
#include <spawn.h>
#include <signal.h>
#include <fcntl.h>
#include <semaphore.h>
#include <poll.h>
#include <sys/time.h>
#include <sys/stat.h>

#if defined(__APPLE__)
int sem_timedwait(sem_t *sem, const struct timespec *abs_timeout);
#endif

#if defined(__CYGWIN__)
#include <dirent.h>
#endif

template <typename T>
using Vector = std::vector<T>;
using String = std::basic_string<char, std::char_traits<char>>;

#define VARGS_CHECK(fmt, ...) (0 && snprintf(NULL, 0, fmt, __VA_ARGS__))
#define StringPrintf(fmt, ...) _StringPrintf(VARGS_CHECK(fmt, __VA_ARGS__), fmt, __VA_ARGS__)
String _StringPrintf(int vargs_check, const char *fmt, ...);

#define ArrayCount(arr) (sizeof(arr) / sizeof(arr[0]))
#define tsnprintf(buf, fmt, ...) snprintf(buf, sizeof(buf), fmt, __VA_ARGS__)
#define DefaultInvalid default: PrintError("hit invalid default switch"); break;
#define GetMax(a, b) (a > b) ? a : b
#define GetMin(a, b) (a < b) ? a : b
#define GetPinned(v, min, max) GetMin(GetMax(v, min), max)
#define GetAbs(a, b) (a > b) ? a - b : b - a

template <typename T>
inline void Zeroize(T &value)
{
    memset(&value, 0, sizeof(value));
}

// c standard wrappers
#if !defined(NDEBUG)
#define Assert(cond)\
if ( !(cond) )\
{\
    char _gdb_buf[128]; tsnprintf(_gdb_buf, "gdb --pid %d", (int)getpid()); int _rc = system(_gdb_buf); (void)_rc; exit(1);\
}
#else
#define Assert(cond) (void)0;
#endif

#define Printf(fmt, ...) do { String _msg = StringPrintf(fmt, __VA_ARGS__); WriteToConsoleBuffer(_msg.data(), _msg.size()); } while(0)
#define Print(msg) Printf("%s", msg)

// log user error message
#define PrintError(str) PrintErrorf("%s", str)
#define PrintErrorf(fmt, ...)\
do {\
    fprintf(stderr, "(%s : %s : %d) ", __FILE__, __FUNCTION__, __LINE__);\
    String _msg = StringPrintf("Error " fmt, __VA_ARGS__);\
    fprintf(stderr, "%s", _msg.c_str());\
    WriteToConsoleBuffer(_msg.data(), _msg.size());\
    /*Assert(false);*/\
} while(0)

#define INVALID_LINE 0
#define RECORD_ROOT_IDX 0

// prefix for preventing name clashes
#define GLOBAL_NAME_PREFIX "GB__"
#define LOCAL_NAME_PREFIX "LC__"

// values with child elements from -data-evaluate-expression
// struct: value={ a = "foo", b = "bar", c = "baz" }
// union: value={ a = "foo", b = "bar", c = "baz" }
// array: value={1, 2, 3}
#define AGGREGATE_CHAR_START '{'
#define AGGREGATE_CHAR_END '}'

// maximum amount of variables displayed in an expression if there 
// are no run length values
#define AGGREGATE_MAX 200

const char *const DEFAULT_REG_ARM[] = {
    "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8",
    "r9", "r10", "r11", /*"fp",*/ "r12", "sp", "lr", "pc", "cpsr",
};

const char *const DEFAULT_REG_AMD64[] = {
    "rax", "rbx", "rcx", "rdx", 
    "rbp", "rsp", "rip", "rsi", 
    "rdi", "r8", "r9", "r10", "r11", 
    "r12", "r13", "r14", "r15"
};

const char *const DEFAULT_REG_X86[] = {
    "eax", "ebx", "ecx", "edx", 
    "ebp", "esp", "eip", "esi", 
    "edi",
};

// TODO: threads
struct Frame
{
    String func;
    uint64_t addr;      // current PC/IP
    size_t file_idx;    // in prog.files
    size_t line_idx;    // next line to be executed - 1
};

struct Breakpoint
{
    uint64_t addr;
    size_t number;          // ordinal assigned by GDB
    size_t line_idx;        // file line number - 1
    size_t file_idx;        // index in prog.files
    bool enabled;
    String cond;            
};

struct DisassemblyLine
{
    uint64_t addr;
    String text;
};

struct DisassemblySourceLine
{
    uint64_t addr;
    size_t num_instructions;
    size_t line_idx;
};

struct File
{
    Vector<size_t> lines;   // offset to line within data
    String filename;
    String data;            // file chars excluding line endings
    size_t longest_line_idx;// line with most chars, used for horizontal scrollbar 
};

#define INVALID_BLOCK_STRING_IDX 0

enum AtomType
{
    Atom_None,  // parse state
    Atom_Name,  // parse state
    Atom_Array,
    Atom_Struct,
    Atom_String,
};

// range of data that lives inside another buffer
struct Span
{
    size_t index;
    size_t length;
};

struct RecordAtom
{
    AtomType type;

    // text span inside Record.buf
    Span name;

    // variant variable based upon type
    // array/struct= array span inside Record.atoms
    // string= text span inside Record.buf
    Span value;
};

struct Record
{
    // ordinal that gets send preceding MI-Commands that gets sent
    // back in the response record
    uint32_t id;
    
    // data describing the line elements
    Vector<RecordAtom> atoms;

    // line buffer, RecordAtom name/value strings point to data inside this
    String buf;
};

struct RecordHolder
{
    bool parsed;
    Record rec;
};

struct VarObj
{
    String name;
    String value;
    bool changed;

    // structs, unions, arrays
    Record expr;
    Vector<bool> expr_changed;
};

// run length RecordAtom in expression value 
struct RecordAtomSequence
{
    RecordAtom atom;
    size_t length;
};

struct Thread
{
    int id;
    String group_id;
    bool running;
    bool focused;   // thread is included in ExecuteCommand
};


// GDB MI sends output as human readable lines, starting with a symbol
// * = exec-async-output contains asynchronous state change on the target 
//     (stopped, started, disappeared) 
//
// & = log-stream-output is debugging messages being produced by GDB’s internals. 
//
// ^ = record
//
// @ = The target output stream contains any textual output from the running target. 
//     This is only present when GDB’s event loop is truly asynchronous, 
//     which is currently only the case for remote targets.
//
// ~ = console-stream-output is output that should be displayed 
//     as is in the console. It is the textual response to a CLI command 
//
// after the commands, it ends with signature "(gdb)"

#define PREFIX_ASYNC0 '='
#define PREFIX_ASYNC1 '*'
#define PREFIX_RESULT '^'
#define PREFIX_DEBUG_LOG '&'
#define PREFIX_TARGET_LOG '@'
#define PREFIX_CONSOLE_LOG '~'

const size_t BAD_INDEX = ~0;

#define FILE_IDX_INVALID 0


struct GDB
{
    pid_t spawned_pid;      // process running GDB
    String debug_filename;  // debug executable filename
    String debug_args;      // args passed to debug executable
    String filename;        // filename of spawned GDB 
    String args;            // args passed to spawned GDB 
    String ptty_slave;
    int fd_ptty_master;
    
    bool end_program;
    pthread_t thread_read_interp;
    //pthread_t thread_write_stdin;

    sem_t *recv_block;
    pthread_mutex_t modify_block;

    // MI command sent from GDB
    int fd_in_read;
    int fd_in_write;

    // commands sent to GDB
    int fd_out_read;
    int fd_out_write;

    // ordinal ID that gets incremented on every 
    // GDB_SendBlocking record sent
    uint32_t record_id = 1;

    // raw data, guarded by modify_storage_lock
    // a block is one or more Records
    char block_data[1024 * 1024];
    Vector<Span> block_spans;    // pipe read span into block_data

    // capabilities of the spawned GDB process using -list-features 
    bool has_frozen_varobj;
    bool has_pending_breakpoints;
    bool has_python_scripting_support;
    bool has_thread_info;
    bool has_data_rw_bytes;                 // -data-read-memory bytes and -data-write-memory-bytes
    bool has_async_breakpoint_notification; // bkpt changes make async record
    bool has_ada_task_info;
    bool has_language_option;
    bool has_gdb_mi_command;
    bool has_undefined_command_error_code;
    bool has_exec_run_start;
    bool has_data_disassemble_option_a;     // -data-disassemble -a function

    // capabilities of the target using -list-target-features
    bool supports_async_execution;          // GDB will accept further commands while the target is running.
    bool supports_reverse_execution;        // target is capable of reverse execution

    bool echo_next_no_symbol_in_context;    // GDB MI error "no symbol "xyz" in current context"
                                            // useful sometimes but mostly gets spammed in console
};

struct Program
{
    // console messages ordered from newest to oldest
    char log[64 * 1024];
    bool log_scroll_to_bottom = true;
    size_t log_idx;

    // GDB console history buffer
    String input_cmd_data;
    Vector<size_t> input_cmd_offsets;
    int input_cmd_idx = -1;

    Vector<VarObj> local_vars;      // locals for the current frame
    Vector<VarObj> global_vars;     // watch for entire program, -var-create name @ expr
    Vector<VarObj> watch_vars;      // user defined watch for entire program
    bool running;
    bool started;
    bool source_out_of_date;
    Vector<Breakpoint> breakpoints;
    // TODO: threads, active_thread

    Vector<RecordHolder> read_recs;
    size_t num_recs;

    Vector<File> files;
    Vector<Thread> threads;
    Vector<Frame> frames;
    size_t frame_idx = BAD_INDEX;
    size_t file_idx = BAD_INDEX;
    size_t thread_idx = BAD_INDEX;
    pid_t inferior_process;
    String stack_sig;               // string of all function names combined
};

extern Program prog;
extern GDB gdb;

const char *GetErrorString(int _errno);
void WriteToConsoleBuffer(const char *raw, size_t rawsize);
bool VerifyFileExecutable(const char *filename);
bool DoesFileExist(const char *filename, bool print_error_on_missing = true);
bool DoesProcessExist(pid_t p);
bool InvokeShellCommand(String command, String &output);
void TrimWhitespace(String &str);
