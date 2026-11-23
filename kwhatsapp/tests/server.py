#!/usr/bin/python3 -I
# server.py LOG PORT [PORT...]: the test pages of pages/ on 127.0.0.1 and
# localhost; GET /log?TEXT appends TEXT to LOG (what the page saw);
# /download/attachment answers with Content-Disposition: attachment.
import http.server, os, socketserver, sys, threading, urllib.parse

PAGES = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'pages')
LOG = sys.argv[1]
lock = threading.Lock()

class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=PAGES, **kwargs)

    def log_message(self, fmt, *args):
        sys.stderr.write('%s %s\n' % (self.server.server_address[1], fmt % args))

    def do_GET(self):
        url = urllib.parse.urlsplit(self.path)
        if url.path == '/log':
            with lock, open(LOG, 'a') as f:
                f.write(urllib.parse.unquote(url.query) + '\n')
            self.send_response(204)
            self.send_header('Cache-Control', 'no-store')
            self.end_headers()
            return
        if url.path == '/redirect':
            # a redirect to the page in ?to=
            self.send_response(302)
            self.send_header('Location', urllib.parse.parse_qs(url.query)['to'][0])
            self.end_headers()
            return
        if url.path == '/download/attachment':
            body = bytes(i % 251 for i in range(100000))
            self.send_response(200)
            self.send_header('Content-Type', 'application/octet-stream')
            self.send_header('Content-Disposition', 'attachment; filename="report.bin"')
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        super().do_GET()

class Server(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True
    allow_reuse_address = True

servers = [Server(('127.0.0.1', int(p)), Handler) for p in sys.argv[2:]]
for s in servers[1:]:
    threading.Thread(target=s.serve_forever, daemon=True).start()
servers[0].serve_forever()
