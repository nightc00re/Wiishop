import json
import sqlite3
import os
import sys

# =========================================================================
# Database Configuration
# DB_FILE must be an absolute path or relative to where the script is run.
# We are placing it in the same directory as the script.
# =========================================================================
DB_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "games.db")

# =========================================================================
# Nginx Web Root for Game Files
# This is used for constructing URLs for the Wii app, not for listing files.
# Nginx will serve actual game files from this path.
# =========================================================================
GAME_FILE_WEB_ROOT = "http://localhost:8080/wii_games/" # Base URL for downloads

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

def generate_game_list(search_query=None):
    """
    Generates a list of games from the database, with optional search filtering.
    """
    games_list = []
    conn = None 
    try:
        conn = get_db_connection()
        cursor = conn.cursor()

        sql_query = "SELECT filename, title, url, size_bytes, genre, description FROM games"
        params = []

        if search_query:
            sql_query += " WHERE lower(title) LIKE ? OR lower(filename) LIKE ?"
            search_term = f"%{search_query.lower()}%"
            params = [search_term, search_term]

        sql_query += " ORDER BY title ASC"

        cursor.execute(sql_query, params)
        rows = cursor.fetchall()

        for row in rows:
            game_info = dict(row)
            # Ensure the URL uses the configured web root, not what's in DB if it's relative
            # For now, we assume DB 'url' is absolute and correct.
            games_list.append(game_info)

    except sqlite3.Error as e:
        sys.stderr.write(f"Database error in generate_game_list: {e}\n")
        return {"error": f"Server database error: {e}"}
    except FileNotFoundError as e:
        sys.stderr.write(f"FATAL: {e}\n")
        return {"error": f"Server setup error: {e}"}
    finally:
        if conn:
            conn.close()

    return {"games": games_list}

if __name__ == "__main__":
    output = generate_game_list() # Call without search query for full list
    print(json.dumps(output, indent=2))
