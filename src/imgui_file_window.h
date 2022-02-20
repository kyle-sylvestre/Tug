#pragma once

// bare bones file/directory picker, nothing fancy here
// example of usage
//
// #include "imgui.h"
// #include "imgui_internal.h"  // for ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
//
// #define IMPL_FILE_WINDOW     // include this line in only one cpp file
// #include "imgui_file_window.h"

#include <string>
#include <vector>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <minwindef.h>      // MAX_PATH
#else
#include <linux/limits.h>   // PATH_MAX
#include <strings.h>        // strcasecmp
#define MAX_PATH PATH_MAX   // lmao
#define stricmp strcasecmp
#endif

enum FileWindowMode
{
    FileWindowMode_SelectFile,
    FileWindowMode_SelectDirectory,
    FileWindowMode_WriteFile,
};

struct FileWindowContext 
{
    static const int BAD_INDEX = -1;

    bool selected = false;  // was an entry selected?
    std::string path = "";  // selected entry

    int select_index = FileWindowContext::BAD_INDEX;
    bool window_opened = true;
    bool query_directory = true;
    char user_input[MAX_PATH] = {};
    std::vector<std::string> dirs = {};
    std::vector<std::string> files = {};

    // path history for backward/forward folder jumps
    struct History
    {
        std::vector<std::string> paths = {};
        size_t idx = SIZE_MAX;
    } history;
};

// param[inout] res: 
//     ctx.selected: submitted? true, canceled? false
//     ctx.path: entry that was submitted from the window
// param[in] mode:
//     how the window is presenting the files
// param[in] directory:
//     directory to start in
// param[in] filters:
//     comma separated list of extensions to filter for, "*" for no filtering
//     no spaces, no periods. ex: "cpp,c,h"
//     (only used with FileWindowMode_SelectFile)
// return: bool (window closed)
//     -if submit or cancel has been clicked, return true
//     -else, return false
bool ImFileWindow(FileWindowContext &ctx, FileWindowMode mode, 
                  const char *directory = ".", const char *filters = "*");



#if defined(IMPL_FILE_WINDOW)

#if !defined(IMGUI_VERSION)
#error include imgui and imgui_internal headers above this #include
#endif


#if defined(_WIN32)

#include <Windows.h>

#define PATHSEPSTR "\\"
void OS_GetAbsolutePath(const char *relpath, char (&abspath)[MAX_PATH])
{
    GetFullPathNameA(relpath, sizeof(abspath), abspath, NULL);
    if (INVALID_FILE_ATTRIBUTES == GetFileAttributesA(abspath))
    {
        fprintf(stderr, "file not found: %s", abspath):
    }
}
int OS_FilterFilename(ImGuiInputTextCallbackData *data)
{
    if (data->BufTextLen == 0) return 0;

    char c = data->Buf[data->BufTextLen - 1];
    bool valid_char = 
        c >= 32 && c <= 126 && // not control or DEL
        c != '<' && c != '>' && c != ':' && c != '"' && // invalid windows file chars
        c != '/' && c != '\\' && c != '|' && c != '?' && c != '*';
        
    if (!valid_char)
        data->DeleteChars(data->BufTextLen - 1, 1);

    return 0;
};
void OS_PopulateDirEntries(const std::string &dirpath,
                           std::vector<std::string> &dirs,
                           std::vector<std::string> &files,
                           const char *filters)
{
    // query directory for all its contents
    // trailing '*' in FindFirstFileA means "Find all files of the directory"
    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA( (dirpath + "\\*").c_str(), &find_data);

    if (hFind != INVALID_HANDLE_VALUE)
    {
        do 
        {
            if (find_data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)
                continue; // skip hidden files

            if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                // skip current/parent directory
                const char *ptr = find_data.cFileName;
                bool skip = (ptr[0] == '.' && ptr[1] == '\0') || 
                            (ptr[0] == '.' && ptr[1] == '.' && ptr[2] == '\0')
                if (!skip) 
                    dirs.push_back(find_data.cFileName);
            }
            else 
            {
                // check the file extension against the filters 
                const char *ext = strrchr(ptr, '.');
                if (!ext) continue;
                ext += 1; // offset '.'

                bool add_file = false;
                if (filters[0] == '*') add_file = true;

                // parse the comma separated filters for acceptable file extensions
                char filter_buf[128]; 
                snprintf(filter_buf, sizeof(filter_buf), "%s", filters);

                for (char *iter = strtok(filter_buf, ","); 
                     !add_file && iter != NULL; 
                     iter = strtok(NULL, ","))
                {
                    if (0 == stricmp(iter, ext)) 
                        add_file = true;
                }

                if (add_file)
                    files.push_back(ptr);
            }

        } while (FindNextFileA(hFind, &find_data));
    }
}

