#include "common.h"

//
// yoinked from glfw_example_opengl2
//

// third party
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl2.h>
#include <GLFW/glfw3.h>
#include "imgui_file_window.h"

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 640
#define SOURCE_WIDTH 800


// dynamic colors that change upon the brightness of the background
ImVec4 IM_COL32_WIN_RED;

// verify a printf-family variadic arguments, MSVC doesn't have attribute printf like GCC
// pass this as a parameter to printf like function macro
#define VARGS_CHECK(fmt, ...) snprintf(NULL, 0, fmt, ##__VA_ARGS__)
#define StringPrintf(fmt, ...) _StringPrintf(VARGS_CHECK(fmt, ##__VA_ARGS__), fmt, ##__VA_ARGS__)
String _StringPrintf(int /* vargs_check */, const char *fmt, ...)
{
    String result;
    va_list args;
    va_start(args, fmt);
    int rc = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    if (rc < 0)
    {
        PrintErrorLibC("vsnprintf");
    }
    else
    {
        result.resize(rc + 1, '\0');
        va_start(args, fmt);
        rc = vsnprintf((char *)result.data(), result.size(), fmt, args);
        va_end(args);

        if (rc < 0)
        {
            PrintErrorLibC("vsnprintf");
            result = "";
        }
        else 
        {
            // remove trailing nt, string maintains one internally
            result.pop_back();
        }
    }

    return result;
}

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

enum LineDisplay
{
    LineDisplay_Source,
    LineDisplay_Disassembly,
    LineDisplay_Source_And_Disassembly,
    //LineDisplay_Disassembly_With_Opcodes,
    //LineDisplay_Source_And_Disassembly_With_Opcodes,
};

struct GUI
{
    GlfwKeyboardState this_keystate;
    GlfwKeyboardState last_keystate;
    GlfwMouseState this_mousestate;
    GlfwMouseState last_mousestate;

    LineDisplay line_display = LineDisplay_Source;
    Vector<DisassemblyLine> line_disasm;
    Vector<DisassemblySourceLine> line_disasm_source;
    bool show_machine_interpreter_commands;

    bool jump_to_exec_line;
    bool source_search_bar_open = false;
    char source_search_keyword[256];
    size_t source_found_line_idx;

    bool IsKeyClicked(int glfw_key, int glfw_mods = 0)
    {
        GlfwInput &this_key = Key(glfw_key);
        GlfwInput &last_key = LastKey(glfw_key);
        bool result = last_key.action == GLFW_PRESS && 
                      this_key.action == GLFW_RELEASE;

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
        bool result = last_button.action == GLFW_PRESS && 
                      this_button.action == GLFW_RELEASE;

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

Program prog;
GDB gdb;
GUI gui;

void dbg() {}

uint64_t ParseHex(const String &str)
{
    uint64_t result = 0;
    uint64_t pow = 1;
    for (size_t i = str.size() - 1; i < str.size(); i--)
    {
        uint64_t num = 0;
        char c = str[i];
        if (c == 'x' || c == 'X') 
            break;

        if (c >= 'a' && c <= 'f')
        {
            num = 10 + (c - 'a');
        }
        else if (c >= 'A' && c <= 'F')
        {
            num = 10 + (c - 'A');
        }
        else if (c >= '0' && c <= '9')
        {
            num = c - '0';
        }
        result += (num * pow);
        pow *= 16;
    }
    return result;
}

void *RedirectStdin(void *)
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

ssize_t read_block_maxsize = 0;
void *ReadInterpreterBlocks(void *)
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

    fprintf(stderr, "closing GDB interpreter read loop...\n");
    return NULL;
}

int Tug_Shutdown()
{
    printf("read_block_maxsize: %zu\n", (size_t)read_block_maxsize);
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

bool CreateFile(const char *fullpath, File &result)
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

size_t CreateOrGetFile(const String &fullpath)
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
        CreateFile(fullpath.c_str(), prog.files[ result ]);
    }

    return result;
}


bool Tug_Init(GLFWwindow *window, int argc, char **argv)
{
    int rc = 0;

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

                static_assert(sizeof(prog.config) % sizeof(ConfigPair) == 0, "prog.config not all ConfigPair's");
                ConfigPair *str = reinterpret_cast<ConfigPair *>(&prog.config);
                for (size_t i = 0; i < NUM_CONFIG; i++)
                {
                    if (key == str[i].key) 
                        str[i].value = value.c_str();
                }

            }
        }

        file.close();
    }

    int pipes[2] = {};
    rc = pipe(pipes);
    if (rc < 0)
    {
        PrintErrorLibC("from gdb pipe");
        return false;
    }

    gdb.fd_in_read = pipes[0];
    gdb.fd_in_write = pipes[1];

    rc = pipe(pipes);
    if (rc < 0)
    {
        PrintErrorLibC("to gdb pipe");
        return false;
    }

    gdb.fd_out_read = pipes[0];
    gdb.fd_out_write = pipes[1];

    rc = pthread_mutex_init(&gdb.modify_block, NULL);
    if (rc < 0) 
    {
        PrintErrorLibC("pthread_mutex_init");
        return false;
    }

    gdb.recv_block = sem_open("recv_gdb_block", O_CREAT, S_IRWXU, 0);
    if (gdb.recv_block == NULL) 
    {
        PrintErrorLibC("sem_open");
        return false;
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

    if (prog.config.gdb_path.value != "")
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
        String args = prog.config.gdb_path.value + " " + prog.config.gdb_args.value + " --interpreter=mi "; 

        std::string buf;
        size_t startoff = 0;
        size_t spaceoff = 0;
        size_t bufoff = 0;
        Vector<char *> gdb_argv;
        bool inside_string = false;
        bool is_whitespace = true;

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
        for (size_t i = 0; i < gdb_argv.size(); i++)
        {
            gdb_argv[i] = (char *)((size_t)gdb_argv[i] + buf.data());
        }
        gdb_argv.push_back(NULL);

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
            return false;
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

        rc = posix_spawnp((pid_t *) &gdb.spawned_pid, prog.config.gdb_path.value.c_str(),
                          &actions, &attrs, gdb_argv.data(), envptr.data());
        if (rc != 0) 
        {
            errno = rc;
            PrintErrorLibC("posix_spawnp");
            return false;
        }

    }

    rc = pthread_create(&gdb.thread_read_interp, NULL, ReadInterpreterBlocks, (void*) NULL);
    if (rc < 0) 
    {
        PrintErrorLibC("pthread_create");
        return false;
    }

#if 1
    //GDB_SendBlocking("-environment-cd /mnt/c/Users/Kyle/Documents/commercial-codebases/original/stevie");
    //GDB_SendBlocking("-file-exec-and-symbols stevie");
    //GDB_SendBlocking("-exec-arguments stevie.c");

    // debug GAS
    //GDB_SendBlocking("-environment-cd /mnt/c/Users/Kyle/Documents/commercial-codebases/original/binutils/binutils-gdb/gas");
    //GDB_SendBlocking("-file-exec-and-symbols as-new");

    //GDB_SendBlocking("-environment-cd \"/mnt/c/Users/Kyle/Downloads/Chrome Downloads/ARM/AARCH32\"");
    //GDB_SendBlocking("-file-exec-and-symbols advent.out");


    Record rec;
    GDB_SendBlocking("-list-features", rec);
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
    src = rec.buf.c_str();
    gdb.supports_async_execution =          (NULL != strstr(src, "async"));
    gdb.supports_reverse_execution =        (NULL != strstr(src, "reverse"));

    if (argc > 1)
    {
        struct stat sb;
        if ( (0 == stat(argv[1], &sb)) && (sb.st_mode & S_IXUSR) )
        {
            // override debug application
            prog.config.debug_exe_path.value = argv[1]; 
        }
        else
        {
            String err = StringPrintf("couldn't open file: %s", argv[1]);
            LogLine(err.data(), err.size());
        }
    }

    if (prog.config.debug_exe_path.value != "")
    {
        String str = StringPrintf("-file-exec-and-symbols \"%s\"", 
                                  prog.config.debug_exe_path.value.c_str());
        GDB_Send(str.c_str());
    }

    if (prog.config.debug_exe_args.value != "")
    {
        String str = StringPrintf("-exec-arguments %s",
                                  prog.config.debug_exe_args.value.c_str());
        GDB_Send(str.c_str());
    }

    // set FILE_IDX_INVALID
    //
    CreateOrGetFile( String("") );

    // preload all the referenced files
    //GDB_SendBlocking("-file-list-exec-source-files", tmp, "files");
    //const RecordAtom *files = GDB_ExtractAtom("files", tmp);
    //for (const RecordAtom &file : GDB_IterChild(tmp, files))
    //{
    //    String fullpath = GDB_ExtractValue("fullname", file, tmp);
    //    File::Create(fullpath.c_str(), file_ctx);
    //    prog.files.emplace_back(file_ctx);
    //}

