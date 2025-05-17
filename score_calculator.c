#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TREASURE_FILE "treasures.dat"
#define MAX_USERNAME 32

typedef struct {
    char id[16];
    char username[MAX_USERNAME];
    float latitude;
    float longitude;
    char clue[128];
    int value;
} Treasure;

typedef struct {
    char username[MAX_USERNAME];
    int total_score;
} ScoreEntry;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <hunt_directory>\n", argv[0]);
        return 1;
    }

    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/%s", argv[1], TREASURE_FILE);
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        perror("Failed to open treasure file");
        return 1;
    }

    ScoreEntry scores[100];
    int count = 0;

    Treasure t;
    while (fread(&t, sizeof(Treasure), 1, fp) == 1) {
        int found = 0;
        for (int i = 0; i < count; i++) {
            if (strcmp(scores[i].username, t.username) == 0) {
                scores[i].total_score += t.value;
                found = 1;
                break;
            }
        }
        if (!found && count < 100) {
            strncpy(scores[count].username, t.username, MAX_USERNAME);
            scores[count].total_score = t.value;
            count++;
        }
    }
    fclose(fp);

    for (int i = 0; i < count; i++) {
        printf("%s %d\n", scores[i].username, scores[i].total_score);
    }

    return 0;
}
