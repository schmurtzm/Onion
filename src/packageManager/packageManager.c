#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>
#include "sys/ioctl.h"
#include <dirent.h>
#include <stdlib.h>

#include "system/keymap_hw.h"
#include "system/keymap_sw.h"
#include "utils/utils.h"
#include "utils/log.h"
#include "utils/msleep.h"
#include "utils/keystate.h"

#ifndef DT_DIR
#define DT_DIR 4
#endif

#define PACKAGE_LAYER_1 "data/Layer1"
#define PACKAGE_LAYER_2 "data/Layer2"
#define PACKAGE_LAYER_3 "data/Layer3"

// Max number of records in the DB
#define NUMBER_OF_LAYERS 200
#define MAX_LAYER_NAME_SIZE 256
#define MAY_LAYER_DISPLAY 35

static char package_names[3][NUMBER_OF_LAYERS][MAX_LAYER_NAME_SIZE];
static bool package_installed[3][NUMBER_OF_LAYERS];
static bool package_changes[3][NUMBER_OF_LAYERS];
static int package_count[3];
static int package_installed_count[] = {0, 0, 0};
static int nSelection = 0;
static int nListPosition = 0;
static int nTab = 0;

static SDL_Surface *video = NULL,
                   *screen = NULL,
                   *surfaceBackground = NULL,
                   *surfaceSelection = NULL,
                   *surfaceTableau = NULL,
                   *surfacesTabSelection = NULL,
                   *surfaceScroller = NULL,
                   *surfaceCheck = NULL,
                   *surfaceCross = NULL;

static TTF_Font *font25 = NULL;
static TTF_Font *font35 = NULL;

static SDL_Color color_white = {255, 255, 255};

void setLayersInstall(bool should_install)
{
    for (int n = 0; n < 3; n++) {
        for (int i = 0 ; i < NUMBER_OF_LAYERS; i++) {
            package_changes[n][i] = package_installed[n][i] ? !should_install : should_install;
        }
    }
}

void appUninstall(char *basePath, int strlenBase)
{
    char path[1000];
    char pathInstalledApp[1000];

    struct dirent *dp;
    DIR *dir = opendir(basePath);

    // Unable to open directory stream
    if (!dir)
        return;

    while ((dp = readdir(dir)) != NULL) {
        if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0) {
            // Construct new path from our base path
            sprintf(path, "%s/%s", basePath, dp->d_name);

            if (exists(path)) {
                sprintf(pathInstalledApp, "/mnt/SDCARD%s", path + strlenBase);

                if (is_file(pathInstalledApp))
                    remove(pathInstalledApp);
                
                if (is_dir(path))
                    appUninstall(path, strlenBase);

                if (is_dir(pathInstalledApp))
                    rmdir(pathInstalledApp);
            }
        }
    }

    closedir(dir);
}

bool checkAppInstalled(const char *basePath, int base_len)
{
    char path[1000];
    char pathInstalledApp[1000];

    struct dirent *dp;
    DIR *dir = opendir(basePath);

    int run = 1;
    bool is_installed = true;

    // Unable to open directory stream
    if (!dir)
        return true;

    while ((dp = readdir(dir)) != NULL && run == 1) {
        if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0) {
            // Construct new path from our base path
            sprintf(path, "%s/%s", basePath, dp->d_name);

            if (exists(path)) {
                sprintf(pathInstalledApp, "/mnt/SDCARD%s", path + base_len);

                if (!exists(pathInstalledApp))
                    is_installed = false;
                else if (dp->d_type == DT_DIR)
                    is_installed = checkAppInstalled(path, base_len);

                if (!is_installed)
                    run = 0;
            }
        }
    }

    closedir(dir);
    return is_installed;
}

