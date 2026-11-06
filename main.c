#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "DX-TRACK/Tracker.h"
#include "cJSON/cJSON.h"

#define BUFFER_SIZE 32768
#define METRICS_FILE "metrics.json"

extern int InitTrackerEngine(void);
extern void ShutdownTrackerEngine(void);

// Global state buffer to hold active root path prefixes parsed from JSON configuration
static char g_active_root_path[MAX_PATH] = { 0 };

// Helper function to extract only the filename component from a full path string
const char* get_filename_only(const char* path) {
    if (!path) return "unknown";
    const char* filename = path;
    while (*path) {
        if (*path == '\\' || *path == '/') {
            filename = path + 1;
        }
        path++;
    }
    return filename;
}

void print_help(const char* exe_name) {
    const char* clean_exe = get_filename_only(exe_name);
    printf("=========================================================================\n");
    printf("              DC2PHP WRAPPER: FILE ARGS INTERFACES & DIRECT PASSTHROUGH  \n");
    printf("=========================================================================\n");
    printf("Usage:\n");
    printf("  %s -bat <script.bat> [args...]   Runs a .bat file echo/print in php.exe.\n", clean_exe);
    printf("  %s -exe <script.exe> [args...]   Runs an .exe file echo/print in php.exe.\n", clean_exe);
    printf("  %s -json <config.json>           Runs a sequence of PHP files parsed from JSON.\n", clean_exe);
    printf("  %s -pass [php_flags...]          Forwards everything directly to php.exe.\n", clean_exe);
    printf("  %s -h | --help                   Displays this usage manual reference.\n\n", clean_exe);
    printf("Notes:\n");
    printf("  - All routing uses short filenames for windows.\n");
    printf("  - When using -pass, all options after the pass go straight to php.exe.\n");
    printf("=========================================================================\n");
    fflush(stdout);
}

void increment_metrics(void) {
    FILE* f = NULL;
    char* file_content = NULL;
    long file_size = 0;
    int app_hits = 0;
    int sess_hits = 0;

    if (fopen_s(&f, METRICS_FILE, "rb") == 0 && f != NULL) {
        fseek(f, 0, SEEK_END);
        file_size = ftell(f);
        fseek(f, 0, SEEK_SET);

        if (file_size > 0) {
            file_content = (char*)malloc(file_size + 1);
            if (file_content) {
                size_t read_bytes = fread(file_content, 1, file_size, f);
                file_content[read_bytes] = '\0';

                cJSON* root = cJSON_Parse(file_content);
                if (root) {
                    cJSON* app = cJSON_GetObjectItemCaseSensitive(root, "application_hits");
                    cJSON* sess = cJSON_GetObjectItemCaseSensitive(root, "session_hits");
                    
                    if (cJSON_IsNumber(app)) app_hits = app->valueint;
                    if (cJSON_IsNumber(sess)) sess_hits = sess->valueint;
                    
                    cJSON_Delete(root);
                }
                free(file_content);
            }
        }
        fclose(f);
    }

    app_hits++;
    sess_hits++;

    cJSON* root = cJSON_CreateObject();
    if (!root) return;

    cJSON_AddNumberToObject(root, "application_hits", app_hits);
    cJSON_AddNumberToObject(root, "session_hits", sess_hits);
    
    char* rendered = cJSON_Print(root);
    
    if (rendered) {
        if (fopen_s(&f, METRICS_FILE, "w") == 0 && f != NULL) {
            fprintf(f, "%s\n", rendered);
            fclose(f);
        }
        cJSON_free(rendered);
    }
    cJSON_Delete(root);
}

