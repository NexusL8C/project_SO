#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

#define CMD_FILE "monitor_cmd.txt"
#define HUNTS_DIR "hunts"
#define TREASURE_FILE "treasures.dat"
#define RECORD_SIZE (sizeof(char)*16 + sizeof(char)*32 + sizeof(float)*2 + sizeof(char)*128 + sizeof(int))

volatile sig_atomic_t got_cmd = 0;
volatile sig_atomic_t shutdown_flag = 0;

// Monitor PID
pid_t monitor_pid = -1;

void handle_signal(int sig) {
    if (sig == SIGUSR1) {
        got_cmd = 1;
    } else if (sig == SIGUSR2) {
        shutdown_flag = 1;
        got_cmd = 1;
    }
}

void setup_signal_handlers() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGUSR1, &sa, NULL) == -1 || sigaction(SIGUSR2, &sa, NULL) == -1) {
        perror("[Monitor] sigaction");
        exit(EXIT_FAILURE);
    }
}

void write_command(const char *cmd) {
    FILE *fp = fopen(CMD_FILE, "w");
    if (!fp) {
        perror("[Hub] fopen cmd file");
        return;
    }
    fprintf(fp, "%s\n", cmd);
    fclose(fp);
}

void process_command(const char *cmd) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("[Monitor] fork");
        return;
    }
    if (pid == 0) {
        // Child: handle commands
        if (strncmp(cmd, "list_hunts", strlen("list_hunts")) == 0) {
            DIR *dir = opendir(HUNTS_DIR);
            if (!dir) {
                perror("[Monitor] opendir hunts");
                exit(EXIT_FAILURE);
            }
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                char full_path[512];
                snprintf(full_path, sizeof(full_path), "%s/%s", HUNTS_DIR, entry->d_name);
                struct stat st;
                if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode) && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0){
                    char path[512];
                    snprintf(path, sizeof(path), "%s/%s/%s", HUNTS_DIR, entry->d_name, TREASURE_FILE);
                    struct stat st;
                    int count = 0;
                    if (stat(path, &st) == 0) {
                        count = st.st_size / RECORD_SIZE;
                    }
                    printf("%s: %d treasure(s)\n", entry->d_name, count);
                }
            }
            closedir(dir);
            exit(EXIT_SUCCESS);
        } else if (strncmp(cmd, "list_treasures", strlen("list_treasures")) == 0) {
            char hunt_id[128];
            if (sscanf(cmd, "list_treasures %127s", hunt_id) == 1) {
                char path[256];
                snprintf(path, sizeof(path), "%s/%s", HUNTS_DIR"/", hunt_id);
                char file[512];
                snprintf(file, sizeof(file), "%s/%s", path, TREASURE_FILE);
                execlp("./treasure_manager", "treasure_manager", "--list", path, NULL);
                perror("execlp list_treasures");
            }
        } else if (strncmp(cmd, "view_treasure", strlen("view_treasure")) == 0) {
            char hunt_id[128], treasure_id[128];
            if (sscanf(cmd, "view_treasure %127s %127s", hunt_id, treasure_id) == 2) {
                char path[256];
                snprintf(path, sizeof(path), "%s/%s", HUNTS_DIR, hunt_id);
                execlp("./treasure_manager", "treasure_manager", "--view", path, treasure_id, NULL);
                perror("execlp view_treasure");
            }
        } else if (strncmp(cmd, "stop_monitor", strlen("stop_monitor")) == 0) {
            sleep(2);
            exit(EXIT_SUCCESS);
        } else {
            fprintf(stderr, "[Monitor] Unknown command: %s\n", cmd);
        }
        exit(EXIT_FAILURE);
    } else {
        int status;
        waitpid(pid, &status, 0);
    }
    unlink(CMD_FILE);
}

void simulate_monitor_loop() {
    setup_signal_handlers();
    printf("[Monitor] PID %d ready. Waiting for commands...\n", getpid());
    while (1) {
        pause();
        if (got_cmd) {
            got_cmd = 0;
            FILE *fp = fopen(CMD_FILE, "r");
            if (fp) {
                char buf[256];
                if (fgets(buf, sizeof(buf), fp)) {
                    buf[strcspn(buf, "\n")] = '\0';
                    process_command(buf);
                }
                fclose(fp);
            } else {
                perror("[Monitor] fopen cmd file");
            }
            if (shutdown_flag) {
                printf("[Monitor] Shutting down...\n");
                exit(EXIT_SUCCESS);
            }
        }
    }
}

int main() {
    char line[256];
    printf("hub> ");
    while (fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\n")] = '\0';
        if (strncmp(line, "start_monitor", strlen("start_monitor")) == 0) {
            if (monitor_pid > 0) {
                printf("[Hub] Monitor already running (PID %d)\n", monitor_pid);
            } else {
                monitor_pid = fork();
                if (monitor_pid < 0) perror("fork");
                else if (monitor_pid == 0) simulate_monitor_loop();
                else printf("[Hub] Started monitor with PID %d\n", monitor_pid);
            }
        } else if (strncmp(line, "list_hunts", strlen("list_hunts")) == 0) {
            if (monitor_pid < 0) printf("[Hub] Monitor not running. Use 'start_monitor'.\n");
            else { write_command(line); kill(monitor_pid, SIGUSR1); }
        } else if (strncmp(line, "list_treasures", strlen("list_treasures")) == 0) {
            if (monitor_pid < 0) printf("[Hub] Monitor not running. Use 'start_monitor'.\n");
            else { write_command(line); kill(monitor_pid, SIGUSR1); }
        } else if (strncmp(line, "view_treasure", strlen("view_treasure")) == 0) {
            if (monitor_pid < 0) printf("[Hub] Monitor not running. Use 'start_monitor'.\n");
            else { write_command(line); kill(monitor_pid, SIGUSR1); }
        } else if (strncmp(line, "stop_monitor", strlen("stop_monitor")) == 0) {
            if (monitor_pid < 0) printf("[Hub] Monitor not running.\n");
            else {
                write_command(line);
                kill(monitor_pid, SIGUSR2);
                int status;
                waitpid(monitor_pid, &status, 0);
                printf("[Hub] Monitor exited with status %d\n", WEXITSTATUS(status));
                monitor_pid = -1;
            }
        } else if (strncmp(line, "exit", strlen("exit")) == 0) {
            if (monitor_pid > 0) printf("[Hub] Monitor still running. Use 'stop_monitor' first.\n");
            else break;
        } else {
            printf("[Hub] Unknown command: %s\n", line);
        }
        printf("hub> "); //nice print formatting does not really work properly
    }
    return 0;
}

//as of currently, treasures have to be manually inserted into the hunts directory via an hunts/ before treasure name