/* Minimal raw DEFLATE (RFC 1951) decoder used to unpack built-in levels.
 * Canonical Huffman codes are decoded bit by bit from per-length counts,
 * which keeps the tables tiny; level data is only a few kilobytes. */
#include "level.h"
#include <string.h>

typedef struct {
  const uint8_t *in;
  size_t len, pos;
  uint32_t bits;
  int nbits;
  uint8_t *out;
  size_t cap, n;
  bool error;
} Inflater;

typedef struct { uint16_t count[16], symbol[288]; } Huffman;

static int bits(Inflater *s, int need) {
  while (s->nbits < need) {
    if (s->pos >= s->len) { s->error = true; return 0; }
    s->bits |= (uint32_t)s->in[s->pos++] << s->nbits;
    s->nbits += 8;
  }
  int v = (int)(s->bits & ((1u << need) - 1));
  s->bits >>= need;
  s->nbits -= need;
  return v;
}

static bool build(Huffman *h, const uint8_t *lengths, int n) {
  uint16_t offs[16];
  memset(h->count, 0, sizeof(h->count));
  for (int i = 0; i < n; i++) h->count[lengths[i]]++;
  h->count[0] = 0;
  int left = 1;
  for (int len = 1; len < 16; len++) {
    left <<= 1;
    left -= h->count[len];
    if (left < 0) return false;
  }
  offs[1] = 0;
  for (int len = 1; len < 15; len++) offs[len + 1] = (uint16_t)(offs[len] + h->count[len]);
  for (int i = 0; i < n; i++)
    if (lengths[i]) h->symbol[offs[lengths[i]]++] = (uint16_t)i;
  return true;
}

static int decode(Inflater *s, const Huffman *h) {
  int code = 0, first = 0, index = 0;
  for (int len = 1; len < 16; len++) {
    code |= bits(s, 1);
    if (s->error) return -1;
    int count = h->count[len];
    if (code - count < first) return h->symbol[index + (code - first)];
    index += count;
    first += count;
    first <<= 1;
    code <<= 1;
  }
  s->error = true;
  return -1;
}

static const uint16_t len_base[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const uint8_t len_extra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const uint16_t dist_base[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const uint8_t dist_extra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

static bool codes(Inflater *s, const Huffman *lit, const Huffman *dist) {
  for (;;) {
    int sym = decode(s, lit);
    if (sym < 0) return false;
    if (sym < 256) {
      if (s->n >= s->cap) return false;
      s->out[s->n++] = (uint8_t)sym;
    } else if (sym == 256) {
      return true;
    } else {
      sym -= 257;
      if (sym >= 29) return false;
      size_t len = len_base[sym] + (size_t)bits(s, len_extra[sym]);
      int d = decode(s, dist);
      if (d < 0 || d >= 30) return false;
      size_t back = dist_base[d] + (size_t)bits(s, dist_extra[d]);
      if (s->error || back > s->n || s->n + len > s->cap) return false;
      for (size_t i = 0; i < len; i++, s->n++) s->out[s->n] = s->out[s->n - back];
    }
  }
}

int inflate_raw(uint8_t *out, size_t cap, const uint8_t *in, size_t len) {
  static Huffman lit, dist;
  static const uint8_t order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
  Inflater s = {in, len, 0, 0, 0, out, cap, 0, false};
  int last;
  do {
    last = bits(&s, 1);
    int type = bits(&s, 2);
    if (s.error) return -1;
    if (type == 0) {
      s.bits = 0; s.nbits = 0;
      if (s.pos + 4 > s.len) return -1;
      unsigned n = s.in[s.pos] | (unsigned)s.in[s.pos + 1] << 8, nn = s.in[s.pos + 2] | (unsigned)s.in[s.pos + 3] << 8;
      s.pos += 4;
      if ((n ^ 0xffffu) != nn || s.pos + n > s.len || s.n + n > s.cap) return -1;
      memcpy(s.out + s.n, s.in + s.pos, n);
      s.pos += n; s.n += n;
    } else if (type == 1) {
      uint8_t lengths[320];
      int i = 0;
      for (; i < 144; i++) lengths[i] = 8;
      for (; i < 256; i++) lengths[i] = 9;
      for (; i < 280; i++) lengths[i] = 7;
      for (; i < 288; i++) lengths[i] = 8;
      build(&lit, lengths, 288);
      for (i = 0; i < 30; i++) lengths[i] = 5;
      build(&dist, lengths, 30);
      if (!codes(&s, &lit, &dist)) return -1;
    } else if (type == 2) {
      uint8_t lengths[320];
      int nlen = bits(&s, 5) + 257, ndist = bits(&s, 5) + 1, ncode = bits(&s, 4) + 4;
      if (s.error || nlen > 286 || ndist > 30) return -1;
      for (int i = 0; i < 19; i++) lengths[order[i]] = i < ncode ? (uint8_t)bits(&s, 3) : 0;
      if (s.error || !build(&lit, lengths, 19)) return -1;
      int i = 0;
      while (i < nlen + ndist) {
        int sym = decode(&s, &lit);
        if (sym < 0) return -1;
        if (sym < 16) { lengths[i++] = (uint8_t)sym; continue; }
        int rep, val = 0;
        if (sym == 16) { if (!i) return -1; val = lengths[i - 1]; rep = 3 + bits(&s, 2); }
        else if (sym == 17) rep = 3 + bits(&s, 3);
        else rep = 11 + bits(&s, 7);
        if (s.error || i + rep > nlen + ndist) return -1;
        while (rep--) lengths[i++] = (uint8_t)val;
      }
      if (!lengths[256]) return -1;
      if (!build(&lit, lengths, nlen) || !build(&dist, lengths + nlen, ndist)) return -1;
      if (!codes(&s, &lit, &dist)) return -1;
    } else {
      return -1;
    }
  } while (!last);
  return (int)s.n;
}