void loadResources()
{
    DIR *dp;
    struct dirent *ep;
    char basePath[1000];

    for (int nT = 0; nT < 3; nT++) {
        char data_path[200];
        package_count[nT] = 0;

        switch(nT) {
            case 0: sprintf(data_path, "%s", PACKAGE_LAYER_1); break;
            case 1: sprintf(data_path, "%s", PACKAGE_LAYER_2); break;
            case 2: sprintf(data_path, "%s", PACKAGE_LAYER_3); break;
            default: break;
        }

        if (!exists(data_path) || (dp = opendir(data_path)) == NULL)
            continue;

        while ((ep = readdir(dp)) && package_count[nT] < NUMBER_OF_LAYERS) {
            char cShort[MAX_LAYER_NAME_SIZE];
            strcpy(cShort, ep->d_name);

            const char *file_name = ep->d_name;            
            if (file_name[0] != '.') {
                // Installation check
                sprintf(basePath,"%s/%s", data_path, file_name);

                bool is_installed = checkAppInstalled(basePath, strlen(basePath));
                package_installed[nT][package_count[nT]] = is_installed;
                if (is_installed)
                    package_installed_count[nT]++;
                strcpy(package_names[nT][package_count[nT]], file_name);
                package_count[nT]++;
            }

        }
        
        closedir(dp);
    }

    // Sort function
    for (int nT = 0 ; nT < 3 ; nT ++){
        char tempFolder[MAX_LAYER_NAME_SIZE];
        bool bInstallTemp;

        int bFound = 1;
        while (bFound == 1){
            bFound = 0;
            for (int i = 0 ; i < package_count[nT]-1; i ++){
                if (strcmp(package_names[nT][i], package_names[nT][i+1])>0){

                    strcpy(tempFolder, package_names[nT][i+1]);
                    strcpy(package_names[nT][i+1], package_names[nT][i]);
                    strcpy(package_names[nT][i], tempFolder);

                    bInstallTemp = package_installed[nT][i+1];
                    package_installed[nT][i+1] = package_installed[nT][i];
                    package_installed[nT][i] = bInstallTemp;

                    bFound = 1 ;
                }
            }

        }
    }
}

void displayLayersNames(){
    SDL_Rect rectResName = {35, 92, 80, 20};
    SDL_Surface* surfaceResName;
    for (int i = 0 ; i < 7 ; i++){
        if ((i + nListPosition) < package_count[nTab]) {
            bool package_changed = package_changes[nTab][i + nListPosition];
            char package_name[STR_MAX];
            sprintf(package_name, "%s%c", package_names[nTab][i + nListPosition], package_changed ? '*' : 0);
            surfaceResName = TTF_RenderUTF8_Blended(font25, package_name, color_white);
            rectResName.y = 92 + i * 47;
            SDL_BlitSurface(surfaceResName, NULL, screen, &rectResName);
        }
    }
    SDL_FreeSurface(surfaceResName);
}

void displayLayersInstall(){
    SDL_Rect rectInstall = {600 - surfaceCheck->w, 96};

    for (int i = 0 ; i < 7 ; i++) {
        if ((i + nListPosition) < package_count[nTab]) {
            bool is_installed = package_installed[nTab][i + nListPosition];
            bool should_change = package_changes[nTab][i + nListPosition];
            rectInstall.y = 108 - surfaceCheck->h / 2 + i * 47;
            if (is_installed != should_change)
                SDL_BlitSurface(surfaceCheck, NULL, screen, &rectInstall);
            else
                SDL_BlitSurface(surfaceCross, NULL, screen, &rectInstall);
        }
    }
}

void showScroller()
{
    int shiftY = 0;
    if (package_count[nTab] - 7 > 0)
        shiftY = (int)(nListPosition * 311 / (package_count[nTab] - 7));
    SDL_Rect rectSroller = { 608, 86 + shiftY, 16, 16};
    SDL_BlitSurface(surfaceScroller, NULL, screen, &rectSroller);
}

bool confirmDoNothing(KeyState *keystate)
{
    bool quit = false;
    SDL_Surface* image = IMG_Load("res/confirmDoNothing.png");

    SDL_BlitSurface(image, NULL, screen, NULL);
    SDL_BlitSurface(screen, NULL, video, NULL);
    SDL_Flip(video);

    bool confirm = false;

    while (!quit) {
        if (updateKeystate(keystate, &quit, true)) {
            if (keystate[SW_BTN_A] == PRESSED)
                confirm = true;
            else if (keystate[SW_BTN_A] == RELEASED && confirm)
                quit = true;
            else if (keystate[SW_BTN_B] == PRESSED)
                quit = true;
        }
    }

    SDL_FreeSurface(image);

    return confirm;
}

