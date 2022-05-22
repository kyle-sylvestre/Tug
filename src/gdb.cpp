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

#include "common.h"
#include "gdb.h"

static bool VerifyFileExecutable(const char *filename)
{
    bool result = false;
    struct stat sb = {};

    if (0 != stat(filename, &sb))
    {
        PrintErrorf("stat filename \"%s\": %s\n", filename, strerror(errno));
    }
    else
    {
        if (!S_ISREG(sb.st_mode) || (sb.st_mode & S_IXUSR) == 0)
        {
            PrintErrorf("file not executable: %s\n", filename);
        }
        else
        {
            result = true;
        }
    }
    
    return result;
}

ssize_t read_block_maxsize = 0;
static void *ReadInterpreterBlocks(void *)
{
    // read data from GDB pipe
    size_t insert_idx = 0;
    size_t read_base_idx = 0;
    bool set_read_start_idx = true;

    while (true)
    {
        if (set_read_start_idx)
            read_base_idx = insert_idx;

        if (ArrayCount(gdb.block_data) - insert_idx < 64 * 1024)
        {
            // wrap around to beginning, moving a partially made record if available
            // TODO: is there a function call to see if there is more read data
            memmove(gdb.block_data, gdb.block_data + read_base_idx, 
                    insert_idx - read_base_idx);
            insert_idx = (insert_idx - read_base_idx);
            read_base_idx = 0;
        }

        ssize_t num_read = read(gdb.fd_in_read, gdb.block_data + insert_idx,
                                ArrayCount(gdb.block_data) - insert_idx);
        if (num_read < 0)
        {
            fprintf(stderr, "gdb read: %s\n", strerror(errno));
            break;
        }

        static int iteration = 0;
        //printf("~%d~\n%d - num read: %zu \n~%d~\n", iteration,
        //       (int)gdb.block_data[ insert_idx + num_read - 1 ], 
        //       (size_t)num_read, iteration);
        printf("~%d~\n%.*s\n~%d~\n", iteration, (int)num_read, 
               gdb.block_data + insert_idx, iteration);
        iteration++;
        insert_idx += num_read;

        if (gdb.block_data[ insert_idx - 1 ] != '\n')
        {
            // GDB blocks I've seen have a max of 64k, this record is
            // split across multiple pipe reads
            set_read_start_idx = false;
            continue;
        }
        else
        {
            set_read_start_idx = true;
        }


        if (gdb.num_blocks + 1 > ArrayCount(gdb.block_spans))
        {
            fprintf(stderr, "exhausted available block spans\n");
            break;
        }

        if (read_block_maxsize < num_read)
            read_block_maxsize = num_read;

        pthread_mutex_lock(&gdb.modify_block);

        // set to the last inserted to get bufsize
        Span span;
        if (gdb.num_blocks == 0) span = {};
        else span = gdb.block_spans[gdb.num_blocks - 1];

        span.index = read_base_idx; 
        span.length = num_read;
        gdb.block_spans[ gdb.num_blocks ] = span;
        gdb.num_blocks++;

        // post a change if the binary semaphore is zero
        int semvalue; 
        sem_getvalue(gdb.recv_block, &semvalue);
        if (semvalue == 0) 
            sem_post(gdb.recv_block);

        pthread_mutex_unlock(&gdb.modify_block);
    }

    printf("closing GDB interpreter read loop...\n");
    return NULL;
}

