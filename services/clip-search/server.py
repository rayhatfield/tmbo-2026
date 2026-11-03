"""Text-to-image search over tmbo's CLIP embeddings.

GET /search?q=<text>&k=<n>  ->  {"ids": [upload ids, best first], "scores": [...]}
GET /health                 ->  {"rows": ..., "images": ...}

It only answers "which uploads look like this text".  The PHP side loads those
uploads from MySQL, so squelch, NSFW and deleted filtering stay where they are.

Reads from $CLIP_SEARCH_DATA:
  clip_text.onnx                text tower of ViT-B-32 / laion2b_s34b_b79k,
                                [n, 77] int64 -> [n, 512] L2-normalized float32
  bpe_simple_vocab_16e6.txt.gz  open_clip's tokenizer vocabulary
  vectors.f16.npy               [rows, 512] float16, L2-normalized image tiles
  ids.u32                       little-endian uint32 upload id per row; 0 = unknown

A tall or wide image has several rows (tiles) under one id, so a search
over-fetches rows and keeps each upload's best tile.

Restart the service after replacing the data files; it reads them once.
"""
import json
import os
import sys
import time
from collections import OrderedDict
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from threading import Lock
from urllib.parse import parse_qs, urlparse

import faiss
import numpy as np
import onnxruntime as ort

from tokenizer import Tokenizer

DATA = os.environ.get("CLIP_SEARCH_DATA", "data")
HOST = os.environ.get("CLIP_SEARCH_HOST", "127.0.0.1")
PORT = int(os.environ.get("CLIP_SEARCH_PORT", "8737"))
THREADS = int(os.environ.get("CLIP_SEARCH_THREADS", "2"))

MAX_K = 200
MAX_QUERY = 300  # characters; CLIP only reads 77 tokens anyway
CHUNK = 32768


def log(msg):
    print(msg, file=sys.stderr, flush=True)


class Searcher:
    def __init__(self, data):
        t0 = time.monotonic()
        self.tokenize = Tokenizer(os.path.join(data, "bpe_simple_vocab_16e6.txt.gz"))

        opts = ort.SessionOptions()
        opts.intra_op_num_threads = THREADS
        opts.inter_op_num_threads = 1
        self.session = ort.InferenceSession(
            os.path.join(data, "clip_text.onnx"), opts, providers=["CPUExecutionProvider"])
        self.input_name = self.session.get_inputs()[0].name

        self.ids = np.fromfile(os.path.join(data, "ids.u32"), dtype="<u4")

        # Exact inner-product search over float16-coded vectors: half the
        # memory of float32 and ~99.7% the same top-200.  The file is read a
        # chunk at a time, rather than mapped, so neither it nor a float32
        # copy of it is ever resident whole next to the index.
        faiss.omp_set_num_threads(THREADS)
        self.index = faiss.IndexScalarQuantizer(512, faiss.ScalarQuantizer.QT_fp16, faiss.METRIC_INNER_PRODUCT)
        with open(os.path.join(data, "vectors.f16.npy"), "rb") as f:
            version = np.lib.format.read_magic(f)
            read_header = np.lib.format.read_array_header_1_0 if version == (1, 0) else np.lib.format.read_array_header_2_0
            shape, fortran, dtype = read_header(f)
            if shape != (len(self.ids), 512) or fortran or dtype != np.float16:
                raise SystemExit(f"vectors {shape} {dtype} don't match {len(self.ids)} float16 rows")
            for start in range(0, shape[0], CHUNK):
                rows = min(CHUNK, shape[0] - start)
                chunk = np.fromfile(f, dtype="<f2", count=rows * 512).reshape(rows, 512)
                self.index.add(chunk.astype(np.float32))

        self.images = len(np.unique(self.ids[self.ids != 0]))
        self.cache = OrderedDict()
        self.lock = Lock()
        log(f"loaded {self.index.ntotal} rows, {self.images} images in {time.monotonic() - t0:.1f}s")

    def embed(self, text):
        with self.lock:
            hit = self.cache.get(text)
            if hit is not None:
                self.cache.move_to_end(text)
                return hit
        vec = self.session.run(None, {self.input_name: self.tokenize(text)})[0]
        with self.lock:
            self.cache[text] = vec
            if len(self.cache) > 1000:
                self.cache.popitem(last=False)
        return vec

    def search(self, text, k):
        query = self.embed(text)
        fetch = k * 8
        while True:
            scores, rows = self.index.search(query, min(fetch, self.index.ntotal))
            ids, best = [], []
            seen = set()
            for score, row in zip(scores[0], rows[0]):
                if row < 0:
                    break
                upload = int(self.ids[row])
                if upload == 0 or upload in seen:
                    continue
                seen.add(upload)
                ids.append(upload)
                best.append(round(float(score), 4))
                if len(ids) == k:
                    return ids, best
            if fetch >= self.index.ntotal:
                return ids, best
            fetch *= 4


class Handler(BaseHTTPRequestHandler):
    searcher = None

    def reply(self, status, body):
        data = json.dumps(body).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        url = urlparse(self.path)
        args = parse_qs(url.query)
        if url.path == "/health":
            return self.reply(200, {"rows": self.searcher.index.ntotal, "images": self.searcher.images})
        if url.path != "/search":
            return self.reply(404, {"error": "not found"})

        q = " ".join(args.get("q", [""])[0].split())[:MAX_QUERY]
        if not q:
            return self.reply(400, {"error": "q is required"})
        try:
            k = max(1, min(MAX_K, int(args.get("k", ["50"])[0])))
        except ValueError:
            return self.reply(400, {"error": "k must be an integer"})

        t0 = time.monotonic()
        ids, scores = self.searcher.search(q, k)
        self.reply(200, {"ids": ids, "scores": scores, "ms": round((time.monotonic() - t0) * 1000, 1)})

    def log_message(self, fmt, *args):
        pass  # queries are user text; don't write them to the journal


def main():
    Handler.searcher = Searcher(DATA)
    server = ThreadingHTTPServer((HOST, PORT), Handler)
    server.daemon_threads = True
    log(f"listening on {HOST}:{PORT}")
    server.serve_forever()


if __name__ == "__main__":
    main()
