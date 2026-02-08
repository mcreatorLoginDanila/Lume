"""
HTP Server - serves .htp files on port 8080
Run: python htpserver.py
"""

import http.server
import os

PORT = 8080
DIRECTORY = "."

class HTPHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=DIRECTORY, **kwargs)

    def do_GET(self):
        ua = self.headers.get('User-Agent', 'Unknown')
        print(f"[Request] {self.command} {self.path} from {ua}")

        path = self.path
        if path == '/':
            path = '/index.htp'

        filepath = os.path.join(DIRECTORY, path.lstrip('/'))

        if os.path.isfile(filepath):
            try:
                with open(filepath, 'r', encoding='utf-8') as f:
                    content = f.read()

                self.send_response(200)
                if filepath.endswith('.htp'):
                    self.send_header('Content-Type', 'text/htp; charset=utf-8')
                else:
                    self.send_header('Content-Type', 'application/octet-stream')

                content_bytes = content.encode('utf-8')
                self.send_header('Content-Length', str(len(content_bytes)))
                self.send_header('Connection', 'close')
                self.send_header('Server', 'HTPServer/1.0')
                self.end_headers()
                self.wfile.write(content_bytes)
                print(f"  -> 200 OK ({len(content_bytes)} bytes)")

            except Exception as e:
                self.send_error(500, str(e))
                print(f"  -> 500 Error: {e}")
        else:
            err = f'@page {{ title: "404"; background: #1a0000; }}\n'
            err += f'@block {{ align: center; padding: 40; margin: 30; background: #2a1010; border-radius: 10;\n'
            err += f'    @text {{ content: "404 - Not Found"; size: 32; color: #ff4444; bold: true; }}\n'
            err += f'    @br {{ size: 15; }}\n'
            err += f'    @text {{ content: "Path: {path}"; size: 14; color: #aa6666; }}\n'
            err += f'    @br {{ size: 20; }}\n'
            err += f'    @link {{ content: "Home"; url: "htp://localhost:8080/"; color: #0abde3; size: 16; }}\n'
            err += f'}}\n'

            content_bytes = err.encode('utf-8')
            self.send_response(404)
            self.send_header('Content-Type', 'text/htp; charset=utf-8')
            self.send_header('Content-Length', str(len(content_bytes)))
            self.send_header('Connection', 'close')
            self.end_headers()
            self.wfile.write(content_bytes)
            print(f"  -> 404 Not Found")

    def log_message(self, format, *args):
        pass

if __name__ == '__main__':
    htps = [f for f in os.listdir(DIRECTORY) if f.endswith('.htp')]
    print(f"{'=' * 50}")
    print(f"  HTP Server v1.0")
    print(f"  Port: {PORT}")
    print(f"  Directory: {os.path.abspath(DIRECTORY)}")
    print(f"  Files: {htps}")
    print(f"{'=' * 50}")
    print(f"  Open in PageViewerPro:")
    print(f"  htp://localhost:{PORT}/")
    print(f"{'=' * 50}")
    print()

    with http.server.HTTPServer(('', PORT), HTPHandler) as httpd:
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nServer stopped.")