bool GDB_Init(String gdb_filename, String gdb_args)
{
    int rc = 0;

    int pipes[2] = {};
    rc = pipe(pipes);
    if (rc < 0)
    {
        PrintErrorf("from gdb pipe: %s\n", strerror(errno));
        return false;
    }

    gdb.fd_in_read = pipes[0];
    gdb.fd_in_write = pipes[1];

    rc = pipe(pipes);
    if (rc < 0)
    {
        PrintErrorf("to gdb pipe: %s\n", strerror(errno));
        return false;
    }

    gdb.fd_out_read = pipes[0];
    gdb.fd_out_write = pipes[1];

    rc = pthread_mutex_init(&gdb.modify_block, NULL);
    if (rc < 0) 
    {
        PrintErrorf("pthread_mutex_init: %s\n", strerror(errno));
        return false;
    }

    gdb.recv_block = sem_open("recv_gdb_block", O_CREAT, S_IRWXU, 0);
    if (gdb.recv_block == NULL) 
    {
        PrintErrorf("sem_open: %s\n", strerror(errno));
        return false;
    }

    //if (0 > (gdb.fd_pty_master = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK)))
    //{
    //    PrintErrorf("posix_openpt: %s\n", strerror(errno));
    //    break;
    //}

    //if (0 > (rc = grantpt(gdb.fd_pty_master)) )
    //{
    //    PrintErrorf("grantpt: %s\n", strerror(errno));
    //    break;
    //}
    //if (0 > (rc = unlockpt(gdb.fd_pty_master)) )
    //{
    //    PrintErrorf("grantpt: %s\n", strerror(errno));
    //    break;
    //}
    //printf("pty slave: %s\n", ptsname(gdb.fd_pty_master));



    //rc = chdir("/mnt/c/Users/Kyle/Downloads/Chrome Downloads/ARM/AARCH32");
    //if (rc < 0)
    //{
    //    PrintErrorf("chdir: %s\n", strerror(errno));
    //    break;
    //}

    if (!VerifyFileExecutable(gdb_filename.c_str()))
    {
        return false;
    }
    else
    {
#if 0 // old way, can't get gdb process PID this way
        dup2(gdb.fd_out_read, 0);        // stdin
        dup2(gdb.fd_in_write, 1);        // stdout
        dup2(gdb.fd_in_write, 2);        // stderr

        close(gdb.fd_in_write);

        rc = execl("/usr/bin/gdb-multiarch", "gdb-multiarch", "./advent.out", "--interpreter=mi", NULL);
#endif
        // TODO: using different versions of machine interpreter
        String args = gdb_filename + " " + gdb_args + " --interpreter=mi "; 

        std::string buf;
        size_t startoff = 0;
        size_t spaceoff = 0;
        size_t bufoff = 0;
        Vector<char *> gdb_argv;
        bool inside_string = false;
        bool is_whitespace = true;

        // convert args strings to a char * vector, ended with NULL
        for (size_t i = 0; i < args.size(); i++)
        {
            char c = args[i];
            char p = (i > 0) ? args[i - 1] : '\0';
            is_whitespace &= (c == ' ' || c == '\t');

            // make sure we don't get a command line arg from inside a user string literal
            if ( (c == '\'' || c == '\"') && p != '\\')
            {
                inside_string = !inside_string;
            }

            if (!inside_string && c == ' ')
            {
                spaceoff = i;
                size_t arglen = spaceoff - startoff;
                if (arglen != 0 && !is_whitespace)
                {
                    gdb_argv.push_back((char *)bufoff);
                    buf.insert(buf.size(), &args[ startoff ], arglen);
                    buf.push_back('\0');
                    bufoff += (arglen + 1);
                }
                is_whitespace = true;
                startoff = spaceoff + 1;
            }
        }

        // convert base offsets to char pointers
        gdb_argv.push_back(NULL);
        for (size_t i = 0; i < gdb_argv.size() - 1; i++)
        {
            gdb_argv[i] = (char *)((size_t)gdb_argv[i] + buf.data());
        }

        // get all the environment variables for the process
        Vector<char *> envptr;
        FILE *fsh = popen("printenv", "r");
        if (fsh == NULL)
        {
            PrintErrorf("printenv popen: %s\n", strerror(errno));
        }
        else
        {
            String env;
            char tmp[1024];
            ssize_t tmpread;
            while (0 < (tmpread = fread(tmp, 1, sizeof(tmp), fsh)) )
            {
                env.insert(env.size(), tmp, tmpread);
            }

            size_t lineoff = 0;

            for (size_t i = 0; i < env.size(); i++)
            {
                if (env[i] == '\n')
                {
                    envptr.push_back(&env[0] + lineoff);
                    env[i] = '\0';
                    lineoff = i + 1;
                }
            }

            envptr.push_back(NULL);
            pclose(fsh); fsh = NULL;
        }

        // start the GDB process
        posix_spawn_file_actions_t actions = {};
        posix_spawn_file_actions_adddup2(&actions, gdb.fd_out_read, 0);     // stdin
        posix_spawn_file_actions_adddup2(&actions, gdb.fd_in_write, 1);     // stdout
        posix_spawn_file_actions_adddup2(&actions, gdb.fd_in_write, 2);     // stderr

        posix_spawnattr_t attrs = {};
        posix_spawnattr_init(&attrs);
        posix_spawnattr_setflags(&attrs, POSIX_SPAWN_SETSID);

        rc = posix_spawnp((pid_t *) &gdb.spawned_pid, gdb_filename.c_str(),
                          &actions, &attrs, gdb_argv.data(), envptr.data());
        if (rc != 0) 
        {
            errno = rc;
            PrintErrorf("posix_spawnp: %s\n", strerror(errno));
            return false;
        }

    }

    rc = pthread_create(&gdb.thread_read_interp, NULL, ReadInterpreterBlocks, (void*) NULL);
    if (rc < 0) 
    {
        PrintErrorf("pthread_create: %s\n", strerror(errno));
        return false;
    }

    //GDB_SendBlocking("-environment-cd /mnt/c/Users/Kyle/Documents/commercial-codebases/original/stevie");
    //GDB_SendBlocking("-file-exec-and-symbols stevie");
    //GDB_SendBlocking("-exec-arguments stevie.c");

    // debug GAS
    //GDB_SendBlocking("-environment-cd /mnt/c/Users/Kyle/Documents/commercial-codebases/original/binutils/binutils-gdb/gas");
    //GDB_SendBlocking("-file-exec-and-symbols as-new");

    //GDB_SendBlocking("-environment-cd \"/mnt/c/Users/Kyle/Downloads/Chrome Downloads/ARM/AARCH32\"");
    //GDB_SendBlocking("-file-exec-and-symbols advent.out");


    Record rec;
    if (GDB_SendBlocking("-list-features", rec))
    {
        const char *src = rec.buf.c_str();
        gdb.has_frozen_varobj =                 (NULL != strstr(src, "frozen-varobjs"));
        gdb.has_pending_breakpoints =           (NULL != strstr(src, "pending-breakpoints"));
        gdb.has_python_scripting_support =      (NULL != strstr(src, "python"));
        gdb.has_thread_info =                   (NULL != strstr(src, "thread-info"));
        gdb.has_data_rw_bytes =                 (NULL != strstr(src, "data-read-memory-bytes"));
        gdb.has_async_breakpoint_notification = (NULL != strstr(src, "breakpoint-notifications"));
        gdb.has_ada_task_info =                 (NULL != strstr(src, "ada-task-info"));
        gdb.has_language_option =               (NULL != strstr(src, "language-option"));
        gdb.has_gdb_mi_command =                (NULL != strstr(src, "info-gdb-mi-command"));
        gdb.has_undefined_command_error_code =  (NULL != strstr(src, "undefined-command-error-code"));
        gdb.has_exec_run_start =                (NULL != strstr(src, "exec-run-start-option"));
        gdb.has_data_disassemble_option_a =     (NULL != strstr(src, "data-disassemble-a-option"));

        // TODO: "Whenever a target can change, due to commands such as -target-select, 
        // -target-attach or -exec-run, the list of target features may change, 
        // and the frontend should obtain it again
        // GDB_SendBlocking("-list-target-features", rec);

    }

    if (GDB_SendBlocking("-list-target-features", rec))
    {
        const char *src = rec.buf.c_str();
        gdb.supports_async_execution =          (NULL != strstr(src, "async"));
        gdb.supports_reverse_execution =        (NULL != strstr(src, "reverse"));
    }

    PrintMessagef("spawned %s %s\n", gdb_filename.c_str(), gdb_args.c_str());
    gdb.filename = gdb_filename;
    gdb.args = gdb_args;
    return true;
}

