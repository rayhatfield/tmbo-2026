# clip-search

Text-to-image search for tmbo. Search text goes through CLIP's text encoder
(ViT-B-32, laion2b_s34b_b79k, exported to ONNX) and is matched against a CLIP
embedding of every image. The response holds upload ids only, and PHP loads and
filters those uploads as it does everywhere else.

    GET /search?q=a+dog+on+a+beach&k=50   {"ids": [...], "scores": [...], "ms": ...}
    GET /health                           {"rows": 460747, "images": 377737}

`k` is capped at 200. Wide and tall images are embedded as several tiles, so
the service ranks each upload by its best tile.

Data files live in `$CLIP_SEARCH_DATA` and are built from the
tmbo-clip-search repository (the embeddings and the ONNX export live there,
not here). See `server.py` for their formats and DEPLOYING for installing it.

Measured with the full index (460,747 rows):

| | |
|---|---|
| Resident memory | ~910 MB (1.1 GB peak while loading) |
| Load time | ~5 s |
| Uncached query, 2 threads, 2.1 GHz Xeon | ~110 ms (encode ~45, search ~65) |
| Repeated query | encode skipped (LRU of 1,000 query vectors) |
| Agreement with the torch pipeline | same top-1 on all 29 eval queries, 140/142 of their top-5 |

`tokenizer.py` is open_clip's tokenizer without the torch dependency. Its output
has to match open_clip's exactly, because the model reads the embedding at the
end-of-text position.