void run_private_php_session(const char* target_file, int is_passthrough, int arg_start, int argc, char* argv[]) {
    HANDLE hChildStd_IN_Rd = NULL, hChildStd_IN_Wr = NULL;
    HANDLE hChildStd_OUT_Rd = NULL, hChildStd_OUT_Wr = NULL;

    SECURITY_ATTRIBUTES saAttr = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    if (!CreatePipe(&hChildStd_OUT_Rd, &hChildStd_OUT_Wr, &saAttr, 0)) return;
    SetHandleInformation(hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0);
    if (!CreatePipe(&hChildStd_IN_Rd, &hChildStd_IN_Wr, &saAttr, 0)) return;
    SetHandleInformation(hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0);

    PROCESS_INFORMATION piProcInfo;
    STARTUPINFO siStartInfo;
    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));

    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.hStdError = hChildStd_OUT_Wr;  
    siStartInfo.hStdOutput = hChildStd_OUT_Wr; 
    siStartInfo.hStdInput = hChildStd_IN_Rd;   
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    siStartInfo.wShowWindow = SW_HIDE;

    char* cmdLine = (char*)malloc(BUFFER_SIZE);
    if (!cmdLine) return;
    ZeroMemory(cmdLine, BUFFER_SIZE);

    if (is_passthrough) {
        strcpy_s(cmdLine, BUFFER_SIZE, "php.exe");
        for (int i = arg_start; i < argc; i++) {
            strcat_s(cmdLine, BUFFER_SIZE, " ");
            strcat_s(cmdLine, BUFFER_SIZE, argv[i]);
        }
    } else {
        sprintf_s(cmdLine, BUFFER_SIZE, "php.exe -f \"%s\"", target_file);
        if (arg_start < argc && argv != NULL) {
            strcat_s(cmdLine, BUFFER_SIZE, " --");
            for (int i = arg_start; i < argc; i++) {
                if (argv[i] != NULL) {
                    strcat_s(cmdLine, BUFFER_SIZE, " ");
                    strcat_s(cmdLine, BUFFER_SIZE, argv[i]);
                }
            }
        }
    }

    printf("[WRAPPER] Launching private background worker session...\n");
    if (is_passthrough) {
        printf("[CMDLINE] Passthrough routing active.\n");
    } else {
        printf("[CMDLINE] Routing execution file target: %s\n", target_file);
    }
    fflush(stdout);

    if (!CreateProcessA(NULL, cmdLine, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &siStartInfo, &piProcInfo)) {
        fprintf(stderr, "[CRITICAL] Failed to execute php.exe background engine.\n");
        free(cmdLine);
        return;
    }

    CloseHandle(hChildStd_OUT_Wr);
    CloseHandle(hChildStd_IN_Wr);
    free(cmdLine);

    increment_metrics();

    printf("[ENGINE RESPONSE]:\n---------------------------------------\n");
    fflush(stdout);

    DWORD dwRead;
    char chBuf[BUFFER_SIZE];
    
    while (ReadFile(hChildStd_OUT_Rd, chBuf, sizeof(chBuf), &dwRead, NULL) && dwRead != 0) {
        fwrite(chBuf, 1, dwRead, stdout);
        fflush(stdout);
    }
    printf("\n---------------------------------------\n");

    WaitForSingleObject(piProcInfo.hProcess, INFINITE);
    CloseHandle(piProcInfo.hProcess);
    CloseHandle(piProcInfo.hThread);
    CloseHandle(hChildStd_OUT_Rd);
}
void execute_script_target(const char* script_path, const char* args_string) {
    if (!script_path) return;

    char resolved_path[MAX_PATH];
    ZeroMemory(resolved_path, sizeof(resolved_path));

    int is_absolute = (script_path[0] != '\0' && script_path[1] == ':') || (script_path[0] == '\\') || (script_path[0] == '/');

    if (is_absolute || strlen(g_active_root_path) == 0) {
        strcpy_s(resolved_path, sizeof(resolved_path), script_path);
    } else {
        strcpy_s(resolved_path, sizeof(resolved_path), g_active_root_path);
        size_t len = strlen(resolved_path);
        if (len > 0 && resolved_path[len - 1] != '\\' && resolved_path[len - 1] != '/') {
            strcat_s(resolved_path, sizeof(resolved_path), "\\");
        }
        strcat_s(resolved_path, sizeof(resolved_path), script_path);
    }

    // Tokenize argument string if present
    char* php_args[16] = { NULL };
    int arg_count = 0;

    if (args_string && strlen(args_string) > 0) {
        char args_buf[MAX_PATH];
        strcpy_s(args_buf, sizeof(args_buf), args_string);
        
        char* next_token = NULL;
        char* token = strtok_s(args_buf, "&", &next_token);
        while (token && arg_count < 15) {
            php_args[arg_count++] = token;
            token = strtok_s(NULL, "&", &next_token);
        }
    }

    // Setup CreateProcess and build command line with '--' arguments
    HANDLE hChildStd_IN_Rd = NULL, hChildStd_IN_Wr = NULL;
    HANDLE hChildStd_OUT_Rd = NULL, hChildStd_OUT_Wr = NULL;

    SECURITY_ATTRIBUTES saAttr = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    if (!CreatePipe(&hChildStd_OUT_Rd, &hChildStd_OUT_Wr, &saAttr, 0)) return;
    SetHandleInformation(hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0);
    if (!CreatePipe(&hChildStd_IN_Rd, &hChildStd_IN_Wr, &saAttr, 0)) return;
    SetHandleInformation(hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0);

    PROCESS_INFORMATION piProcInfo;
    STARTUPINFO siStartInfo;
    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));

    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.hStdError = hChildStd_OUT_Wr;  
    siStartInfo.hStdOutput = hChildStd_OUT_Wr; 
    siStartInfo.hStdInput = hChildStd_IN_Rd;   
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    siStartInfo.wShowWindow = SW_HIDE;

    char* cmdLine = (char*)malloc(BUFFER_SIZE);
    if (!cmdLine) return;
    ZeroMemory(cmdLine, BUFFER_SIZE);

    if (arg_count > 0) {
        sprintf_s(cmdLine, BUFFER_SIZE, "php.exe -f \"%s\" --", resolved_path);
        for (int i = 0; i < arg_count; i++) {
            strcat_s(cmdLine, BUFFER_SIZE, " ");
            strcat_s(cmdLine, BUFFER_SIZE, php_args[i]);
        }
    } else {
        sprintf_s(cmdLine, BUFFER_SIZE, "php.exe -f \"%s\"", resolved_path);
    }

    printf("[WRAPPER] Launching private background worker session...\n");
    printf("[CMDLINE] Routing target: %s\n", resolved_path);
    fflush(stdout);

    if (!CreateProcessA(NULL, cmdLine, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &siStartInfo, &piProcInfo)) {
        fprintf(stderr, "[CRITICAL] Failed to execute php.exe background engine.\n");
        free(cmdLine);
        return;
    }

    CloseHandle(hChildStd_OUT_Wr);
    CloseHandle(hChildStd_IN_Wr);
    free(cmdLine);

    increment_metrics();

    printf("[ENGINE RESPONSE]:\n---------------------------------------\n");
    fflush(stdout);

    DWORD dwRead;
    char chBuf[BUFFER_SIZE];
    while (ReadFile(hChildStd_OUT_Rd, chBuf, sizeof(chBuf), &dwRead, NULL) && dwRead != 0) {
        fwrite(chBuf, 1, dwRead, stdout);
        fflush(stdout);
    }
    printf("\n---------------------------------------\n");

    WaitForSingleObject(piProcInfo.hProcess, INFINITE);
    CloseHandle(piProcInfo.hProcess);
    CloseHandle(piProcInfo.hThread);
    CloseHandle(hChildStd_OUT_Rd);
}