bool GDB_LoadInferior(String filename, String args)
{
    bool result = false;
    char tmp[PATH_MAX];
    char *dir = NULL;

    tsnprintf(tmp, "%s", filename.c_str());
    dir = dirname(tmp);

    if (VerifyFileExecutable(filename.c_str()) && dir != NULL)
    {
        // set the debugged executable
        String str = StringPrintf("-file-exec-and-symbols \"%s\"", 
                                  filename.c_str());

        if (GDB_SendBlocking(str.c_str()))
        {
            // look for source files in the directory of the exe
            str = StringPrintf("-environment-directory \"%s\"", dir);
            if (GDB_SendBlocking(str.c_str()))
            {
                // set the command line arguments for the debugged executable
                bool good_args = true;
                if (args != "")
                {
                    str = StringPrintf("-exec-arguments %s",
                                       args.c_str());

                    good_args = GDB_SendBlocking(str.c_str());
                }

                result = good_args;
                if (result)
                {
                    PrintMessagef("set debug program: %s %s\n", filename.c_str(), args.c_str());
                    gdb.debug_filename = filename;
                    gdb.debug_args = args;
                }
            }
        }
    }

    return result;
}

void GDB_Shutdown()
{
    pthread_kill(gdb.thread_read_interp, SIGINT);
    pthread_join(gdb.thread_read_interp, NULL);
    kill(gdb.spawned_pid, SIGINT);

    pthread_mutex_destroy(&gdb.modify_block);
    sem_close(gdb.recv_block);
    close(gdb.fd_in_read);
    close(gdb.fd_out_read);
    close(gdb.fd_in_write);
    close(gdb.fd_out_write);

    gdb.thread_read_interp = 0;
    gdb.spawned_pid = 0;
    gdb.modify_block = {};
    gdb.fd_in_read = 0;
    gdb.fd_out_read = 0;
    gdb.fd_in_write = 0;
    gdb.fd_out_write = 0;
}

AtomIter GDB_IterChild(const Record &rec, const RecordAtom *parent)
{
    // allow for fudge factor while iterating child atoms
    // need to fail gracefully for bad/missing msgs
    AtomIter result = {};

    if ( (size_t(parent - rec.atoms.data()) < rec.atoms.size()) &&
         (parent->type == Atom_Array || parent->type == Atom_Struct) &&
         (parent->value.index + parent->value.length <= rec.atoms.size()) )
    {
        const Span &span = parent->value;
        result.iter_begin = &rec.atoms[ span.index ];
        result.iter_end = result.iter_begin + span.length;
    }

    return result;
}

static AtomType InferAtomStart(char c)
{
    AtomType result = Atom_None;
    if (c == '{')
    {
        result = Atom_Struct;
    }
    else if (c == '[')
    {
        result = Atom_Array;
    }
    else if (c == '\"')
    {
        result = Atom_String;
    }
    else if ( (c >= 'a' && c <= 'z') || 
              (c >= 'A' && c <= 'Z') || 
              (c == '-' || c == '_') )
    {
        result = Atom_Name;
    }

    return result;
}

static void PushUnorderedAtom(ParseRecordContext &ctx, RecordAtom atom)
{
    Assert(ctx.num_end_atoms <= ctx.atoms.size() &&
           ctx.atom_idx < ctx.atoms.size() - ctx.num_end_atoms);
    memcpy(&ctx.atoms[ ctx.atom_idx ], &atom, sizeof(atom));
    ctx.atom_idx++;
}

