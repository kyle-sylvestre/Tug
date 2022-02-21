#include "gdb_common.h"

//
// yoinked from glfw_example_opengl2
//

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl2.h>
#include <GLFW/glfw3.h>

#include "imgui_file_window.h"

struct GlfwInput
{
    uint8_t action;
    uint8_t mods;
};

struct GlfwKeyboardState
{
    GlfwInput keys[ GLFW_KEY_LAST + 1 ];
};

struct GlfwMouseState
{
    GlfwInput buttons[ GLFW_MOUSE_BUTTON_LAST + 1 ];
};

struct GuiContext
{
    GlfwKeyboardState this_keystate;
    GlfwKeyboardState last_keystate;
    GlfwMouseState this_mousestate;
    GlfwMouseState last_mousestate;

    size_t source_highlighted_line = BAD_INDEX;
    bool source_search_bar_open = false;
    char source_search_keyword[256];
    size_t source_found_line_idx;

    bool IsKeyClicked(int glfw_key, int glfw_mods = 0)
    {
        GlfwInput &this_key = Key(glfw_key);
        GlfwInput &last_key = LastKey(glfw_key);
        bool result = last_key.action == GLFW_RELEASE && 
                      this_key.action == GLFW_PRESS;

        if (glfw_mods != 0)
        {
            result &= (glfw_mods == (this_key.mods & glfw_mods));
        }
        return result;
    }
    bool IsMouseClicked(int glfw_mouse_button, int glfw_mods = 0)
    {
        GlfwInput &this_button = this_mousestate.buttons[glfw_mouse_button];
        GlfwInput &last_button = last_mousestate.buttons[glfw_mouse_button];
        bool result = this_button.action == GLFW_RELEASE && 
                      last_button.action == GLFW_PRESS;

        if (glfw_mods != 0)
        {
            result &= (glfw_mods == (this_button.mods & glfw_mods));
        }
        return result;
    }
    GlfwInput &Key(int glfw_key)
    {
        return this_keystate.keys[glfw_key];
    }
    GlfwInput &LastKey(int glfw_key)
    {
        return last_keystate.keys[glfw_key];
    }
};

ProgramContext prog;
GDB gdb;
GuiContext gui;

void GDB_LogLine(const char *raw, size_t rawsize);

inline void dbg() {};

void *RedirectStdin(void *ctx)
{
    // forward user input to GDB
    while (!gdb.end_program)
    {
        char c = fgetc(stdin);
        ssize_t num_written = write(gdb.fd_out_write, &c, 1);
        if (num_written < 0)
            PrintErrorLibC("console write to stdin");
    }
    return NULL;
}

void *ReadInterpreterBlocks(void *ctx)
{
    // read data from GDB pipe
    char tmp[1024 * 1024 + 1] = {};
    size_t insert_idx = 0;
    gdb.blocks.resize(16 * 1024);

    while (true)
    {
        ssize_t num_read = read(gdb.fd_in_read, tmp + insert_idx,
                                sizeof(tmp) - insert_idx - 1 /*NT*/);
        if (num_read <= 0)
            break;

        tmp[insert_idx + num_read] = '\0';
        printf("%.*s\n\n", int(num_read), tmp + insert_idx);
        insert_idx += num_read;

        const char *sig = strstr(tmp, RECORD_ENDSIG);
        if (sig) 
        {
            //printf("--- GDB %d ---\n", gdb.num_blocks++);
            pthread_mutex_lock(&gdb.modify_block);
            do 
            {
                if (gdb.num_blocks + 1 > ArrayCount(gdb.block_spans))
                    return NULL; // exhausted block space memory

                // set to the last inserted to get bufsize
                Span span;
                if (gdb.num_blocks == 0) span = {};
                else span = gdb.block_spans[gdb.num_blocks - 1];

                span.index += span.length; 
                span.length = (sig - tmp);
                size_t total_bytes = span.index + span.length;
                if (gdb.blocks.size() < total_bytes) 
                    gdb.blocks.resize(total_bytes);

                memcpy(gdb.blocks.data() + span.index, tmp, span.length);
                gdb.block_spans[ gdb.num_blocks ] = span;
                gdb.num_blocks++;

                // advance record blocks, skip over trailing whitespace
                const char *next = sig + RECORD_ENDSIG_SIZE;
                while ( size_t(next - tmp) < insert_idx && 
                        (*next == ' ' || *next == '\n') )
                {
                    next++;
                }

                size_t fullsize = next - tmp;
                memset(tmp, 0, fullsize);
                memmove(tmp, next, insert_idx - fullsize);
                insert_idx -= fullsize;
                tmp[insert_idx] = '\0';

            } while ( (sig = strstr(tmp, RECORD_ENDSIG)) );

            // post a change if the binary semaphore is zero
            static int semvalue; 
            sem_getvalue(gdb.recv_block, &semvalue);
            if (semvalue == 0) 
                sem_post(gdb.recv_block);

            pthread_mutex_unlock(&gdb.modify_block);
        }
    }

    Print("closing GDB interpreter read loop...\n");
    return NULL;
}

int GDB_Shutdown()
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
    return 0;
}

void GDB_ProcessBlock(char *block, size_t blocksize)
{
    // parse buffer of interpreter lines ending in (gdb)
    // sync/async records get stored 
    // console/debug logs get written to log_lines
    size_t block_idx = 0;
    while (block_idx < blocksize)
    {
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
        GDB_LogLine(start, eol - start);

        // get the record type
        char c = start[0];
        if (c == PREFIX_RESULT || c == PREFIX_ASYNC0 || c == PREFIX_ASYNC1) 
        {
            static ParseRecordContext ctx;
            if ( GDB_ParseRecord(start, eol - start, ctx) )
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
                rec.buf.resize(eol - start);
                memcpy(rec.buf.data(), start, eol - start);
            }
        }

        // advance to next line, skip over index of NT
        block_idx = block_idx + linesize;
    }
}