#elif defined(__linux__)

#include <dirent.h>
#define PATHSEPSTR "/"
void OS_GetAbsolutePath(const char *relpath, char (&abspath)[MAX_PATH])
{
    char *rc = realpath(relpath, abspath);
    if (rc == NULL)
    {
        fprintf(stderr, "realpath on %s: %s", relpath, strerror(errno));
    }
}
int OS_FilterFilename(ImGuiInputTextCallbackData *data)
{
    if (data->BufTextLen == 0) return 0;

    char c = data->Buf[data->BufTextLen - 1];
    bool valid_char = c != '\0' && c != '/';
        
    if (!valid_char)
        data->DeleteChars(data->BufTextLen - 1, 1);

    return 0;
}
void OS_PopulateDirEntries(const std::string &dirpath,
                           std::vector<std::string> &dirs,
                           std::vector<std::string> &files,
                           const char *filters)
{
    struct dirent *entry;
    DIR *dir;

    dir = opendir(dirpath.c_str());
    if (dir == NULL) 
    {
        fprintf(stderr, "opendir on %s: %s", dirpath.c_str(), strerror(errno));
    }
    else
    {
        // TODO: lstat then S_ISREG and S_ISDIR macros for when d_type isn't supported
        while ((entry = readdir(dir))) 
        {
            if (entry->d_type & DT_DIR)
            {
                // skip current/parent directory
                const char *ptr = entry->d_name;
                bool skip = (ptr[0] == '.' && ptr[1] == '\0') || 
                            (ptr[0] == '.' && ptr[1] == '.' && ptr[2] == '\0');
                if (!skip) 
                    dirs.push_back(entry->d_name);
            }
            else if (entry->d_type & DT_REG)
            {
                // check the file extension against the filters 
                const char *ext = strrchr(entry->d_name, '.');
                if (!ext) continue;
                ext += 1; // offset '.'

                bool add_file = false;
                if (filters[0] == '*') add_file = true;

                // parse the comma separated filters for acceptable file extensions
                char filter_buf[128]; 
                snprintf(filter_buf, sizeof(filter_buf), "%s", filters);

                for (char *iter = strtok(filter_buf, ","); 
                     !add_file && iter != NULL; 
                     iter = strtok(NULL, ","))
                {
                    if (0 == stricmp(iter, ext)) 
                        add_file = true;
                }

                if (add_file)
                    files.push_back(entry->d_name);
            }
        }
        closedir(dir);
    }
}

#endif

struct DirectoryButton
{
    const char *button_text;
    const char *path;
};

const DirectoryButton BOOKMARKS[] = 
{
    { "Downloads", "C:\\Users\\Kyle\\Downloads" },
    { "Videos", "C:\\Users\\Kyle\\Videos" },
    { "Pictures", "C:\\Users\\Kyle\\Pictures" },
};