// Recursive parsing function with whitelisted execution keys and metadata bypass
void process_json_element(const cJSON* element) {
    if (!element) return;

if (cJSON_IsString(element)) {
        char raw_path_buf[MAX_PATH];
        strcpy_s(raw_path_buf, sizeof(raw_path_buf), element->valuestring);

        char script_path[MAX_PATH];
        char args_str[MAX_PATH];
        ZeroMemory(script_path, sizeof(script_path));
        ZeroMemory(args_str, sizeof(args_str));

        char* q_ptr = strchr(raw_path_buf, '?');
        if (q_ptr) {
            size_t p_len = q_ptr - raw_path_buf;
            strncpy_s(script_path, sizeof(script_path), raw_path_buf, p_len);
            strcpy_s(args_str, sizeof(args_str), q_ptr + 1);
        } else {
            strcpy_s(script_path, sizeof(script_path), raw_path_buf);
        }

        // Pass args_str so step=boot&mode=clean gets tokenized into argv
        execute_script_target(script_path, strlen(args_str) > 0 ? args_str : NULL);
    } 
    else if (cJSON_IsArray(element)) {
        cJSON* child = NULL;
        cJSON_ArrayForEach(child, element) {
            process_json_element(child);
        }
    } 
    else if (cJSON_IsObject(element)) {
        // Check if this object represents an explicit structured script node { "script": "...", "args": "..." }
        cJSON* script_item = cJSON_GetObjectItemCaseSensitive(element, "script");
        cJSON* args_item = cJSON_GetObjectItemCaseSensitive(element, "args");

        if (cJSON_IsString(script_item) && script_item->valuestring) {
            const char* arg_str = (cJSON_IsString(args_item)) ? args_item->valuestring : NULL;
            execute_script_target(script_item->valuestring, arg_str);
            return;
        }

        // Handle root configuration if declared
        cJSON* root_item = cJSON_GetObjectItemCaseSensitive(element, "root");
        char old_root[MAX_PATH];
        strcpy_s(old_root, sizeof(old_root), g_active_root_path);

        if (cJSON_IsString(root_item) && root_item->valuestring) {
            strcpy_s(g_active_root_path, sizeof(g_active_root_path), root_item->valuestring);
            printf("[PATH PARSER] Active root context updated -> %s\n", g_active_root_path);
        }

        // Whitelist execution keys approach: Only traverse valid task containers, completely ignoring metadata keys
        const char* valid_execution_keys[] = { "scripts", "pipeline", "tasks", "actions", "steps" };
        for (int i = 0; i < sizeof(valid_execution_keys) / sizeof(valid_execution_keys[0]); i++) {
            cJSON* target_node = cJSON_GetObjectItemCaseSensitive(element, valid_execution_keys[i]);
            if (target_node) {
                process_json_element(target_node);
            }
        }

        strcpy_s(g_active_root_path, sizeof(g_active_root_path), old_root);
    }
}