int main(int argc, char *argv[])
{
    bool show_confirm = false;
    bool reapply_all = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--confirm") == 0)
            show_confirm = true;
        else if (strcmp(argv[i], "--reapply") == 0)
            reapply_all = true;
        else {
            printf("unknown argument: %s\n", argv[i]);
            return EXIT_FAILURE;
        }
    }

    SDL_Init(SDL_INIT_VIDEO);
    SDL_ShowCursor(SDL_DISABLE);
    SDL_EnableKeyRepeat(300, 50);
    TTF_Init();

	video = SDL_SetVideoMode(640,480, 32, SDL_HWSURFACE);
	screen = SDL_CreateRGBSurface(SDL_HWSURFACE, 640,480, 32, 0,0,0,0);

	surfaceBackground = IMG_Load("res/bgApp.png");
	surfaceSelection = IMG_Load("res/selection.png");
	surfaceTableau = IMG_Load("res/tableau.png");
	surfacesTabSelection = IMG_Load("res/selectionTitle.png");
	surfaceScroller = IMG_Load("res/scroller.png");
	surfaceCheck = IMG_Load("/mnt/SDCARD/.tmp_update/res/toggle-on.png");
	surfaceCross = IMG_Load("/mnt/SDCARD/.tmp_update/res/toggle-off.png");

    font25 = TTF_OpenFont("/customer/app/Exo-2-Bold-Italic.ttf", 25);
    font35 = TTF_OpenFont("/customer/app/Exo-2-Bold-Italic.ttf", 35);

    SDL_Surface *loadingScreen = IMG_Load("res/loading.png");
    SDL_BlitSurface(loadingScreen, NULL, screen, NULL);
    SDL_BlitSurface(screen, NULL, video, NULL);
    SDL_Flip(video);
    SDL_FreeSurface(loadingScreen);

    loadResources();

    SDL_Rect rectSelection = {15, 84, 593, 49};
    SDL_Rect rectTabSelection = {15, 59, 199, 26};

    bool quit = false;
    bool state_changed = true;
    KeyState keystate[320] = {0};

    int changes_total = 0;
    int changes_installs = 0;
    int changes_removals = 0;
    bool apply_changes = false;

    while (!quit) {
        if (updateKeystate(keystate, &quit, true)) {
            if (keystate[SW_BTN_RIGHT] >= PRESSED) {
                if (nTab < 2) {
                    nTab++;
                    nSelection = 0;
                    nListPosition = 0;
                    state_changed = true;
                }
            }
            if (keystate[SW_BTN_LEFT] >= PRESSED) {
                if (nTab > 0) {
                    nTab--;
                    nSelection = 0;
                    nListPosition = 0;
                    state_changed = true;
                }
            }

            if (keystate[SW_BTN_R1] >= PRESSED && package_count[nTab] > 0) {
                if ((nListPosition + 14) <package_count[nTab]){
                    nListPosition += 7;
                }
                else if ((nListPosition + 7) <package_count[nTab]){
                    nListPosition = package_count[nTab] - 7;
                    nSelection = 6 ;
                }
                state_changed = true;
            }

            if (keystate[SW_BTN_L1] >= PRESSED && package_count[nTab] > 0) {
                if ((nListPosition - 7) > 0) {
                    nListPosition -= 7;
                }
                else {
                    nListPosition = 0;
                    nSelection = 0;

                }
                state_changed = true;
            }

            if (keystate[SW_BTN_DOWN] >= PRESSED && package_count[nTab] > 0) {
                if (nSelection < 6){
                    nSelection ++;
                }
                else if ((nSelection+nListPosition) < package_count[nTab]-1){
                    nListPosition++;
                }
                state_changed = true;
            }
            if (keystate[SW_BTN_UP] >= PRESSED && package_count[nTab] > 0) {
                if (nSelection > 0){
                    nSelection --;
                }
                else if (nListPosition > 0){
                    nListPosition--;
                }
                state_changed = true;
            }

            if (keystate[SW_BTN_B] == PRESSED || keystate[SW_BTN_START] == PRESSED) {
                if (keystate[SW_BTN_START] == PRESSED)
                    apply_changes = true;

                if (show_confirm) {
                    if (apply_changes) {
                        if (changes_total > 0 || (reapply_all && package_installed_count[0] > 0) || confirmDoNothing(keystate))
                            quit = true;
                    }
                    else if (confirmDoNothing(keystate))
                        quit = true;
                }
                else
                    quit = true;
                state_changed = true;
            }

            if (keystate[SW_BTN_A] == PRESSED && package_count[nTab] > 0) {
                int pos = nListPosition + nSelection;
                if (pos < package_count[nTab]) {
                    bool package_changed = !package_changes[nTab][pos];
                    bool is_installed = package_installed[nTab][pos];

                    package_changes[nTab][pos] = package_changed;

                    if (!is_installed)
                        changes_installs += package_changed ? 1 : -1;
                    else
                        changes_removals += package_changed ? 1 : -1;
                    changes_total += package_changed ? 1 : -1;
                    state_changed = true;
                }
            }
        }

        if (quit)
            break;

        if (state_changed) {
            rectSelection.y = 84 + nSelection * 47;
            rectTabSelection.x = 15 + (199 * nTab);

            SDL_BlitSurface(surfaceBackground, NULL, screen, NULL);
            SDL_BlitSurface(surfacesTabSelection, NULL, screen, &rectTabSelection);
            SDL_BlitSurface(surfaceTableau, NULL, screen, NULL);
            SDL_BlitSurface(surfaceSelection, NULL, screen, &rectSelection);

            if (package_count[nTab] > 0){
                displayLayersNames();
                showScroller();
                displayLayersInstall();
            }

            if (changes_total > 0) {
                char status_str[STR_MAX] = "";
                if (changes_installs > 0)
                    sprintf(status_str, "+%d", changes_installs);
                if (changes_removals > 0) {
                    int len = strlen(status_str);
                    if (len > 0) {
                        strcpy(status_str + len, "  ");
                        len += 2;
                    }
                    sprintf(status_str + len, " −%d", changes_removals);
                }
                SDL_Surface *status = TTF_RenderUTF8_Blended(font25, status_str, color_white);
                SDL_Rect status_rect = {620 - status->w, 30 - status->h / 2};
                SDL_BlitSurface(status, NULL, screen, &status_rect);
                SDL_FreeSurface(status);
            }

            SDL_BlitSurface(screen, NULL, video, NULL);
            SDL_Flip(video);

            state_changed = false;
        }
    }

    if (apply_changes) {
        // installation
        char param1[250];
        char data_path[250];
        char cmd[500];

        SDL_Surface* surfaceBackground = IMG_Load("/mnt/SDCARD/.tmp_update/res/waitingBG.png");
        SDL_Surface* surfaceMessage;

        for (int nT = 0 ; nT < 3 ; nT ++){
            switch(nT) {
                case 0: sprintf(data_path, "%s", PACKAGE_LAYER_1); break;
                case 1: sprintf(data_path, "%s", PACKAGE_LAYER_2); break;
                case 2: sprintf(data_path, "%s", PACKAGE_LAYER_3); break;
                default: break;
            }

            if (!exists(data_path))
                continue;

            sprintf(param1, "/mnt/SDCARD/App/The_Onion_Installer/data/Layer%d", nT + 1);

            SDL_Rect rectMessage = {10, 420 , 603, 48};

            for (int nLayer = 0; nLayer < package_count[nT]; nLayer++) {
                char *package_name = package_names[nT][nLayer];
                bool should_change = package_changes[nT][nLayer];

                if (!reapply_all && !should_change)
                    continue;

                bool is_installed = package_installed[nT][nLayer];
                bool should_install = is_installed != should_change;

                if (should_install) {
                    printf_debug("Installing %s...\n", package_name);
                    SDL_BlitSurface(surfaceBackground, NULL, screen, NULL);

                    surfaceMessage = TTF_RenderUTF8_Blended(font35, package_name, color_white);
                    SDL_BlitSurface(surfaceMessage, NULL, screen, &rectMessage);
                    SDL_FreeSurface(surfaceMessage);

                    SDL_BlitSurface(screen, NULL, video, NULL);
                    SDL_Flip(video);

                    sprintf(cmd, "./install.sh \"%s\" \"%s\"", data_path, package_name);
                    system(cmd);
                }
                else if (is_installed) {
                    printf_debug("Removing %s...\n", package_name);
                    // app uninstallation
                    char pathAppUninstall[1000];
                    sprintf(pathAppUninstall, "%s/%s", data_path, package_name);
                    appUninstall(pathAppUninstall, strlen(pathAppUninstall));
                }
            }

        }
    }

    msleep(200);

    TTF_CloseFont(font25);
    TTF_CloseFont(font35);
    TTF_Quit();
    SDL_FreeSurface(surfaceCheck);
    SDL_FreeSurface(surfaceCross);
    SDL_FreeSurface(surfaceBackground);
    SDL_FreeSurface(surfaceTableau);
    SDL_FreeSurface(surfaceSelection);
    SDL_FreeSurface(surfaceScroller);
    SDL_FreeSurface(surfacesTabSelection);
    SDL_Quit();

    return EXIT_SUCCESS;
}
