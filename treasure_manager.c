#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>

#define MAX_USERNAME 32
#define MAX_CLUE 128
#define TREASURE_FILE "treasures.dat"
#define LOG_FILE "logged_hunt"

// define fixed-size treasure structure
typedef struct {
    char id[16];
    char username[MAX_USERNAME];
    float latitude;
    float longitude;
    char clue[MAX_CLUE];
    int value;
} Treasure;

void log_action(const char *hunt_dir, const char *action) {
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", hunt_dir, LOG_FILE);
    int log_fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (log_fd < 0) {
        perror("open log");
        return;
    }
    dprintf(log_fd, "%s\n", action);
    close(log_fd);

    // Create symbolic link
    char linkname[256];
    snprintf(linkname, sizeof(linkname), "logged_hunt-%s", hunt_dir);
    symlink(path, linkname); // ignore errors for now
}

//helper functions:
int read_float(const char *prompt, float *out) {
    char buffer[128];
    printf("%s", prompt);
    if (!fgets(buffer, sizeof(buffer), stdin)) return 0;
    return sscanf(buffer, "%f", out) == 1;
}

int read_int(const char *prompt, int *out) {
    char buffer[128];
    printf("%s", prompt);
    if (!fgets(buffer, sizeof(buffer), stdin)) return 0;
    return sscanf(buffer, "%d", out) == 1;
}

void read_string(const char *prompt, char *out, size_t max_len) {
    printf("%s", prompt);
    if (fgets(out, max_len, stdin)) {
        out[strcspn(out, "\n")] = 0; // remove newline
    }
}


void add_treasure(const char *hunt_id) {
    char dir_path[256];
    snprintf(dir_path, sizeof(dir_path), "%s", hunt_id);
    mkdir(dir_path, 0755);

    char file_path[512]; // fixed warning
    snprintf(file_path, sizeof(file_path), "%s/%s", dir_path, TREASURE_FILE);

    // determine next available ID
    int fd = open(file_path, O_RDONLY);
    int next_id = 1;
    Treasure temp;
    while (fd >= 0 && read(fd, &temp, sizeof(Treasure)) == sizeof(Treasure)) {
        int current_id = atoi(temp.id);
        if (current_id >= next_id) {
            next_id = current_id + 1;
        }
    }
    if (fd >= 0) close(fd);

    fd = open(file_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        perror("open");
        return;
    }

    // implemented data validation
    Treasure t;
    snprintf(t.id, sizeof(t.id), "%d", next_id);
    printf("Generated Treasure ID: %s\n", t.id);

    read_string("Enter Username: ", t.username, MAX_USERNAME);

    while (!read_float("Enter Latitude: ", &t.latitude)) {
        printf("Invalid input. Please enter a valid floating point number.\n");
    }

    while (!read_float("Enter Longitude: ", &t.longitude)) {
        printf("Invalid input. Please enter a valid floating point number.\n");
    }

    read_string("Enter Clue: ", t.clue, MAX_CLUE);

    while (!read_int("Enter Value: ", &t.value)) {
        printf("Invalid input. Please enter a valid integer.\n");
    }


    write(fd, &t, sizeof(Treasure));
    close(fd);

    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Added treasure %s to hunt %s", t.id, hunt_id);
    log_action(hunt_id, log_msg);

    printf("Treasure added.\n");
}

void list_treasures(const char *hunt_id) {
    char file_path[256];
    snprintf(file_path, sizeof(file_path), "%s/%s", hunt_id, TREASURE_FILE);

    int fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return;
    }

    struct stat st;
    if (stat(file_path, &st) == 0) {
        printf("Hunt: %s\nSize: %ld bytes\nLast modified: %s",
               hunt_id, st.st_size, ctime(&st.st_mtime));
    }

    Treasure t;
    printf("Treasure List:\n");
    while (read(fd, &t, sizeof(Treasure)) == sizeof(Treasure)) {
        printf("ID: %s, User: %s, Lat: %.2f, Long: %.2f, Value: %d\n",
               t.id, t.username, t.latitude, t.longitude, t.value);
    }
    close(fd);

    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Listed treasures in hunt %s", hunt_id);
    log_action(hunt_id, log_msg);
}