void parse_and_run_json_config(const char* json_path) {
    char abs_path[MAX_PATH];
    _fullpath(abs_path, json_path, MAX_PATH);

    FILE* f = NULL;
    if (fopen_s(&f, json_path, "rb") != 0 || f == NULL) {
        fprintf(stderr, "[CRITICAL] Failed to open config file: %s\n", json_path);
        fprintf(stderr, "[DEBUG] Resolved absolute path attempted: %s\n", abs_path);
        return;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(f);
        fprintf(stderr, "[ERROR] Config file empty: %s\n", json_path);
        return;
    }

    char* buffer = (char*)malloc(file_size + 1);
    if (!buffer) {
        fclose(f);
        return;
    }

    size_t read_bytes = fread(buffer, 1, file_size, f);
    buffer[read_bytes] = '\0';
    fclose(f);

    cJSON* root = cJSON_Parse(buffer);
    free(buffer);

    if (!root) {
        fprintf(stderr, "[CRITICAL] Invalid JSON syntax in config file.\n");
        return;
    }

    // Reset root context
    ZeroMemory(g_active_root_path, sizeof(g_active_root_path));

    printf("[JSON HOST] Commencing recursive root & path parsing pipeline...\n");
    process_json_element(root);
    cJSON_Delete(root);
}

int main(int argc, char* argv[]) {
    const char* target_file = NULL;
    const char* json_config_file = NULL;
    int is_passthrough = 0;
    int arg_start = 0;
    const char* exe_name = (argc > 0) ? argv[0] : "dc2php.exe";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help(exe_name);
            return 0;
        }
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-pass") == 0) {
            is_passthrough = 1;
            arg_start = i + 1;
            break;
        } else if (strcmp(argv[i], "-bat") == 0 && (i + 1 < argc)) {
            target_file = argv[i + 1];
            arg_start = i + 2;
            break;
        } else if (strcmp(argv[i], "-exe") == 0 && (i + 1 < argc)) {
            target_file = argv[i + 1];
            arg_start = i + 2;
            break;
        } else if (strcmp(argv[i], "-json") == 0 && (i + 1 < argc)) {
            json_config_file = argv[i + 1];
            break;
        }
    }

    if (!target_file && !is_passthrough && !json_config_file) {
        print_help(exe_name);
        return 1;
    }

    printf("[TRACKER] Bootstrapping global matrix allocation registers...\n");
    fflush(stdout);
    if (!InitTrackerEngine()) return 1;

    if (json_config_file) {
        parse_and_run_json_config(json_config_file);
    } else {
        if (!is_passthrough) {
            printf("[HOST] Target execution file verified: %s\n", get_filename_only(target_file));
        }
        run_private_php_session(target_file, is_passthrough, arg_start, argc, argv);
    }

    printf("[TRACKER] Cleaning up window context tracking handles...\n");
    fflush(stdout);
    ShutdownTrackerEngine();
    
    printf("[WRAPPER] Execution lifecycle completed cleanly.\n");
    return 0;
}