// The MIT License (MIT)
// 
// Copyright (c) 2022 Kyle Sylvestre
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.


#pragma once

// bare bones file/directory picker, nothing fancy here

#include <string>
#include <vector>

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <minwindef.h>      // MAX_PATH
#else
#include <limits.h>   // PATH_MAX
#include <strings.h>        // strcasecmp
#define MAX_PATH PATH_MAX   // lmao
#define stricmp strcasecmp
#endif

enum ImGuiFileWindowMode
{
    ImGuiFileWindowMode_SelectFile,
    ImGuiFileWindowMode_SelectDirectory,
    ImGuiFileWindowMode_WriteFile,
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
    bool show_path_as_input = false;
    char path_input[MAX_PATH] = {};
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
// return: bool (window closed)
//     -if submit or cancel has been clicked, return true
//     -else, return false
inline bool ImGuiFileWindow(FileWindowContext &ctx, ImGuiFileWindowMode mode, 
                            const char *directory = ".", const char *filters = "*");

#if defined(_WIN32)

#include <Windows.h>

#define PATHSEPSTR "\\"
inline bool OS_GetAbsolutePath(const char *relpath, char (&abspath)[MAX_PATH])
{
    bool result;
    GetFullPathNameA(relpath, sizeof(abspath), abspath, NULL);
    if (INVALID_FILE_ATTRIBUTES == GetFileAttributesA(abspath))
    {
        fprintf(stderr, "file not found: %s\n", abspath);
        result = false;
    }
    else
    {
        result = true;
    }
    return result;
}
inline int OS_FilterFilename(ImGuiInputTextCallbackData *data)
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
}
inline void OS_PopulateDirEntries(const std::string &dirpath,
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
                            (ptr[0] == '.' && ptr[1] == '.' && ptr[2] == '\0');
                if (!skip) 
                    dirs.push_back(find_data.cFileName);
            }
            else 
            {
                // check the file extension against the filters 
                bool add_file = false;
                const char *ext = NULL;
                if (filters[0] == '*')
                {
                    add_file = true;
                }
                else if (NULL != (ext = strrchr(find_data.cFileName, '.')))
                {
                    ext += 1; // offset '.'

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
                }

                if (add_file)
                    files.push_back(find_data.cFileName);
            }

        } while (FindNextFileA(hFind, &find_data));
    }
}

#elif defined(__linux__)

#include <dirent.h>
#define PATHSEPSTR "/"
inline bool OS_GetAbsolutePath(const char *relpath, char (&abspath)[MAX_PATH])
{
    bool result;
    char *rc = realpath(relpath, abspath);
    if (rc == NULL)
    {
        fprintf(stderr, "realpath on %s: %s\n", relpath, strerror(errno));
        result = false;
    }
    else
    {
        result = true;
    }
    return result;
}
inline int OS_FilterFilename(ImGuiInputTextCallbackData *data)
{
    if (data->BufTextLen == 0) return 0;

    char c = data->Buf[data->BufTextLen - 1];
    bool valid_char = c != '\0' && c != '/';
        
    if (!valid_char)
        data->DeleteChars(data->BufTextLen - 1, 1);

    return 0;
}
inline void OS_PopulateDirEntries(const std::string &dirpath,
                                  std::vector<std::string> &dirs,
                                  std::vector<std::string> &files,
                                  const char *filters)
{
    struct dirent *entry;
    DIR *dir;

    dir = opendir(dirpath.c_str());
    if (dir == NULL) 
    {
        fprintf(stderr, "opendir on %s: %s\n", dirpath.c_str(), strerror(errno));
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
                bool add_file = false;
                const char *ext = NULL;
                if (filters[0] == '*')
                {
                    add_file = true;
                }
                else if (NULL != (ext = strrchr(entry->d_name, '.')))
                {
                    ext += 1; // offset '.'

                    // parse the comma separated filters for acceptable file extensions
                    char filter_buf[128]; 
                    snprintf(filter_buf, sizeof(filter_buf), "%s", filters);

                    for (char *iter = strtok(filter_buf, ","); 
                         !add_file && iter != NULL; 
                         iter = strtok(NULL, ","))
                    {
                        if (0 == stricmp(iter, ext)) 
                        {
                            add_file = true;
                        }
                    }
                }

                if (add_file)
                    files.push_back(entry->d_name);
            }
        }
        closedir(dir);
    }
}

#endif