#endif

    // TODO: setup global var objects that will last the duration of the program

    const auto UpdateKey = [](GLFWwindow * /* window */, int key, int /* scancode */,
                              int action, int mods) -> void
    {
        Assert((size_t)key < ArrayCount(gui.this_keystate.keys));
        Assert((uint8_t)action <= UINT8_MAX && (uint8_t)mods <= UINT8_MAX);
        //printf("key pressed: %s, glfw_macro: %d", name, key);

        GlfwInput &update = gui.this_keystate.keys[key];
        update.action = action;
        update.mods = mods;
    };

    const auto UpdateMouseButton = [](GLFWwindow * /* window */, int button, int action, int mods)
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

    return true;
}

#define ImGuiDisabled(is_disabled, code)\
ImGui::BeginDisabled(is_disabled);\
code;\
ImGui::EndDisabled();

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

void LogLine(const char *buf, size_t bufsize)
{
    if ( bufsize >= 5 && (0 == memcmp(buf, "(gdb)", 5)) )
        return;

    bool is_mi_record = (bufsize > 0) && 
                        (buf[0] == PREFIX_ASYNC0 || 
                         buf[0] == PREFIX_ASYNC1 ||
                         buf[0] == PREFIX_RESULT );

    if (is_mi_record && !gui.show_machine_interpreter_commands)
        return;

    // debug
    //printf("%.*s\n", int(bufsize), buf);

    // shift lines up, add this line to the collection
    memmove(&prog.log[0], &prog.log[1], 
            sizeof(prog.log) - sizeof(prog.log[0]));

    ConsoleLine &dest = prog.log[NUM_LOG_ROWS - 1];
    dest.type = ConsoleLineType_None;
    memset(dest.text, ' ', NUM_LOG_COLS);
    size_t dest_idx = 0;

    const auto PushChar = [&](char c)
    {
        if (dest_idx < NUM_LOG_COLS)
            dest.text[dest_idx++] = c;
    };

    dbg();

    if (bufsize > 2 && 
        (buf[0] == PREFIX_DEBUG_LOG ||
         buf[0] == PREFIX_TARGET_LOG ||
         buf[0] == PREFIX_CONSOLE_LOG ) &&
        buf[1] == '\"')
    {
        if (buf[0] == PREFIX_DEBUG_LOG)
            dest.type = ConsoleLineType_UserInput;

        // console record, format ~"text text text"\n
        // skip over the beginning/ending characters
        size_t i = 2;
        bufsize -= 2;

        for (; i < bufsize; i++)
        {
            char c = buf[i];
            char n = (i + 1 < bufsize) ? buf[i + 1] : '\0';

            if (c == '\\')
            {
                switch (n)
                {
                    case 't': 
                        for (size_t t = 0; t < 2; t++)
                            PushChar(' ');
                        break;
                    case '\\':
                    case '\"':
                        PushChar(n);
                        break;
                    default:
                        break;
                }

                i++; // skip over the evaluated literal char
            }
            else
            {
                PushChar(buf[i]);
            }
        }
    }
    else
    {
        // text that isn't a log record ex: shell ls
        memcpy(dest.text, buf, GetMin(bufsize, NUM_LOG_COLS));
    }

    size_t nt_index = GetMin(bufsize - 1, NUM_LOG_COLS);
    dest.text[nt_index] = '\0';
    prog.log_scroll_to_bottom = true;
}

VarObj CreateVarObj(String name, String value = "")
{
    VarObj result = {};
    result.name = name;
    result.value = value;
    if (result.value == "") 
    {
        result.value = "???";
    }

    result.changed = true;
    if (result.value[0] == '{')
    {
        static bool resize_once = true;
        value = name + " = " + value;
        static struct ParseRecordContext ctx = {};

        if (resize_once)
        {
            resize_once = false;
            ctx.atoms.resize( 16 * 1024 ); // @Hack
        }

        ctx.i = 0;
        ctx.atom_idx = 0;
        ctx.num_end_atoms = 0;
        ctx.error = false;
        ctx.buf = value.c_str();
        ctx.bufsize = value.size();

        RecordAtom root = GDB_RecurseEvaluation(ctx).atom;

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
                RecordAtom *atom = &ordered_base[i];
                if ( (atom->type == Atom_Array || atom->type == Atom_Struct) &&
                     (atom->value.length != 0))
                {
                    // adjust aggregate offset
                    Assert(atom->value.index > ordered_offset);
                    atom->value.index -= ordered_offset;
                }
            }

            result.expr.atoms.resize(ctx.num_end_atoms);
            memcpy(&result.expr.atoms[0], 
                   &ctx.atoms[ ctx.atoms.size() - ctx.num_end_atoms ],
                   sizeof(RecordAtom) * ctx.num_end_atoms);
            result.expr.buf = value;
            result.expr_changed.resize( ctx.num_end_atoms );

            //GDB_PrintRecordAtom(result.expr, result.expr.atoms[0], 0);
        }
    }

    return result;
}

bool RecurseCheckChanged(VarObj &this_var, const VarObj &last_var, size_t atom_idx)
{
    bool changed = false;
    Assert((atom_idx < this_var.expr.atoms.size()) && 
           (atom_idx < last_var.expr.atoms.size()));
    RecordAtom &parent = this_var.expr.atoms[atom_idx];
    //Assert(parent.type == Atom_Struct || parent.type == Atom_Array);
    if (parent.type != Atom_Struct && parent.type != Atom_Array)
        return true;

    size_t i = parent.value.index;
    size_t end = i + parent.value.length;

    for (; i < end; i++)
    {
        const RecordAtom &this_child = this_var.expr.atoms[i];
        if (i >= last_var.expr.atoms.size())
        {
            // array changing from run length to actual value
            // TODO: get the actual changed value
            this_var.expr_changed[i] = true;
            changed |= this_var.expr_changed[i];
        }
        else
        {
            const RecordAtom &last_child = last_var.expr.atoms[i];
            if (this_child.type == Atom_Struct || last_child.type == Atom_Array)
            {
                changed |= RecurseCheckChanged(this_var, last_var, i);
            }
            else if (this_child.type == Atom_String)
            {
                const char *this_buf = this_var.expr.buf.c_str();
                const char *last_buf = last_var.expr.buf.c_str();

                Assert(this_child.value.index + this_child.value.length <=
                       this_var.expr.buf.size());
                Assert(last_child.value.index + last_child.value.length <=
                       last_var.expr.buf.size());

                this_var.expr_changed[i] 
                    = (this_child.value.length != last_child.value.length) ||
                    (0 != memcmp(this_buf + this_child.value.index,
                                 last_buf + last_child.value.index,
                                 this_child.value.length) );

                changed |= this_var.expr_changed[i];
            }
        }
    }

    this_var.expr_changed[atom_idx] = changed;
    return changed;
}

void CheckIfChanged(VarObj &this_var, const VarObj &last_var)
{
    if (last_var.value[0] == '{')
    {
        // aggregate, go through each child and check if it changed
        dbg();
        RecurseCheckChanged(this_var, last_var, 0 /* root index */);
    }
    else
    {
        this_var.changed = (this_var.value != last_var.value);
    }
}

