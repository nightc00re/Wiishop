#include <gctypes.h>
#include <stdio.h>
#include <stdarg.h>
#include <ogc/console.h>
#include <ogc/system.h>
#include <time.h> // Required for struct timeval
#include <stdlib.h>
#include <gccore.h>
#include <gctypes.h>
#include <wiiuse/wpad.h>
#include <string.h>
#include <fat.h>
#include <network.h> // Required for net_init() and if_config()
#include <sdcard/wiisd_io.h> // Required for __io_wiisd

#define TEXTWIN_W 80
#define TEXTWIN_H 60
#define CONNECT_TIMEOUT_SEC 10 // 10 seconds timeout

// --- GAME DATA STRUCTURE AND LIST ---
#define MAX_GAMES 10

typedef struct {
    const char *title;
    const char *id;           // The 6-character game ID (e.g., "RSBE01")
    u64 size_bytes;       // The total size in bytes for free space checking
    bool is_dual_layer;   // True for games > 4GB like Brawl (needs split files on FAT32)
} GameEntry;

// --- INITIAL 10 FIRST-PARTY GAME LIST ---
GameEntry game_list[MAX_GAMES] = {
    {"Mario Kart Wii", "RMCE01", 2776629248ULL, false},
    {"New Super Mario Bros. Wii", "SMNP01", 4613734144ULL, false},
    {"Super Mario Galaxy", "SMGE01", 3758096384ULL, false},
    {"Super Smash Bros. Brawl", "RSBE01", 7922432000ULL, true},
    {"The Legend of Zelda: Twilight Princess", "RZDE01", 3758096384ULL, false}, 
    {"Wii Sports Resort", "RSPE01", 3435973836ULL, false},
    {"Donkey Kong Country Returns", "RMKE01", 4613734144ULL, false},
    {"Metroid Prime 3: Corruption", "RM3E01", 4294967296ULL, false},
    {"Super Mario Galaxy 2", "SMQJ01", 4000000000ULL, false},
    {"Test Download 50MB", "TEST01", 52428800ULL, false}, // 52,428,800 bytes = 50 MB
};
// ---------------------------------------

/// InputManager code start

float IN_WiiMoteX;
float IN_WiiMoteY;
u32 IN_ButtonsDown;
bool IN_bIsWiiMoteConnected;
ir_t IN_wii_pointer;

void IN_Init()
{
    WPAD_Init();

    WPAD_SetDataFormat(WPAD_CHAN_0,WPAD_FMT_BTNS_ACC_IR);
    IN_bIsWiiMoteConnected=false;
    IN_WiiMoteX=IN_WiiMoteY=0;
}

void IN_Update()
{
    WPAD_ScanPads();
    WPADData* data=WPAD_Data(0);
    // WPAD_ButtonsDown(0) is used for menu presses to avoid rapid scrolling
    IN_ButtonsDown=WPAD_ButtonsDown(0); 

    if(data->data_present)
    {
        IN_bIsWiiMoteConnected=true;
        IN_wii_pointer=data->ir;
    }
    else
    {
        IN_bIsWiiMoteConnected=false;
        IN_WiiMoteX=IN_WiiMoteY=0;
    }
}

///Inputmanager code End

// --- Print functions are mostly unchanged ---
void PrintMsg(const char* pMsg)
{
    printf("\n%s",pMsg);
}

void PrintMsg(int x,int y,const char* pMsg)
{
    printf ("\x1b[%d;%dH %s", y, x,pMsg);
}

void PrintCentre(int y,const char* pMsg)
{
    int len=strlen(pMsg);
    len>>=1;
    len=((TEXTWIN_W>>1)-len);
    printf ("\x1b[%d;%dH %s", y, len,pMsg );
}

void InitRnd()
{
    srand(time(NULL));
}

int Rnd(int value)
{
    return rand()%value;
}