inline bool ImGuiFileWindow(FileWindowContext &ctx, ImGuiFileWindowMode mode, 
                            const char *directory, const char *filters)
{
    // TODO:
    // -displaying the current filters

    bool close_window = false;
    char tmp[MAX_PATH]; // scratch for getting strings
    int uid = 0; // prevent duplicate names from messing up ImGui objects

    const char *window_name =
        (mode == ImGuiFileWindowMode_WriteFile) ? "Write File" :
        (mode == ImGuiFileWindowMode_SelectDirectory) ? "Open Directory" :
        (mode == ImGuiFileWindowMode_SelectFile) ? "Open File" : "???";

    if (mode == ImGuiFileWindowMode_SelectDirectory) 
        filters = "";

    if (ctx.window_opened) 
    {
        ctx.window_opened = false;
        ctx.query_directory = true;
        OS_GetAbsolutePath(directory, tmp);
        ctx.path = std::string(tmp);
        ImGui::SetNextWindowSize( ImVec2(700, 400) );
    }

    ImGui::Begin(window_name);

    bool submit_disabled;
    if (mode == ImGuiFileWindowMode_WriteFile)
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
            snprintf(filter_buf, sizeof(filter_buf), "%s", filters);

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
        history_idx_changed = true;
        ctx.history.idx--;
        ctx.path = ctx.history.paths[ctx.history.idx];
        ctx.query_directory = true;
    } 
    ImGui::SameLine();

    ImGui::BeginDisabled(ctx.history.idx + 1 >= ctx.history.paths.size());
    bool forward_clicked = ImGui::Button("-->");
    ImGui::EndDisabled();

    if (forward_clicked) 
    {
        history_idx_changed = true;
        ctx.history.idx++;
        ctx.path = ctx.history.paths[ctx.history.idx];
        ctx.query_directory = true;
    } 


    bool disable_input = mode == ImGuiFileWindowMode_SelectDirectory ||
        mode == ImGuiFileWindowMode_SelectFile;

    ImGui::Spacing();
    ImGui::Spacing();

    const float LOCAL_WIN_WIDTH = ImGui::GetWindowWidth();

    if (ctx.show_path_as_input)
    {
        if ( ImGui::InputText("##imfilewin_path_input", 
                              ctx.path_input, sizeof(ctx.path_input), 
                              ImGuiInputTextFlags_EnterReturnsTrue) )
        {
            ctx.show_path_as_input = false;
            if (OS_GetAbsolutePath(ctx.path_input, tmp))
            {
                ctx.path = tmp;
                ctx.query_directory = true;
            }
        }
    }
    else
    {
        // draw path as a list of selectable text fields, 
        // trailing ... is for converting the path into a text input box
        // C:/Foo/Bar becomes C | Foo | Bar | ...
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
            ImVec2 padsize = ImGui::CalcTextSize("  "); // before/after name space
            textsize.x += padsize.x;

            ImVec2 curpos = ImGui::GetCursorPos();
            if (curpos.x + textsize.x > LOCAL_WIN_WIDTH) 
            {
                // move to next line
                curpos.y += textsize.y;
                curpos.x = 0.0f;
                ImGui::SetCursorPos(curpos);
            }

            snprintf(tmp, sizeof(tmp), " %.*s ##%d", 
                     dirlength, dirptr, uid++);

            // dirlength of 0 causes the selectable to clobber its right neighbors
            if (dirlength > 0)
            {
                ImGui::SameLine();
                if (ImGui::Selectable(tmp, false, 0, textsize) && !last_dir) 
                {
                    ctx.path.erase(dirstart + dirlength);
                    ctx.query_directory = true;
                }

                ImGui::SameLine();
                ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_Separator),
                                   "%s", "|");
            }

            dirstart = i + 1;
        }

        const char *str = " ... "; 
        ImVec2 size = ImGui::CalcTextSize(str);
        ImGui::SameLine();
        if ( ImGui::Selectable(str, false, 0, size) )
        {
            ctx.show_path_as_input = true;
            snprintf(ctx.path_input, sizeof(ctx.path_input), 
                     "%s", ctx.path.c_str());
        }
    }

    ImGui::Spacing();
    ImGui::Spacing();

    const float BOTTOM_BAR_HEIGHT = 40.0f;
    ImVec2 childstart = ImGui::GetCursorPos();
    ImVec2 childsize = ImGui::GetWindowSize();
    childsize.y = (childsize.y - childstart.y - BOTTOM_BAR_HEIGHT);
    childsize.x = 0.0f; // take up the full window width

    // draw active item, select and cancel button at the bottom of the window
    ImVec2 bottomstart = {childstart.x, ImGui::GetWindowSize().y - BOTTOM_BAR_HEIGHT + 10.0f};
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
        ctx.show_path_as_input = false;
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
        if (mode == ImGuiFileWindowMode_SelectDirectory)
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
            ctx.show_path_as_input = false;
        }
        dir_idx++;
    }

    // draw the files as radio buttons
    if (mode != ImGuiFileWindowMode_SelectDirectory)
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
