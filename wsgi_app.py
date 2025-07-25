import json
from urllib.parse import parse_qs
from list_games import generate_game_list # Import generate_game_list

def application(env, start_response):
    path = env.get('PATH_INFO', '/')

    if path == '/games.json':
        headers = [('Content-Type', 'application/json')]
        start_response('200 OK', headers)

        # Extract search query from URL if present
        query_string = env.get('QUERY_STRING', '')
        query_params = parse_qs(query_string)
        search_query = query_params.get('q', [None])[0] # Get 'q' parameter, default to None

        output_data = generate_game_list(search_query=search_query) # Pass search_query correctly
        return [json.dumps(output_data, indent=2).encode('utf-8')]
    else:
        start_response('404 Not Found', [('Content-Type', 'text/plain')])
        return [b"Not Found"]