int Rnd(int minVal,int maxVal)
{
    return (rand()%(maxVal-minVal)+minVal);
}

static void *xfb = nullptr;
static GXRModeObj *rmode = nullptr;
int selected_game_index = 0; // Global menu tracker

// --- ADDED: Permanent buffer for network status, visible every frame ---
static char permanent_status_msg[80] = "Waiting for network...";
// --- ADDED: Flag to track if network config failed (for printing line 11) ---
static bool network_failed = false;


// --- NEW FUNCTION: The client-side download logic (FIXED FOR COMPILATION AND HANG) ---
void download_game(const GameEntry *game)
{
    // --- 1. SETUP & ALL VARIABLE DECLARATIONS (MOVED TO TOP FOR GOTO COMPLIANCE) ---
    s32 client_socket = -1;
    
    // Assumed server location (Update if necessary)
    const char *SERVER_IP = "192.168.1.79"; 
    const u16 SERVER_PORT = 8080;
    
    // Buffers and Logic
    char recv_buffer[4096];
    char http_request[512];
    FILE *fp = NULL;
    int bytes_read = 0;
    
    // Total file size for progress bar calculation
    u64 file_total_size = game->size_bytes; 
    u64 file_bytes_received = 0;

    // --- VARIABLES RELATED TO HEADER PARSING MOVED HERE (Fixes the C++ goto error) ---
    char header_buffer[4096 * 2] = {0}; 
    int header_bytes_read = 0;
    char *body_start = NULL;
    int header_len = 0; 
    int data_len = 0; 
    
    // Paths
    char filepath[64];
    sprintf(filepath, "sd:/wbfs/%s.wbfs", game->id);
    // --- END VARIABLE MOVEMENT ---

    // ADD THE DISPLAY BUFFER HERE (around line 167 of the final code block)
    char display_buffer[80]; 

    PrintCentre(18, "Connecting to server...");
    
    // --- 2. SOCKET, TIMEOUT, AND CONNECTION (NO CHANGES HERE) ---
    client_socket = net_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (client_socket < 0) { 
        PrintCentre(19, "ERROR: Failed to create socket.");
        for(int i = 0; i < 180; i++) VIDEO_WaitVSync(); 
        return;
    }

    struct timeval tv;
    tv.tv_sec = CONNECT_TIMEOUT_SEC;
    tv.tv_usec = 0; 
    net_setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    net_setsockopt(client_socket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    if (net_connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        PrintCentre(19, "ERROR: Connect failed/timed out!");
        PrintCentre(20, SERVER_IP);
        goto cleanup; 
    }

    // --- 3. SEND FULL HTTP GET REQUEST (NO CHANGES HERE) ---
    
    sprintf(http_request, "GET /%s.wbfs HTTP/1.0\r\nHost: %s\r\n\r\n", game->id, SERVER_IP);
    
    PrintCentre(19, "Sending request for file:");
    PrintCentre(20, game->id);

    if (net_write(client_socket, http_request, strlen(http_request)) < 0) {
        PrintCentre(21, "ERROR: Failed to send request.");
        goto cleanup; 
    }
    
    PrintCentre(21, "Request sent. Waiting for data...");

    // --- 4. RECEIVE AND STRIP HTTP HEADER (FIXED LOGIC) ---
    
    fatMountSimple("sd", &__io_wiisd); 
    mkdir("sd:/wbfs", 0777); 
    
    fp = fopen(filepath, "wb"); 
    if (fp == NULL) {
        PrintCentre(22, "ERROR: Could not create file on SD!");
        goto cleanup; 
    }

    // --- A. Read until the end of the header is found ---
    while (!body_start) {
        char chunk[1024];
        int chunk_read = net_read(client_socket, chunk, sizeof(chunk));
        
        if (chunk_read <= 0) {
             PrintCentre(22, "ERROR: Failed to receive full header data!");
             goto cleanup; 
        }

        if (header_bytes_read + chunk_read > (int)sizeof(header_buffer)) {
             PrintCentre(22, "ERROR: Header too large!");
             goto cleanup;
        }

        memcpy(header_buffer + header_bytes_read, chunk, chunk_read);
        header_bytes_read += chunk_read;
        
        body_start = strstr(header_buffer, "\r\n\r\n");
    }

    // --- B. Extract and Write Initial File Data ---
    header_len = (body_start - header_buffer) + 4; 
    data_len = header_bytes_read - header_len;     

    if (data_len > 0) {
        fwrite(body_start + 4, 1, data_len, fp);
        file_bytes_received += data_len;
    }
    
    PrintCentre(21, "Download started..."); 

    // --- C. MAIN DOWNLOAD LOOP (STREAM DATA BODY ONLY) ---
    while ((bytes_read = net_read(client_socket, recv_buffer, sizeof(recv_buffer))) > 0) {
        
        fwrite(recv_buffer, 1, bytes_read, fp);
        file_bytes_received += bytes_read;

        // *** HANG FIX: Check if we've received the expected amount of data ***
        if (file_bytes_received >= file_total_size) {
            PrintCentre(22, "NOTICE: Received expected file size. Breaking read loop.");
            break; // Forcefully break out of the loop
        }
        
        // --- PROGRESS BAR LOGIC (UNCHANGED) ---
        if (file_total_size > 0) {
            int percent = (int)(((u64)file_bytes_received * 100) / file_total_size);
            if (percent > 100) percent = 100;

            int blocks = (percent * 40) / 100;
            char progress_bar[41];
            memset(progress_bar, '#', blocks);
            memset(progress_bar + blocks, '-', 40 - blocks);
            progress_bar[40] = '\0'; 

            sprintf(display_buffer, "%3d%% | [%s]", percent, progress_bar);
            PrintCentre(22, display_buffer);
        } else {
            sprintf(display_buffer, "Downloaded: %llu KB...", file_bytes_received / 1024);
           PrintCentre(22, display_buffer);
        }
        
        VIDEO_WaitVSync(); 
    }

    // --- 5. CLEANUP AND COMPLETION ---
    cleanup:
    
    if (bytes_read < 0) {
        PrintCentre(22, "Download failed (Net Read Error)!");
    } else {
        sprintf(recv_buffer, "Download complete: %llu KB saved.", file_bytes_received / 1024);
        PrintCentre(22, recv_buffer);
    }
    
    if (fp) fclose(fp); 
    if (client_socket >= 0) net_close(client_socket); 
    
    for(int i = 0; i < 300; i++) VIDEO_WaitVSync(); 

    // Clear the download status area
    PrintCentre(18, "                                  "); 
    PrintCentre(19, "                                  "); 
    PrintCentre(20, "                                  "); 
    PrintCentre(21, "                                  "); 
    PrintCentre(22, "Press A to Download | Press HOME to Exit");
}
// ------------------------------------------------------------------

int main(int argc,char **argv)
{
    // --- NEW: Network variables declared inside main ---
    s32 ret;
    char localip[16] = {0};
    char gateway[16] = {0};
    char netmask[16] = {0};
    const int menu_x_start = 10;
    // ----------------------------------------------------
    
    VIDEO_Init();
    rmode = VIDEO_GetPreferredMode(nullptr);

    // Allocate frame buffer
    xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    
    // Temporary buffer for status messages (Only used briefly during init)
    char status_msg[80];
    
    // Initialize the console to print text to the screen
    console_init(xfb, 20, 20, rmode->fbWidth-20, rmode->xfbHeight-20, rmode->fbWidth * VI_DISPLAY_PIX_SZ);

    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(xfb);
    VIDEO_ClearFrameBuffer(rmode, xfb, COLOR_BLACK);

    VIDEO_SetBlack(false);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if(rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();

    IN_Init();

    // --- 1. FILE SYSTEM INITIALIZATION (SD CARD) ---
    bool sd_ok = fatInitDefault(); 
    if (!sd_ok) {
        PrintCentre(5, "FATAL ERROR: SD Card not found. Cannot run.");
        PrintCentre(6, "Please insert a FAT32-formatted SD card.");
        while(1) VIDEO_WaitVSync(); // Freeze on critical error
    }

    // --- 2. NETWORK INITIALIZATION ---
    // net_init() is implicitly called by if_config()
    PrintCentre(9, "Configuring network...");

    // Configure the network interface (true = non-blocking, 20 = timeout seconds)
    ret = if_config(localip, netmask, gateway, true, 20); 
    
    if (ret >= 0) {
        // --- CORRECTED LOGIC ---
        // 1. Format the string into the temporary buffer
        sprintf(status_msg, "Network configured. IP: %s", localip); 
        // 2. Save the result to the PERMANENT global buffer
        strcpy(permanent_status_msg, status_msg);
        // 3. Call PrintCentre with only two arguments
        PrintCentre(10, permanent_status_msg);
        // --- END CORRECTED LOGIC ---
    } else {
        // Save the error status permanently
        strcpy(permanent_status_msg, "ERROR: Network config failed!");
        network_failed = true;
        
        PrintCentre(10, permanent_status_msg);
        PrintCentre(11, "Check Wi-Fi settings on Wii/Dolphin.");
    }
    // ------------------------------------

    // --- MAIN GAME LOOP ---
    while(1)
    {
        IN_Update();
        consoleClear();
        
        // --- RE-PRINT PERSISTENT NETWORK STATUS EVERY FRAME ---
        PrintCentre(10, permanent_status_msg);
        if (network_failed) {
            // Only prints line 11 if the network config failed
            PrintCentre(11, "Check Wi-Fi settings on Wii/Dolphin.");
        }
        // ---------------------------------------------------
        
        // --- D-Pad Menu Navigation Logic (FIXED TO USE WPAD_ButtonsHeld for scrolling) ---
        u32 held = WPAD_ButtonsHeld(0); // Get buttons currently held down

          if (held & WPAD_BUTTON_LEFT) {// Check the held state
            selected_game_index--;
            if (selected_game_index < 0) selected_game_index = MAX_GAMES - 1; 
        }
         if (held & WPAD_BUTTON_RIGHT) {  // Check the held state
            selected_game_index++;
            if (selected_game_index >= MAX_GAMES) selected_game_index = 0;
        }
        
        // --- HOME Button Exit Logic (Still uses IN_ButtonsDown for single press) ---
        if (IN_ButtonsDown & WPAD_BUTTON_HOME) {
            // Shut down network safely before exit
            net_deinit(); 
            PrintCentre(20, "Exiting to Wii Menu...");
            VIDEO_WaitVSync();
            SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0); 
        }
        
        // --- DISPLAY MENU ---
        PrintCentre(2,"WiiShop Homebrew Game Downloader");
        PrintCentre(4,"--------------------------------");

        int menu_y = 6;
        for (int i = 0; i < MAX_GAMES; i++) {
            const char *prefix = (i == selected_game_index) ? ">>" : "  ";
            // Uses printf with console escape sequences for positioning
            printf ("\x1b[%d;%dH %s %s", menu_y + i, menu_x_start, prefix, game_list[i].title); 
        }
        
        // Display status message
        PrintCentre(22, "Press A to Download | Press HOME to Exit");

        // --- SELECTION Logic (A BUTTON) ---
        if (IN_ButtonsDown & WPAD_BUTTON_A) {
            // Call the download function with the selected game data
            download_game(&game_list[selected_game_index]);
        }
        
        // --- Crucial Step: Wait for the next vertical sync ---
        VIDEO_WaitVSync();
    }
    // Network cleanup is handled by the HOME button exit logic
    return 0; 
}