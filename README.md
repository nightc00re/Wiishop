# Wiishop

### A simple Wii homebrew client for installing game backups from a private server.

Wiishop is an application designed to streamline the process of downloading and installing Wii game backups (WBFS files) directly to a USB drive, making them immediately playable through popular loaders like USB Loader GX.

---

## 🚀 Features

* **Direct-to-USB Installation:** Downloads game data directly onto the user's USB storage device.
* **WBFS Compatibility:** Writes files in the **WBFS format** (and handles splitting large files for FAT32) to ensure games are recognized by **USB Loader GX** and other standard loaders.
* **Custom Server Integration:** Connects to a dedicated **Python server** to browse and fetch game files.

---

## 🛠️ Requirements (For End-Users)

To run Wiishop and successfully load games, users must meet the following criteria:

| Component | Requirement | Purpose |
| :--- | :--- | :--- |
| **Wii Console** | **Softmodded** with the Homebrew Channel installed. | Necessary to launch the application. |
| **USB Drive** | Formatted to **FAT32** (or compatible filesystem). | Where the downloaded game files are saved (in the `/wbfs` folder). |
| **SD Card** | Formatted to **FAT32**. | Where the Wiishop application files (`boot.dol`, `meta.xml`) are stored. |
| **cIOS** | **d2x cIOS** installed (highly recommended). | Required for proper USB device and game access. |
| **Network** | The Wii must be connected to the same local network as the server. | Necessary for the game download. |
| **Loader** | **USB Loader GX** (or similar) installed. | To launch the games that Wiishop installs. |

---

## 💻 Developer / Project Notes

This section outlines the current implementation focus:

### Goal: Seamless USB Installation

To ensure full compatibility with USB Loader GX, the application must perform the following technical steps during the download process:

1.  **Network Handling:** Initialize the Wii's network (using `net_init`) and connect to the Python server via HTTP to stream the WBFS file data.
2.  **USB Filesystem Initialization:** Successfully initialize the USB drive using `fatInitUSB()` with proper cIOS access.
3.  **WBFS Formatting:** Use the **`libwbfs`** library to convert the incoming stream, create the necessary directory structure (`usb:/wbfs/Game Title [GAMEID]/`), and write the data to the USB. This must handle files larger than 4GB by splitting them (e.g., into `.wbfs` and `.wbf1` chunks) if the drive is FAT32.
4.  **Server Address:** The current target server is running at `http://[M1_MACBOOK_IP]:[PORT]`.

### Next Steps

* [ ] Implement network initialization and HTTP GET request handling.
* [ ] Integrate `libfat` and ensure reliable USB writing.
* [ ] Integrate `libwbfs` for proper WBFS creation and file splitting.
* [ ] Design and implement the basic menu/UI (e.g., in GRRLIB or libwiigui) to display available games.

---

***Disclaimer:** This software is intended for use with legally obtained game backups only.*