static RecordAtom PopUnorderedAtom(ParseRecordContext &ctx, size_t start_idx)
{
    // pop unordered atoms to the end of the array 
    RecordAtom result = {};
    Assert(start_idx <= ctx.atom_idx);
    size_t num_atoms = ctx.atom_idx - start_idx;
    Assert(ctx.atom_idx + num_atoms < ctx.atoms.size());

    RecordAtom *dest = 
        &ctx.atoms[ ctx.atoms.size() - ctx.num_end_atoms - num_atoms ];
    memmove(dest, &ctx.atoms[ start_idx ],
            num_atoms * sizeof(RecordAtom));

    ctx.num_end_atoms += num_atoms;
    result.value.length = num_atoms;
    result.value.index = (num_atoms == 0) ? 0 : dest - &ctx.atoms[0];

    // clear the popped memory
    ctx.atom_idx -= num_atoms;
    //memset(ctx.atoms + ctx.atom_idx, 0, num_atoms * sizeof(RecordAtom));

    return result;
}

static RecordAtom RecurseRecord(ParseRecordContext &ctx)
{
    static const auto RecurseError = [&](const char *message, char error_char)
    {
        fprintf(stderr, "parse record error: %s\n", message);
        fprintf(stderr, "   before error: %.*s\n", int(ctx.i), ctx.buf);
        fprintf(stderr, "   error char: %c\n", error_char);
        fprintf(stderr, "   after error: %.*s\n", int(ctx.bufsize - (ctx.i + 1)), ctx.buf + ctx.i + 1);

        timeval te;
        gettimeofday(&te, NULL); // get current time
        long long msec = te.tv_sec*1000LL + te.tv_usec/1000; 

        char filename[128]; 
        tsnprintf(filename, "badrecord_%lld.txt", msec);

        FILE *f = fopen(filename, "wb");
        fprintf(f, "error message: %s\n", message);
        fprintf(f, "error index: %zu\n", ctx.i);
        fprintf(f, "%.*s", (int)ctx.bufsize, ctx.buf);
        fclose(f);
        
        // force to end then bail out of here
        Assert(false);
        ctx.error = true;
        ctx.i = ctx.bufsize;
    };

    RecordAtom result = {};
    size_t string_start_idx = 0;
    size_t aggregate_start_idx = 0;

    for (; ctx.i < ctx.bufsize; ctx.i++)
    {
        char c = ctx.buf[ ctx.i ];

        // skip over chars outside of string
        if (result.type != Atom_String && (c == ' ' || c == ',' || c == ';' || c == '_' || c == '\n') ) 
            continue;

        switch (result.type)
        {
            case Atom_None:
            {
                // figure out what kind of block this is 
                AtomType start = InferAtomStart(c);
                if (start == Atom_String)
                {
                    // start after " index
                    string_start_idx = ctx.i + 1;
                }
                else if (start == Atom_Name)
                {
                    string_start_idx = ctx.i;
                }
                else if (start == Atom_Array || start == Atom_Struct)
                {
                    // store the start of the aggregates
                    aggregate_start_idx = ctx.atom_idx;
                }
                else if (start == Atom_None)
                {
                    RecurseError("can't deduce block type", c);
                    continue;
                }

                result.type = start;

            } break;

            case Atom_Name:
            {
                if (c == '=')
                {
                    // make the string, reset block state
                    Assert(ctx.i >= string_start_idx);
                    result.name.index = string_start_idx;
                    result.name.length = ctx.i - string_start_idx;
                    result.type = Atom_None;
                }
                else if (Atom_Name != InferAtomStart(c))
                {
                    RecurseError("hit bad atom name character", c);
                    continue;
                }

            } break;

            case Atom_String:
            {
                // TODO: pointer previews of strings are messing this up
                //       ex: value="0x555555556004 "%d""
                char n = '\0';
                char p = '\0';
                if (ctx.i + 1 < ctx.bufsize)
                    n = ctx.buf[ ctx.i + 1 ];
                if (ctx.i >= 1)
                    p = ctx.buf[ ctx.i - 1 ];

                if (c == '"' && p != '\\' && (n == ',' || n == '}' || n == ']'))
                {
                    // hit closing quote
                    // make the string, advance idx, return
                    Assert(ctx.i >= string_start_idx);
                    result.value.index = string_start_idx;
                    result.value.length = ctx.i - string_start_idx;
                    return result;
                }
            } break;

            case Atom_Array:
            case Atom_Struct:
            {
                AtomType start = InferAtomStart(c);

                if (start != Atom_None)
                {
                    // start of new elem, recurse and add
                    RecordAtom elem = RecurseRecord(ctx);
                    PushUnorderedAtom(ctx, elem);
                }
                else if (c == ']' || c == '}')
                {
                    if ((c == ']' && result.type != Atom_Array) ||
                        (c == '}' && result.type != Atom_Struct))
                    {
                        // hit wrong ending character
                        char buf[128] = {};
                        const char *type_str = (result.type == Atom_Array) ? "array" : "struct";
                        tsnprintf(buf, "wrong ending character for %s", type_str);
                        RecurseError(buf, c);
                    }
                    else
                    {
                        // end of aggregate, pop from unordered and store in gdb 
                        RecordAtom pop = PopUnorderedAtom(ctx, aggregate_start_idx);
                        result.value.index = pop.value.index;
                        result.value.length = pop.value.length;
                        return result;
                    }
                }
                else
                {
                    RecurseError("hit bad aggregate char", c);
                    continue;
                }

            } break;
        }
    }

    return result;
}