int GDB_Init(GLFWwindow *window)
{
    int rc = 0;

    do
    {

        // read in configuration file
        {
            std::string line;
            std::ifstream file(TUG_CONFIG_FILENAME, std::ios::in);
            while (std::getline(file, line))
            {
                size_t commentoff = line.find('#');
                if (commentoff != std::string::npos)
                    line.erase(commentoff);

                if (line.size() > 0 && line[ line.size() - 1 ] == '\r')
                    line.pop_back();

                size_t equaloff = line.find('=');
                if (equaloff != std::string::npos && equaloff + 1 < line.size())
                {
                    std::string key = line.substr(0, equaloff);
                    std::string value = line.substr(equaloff + 1);

                    #define KEYCONF(keyname) if (key == #keyname) prog.config_##keyname = value.c_str();

                    KEYCONF(gdb_path)
                    KEYCONF(debug_exe)
                    KEYCONF(debug_exe_args)
                    KEYCONF(font_filename)
                    KEYCONF(font_size)

                    #undef KEYCONF
                }
            }

            file.close();
        }

        int pipes[2] = {};
        rc = pipe(pipes);
        if (rc < 0)
        {
            PrintErrorLibC("from gdb pipe");
            break;
        }

        gdb.fd_in_read = pipes[0];
        gdb.fd_in_write = pipes[1];

        rc = pipe(pipes);
        if (rc < 0)
        {
            PrintErrorLibC("to gdb pipe");
            break;
        }

        gdb.fd_out_read = pipes[0];
        gdb.fd_out_write = pipes[1];

        rc = pthread_mutex_init(&gdb.modify_block, NULL);
        if (rc < 0) 
        {
            PrintErrorLibC("pthread_mutex_init");
            break;
        }

        gdb.recv_block = sem_open("recv_gdb_block", O_CREAT, S_IRWXU, 0);
        if (gdb.recv_block == NULL) 
        {
            PrintErrorLibC("sem_open");
            break;
        }

        //if (0 > (gdb.fd_pty_master = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK)))
        //{
        //    PrintErrorLibC("posix_openpt");
        //    break;
        //}

        //if (0 > (rc = grantpt(gdb.fd_pty_master)) )
        //{
        //    PrintErrorLibC("grantpt");
        //    break;
        //}
        //if (0 > (rc = unlockpt(gdb.fd_pty_master)) )
        //{
        //    PrintErrorLibC("grantpt");
        //    break;
        //}
        //printf("pty slave: %s\n", ptsname(gdb.fd_pty_master));



        //rc = chdir("/mnt/c/Users/Kyle/Downloads/Chrome Downloads/ARM/AARCH32");
        //if (rc < 0)
        //{
        //    PrintErrorLibC("chdir");
        //    break;
        //}

        if (prog.config_gdb_path != "")
        {
#if 0 // old way, can't get gdb process PID this way
            dup2(gdb.fd_out_read, 0);           // stdin
            dup2(gdb.fd_in_write, 1);        // stdout
            dup2(gdb.fd_in_write, 2);        // stderr

            close(gdb.fd_in_write);

            //rc = execl("/usr/bin/gdb", "gdb", "--interpreter=mi", NULL);
            rc = execl("/usr/bin/gdb-multiarch", "gdb-multiarch", "./advent.out", "--interpreter=mi", NULL);
            VerifyCond(rc == 0);
#endif
            // TODO: config for MI version used
            String args = prog.config_gdb_path + " " + prog.config_gdb_args + " --interpreter=mi "; 

            dbg();
            std::string buf;
            size_t startoff = 0;
            size_t spaceoff = 0;
            size_t bufoff = 0;
            std::vector<char *> argv;

    dbg();
            while ( std::string::npos != (spaceoff = args.find(' ', startoff)) )
            {
                size_t arglen = spaceoff - startoff;
                if (arglen != 0)
                {
                    argv.push_back((char *)bufoff);
                    buf.insert(buf.size(), &args[ startoff ], arglen);
                    buf.push_back('\0');
                    bufoff += (arglen + 1);
                }
                startoff = spaceoff + 1;
            }
            
            // convert base offsets to char pointers
            for (size_t i = 0; i < argv.size(); i++)
            {
                argv[i] = (char *)((size_t)argv[i] + buf.data());
            }
            argv.push_back(NULL);

            posix_spawn_file_actions_t actions = {};
            posix_spawn_file_actions_adddup2(&actions, gdb.fd_out_read,  0);    // stdin
            posix_spawn_file_actions_adddup2(&actions, gdb.fd_in_write, 1);     // stdout
            posix_spawn_file_actions_adddup2(&actions, gdb.fd_in_write, 2);     // stderr

            posix_spawnattr_t attrs = {};
            posix_spawnattr_init(&attrs);
            posix_spawnattr_setflags(&attrs, POSIX_SPAWN_SETSID);

            String env;
            FILE *fsh = popen("printenv", "r");
            if (fsh == NULL)
            {
                PrintErrorLibC("getenv popen");
                break;
            }

            char tmp[1024];
            ssize_t tmpread;
            while (0 < (tmpread = fread(tmp, 1, sizeof(tmp), fsh)) )
            {
                env.insert(env.size(), tmp, tmpread);
            }

            Vector<char *> envptr;
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

            pclose(fsh);

            rc = posix_spawnp((pid_t *) &gdb.spawned_pid, prog.config_gdb_path.c_str(),
                              &actions, &attrs, argv.data(), envptr.data());
            if (rc < 0 || gdb.spawned_pid == 0) 
            {
                PrintErrorLibC("posix_spawnp");
                break;
            }
        }
        
        rc = pthread_create(&gdb.thread_read_interp, NULL, ReadInterpreterBlocks, (void*) NULL);
        if (rc < 0) 
        {
            PrintErrorLibC("pthread_create");
            break;
        }

        // set initial state of log
        char empty[] = ""; 
        for (int i = 0; i < NUM_LOG_ROWS; i++)
            GDB_LogLine(empty, 0);

        Record tmp = {};
        FileContext file_ctx = {};

        // wait for the prompt messages to show up
        // clear the first (gdb) semaphore

#if 1
        //GDB_SendBlocking("-environment-cd /mnt/c/Users/Kyle/Documents/commercial-codebases/original/stevie");
        //GDB_SendBlocking("-file-exec-and-symbols stevie");
        //GDB_SendBlocking("-exec-arguments stevie.c");

        // debug GAS
        //GDB_SendBlocking("-environment-cd /mnt/c/Users/Kyle/Documents/commercial-codebases/original/binutils/binutils-gdb/gas");
        //GDB_SendBlocking("-file-exec-and-symbols as-new");

        //GDB_SendBlocking("-environment-cd \"/mnt/c/Users/Kyle/Downloads/Chrome Downloads/ARM/AARCH32\"");
        //GDB_SendBlocking("-file-exec-and-symbols advent.out");

        if (prog.config_debug_exe != "")
        {
            char tmp[1024];
            tsnprintf(tmp, "-file-exec-and-symbols \"%s\"", 
                      prog.config_debug_exe.c_str());
            GDB_SendBlocking(tmp);
        }

        // preload all the referenced files
        //GDB_SendBlocking("-file-list-exec-source-files", tmp, "files");
        //const RecordAtom *files = GDB_ExtractAtom("files", tmp);
        //for (const RecordAtom &file : GDB_IterChild(tmp, *files))
        //{
        //    String fullpath = GDB_ExtractValue("fullname", file, tmp);
        //    FileContext::Create(fullpath.c_str(), file_ctx);
        //    prog.files.emplace_back(file_ctx);
        //}

#endif

        // setup global var objects that will last the duration of the program
        String reg_cmd;
        if (0) for (const char *reg : REG_AMD64)
        {
            char name[256];
            char cmd[512];
            tsnprintf(name, GLOBAL_NAME_PREFIX "%s", reg);
            tsnprintf(cmd, "-var-create %s @ $%s\n", name, reg);
            reg_cmd += cmd;

            prog.global_vars.push_back( { reg, "", false} );
        }

        reg_cmd.pop_back(); // last \n
        GDB_Send(reg_cmd.c_str());

    } while(0);

    const auto UpdateKey = [](GLFWwindow *window, int key, int scancode,
                              int action, int mods) -> void
    {
        Assert((size_t)key < ArrayCount(gui.this_keystate.keys));
        Assert((uint8_t)action <= UINT8_MAX && (uint8_t)mods <= UINT8_MAX);
        //printf("key pressed: %s, glfw_macro: %d", name, key);

        GlfwInput &update = gui.this_keystate.keys[key];
        update.action = action;
        update.mods = mods;
    };

    const auto UpdateMouseButton = [](GLFWwindow *window, int button, 
                                      int action, int mods)
    {
        Assert((size_t)button < ArrayCount(gui.this_mousestate.buttons));
        Assert((uint8_t)action <= UINT8_MAX && (uint8_t)mods <= UINT8_MAX);
        //printf("key pressed: %s, glfw_macro: %d", name, key);

        GlfwInput &update = gui.this_mousestate.buttons[button];
        update.action = action;
        update.mods = mods;
    };

    glfwSetKeyCallback(window, UpdateKey);
    glfwSetMouseButtonCallback(window, UpdateMouseButton);

    return rc;
}