void view_treasure(const char *hunt_id, const char *treasure_id) {
    char file_path[256];
    snprintf(file_path, sizeof(file_path), "%s/%s", hunt_id, TREASURE_FILE);
    int fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return;
    }

    Treasure t;
    int found = 0;
    while (read(fd, &t, sizeof(Treasure)) == sizeof(Treasure)) {
        if (strcmp(t.id, treasure_id) == 0) {
            printf("Treasure Details:\n");
            printf("ID: %s\nUser: %s\nLat: %.2f\nLong: %.2f\nClue: %s\nValue: %d\n",
                   t.id, t.username, t.latitude, t.longitude, t.clue, t.value);
            found = 1;
            break;
        }
    }
    close(fd);

    if (!found) {
        printf("Treasure with ID %s not found.\n", treasure_id);
    } else {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Viewed treasure %s in hunt %s", treasure_id, hunt_id);
        log_action(hunt_id, log_msg);
    }
}

void remove_treasure(const char *hunt_id, const char *treasure_id) {
    char file_path[512];
    snprintf(file_path, sizeof(file_path), "%s/%s", hunt_id, TREASURE_FILE);

    int fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return;
    }

    // Read all treasures
    Treasure *all_treasures = NULL;
    int total = 0;
    Treasure t;
    while (read(fd, &t, sizeof(Treasure)) == sizeof(Treasure)) {
        Treasure *new_block = realloc(all_treasures, (total + 1) * sizeof(Treasure));
        if (!new_block) {
            perror("realloc");
            free(all_treasures);
            close(fd);
            return;
        }
        all_treasures = new_block;
        all_treasures[total++] = t;
    }
    close(fd);

    // Filter treasures (remove the one with matching ID)
    Treasure *filtered = malloc(total * sizeof(Treasure));
    if (!filtered) {
        perror("malloc");
        free(all_treasures);
        return;
    }

    int found = 0, new_count = 0;
    for (int i = 0; i < total; ++i) {
        if (strcmp(all_treasures[i].id, treasure_id) == 0) {
            found = 1; // Found the one to remove
        } else {
            filtered[new_count++] = all_treasures[i];
        }
    }

    free(all_treasures);

    if (!found) {
        printf("Treasure ID %s not found.\n", treasure_id);
        free(filtered);
        return;
    }

    // Rewrite file with remaining treasures (this part i think does not exactly work properly)
    fd = open(file_path, O_WRONLY | O_TRUNC);
    if (fd < 0) {
        perror("open write");
        free(filtered);
        return;
    }

    if (new_count > 0) {
        if (write(fd, filtered, new_count * sizeof(Treasure)) != new_count * sizeof(Treasure)) {
            perror("write");
        }
    }

    close(fd);
    free(filtered);

    // Log the removal
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Removed treasure %s from hunt %s", treasure_id, hunt_id);
    log_action(hunt_id, log_msg);

    printf("Treasure removed.\n");
}


void remove_hunt(const char *hunt_id) {
    char file_path[256], log_path[256];
    snprintf(file_path, sizeof(file_path), "%s/%s", hunt_id, TREASURE_FILE);
    snprintf(log_path, sizeof(log_path), "%s/%s", hunt_id, LOG_FILE);

    unlink(file_path);
    unlink(log_path);

    char linkname[256];
    snprintf(linkname, sizeof(linkname), "logged_hunt-%s", hunt_id);
    unlink(linkname);

    rmdir(hunt_id);

    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Removed hunt %s", hunt_id);
    //log_action(".", log_msg); //no longer logs deletion 

    printf("Hunt removed.\n");
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s \n--add <hunt_id> \n--list <hunt_id> \n--view <hunt_id> <treasure_id> \n--remove_treasure <hunt_id> <treasure_id> \n--remove_hunt <hunt_id>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "--add") == 0) {
        add_treasure(argv[2]);
    } else if (strcmp(argv[1], "--list") == 0) {
        list_treasures(argv[2]);
    } else if (strcmp(argv[1], "--view") == 0 && argc == 4) {
        view_treasure(argv[2], argv[3]);
    } else if (strcmp(argv[1], "--remove_treasure") == 0 && argc == 4) {
        remove_treasure(argv[2], argv[3]);
    } else if (strcmp(argv[1], "--remove_hunt") == 0) {
        remove_hunt(argv[2]);
    } else {
        fprintf(stderr, "Unknown or incorrect command\n");
        return 1;
    }
    return 0;
}