RecordAtomSequence GDB_RecurseEvaluation(ParseRecordContext &ctx)
{
    // parse the atom of a -data-evaluate-expression
    // close to a GDB record, but not quite the same differences being: 
    // -not packed, there are spaces in the buffer
    // -array has {} syntax instead of []
    // -for arrays, if there are more than 200 elements it ends in "...}"
    // -run length for arrays ex: {0 <repeats 1024 times>}

    const auto EvaluateRunLength = [&](size_t &rle_last_idx, 
                                       size_t &rle_num_repeat) -> bool
    {
        bool atom = false;
        if (ctx.i + 10 < ctx.bufsize &&
            0 == strcmp(&ctx.buf[ ctx.i + 2 ], "<repeats "))
        {
            // TODO: make run length atom that precedes an atom array/value with
            // a <repeats XXX times> string appended to it
            // Options:
            // 1. carry around run length signifier in atom
            // 2. expand <repeats XXX times> text out 

            atom = true;
            rle_num_repeat = 0;
            size_t dig_idx = ctx.i + 10 + 1;
            for (; dig_idx < ctx.bufsize; dig_idx++)
            {
                char c = ctx.buf[ dig_idx ];
                if (c >= '0' && c <= '9')
                {
                    rle_num_repeat *= 10;
                    rle_num_repeat += (c - '0');
                }
                else 
                {
                    Assert(c == ' ');
                    break;
                }
            }

            // skip over " times>"
            Assert(ctx.buf[ dig_idx ] == ' ');
            rle_last_idx = dig_idx + 6;
        }

        return atom;
    };

    RecordAtomSequence sequence = {};
    sequence.length = 1;
    RecordAtom &atom = sequence.atom;
    size_t string_start_idx = 0;
    size_t aggregate_start_idx = 0;
    bool inside_string_literal = false;
    size_t rle_last_idx = 0;
    size_t rle_num_repeat = 0;
    size_t num_children = 0;

    for (; ctx.i < ctx.bufsize; ctx.i++)
    {
        char c = ctx.buf[ ctx.i ];
        char p = (ctx.i > 1) ? ctx.buf[ ctx.i - 1 ] : '\0';
        char pp = (ctx.i > 2) ? ctx.buf[ ctx.i - 2 ] : '\0';
        char n = (ctx.i + 1 < ctx.bufsize) ? ctx.buf[ ctx.i + 1 ] : '\0';
        char nn = (ctx.i + 2 < ctx.bufsize) ? ctx.buf[ ctx.i + 2 ] : '\0';
        if (pp != '\\' && p == '\\' && c == '\"')
            inside_string_literal = !inside_string_literal;

        if (inside_string_literal)
            continue;

        if (EvaluateRunLength(rle_last_idx, rle_num_repeat) &&
            (atom.type == Atom_Name || atom.type == Atom_String))
        {
            // not a Atom_Name, actually an Atom_String
            Assert(ctx.i >= string_start_idx);
            atom.type = Atom_String;
            atom.value.index = string_start_idx;
            atom.value.length = (ctx.i + 1) - string_start_idx;

            ctx.i = rle_last_idx; //+1 = one past '>' index
            sequence.length = rle_num_repeat;

            return sequence;
        }



        switch (atom.type)
        {

        case Atom_None:
        {
            if (c == ' ' || c == ',')
            {
                continue;
            }
            else if (c == '{')
            {
                // store the start of the aggregates
                aggregate_start_idx = ctx.atom_idx;
                atom.type = Atom_Struct;
            }
            else
            {
                string_start_idx = ctx.i;
                if ((n == ',' || n == '}' || nn == '<') && ctx.i > 0) 
                    ctx.i--; // single digit elements like {0, 1, 2}

                if (atom.name.length == 0)
                {
                    atom.type = Atom_Name;
                }
                else
                {
                    atom.type = Atom_String;
                }
            }
        } break;

        case Atom_Name:
        {

            if (c == '=')
            {
                // name = value, -1 to step back to space index
                Assert(ctx.i - 1 >= string_start_idx);
                atom.name.index = string_start_idx;
                atom.name.length = (ctx.i - 1) - string_start_idx;
                atom.type = Atom_None;
            }
            else if (n == ',' || n == '}')
            {
                // not a Atom_Name, actually an Atom_String
                Assert(ctx.i >= string_start_idx);
                atom.type = Atom_String;
                atom.value.index = string_start_idx;
                atom.value.length = (ctx.i + 1) - string_start_idx;
                return sequence;
            }
        } break;

        case Atom_String:
        {
            if (n == ',' || n == '}')
            {
                Assert(ctx.i >= string_start_idx);
                atom.value.index = string_start_idx;
                atom.value.length = (ctx.i + 1) - string_start_idx;

                return sequence;
            }
        } break;

        case Atom_Array:
        case Atom_Struct:
        {
            if (c == '}')
            {
                // end of aggregate, pop from unordered and store in gdb 
                RecordAtom pop = PopUnorderedAtom(ctx, aggregate_start_idx);
                atom.value.index = pop.value.index;
                atom.value.length = pop.value.length;

                if (EvaluateRunLength(rle_last_idx, rle_num_repeat))
                {
                    ctx.i = rle_last_idx;
                    sequence.length = rle_num_repeat;
                }

                return sequence;
            }
            else
            {
                // start of new elem, recurse and add
                size_t saved_num_end_atoms = ctx.num_end_atoms;
                RecordAtomSequence elem = GDB_RecurseEvaluation(ctx);
                if (elem.atom.name.length == 0)
                    atom.type = Atom_Array;

                if (num_children < AGGREGATE_MAX)
                {
                    size_t addcount = GetMin(elem.length, AGGREGATE_MAX - num_children);
                    for (size_t i = 0; i < addcount; i++)
                        PushUnorderedAtom(ctx, elem.atom);
                    num_children += addcount;
                }
                else
                {
                    // no atoms added, remove any child in order pushes
                    // to the end of the array
                    ctx.num_end_atoms = saved_num_end_atoms;
                }
            }
        } break;

        default:
            break;
        }
    }

    return sequence;
}