int _main(int argc, char **argv)
{
    return 0;
}


#define ImGuiDisabled(is_disabled, code)\
ImGui::BeginDisabled(is_disabled);\
code;\
ImGui::EndDisabled();

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 640
#define SOURCE_WIDTH 800

static void DrawHelpMarker(const char *desc)
{
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void GDB_LogLine(const char *raw, size_t rawsize)
{
    // debug
    //printf("%.*s\n", int(rawsize), raw);

#if 0   // disable line formatting
    for (size_t i = 0; i < rawsize; i++)
    {
        char c = raw[i];
        if (c == '\\' && i + 2 <= rawsize)
        {
            // convert to literal, shift array up and decrease size
            char n = raw[i + 1];

            if (n == 'n') raw[i] = ' '; // skip over sent newlines
            else if (n == '\\') raw[i] = '\\';
            else if (n == '"') raw[i] = '"';

            memmove(raw + i + 1,
                    raw + i + 2,
                    rawsize - (i + 2));
            rawsize--;
        }
    }
#endif

    // shift lines up, add to collection
    prog.log[NUM_LOG_ROWS - 1][NUM_LOG_COLS] = '\n'; // clear prev null terminator
    memcpy(&prog.log[0][0], &prog.log[1][0], sizeof(prog.log) - sizeof(prog.log[0]));

    // truncate line to NUM_LOG_COLS and omit the last " and \n
    char *dest = &prog.log[NUM_LOG_ROWS - 1][0];
    memset(dest, ' ', NUM_LOG_COLS);
    dest[NUM_LOG_COLS] = '\0';
    memcpy(dest, raw, (rawsize > NUM_LOG_COLS) ? NUM_LOG_COLS : rawsize);
}

ssize_t GDB_Send(const char *cmd)
{
    if (*cmd == '\0') return 0; // special case: don't send but dec semaphore


    // write to GDB
    size_t cmdsize = strlen(cmd);

    // DEBUG: log all outgoing traffic
    GDB_LogLine(cmd, cmdsize);

    ssize_t written = write(gdb.fd_out_write, cmd, cmdsize);
    if (written != (ssize_t)cmdsize)
    {
        PrintErrorLibC("GDB_Send");
    }
    else
    {
        ssize_t newline_written = write(gdb.fd_out_write, "\n", 1);
        if (newline_written != 1)
        {
            PrintErrorLibC("GDB_Send");
        }
    }

    return written;
}

void GDB_GrabBlockData()
{
    pthread_mutex_lock(&gdb.modify_block);

    size_t total_bytes = 0;
    for (size_t i = 0; i < gdb.num_blocks; i++)
    {
        Span &iter = gdb.block_spans[i];
        GDB_ProcessBlock(gdb.blocks.data() + iter.index, iter.length);
        total_bytes = iter.index + iter.length;
        iter = {};
    }
    memset(gdb.blocks.data(), 0, total_bytes);
    gdb.num_blocks = 0;

    pthread_mutex_unlock(&gdb.modify_block);
}

int GDB_SendBlocking(const char *cmd, const char *header, bool remove_after)
{
    int rc;
    ssize_t num_sent = GDB_Send(cmd);

    if (num_sent >= 0)
    {
        bool found = false;
        do
        {
            timespec wait_for;
            clock_gettime(CLOCK_REALTIME, &wait_for);
            wait_for.tv_sec += 1;

            rc = sem_timedwait(gdb.recv_block, &wait_for);
            if (rc < 0)
            {
                if (errno == ETIMEDOUT)
                {
                    // TODO: retry counts
                    char buf[512];
                    size_t bufsize = tsnprintf(buf, "Command Timeout: %s", cmd);
                    GDB_LogLine(buf, bufsize);
                }
                else
                {
                    PrintErrorLibC("sem_timedwait");
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
                    auto &recbuf = iter.rec.buf;

                    if (!iter.parsed && iter.rec.buf.size() > 0)
                    {
                        // cap for strstr
                        char &last = recbuf[ recbuf.size() - 1 ];
                        char c = last;
                        if (NULL != strstr(iter.rec.buf.data(), header))
                        {
                            iter.parsed = remove_after;
                            rc = i;
                            found = true;
                        }
                        else if (NULL != strstr(iter.rec.buf.data(), "^error"))
                        {
                            char msg[] = "---Error---";
                            GDB_LogLine(msg, strlen(msg));
                            iter.parsed = true;
                            return -1;
                        }

                        last = c;
                    }
                }
            }

        } while (!found);
    }

    return rc;
}

int GDB_SendBlocking(const char *cmd, Record &rec, const char *end_pattern)
{
    // errno or result record index
    int rc_or_index = GDB_SendBlocking(cmd, end_pattern, false);
    if (rc_or_index < 0)
    {
        rec = {};
    }
    else
    {
        rec = prog.read_recs[rc_or_index].rec;
        prog.read_recs[rc_or_index].parsed = true;
    }

    return rc_or_index;
}

size_t GDB_CreateOrGetFile(const String &fullpath)
{
    size_t result = BAD_INDEX;

    // search for the stored file context
    for (size_t i = 0; i < prog.files.size(); i++)
    {
        if (prog.files[i].fullpath == fullpath)
        {
            result = i;
            break;
        }
    }

    // file context not found, create it 
    if (result == BAD_INDEX)
    {
        result = prog.files.size();
        prog.files.resize( prog.files.size() + 1);
        FileContext::Create(fullpath.c_str(), 
                            prog.files[ result ]);
    }

    return result;
}
 
void GDB_Draw(GLFWwindow *window)
{
    // process async events
    static Record rec;  // scratch record 
    char tmpbuf[4096];  // scratch for snprintf

    // check for new blocks
    int recv_block_semvalue;
    sem_getvalue(gdb.recv_block, &recv_block_semvalue);
    if (recv_block_semvalue > 0)
    {
        GDB_GrabBlockData();
        sem_wait(gdb.recv_block);
    }

    // process and clear all records found
    bool async_stopped = false;
    bool remake_callstack = true;

    size_t last_num_recs = prog.num_recs;
    prog.num_recs = 0;
    for (size_t i = 0; i < last_num_recs; i++)
    {
        RecordHolder &iter = prog.read_recs[i];
        if (!iter.parsed)
        {
            Record &rec = iter.rec;
            iter.parsed = true;
            char prefix = rec.buf[0];

            char *comma = (char *)memchr(rec.buf.data(), ',', rec.buf.size());
            if (comma == NULL) 
                comma = &rec.buf[ rec.buf.size() ];

            char *start = rec.buf.data() + 1;
            String prefix_word(start, comma - start);

            if (prefix == PREFIX_ASYNC0)
            {
                if (prefix_word == "breakpoint-created")
                {
                    Breakpoint res = {};
                    res.number = GDB_ExtractInt("bkpt.number", rec);
                    res.line = GDB_ExtractInt("bkpt.line", rec);

                    String fullpath = GDB_ExtractValue("bkpt.fullname", rec);
                    res.file_idx = GDB_CreateOrGetFile(fullpath);

                    prog.breakpoints.push_back(res);
                }
                else if (prefix_word == "breakpoint-deleted")
                {
                    size_t id = (size_t)GDB_ExtractInt("id", rec);
                    auto &bpts = prog.breakpoints;
                    for (size_t i = 0; i < bpts.size(); i++)
                    {
                        if (bpts[i].number == id)
                        {
                            bpts.erase(bpts.begin() + i, 
                                       bpts.begin() + i + 1);
                            break;
                        }
                    }
                }
                else if (prefix_word == "thread-group-started")
                {
                    prog.inferior_process = (pid_t)GDB_ExtractInt("pid", rec);
                }
            }
            else if (prefix_word == "stopped")
            {
                prog.running = false;
                async_stopped = true;
                String reason = GDB_ExtractValue("reason", rec);

                if ( (NULL != strstr(reason.c_str(), "exited")) )
                {
                    prog.started = false;
                    prog.frames.clear();
                    // TODO: delete local vars
                }
                else
                {
                    // check if we have entered a new function frame
                    // this is done to prevent unnecessary varobj creations/deletions
                }
            }
        }
    }

    if (async_stopped)
    {
        bool remake_varobjs = false;
        if (remake_callstack)
        {
            // TODO: remote ARM32 debugging is this up after jsr macro
            //GDB_SendBlocking("-stack-list-frames 0 0", rec, "stack");
            GDB_SendBlocking("-stack-list-frames", rec, "stack");
            const RecordAtom *callstack = GDB_ExtractAtom("stack", rec);
            if (callstack)
            {
                prog.frames.clear();
                for (const RecordAtom &level : GDB_IterChild(rec, *callstack))
                {

                    Frame add = {};
                    add.line = (size_t)GDB_ExtractInt("line", level, rec);
                    add.addr = GDB_ExtractValue("addr", level, rec);
                    add.func = GDB_ExtractValue("func", level, rec);

                    String fullpath = GDB_ExtractValue("fullname", level, rec);
                    add.file_idx = GDB_CreateOrGetFile(fullpath);

                    prog.frames.emplace_back(add);
                }

                prog.frame_idx = 0;

                // make a unique signature from the function name and types
                // if the signature has changed, need to delete/recreate varobj's
                static String last_funcsig;
                String this_funcsig = (prog.frames.size() == 0) ? "" : prog.frames[0].func;
                GDB_SendBlocking("-stack-list-arguments --simple-values 0 0", rec);
                const RecordAtom *args = GDB_ExtractAtom("stack-args[0].args", rec);

                for (const RecordAtom &arg : GDB_IterChild(rec, *args))
                {
                    this_funcsig += GDB_ExtractValue("type", arg, rec);
                }
                this_funcsig += std::to_string( prog.frames.size() ).c_str();

                if (last_funcsig != this_funcsig)
                {
                    last_funcsig = this_funcsig;
                    remake_varobjs = true;
                }
            }

            if (remake_varobjs)
            {
                // new stack frame, delete all locals
                for (VarObj &iter : prog.local_vars)
                {
                    tsnprintf(tmpbuf, "-var-delete " LOCAL_NAME_PREFIX "%s", 
                              iter.name.c_str());
                    GDB_SendBlocking(tmpbuf);
                }
                prog.local_vars.clear();

                // get current frame values 
                GDB_SendBlocking("-stack-list-variables 0", rec, "variables");

                const RecordAtom *vars = GDB_ExtractAtom("variables", rec);
                if (vars)
                {
                    Vector<String> stack_vars;
                    for (const RecordAtom &child : GDB_IterChild(rec, *vars))
                    {
                        String name = GDB_ExtractValue("name", child, rec); 
                        stack_vars.push_back(name);
                    }

                    // create MI variable objects for each stack variable
                    for (String &iter : stack_vars)
                    {
                        tsnprintf(tmpbuf, "-var-create " LOCAL_NAME_PREFIX "%s * %s",
                                  iter.c_str(), iter.c_str());
                        GDB_SendBlocking(tmpbuf, rec);

                        String value = GDB_ExtractValue("value", rec);
                        if (value == "") value = "???";

                        VarObj insert = { iter, value, true };
                        prog.local_vars.emplace_back(insert);
                    }
                }
            }
        }

        // update local values
        GDB_SendBlocking("-var-update --all-values *", rec, "changelist");
        const RecordAtom *changelist = GDB_ExtractAtom("changelist", rec);
        if (changelist)
        {
            // clear all the old varobj changed flags
            // newly created local variables will be display changed
            if (!remake_varobjs) for (auto &iter : prog.local_vars) iter.changed = false;
            for (auto &iter : prog.global_vars) iter.changed = false;

            for (const RecordAtom &iter : GDB_IterChild(rec, *changelist))
            {
                String name = GDB_ExtractValue("name", iter, rec);
                String value = GDB_ExtractValue("value", iter, rec);
                if (value == "") value = "???";

                const char *srcname = name.c_str();
                const char *namestart = NULL;
                if ( (namestart = strstr(srcname, GLOBAL_NAME_PREFIX)) )
                {
                    // check for global variable change 
                    namestart += strlen(GLOBAL_NAME_PREFIX);
                    for (VarObj &iter : prog.global_vars)
                    {
                        if (iter.name == namestart)
                        {
                            iter.changed = (iter.value != value);
                            iter.value = value;
                            break;
                        }
                    }
                }
                else if ( (namestart = strstr(srcname, LOCAL_NAME_PREFIX)) )
                {
                    // check for local variable change 
                    namestart += strlen(LOCAL_NAME_PREFIX);
                    for (VarObj &iter : prog.local_vars)
                    {
                        if (iter.name == namestart)
                        {
                            iter.changed = (iter.value != value);
                            iter.value = value;
                            break;
                        }
                    }
                }
            }
        }
    }

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoCollapse | 
                                    ImGuiWindowFlags_NoBringToFrontOnFocus;

    // code window, see current instruction and set breakpoints
    {
        ImGui::SetNextWindowBgAlpha(1.0);   // @Imgui: bug where GetStyleColor doesn't respect window opacity
        ImGui::SetNextWindowPos( { 0, 0 } );
        ImGui::SetNextWindowSize({ SOURCE_WIDTH, WINDOW_HEIGHT });
        ImGui::Begin("Source", NULL, window_flags);      // Create a window called "Hello, world!" and append into it.

        if ( gui.IsKeyClicked(GLFW_KEY_F, GLFW_MOD_CONTROL) )
        {
            gui.source_search_bar_open = true;
            ImGui::SetKeyboardFocusHere(0); // auto click the input box
            gui.source_search_keyword[0] = '\0';
        }
        else if (gui.source_search_bar_open && gui.IsKeyClicked(GLFW_KEY_ESCAPE))
        {
            gui.source_search_bar_open = false;
        }

        // @Imgui: is there another way to make a fixed position widget
        //         without making a child window
        if (gui.source_search_bar_open)
        {
            bool source_open = prog.frame_idx < prog.frames.size() &&
                               prog.frames[ prog.frame_idx ].file_idx < prog.files.size();
            ImGui::InputText("##source_search",
                             gui.source_search_keyword, 
                             sizeof(gui.source_search_keyword));
            if (source_open)
            {
                size_t dir = 1;
                size_t &this_frame_idx = prog.frames[ prog.frame_idx ].file_idx; 
                FileContext &this_file = prog.files[ this_frame_idx ];
                bool found = false;

                if ( gui.IsKeyClicked(GLFW_KEY_N) &&
                     !ImGui::GetIO().WantCaptureKeyboard)
                {
                    // N = search forward
                    // Shift N = search backward
                    bool shift_down = ( 0 != (gui.Key(GLFW_KEY_N).mods & GLFW_MOD_SHIFT) );
                    dir = (shift_down) ? -1 : 1;

                    // advance search by skipping the previous match
                    gui.source_found_line_idx += dir;
                    size_t linesize = this_file.lines.size();
                    if (gui.source_found_line_idx > linesize)   // wrap around
                        gui.source_found_line_idx = linesize - 1;

                }

                // search for a keyword starting at the last found index + 1
                bool wraparound = false;
                for (size_t i = gui.source_found_line_idx; 
                     i < this_file.lines.size(); i += dir)
                {
                    String &line = this_file.lines[i];
                    if ( NULL != strstr(line.c_str(), gui.source_search_keyword) )
                    {
                        gui.source_found_line_idx = i;
                        found = true;
                        break;
                    }

                    if (!wraparound && i + dir >= this_file.lines.size())
                    {
                        // continue searching at the other end of the array
                        i = (dir == 1) ? 0 : this_file.lines.size() - 1;
                        wraparound = true;
                    }
                }
                if (!found) gui.source_found_line_idx = 0;
            }

            ImGui::Separator();
            ImGui::BeginChild("SourceScroll");
        }

        if (prog.frame_idx < prog.frames.size() &&
            prog.frames[ prog.frame_idx ].file_idx < prog.files.size())
        {
            Frame &frame = prog.frames[ prog.frame_idx ];
            FileContext &file = prog.files[ frame.file_idx ];

            // display file lines, skip over designated blank zero index to sync
            // up line indices with line numbers
            for (size_t i = 1; i < file.lines.size(); i++)
            {
                String &line = file.lines[i];

                bool is_set = false;
                for (Breakpoint &iter : prog.breakpoints)
                    if (iter.line == i && iter.file_idx == frame.file_idx)
                        is_set = true;

                // start radio button style
                ImColor window_bg_color = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
                ImColor bkpt_active_color = IM_COL32(255, 64, 64, 255);
                ImColor bkpt_hovered_color = {};

                const float inc = 32 / 255.0f;
                bkpt_hovered_color.Value.x = window_bg_color.Value.x + inc;
                bkpt_hovered_color.Value.y = window_bg_color.Value.y + inc;
                bkpt_hovered_color.Value.z = window_bg_color.Value.z + inc;
                bkpt_hovered_color.Value.w = window_bg_color.Value.w;

                ImGui::PushStyleColor(ImGuiCol_FrameBg, 
                                      window_bg_color.Value);
                ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, 
                                      bkpt_hovered_color.Value);

                // color while pressing mouse left on button
                ImGui::PushStyleColor(ImGuiCol_FrameBgActive, 
                                      bkpt_active_color.Value);    
                
                // color while active
                ImGui::PushStyleColor(ImGuiCol_CheckMark, 
                                      bkpt_active_color.Value);        

                tsnprintf(tmpbuf, "##bkpt%d", (int)i); 
                if (ImGui::RadioButton(tmpbuf, is_set))
                {
                    // dispatch command to set breakpoint
                    if (is_set)
                    {
                        for (size_t b = 0; b < prog.breakpoints.size(); b++)
                        {
                            Breakpoint &iter = prog.breakpoints[b];
                            if (iter.line == i && iter.file_idx == frame.file_idx)
                            {
                                // remove breakpoint
                                tsnprintf(tmpbuf, "-break-delete %zu", iter.number);
                                GDB_SendBlocking(tmpbuf);
                                prog.breakpoints.erase(prog.breakpoints.begin() + b,
                                                       prog.breakpoints.begin() + b + 1);
                                break;
                            }
                        }
                    }
                    else
                    {
                        // insert breakpoint
                        tsnprintf(tmpbuf, "-break-insert --source \"%s\" --line %d", 
                                  file.fullpath.c_str(), (int)i);
                        GDB_SendBlocking(tmpbuf, rec);

                        Breakpoint res = {};
                        res.number = GDB_ExtractInt("bkpt.number", rec);
                        res.line = GDB_ExtractInt("bkpt.line", rec);

                        String fullpath = GDB_ExtractValue("bkpt.fullname", rec);
                        res.file_idx = GDB_CreateOrGetFile(fullpath);

                        prog.breakpoints.push_back(res);
                    }
                }

                // stop radio button style
                ImGui::PopStyleColor(4);

                // automatically scroll to the next executed line
                int linediff = abs((long long)gui.source_highlighted_line - (long long)frame.line);
                bool highlight_search_found = false;
                if (i == gui.source_found_line_idx)
                {
                    if (gui.source_search_bar_open)
                    {
                        highlight_search_found = true;
                    }
                    else
                    {
                        // we closed the window and no longer in child window,
                        // retain the search index found
                        gui.source_found_line_idx = 0;
                    }
                    ImGui::SetScrollHereY();
                }
                else if (linediff > 10 && i == frame.line)
                {
                    gui.source_highlighted_line = frame.line;
                    ImGui::SetScrollHereY();
                }

                ImGui::SameLine();
                ImVec2 textstart = ImGui::GetCursorPos();
                if (i == frame.line)
                {
                    ImGui::Selectable(line.c_str(), true);
                }
                else
                {
                    if (highlight_search_found)
                    {
                        ImColor IM_COL_YELLOW = IM_COL32(255, 255, 0, 255);
                        ImGui::TextColored(IM_COL_YELLOW, "%s", line.c_str());
                    }
                    else
                    {
                        ImGui::Text("%s", line.c_str());
                    }
                }

                if (ImGui::IsItemHovered())
                {
                    // convert absolute mouse to window relative position
                    ImVec2 relpos = {};
                    relpos.x = ImGui::GetMousePos().x - ImGui::GetWindowPos().x;
                    relpos.y = ImGui::GetMousePos().y - ImGui::GetWindowPos().y;

                    // enumerate words of the line
                    size_t word_idx = BAD_INDEX;
                    size_t delim_idx = BAD_INDEX;
                    for (size_t char_idx = 0; char_idx < line.size(); char_idx++)
                    {
                        char c = line[char_idx];
                        bool is_ident = (c >= 'a' && c <= 'z') || 
                                        (c >= 'A' && c <= 'Z') ||
                                        (word_idx != BAD_INDEX && (c >= '0' && c <= '9')) ||
                                        (c == '_');

                        if (char_idx == line.size() - 1 && is_ident)
                        {
                            // force a word on the last index
                            if (word_idx == BAD_INDEX)
                                word_idx = char_idx;

                            char_idx++;
                            is_ident = false;
                        }

                        if (!is_ident)
                        {
                            bool not_delim_struct = true;
                            if (word_idx != BAD_INDEX)
                            {
                                // we got a word delimited by spaces
                                // calculate size and see if mouse is over it 
                                ImVec2 worddim = ImGui::CalcTextSize(line.data() + word_idx, 
                                                                     line.data() + char_idx);
                                if (relpos.x >= textstart.x && 
                                    relpos.x <= textstart.x + worddim.x)
                                {
                                    // query a word if we moved words / callstack frames
                                    // TODO: register hover needs '$' in front of name for asm debugging
                                    static size_t hover_frame_line;
                                    static size_t hover_word_idx;
                                    static size_t hover_char_idx;
                                    static size_t hover_num_frames;
                                    static size_t hover_frame_idx;
                                    static String hover_value;

                                    // check to see if we should add the variable
                                    // to the watch variables
                                    if (gui.IsMouseClicked(GLFW_MOUSE_BUTTON_RIGHT))
                                    {
                                        //@@@ force a watch list update
                                        async_stopped = true;
                                        String hover_string(line.data() + word_idx, char_idx - word_idx);
                                        prog.watch_vars.push_back({hover_string, "???"});
                                    }

                                    if (hover_word_idx != word_idx || 
                                        hover_char_idx != char_idx || 
                                        hover_frame_line != frame.line || 
                                        hover_num_frames != prog.frames.size() || 
                                        hover_frame_idx != prog.frame_idx)
                                    {
                                        hover_word_idx = word_idx;
                                        hover_char_idx = char_idx;
                                        hover_frame_line = frame.line;
                                        hover_num_frames = prog.frames.size();
                                        hover_frame_idx = prog.frame_idx;
                                        String word(line.data() + word_idx, char_idx - word_idx);

                                        tsnprintf(tmpbuf, "-data-evaluate-expression --frame %zu --thread 1 \"%s\"", 
                                                  prog.frame_idx, word.c_str());
                                        GDB_SendBlocking(tmpbuf, rec);
                                        hover_value = GDB_ExtractValue("value", rec);
                                    }
                                    else
                                    {
                                        ImGui::BeginTooltip();
                                        ImGui::Text("%s", hover_value.c_str());
                                        ImGui::EndTooltip();
                                    }

                                    break;
                                }

                                char n = '\0';
                                if (char_idx + 1 < line.size())
                                    n = line[char_idx + 1];

                                // C/C++: skip over struct syntax chars to evaluate their members
                                if (c == '.')
                                {
                                    char_idx += 1;
                                    not_delim_struct = false;
                                }
                                else if (c == '-' && n == '>')
                                {
                                    char_idx += 2;
                                    not_delim_struct = false;
                                }
                                else
                                {
                                    textstart.x += worddim.x;
                                }
                            }

                            if (not_delim_struct)
                            {
                                word_idx = BAD_INDEX;
                                if (delim_idx == BAD_INDEX)
                                {
                                    delim_idx = char_idx;
                                }
                            }
                        }
                        else if (word_idx == BAD_INDEX)
                        {
                            if (delim_idx != BAD_INDEX)
                            {
                                // advance non-ident text width
                                ImVec2 dim = ImGui::CalcTextSize(line.data() + delim_idx, 
                                                                 line.data() + char_idx);
                                textstart.x += dim.x;
                            }
                            word_idx = char_idx;
                            delim_idx = BAD_INDEX;
                        }
                    }
                }
            }

            // scroll with up/down arrow key
            float line_height = ImGui::GetScrollMaxY() / (double)file.lines.size();
            float scroll_y = ImGui::GetScrollY();

            if (glfwGetKey(window,GLFW_KEY_UP) == GLFW_PRESS)
            {
                ImGui::SetScrollY(scroll_y - line_height);
            }
            else if (glfwGetKey(window,GLFW_KEY_DOWN) == GLFW_PRESS)
            {
                ImGui::SetScrollY(scroll_y + line_height);
            }
            
        }

        if (gui.source_search_bar_open)
            ImGui::EndChild();
        ImGui::End();
    }

    // control window, registers, locals, watch, program control
    {

        ImGui::SetNextWindowBgAlpha(1.0);   // @Imgui: bug where GetStyleColor doesn't respect window opacity
        ImGui::SetNextWindowPos( { SOURCE_WIDTH, 0 } );
        ImGui::SetNextWindowSize({ WINDOW_WIDTH - SOURCE_WIDTH, WINDOW_HEIGHT });
        ImGui::Begin("Control", NULL, window_flags);

        // TODO: thread control, assembly mode (-exec-step-instruction)


        // continue
        bool clicked;

        ImGuiDisabled(prog.running, clicked = ImGui::Button("---"));
        if (clicked)
        {
            // jump to program counter line
            gui.source_highlighted_line = BAD_INDEX;
        }

        // start
        ImGui::SameLine();
        bool vs_continue = gui.IsKeyClicked(GLFW_KEY_F5);
        ImGuiDisabled(prog.running, clicked = ImGui::Button("|>") || vs_continue);
        if (clicked)
        {
            // TODO: remote doesn't support exec run

            // jump to program counter line
            gui.source_highlighted_line = BAD_INDEX;

            if (!prog.started)
            {
                GDB_SendBlocking("-exec-run --start", "stopped", false);
                prog.started = true;
            }
            else
            {
                GDB_SendBlocking("-exec-continue", "running");
                prog.running = true;
                if (prog.frames.size() > 0)
                    prog.frames[0].line = 0;
            }
        }

        // send SIGINT 
        ImGui::SameLine();
        if (ImGui::Button("||"))
        {
            kill(prog.inferior_process, SIGINT);
        }

        // step line
        ImGui::SameLine();
        ImGuiDisabled(prog.running, clicked = ImGui::Button("-->"));
        if (clicked)
        {
            GDB_SendBlocking("-exec-step", "stopped", false);
        }

        // step over
        ImGui::SameLine();
        ImGuiDisabled(prog.running, clicked = ImGui::Button("/\\>"));
        if (clicked)
        {
            GDB_SendBlocking("-exec-next", "stopped", false);
        }

        // step out
        ImGui::SameLine();
        ImGuiDisabled(prog.running, clicked = ImGui::Button("</\\"));
        if (clicked)
        {
            if (prog.frames.size() == 1)
            {
                // GDB error in top frame: "finish" not meaningful in the outermost frame.
                // emulate visual studios by just running the program  
                GDB_SendBlocking("-exec-continue", "running");
            }
            else
            {
                GDB_SendBlocking("-exec-finish", "stopped", false);
            }
        }
        
        ImGui::SameLine();
        const char *button_desc =
            "--- = display next executed line\n"
            "|>  = start/continue program\n"
            "||  = pause program\n"
            "--> = step into\n"
            "/\\> = step over\n"
           R"(</\ = step out)";
        DrawHelpMarker(button_desc);

        static bool show_config_window;
        show_config_window |= ImGui::Button("config");
        if (show_config_window)
        {
            ImGui::Begin("Tug Configuration", &show_config_window);

            static char config_gdb_path[PATH_MAX];
            ImGui::InputText("gdb path##config_gdb_path", config_gdb_path, 
                             sizeof(config_gdb_path));

            static bool pick_gdb_path;
            ImGui::SameLine();
            pick_gdb_path |= ImGui::Button("...");
            if (pick_gdb_path)
            {
                static FileWindowContext ctx;
                if ( ImGuiFileWindow(ctx, ImGuiFileWindowMode_SelectFile) )
                {
                    if (ctx.selected)
                        tsnprintf(config_gdb_path, "%s", ctx.path.c_str());
                    pick_gdb_path = false;
                }
            }

            ImGui::End();
        }

        for (int i = 0; i < NUM_LOG_ROWS; i++)
        {
            char *row = &prog.log[i][0];
            row[NUM_LOG_COLS] = '\0';
            ImGui::Text("%s", row);
        }

#define CMDSIZE sizeof(ProgramContext::input_cmd[0])
        static char input_command[CMDSIZE] = 
            "target remote localhost:12345";

        static const auto HistoryCallback = [](ImGuiInputTextCallbackData *data) -> int
        {
            if (data->EventKey == ImGuiKey_UpArrow &&
                prog.input_cmd_idx + 1 < prog.num_input_cmds)
            {
                prog.input_cmd_idx++;
                memcpy(data->Buf, &prog.input_cmd[prog.input_cmd_idx][0], CMDSIZE);
                data->BufTextLen = strlen(data->Buf);
            }
            else if (data->EventKey == ImGuiKey_DownArrow)
            {
                if (prog.input_cmd_idx - 1 < 0)
                {
                    prog.input_cmd_idx = -1;
                    memset(data->Buf, 0, CMDSIZE);
                    data->BufTextLen = 0;
                }
                else
                {
                    prog.input_cmd_idx--;
                    memcpy(data->Buf, &prog.input_cmd[prog.input_cmd_idx][0], CMDSIZE);
                    data->BufTextLen = strlen(data->Buf);
                }
            }

            data->BufDirty = true;
            data->CursorPos = data->BufTextLen;
            data->SelectionStart = data->SelectionEnd;  // no select
            return 0;
        };


        if (ImGui::InputText("##input_command", input_command, 
                             sizeof(input_command), 
                             ImGuiInputTextFlags_EnterReturnsTrue | 
                             ImGuiInputTextFlags_CallbackHistory, 
                             HistoryCallback, NULL))
        {
            kill(gdb.spawned_pid, SIGINT);

            // retain focus on the input line
            ImGui::SetKeyboardFocusHere(-1);

            if (input_command[0] == '\0' && prog.num_input_cmds > 0)
            {
                // emulate GDB, repeat last executed command upon hitting
                // enter on an empty line
                GDB_Send(&prog.input_cmd[0][0]);
            }
            else
            {
                GDB_Send(input_command);

                if (prog.num_input_cmds == NUM_USER_CMDS)
                {
                    // hit end of list, end command gets popped
                    prog.num_input_cmds -= 1;
                } 

                prog.input_cmd_idx = -1;

                // store this command to the input cmd
                memmove(&prog.input_cmd[1][0],
                        &prog.input_cmd[0][0],
                        sizeof(prog.input_cmd) - CMDSIZE);

                memcpy(&prog.input_cmd[0][0], input_command, CMDSIZE); 
                prog.num_input_cmds++;

                memset(input_command, 0, CMDSIZE);
            }
        }

        ImGuiTableFlags flags = ImGuiTableFlags_ScrollX |
                                ImGuiTableFlags_ScrollY |
                                ImGuiTableFlags_Borders;

        // @Imgui: can't figure out the right combo of table/column flags corresponding to 
        //         a table with initial column widths that expands column width on elem width increase
        char table_pad[128];
        memset(table_pad, ' ', sizeof(table_pad));
        const int MIN_TABLE_WIDTH_CHARS = 22;
        table_pad[ MIN_TABLE_WIDTH_CHARS ] = '\0';

        if (ImGui::BeginTable("Locals", 2, flags, {300, 200}))
        {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Value");
            ImGui::TableHeadersRow();

            Vector<VarObj> &frame_vars = (prog.frame_idx == 0) ? prog.local_vars : prog.other_frame_vars;
            for (size_t i = 0; i < frame_vars.size(); i++)
            {
                const VarObj &iter = frame_vars[i];
                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::Text("%s", iter.name.c_str());

                ImGui::TableNextColumn();
                ImColor color = (iter.changed) 
                                ? IM_COL32(255, 128, 128, 255)  // light red
                                : IM_COL32_WHITE;
                ImGui::TextColored(color, "%s", iter.value.c_str());
            }

            // empty columns to pad width
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", table_pad);
            ImGui::TableNextColumn();
            ImGui::Text("%s", table_pad);

            ImGui::EndTable();
        }


        int csflags = flags & ~ImGuiTableFlags_BordersInnerH;
        if (ImGui::BeginTable("Callstack", 1, csflags, {300, 200}) )
        {
            //ImGui::TableSetupColumn("Function");
            //ImGui::TableSetupColumn("Value");
            //ImGui::TableHeadersRow();

            for (size_t i = 0; i < prog.frames.size(); i++)
            {
                const Frame &iter = prog.frames[i];
                ImGui::TableNextRow();

                String file = (iter.file_idx < prog.files.size())
                                ? prog.files[ iter.file_idx ].fullpath
                                : "???";

                ImGui::TableNextColumn();
                tsnprintf(tmpbuf, "%zu : %s##%zu", iter.line, file.c_str(), i);
                if ( ImGui::Selectable(tmpbuf, i == prog.frame_idx) )
                {
                    prog.frame_idx = i;
                    gui.source_highlighted_line = BAD_INDEX; // force a re-center
                    if (prog.frame_idx != 0)
                    {
                        // do a one-shot query of a non-current frame
                        // prog.frames is stored from bottom to top so need to do size - 1
                        prog.other_frame_vars.clear();
                        tsnprintf(tmpbuf, "-stack-list-variables --frame %zu --thread 1 --all-values", i);
                        GDB_SendBlocking(tmpbuf, rec);
                        const RecordAtom *variables = GDB_ExtractAtom("variables", rec);

                        for (const RecordAtom &iter : GDB_IterChild(rec, *variables))
                        {
                            VarObj add = {}; 
                            add.name = GDB_ExtractValue("name", iter, rec);
                            add.value = GDB_ExtractValue("value", iter, rec);
                            add.changed = false;
                            prog.other_frame_vars.emplace_back(add);
                        }

                        // set async stopped to re-evaluate watch variables for the selected frame
                        async_stopped = true;
                    }
                }
            }

            ImGui::EndTable();
        }

        struct RegisterName
        {
            String text;
            bool registered;
        };
        static Vector<RegisterName> all_registers;
        static bool show_add_register_window = false;

        if (ImGui::Button("Modify Registers##button"))
        {
            show_add_register_window = true;
            all_registers.clear();
            GDB_SendBlocking("-data-list-register-names", rec);
            const RecordAtom *regs = GDB_ExtractAtom("register-names", rec);

            for (const RecordAtom &reg : GDB_IterChild(rec, *regs))
            {
                RegisterName add = {};
                add.text = GetAtomString(reg.value, rec);
                if (add.text != "") 
                {
                    String to_find = GLOBAL_NAME_PREFIX + add.text;
                    add.registered = false;
                    for (const VarObj &iter : prog.global_vars)
                    {
                        if (iter.name == add.text) 
                        {
                            add.registered = true;
                            break;
                        }
                    }
                    all_registers.emplace_back(add);
                }
            }
        }

        // add register window: query GDB for the list of register
        // names then let the user pick which ones to use
        if (show_add_register_window)
        {
            ImGui::SetNextWindowSize({ 400, 400 });
            ImGui::Begin("Modify Registers##window", &show_add_register_window);

            for (const RegisterName &reg : all_registers)
            {
                if ( ImGui::Checkbox(reg.text.c_str(), (bool *)&reg.registered) )
                {
                    if (reg.registered)
                    {
                        // add register varobj
                        tsnprintf(tmpbuf, "-var-create " GLOBAL_NAME_PREFIX "%s @ $%s",
                                  reg.text.c_str(), reg.text.c_str());
                        GDB_SendBlocking(tmpbuf, rec);
                        prog.global_vars.push_back( {reg.text, "???"} );
                    }
                    else
                    {
                        // delete register varobj
                        for (size_t i = 0; i < prog.global_vars.size(); i++)
                        {
                            auto &src = prog.global_vars;
                            if (src[i].name == reg.text) 
                            {
                                src.erase(src.begin() + i,
                                          src.begin() + i + 1);
                                tsnprintf(tmpbuf, "-var-delete " GLOBAL_NAME_PREFIX "%s", reg.text.c_str());
                                GDB_SendBlocking(tmpbuf);
                            }
                        }
                    }
                }
            }

            ImGui::End();
        }

        if (ImGui::BeginTable("Registers", 2, flags))
        {
            ImGui::TableSetupColumn("Register");
            ImGui::TableSetupColumn("Value");
            ImGui::TableHeadersRow();

            for (size_t i = 0; i < prog.global_vars.size(); i++)
            {
                const VarObj &iter = prog.global_vars[i];
                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::Text("%s", iter.name.c_str());

                ImGui::TableNextColumn();
                ImGui::Text("%s", iter.value.c_str());
                //ImGui::Text("%s %s", iter.name.c_str(), iter.value.c_str());
            }

            ImGui::EndTable();
        }

        bool modified_watchlist = false;
        if (0 && ImGui::BeginTable("Watch", 2, flags, {300, 200}))
        {

            static size_t edit_var_name_idx = -1;
            static size_t max_name_length = 0;
            static bool focus_name_input = false;
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Value");
            ImGui::TableHeadersRow();

            ImGui::PushStyleColor(ImGuiCol_FrameBg,
                                  IM_COL32(255,255,255,16));

            // @Imgui: how to see if an empty column cell has been clicked
            // @VisualBug: after resizing a name column with a long name 
            //             then clicking on a shorter name, the textbox will 
            //             appear empty until arrow left or clicking
            size_t this_max_name_length = MIN_TABLE_WIDTH_CHARS;
            for (size_t i = 0; i < prog.watch_vars.size(); i++)
            {
                VarObj &iter = prog.watch_vars[i];
                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-FLT_MIN);

                // is column clicked?
                static char editwatch[256];
                bool column_clicked = false;
                if (i == edit_var_name_idx)
                {


                    if (ImGui::InputText("##edit_watch", editwatch, 
                                     sizeof(editwatch), 
                                     ImGuiInputTextFlags_EnterReturnsTrue,
                                     NULL, NULL))
                    {
                        iter.name = editwatch;
                        modified_watchlist = true;
                        memset(editwatch, 0, sizeof(editwatch));
                        edit_var_name_idx = -1;
                    }


                    static int hack = 0; // ImGui::WantCaptureKeyboard frame delay
                    if (focus_name_input)
                    {
                        ImGui::SetKeyboardFocusHere(-1);
                        focus_name_input = false;
                        hack = 0;
                    }
                    else
                    {
                        bool deleted = false;
                        hack++;
                        bool active = ImGui::IsItemFocused() && (hack < 2 || ImGui::GetIO().WantCaptureKeyboard);
                        if (gui.IsKeyClicked(GLFW_KEY_DELETE))
                        {
                            prog.watch_vars.erase(prog.watch_vars.begin() + i,
                                                  prog.watch_vars.begin() + i + 1);
                            size_t sz = prog.watch_vars.size();
                            if (sz > 0)
                            {
                                // activate another watch variable input box
                                ImGui::SetKeyboardFocusHere(0);
                                edit_var_name_idx = (i > sz) ? sz - 1 : i;
                                column_clicked = true;
                            }
                            else
                            {
                                deleted = true;
                            } 
                        }

                        if (!active || deleted || gui.IsKeyClicked(GLFW_KEY_ESCAPE))
                        {
                            edit_var_name_idx = -1;
                        }
                    }

                }
                else
                {
                    if (iter.name.size() > this_max_name_length) 
                        this_max_name_length = iter.name.size();

                    // make a clickable region for the empty column space
                    size_t padsize = (iter.name.size() < max_name_length)
                                     ? max_name_length - iter.name.size() : 0;
                    String pad(padsize, ' ');

                    ImGui::Text("%s%s", iter.name.c_str(), pad.c_str());
                    if (ImGui::IsItemClicked())
                        column_clicked = true;
                }

                if (column_clicked)
                {
                    tsnprintf(editwatch, "%s", iter.name.c_str());
                    focus_name_input = true;
                    edit_var_name_idx = i;
                }


                ImGui::TableNextColumn();
                ImColor color = (iter.changed) 
                                ? IM_COL32(255, 128, 128, 255)  // light red
                                : IM_COL32_WHITE;
                ImGui::TextColored(color, "%s", iter.value.c_str());
            }

            max_name_length = this_max_name_length;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            static char watch[128];

            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::InputText("##create_new_watch", watch, 
                             sizeof(watch), 
                             ImGuiInputTextFlags_EnterReturnsTrue,
                             NULL, NULL))
            {
                prog.watch_vars.push_back( {watch, "???"} );
                modified_watchlist = true;
                memset(watch, 0, sizeof(watch));
            }
            ImGui::PopStyleColor();

            ImGui::TableNextColumn();
            ImGui::Text("%s", "");  // -Wformat

            // empty columns to pad width
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", table_pad);
            ImGui::TableNextColumn();
            ImGui::Text("%s", table_pad);

            ImGui::EndTable();
        }


        if (async_stopped || modified_watchlist)
        {
            // evaluate user defined watch variables
            for (VarObj &iter : prog.watch_vars)
            {
                tsnprintf(tmpbuf, "-data-evaluate-expression --frame %zu --thread 1 \"%s\"", 
                          prog.frame_idx, iter.name.c_str());
                GDB_SendBlocking(tmpbuf, rec);
                String s = GDB_ExtractValue("value", rec);
                if (s == "") s = "???";
                iter.changed = (iter.value != s);
                iter.value = s;
            }
        }
        
        ImGui::End();
    }

    // update the last gui states
    gui.last_keystate = gui.this_keystate;
    gui.last_mousestate = gui.this_mousestate;
}

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
    Assert(false);
}

