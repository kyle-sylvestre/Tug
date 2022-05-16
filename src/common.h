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
#include <fstream>

// cstd
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

// linoox
#include <errno.h>
#include <pthread.h>
#include <spawn.h>
#include <signal.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/time.h>
#include <sys/stat.h>

//
// STL Switcheroo
//

extern "C" {
    void *dlmalloc(size_t);
    void dlfree(void*);
}

template <class T>
struct DL_Allocator
{
    // the ugly stuff
    typedef T value_type;
    typedef char char_type;

    DL_Allocator() noexcept {} //default ctor not required by C++ Standard Library
    template<class U> DL_Allocator(const DL_Allocator<U>&) noexcept {}
    template<class U> bool operator==(const DL_Allocator<U>&) const noexcept
    {
        return true;
    }
    template<class U> bool operator!=(const DL_Allocator<U>&) const noexcept
    {
        return false;
    }

    // the good stuff
    T* allocate(const size_t n)
    {
        return (T *)dlmalloc(n * sizeof(T));
    }
    void deallocate(T* const p, size_t)
    {
        dlfree(p);
    }
};

template <typename T>
using Vector = std::vector<T, DL_Allocator<T>>;

using String = std::basic_string<char, std::char_traits<char>, DL_Allocator<char>>;

#define ArrayCount(arr) (sizeof(arr) / sizeof(arr[0]))
#define tsnprintf(buf, fmt, ...) snprintf(buf, sizeof(buf), fmt, __VA_ARGS__)
#define DefaultInvalid default: Assert(false);
#define GetMax(a, b) (a > b) ? a : b
#define GetMin(a, b) (a < b) ? a : b
#define GetAbs(a, b) (a > b) ? a - b : b - a

// c standard wrappers
#if !defined(NDEBUG)
#define Assert(cond)\
if ( !(cond) )\
{\
    char __gdb_buf[128]; tsnprintf(__gdb_buf, "gdb --pid %d", (int)getpid()); system(__gdb_buf); exit(1);\
}
#else
#define Assert(cond) (void)0;
#endif

// log user error message
#define PrintError(str) PrintErrorf("%s", str)
#define PrintErrorf(fmt, ...)\
do {\
    fprintf(stderr, "(%s : %s : %d) ", __FILE__, __FUNCTION__, __LINE__);\
    fprintf(stderr, fmt, __VA_ARGS__);\
    /*Assert(false);*/\
} while(0)

#define NUM_LOG_ROWS 40
#define NUM_LOG_COLS 128
#define INVALID_LINE 0
#define MAX_USER_CMDSIZE 128
#define NUM_USER_CMDS 80
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

#define TUG_CONFIG_FILENAME "tug.ini"

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
    size_t line;        // next line to be executed
};

struct Breakpoint
{
    uint64_t addr;
    size_t number;      // ordinal assigned by GDB
    size_t line;        // file line number
    size_t file_idx;    // index in prog.files
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
    size_t line_number;
};

struct File
{
    Vector<String> lines;
    String fullpath;
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

#define MAX_STORED_BLOCKS 128

const size_t BAD_INDEX = ~0;

#define FILE_IDX_INVALID 0


struct GDB
{
    pid_t spawned_pid;      // process running GDB
    
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
    Span block_spans[MAX_STORED_BLOCKS];    // pipe read span into block_data
    size_t num_blocks;

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
};

enum ConfigType
{
    ConfigType_Text, 
    ConfigType_File, 
    ConfigType_Bool, 
};

struct ConfigPair
{
    const char *const key;
    String value;
    ConfigType type;

    ConfigPair(const char *const pkey, 
               ConfigType ptype = ConfigType_Text) : key(pkey), value(""), type(ptype) {}
};

enum ConsoleLineType
{
    ConsoleLineType_None,
    ConsoleLineType_UserInput,
};

struct ConsoleLine
{
    ConsoleLineType type;
    char text[NUM_LOG_COLS + 1 /* NT */];
};

struct Program
{
    // console messages ordered from newest to oldest
    ConsoleLine log[NUM_LOG_ROWS];
    bool log_scroll_to_bottom = true;
    size_t log_line_char_idx;