void GDB_PrintRecordAtom(const Record &rec, const RecordAtom &iter, 
                         int tab_level, FILE *out)
{
    for (int i = 0; i < tab_level; i++)
        fprintf(out, "  ");

    switch (iter.type)
    {
        case Atom_String:  // key value pair
        {
            fprintf(out, "%.*s=\"%.*s\"\n",
                    int(iter.name.length), &rec.buf[ iter.name.index ],
                    int(iter.value.length), &rec.buf[ iter.value.index ]);
        } break;

        case Atom_Struct:
        case Atom_Array:
        {
            fprintf(out, "%.*s\n", int(iter.name.length), 
                    &rec.buf[ iter.name.index ]);

            for (const RecordAtom &child : GDB_IterChild(rec, &iter))
            {
                GDB_PrintRecordAtom(rec, child, tab_level + 1, out);
            }

        } break;
        default:
            fprintf(out, "---BAD ATOM TYPE---\n");
    }
}

const RecordAtom *GDB_ExtractAtom(const char *name, size_t namelen, 
                                  size_t name_idx, const RecordAtom &iter,
                                  const Record &rec)
{
    // copy segment of full name to temp buffer
    char temp[256] = {};
    const char *dotpos = strchr(name + name_idx, '.');
    size_t end_idx = (dotpos != NULL) ? dotpos - name : namelen;
    memcpy(temp, name + name_idx, end_idx - name_idx);

    unsigned long long index = ~0;
    char *bracket;
    if ( (bracket = strchr(temp, '[')) )
    {
        int num_found = sscanf(bracket, "[%llu]", &index);
        if (num_found != 1)
        {
            PrintError("sscanf error\n");
        }
        else
        {
            size_t bracket_offset = bracket - temp;
            memset(bracket, '\0', sizeof(temp) - bracket_offset);
        }
    }
    size_t tempsize = strlen(temp);

    for (const RecordAtom &child : GDB_IterChild(rec, &iter))
    {
        if (index < child.value.length && child.type == Atom_Array)
        {
            // array[] syntax, select n'th child
            const RecordAtom *child_base = 
                &rec.atoms[ child.value.index ];
            return GDB_ExtractAtom(name, namelen, end_idx + 1,
                                   child_base[index], rec);
        }
        else if (child.name.length == tempsize &&
                 0 == memcmp(temp, &rec.buf[ child.name.index ], tempsize))
        {
            if (end_idx == namelen)
            {
                // found the target
                return &child;
            }
            else
            {
                return GDB_ExtractAtom(name, namelen, end_idx + 1, child, rec);
            }
        }
    }
    return NULL;
}

String GDB_ExtractValue(const char *keyname, const RecordAtom &root, const Record &rec)
{
    String result = "";
    const RecordAtom *target = GDB_ExtractAtom(keyname, strlen(keyname), 0, root, rec);
    if (target)
    {
        Assert(target->type == Atom_String);
        result = GetAtomString(target->value, rec);
    }
    return result;
}

int GDB_ExtractInt(const char *name, const RecordAtom &root, const Record &rec)
{
    int result = 0;
    const RecordAtom *target = GDB_ExtractAtom(name, strlen(name), 0, root, rec);
    if (target)
    {
        Assert(target->type == Atom_String);
        result = atoi( GDB_ExtractValue(name, root, rec).c_str() );
    }
    return result;
}

const RecordAtom *GDB_ExtractAtom(const char *name, const RecordAtom &root,
                                  const Record &rec)
{
    const RecordAtom *result = GDB_ExtractAtom(name, strlen(name), 0, root, rec);
    return result;
}

// 
// helper functions
//
String GDB_ExtractValue(const char *name, const Record &rec)
{
    return (rec.atoms.size() == 0) ? "" : GDB_ExtractValue(name, rec.atoms[0], rec);
}
int GDB_ExtractInt(const char *name, const Record &rec)
{
    return (rec.atoms.size() == 0) ? 0 : GDB_ExtractInt(name, rec.atoms[0], rec);
}
const RecordAtom *GDB_ExtractAtom(const char *name, const Record &rec)
{
    return (rec.atoms.size() == 0) ? NULL : GDB_ExtractAtom(name, rec.atoms[0], rec);
}

void IterateAtoms(Record &rec, RecordAtom &iter, AtomIterator *iterator, void *ctx)
{
    Assert(iter.type == Atom_Struct || iter.type == Atom_Array);
    for (size_t i = 0; i < iter.value.length; i++)
    {
        RecordAtom &child = rec.atoms[iter.value.index + i];
        iterator(rec, child, ctx);
        if (child.type == Atom_Struct || child.type == Atom_Array)
        {
            IterateAtoms(rec, child, iterator, ctx);
        }
    }
}

