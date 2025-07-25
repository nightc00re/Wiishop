import sqlite3
import hashlib
import os
import sys # For sys.stderr.write

# =========================================================================
# Configuration - IMPORTANT: Adjust these paths
# =========================================================================
# Path to your SQLite database file
DB_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "games.db")
# Path to the directory where your actual game files (.wbfs, .iso) are stored
GAME_FILES_DIR = "/opt/homebrew/var/www/wii_games" # Ensure this is correct

# =========================================================================

def get_db_connection():
    """Establishes and returns a connection to the SQLite database."""
    abs_db_path = os.path.abspath(DB_FILE)
    sys.stderr.write(f"Attempting to connect to database at: {abs_db_path}\n")
    if not os.path.exists(abs_db_path):
        sys.stderr.write(f"FATAL ERROR: Database file DOES NOT EXIST at: {abs_db_path}\n")
        raise FileNotFoundError(f"Database file not found at: {abs_db_path}")

    conn = sqlite3.connect(DB_FILE)
    conn.row_factory = sqlite3.Row
    return conn

def calculate_sha256(file_path, chunk_size=8192):
    """Calculates the SHA256 hash of a file."""
    sha256_hash = hashlib.sha256()
    try:
        with open(file_path, "rb") as f:
            for byte_block in iter(lambda: f.read(chunk_size), b""):
                sha256_hash.update(byte_block)
        return sha256_hash.hexdigest()
    except FileNotFoundError:
        sys.stderr.write(f"File not found for hashing: {file_path}\n")
        return None
    except IOError as e:
        sys.stderr.write(f"I/O error during hashing {file_path}: {e}\n")
        return None

def update_game_hashes():
    """
    Iterates through games in the database, calculates/updates their SHA256 hashes,
    and adds new games found in GAME_FILES_DIR.
    """
    conn = None
    try:
        conn = get_db_connection()
        cursor = conn.cursor()

        # First, update hashes for existing games and find new ones
        for filename in os.listdir(GAME_FILES_DIR):
            if os.path.isfile(os.path.join(GAME_FILES_DIR, filename)):
                game_path = os.path.join(GAME_FILES_DIR, filename)
                game_size = os.path.getsize(game_path)

                # Check if game already exists and has a hash
                cursor.execute("SELECT id, sha256_hash FROM games WHERE filename = ?", (filename,))
                existing_game = cursor.fetchone()

                calculated_hash = calculate_sha256(game_path)
                if calculated_hash is None: # Hashing failed
                    continue

                if existing_game:
                    if existing_game["sha256_hash"] != calculated_hash:
                        sys.stderr.write(f"Updating hash for {filename}: old={existing_game['sha256_hash']}, new={calculated_hash}\n")
                        cursor.execute("UPDATE games SET sha256_hash = ?, size_bytes = ? WHERE id = ?", 
                                       (calculated_hash, game_size, existing_game["id"]))
                    else:
                        sys.stderr.write(f"Hash for {filename} is up to date.\n")
                else:
                    # Add new game to DB (you'd typically do this via an admin panel, but for now)
                    sys.stderr.write(f"Adding new game {filename} to database.\n")
                    game_title = os.path.splitext(filename)[0].replace("_", " ")
                    game_url = f"{GAME_FILE_WEB_ROOT}{filename}"
                    cursor.execute("INSERT INTO games (filename, title, url, size_bytes, sha256_hash) VALUES (?, ?, ?, ?, ?)",
                                   (filename, game_title, game_url, game_size, calculated_hash))

        conn.commit()
        sys.stderr.write("Game hash update and new game scan complete.\n")

    except sqlite3.Error as e:
        sys.stderr.write(f"Database error in update_game_hashes: {e}\n")
    except FileNotFoundError as e:
        sys.stderr.write(f"FATAL: {e}\n")
    finally:
        if conn:
            conn.close()

if __name__ == "__main__":
    sys.stderr.write("Starting hash update script...\n")
    update_game_hashes()
    sys.stderr.write("Hash update script finished.\n")