    char input_cmd[NUM_USER_CMDS][MAX_USER_CMDSIZE + 1 /* NT */];
    int input_cmd_idx = -1;
    int num_input_cmds;

    Vector<VarObj> other_frame_vars;// one-shot view for non-current frame values
    Vector<VarObj> local_vars;      // watch for the current frame, -var-create name * expr
    Vector<VarObj> global_vars;     // watch for entire program, -var-create name @ expr
    Vector<VarObj> watch_vars;      // user defined watch for entire program
    bool running;
    bool started;
    Vector<Breakpoint> breakpoints;
    // TODO: threads, active_thread

    Vector<RecordHolder> read_recs;
    size_t num_recs;

    Vector<File> files;

    Vector<Frame> frames;
    size_t frame_idx = BAD_INDEX;
    pid_t inferior_process;

    // key=value .ini configuration 
    struct Config
    {
        ConfigPair gdb_path         = ConfigPair("gdb_path", ConfigType_File);
        ConfigPair gdb_args         = ConfigPair("gdb_args");
        ConfigPair debug_exe_path   = ConfigPair("debug_exe_path", ConfigType_File);
        ConfigPair debug_exe_args   = ConfigPair("debug_exe_args");
        ConfigPair font_filename    = ConfigPair("font_filename", ConfigType_File);
        ConfigPair font_size        = ConfigPair("font_size");
    } config;

};

const size_t NUM_CONFIG = sizeof(Program::Config) / sizeof(ConfigPair);




// 
// main.cpp
//
extern Program prog;
void WriteToConsoleBuffer(const char *raw, size_t rawsize);

//
// gdb.cpp
//
extern GDB gdb;

// record management functions
struct ParseRecordContext
{
    Vector<RecordAtom> atoms;
    size_t atom_idx;
    size_t num_end_atoms;   // contiguous atoms stored at the end of atoms

    bool error;

    size_t i;
    const char *buf;      // record line data
    size_t bufsize;
};

// traverse through all the child elements of an array/struct
struct AtomIter
{
    const RecordAtom *iter_begin;
    const RecordAtom *iter_end;
    const RecordAtom *begin() { return iter_begin; }
    const RecordAtom *end() { return iter_end; }
};
AtomIter GDB_IterChild(const Record &rec, const RecordAtom *array);

// extract values from parsed records
String GDB_ExtractValue(const char *name, const RecordAtom &root, const Record &rec);
int GDB_ExtractInt(const char *name, const RecordAtom &root, const Record &rec);
const RecordAtom *GDB_ExtractAtom(const char *name, const RecordAtom &root, const Record &rec);

// helper functions for searching the root node of a record 
String GDB_ExtractValue(const char *name, const Record &rec);
int GDB_ExtractInt(const char *name, const Record &rec);
const RecordAtom *GDB_ExtractAtom(const char *name, const Record &rec);

inline String GetAtomString(Span s, const Record &rec)
{
    Assert(s.index + s.length <= rec.buf.size());
    String result = {};
    result.assign(rec.buf.data() + s.index, s.length);
    return result;
}

// send a message to GDB, don't wait for result
bool GDB_Send(const char *cmd);

// send a message to GDB, wait for a result record
bool GDB_SendBlocking(const char *cmd, bool remove_after = true);

// send a message to GDB, wait for a result record, then retrieve it
bool GDB_SendBlocking(const char *cmd, Record &rec);

// extract a MI record from a newline terminated line
bool GDB_ParseRecord(char *buf, size_t bufsize, ParseRecordContext &ctx);

void GDB_GrabBlockData();

RecordAtomSequence GDB_RecurseEvaluation(ParseRecordContext &ctx);

typedef void AtomIterator(Record &rec, RecordAtom &iter, void *ctx);
void IterateAtoms(Record &rec, RecordAtom &iter, AtomIterator *iterator, void *ctx);

void GDB_PrintRecordAtom(const Record &rec, const RecordAtom &iter, int tab_level, FILE *out = stdout);