bool GDB_ParseRecord(char *buf, size_t bufsize, ParseRecordContext &ctx)
{
    // parse async/sync record
    ctx = {};
    ctx.buf = buf;
    ctx.bufsize = bufsize;

    // get the record keyword, immediately after type prefix
    char *comma = (char *)memchr(buf, ',', bufsize);
    RecordAtom root = {};

    if (comma)
    {
        ctx.i = comma - buf;

        // this is a record with child elements
        // convert the root atom into an array
        char *last = buf + bufsize - 1;
        char prev_comma = *comma;
        char prev_eol = *last;
        *comma = '[';
        *last = ']';

        // scan the buffer to the the total amount of atoms
        size_t num_atoms_found = 0;
        for (size_t i = 0; i < ctx.bufsize; i++)
        {
            char n = (i + 1 < ctx.bufsize) ? ctx.buf[i + 1] : '\0';
            char c = ctx.buf[i];
            if ((c == '[' || c == '{') || 
                (c == '=' && n == '"') || 
                (c == '"' && n == ',') )
                num_atoms_found++;
        }

        // total=(name+value atoms) * num atoms
        ctx.atoms.resize(num_atoms_found * 2);

        root = RecurseRecord(ctx);

        // restore the modified chars
        *comma = prev_comma;
        *last = prev_eol;    // @@@ case where no comma is found problem

    }
    else
    {
        // this is a prefix-one word record i.e. ^done
        ctx.error = false;
        ctx.atoms.resize(1);    // root
    }

    if (!ctx.error)
    {
        // put the root in place since it doesn't get popped to the 
        // ordered section of the array
        ctx.num_end_atoms++;

        // atoms are in order at the end of the array,
        // subtract the difference from base ordered
        Assert(ctx.num_end_atoms <= ctx.atoms.size());
        size_t ordered_offset = ctx.atoms.size() - ctx.num_end_atoms;
        RecordAtom *ordered_base = &ctx.atoms[ ordered_offset ];
        ordered_base[0] = root;

        for (size_t i = 0; i < ctx.num_end_atoms; i++)
        {
            RecordAtom *iter = &ordered_base[i];
            if ( (iter->type == Atom_Array || iter->type == Atom_Struct) &&
                 (iter->value.length != 0))
            {
                // adjust aggregate offset
                Assert(iter->value.index > ordered_offset);
                iter->value.index -= ordered_offset;
            }
        }

        // num_end_atoms = num atoms
        auto begin = ctx.atoms.begin();
        ctx.atoms.erase(begin, begin + ctx.atoms.size() - ctx.num_end_atoms);
    }

    return !ctx.error;
}

bool GDB_Send(const char *cmd)
{
    bool result = false;
    size_t cmdsize = strlen(cmd);

    if (gdb.spawned_pid != 0 && (!prog.running || gdb.supports_async_execution) )
    {
        // write to GDB
        ssize_t written = write(gdb.fd_out_write, cmd, cmdsize);
        if (written != (ssize_t)cmdsize)
        {
            PrintErrorf("GDB_Send: %s\n", strerror(errno));
        }
        else
        {
            ssize_t newline_written = write(gdb.fd_out_write, "\n", 1);
            if (newline_written != 1)
            {
                PrintErrorf("GDB_Send: %s\n", strerror(errno));
            }
            else
            {
                result = true;
            }
        }
    }

    return result;
}

static size_t GDB_SendBlockingInternal(const char *cmd, bool remove_after)
{
    uint32_t this_record_id = gdb.record_id++;
    char fullrecord[8 * 1024];
    tsnprintf(fullrecord, "%u%s", this_record_id, cmd);
    size_t result = BAD_INDEX;

    if (GDB_Send(fullrecord))
    {
        bool found = false;
        do
        {
            timespec wait_for;
            clock_gettime(CLOCK_REALTIME, &wait_for);
            wait_for.tv_sec += 3;

            int rc = sem_timedwait(gdb.recv_block, &wait_for);
            if (rc < 0)
            {
                if (errno == ETIMEDOUT)
                {
                    // TODO: retry counts
                    PrintErrorf("Command Timeout: %s\n", cmd);
                }
                else
                {
                    PrintErrorf("sem_timedwait: %s\n", strerror(errno));
                }
                break;
            }
            else
            {
                GDB_GrabBlockData();

                // scan the lines for a result record, mark it as read
                // to prevent later processing
                for (size_t i = 0; i < prog.num_recs; i++)
                {
                    RecordHolder &iter = prog.read_recs[i];
                    const char *bufstr = iter.rec.buf.c_str();

                    if (!iter.parsed && iter.rec.id == this_record_id)
                    {
                        if (NULL != strstr(bufstr, "^error"))
                        {
                            if (NULL != strstr(bufstr, "optimized out"))
                            {
                                // @GDB: match up results for optimized out variables
                                // 1. -data-evaluate-expression argv --> ^done,value="<optimized out>"
                                // 2. -data-evaluate-expression argv[0] --> ^error,msg="value has been optimized out"

                                const Record OPTIMIZED_OUT_FIX = {
                                    this_record_id,
                                    {
                                        { Atom_Array, {0, 0}, {1,1} }, // root atom
                                        { Atom_String, /*value*/{6, 5}, /*<optimized out>*/{13, 15} }
                                    },
                                    "^done,value=\"<optimized out>\""
                                };

                                prog.num_recs++;
                                RecordHolder &last = prog.read_recs[ prog.num_recs - 1 ];
                                last.rec = OPTIMIZED_OUT_FIX;
                                last.parsed = false;
                            }
                            else
                            {
                                // convert error record to GDB console output record
                                String errmsg = GDB_ExtractValue("msg", iter.rec);
                                errmsg = "&\"GDB MI Error: " + errmsg + "\\n\"\n";
                                WriteToConsoleBuffer(errmsg.data(), errmsg.size());
                                iter.parsed = true;
                                return BAD_INDEX;
                            }
                        }
                        else
                        {
                            iter.parsed = remove_after;
                            result = i;
                            found = true;
                        }
                    }
                }
            }

        } while (!found);
    }

    return result;
}

