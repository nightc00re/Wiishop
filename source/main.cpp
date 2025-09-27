#include <gctypes.h>
#include <stdio.h>
#include <stdarg.h>
#include <ogc/console.h>
#include <ogc/system.h>
#include <time.h>
#include <stdlib.h>
#include <gccore.h>
#include <gctypes.h>
#include <wiiuse/wpad.h>
#include <string.h>
#include <fat.h>
#include <network.h> // *** NEW: Required for net_init() and if_config() ***

#define TEXTWIN_W 80
#define TEXTWIN_H 60

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
    {"Mario Kart Wii", "RMCE01", 3758096384ULL, false},
    {"New Super Mario Bros. Wii", "SMNP01", 4613734144ULL, false},
    {"Super Mario Galaxy", "SMGE01", 3758096384ULL, false},
    {"Super Smash Bros. Brawl", "RSBE01", 7922432000ULL, true},
    {"The Legend of Zelda: Twilight Princess", "RZDE01", 3758096384ULL, false}, 
    {"Wii Sports Resort", "RSPE01", 3435973836ULL, false},
    {"Donkey Kong Country Returns", "RMKE01", 4613734144ULL, false},
    {"Metroid Prime 3: Corruption", "RM3E01", 4294967296ULL, false},
    {"Super Mario Galaxy 2", "SMQJ01", 4000000000ULL, false},
    {"Mario Party 8", "RM8E01", 3500000000ULL, false}
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


// --- NEW FUNCTION: The client-side download logic will live here ---
void download_game(const GameEntry *game)
{
    // This function will contain the net_socket, net_connect, net_write (HTTP Request)
    // and net_recv (Data Stream) logic.
    PrintCentre(20, "Initiating Download (Logic TBD)...");
    PrintCentre(21, game->title);
    
    // Placeholder for actual network I/O
    for(int i = 0; i < 50; i++) VIDEO_WaitVSync(); 
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