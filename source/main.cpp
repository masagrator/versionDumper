#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>

void Compare() {
    std::map<int64_t, int32_t> old_entries;
    std::map<int64_t, int32_t> new_entries;

    FILE* file = fopen("sdmc:/version_dump_temp.txt", "r");
    if (!file) {
        printf("Couldn't find old file!\n");
        return;
    }
    char text[64];
    fgets(text, 64, file);
    while (fgets(text, 64, file)) {
        int64_t titleid = std::strtoll(&text[0], nullptr, 16);
        int32_t version = std::strtol(&text[50], nullptr, 10);
        old_entries[titleid] = version;
    }
    fclose(file);
    file = fopen("sdmc:/version_dump.txt", "r");
    if (!file) {
        printf("Couldn't find new file!\n");
        return;
    }
    fgets(text, 64, file);
    size_t found = 0;
    while (fgets(text, 64, file)) {
        int64_t titleid = std::strtoll(&text[0], nullptr, 16);
        int32_t version = std::strtol(&text[50], nullptr, 10);
        auto it = old_entries.find(titleid);
        if (it != old_entries.end()) {
            int32_t old_version = (size_t)it->second;
            if (version < old_version) {
                printf("Found older update for %016lX: %d[v%d] -> %d[v%d]\n", titleid, old_version, old_version >> 16, version, version >> 16);
                found++;
                consoleUpdate(NULL);
            }
            else if (version > old_version) {
                printf("Found new update for %016lX: %d[v%d] -> %d[v%d]\n", titleid, old_version, old_version >> 16, version, version >> 16);
                found++;
                consoleUpdate(NULL);
            }
        }
        else printf("%016lX was added!\n", titleid);
    }
    fclose(file);
    if (found == 0) printf("No new changes in updates were found!\n");
}

void Test() {

    AvmVersionListEntry* entries = new AvmVersionListEntry[65536];
    u32 output_count = 0;
    Result rc = avmListVersionList(entries, 65536, &output_count);
    if (R_FAILED(rc)) {
        printf(CONSOLE_RED "avmListVersionList failed: 0x%x.\n" CONSOLE_RESET, rc);
        return;
    }
    else printf("avmListVersionList " CONSOLE_GREEN "success" CONSOLE_RESET ", count: %d.\n", output_count);
    printf("Dumping to file version_dump.txt...\n");
    consoleUpdate(NULL);
    FILE* file = fopen("sdmc:/version_dump.txt", "w");
    if (file) {
        fprintf(file, "id|rightsId|version\n");
        for (size_t i = 0; i < output_count; i++) {
            fprintf(file, "%016lX|00000000000000000000000000000000|%d\n", entries[i].application_id, entries[i].version > entries[i].required ? entries[i].version : entries[i].required);
        }
        fclose(file);
        printf(CONSOLE_GREEN "Dumping succeeded!\n" CONSOLE_RESET);
    }
    else printf(CONSOLE_RED "File creation failed!\n" CONSOLE_RESET);
    delete[] entries;

    
}

// Main program entrypoint
int main(int argc, char* argv[])
{
    // This example uses a text console, as a simple way to output text to the screen.
    // If you want to write a software-rendered graphics application,
    //   take a look at the graphics/simplegfx example, which uses the libnx Framebuffer API instead.
    // If on the other hand you want to write an OpenGL based application,
    //   take a look at the graphics/opengl set of examples, which uses EGL instead.
    avmInitialize();
    consoleInit(NULL);

    // Configure our supported input layout: a single player with standard controller styles
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    // Initialize the default gamepad (which reads handheld mode inputs as well as the first connected controller)
    PadState pad;
    padInitializeDefault(&pad);

    //size_t heap_size = checkAvailableHeap();
    //size_t max_entry_count = (heap_size / sizeof(AvmVersionListEntry)) - 1;

    FILE* file = fopen("sdmc:/version_dump.txt", "r");
    bool old_file_available = false;
    if (file) {
        old_file_available = true;
        fclose(file);
    }
    printf("Press X to dump.\n");
    printf("Press + to exit.\n");

    consoleUpdate(NULL);

    // Other initialization goes here. As a demonstration, we print hello world.

    // Main loop
    while (appletMainLoop())
    {
        // Scan the gamepad. This should be done once for each frame
        padUpdate(&pad);

        // padGetButtonsDown returns the set of buttons that have been
        // newly pressed in this frame compared to the previous one
        u64 kDown = padGetButtonsDown(&pad);

        if (kDown & HidNpadButton_X) {
            if (old_file_available) rename("sdmc:/version_dump.txt", "sdmc:/version_dump_temp.txt");
            Test();
            if (old_file_available) {
                printf("\n---\n\n");
                Compare();
                remove("sdmc:/version_dump_temp.txt");
                printf("\n---\n\n");
            }
            printf("File was saved to sdmc:/version_dump.txt\n");
            printf("Press + to exit.\n");
        }

        if (kDown & HidNpadButton_Plus)
            break; // break in order to return to hbmenu

        // Update the console, sending a new frame to the display
        consoleUpdate(NULL);
    }

    // Deinitialize and clean up resources used by the console (important!)
    consoleExit(NULL);
    avmExit();
    return 0;
}