bool GDB_SendBlocking(const char *cmd, bool remove_after)
{
    size_t index = GDB_SendBlockingInternal(cmd, remove_after);
    return (index < prog.read_recs.size());
}

bool GDB_SendBlocking(const char *cmd, Record &rec)
{
    // errno or result record index
    size_t index = GDB_SendBlockingInternal(cmd, false);
    bool result;

    if (index < prog.read_recs.size())
    {
        rec = prog.read_recs[index].rec;
        prog.read_recs[index].parsed = true;
        result = true;
    }
    else
    {
        rec = {};
        result = false;
    }

    return result;
}

static void GDB_ProcessBlock(char *block, size_t blocksize)
{
    // parse buffer of interpreter lines ending in (gdb)
    // sync/async records get stored 
    // console/debug logs get written to log_lines
    size_t block_idx = 0;
    while (block_idx < blocksize)
    {

        // parse the optional id preceding the record
        uint32_t this_record_id = 0;
        while (block_idx < blocksize)
        {
            char c = block[ block_idx ];
            if (c >= '0' && c <= '9')
            {
                this_record_id *= 10;
                this_record_id += (c - '0');
                block_idx++;
            }
            else
            {
                break;
            }
        }

        char *start = block + block_idx;
        char *eol = strchr(start, '\n');
        if (eol)
        {
            if (eol[-1] == '\r')
            {
                eol[-1] = ' ';
            }
            eol[0] = ' ';

            eol++; // GDB_ParseRecord inserts a ']' at the end
        }
        else
        {
            // all records should be NL terminated
            Assert(false);
            break;
        }

        size_t linesize = eol - start;
        WriteToConsoleBuffer(start, linesize);

        // get the record type
        char prefix = start[0];
        if (prefix == PREFIX_RESULT || prefix == PREFIX_ASYNC0 || prefix == PREFIX_ASYNC1) 
        {
            static ParseRecordContext ctx;
            if ( GDB_ParseRecord(start, linesize, ctx) )
            {
                // search for unused block before adding new one
                RecordHolder *out = NULL;
                for (size_t i = 0; i < prog.num_recs; i++)
                {
                    if (prog.read_recs[i].parsed)
                    {
                        // reused parsed elem in read records
                        out = &prog.read_recs[i];
                        break;
                    }
                }

                if (out == NULL)
                {
                    if (prog.read_recs.size() < prog.num_recs + 1)
                    {
                        size_t newcount = (prog.num_recs + 1) * 4;
                        prog.read_recs.resize(newcount);
                    }

                    out = &prog.read_recs[ prog.num_recs ];
                    prog.num_recs++;
                }

                *out = {};
                out->parsed = false;

                Record &rec = out->rec;
                rec.atoms = ctx.atoms;
                rec.buf.resize(linesize);
                rec.id = this_record_id;
                memcpy(const_cast<char*>(rec.buf.data()), start, linesize);

                // resolve literal within strings
                // ignore those of name "value" because these get handled in RecurseEvaluation
                const auto RemoveStringBackslashes = [](Record &record, RecordAtom &iter, void * /* user context */)
                {
                    if (iter.type == Atom_String)
                    {
                        size_t new_length = iter.value.length;
                        for (size_t i = 0; i < iter.value.length; i++)
                        {
                            size_t buf_idx = iter.value.index + i;
                            char c = record.buf[buf_idx];
                            char n = (i + 1 < iter.value.length) ? record.buf[buf_idx + 1] : '\0';
                            if (c == '\\' && (n == '\\' || n == '\"'))
                            {
                                //record.buf[iter.value.index + new_length - 1] = ' ';
                                memmove(&record.buf[buf_idx], &record.buf[buf_idx + 1], iter.value.length - (i + 1));
                                new_length--;
                            }
                        }
                        iter.value.length = new_length;
                    }      
                };

                if (rec.atoms.size() > 1)
                    IterateAtoms(rec, rec.atoms[0], RemoveStringBackslashes, NULL);

                // @Debug
                //GDB_PrintRecordAtom(rec, rec.atoms[0], 0);
            }
        }

        // advance to next line, skip over index of NT
        block_idx = block_idx + linesize;
    }
}

void GDB_GrabBlockData()
{
    pthread_mutex_lock(&gdb.modify_block);

    for (size_t i = 0; i < gdb.num_blocks; i++)
    {
        Span &iter = gdb.block_spans[i];
        GDB_ProcessBlock(gdb.block_data + iter.index, iter.length);
        iter = {};
    }
    gdb.num_blocks = 0;

    pthread_mutex_unlock(&gdb.modify_block);
}