void QueryWatchlist()
{
    // evaluate user defined watch variables
    Record rec;
    for (VarObj &iter : prog.watch_vars)
    {
        String expr;
        const char *src = iter.name.c_str();
        const char *comma = strchr(src, ',');

        if (comma != NULL)
        {
            // translate visual studio syntax to GDB
            // arrayname, 10 -> *arrayname@10
            expr = StringPrintf("*(%.*s)@%s", (int)(comma - src), src, comma + 1);
        }
        else
        {
            expr = iter.name;
        }

        String cmd = StringPrintf("-data-evaluate-expression --frame %zu --thread 1 \"%s\"", 
                                  prog.frame_idx, expr.c_str());

        GDB_SendBlocking(cmd.c_str(), rec);
        VarObj incoming = CreateVarObj("incoming", GDB_ExtractValue("value", rec));
        CheckIfChanged(incoming, iter);
        iter.value = incoming.value;
        iter.expr = incoming.expr;
        iter.expr_changed = incoming.expr_changed;
    }
}

void GetFunctionDisassembly(const Frame &frame)
{
    char tmpbuf[4096];
    Record rec = {};
    gui.line_disasm.clear();
    gui.line_disasm_source.clear();

    // functions with this name don't support function disassembly from address or file/line combos
    // found this type of function in file: /lib64/ld-linux-x86-64.so.2
    if (frame.func == "??")
        return;

    if (frame.file_idx == FILE_IDX_INVALID)
    {
        if (!gdb.has_data_disassemble_option_a)
            return; // operation not supported, bail early

        // some frames don't have an associated file ex: _start function after returning from main
        tsnprintf(tmpbuf, "-data-disassemble -a %s 0", // 0 = disasm only
                  frame.func.c_str());
    }
    else
    {
        // -n -1 = disassemble all lines in the function its contained in
        // 5 = source and disasm with opcodes
        tsnprintf(tmpbuf, "-data-disassemble -f \"%s\" -l %zu -n -1 5",
                  prog.files[ frame.file_idx ].fullpath.c_str(), frame.line);
    } 
    

    GDB_SendBlocking(tmpbuf, rec);

    const RecordAtom *instrs = GDB_ExtractAtom("asm_insns", rec);
    if (frame.file_idx != FILE_IDX_INVALID)
    {
        for (const RecordAtom &src_and_asm_line : GDB_IterChild(rec, instrs))
        {
            // array of src_and_asm_line
            //     line="32"
            //     file="debug.c"
            //     fullname="/mnt/c/Users/Kyle/Documents/Visual Studio 2017/Projects/Tug/debug.c"
            //     line_asm_insn
            bool is_first_inst = true;
            DisassemblySourceLine line_src = {};
            const RecordAtom *atom = GDB_ExtractAtom("line_asm_insn", src_and_asm_line, rec);
            line_src.line_number = (size_t)GDB_ExtractInt("line", src_and_asm_line, rec);
            line_src.num_instructions = 0;

            for (const RecordAtom &line_asm_inst : GDB_IterChild(rec, atom))
            {
                // array of unnamed struct 
                //     address="0x0000555555555248"
                //     func-name="main"
                //     offset="176"
                //     opcodes="74 05"
                //     line_asm_inst="je     0x55555555524f <main+183>"

                DisassemblyLine add = {};
                String string_addr = GDB_ExtractValue("address", line_asm_inst, rec);
                String func = GDB_ExtractValue("func-name", line_asm_inst, rec);
                String offset_from_func = GDB_ExtractValue("offset", line_asm_inst, rec);
                String inst = GDB_ExtractValue("inst", line_asm_inst, rec);
                String opcodes = GDB_ExtractValue("opcodes", line_asm_inst, rec);


                tsnprintf(tmpbuf, "%s <%s+%s> %s", 
                          string_addr.c_str(), func.c_str(),
                          offset_from_func.c_str(), inst.c_str());

                add.addr = ParseHex(string_addr);
                add.text = tmpbuf;
                gui.line_disasm.emplace_back(add);
                line_src.num_instructions++;

                if (is_first_inst)
                {
                    line_src.addr = add.addr;
                    is_first_inst = false;
                }
            }

            gui.line_disasm_source.emplace_back(line_src);
        }
    }
    else
    {
        // getting function disassembly for a fileless frame
        for (const RecordAtom &line_asm_inst : GDB_IterChild(rec, instrs))
        {
            // array of unnamed struct 
            //     address="0x0000555555555248"
            //     func-name="main"
            //     offset="176"
            //     opcodes="74 05"
            //     inst="je     0x55555555524f <main+183>"

            DisassemblyLine add = {};
            String string_addr = GDB_ExtractValue("address", line_asm_inst, rec);
            String func = GDB_ExtractValue("func-name", line_asm_inst, rec);
            String offset_from_func = GDB_ExtractValue("offset", line_asm_inst, rec);
            String inst = GDB_ExtractValue("inst", line_asm_inst, rec);
            String opcodes = GDB_ExtractValue("opcodes", line_asm_inst, rec);

            tsnprintf(tmpbuf, "%s <%s+%s> %s", 
                      string_addr.c_str(), func.c_str(),
                      offset_from_func.c_str(), inst.c_str());

            add.addr = ParseHex(string_addr);
            add.text = tmpbuf;
            gui.line_disasm.emplace_back(add);
        }
    }
}

void RecurseSetNodeState(const Record &rec, size_t atom_idx, int state, String name)
{
    const RecordAtom &parent = rec.atoms[atom_idx];
    ImGuiID parent_id = ImGui::GetID(name.c_str()); 
    ImGui::GetStateStorage()->SetInt(parent_id, state);

    for (size_t i = 0; i < parent.value.length; i++)
    {
        size_t childoffset = parent.value.index + i;
        const RecordAtom &child = rec.atoms[childoffset];
        String childname = name + String(rec.buf.c_str() + child.name.index,
                                         child.name.length);
        if (child.type == Atom_Array || child.type == Atom_String)
        {
            RecurseSetNodeState(rec, childoffset, state, childname);
        }
    } 
}

// draw an aggregate data type in the form of a two column table
void RecurseExpressionTreeNodes(const VarObj &var, size_t atom_idx, 
                                size_t parent_array_index = 0)
{
    char tmpbuf[4096];
    const Record &src = var.expr;
    const RecordAtom &parent = src.atoms[atom_idx];
    Assert(parent.type == Atom_Struct || parent.type == Atom_Array);
    if (parent.name.length != 0)
    {
        tsnprintf(tmpbuf, "%.*s##%zu", (int)parent.name.length, 
                  &src.buf[ parent.name.index ], atom_idx);
    }
    else
    {
        tsnprintf(tmpbuf, "[%zu]", parent_array_index);
    }

    bool close_after = false;
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    if (ImGui::TreeNode(tmpbuf))
    {
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
            close_after = true;

        ImGui::TableNextColumn();
        size_t i = parent.value.index;
        size_t end = i + parent.value.length;
        size_t array_index = 0;
        for (; i < end; i++)
        {
            const RecordAtom &child = src.atoms[i];
            if (child.type == Atom_Struct || child.type == Atom_Array)
            {
                RecurseExpressionTreeNodes(var, i, array_index);
            }
            else
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                if (child.name.length > 0)
                {
                    Assert(child.name.length > 0);
                    ImGui::Text("%.*s", (int)child.name.length,
                                &src.buf[ child.name.index ]);
                }
                else
                {
                    ImGui::Text("[%zu]", array_index);
                }

                ImColor color = (var.expr_changed[i])
                    ? IM_COL32_WIN_RED
                    : ImGui::GetStyleColorVec4(ImGuiCol_Text);
                ImGui::TableNextColumn();
                ImGui::TextColored(color, "%.*s", (int)child.value.length, 
                                   &src.buf[ child.value.index ]);
            }

            array_index++;
        }

        ImGui::TreePop();
    }
    else if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
    {
        String name = String(src.buf.c_str() + parent.name.index, 
                             parent.name.length);
        // TODO fix empty names RecurseSetNodeState(src, atom_idx, 1, name);
    }

    if (close_after)
    {
        String name = String(src.buf.c_str() + parent.name.index,
                             parent.name.length);
        // TODO fix empty names RecurseSetNodeState(src, atom_idx, 0, name);
    }
}
 
