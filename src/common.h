#pragma once

// sepples
#include <string>
#include <vector>
#include <fstream>
#include <map>

// cstd
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// linoox
#include <errno.h>
#include <pthread.h>
#include <spawn.h>
#include <signal.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/time.h>

#include "utility.h"


#ifdef _WIN32

#include "win_elf.h"
#pragma warning(disable:4996)   //disable lame _CRT_SECURE error (actually warning)
#define Break __debugbreak
typedef int pthread_mutex_t;
typedef int pthread_cond_t;
typedef int pthread_t;
typedef int sem_t;
typedef int pid_t;
typedef intptr_t ssize_t;

#else

#include <elf.h>
#define Break() asm("int $3")

#endif

#define PrintTrace() 
#define Fatal(msg) Verify(0 && msg)
#define ArrayCount(arr) (sizeof(arr) / sizeof(arr[0]))
#define tsnprintf(buf, fmt, ...) snprintf(buf, sizeof(buf), fmt, ##__VA_ARGS__)

// c standard wrappers
#define Assert(cond)\
if ( !(cond) )\
{\
    char __gdb_buf[128]; tsnprintf(__gdb_buf, "gdb --pid %d", int(getpid())); (void)system(__gdb_buf); exit(0); \
}
#define Malloc malloc
#define Calloc malloc
#define Free free

// log errno and strerror, along with user error message
#define PrintErrorLibC(msg) PrintErrorf("%s, errno(%d): %s", msg, errno, strerror(errno))

// log user error message
#define PrintError(str) PrintErrorf("%s", str)
#define PrintErrorf(fmt, ...)\
do {\
    fprintf(stderr, "(%s : %d : %s) ", __FILE__, __LINE__, __FUNCTION__);\
    fprintf(stderr, fmt, ##__VA_ARGS__);\
    Assert(0);\
    exit(0);\
} while(0)

#define Printf(fmt, ...) printf(fmt, ##__VA_ARGS__);
#define Print(msg) Printf("%s", msg);

#define NUM_LOG_ROWS 10
#define NUM_LOG_COLS 60
#define INVALID_LINE 0
#define MAX_USER_CMDSIZE 128
#define NUM_USER_CMDS 80
#define RECORD_ROOT_IDX 0

// prefix for preventing name clashes
#define GLOBAL_NAME_PREFIX "GB__"
#define LOCAL_NAME_PREFIX "LC__"
#define WATCH_NAME_PREFIX "WT__"

#define TUG_CONFIG_FILENAME "tug.ini"

// arm32
const char *const REG_ARM32[] = {
    "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8",
    "r9", "r10", "r11", /*"fp",*/ "r12", "sp", "lr", "pc", "cpsr",
};


// amd64
const char *const REG_AMD64[] = {
    "rax", "rbx", "rcx", "rdx", 
    "rbp", "rsp", "rip", "rsi", 
    "rdi", "r8", "r9", "r10", "r11", 
    "r12", "r13", "r14", "r15"
};

// TODO: threads
struct Frame
{
    String func;
    String addr;        // current PC/IP
    size_t file_idx;    // in prog.files
    size_t line;        // next line to be executed
};

struct Breakpoint
{
    size_t number;      // ordinal assigned by GDB
    size_t line;        // file line number
    size_t file_idx;    // index in prog.files
};

struct VarObj
{
    String name;
    String value;
    bool changed;
};

struct FileContext
{
    Vector<String> lines;
    String fullpath;

    static bool Create(const char *fullpath, FileContext &result)
    {
        result = {};
        result.fullpath = fullpath;
        result.lines.push_back("");    // lines[0] empty for syncing index with line number
        bool found = false;

        if (0 == access( fullpath, F_OK ))
        {
            found = true;
            std::ifstream file(fullpath, std::ios::in);

            String tmp;
            int line = 1;
            char linebuf[10];

            while (std::getline(file, tmp))
            {
                tsnprintf(linebuf, "%-4d ", line++);
                result.lines.emplace_back(linebuf + tmp);
            }
        }

        return found;
    }
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

const Span INVALID_ATOM_SPAN = {};

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

#define RECORD_ENDSIG "(gdb)"
#define RECORD_ENDSIG_SIZE ( sizeof(RECORD_ENDSIG) - 1 ) 

#define PREFIX_ASYNC0 '='
#define PREFIX_ASYNC1 '*'
#define PREFIX_RESULT '^'
#define PREFIX_DEBUG_LOG '&'
#define PREFIX_TARGET_LOG '@'
#define PREFIX_CONSOLE_LOG '~'

#define MAX_STORED_BLOCKS 128

const size_t BAD_INDEX = ~0;


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

    // raw data, guarded by modify_storage_lock
    // a block is one or more Records
    Vector<char> blocks;
    Span block_spans[MAX_STORED_BLOCKS];      // block ending in (gdb) endsig
    size_t num_blocks;
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

struct ProgramContext
{
    char log[NUM_LOG_ROWS][NUM_LOG_COLS + 1 /* NT */]; 
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

    Vector<FileContext> files;

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

const size_t NUM_CONFIG = sizeof(ProgramContext::Config) / sizeof(ConfigPair);




// 
// main.cpp
//
extern ProgramContext prog;
void LogLine(const char *raw, size_t rawsize);

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
    char *buf;      // record line data
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
AtomIter GDB_IterChild(const Record &rec, const RecordAtom &array);

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
ssize_t GDB_Send(const char *cmd);

// send a message to GDB, wait for a result record
int GDB_SendBlocking(const char *cmd, const char *header = "^done", bool remove_after = true);

// send a message to GDB, wait for a result record, then retrieve it
int GDB_SendBlocking(const char *cmd, Record &rec, const char *header = "^done");

// extract a MI record
bool GDB_ParseRecord(char *buf, size_t bufsize, ParseRecordContext &ctx);

void GDB_GrabBlockData();
