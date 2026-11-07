"""A stand-in for the parts of Google's sign-in and Drive that cloud saves use, strict about what
it is sent, so test_cloud_saves can run the Google Drive path without a Google account.
    python3 tests/fake_drive.py 8766
"""
import json, re, sys, urllib.parse, itertools
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

CLIENT_ID, CLIENT_SECRET = "test-client.apps.googleusercontent.com", "test-secret"
SCOPE = "https://www.googleapis.com/auth/drive.file"
FOLDER_MIME = "application/vnd.google-apps.folder"
files = {}            # id -> {"name", "parents", "appProperties", "data"}
access = set()        # access tokens currently good
refresh = set()
polls = {}            # device code -> times asked
ids = itertools.count(1)


class H(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def reply(self, code, body, ctype="application/json"):
        data = body if isinstance(body, bytes) else json.dumps(body).encode()
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def body(self):
        return self.rfile.read(int(self.headers.get("Content-Length", 0)))

    def authed(self):
        a = self.headers.get("Authorization", "")
        if not a.startswith("Bearer ") or a[7:] not in access:
            self.reply(401, {"error": {"code": 401, "message": "Invalid Credentials"}})
            return False
        return True

    def do_GET(self):
        u = urllib.parse.urlparse(self.path)
        q = urllib.parse.parse_qs(u.query)
        if u.path == "/_expire":                       # test aid: every access token goes bad
            access.clear()
            return self.reply(200, {})
        if not self.authed():
            return
        if u.path == "/drive/v3/files":
            assert "spaces" not in q, q
            query = q["q"][0]
            m = re.fullmatch(r"name = '([^']+)' and mimeType = '%s' and trashed = false" % re.escape(FOLDER_MIME), query)
            if m:
                found = [{"id": i} for i, f in files.items() if f["name"] == m.group(1) and f.get("mimeType") == FOLDER_MIME]
                return self.reply(200, {"files": found})
            m = re.fullmatch(r"name = '([^']+)' and '([^']+)' in parents and trashed = false", query)
            assert m, query
            name, parent = m.groups()
            found = [{"id": i, "appProperties": f["appProperties"]} for i, f in files.items()
                     if f["name"] == name and parent in f["parents"]]
            for f in found:
                if not f["appProperties"]:
                    del f["appProperties"]
            return self.reply(200, {"files": found})
        if u.path.startswith("/drive/v3/files/") and q.get("alt") == ["media"]:
            f = files.get(u.path.rsplit("/", 1)[1])
            return self.reply(200, f["data"], "application/octet-stream") if f else self.reply(404, {"error": {"message": "File not found"}})
        self.reply(404, {"error": {"message": "no such call " + self.path}})

    def multipart(self):
        ctype = self.headers["Content-Type"]
        assert ctype.startswith("multipart/related; boundary="), ctype
        b = ("--" + ctype.split("boundary=")[1]).encode()
        raw = self.body()
        parts = raw.split(b)
        assert parts[-1].strip() == b"--", parts[-1][:20]
        meta_part, data_part = parts[1], parts[2]
        mh, mbody = meta_part.split(b"\r\n\r\n", 1)
        assert b"application/json" in mh
        dh, dbody = data_part.split(b"\r\n\r\n", 1)
        assert dbody.endswith(b"\r\n")
        return json.loads(mbody.strip()), dbody[:-2]

    def do_POST(self):
        u = urllib.parse.urlparse(self.path)
        q = urllib.parse.parse_qs(u.query)
        if u.path == "/device/code":
            form = urllib.parse.parse_qs(self.body().decode())
            assert form["client_id"] == [CLIENT_ID] and form["scope"] == [SCOPE], form
            return self.reply(200, {"device_code": "dev-1", "user_code": "ABCD-EFGH", "verification_url": "https://www.google.com/device",
                                    "expires_in": 1800, "interval": 5})
        if u.path == "/token":
            form = urllib.parse.parse_qs(self.body().decode())
            assert form["client_id"] == [CLIENT_ID] and form["client_secret"] == [CLIENT_SECRET], form
            if form["grant_type"] == ["urn:ietf:params:oauth:grant-type:device_code"]:
                n = polls[form["device_code"][0]] = polls.get(form["device_code"][0], 0) + 1
                if n == 1:
                    return self.reply(428, {"error": "authorization_pending"})
                r = "refresh-%d" % next(ids)
                refresh.add(r)
                a = "access-%d" % next(ids)
                access.add(a)
                return self.reply(200, {"access_token": a, "refresh_token": r, "expires_in": 3599, "scope": SCOPE})
            if form["grant_type"] == ["refresh_token"]:
                if form["refresh_token"][0] not in refresh:
                    return self.reply(400, {"error": "invalid_grant", "error_description": "Token has been expired or revoked."})
                a = "access-%d" % next(ids)
                access.add(a)
                return self.reply(200, {"access_token": a, "expires_in": 3599})
            return self.reply(400, {"error": "unsupported_grant_type"})
        if u.path == "/drive/v3/files":                # a folder
            if not self.authed():
                return
            meta = json.loads(self.body())
            assert meta == {"name": "Fulldex Cloud Saves", "mimeType": FOLDER_MIME}, meta
            i = "folder-%d" % next(ids)
            files[i] = {"name": meta["name"], "mimeType": FOLDER_MIME, "parents": [], "appProperties": {}, "data": b""}
            return self.reply(200, {"id": i})
        if u.path == "/upload/drive/v3/files":
            if not self.authed():
                return
            assert q.get("uploadType") == ["multipart"], q
            meta, data = self.multipart()
            assert len(meta["parents"]) == 1 and files[meta["parents"][0]].get("mimeType") == FOLDER_MIME, meta
            i = "file-%d" % next(ids)
            files[i] = {"name": meta["name"], "parents": meta["parents"], "appProperties": meta.get("appProperties", {}), "data": data}
            return self.reply(200, {"id": i})
        self.reply(404, {"error": {"message": "no such call " + self.path}})

    def do_PATCH(self):
        u = urllib.parse.urlparse(self.path)
        if not u.path.startswith("/upload/drive/v3/files/") or not self.authed():
            return self.reply(404, {}) if not u.path.startswith("/upload/") else None
        f = files.get(u.path.rsplit("/", 1)[1])
        if not f:
            self.body()
            return self.reply(404, {"error": {"message": "File not found"}})
        meta, data = self.multipart()
        assert "parents" not in meta and "name" not in meta, meta
        f["appProperties"].update(meta.get("appProperties", {}))
        f["data"] = data
        self.reply(200, {"id": u.path.rsplit("/", 1)[1]})


ThreadingHTTPServer(("127.0.0.1", int(sys.argv[1])), H).serve_forever()
