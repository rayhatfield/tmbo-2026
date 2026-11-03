"""CLIP's BPE tokenizer without torch.

Trimmed from open_clip 3.3.0's open_clip/tokenizer.py (SimpleTokenizer with its
default "lower" cleaning and truncation), which is itself copied from
https://github.com/openai/CLIP.  MIT License, Copyright (c) 2021 OpenAI.

The output has to match open_clip exactly: the text model pools at the
position of the end-of-text token, so any difference changes the embedding.
"""
import gzip
import html
from functools import lru_cache

import ftfy
import numpy as np
import regex as re

CONTEXT_LENGTH = 77


@lru_cache()
def bytes_to_unicode():
    bs = list(range(ord("!"), ord("~") + 1)) + list(range(ord("¡"), ord("¬") + 1)) + list(range(ord("®"), ord("ÿ") + 1))
    cs = bs[:]
    n = 0
    for b in range(2**8):
        if b not in bs:
            bs.append(b)
            cs.append(2**8 + n)
            n += 1
    return dict(zip(bs, [chr(c) for c in cs]))


def get_pairs(word):
    return set(zip(word, word[1:]))


def clean(text):
    text = ftfy.fix_text(text)
    text = html.unescape(html.unescape(text))
    return " ".join(text.strip().split()).strip().lower()


class Tokenizer:
    def __init__(self, bpe_path):
        self.byte_encoder = bytes_to_unicode()
        merges = gzip.open(bpe_path).read().decode("utf-8").split("\n")
        merges = [tuple(m.split()) for m in merges[1:49152 - 256 - 2 + 1]]
        vocab = list(self.byte_encoder.values())
        vocab = vocab + [v + "</w>" for v in vocab]
        vocab += ["".join(m) for m in merges]
        special = ["<start_of_text>", "<end_of_text>"]
        vocab += special
        self.encoder = dict(zip(vocab, range(len(vocab))))
        self.bpe_ranks = dict(zip(merges, range(len(merges))))
        self.cache = {t: t for t in special}
        self.pat = re.compile(
            "|".join(special) + r"""|'s|'t|'re|'ve|'m|'ll|'d|[\p{L}]+|[\p{N}]|[^\s\p{L}\p{N}]+""",
            re.IGNORECASE,
        )
        self.sot = self.encoder["<start_of_text>"]
        self.eot = self.encoder["<end_of_text>"]

    def bpe(self, token):
        if token in self.cache:
            return self.cache[token]
        word = tuple(token[:-1]) + (token[-1] + "</w>",)
        pairs = get_pairs(word)
        if not pairs:
            return token + "</w>"
        while True:
            bigram = min(pairs, key=lambda p: self.bpe_ranks.get(p, float("inf")))
            if bigram not in self.bpe_ranks:
                break
            first, second = bigram
            new_word = []
            i = 0
            while i < len(word):
                try:
                    j = word.index(first, i)
                except ValueError:
                    new_word.extend(word[i:])
                    break
                new_word.extend(word[i:j])
                i = j
                if word[i] == first and i < len(word) - 1 and word[i + 1] == second:
                    new_word.append(first + second)
                    i += 2
                else:
                    new_word.append(word[i])
                    i += 1
            word = tuple(new_word)
            if len(word) == 1:
                break
            pairs = get_pairs(word)
        word = " ".join(word)
        # queries are user input, so don't let the cache grow without bound
        if len(self.cache) < 50000:
            self.cache[token] = word
        return word

    def encode(self, text):
        out = []
        for token in re.findall(self.pat, clean(text)):
            token = "".join(self.byte_encoder[b] for b in token.encode("utf-8"))
            out.extend(self.encoder[t] for t in self.bpe(token).split(" "))
        return out

    def __call__(self, text):
        """One text -> int64 array of shape [1, 77], as open_clip's tokenize()."""
        tokens = [self.sot] + self.encode(text) + [self.eot]
        if len(tokens) > CONTEXT_LENGTH:
            tokens = tokens[:CONTEXT_LENGTH]
            tokens[-1] = self.eot
        result = np.zeros((1, CONTEXT_LENGTH), dtype=np.int64)
        result[0, :len(tokens)] = tokens
        return result