int main(int, char**)
{
    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Tug", NULL, NULL);
    if (window == NULL)
        return 1;

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync
    if (0 < GDB_Init(window))
        return 1;

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL2_Init();

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();

    if (prog.config_font_filename != "")
    {
        float fontsize = atof(prog.config_font_filename.c_str());
        if (fontsize == 0) fontsize = 12.0f;
        io.Fonts->AddFontFromFileTTF(prog.config_font_filename.c_str(), fontsize);
    }

    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();


        {

            if (GLFW_PRESS == glfwGetKey(window, GLFW_KEY_F1))
            {
                char tmp[256];
                ImDrawList *drawlist = ImGui::GetForegroundDrawList();
                tsnprintf(tmp, "Mouse Position: (%.1f,%.1f)", io.MousePos.x, io.MousePos.y);
                ImVec2 textsize = ImGui::CalcTextSize(tmp);
                ImVec2 TL = { 0, 0 };

                drawlist->AddRectFilled(TL, textsize, 0xFFFFFFFF);
                drawlist->AddText(TL, 0xFF000000, tmp);

                tsnprintf(tmp, "Application average %.3f ms/frame (%.1f FPS)",
                          1000.0f / ImGui::GetIO().Framerate, 
                          ImGui::GetIO().Framerate);

                TL.y += textsize.y;
                textsize = ImGui::CalcTextSize(tmp);
                drawlist->AddRectFilled(TL, textsize, 0xFFFFFFFF);
                drawlist->AddText(TL, 0xFF000000, tmp);
            }

            //
            // global styles
            //

            // lessen the intensity of selectable hover color
            ImVec4 active_col = ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive);
            active_col.x *= (1.0/2.0);
            active_col.y *= (1.0/2.0);
            active_col.z *= (1.0/2.0);

            ImColor IM_COL_BACKGROUND = IM_COL32(22,22,22,255);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL_BACKGROUND.Value);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, active_col);
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, active_col);

            GDB_Draw(window);

            ImGui::PopStyleColor(3);
        }
        
        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);

        // If you are using this code with non-legacy OpenGL header/contexts (which you should not, prefer using imgui_impl_opengl3.cpp!!),
        // you may need to backup/reset/restore other state, e.g. for current shader using the commented lines below.
        //GLint last_program;
        //glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
        //glUseProgram(0);
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        //glUseProgram(last_program);

        glfwMakeContextCurrent(window);
        glfwSwapBuffers(window);
    }

    GDB_Shutdown();

    // Cleanup
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