void Draw(GLFWwindow *window)
{
    // process async events
    static Record rec;                  // scratch record 
    char tmpbuf[4096];                  // scratch for snprintf

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
    size_t last_num_recs = prog.num_recs;
    prog.num_recs = 0;

    for (size_t i = 0; i < last_num_recs; i++)
    {
        RecordHolder &iter = prog.read_recs[i];
        if (!iter.parsed)
        {
            Record &parse_rec = iter.rec;
            iter.parsed = true;
            char prefix = parse_rec.buf[0];

            char *comma = (char *)memchr(parse_rec.buf.data(), ',', parse_rec.buf.size());
            if (comma == NULL) 
                comma = &parse_rec.buf[ parse_rec.buf.size() ];

            const char *start = parse_rec.buf.data() + 1;
            String prefix_word(start, comma - start);

            if (prefix == PREFIX_ASYNC0)
            {
                if (prefix_word == "breakpoint-created")
                {
                    Breakpoint res = {};
                    res.number = GDB_ExtractInt("bkpt.number", parse_rec);
                    res.line = GDB_ExtractInt("bkpt.line", parse_rec);
                    res.addr = ParseHex( GDB_ExtractValue("bkpt.addr", parse_rec) );

                    String fullpath = GDB_ExtractValue("bkpt.fullname", parse_rec);
                    res.file_idx = CreateOrGetFile(fullpath);

                    prog.breakpoints.push_back(res);
                }
                else if (prefix_word == "breakpoint-deleted")
                {
                    size_t id = (size_t)GDB_ExtractInt("id", parse_rec);
                    auto &bpts = prog.breakpoints;
                    for (size_t b = 0; b < bpts.size(); b++)
                    {
                        if (bpts[b].number == id)
                        {
                            bpts.erase(bpts.begin() + b, 
                                       bpts.begin() + b + 1);
                            break;
                        }
                    }
                }
                else if (prefix_word == "thread-group-started")
                {
                    prog.inferior_process = (pid_t)GDB_ExtractInt("pid", parse_rec);
                }
                else if (prefix_word == "thread-selected")
                {
                    // user jumped to a new thread/frame from the console window
                    int index = (size_t)GDB_ExtractInt("frame.level", parse_rec);
                    if ((size_t)index < prog.frames.size())
                    {
                        // jump to program counter line at this new frame
                        // this code is copied from the Callstack window section

                        prog.frame_idx = (size_t)index;
                        gui.jump_to_exec_line = true;
                        if (prog.frame_idx != 0)
                        {
                            // do a one-shot query of a non-current frame
                            // prog.frames is stored from bottom to top so need to do size - 1

                            prog.other_frame_vars.clear();
                            tsnprintf(tmpbuf, "-stack-list-variables --frame %zu --thread 1 --all-values", prog.frame_idx);
                            GDB_SendBlocking(tmpbuf, rec);
                            const RecordAtom *variables = GDB_ExtractAtom("variables", rec);

                            for (const RecordAtom &atom : GDB_IterChild(rec, variables))
                            {
                                VarObj add = CreateVarObj(GDB_ExtractValue("name", atom, rec),
                                                          GDB_ExtractValue("value", atom, rec));
                                prog.other_frame_vars.emplace_back(add);
                            }

                        }

                        if (gui.line_display != LineDisplay_Source)
                            GetFunctionDisassembly(prog.frames[ prog.frame_idx ]);

                        QueryWatchlist();
                    }
                }
            }
            else if (prefix_word == "stopped")
            {
                prog.frame_idx = 0;
                prog.running = false;
                async_stopped = true;
                String reason = GDB_ExtractValue("reason", parse_rec);

                if ( (NULL != strstr(reason.c_str(), "exited")) )
                {
                    prog.started = false;
                    prog.frames.clear();
                    prog.local_vars.clear();
                }
            }
        }
    }

    if (async_stopped)
    {
        gui.jump_to_exec_line = true;
        QueryWatchlist();

        // TODO: remote ARM32 debugging is this up after jsr macro
        /* !!! */ //GDB_SendBlocking("-stack-list-frames 0 0", rec, "stack");
        /* !!! */ GDB_SendBlocking("-stack-list-frames", rec);
        const RecordAtom *callstack = GDB_ExtractAtom("stack", rec);
        if (callstack)
        {
            String arch = "";
            static bool set_default_registers = true;
            prog.frames.clear();
            static String last_stack_sig;
            String this_stack_sig;

            for (const RecordAtom &level : GDB_IterChild(rec, callstack))
            {
                Frame add = {};
                add.line = (size_t)GDB_ExtractInt("line", level, rec);
                add.addr = ParseHex( GDB_ExtractValue("addr", level, rec) );
                add.func = GDB_ExtractValue("func", level, rec);
                arch = GDB_ExtractValue("arch", level, rec);
                this_stack_sig += add.func;

                String fullpath = GDB_ExtractValue("fullname", level, rec);
                add.file_idx = CreateOrGetFile(fullpath);

                prog.frames.emplace_back(add);
            }

            if (last_stack_sig != this_stack_sig)
            {
                prog.local_vars.clear();
                last_stack_sig = this_stack_sig;
                if (gui.line_display != LineDisplay_Source && prog.frames.size() > 0)
                    GetFunctionDisassembly(prog.frames[0]);
            }

            if (set_default_registers && arch != "")
            {
                set_default_registers = false;

                // set the default registers for a given architecture
                // TODO: @GDB: is there a command to query this without having to
                // get stack frames
                const char * const* registers = NULL;
                size_t num_registers = 0;
                if (arch == "i386:x86-64")
                {
                    registers = DEFAULT_REG_AMD64;
                    num_registers = ArrayCount(DEFAULT_REG_AMD64);
                }
                else if (arch == "i386")
                {
                    registers = DEFAULT_REG_X86;
                    num_registers = ArrayCount(DEFAULT_REG_X86);
                }
                else if (arch.size() >= 3 && 0 == memcmp(arch.data(), "arm", 3))
                {
                    registers = DEFAULT_REG_ARM;
                    num_registers = ArrayCount(DEFAULT_REG_ARM);
                }

                for (size_t i = 0; i < num_registers; i++)
                {
                    // add register varobj, copied from var creation in async_stopped if statement
                    // the only difference is GLOBAL_NAME_PREFIX and '@' used to signify a varobj
                    // that lasts the duration of the program

                    tsnprintf(tmpbuf, "-var-create " GLOBAL_NAME_PREFIX "%s @ $%s", 
                              registers[i], registers[i]);
                    GDB_SendBlocking(tmpbuf, rec);
                    VarObj add = CreateVarObj(registers[i], GDB_ExtractValue("value", rec));
                    prog.global_vars.emplace_back(add);
                }
            }
        }

        // get local variables for this stack frame
        // TODO: not actually GDB variable objects, problems with aggregates displaying updates

        GDB_SendBlocking("-stack-list-variables --all-values", rec);
        for (VarObj &local : prog.local_vars) local.changed = false;
        
        const RecordAtom *vars = GDB_ExtractAtom("variables", rec);
        for (const RecordAtom &child : GDB_IterChild(rec, vars))
        {
            VarObj incoming = CreateVarObj(GDB_ExtractValue("name", child, rec),
                                           GDB_ExtractValue("value", child, rec));

            bool found = false;
            for (size_t i = prog.local_vars.size(); i > 0; i--)
            {
                VarObj &local = prog.local_vars[i - 1];
                if (local.name == incoming.name)
                {
                    // @Hack: clean this up
                    CheckIfChanged(incoming, local);
                    local.value = incoming.value;
                    local.expr = incoming.expr;
                    local.expr_changed = incoming.expr_changed;
                    local.changed = incoming.changed;
                    found = true;
                    break;
                }
            }

            if (!found)
                prog.local_vars.emplace_back(incoming);
        }

        // update global values, just registers right now
        GDB_SendBlocking("-var-update --all-values *", rec);
        const RecordAtom *changelist = GDB_ExtractAtom("changelist", rec);
        for (VarObj &global : prog.global_vars) global.changed = false;

        for (const RecordAtom &iter : GDB_IterChild(rec, changelist))
        {
            VarObj incoming = CreateVarObj(GDB_ExtractValue("name", iter, rec),
                                           GDB_ExtractValue("value", iter, rec));

            const char *srcname = incoming.name.c_str();
            const char *namestart = NULL;
            if ( (namestart = strstr(srcname, GLOBAL_NAME_PREFIX)) )
            {
                // check for global variable change 
                namestart += strlen(GLOBAL_NAME_PREFIX);
                for (VarObj &global : prog.global_vars)
                {
                    if (global.name == namestart)
                    {
                        CheckIfChanged(incoming, global);
                        global.value = incoming.value;
                        global.changed = incoming.changed;
                        global.expr_changed = incoming.expr_changed;
                        break;
                    }
                }
            }
        }
    }

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoCollapse | 
                                    ImGuiWindowFlags_NoBringToFrontOnFocus;

    // 
    // source code window
    //
    {
        ImGui::SetNextWindowBgAlpha(1.0);   // @Imgui: bug where GetStyleColor doesn't respect window opacity
        ImGui::SetNextWindowPos( { 0, 0 } );
        ImGui::SetNextWindowSize({ SOURCE_WIDTH, WINDOW_HEIGHT });
        ImGui::Begin("Source", NULL, window_flags);

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

        //
        // goto window: jump to a line in the source document
        //

        static bool goto_line_open;
        bool goto_line_activate;
        static int goto_line;

        if ( gui.IsKeyClicked(GLFW_KEY_G, GLFW_MOD_CONTROL) )
        {
            goto_line_open = true;
            goto_line_activate = true;
        }

        bool source_open = prog.frame_idx < prog.frames.size() &&
                           prog.frames[ prog.frame_idx ].file_idx < prog.files.size();
        if (goto_line_open && source_open)
        {
            ImGui::Begin("Goto Line", &goto_line_open);
            if (goto_line_activate) 
            {
                ImGui::SetKeyboardFocusHere(0); // auto click the goto line field
                goto_line_activate = false;
            }

            if (gui.IsKeyClicked(GLFW_KEY_ESCAPE))
                goto_line_open = false;

            if ( ImGui::InputInt("##goto_line", &goto_line, 1, 1, ImGuiInputTextFlags_EnterReturnsTrue) )    
            {
                Frame &this_frame = prog.frames[ prog.frame_idx ];
                size_t linecount = 0;
                if (this_frame.file_idx < prog.files.size())
                {
                    linecount = prog.files[ this_frame.file_idx ].lines.size();
                }

                if (goto_line < 0) goto_line = 0;
                if ((size_t)goto_line >= linecount) goto_line = (linecount > 0) ? linecount - 1 : 0;

                gui.source_found_line_idx = (size_t)goto_line; // @Hack: reuse the search source index for jumping to goto line
                goto_line_open = false;
            }
            ImGui::End();
        }


        //
        // search bar: look for text in source window
        //

        if (gui.source_search_bar_open)
        {
            ImGui::InputText("##source_search",
                             gui.source_search_keyword, 
                             sizeof(gui.source_search_keyword));
            if (source_open)
            {
                size_t dir = 1;
                size_t &this_frame_idx = prog.frames[ prog.frame_idx ].file_idx; 
                File &this_file = prog.files[ this_frame_idx ];
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
            File &file = prog.files[ frame.file_idx ];

            // draw radio button and line offscreen to get the line size
            // TODO: how to do this without drawing offscreen
            float sourcestart = ImGui::GetCursorPosY();
            ImGui::SetCursorPosY(-100);
            float ystartoffscreen = ImGui::GetCursorPosY();
            ImGui::RadioButton("##MeasureRadioHeight", false); ImGui::SameLine(); ImGui::Text("MeasureText");
            float lineheight = ImGui::GetCursorPosY() - ystartoffscreen;
            size_t perscreen = ceilf(ImGui::GetWindowHeight() / lineheight) + 1;
            ImGui::SetCursorPosY(sourcestart);

            // @Optimization: only draw the visible lines then SetCursorPosY to 
            // be lineheight * height per line to set the total scroll

            // display file lines, skip over designated blank zero index to sync
            // up line indices with line numbers
            if (gui.line_display == LineDisplay_Source)
            {
                // automatically scroll to the next executed line if it is far enough away
                // and we've just stopped execution
                if (gui.jump_to_exec_line)
                {
                    gui.jump_to_exec_line = false;
                    size_t linenum = 1 + (ImGui::GetScrollY() / lineheight);
                    if ( !(frame.line >= linenum + 10 && 
                           frame.line <= linenum + perscreen - 10) )
                    {
                        float scroll = lineheight * ((frame.line - 1) - (lineheight / 2));
                        if (scroll < 0.0f) scroll = 0.0f;
                        ImGui::SetScrollY(scroll);
                    }
                }

                // index zero is always skipped, only used to sync up line number and index
                size_t start_idx = 1 + (ImGui::GetScrollY() / lineheight);
                size_t end_idx = GetMin(start_idx + perscreen, file.lines.size());
                if (file.lines.size() > perscreen)
                {
                    // set scrollbar height, -1 because index zero is never drawn
                    ImGui::SetCursorPosY((file.lines.size() - 1) * lineheight); 
                }
                ImGui::SetCursorPosY((start_idx - 1) * lineheight + sourcestart);

                for (size_t i = start_idx; i < end_idx; i++)
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
                            res.addr = ParseHex( GDB_ExtractValue("bkpt.addr", rec) );

                            String fullpath = GDB_ExtractValue("bkpt.fullname", rec);
                            res.file_idx = CreateOrGetFile(fullpath);

                            prog.breakpoints.push_back(res);
                        }
                    }

                    // stop radio button style
                    ImGui::PopStyleColor(4);

                    ImGui::SameLine();
                    ImVec2 textstart = ImGui::GetCursorPos();
                    if (i == frame.line)
                    {
                        ImGui::Selectable(line.c_str(), true);
                    }
                    else
                    {
                        //if (highlight_search_found)
                        //{
                        //    ImColor IM_COL_YELLOW = IM_COL32(255, 255, 0, 255);
                        //    ImGui::TextColored(IM_COL_YELLOW, "%s", line.c_str());
                        //}
                        //else
                        {
                            // @Imgui: ImGui::Text isn't selectable with a caret cursor, lame
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
                                        static size_t hover_line_idx;
                                        static size_t hover_word_idx;
                                        static size_t hover_char_idx;
                                        static size_t hover_num_frames;
                                        static size_t hover_frame_idx;
                                        static String hover_value;

                                        // check to see if we should add the variable
                                        // to the watch variables
                                        if (gui.IsMouseClicked(GLFW_MOUSE_BUTTON_RIGHT))
                                        {
                                            String hover_string(line.data() + word_idx, char_idx - word_idx);
                                            VarObj add = CreateVarObj(hover_string);
                                            prog.watch_vars.push_back(add);
                                            QueryWatchlist();
                                        }

                                        if (hover_word_idx != word_idx || 
                                            hover_char_idx != char_idx || 
                                            hover_line_idx != i || 
                                            hover_num_frames != prog.frames.size() || 
                                            hover_frame_idx != prog.frame_idx)
                                        {
                                            hover_word_idx = word_idx;
                                            hover_char_idx = char_idx;
                                            hover_line_idx = i;
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
                float line_height = ImGui::GetScrollMaxY() / (float)file.lines.size();
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
            else 
            {

                // automatically scroll to the next executed line if it is far enough away
                // and we've just stopped execution
                if (gui.jump_to_exec_line)
                {
                    size_t current_index = (ImGui::GetScrollY() / lineheight);
                    gui.jump_to_exec_line = false;
                    size_t jump_index = 0;
                    for (size_t i = 0; i < gui.line_disasm.size(); i++)
                    {
                        if (gui.line_disasm[i].addr == frame.addr)
                        {
                            jump_index = i; 
                            break;
                        }
                    }

                    if ( !(jump_index >= current_index + 10 && 
                           jump_index <= current_index + perscreen - 10) )
                    {
                        float scroll = lineheight * ((frame.line - 1) - (lineheight / 2));
                        if (scroll < 0.0f) scroll = 0.0f;
                        ImGui::SetScrollY(scroll);
                    }
                    float scroll = lineheight * (jump_index - (lineheight / 2));
                    if (scroll < 0.0f) scroll = 0.0f;
                    ImGui::SetScrollY(scroll);
                }

                size_t start_idx = (ImGui::GetScrollY() / lineheight);
                size_t end_idx = GetMin(start_idx + perscreen, gui.line_disasm.size());
                if (gui.line_disasm.size() > perscreen)
                {
                    ImGui::SetCursorPosY(gui.line_disasm.size() * lineheight); 
                }

                ImGui::SetCursorPosY(start_idx * lineheight + sourcestart);

                // display source window using retrieved disassembly
                size_t src_idx = 0;
                size_t inst_left = 0;
                for (size_t i = start_idx; i < end_idx; i++)
                {
                    const DisassemblyLine &line = gui.line_disasm[i];    

                    if (gui.line_display == LineDisplay_Source_And_Disassembly)
                    {
                        // display source line then all of its instructions below
                        if (inst_left == 0)
                        {
                            while (src_idx < gui.line_disasm_source.size())
                            {
                                size_t lidx = gui.line_disasm_source[src_idx].line_number;
                                inst_left = gui.line_disasm_source[src_idx].num_instructions;
                                if (lidx < file.lines.size())
                                {
                                    ImGui::Text("%s", file.lines[lidx].c_str());
                                }

                                src_idx++;
                                if (inst_left != 0)
                                    break;
                            }
                        }

                        inst_left--;
                    }

                    bool is_set = false;
                    for (Breakpoint &bkpt : prog.breakpoints)
                    {
                        if (bkpt.addr == line.addr && 
                            bkpt.file_idx == frame.file_idx)
                            is_set = true;
                    }

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
                                Breakpoint &bkpt = prog.breakpoints[b];
                                if (bkpt.addr == line.addr && bkpt.file_idx == frame.file_idx)
                                {
                                    // remove breakpoint
                                    tsnprintf(tmpbuf, "-break-delete %zu", bkpt.number);
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
                            tsnprintf(tmpbuf, "-break-insert *0x%" PRIx64, line.addr);
                            GDB_SendBlocking(tmpbuf, rec);

                            Breakpoint res = {};
                            res.number = GDB_ExtractInt("bkpt.number", rec);
                            res.line = GDB_ExtractInt("bkpt.line", rec);
                            res.addr = ParseHex( GDB_ExtractValue("bkpt.addr", rec) );

                            String fullpath = GDB_ExtractValue("bkpt.fullname", rec);
                            res.file_idx = CreateOrGetFile(fullpath);

                            prog.breakpoints.push_back(res);
                        }
                    }

                    // stop radio button style
                    ImGui::PopStyleColor(4);

                    ImGui::SameLine();
                    if (line.addr == frame.addr)
                    {
                        ImGui::Selectable(line.text.c_str(), true);
                    }
                    else
                    {
                        // @Imgui: ImGui::Text isn't selectable with a caret cursor, lame
                        ImGui::Text("%s", line.text.c_str());
                    }


                    if (line.addr == frame.addr)
                    {
                        //@@@
                        //size_t s = gui.source_highlighted_line;
                        //size_t linediff = (s > i) ? s - i : i - s;
                        //if (linediff > 10)
                        //{
                        //    // @Hack: line number is now line_disasm index
                        //    gui.source_highlighted_line = i;
                        //    ImGui::SetScrollHereY();
                        //}
                    }
                }
            }
        }

        if (gui.source_search_bar_open)
            ImGui::EndChild();
        ImGui::End();
    }

    //
    // control window, registers, locals, watch, program control
    //
    {

        ImVec2 control_subwindow_size = ImVec2(400, 200);
        ImGui::SetNextWindowBgAlpha(1.0);   // @Imgui: bug where GetStyleColor doesn't respect window opacity
        ImGui::SetNextWindowPos( ImVec2(SOURCE_WIDTH, 0) );
        ImGui::SetNextWindowSize( ImVec2(WINDOW_WIDTH - SOURCE_WIDTH, WINDOW_HEIGHT) );
        ImGui::Begin("Control", NULL, window_flags);


        // continue
        bool clicked;

        ImGuiDisabled(prog.running, clicked = ImGui::Button("---"));
        if (clicked)
        {
            // jump to program counter line
            gui.jump_to_exec_line = true;
        }

        // start
        ImGui::SameLine();
        bool vs_continue = gui.IsKeyClicked(GLFW_KEY_F5);
        ImGuiDisabled(prog.running, clicked = ImGui::Button("|>") || vs_continue);
        if (clicked)
        {
            if (!prog.started)
            {
                if (gdb.has_exec_run_start)
                {
                    if (0 <= GDB_SendBlocking("-exec-run --start"))
                    {
                        prog.started = true;
                    }
                }
            }
            else
            {
                GDB_SendBlocking("-exec-continue");
                prog.running = true;
                if (prog.frames.size() > 0)
                    prog.frames[0].line = 0; // clear the highlighted line
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
            GDB_SendBlocking("-exec-step", false);
        }

        // step over
        ImGui::SameLine();
        ImGuiDisabled(prog.running, clicked = ImGui::Button("/\\>"));
        if (clicked)
        {
            GDB_SendBlocking("-exec-next", false);
        }

        // step out
        ImGui::SameLine();
        ImGuiDisabled(prog.running, clicked = ImGui::Button("</\\"));
        if (clicked)
        {
            if (prog.frame_idx == prog.frames.size() - 1)
            {
                // GDB error in top frame: "finish" not meaningful in the outermost frame.
                // emulate visual studios by just running the program  
                GDB_SendBlocking("-exec-continue");
            }
            else
            {
                GDB_SendBlocking("-exec-finish", false);
            }
        }

        ImGui::SameLine();
        const char *button_desc =
            "--- = jump to next executed line\n"
            "|>  = start/continue program\n"
            "||  = pause program\n"
            "--> = step into\n"
            "/\\> = step over\n"
            "</\\ = step out)";
        //R"(</\ = step out)";
        DrawHelpMarker(button_desc);

        //
        // console child window
        //
        ImGui::BeginChild("GDB console", control_subwindow_size, false,
                          ImGuiWindowFlags_HorizontalScrollbar);

        for (int i = 0; i < NUM_LOG_ROWS; i++)
        {
            const char *prefix = (prog.log[i].type == ConsoleLineType_UserInput)
                ? "(gdb) " : "";
            ImGui::Text("%s%s", prefix, prog.log[i].text);
        }

        if (prog.log_scroll_to_bottom) 
        {
            ImGui::SetScrollHereY(1.0f);
            prog.log_scroll_to_bottom = false;
        }

        ImGui::EndChild();

        #define CMDSIZE sizeof(Program::input_cmd[0])
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

        if (ImGui::BeginTable("Locals", 2, flags, control_subwindow_size))
        {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Value");
            ImGui::TableHeadersRow();

            Vector<VarObj> &frame_vars = (prog.frame_idx == 0) ? prog.local_vars : prog.other_frame_vars;
            for (size_t i = 0; i < frame_vars.size(); i++)
            {
                const VarObj &iter = frame_vars[i];
                if (iter.value[0] == '{')
                {
                    RecurseExpressionTreeNodes(iter, 0);
                }
                else
                {
                    ImGui::TableNextRow();

                    ImGui::TableNextColumn();
                    ImGui::Text("%s", iter.name.c_str());

                    ImGui::TableNextColumn();
                    ImColor color = (iter.changed)
                        ? IM_COL32_WIN_RED
                        : ImGui::GetStyleColorVec4(ImGuiCol_Text);
                    ImGui::TextColored(color, "%s", iter.value.c_str());
                }
            }

            // empty columns to pad width
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", table_pad);
            ImGui::TableNextColumn();
            ImGui::Text("%s", table_pad);

            ImGui::EndTable();
        }

        if (ImGui::BeginChild("Callstack", control_subwindow_size, true) )
        {
            //ImGui::TableSetupColumn("Function");
            //ImGui::TableSetupColumn("Value");
            //ImGui::TableHeadersRow();

            for (size_t i = 0; i < prog.frames.size(); i++)
            {
                const Frame &iter = prog.frames[i];

                String file = (iter.file_idx < prog.files.size())
                    ? prog.files[ iter.file_idx ].fullpath
                    : "???";

                // @Windows
                // get the filename from the full path
                const char *last_fwd_slash = strrchr(file.c_str(), '/');
                const char *filename = (last_fwd_slash != NULL) ? last_fwd_slash + 1 : file.c_str();

                tsnprintf(tmpbuf, "%4zu %s##%zu", iter.line, filename, i);

                if ( ImGui::Selectable(tmpbuf, i == prog.frame_idx) )
                {
                    prog.frame_idx = i;
                    gui.jump_to_exec_line = true;
                    if (prog.frame_idx != 0)
                    {
                        // do a one-shot query of a non-current frame
                        // prog.frames is stored from bottom to top so need to do size - 1
                        prog.other_frame_vars.clear();
                        tsnprintf(tmpbuf, "-stack-list-variables --frame %zu --thread 1 --all-values", i);
                        GDB_SendBlocking(tmpbuf, rec);
                        const RecordAtom *variables = GDB_ExtractAtom("variables", rec);

                        for (const RecordAtom &atom : GDB_IterChild(rec, variables))
                        {
                            VarObj add = CreateVarObj(GDB_ExtractValue("name", atom, rec),
                                                      GDB_ExtractValue("value", atom, rec));
                            prog.other_frame_vars.emplace_back(add);
                        }

                    }

                    QueryWatchlist();

                    if (gui.line_display != LineDisplay_Source)
                        GetFunctionDisassembly(prog.frames[ prog.frame_idx ]);
                }
            }

            ImGui::EndChild();
        }

        if (ImGui::BeginTable("Registers", 2, flags, control_subwindow_size))
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
                ImColor color = (iter.changed)
                    ? IM_COL32_WIN_RED
                    : ImGui::GetStyleColorVec4(ImGuiCol_Text);
                ImGui::TextColored(color, "%s", iter.value.c_str());
            }

            ImGui::EndTable();
        }

        if (ImGui::BeginTable("Watch", 2, flags, control_subwindow_size))
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
            //             then clicking on a shorter name, the column will 
            //             appear empty until arrow left or clicking
            size_t this_max_name_length = MIN_TABLE_WIDTH_CHARS;
            for (size_t i = 0; i < prog.watch_vars.size(); i++)
            {
                VarObj &iter = prog.watch_vars[i];
                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-FLT_MIN);

                // is column clicked?
                static char editwatch[4096];
                bool column_clicked = false;
                if (i == edit_var_name_idx)
                {
                    if (ImGui::InputText("##edit_watch", editwatch, 
                                         sizeof(editwatch), 
                                         ImGuiInputTextFlags_EnterReturnsTrue,
                                         NULL, NULL))
                    {
                        iter.name = editwatch;
                        QueryWatchlist();
                        memset(editwatch, 0, sizeof(editwatch));
                        edit_var_name_idx = -1;
                    }


                    static int delay = 0; // @Imgui: need a delay or else it will auto de-focus
                    if (focus_name_input)
                    {
                        ImGui::SetKeyboardFocusHere(-1);
                        focus_name_input = false;
                        delay = 0;
                    }
                    else
                    {
                        bool deleted = false;
                        delay++;
                        bool active = ImGui::IsItemFocused() && (delay < 2 || ImGui::GetIO().WantCaptureKeyboard);
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
                    ? IM_COL32_WIN_RED
                    : ImGui::GetStyleColorVec4(ImGuiCol_Text);
                ImGui::TextColored(color, "%s", iter.value.c_str());
            }

            max_name_length = this_max_name_length;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            static char watch[256];

            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::InputText("##create_new_watch", watch, 
                                 sizeof(watch), 
                                 ImGuiInputTextFlags_EnterReturnsTrue,
                                 NULL, NULL))
            {
                VarObj add = CreateVarObj(watch);
                prog.watch_vars.push_back(add);
                QueryWatchlist();
                memset(watch, 0, sizeof(watch));
            }
            ImGui::PopStyleColor();

            ImGui::TableNextColumn();
            ImGui::Text("%s", "");

            // empty columns to pad width
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", table_pad);
            ImGui::TableNextColumn();
            ImGui::Text("%s", table_pad);

            ImGui::EndTable();
        }


        //
        // configuration row of widgets
        //
        {
            // @Hack(ish): all of prog.config needs to be ConfigPair for this to work
            const size_t CONFIG_SIZE = 4096;
            static char config_input[NUM_CONFIG][CONFIG_SIZE];
            static bool show_config_window;
            static_assert(sizeof(prog.config) % sizeof(ConfigPair) == 0, "prog.config not all ConfigPair's");

            ConfigPair *src = reinterpret_cast<ConfigPair *>(&prog.config);

            // configuration window
            if ( ImGui::Button("Config") )
            {
                // set all of the initial values for the config input fields
                show_config_window = true;
                for (size_t i = 0; i < NUM_CONFIG; i++)
                    tsnprintf(config_input[i], "%s", src[i].value.c_str());
            }

            if (show_config_window)
            {
                ImGui::Begin("Tug Configuration", &show_config_window);

                for (size_t i = 0; i < NUM_CONFIG; i++)
                {
                    ConfigPair &iter = src[i];
                    tsnprintf(tmpbuf, "%s##config", iter.key);
                    ImGui::InputText(tmpbuf, &config_input[i][0], CONFIG_SIZE);

                    if (iter.type == ConfigType_File)
                    {
                        static size_t open_index = BAD_INDEX;

                        ImGui::SameLine();
                        tsnprintf(tmpbuf, "...##%zu", i);
                        if (ImGui::Button(tmpbuf))
                            open_index = i;

                        if (open_index == i)
                        {
                            ImGuiFileWindowMode mode = ImGuiFileWindowMode_SelectFile;
                            static FileWindowContext ctx;
                            if ( ImGuiFileWindow(ctx, mode) )
                            {
                                if (ctx.selected)
                                {
                                    tsnprintf(config_input[i], "%s", 
                                              ctx.path.c_str());
                                }
                                open_index = BAD_INDEX;
                            }
                        }
                    }
                }

                if ( ImGui::Button("apply") )
                {
                    show_config_window = false;
                    for (size_t i = 0; i < NUM_CONFIG; i++)
                        src[i].value = config_input[i];

                    FILE *f = fopen("tug.ini", "w");
                    if (f == NULL) 
                    {
                        PrintErrorLibC("fopen tug.ini");
                    }
                    else
                    {
                        // write out key=value pairs, double spaced
                        for (size_t i = 0; i < NUM_CONFIG; i++)
                        {
                            fprintf(f, "%s=%s\n\n", src[i].key, src[i].value.c_str());
                        }        
                        fclose(f);
                    }
                }

                ImGui::End();
            }

            struct RegisterName
            {
                String text;
                bool registered;
            };
            static Vector<RegisterName> all_registers;
            static bool show_add_register_window = false;

            ImGui::SameLine();
            if (ImGui::Button("Modify Tracked Registers##button"))
            {
                show_add_register_window = true;
                all_registers.clear();
                GDB_SendBlocking("-data-list-register-names", rec);
                const RecordAtom *regs = GDB_ExtractAtom("register-names", rec);

                for (const RecordAtom &reg : GDB_IterChild(rec, regs))
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
                ImGui::Begin("Modify Tracked Registers##window", &show_add_register_window);

                for (RegisterName &reg : all_registers)
                {
                    if ( ImGui::Checkbox(reg.text.c_str(), &reg.registered) )
                    {
                        if (reg.registered)
                        {
                            // add register varobj, copied from var creation in async_stopped if statement
                            // the only difference is GLOBAL_NAME_PREFIX and '@' used to signify a varobj
                            // that lasts the duration of the program

                            tsnprintf(tmpbuf, "-var-create " GLOBAL_NAME_PREFIX "%s @ $%s",
                                      reg.text.c_str(), reg.text.c_str());
                            GDB_SendBlocking(tmpbuf, rec);

                            VarObj add = CreateVarObj(reg.text, GDB_ExtractValue("value", rec));
                            prog.global_vars.emplace_back(add);
                        }
                        else
                        {
                            // delete register varobj
                            for (size_t i = 0; i < prog.global_vars.size(); i++)
                            {
                                if (prog.global_vars[i].name == reg.text) 
                                {
                                    prog.global_vars.erase(prog.global_vars.begin() + i,
                                                           prog.global_vars.begin() + i + 1);
                                    tsnprintf(tmpbuf, "-var-delete " GLOBAL_NAME_PREFIX "%s", reg.text.c_str());
                                    GDB_SendBlocking(tmpbuf);
                                }
                            }
                        }
                    }
                }

                ImGui::End();
            }

            // line display: how to present the debugged executable: 
            // source, disassembly, or source-and-disassembly
            LineDisplay last_line_display = gui.line_display;
            ImGui::SameLine();
            ImGui::SetNextItemWidth(160.0f);
            ImGui::Combo("##line_display", 
                         reinterpret_cast<int *>(&gui.line_display),
                         "Source\0Disassembly\0Source And Disassembly\0");


            if (last_line_display == LineDisplay_Source && 
                gui.line_display != LineDisplay_Source &&
                prog.frame_idx < prog.frames.size())
            {
                // query the disassembly for this function
                GetFunctionDisassembly(prog.frames[ prog.frame_idx ]);
            }

            ImGui::Checkbox("Show MI Records", &gui.show_machine_interpreter_commands);
        }

        ImGui::End();
    }

    gui.last_keystate = gui.this_keystate;
    gui.last_mousestate = gui.this_mousestate;
}

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
    Assert(false);
}