bool ImFileWindow(FileWindowContext &ctx, FileWindowMode mode, 
                  const char *directory, const char *filters)
{
    // TODO:
    // -displaying the current filters
    // -button for transforming path bar into input box for copy/pasting paths

    bool close_window = false;
    char tmp[MAX_PATH]; // scratch for getting strings

    const char *window_name =
        (mode == FileWindowMode_WriteFile) ? "Write File" :
        (mode == FileWindowMode_SelectDirectory) ? "Open Directory" :
        (mode == FileWindowMode_SelectFile) ? "Open File" : "???";

    ImGui::Begin(window_name);

    int uid = 0; // prevent duplicate names from messing up ImGui objects

    if (ctx.window_opened) 
    {
        ctx.window_opened = false;
        ctx.query_directory = true;
        OS_GetAbsolutePath(directory, tmp);
        ctx.path = std::string(tmp);
    }

    bool submit_disabled;
    if (mode == FileWindowMode_WriteFile)
    {
        // compare the entered extension against the filters list
        // implemented again later in query_directory if branch
        submit_disabled = true;
        const char *ext = strrchr(ctx.user_input, '.');

        if (!ext) 
        {
            // set false earlier
        }
        else if (filters && filters[0] == '*')
        {
            // if there is no save filter, just check for anything past '.'
            submit_disabled = (ext[1] == '\0');
        }
        else 
        {
            ext += 1; // offset '.'

            // parse the comma separated filters for acceptable file extensions
            char filter_buf[128]; 
            snprintf(filter_buf, sizeof(filter_buf), 
                     "%s", filters);

            for (char *iter = strtok(filter_buf, ","); 
                 iter != NULL; 
                 iter = strtok(NULL, ","))
            {
                if (0 == strcmp(iter, ext)) 
                {
                    submit_disabled = false;
                    break;
                }
            }
        }
    }
    else 
    {
        // enable submit once the user has selected a file
        submit_disabled = (ctx.select_index == FileWindowContext::BAD_INDEX);
    }

    bool history_idx_changed = false;

    ImGui::BeginDisabled(ctx.history.idx <= 0 || 
                         ctx.history.idx - 1 >= ctx.history.paths.size());
    bool backward_clicked = ImGui::Button("<--");
    ImGui::EndDisabled();

    if (backward_clicked)
    {
        ctx.history.idx--;
        ctx.path = ctx.history.paths[ctx.history.idx];
        ctx.query_directory = true;
        history_idx_changed = true;
    } 
    ImGui::SameLine();

    ImGui::BeginDisabled(ctx.history.idx + 1 >= ctx.history.paths.size());
    bool forward_clicked = ImGui::Button("-->");
    ImGui::EndDisabled();

    if (forward_clicked) 
    {
        ctx.history.idx++;
        ctx.path = ctx.history.paths[ctx.history.idx];
        history_idx_changed = true;
        ctx.query_directory = true;
    } 
    ImGui::SameLine();


    bool disable_input = mode == FileWindowMode_SelectDirectory ||
                         mode == FileWindowMode_SelectFile;

    // quick access directory buttons
    for (const DirectoryButton &iter : BOOKMARKS)
    {
        ImGui::SameLine();
        if (ImGui::Button(iter.button_text))
        {
            ctx.path = iter.path;
            ctx.query_directory = true;
        }
    }



    const float LOCAL_WIN_WIDTH = ImGui::GetWindowWidth();
    ImGui::Spacing();
    const char PATHSEP = PATHSEPSTR[0];
    size_t dirstart = 0;

    for (size_t i = 0; i < ctx.path.size(); i++)
    {
        char c = ctx.path[i];
        bool last_dir = false;
        if (i == ctx.path.size() - 1)
        {
            // offset by one to get proper dirname length
            last_dir = true;
            i++; 
        }
        else if (c != PATHSEP) 
        {
            continue;
        }

        const char *dirptr = ctx.path.data() + dirstart;
        int dirlength = i - dirstart;

        // hit path separator, get the dirname
        // check if the next button fits on line
        ImVec2 textsize = ImGui::CalcTextSize(dirptr, dirptr + dirlength, true);
        ImVec2 curpos = ImGui::GetCursorPos();
        if (curpos.x + textsize.x > LOCAL_WIN_WIDTH) 
        {
            // move to next line
            curpos.y += textsize.y;
            curpos.x = 0.0f;
            ImGui::SetCursorPos(curpos);
        }

        snprintf(tmp, sizeof(tmp), "%.*s##%d", 
                 dirlength, dirptr, uid++);

        // dirlength of 0 causes the selectable to clobber its right neighbors
        if (dirlength > 0 && ImGui::Selectable(tmp, false, 0, textsize))
        {
            if (!last_dir) // don't jump to current dir
            {
                ctx.path.erase(dirstart + dirlength);
                ctx.query_directory = true;
            }
        }
        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        dirstart = i + 1;
    }
    
    ImGui::Separator();


    const float BOTTOM_BAR_HEIGHT = 40.0f;
    ImVec2 childstart = ImGui::GetCursorPos();
    ImVec2 childsize = ImGui::GetWindowSize();
    childsize.y = (childsize.y - childstart.y - BOTTOM_BAR_HEIGHT);
    childsize.x = 0.0f; // take up the full window width

    // draw active item, select and cancel button at the bottom of the window
    ImVec2 bottomstart = {0.0f, ImGui::GetWindowSize().y - BOTTOM_BAR_HEIGHT };
    ImGui::SetCursorPos(bottomstart);

    ImGui::BeginDisabled(disable_input);
    ImGui::InputText("##active_file_input", ctx.user_input, sizeof(ctx.user_input),
                     ImGuiInputTextFlags_CallbackAlways, 
                     OS_FilterFilename, NULL);
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(submit_disabled);
    bool submit_clicked = ImGui::Button("Submit");
    ImGui::EndDisabled();

    ImGui::SameLine();
    bool cancel_clicked = ImGui::Button("Cancel");

    ImGui::SetCursorPos(childstart);
    ImGui::BeginChild("##folderview", childsize, true);

    // record all the files and folders of the current directory
    if (ctx.query_directory)
    {
        // reset scrollbar to the top
        ImGui::SetScrollY(0.0f);

        // overwrite old entry or add new one
        if (!history_idx_changed)
        {
            if (ctx.history.idx + 1 < ctx.history.paths.size())
            {
                ctx.history.idx++;

                // if the retraced folder step doesn't match, erase the history trail
                if (ctx.history.paths[ctx.history.idx] != ctx.path)
                {
                    size_t erasecount = ctx.history.paths.size() - ctx.history.idx;
                    for (size_t i = 0; i < erasecount; i++)
                        ctx.history.paths.pop_back();
                    ctx.history.paths.push_back(ctx.path);
                }
            }
            else
            {
                ctx.history.paths.push_back(ctx.path);
                ctx.history.idx++;
            }
        }

        ctx.query_directory = false;
        ctx.dirs.clear();
        ctx.files.clear();

        // reset user input for entering a new directory
        memset(ctx.user_input, 0, sizeof(ctx.user_input));
        ctx.select_index = FileWindowContext::BAD_INDEX; 
        OS_PopulateDirEntries(ctx.path, ctx.dirs, ctx.files, filters);
    }

    // draw the directories as buttons 
    int dir_idx = 0;
    for (const auto &dirname : ctx.dirs) 
    {
        std::string file = std::string(ctx.path) + PATHSEPSTR + dirname;

        // radio button for selecting directories 
        if (mode == FileWindowMode_SelectDirectory)
        {
            snprintf(tmp, sizeof(tmp), "##dirbutton_%d", uid++);
            if (ImGui::RadioButton( tmp, &ctx.select_index, dir_idx))
            {
                snprintf(ctx.user_input, sizeof(ctx.user_input), 
                         "%s", dirname.c_str());
            }
            ImGui::SameLine();
        } 

        snprintf(tmp, sizeof(tmp), "%s##%d", dirname.c_str(), uid++);
        if (ImGui::Button(tmp) ) 
        {
            // navigate to the selected button directory
            ctx.path += PATHSEPSTR + dirname;
            OS_GetAbsolutePath(ctx.path.c_str(), tmp);
            ctx.path = std::string(tmp);
            ctx.select_index = FileWindowContext::BAD_INDEX;
            ctx.query_directory = true;
        }
        dir_idx++;
    }

    // draw the files as radio buttons
    if (mode != FileWindowMode_SelectDirectory)
    {
        int idx = 0;
        for (const std::string &filename : ctx.files)
        {
            std::string file = std::string(ctx.path) + PATHSEPSTR + filename;
            if (ImGui::RadioButton(filename.c_str(), &ctx.select_index, idx))
            {
                snprintf(ctx.user_input, sizeof(ctx.user_input), 
                         "%s", filename.c_str());
            } 
            idx += 1;
        }
    }

    if (submit_clicked) 
    {
        std::string preserve = ctx.path + PATHSEPSTR + std::string(ctx.user_input);

        // prepare context for next window call
        ctx = FileWindowContext();
        ctx.path = preserve;
        ctx.selected = true;
        close_window = true;
    }

    if (cancel_clicked)
    {
        // prepare context for next window call
        ctx = FileWindowContext();
        close_window = true;
    }

    ImGui::EndChild();
    ImGui::End();
    return close_window;
}

#endif
