#include <cellui/Utf8.h>

#include <cstdint>

// Two backends give the same three primitives (the end of the grapheme
// cluster at a position, the columns of a cluster, the columns of a string):
// libunistring, or utf8proc (CELLUI_UNICODE in CMakeLists.txt, which sets
// CELLUI_UTF8PROC).
#ifdef CELLUI_UTF8PROC
#include <utf8proc.h>
#else
#include <unigbrk.h>
#include <uniwidth.h>
#include <unistring/localcharset.h>
#endif

namespace cellui::utf8 {

namespace {

const uint8_t * asBytes(const std::string & text) {
  return reinterpret_cast<const uint8_t *>(text.data());
}

#ifdef CELLUI_UTF8PROC

// The columns of one codepoint: 0 for a control or combining one, 2 for a
// wide one.
int codepointWidth(utf8proc_int32_t cp) {
  int w = utf8proc_charwidth(cp);
  return w < 0 ? 0 : w;
}

// Where the cluster starting at `p` ends. A byte that does not decode is a
// cluster of its own, so the caller always advances.
const uint8_t * clusterEnd(const uint8_t * p, const uint8_t * end) {
  utf8proc_int32_t prev = 0;
  utf8proc_ssize_t n = utf8proc_iterate(p, end - p, &prev);
  if (n < 0) return p + 1;
  const uint8_t * q = p + n;
  // The state carries what UAX #29 needs beyond two codepoints: the parity
  // of regional indicators and the emoji + ZWJ lookback.
  utf8proc_int32_t state = 0;
  while (q < end) {
    utf8proc_int32_t next = 0;
    utf8proc_ssize_t m = utf8proc_iterate(q, end - q, &next);
    if (m < 0 || utf8proc_grapheme_break_stateful(prev, next, &state)) break;
    q += m;
    prev = next;
  }
  return q;
}

// The columns of the cluster [p, q): its codepoints' widths added, as
// libunistring's u8_width does, so a base with a combining mark is the
// base's.
int clusterWidth(const uint8_t * p, const uint8_t * q) {
  int w = 0;
  while (p < q) {
    utf8proc_int32_t cp = 0;
    utf8proc_ssize_t n = utf8proc_iterate(p, q - p, &cp);
    if (n < 0) return 0;  // not text: as libunistring, no width at all
    w += codepointWidth(cp);
    p += n;
  }
  return w;
}

#else

const uint8_t * clusterEnd(const uint8_t * p, const uint8_t * end) {
  return u8_grapheme_next(p, end);
}

int clusterWidth(const uint8_t * p, const uint8_t * q) {
  int w = u8_width(p, static_cast<size_t>(q - p), locale_charset());
  return w < 0 ? 0 : w;
}

#endif

}  // namespace

bool
followsUax29Clusters() {
#ifdef CELLUI_UTF8PROC
  return true;
#else
  return false;
#endif
}

int
displayWidth(const std::string & text) {
  if (text.empty()) return 0;
#ifdef CELLUI_UTF8PROC
  const uint8_t * start = asBytes(text);
  const uint8_t * end = start + text.size();
  int w = 0;
  for (const uint8_t * p = start; p < end;) {
    const uint8_t * q = clusterEnd(p, end);
    if (q <= p) q = p + 1;
    w += clusterWidth(p, q);
    p = q;
  }
  return w;
#else
  int w = u8_strwidth(reinterpret_cast<const uint8_t *>(text.c_str()), locale_charset());
  return w < 0 ? 0 : w;
#endif
}

std::string
truncateToWidth(const std::string & text, int max_columns) {
  if (max_columns <= 0) return std::string();

  const uint8_t * start = asBytes(text);
  const uint8_t * end = start + text.size();

  size_t committed_offset = 0;
  int accumulated = 0;
  const uint8_t * cluster_start = start;

  while (cluster_start < end) {
    const uint8_t * cluster_end = clusterEnd(cluster_start, end);
    if (!cluster_end || cluster_end <= cluster_start) cluster_end = cluster_start + 1;  // defensive: never spin on malformed input

    int w = clusterWidth(cluster_start, cluster_end);

    if (accumulated + w > max_columns) break;

    accumulated += w;
    committed_offset = static_cast<size_t>(cluster_end - start);
    cluster_start = cluster_end;
  }

  return text.substr(0, committed_offset);
}

std::vector<std::string>
clusters(const std::string & text) {
  std::vector<std::string> out;
  const uint8_t * start = asBytes(text);
  const uint8_t * end = start + text.size();
  const uint8_t * cluster_start = start;
  while (cluster_start < end) {
    const uint8_t * cluster_end = clusterEnd(cluster_start, end);
    if (!cluster_end || cluster_end <= cluster_start) cluster_end = cluster_start + 1;
    out.emplace_back(reinterpret_cast<const char *>(cluster_start), static_cast<size_t>(cluster_end - cluster_start));
    cluster_start = cluster_end;
  }
  return out;
}

std::string
dropLeadingColumns(const std::string & text, int columns) {
  if (columns <= 0) return text;

  const uint8_t * start = asBytes(text);
  const uint8_t * end = start + text.size();

  int accumulated = 0;
  const uint8_t * cluster_start = start;
  while (cluster_start < end && accumulated < columns) {
    const uint8_t * cluster_end = clusterEnd(cluster_start, end);
    if (!cluster_end || cluster_end <= cluster_start) cluster_end = cluster_start + 1;  // defensive: never spin on malformed input

    int w = clusterWidth(cluster_start, cluster_end);

    accumulated += w;
    cluster_start = cluster_end;
  }

  std::string result(static_cast<size_t>(accumulated > columns ? accumulated - columns : 0), ' ');
  result.append(text, static_cast<size_t>(cluster_start - start), std::string::npos);
  return result;
}

std::string
padToWidth(const std::string & text, int width) {
  int current = displayWidth(text);
  if (current >= width) return text;

  std::string result = text;
  result.append(static_cast<size_t>(width - current), ' ');
  return result;
}

std::string
encodeCodepoint(uint32_t codepoint) {
  std::string s;
  if (codepoint <= 0x7F) {
    s += static_cast<char>(codepoint);
  } else if (codepoint <= 0x7FF) {
    s += static_cast<char>(0xC0 | (codepoint >> 6));
    s += static_cast<char>(0x80 | (codepoint & 0x3F));
  } else if (codepoint <= 0xFFFF) {
    s += static_cast<char>(0xE0 | (codepoint >> 12));
    s += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
    s += static_cast<char>(0x80 | (codepoint & 0x3F));
  } else {
    s += static_cast<char>(0xF0 | (codepoint >> 18));
    s += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
    s += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
    s += static_cast<char>(0x80 | (codepoint & 0x3F));
  }
  return s;
}

}  // namespace cellui::utf8
