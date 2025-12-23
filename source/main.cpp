#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <map>
#include <curl/curl.h>

constexpr char privateKey[] = {
#embed "privateKey.txt"
};

bool screenTurnedOff = false;

void userAppExit(void) {
    if (screenTurnedOff == false) return;
    appletSetMediaPlaybackState(false);
    lblInitialize();
    lblSwitchBacklightOn(1000);
    lblExit();
}

bool Compare() {
    std::map<int64_t, int32_t> old_entries;
    std::map<int64_t, int32_t> new_entries;

    FILE* file = fopen("sdmc:/version_dump_temp.txt", "r");
    if (!file) {
        printf("Couldn't find old file!\n");
        return false;
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
        return false;
    }
    fgets(text, 64, file);
    size_t found = 0;
    size_t added = 0;
    while (fgets(text, 64, file)) {
        int64_t titleid = std::strtoll(&text[0], nullptr, 16);
        int32_t version = std::strtol(&text[50], nullptr, 10);
        auto it = old_entries.find(titleid);
        if (it != old_entries.end()) {
            int32_t old_version = it->second;
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
        else {
            printf("%016lX was added!\n", titleid);
            added++;
        }
    }
    fclose(file);
    if (found == 0) printf("No new changes in updates were found!\n");
    return (found > 0) || (added > 0);
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

// Callback to handle response data
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

void SendIt() {
    if (R_FAILED(nifmInitialize(NifmServiceType_User))) {
        printf("No internet connection detected!\n");
        return;
    }
	u32 dummy = 0;
	NifmInternetConnectionType NifmConnectionType = (NifmInternetConnectionType)-1;
	NifmInternetConnectionStatus NifmConnectionStatus = (NifmInternetConnectionStatus)-1;
	Result rc = nifmGetInternetConnectionStatus(&NifmConnectionType, &dummy, &NifmConnectionStatus);
    nifmExit();
    if (R_FAILED(rc) || NifmConnectionStatus != NifmInternetConnectionStatus_Connected) {
        printf("No internet connection detected!\n");
        return;
    }
    socketInitializeDefault();
    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL* curl = curl_easy_init();
    if (!curl) {
        printf("Curl Easy Init error!\n");
        curl_global_cleanup();
        socketExit();
        return;
    }
    FILE* file = fopen("sdmc:/version_dump.txt", "r");
    fseek(file, 0, 2);
    auto size = ftell(file);
    fseek(file, 0, 0);
    char* dump = (char*)calloc(1, size+1);
    fread(dump, 1, size, file);
    fclose(file);

    std::string file_content = dump;
    free(dump);

    // Base64 encode the content
    std::string encoded_content;
    static const char* base64_chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    int val = 0;
    int valb = 0;
    for (unsigned char c : file_content) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 6) {
            valb -= 6;
            encoded_content.push_back(base64_chars[(val >> valb) & 0x3F]);
        }
    }
    if (valb > 0) {
        encoded_content.push_back(base64_chars[(val << (6 - valb)) & 0x3F]);
    }
    while (encoded_content.size() % 4) {
        encoded_content.push_back('=');
    }
    
    // Build API URL
    std::string url = "https://api.github.com/repos/masagrator/version_dump/contents/version_dump.txt";
    std::string auth = "Authorization: token ";
    auth += privateKey;

    std::string response;
    std::string file_sha;

    curl_slist* headers_get = nullptr;
    headers_get = curl_slist_append(headers_get, "User-Agent: libcurl");
    headers_get = curl_slist_append(headers_get, auth.c_str());

    std::string get_response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers_get);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &get_response);

    curl_easy_perform(curl);
    long get_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &get_code);

    if (get_code == 200) {
        size_t sha_pos = get_response.find("\"sha\"");
        if (sha_pos != std::string::npos) {
            size_t start = get_response.find("\"", sha_pos + 6) + 1;
            size_t end = get_response.find("\"", start);
            file_sha = get_response.substr(start, end - start);
            printf("Found existing file SHA: %s\n", file_sha.c_str());
        }
    }

    curl_slist_free_all(headers_get);

    // Create JSON payload
    std::string json_payload = R"({"message":"Update version_dump.txt","content":")" 
            + encoded_content + R"(","sha":")" + file_sha + R"(","branch":")" + "main" + R"("})";

    curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, auth.c_str());
    headers = curl_slist_append(headers, "User-Agent: libcurl");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    // Perform request
    CURLcode res = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    socketExit();

    if (res != CURLE_OK) {
        printf("CURLE NOT OK! %d\n", res);
    }
    else if (http_code != 201 && http_code != 200) {
        printf("HTTP CODE WRONG! %ld\n", http_code);
        printf("%s\n", response.c_str());
    }
    else printf("SENDING SUCCESSFUL!\n");
    return;
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
    if (old_file_available) {
        printf("Press Y to send it to github.\n");
    }
    printf("Press - to do automatic scan every 5 minute with turned off screen. No way to exit it without pressing HOME menu.\n");

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
            printf("Press Y to send it to github.\n");
            printf("Press + to exit.\n");
        }

        if (kDown & HidNpadButton_Plus)
            break; // break in order to return to hbmenu

        if (old_file_available && (kDown & HidNpadButton_Y))
            SendIt();

        if (kDown & HidNpadButton_Minus) {
            appletSetMediaPlaybackState(true);
            lblInitialize();
            lblSwitchBacklightOff(1000);
            lblExit();
            screenTurnedOff = true;
            while(true) {
                rename("sdmc:/version_dump.txt", "sdmc:/version_dump_temp.txt");
                Test();
                if (Compare()) {
                    SendIt();
                }
                remove("sdmc:/version_dump_temp.txt");
                svcSleepThread(5llu * 60 * 1000 * 1000 * 1000);
            }
        }

        // Update the console, sending a new frame to the display
        consoleUpdate(NULL);
    }

    // Deinitialize and clean up resources used by the console (important!)
    consoleExit(NULL);
    avmExit();
    return 0;
}