int main(int argc, char **argv)
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
    if (!Tug_Init(window, argc, argv))
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
    //ImGui::StyleColorsLight();

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

    if (prog.config.font_filename.value != "")
    {
        float fontsize = atof(prog.config.font_filename.value.c_str());
        if (fontsize == 0) fontsize = 12.0f;
        io.Fonts->AddFontFromFileTTF(prog.config.font_filename.value.c_str(), fontsize);
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


        if (gui.IsKeyClicked(GLFW_KEY_F12))
            Assert(false);

        static bool debug_window_toggled;
        if (gui.IsKeyClicked(GLFW_KEY_F1))
            debug_window_toggled = !debug_window_toggled;

        if (debug_window_toggled)
        {
            char tmp[4096];
            ImDrawList *drawlist = ImGui::GetForegroundDrawList();
            snprintf(tmp, sizeof(tmp), "Mouse Position: (%.1f,%.1f)", io.MousePos.x, io.MousePos.y);
            ImVec2 BR = ImGui::CalcTextSize(tmp);
            ImVec2 TL = { 0, 0 };

            drawlist->AddRectFilled(TL, BR, 0xFFFFFFFF);
            drawlist->AddText(TL, 0xFF000000, tmp);

            snprintf(tmp, sizeof(tmp), "Application average %.3f ms/frame (%.1f FPS)",
                     1000.0f / ImGui::GetIO().Framerate, 
                     ImGui::GetIO().Framerate);

            TL.y = BR.y;
            BR = ImGui::CalcTextSize(tmp);
            BR.x += TL.x;
            BR.y += TL.y;
            drawlist->AddRectFilled(TL, BR, 0xFFFFFFFF);
            drawlist->AddText(TL, 0xFF000000, tmp);

            static bool pinned_point_toggled;
            static ImVec2 pinned_point;
            static ImVec2 pinned_window;

            ImVec2 mousepos = ImGui::GetMousePos();
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            {
                pinned_point_toggled = !pinned_point_toggled;
                if (pinned_point_toggled)
                {
                    // get relative window position
                    ImGuiContext *ctx = ImGui::GetCurrentContext();

                    // windows are stored back to front
                    for (ImGuiWindow *iter : ctx->Windows)
                    {
                        const ImVec2 WINDOW_BR = ImVec2(iter->Pos.x + iter->Size.x,
                                                        iter->Pos.y + iter->Size.y);
                        if ((!iter->Hidden && !iter->Collapsed) &&
                            (iter->Pos.x <= mousepos.x && iter->Pos.y <= mousepos.y) &&
                            (WINDOW_BR.x >= mousepos.x && WINDOW_BR.y >= mousepos.y) )
                        {
                            pinned_window = iter->Pos;
                        }
                    }
                    pinned_point = mousepos;
                }
            }

            if (pinned_point_toggled)
            {
                // draw a rect in the selected window
                const ImVec2 PIN_BR = ImVec2(mousepos.x - pinned_window.x,
                                             mousepos.y - pinned_window.y);
                const ImVec2 PIN_TL = ImVec2(pinned_point.x - pinned_window.x,
                                             pinned_point.y - pinned_window.y);

                uint32_t col = IM_COL32(0, 255, 0, 32);
                drawlist->AddRectFilled(pinned_point, mousepos, col);

                snprintf(tmp, sizeof(tmp), "window rect:\n  pos: (%d, %d)\n  size: (%d, %d)",
                         (int)PIN_TL.x, (int)PIN_TL.y,
                         (int)(PIN_BR.x - PIN_TL.x), (int)(PIN_BR.y - PIN_TL.y));
                BR = ImGui::CalcTextSize(tmp);
                TL = ImVec2(pinned_point.x, pinned_point.y - BR.y);
                BR.x += TL.x;
                BR.y += TL.y;

                drawlist->AddRectFilled(TL, BR, IM_COL32_WHITE);
                drawlist->AddText(TL, IM_COL32_BLACK, tmp);
            }
        }

        //
        // global styles
        //

        // lessen the intensity of selectable hover color
        //ImVec4 active_col = ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive);
        //active_col.x *= (1.0/2.0);
        //active_col.y *= (1.0/2.0);
        //active_col.z *= (1.0/2.0);

        //ImGui::PushStyleColor(ImGuiCol_HeaderHovered, active_col);
        //ImGui::PushStyleColor(ImGuiCol_HeaderActive, active_col);

        // set global colors that change based 
        // upon the luminance of the window background
        static const auto GetLuminance01 = [](ImColor col) -> float
        {
            return (0.2126*col.Value.x) +
                   (0.7152*col.Value.y) +
                   (0.0722*col.Value.z);
        };
        float lum = GetLuminance01( ImGui::GetStyleColorVec4(ImGuiCol_WindowBg) );
        IM_COL32_WIN_RED = ImColor(1.0f, 0.5f - 0.5f*lum, 0.5f - 0.5f*lum, 1.0f);
        //IM_COL32_WIN_GDB_USER_INPUT = ImColor(1.0f, 0.8f, 0.5f - 0.5f*lum);

        // defaults are too damn bright!
        ImVec4 hdr = ImGui::GetStyleColorVec4(ImGuiCol_Header);
        ImVec4 hdr_hovered = ImVec4(hdr.x, hdr.y, hdr.z, GetMin(1.0f, hdr.w + 0.2));
        ImVec4 hdr_active = ImVec4(hdr.x, hdr.y, hdr.z, GetMin(1.0f, hdr.w + 0.4));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, hdr_hovered);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, hdr_active);

        ImVec4 btn = ImGui::GetStyleColorVec4(ImGuiCol_Button);
        ImVec4 btn_hovered = ImVec4(btn.x, btn.y, btn.z, GetMin(1.0f, btn.w + 0.2));
        ImVec4 btn_active = ImVec4(btn.x, btn.y, btn.z, GetMin(1.0f, btn.w + 0.4));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, btn_hovered);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, btn_active);

        Draw(window);

        ImGui::PopStyleColor(4);

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

    Tug_Shutdown();

    // Cleanup
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
