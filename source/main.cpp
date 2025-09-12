#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

size_t checkAvailableHeap() {
	size_t startSize = 200 * 1024 * 1024;
	void* allocation = malloc(startSize);
	while (allocation) {
		free(allocation);
		startSize += 1024 * 1024;
		allocation = malloc(startSize);
	}
	return startSize - (1024 * 1024);
}

void Test(size_t max_entry_count) {

    AvmVersionListEntry* entries = new AvmVersionListEntry[max_entry_count];
    u32 output_count = 0;
    Result rc = avmListVersionList(entries, max_entry_count, &output_count);
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

    size_t heap_size = checkAvailableHeap();
    size_t max_entry_count = (heap_size / sizeof(AvmVersionListEntry)) - 1;
    printf("Available possible entries to dump: %ld\n", max_entry_count);

    printf("Start Y to begin.\n");
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


        if (kDown & HidNpadButton_Y) {
            Test(max_entry_count);
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